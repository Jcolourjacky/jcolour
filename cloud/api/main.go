package main

import (
	"context"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/redis/go-redis/v9"

	"github.com/jcolour/cloud/api"
)

func main() {
	// Configuration
	port := getEnv("PORT", "8080")
	dbURL := getEnv("DATABASE_URL", "postgres://localhost:5432/jcolour?sslmode=disable")
	redisURL := getEnv("REDIS_URL", "redis://localhost:6379")
	workDir := getEnv("WORK_DIR", "/tmp/jcolour-icc")

	// Create work directory
	if err := os.MkdirAll(workDir, 0755); err != nil {
		log.Fatalf("Failed to create work directory: %v", err)
	}

	// Connect to PostgreSQL
	ctx := context.Background()
	dbPool, err := pgxpool.New(ctx, dbURL)
	if err != nil {
		log.Fatalf("Failed to connect to database: %v", err)
	}
	defer dbPool.Close()

	// Test database connection
	if err := dbPool.Ping(ctx); err != nil {
		log.Fatalf("Failed to ping database: %v", err)
	}
	log.Println("Connected to PostgreSQL")

	// Connect to Redis
	opt, err := redis.ParseURL(redisURL)
	if err != nil {
		log.Fatalf("Failed to parse Redis URL: %v", err)
	}
	redisClient := redis.NewClient(opt)
	defer redisClient.Close()

	// Test Redis connection
	if _, err := redisClient.Ping(ctx).Result(); err != nil {
		log.Fatalf("Failed to ping Redis: %v", err)
	}
	log.Println("Connected to Redis")

	// Create server
	server := api.NewServer(dbPool, redisClient, workDir)

	// Setup Gin router
	gin.SetMode(gin.ReleaseMode)
	r := gin.New()
	r.Use(gin.Recovery())
	r.Use(gin.LoggerWithConfig(gin.LoggerConfig{
		SkipPaths: []string{"/api/v1/jcolour/health"},
	}))
	r.Use(corsMiddleware())
	r.Use(rateLimitMiddleware(redisClient))

	server.SetupRoutes(r)

	// Start server
	srv := &http.Server{
		Addr:         ":" + port,
		Handler:      r,
		ReadTimeout:  30 * time.Second,
		WriteTimeout: 60 * time.Second,
		IdleTimeout:  120 * time.Second,
	}

	// Graceful shutdown
	go func() {
		sigChan := make(chan os.Signal, 1)
		signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
		<-sigChan

		log.Println("Shutting down server...")
		ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
		defer cancel()

		if err := srv.Shutdown(ctx); err != nil {
			log.Printf("Server shutdown error: %v", err)
		}
	}()

	log.Printf("jcolour cloud API starting on port %s", port)
	if err := srv.ListenAndServe(); err != http.ErrServerClosed {
		log.Fatalf("Server failed: %v", err)
	}
}

func getEnv(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}

func corsMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Header("Access-Control-Allow-Origin", "*")
		c.Header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
		c.Header("Access-Control-Allow-Headers", "Content-Type, Authorization")
		c.Header("Access-Control-Max-Age", "86400")

		if c.Request.Method == http.MethodOptions {
			c.AbortWithStatus(http.StatusNoContent)
			return
		}
		c.Next()
	}
}

func rateLimitMiddleware(redis *redis.Client) gin.HandlerFunc {
	return func(c *gin.Context) {
		// Skip rate limiting for health check
		if c.Request.URL.Path == "/api/v1/jcolour/health" {
			c.Next()
			return
		}

		ip := c.ClientIP()
		key := fmt.Sprintf("rate:%s:%s", ip, c.FullPath())

		count, err := redis.Incr(context.Background(), key).Result()
		if err != nil {
			log.Printf("Rate limit check failed: %v", err)
			c.Next()
			return
		}

		if count == 1 {
			redis.Expire(context.Background(), key, time.Minute)
		}

		if count > 100 {
			c.AbortWithStatusJSON(http.StatusTooManyRequests, gin.H{
				"error": "Rate limit exceeded. Please try again later.",
			})
			return
		}

		c.Next()
	}
}