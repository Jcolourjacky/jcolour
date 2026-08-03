#pragma once

#include <string>
#include <vector>

namespace jcolour {
namespace argyll {

struct RunResult {
    int         exit_code  = -1;
    std::string stdout_out;
    std::string stderr_out;
    bool        timed_out  = false;
};

// Returns the full path to an Argyll tool, or "" if not found.
// Search order: /opt/homebrew/bin, /usr/local/bin, ~/bin, PATH.
std::string find_tool(const std::string& name);

// Returns the Argyll version string (e.g. "2.3.1"), or "" if not installed.
std::string version();

// Run an Argyll tool with the given arguments.
// timeout_sec <= 0 means no timeout.
RunResult run(const std::string& tool_path,
              const std::vector<std::string>& args,
              int timeout_sec = 60);

// High-level wrappers — locate the tool automatically.
// Return RunResult with exit_code == -2 if the tool is not found.

// targen: generate test chart target
//   output_base: path without extension (targen appends .ti1)
//   extra_args:  e.g. {"-d","5","-G","-g","11","-f","50"}
RunResult targen(const std::string& output_base,
                 const std::vector<std::string>& extra_args = {},
                 int timeout_sec = 120);

// colprof: build ICC profile from measured ti3
//   output_base: path without extension
//   extra_args:  e.g. {"-D","MyPrinter","-qm","-aS"}
RunResult colprof(const std::string& output_base,
                  const std::vector<std::string>& extra_args = {},
                  int timeout_sec = 300);

// profcheck: verify ICC profile accuracy against ti3
//   profile:    path to .icc file
//   ti3:        path to .ti3 file
RunResult profcheck(const std::string& profile,
                    const std::string& ti3,
                    const std::vector<std::string>& extra_args = {},
                    int timeout_sec = 60);

}  // namespace argyll
}  // namespace jcolour
