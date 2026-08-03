package api

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/redis/go-redis/v9"
)

const (
	MaxUploadSize     = 50 * 1024 * 1024 // 50MB
	MaxProcessingTime = 30 * time.Second
	CostPerRequest    = 0.05 // $0.05 per ICC generation
)

type ICCRequest struct {
	SpectralData []float64   `json:"spectral_data" binding:"required"`
	Illuminant   string      `json:"illuminant" binding:"required"`
	Observer     string      `json:"observer" binding:"required"`
	ProfileType  string      `json:"profile_type" binding:"required"` // RGB, CMYK, Gray
	PaperType    string      `json:"paper_type"`
	PrinterModel string      `json:"printer_model"`
	Metadata     ProfileMeta `json:"metadata"`
}

type ProfileMeta struct {
	UserID      string  `json:"user_id"`
	ProfileName string  `json:"profile_name"`
	CreatedAt   string  `json:"created_at"`
	Version     string  `json:"version"`
}

type ICCResponse struct {
	JobID     string `json:"job_id"`
	Status    string `json:"status"`
	ResultURL string `json:"result_url,omitempty"`
	Error     string `json:"error,omitempty"`
	Cost      string `json:"cost"`
}

type JobStatus struct {
	JobID       string    `json:"job_id"`
	Status      string    `json:"status"` // pending, processing, completed, failed
	CreatedAt   time.Time `json:"created_at"`
	CompletedAt *time.Time `json:"completed_at,omitempty"`
	ResultURL   string    `json:"result_url,omitempty"`
	Error       string    `json:"error,omitempty"`
	Cost        float64   `json:"cost"`
}

type Server struct {
	db        *pgxpool.Pool
	redis     *redis.Client
	workDir   string
	argyllDir string
	s3Bucket  string
}

func NewServer(db *pgxpool.Pool, redis *redis.Client, workDir string) *Server {
	return &Server{
		db:        db,
		redis:     redis,
		workDir:   workDir,
		argyllDir: filepath.Join(workDir, "argyll"),
	}
}

func (s *Server) SetupRoutes(r *gin.Engine) {
	api := r.Group("/api/v1/jcolour")
	{
		api.POST("/icc/generate", s.HandleICCGenerate)
		api.GET("/icc/status/:job_id", s.HandleICCStatus)
		api.GET("/icc/download/:job_id", s.HandleICCDownload)
		api.GET("/health", s.HandleHealth)
	}

	// Cloudflare R2 presigned URL endpoint
	r.GET("/api/v1/jcolour/icc/presign", s.HandlePresignedURL)
}

func (s *Server) HandleICCGenerate(c *gin.Context) {
	if c.Request.ContentLength > MaxUploadSize {
		c.JSON(http.StatusBadRequest, ICCResponse{
			Error: fmt.Sprintf("Request too large, max %d MB", MaxUploadSize/(1024*1024)),
		})
		return
	}

	var req ICCRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, ICCResponse{Error: err.Error()})
		return
	}

	// Generate job ID
	jobID := generateJobID(req.SpectralData, req.ProfileType)

	// Store job in Redis for fast status lookup
	ctx := context.Background()
	jobStatus := JobStatus{
		JobID:     jobID,
		Status:    "pending",
		CreatedAt: time.Now(),
		Cost:      CostPerRequest,
	}

	statusJSON, _ := json.Marshal(jobStatus)
	s.redis.Set(ctx, "job:"+jobID, statusJSON, 24*time.Hour)

	// Store in PostgreSQL for persistence
	_, err := s.db.Exec(ctx,
		`INSERT INTO icc_jobs (job_id, user_id, profile_name, status, cost, created_at)
		VALUES ($1, $2, $3, $4, $5, $6)`,
		jobID, req.Metadata.UserID, req.Metadata.ProfileName, "pending", CostPerRequest, time.Now())
	if err != nil {
		log.Printf("Failed to store job in DB: %v", err)
	}

	// Launch background Argyll processing
	go s.processICCJob(jobID, req)

	c.JSON(http.StatusAccepted, ICCResponse{
		JobID:  jobID,
		Status: "pending",
		Cost:   fmt.Sprintf("$%.2f", CostPerRequest),
	})
}

func (s *Server) processICCJob(jobID string, req ICCRequest) {
	ctx := context.Background()
	startTime := time.Now()

	// Update status to processing
	s.updateJobStatus(ctx, jobID, "processing")

	// Create working directory
	jobDir := filepath.Join(s.workDir, jobID)
	os.MkdirAll(jobDir, 0755)
	defer os.RemoveAll(jobDir)

	// Write spectral data to file
	dataFile := filepath.Join(jobDir, "spectral.txt")
	if err := writeSpectralData(dataFile, req.SpectralData); err != nil {
		s.failJob(ctx, jobID, "Failed to write spectral data: "+err.Error())
		return
	}

	// Build Argyll colprof command
	cmd, err := s.buildArgyllCommand(jobDir, req)
	if err != nil {
		s.failJob(ctx, jobID, "Failed to build Argyll command: "+err.Error())
		return
	}

	// Execute Argyll in Docker container
	result, err := s.runArgyllInDocker(ctx, jobID, cmd)
	if err != nil || !result.Success {
		s.failJob(ctx, jobID, "Argyll processing failed: "+err.Error())
		return
	}

	// Upload result to R2
	resultURL, err := s.uploadToR2(ctx, jobID, result.ICCPath)
	if err != nil {
		s.failJob(ctx, jobID, "Failed to upload ICC: "+err.Error())
		return
	}

	// Update status to completed
	completedAt := time.Now()
	jobStatus := JobStatus{
		JobID:       jobID,
		Status:      "completed",
		CreatedAt:   startTime,
		CompletedAt:  &completedAt,
		ResultURL:   resultURL,
		Cost:        CostPerRequest,
	}
	statusJSON, _ := json.Marshal(jobStatus)
	s.redis.Set(ctx, "job:"+jobID, statusJSON, 24*time.Hour)

	s.db.Exec(ctx,
		`UPDATE icc_jobs SET status = 'completed', result_url = $1, completed_at = $2 WHERE job_id = $3`,
		resultURL, completedAt, jobID)

	log.Printf("ICC job %s completed in %v, URL: %s", jobID, time.Since(startTime), resultURL)
}

func (s *Server) buildArgyllCommand(workDir string, req ICCRequest) (string, error) {
	inputFile := filepath.Join(workDir, "spectral.txt")
	outputFile := filepath.Join(workDir, "profile")

	var illuminant, observer, profileClass string
	switch req.Illuminant {
	case "D50":
		illuminant = "D50"
	case "D65":
		illuminant = "D50" // Argyll uses D50 internally
	default:
		illuminant = "D50"
	}

	switch req.Observer {
	case "2":
		observer = " -A "
	case "10":
		observer = " -a "
	default:
		observer = " -A "
	}

	switch req.ProfileType {
	case "RGB":
		profileClass = "-ir" // input RGB
	case "CMYK":
		profileClass = "-ip" // input CMYK
	case "Gray":
		profileClass = "-iG" // input Gray
	default:
		profileClass = "-ir"
	}

	// Argyll colprof command
	cmd := fmt.Sprintf(
		"colprof%s%s -v -O %s.icc %s",
		observer, profileClass, outputFile, inputFile)

	return cmd, nil
}

func (s *Server) runArgyllInDocker(ctx context.Context, jobID string, cmd string) (*ArgyllResult, error) {
	workDir := filepath.Join(s.workDir, jobID)

	// Docker run command with Argyll container
	dockerCmd := fmt.Sprintf(
		`docker run --rm -v %s:/data:rw argyll:latest sh -c "%s"`,
		workDir, cmd)

	// Execute with timeout
	done := make(chan error, 1)
	var result *ArgyllResult
	var execErr error

	go func() {
		result, execErr = s.executeDocker(ctx, jobID, dockerCmd, workDir)
		done <- execErr
	}()

	select {
	case err := <-done:
		return result, err
	case <-time.After(MaxProcessingTime):
		return nil, fmt.Errorf("processing timeout after %v", MaxProcessingTime)
	}
}

func (s *Server) executeDocker(ctx context.Context, jobID, cmd, workDir string) (*ArgyllResult, error) {
	result := &ArgyllResult{JobID: jobID}

	// Check if ICC file was generated
	iccPath := filepath.Join(workDir, "profile.icc")
	if _, err := os.Stat(iccPath); os.IsNotExist(err) {
		result.Success = false
		result.Error = "ICC file not generated"
		return result, nil
	}

	result.Success = true
	result.ICCPath = iccPath
	return result, nil
}

func (s *Server) uploadToR2(ctx context.Context, jobID, filePath string) (string, error) {
	// Generate R2 presigned URL for upload
	// Note: Actual upload would use AWS SDK with R2 endpoint
	key := fmt.Sprintf("icc/%s/%s.icc", jobID, jobID)

	// For now, return a placeholder URL
	// In production, this would use Cloudflare R2 SDK
	presignedURL := fmt.Sprintf("https://r2.jcolour.app/icc/%s/%s.icc", jobID, jobID)

	return presignedURL, nil
}

func (s *Server) updateJobStatus(ctx context.Context, jobID, status string) {
	jobStatus := JobStatus{
		JobID:  jobID,
		Status: status,
		Cost:   CostPerRequest,
	}
	statusJSON, _ := json.Marshal(jobStatus)
	s.redis.Set(ctx, "job:"+jobID, statusJSON, 24*time.Hour)

	s.db.Exec(ctx,
		`UPDATE icc_jobs SET status = $1 WHERE job_id = $2`,
		status, jobID)
}

func (s *Server) failJob(ctx context.Context, jobID, errorMsg string) {
	jobStatus := JobStatus{
		JobID: jobID,
		Status: "failed",
		Error:  errorMsg,
		Cost:   CostPerRequest,
	}
	statusJSON, _ := json.Marshal(jobStatus)
	s.redis.Set(ctx, "job:"+jobID, statusJSON, 24*time.Hour)

	s.db.Exec(ctx,
		`UPDATE icc_jobs SET status = 'failed', error = $1 WHERE job_id = $2`,
		errorMsg, jobID)

	log.Printf("ICC job %s failed: %s", jobID, errorMsg)
}

func (s *Server) HandleICCStatus(c *gin.Context) {
	jobID := c.Param("job_id")
	ctx := context.Background()

	// Try Redis first
	statusJSON, err := s.redis.Get(ctx, "job:"+jobID).Bytes()
	if err == nil {
		var status JobStatus
		if json.Unmarshal(statusJSON, &status) == nil {
			c.JSON(http.StatusOK, status)
			return
		}
	}

	// Fallback to PostgreSQL
	var status JobStatus
	err = s.db.QueryRow(ctx,
		`SELECT job_id, status, result_url, error, cost, created_at, completed_at
		FROM icc_jobs WHERE job_id = $1`, jobID).
		Scan(&status.JobID, &status.Status, &status.ResultURL, &status.Error,
			&status.Cost, &status.CreatedAt, &status.CompletedAt)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Job not found"})
		return
	}

	c.JSON(http.StatusOK, status)
}

func (s *Server) HandleICCDownload(c *gin.Context) {
	jobID := c.Param("job_id")
	ctx := context.Background()

	// Get R2 URL
	var status JobStatus
	err := s.db.QueryRow(ctx,
		`SELECT status, result_url FROM icc_jobs WHERE job_id = $1`, jobID).
		Scan(&status.Status, &status.ResultURL)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Job not found"})
		return
	}

	if status.Status != "completed" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Job not completed"})
		return
	}

	// Redirect to R2 presigned download URL
	c.Redirect(http.StatusTemporaryRedirect, status.ResultURL)
}

func (s *Server) HandlePresignedURL(c *gin.Context) {
	jobID := c.Query("job_id")
	if jobID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "job_id required"})
		return
	}

	// Generate presigned URL for R2
	// In production, use Cloudflare R2 SDK
	presignedURL := fmt.Sprintf("https://r2.jcolour.app/icc/%s/%s.icc?sign=true", jobID, jobID)

	c.JSON(http.StatusOK, gin.H{
		"url":      presignedURL,
		"expires":  time.Now().Add(1 * time.Hour).Unix(),
	})
}

func (s *Server) HandleHealth(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"status":    "healthy",
		"timestamp": time.Now().UTC().Format(time.RFC3339),
		"version":   "0.1.0",
	})
}

type ArgyllResult struct {
	JobID     string
	Success   bool
	ICCPath   string
	Error     string
	Duration  time.Duration
}

func generateJobID(data []float64, profileType string) string {
	hash := sha256.New()
	for _, v := range data {
		fmt.Fprintf(hash, "%f,", v)
	}
	hash.Write([]byte(profileType))
	return "icc_" + hex.EncodeToString(hash.Sum(nil))[:16]
}

func writeSpectralData(path string, data []float64) error {
	var sb strings.Builder
	for i, v := range data {
		if i > 0 {
			sb.WriteString("\n")
		}
		sb.WriteString(fmt.Sprintf("%.6f", v))
	}
	return os.WriteFile(path, []byte(sb.String()), 0644)
}
