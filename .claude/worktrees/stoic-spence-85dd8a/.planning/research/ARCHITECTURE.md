# ARCHITECTURE.md - Architecture for Personal Development Environment

## System Overview

The personal development environment is structured as a collection of modular projects and tools that interoperate to create an AI-enhanced development workflow. Rather than a traditional application architecture, this is a metasystem that coordinates various tools and environments.

## Physical Structure

### Directory Organization
- **Root Level**: Personal home directory with project-specific subdirectories
- **Project Isolation**: Separate directories for different projects (e.g., everything-claude-code, MyOpenClaw)
- **Tool Integration**: Standardized locations for tools and configurations
- **Shared Resources**: Common libraries and utilities accessible across projects

### Component Boundaries

#### AI Tool Layer
- **Claude Code**: Primary AI-assisted development environment
- **Ollama**: Local LLM execution environment
- **External APIs**: Connections to cloud-based AI services

#### Development Environment Layer
- **Shell Environment**: Zsh configuration and customizations
- **Editor Integration**: VS Code, Cursor, and other editor configurations
- **Terminal Setup**: iTerm2 configurations and profiles

#### Project Management Layer
- **Git Workflows**: Branching and merging strategies
- **Package Management**: npm, pip, and other dependency systems
- **Virtual Environments**: Isolated Python and Node.js environments

## Data Flow

### Information Flow
1. **Input Sources**: User requests, external data, AI responses
2. **Processing**: Local computation, API calls to AI services
3. **Storage**: Files, Git repositories, AI context stores
4. **Output**: Code, documentation, automated tasks

### Context Management
- **Session Persistence**: Maintaining context across CLI sessions
- **Project Context**: Isolating context between different projects
- **Knowledge Accumulation**: Learning and storing patterns over time

## Suggested Build Order

1. **Foundation**: Establish core tooling and environment consistency
2. **AI Integration**: Implement AI tool connections and workflows
3. **Automation**: Add task automation and workflow improvements
4. **Optimization**: Fine-tune performance and user experience

## Integration Points

### External Services
- **GitHub**: Version control and code hosting
- **AI Providers**: Anthropic Claude, OpenAI, open-source models via Ollama
- **Cloud Services**: Storage, compute, and specialized services

### Internal Connections
- **Cross-Project Utilities**: Shared scripts and configurations
- **Knowledge Base**: Centralized information storage and retrieval
- **Credential Management**: Secure handling of sensitive information