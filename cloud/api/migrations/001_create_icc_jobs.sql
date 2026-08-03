-- Migration: Create ICC jobs table for Argyll cloud ICC generation
-- Spike 4: Cloud Argyll ICC Generation API

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE IF NOT EXISTS icc_jobs (
    id BIGSERIAL PRIMARY KEY,
    job_id VARCHAR(64) UNIQUE NOT NULL,
    user_id VARCHAR(64),
    profile_name VARCHAR(255),
    status VARCHAR(32) NOT NULL DEFAULT 'pending',
    -- Status: pending, processing, completed, failed
    spectral_data_hash VARCHAR(64),
    result_url TEXT,
    error TEXT,
    cost DECIMAL(10, 4) NOT NULL DEFAULT 0.05,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    completed_at TIMESTAMPTZ,
    expires_at TIMESTAMPTZ
);

CREATE INDEX idx_icc_jobs_user_id ON icc_jobs(user_id);
CREATE INDEX idx_icc_jobs_status ON icc_jobs(status);
CREATE INDEX idx_icc_jobs_created_at ON icc_jobs(created_at DESC);

-- Rate limiting table
CREATE TABLE IF NOT EXISTS rate_limits (
    id BIGSERIAL PRIMARY KEY,
    user_id VARCHAR(64) NOT NULL,
    endpoint VARCHAR(128) NOT NULL,
    request_count INT NOT NULL DEFAULT 1,
    window_start TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE(user_id, endpoint)
);

CREATE INDEX idx_rate_limits_window ON rate_limits(window_start);

-- Audit log for GPL compliance tracking
CREATE TABLE IF NOT EXISTS argyll_usage_log (
    id BIGSERIAL PRIMARY KEY,
    job_id VARCHAR(64) NOT NULL REFERENCES icc_jobs(job_id),
    execution_time_ms INT,
    argyll_version VARCHAR(32),
    command_executed TEXT,
    exit_code INT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_argyll_usage_job ON argyll_usage_log(job_id);

-- Stored procedure for rate limiting check
CREATE OR REPLACE FUNCTION check_rate_limit(
    p_user_id VARCHAR,
    p_endpoint VARCHAR,
    p_max_requests INT,
    p_window_minutes INT
) RETURNS BOOLEAN AS $$
DECLARE
    current_count INT;
    window_start TIMESTAMPTZ;
BEGIN
    SELECT request_count, window_start INTO current_count, window_start
    FROM rate_limits
    WHERE user_id = p_user_id AND endpoint = p_endpoint
    FOR UPDATE;

    IF current_count IS NULL THEN
        INSERT INTO rate_limits (user_id, endpoint, request_count, window_start)
        VALUES (p_user_id, p_endpoint, 1, NOW());
        RETURN TRUE;
    END IF;

    IF window_start < NOW() - (p_window_minutes || ' minutes')::INTERVAL THEN
        UPDATE rate_limits SET request_count = 1, window_start = NOW()
        WHERE user_id = p_user_id AND endpoint = p_endpoint;
        RETURN TRUE;
    END IF;

    IF current_count >= p_max_requests THEN
        RETURN FALSE;
    END IF;

    UPDATE rate_limits SET request_count = current_count + 1
    WHERE user_id = p_user_id AND endpoint = p_endpoint;
    RETURN TRUE;
END;
$$ LANGUAGE plpgsql;

-- Function to clean up expired jobs
CREATE OR REPLACE FUNCTION cleanup_expired_jobs() RETURNS INT AS $$
DECLARE
    deleted_count INT;
BEGIN
    WITH deleted AS (
        DELETE FROM icc_jobs
        WHERE expires_at < NOW() AND expires_at IS NOT NULL
        RETURNING id
    )
    SELECT COUNT(*) INTO deleted_count FROM deleted;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;

-- Add comments for legal compliance (GPL tracking)
COMMENT ON TABLE argyll_usage_log IS
    'Audit log for ArgyllCMS subprocess execution. Required for GPL compliance under SaaS loophole - Argyll runs in isolated Docker container as subprocess.';

COMMENT ON COLUMN argyll_usage_log.command_executed IS
    'Full Argyll command executed. Stored for legal compliance verification that Argyll is used as isolated subprocess only.';

COMMENT ON TABLE icc_jobs IS
    'ICC generation jobs. ArgyllCMS is spawned as Docker subprocess - see argyll_usage_log for compliance tracking.';
