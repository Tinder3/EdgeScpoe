#include "ProcessCollector.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <unistd.h>

namespace {

constexpr std::uint64_t kBytesPerKilobyte = 1024;

bool IsNumeric(const std::string& value) {
    return !value.empty() &&
           std::all_of(value.begin(), value.end(),
                       [](unsigned char character) {
                           return character >= '0' && character <= '9';
                       });
}

bool ReadFile(const std::filesystem::path& path, std::string* contents,
              int* saved_errno) {
    errno = 0;
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        *saved_errno = errno;
        return false;
    }

    std::ostringstream output;
    output << input.rdbuf();
    if (input.bad()) {
        *saved_errno = EIO;
        return false;
    }
    *contents = output.str();
    *saved_errno = 0;
    return true;
}

ProcessCollectorError ErrorFromErrno(int error_number) {
    if (error_number == ENOENT || error_number == ESRCH) {
        return ProcessCollectorError::kNotFound;
    }
    if (error_number == EACCES || error_number == EPERM) {
        return ProcessCollectorError::kPermissionDenied;
    }
    return ProcessCollectorError::kIoError;
}

bool ParseKilobyteField(const std::string& line, const std::string& expected_key,
                        std::uint64_t* bytes) {
    std::istringstream stream(line);
    std::string key;
    std::uint64_t value = 0;
    std::string unit;
    if (!(stream >> key >> value >> unit) || key != expected_key ||
        unit != "kB" ||
        value > std::numeric_limits<std::uint64_t>::max() /
                    kBytesPerKilobyte) {
        return false;
    }
    *bytes = value * kBytesPerKilobyte;
    return true;
}

std::string ReadCommandLine(const std::filesystem::path& path,
                            const std::string& process_name) {
    std::string command_line;
    int saved_errno = 0;
    if (!ReadFile(path, &command_line, &saved_errno) || command_line.empty()) {
        return "[" + process_name + "]";
    }

    std::replace(command_line.begin(), command_line.end(), '\0', ' ');
    while (!command_line.empty() && command_line.back() == ' ') {
        command_line.pop_back();
    }
    return command_line.empty() ? "[" + process_name + "]" : command_line;
}

double CalculateCpuUsage(std::uint64_t process_cpu_ticks,
                         std::uint64_t start_time_ticks) {
    std::ifstream uptime_input("/proc/uptime");
    double uptime_seconds = 0.0;
    if (!uptime_input || !(uptime_input >> uptime_seconds) ||
        !std::isfinite(uptime_seconds) || uptime_seconds <= 0.0) {
        return 0.0;
    }

    const long ticks_per_second = sysconf(_SC_CLK_TCK);
    if (ticks_per_second <= 0) {
        return 0.0;
    }

    const double process_seconds =
        static_cast<double>(process_cpu_ticks) / ticks_per_second;
    const double start_seconds =
        static_cast<double>(start_time_ticks) / ticks_per_second;
    const double elapsed_seconds = uptime_seconds - start_seconds;
    if (elapsed_seconds <= 0.0) {
        return 0.0;
    }
    return process_seconds / elapsed_seconds * 100.0;
}

}  // namespace

bool ProcessCollector::ParseStatLine(const std::string& line,
                                     ProcessInfo* process,
                                     std::uint64_t* process_cpu_ticks,
                                     std::string* error) {
    if (process == nullptr || process_cpu_ticks == nullptr || error == nullptr) {
        return false;
    }

    const std::size_t open_parenthesis = line.find('(');
    const std::size_t close_parenthesis = line.rfind(')');
    if (open_parenthesis == std::string::npos ||
        close_parenthesis == std::string::npos ||
        close_parenthesis <= open_parenthesis || close_parenthesis + 2 > line.size()) {
        *error = "invalid /proc/[pid]/stat process name";
        return false;
    }

    std::istringstream pid_stream(line.substr(0, open_parenthesis));
    std::int64_t parsed_pid = 0;
    if (!(pid_stream >> parsed_pid) || parsed_pid <= 0 ||
        parsed_pid > std::numeric_limits<std::int32_t>::max()) {
        *error = "invalid pid in /proc/[pid]/stat";
        return false;
    }

    process->pid = static_cast<std::int32_t>(parsed_pid);
    process->name = line.substr(open_parenthesis + 1,
                                close_parenthesis - open_parenthesis - 1);

    std::istringstream fields(line.substr(close_parenthesis + 1));
    char state = '\0';
    std::int64_t parsed_ppid = 0;
    if (!(fields >> state >> parsed_ppid) || parsed_ppid < 0 ||
        parsed_ppid > std::numeric_limits<std::int32_t>::max()) {
        *error = "invalid state or ppid in /proc/[pid]/stat";
        return false;
    }
    process->state.assign(1, state);
    process->ppid = static_cast<std::int32_t>(parsed_ppid);

    std::uint64_t ignored = 0;
    for (int field = 5; field <= 13; ++field) {
        if (!(fields >> ignored)) {
            *error = "missing fields in /proc/[pid]/stat";
            return false;
        }
    }

    std::uint64_t user_ticks = 0;
    std::uint64_t system_ticks = 0;
    if (!(fields >> user_ticks >> system_ticks)) {
        *error = "invalid cpu fields in /proc/[pid]/stat";
        return false;
    }
    if (user_ticks > std::numeric_limits<std::uint64_t>::max() - system_ticks) {
        *error = "cpu fields overflow in /proc/[pid]/stat";
        return false;
    }
    *process_cpu_ticks = user_ticks + system_ticks;

    for (int field = 16; field <= 19; ++field) {
        std::int64_t signed_ignored = 0;
        if (!(fields >> signed_ignored)) {
            *error = "missing scheduler fields in /proc/[pid]/stat";
            return false;
        }
    }

    std::uint64_t parsed_threads = 0;
    std::int64_t interval_timer = 0;
    if (!(fields >> parsed_threads >> interval_timer >> process->start_time_ticks) ||
        parsed_threads > std::numeric_limits<std::uint32_t>::max()) {
        *error = "invalid thread or start fields in /proc/[pid]/stat";
        return false;
    }
    process->thread_count = static_cast<std::uint32_t>(parsed_threads);
    return true;
}

bool ProcessCollector::ParseStatus(const std::string& contents,
                                   ProcessInfo* process, std::string* error) {
    if (process == nullptr || error == nullptr) {
        return false;
    }

    bool found_rss = false;
    bool found_virtual_memory = false;
    std::istringstream input(contents);
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            if (!ParseKilobyteField(line, "VmRSS:",
                                    &process->resident_memory_bytes)) {
                *error = "invalid VmRSS in /proc/[pid]/status";
                return false;
            }
            found_rss = true;
        } else if (line.rfind("VmSize:", 0) == 0) {
            if (!ParseKilobyteField(line, "VmSize:",
                                    &process->virtual_memory_bytes)) {
                *error = "invalid VmSize in /proc/[pid]/status";
                return false;
            }
            found_virtual_memory = true;
        } else if (line.rfind("Threads:", 0) == 0) {
            std::istringstream stream(line);
            std::string key;
            std::uint64_t threads = 0;
            if (!(stream >> key >> threads) || key != "Threads:" ||
                threads > std::numeric_limits<std::uint32_t>::max()) {
                *error = "invalid Threads in /proc/[pid]/status";
                return false;
            }
            process->thread_count = static_cast<std::uint32_t>(threads);
        }
    }

    // Kernel threads may legitimately omit the memory fields.
    if (!found_rss) {
        process->resident_memory_bytes = 0;
    }
    if (!found_virtual_memory) {
        process->virtual_memory_bytes = 0;
    }
    return true;
}

bool ProcessCollector::ParseIo(const std::string& contents, ProcessInfo* process,
                               std::string* error) {
    if (process == nullptr || error == nullptr) {
        return false;
    }

    std::istringstream input(contents);
    std::string key;
    std::uint64_t value = 0;
    while (input >> key >> value) {
        if (key == "read_bytes:") {
            process->read_bytes = value;
        } else if (key == "write_bytes:") {
            process->write_bytes = value;
        }
    }
    if (input.bad()) {
        *error = "failed to parse /proc/[pid]/io";
        return false;
    }
    return true;
}

bool ProcessCollector::GetProcess(std::int32_t pid, ProcessInfo* process,
                                  ProcessCollectorError* error_code,
                                  std::string* error) const {
    if (process == nullptr || error_code == nullptr || error == nullptr) {
        return false;
    }
    *process = ProcessInfo{};
    *error_code = ProcessCollectorError::kNone;
    error->clear();
    if (pid <= 0) {
        *error_code = ProcessCollectorError::kInvalidArgument;
        *error = "pid must be positive";
        return false;
    }

    const std::filesystem::path process_directory =
        std::filesystem::path("/proc") / std::to_string(pid);
    std::string stat_contents;
    int saved_errno = 0;
    if (!ReadFile(process_directory / "stat", &stat_contents, &saved_errno)) {
        *error_code = ErrorFromErrno(saved_errno);
        *error = "failed to read process stat for pid " + std::to_string(pid);
        return false;
    }

    std::uint64_t process_cpu_ticks = 0;
    if (!ParseStatLine(stat_contents, process, &process_cpu_ticks, error)) {
        *error_code = ProcessCollectorError::kParseError;
        return false;
    }

    std::string status_contents;
    if (!ReadFile(process_directory / "status", &status_contents, &saved_errno)) {
        *error_code = ErrorFromErrno(saved_errno);
        *error = "failed to read process status for pid " + std::to_string(pid);
        return false;
    }
    if (!ParseStatus(status_contents, process, error)) {
        *error_code = ProcessCollectorError::kParseError;
        return false;
    }

    process->command_line =
        ReadCommandLine(process_directory / "cmdline", process->name);

    std::string io_contents;
    if (ReadFile(process_directory / "io", &io_contents, &saved_errno)) {
        // I/O accounting can be unavailable even when the remaining process
        // data is readable, so it is treated as optional.
        std::string io_error;
        ParseIo(io_contents, process, &io_error);
    }

    process->cpu_usage_percent =
        CalculateCpuUsage(process_cpu_ticks, process->start_time_ticks);
    return true;
}

bool ProcessCollector::ListProcesses(std::vector<ProcessInfo>* processes,
                                     std::string* error) const {
    if (processes == nullptr || error == nullptr) {
        return false;
    }
    processes->clear();
    error->clear();

    std::error_code directory_error;
    std::filesystem::directory_iterator iterator("/proc", directory_error);
    if (directory_error) {
        *error = "failed to scan /proc: " + directory_error.message();
        return false;
    }

    const std::filesystem::directory_iterator end;
    while (iterator != end) {
        const std::string filename = iterator->path().filename().string();
        if (IsNumeric(filename)) {
            try {
                const long long parsed_pid = std::stoll(filename);
                if (parsed_pid > 0 &&
                    parsed_pid <= std::numeric_limits<std::int32_t>::max()) {
                    ProcessInfo process;
                    ProcessCollectorError process_error =
                        ProcessCollectorError::kNone;
                    std::string process_error_message;
                    if (GetProcess(static_cast<std::int32_t>(parsed_pid), &process,
                                   &process_error, &process_error_message)) {
                        processes->push_back(std::move(process));
                    }
                }
            } catch (const std::exception&) {
                // Ignore an invalid or out-of-range numeric directory name.
            }
        }

        iterator.increment(directory_error);
        if (directory_error) {
            directory_error.clear();
        }
    }

    std::sort(processes->begin(), processes->end(),
              [](const ProcessInfo& left, const ProcessInfo& right) {
                  return left.pid < right.pid;
              });
    return true;
}
