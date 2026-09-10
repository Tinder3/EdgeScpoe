#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct LogSourceInfo {
    std::string id;
    std::string display_name;
    bool available = false;
};

struct LogReadResult {
    std::vector<std::string> lines;
    bool truncated = false;
};

enum class LogCollectorError {
    kNone,
    kInvalidArgument,
    kNotFound,
    kPermissionDenied,
    kIoError,
};

class LogCollector {
public:
    explicit LogCollector(
        std::string agent_log_path = "/tmp/edgescope-agent.log");
    std::vector<LogSourceInfo> ListLogs() const;
    bool ReadLog(const std::string& log_id, std::uint32_t max_lines,
                 const std::string& keyword, LogReadResult* result,
                 LogCollectorError* error_code, std::string* error) const;
    bool ResolveLogPath(const std::string& log_id, std::string* path,
                        LogCollectorError* error_code,
                        std::string* error) const;

    static std::string SanitizeUtf8(const std::string& input);

private:
    struct AllowedLog {
        std::string id;
        std::string display_name;
        std::string path;
    };

    const AllowedLog* FindAllowedLog(const std::string& id) const;
    std::vector<AllowedLog> allowed_logs_;
};
