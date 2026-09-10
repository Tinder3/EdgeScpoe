#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ProcessInfo {
    std::int32_t pid = 0;
    std::int32_t ppid = 0;
    std::string name;
    std::string state;
    std::string command_line;
    double cpu_usage_percent = 0.0;
    std::uint64_t resident_memory_bytes = 0;
    std::uint64_t virtual_memory_bytes = 0;
    std::uint32_t thread_count = 0;
    std::uint64_t read_bytes = 0;
    std::uint64_t write_bytes = 0;
    std::uint64_t start_time_ticks = 0;
};

enum class ProcessCollectorError {
    kNone,
    kInvalidArgument,
    kNotFound,
    kPermissionDenied,
    kParseError,
    kIoError,
};

class ProcessCollector {
public:
    bool ListProcesses(std::vector<ProcessInfo>* processes,
                       std::string* error) const;
    bool GetProcess(std::int32_t pid, ProcessInfo* process,
                    ProcessCollectorError* error_code,
                    std::string* error) const;

    static bool ParseStatLine(const std::string& line, ProcessInfo* process,
                              std::uint64_t* process_cpu_ticks,
                              std::string* error);
    static bool ParseStatus(const std::string& contents, ProcessInfo* process,
                            std::string* error);
    static bool ParseIo(const std::string& contents, ProcessInfo* process,
                        std::string* error);
};
