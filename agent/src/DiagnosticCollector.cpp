#include "DiagnosticCollector.h"

#include <chrono>
#include <cerrno>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

constexpr const char* kTarPath = "/usr/bin/tar";
constexpr std::size_t kMaximumBundles = 5;
constexpr std::uint32_t kRecentLogLines = 200;

std::string Hostname() {
    char hostname[256]{};
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return "unknown";
    }
    hostname[sizeof(hostname) - 1] = '\0';
    return hostname;
}

bool WriteTextFile(const std::filesystem::path& path,
                   const std::string& contents, std::string* error) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output || !(output << contents)) {
        *error = "failed to write diagnostic file: " + path.filename().string();
        return false;
    }
    return true;
}

bool CreateTarGzip(const std::filesystem::path& source,
                   const std::filesystem::path& destination,
                   std::string* error) {
    const pid_t child = fork();
    if (child < 0) {
        *error = "failed to fork tar process";
        return false;
    }
    if (child == 0) {
        execl(kTarPath, kTarPath, "-czf", destination.c_str(), "-C",
              source.c_str(), ".", static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    pid_t waited = -1;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        *error = "tar failed to create diagnostic archive";
        return false;
    }
    return true;
}

std::string Join(const std::vector<std::string>& values) {
    std::ostringstream output;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) output << ',';
        output << values[index];
    }
    return output.str();
}

}  // namespace

DiagnosticCollector::DiagnosticCollector(
    std::filesystem::path root_directory,
    const ProcessCollector* process_collector,
    const NetworkCollector* network_collector,
    const LogCollector* log_collector)
    : root_directory_(std::move(root_directory)),
      process_collector_(process_collector),
      network_collector_(network_collector),
      log_collector_(log_collector) {}

DiagnosticCollector::~DiagnosticCollector() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const StoredBundle& bundle : bundles_) {
        std::error_code ignored;
        std::filesystem::remove(bundle.path, ignored);
    }
}

bool DiagnosticCollector::PrepareRoot(std::string* error) const {
    std::error_code filesystem_error;
    std::filesystem::create_directories(root_directory_, filesystem_error);
    if (filesystem_error) {
        *error = "failed to create diagnostic directory: " +
                 filesystem_error.message();
        return false;
    }
    std::filesystem::permissions(
        root_directory_, std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace, filesystem_error);
    if (filesystem_error) {
        *error = "failed to secure diagnostic directory: " +
                 filesystem_error.message();
        return false;
    }
    return true;
}

bool DiagnosticCollector::CreateBundle(const SystemMetrics& metrics,
                                       DiagnosticBundleInfo* bundle,
                                       std::string* error) {
    if (bundle == nullptr || error == nullptr || process_collector_ == nullptr ||
        network_collector_ == nullptr || log_collector_ == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!PrepareRoot(error)) return false;

    const auto now = std::chrono::system_clock::now();
    const std::uint64_t timestamp = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count());
    const std::string id = std::to_string(timestamp) + "-" +
                           std::to_string(getpid()) + "-" +
                           std::to_string(++sequence_);
    const std::filesystem::path staging = root_directory_ / ("staging-" + id);
    const std::filesystem::path archive =
        root_directory_ / ("edgescope-diagnostic-" + id + ".tar.gz");
    std::error_code filesystem_error;
    if (!std::filesystem::create_directory(staging, filesystem_error)) {
        *error = "failed to create diagnostic staging directory: " +
                 filesystem_error.message();
        return false;
    }
    std::filesystem::permissions(staging, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace,
                                 filesystem_error);
    if (filesystem_error) {
        *error = "failed to secure diagnostic staging directory: " +
                 filesystem_error.message();
        std::filesystem::remove_all(staging, filesystem_error);
        return false;
    }

    bool success = true;
    std::ostringstream failures;
    struct utsname system_name {};
    uname(&system_name);
    std::ostringstream system;
    system << "hostname: " << Hostname() << '\n'
           << "kernel: " << system_name.release << '\n'
           << "cpu_usage_percent: " << std::fixed << std::setprecision(2)
           << metrics.cpu_usage_percent << '\n'
           << "memory_total_bytes: " << metrics.memory_total_bytes << '\n'
           << "memory_available_bytes: " << metrics.memory_available_bytes << '\n'
           << "memory_used_bytes: " << metrics.memory_used_bytes << '\n'
           << "load_average: " << metrics.load_average_1m << ' '
           << metrics.load_average_5m << ' ' << metrics.load_average_15m << '\n'
           << "uptime_seconds: " << metrics.uptime_seconds << '\n';
    success = WriteTextFile(staging / "system_info.txt", system.str(), error);
    if (success) success = WriteTextFile(staging / "agent_version.txt",
                                         "1.0.0\n", error);

    std::vector<ProcessInfo> processes;
    std::string collect_error;
    if (success && process_collector_->ListProcesses(&processes,
                                                     &collect_error)) {
        std::ostringstream output;
        output << "PID\tPPID\tSTATE\tCPU%\tRSS_BYTES\tTHREADS\tNAME\tCOMMAND\n";
        for (const ProcessInfo& process : processes) {
            output << process.pid << '\t' << process.ppid << '\t'
                   << process.state << '\t' << process.cpu_usage_percent << '\t'
                   << process.resident_memory_bytes << '\t'
                   << process.thread_count << '\t' << process.name << '\t'
                   << process.command_line << '\n';
        }
        success = WriteTextFile(staging / "processes.txt", output.str(), error);
    } else if (success) {
        failures << "processes: " << collect_error << '\n';
    }

    std::vector<NetworkInterfaceInfo> interfaces;
    std::vector<TcpConnectionInfo> connections;
    std::ostringstream network;
    if (success && network_collector_->GetInterfaces(&interfaces,
                                                     &collect_error)) {
        network << "INTERFACES\n";
        for (const auto& interface : interfaces) {
            network << interface.name << '\t' << interface.state << '\t'
                    << Join(interface.ipv4_addresses) << '\t'
                    << Join(interface.ipv6_addresses) << '\t'
                    << interface.rx_bytes << '\t' << interface.tx_bytes << '\n';
        }
    } else if (success) {
        failures << "interfaces: " << collect_error << '\n';
    }
    if (success && network_collector_->ListTcpConnections(&connections,
                                                          &collect_error)) {
        network << "\nTCP_CONNECTIONS\n";
        for (const auto& connection : connections) {
            network << connection.protocol << '\t' << connection.local_address
                    << ':' << connection.local_port << '\t'
                    << connection.remote_address << ':' << connection.remote_port
                    << '\t' << connection.state << '\t' << connection.pid << '\n';
        }
    } else if (success) {
        failures << "tcp_connections: " << collect_error << '\n';
    }
    if (success) success = WriteTextFile(staging / "network.txt",
                                         network.str(), error);

    const std::filesystem::path logs_directory = staging / "recent_logs";
    if (success) {
        std::filesystem::create_directory(logs_directory, filesystem_error);
        if (filesystem_error) {
            *error = "failed to create recent_logs directory";
            success = false;
        }
    }
    if (success) {
        for (const LogSourceInfo& source : log_collector_->ListLogs()) {
            if (!source.available) continue;
            LogReadResult result;
            LogCollectorError code = LogCollectorError::kNone;
            if (!log_collector_->ReadLog(source.id, kRecentLogLines, "", &result,
                                         &code, &collect_error)) {
                failures << "log " << source.id << ": " << collect_error << '\n';
                continue;
            }
            std::ostringstream contents;
            for (const std::string& line : result.lines) contents << line << '\n';
            if (!WriteTextFile(logs_directory / (source.id + ".log"),
                               contents.str(), error)) {
                success = false;
                break;
            }
        }
    }
    if (success) success = WriteTextFile(staging / "collection_errors.txt",
                                         failures.str(), error);
    if (success) success = CreateTarGzip(staging, archive, error);
    std::filesystem::remove_all(staging, filesystem_error);
    if (!success) {
        std::filesystem::remove(archive, filesystem_error);
        return false;
    }

    const std::uint64_t size = std::filesystem::file_size(archive,
                                                          filesystem_error);
    if (filesystem_error) {
        *error = "failed to determine diagnostic archive size";
        std::filesystem::remove(archive, filesystem_error);
        return false;
    }
    DiagnosticBundleInfo info{id, archive.filename().string(), size, timestamp};
    bundles_.push_back({info, archive});
    RemoveOldBundles();
    *bundle = std::move(info);
    error->clear();
    return true;
}

bool DiagnosticCollector::GetBundlePath(const std::string& id,
                                        std::filesystem::path* path,
                                        std::uint64_t* size_bytes,
                                        std::string* error) const {
    if (path == nullptr || size_bytes == nullptr || error == nullptr) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const StoredBundle& bundle : bundles_) {
        if (bundle.info.id == id) {
            *path = bundle.path;
            *size_bytes = bundle.info.size_bytes;
            error->clear();
            return true;
        }
    }
    *error = "diagnostic bundle not found";
    return false;
}

void DiagnosticCollector::RemoveOldBundles() {
    while (bundles_.size() > kMaximumBundles) {
        std::error_code ignored;
        std::filesystem::remove(bundles_.front().path, ignored);
        bundles_.pop_front();
    }
}
