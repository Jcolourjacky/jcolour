# Dockerfile for ArgyllCMS container
# Used for cloud ICC profile generation (Spike 4)
# GPL v3 - Argyll runs as isolated subprocess under SaaS loophole

FROM debian:bookworm-slim

LABEL maintainer="jcolour"
LABEL description="ArgyllCMS for jcolour cloud ICC generation"
LABEL license="GPL-3.0-or-later"
LABEL architecture="amd64"

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install ArgyllCMS and dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    argyll \
    && rm -rf /var/lib/apt/lists/*

# Create working directory
WORKDIR /data

# Security: run as non-root user
RUN useradd -m -u 1000 argyll && \
    chown -R argyll:argyll /data

USER argyll

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD cmd = "test -f /usr/bin/colprof" || exit 1

# Entry point is shell for command execution
ENTRYPOINT ["/bin/sh"]

# Default command shows Argyll version
CMD ["-c", "colprof --help | head -3"]