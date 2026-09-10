#include "EdgeScopeServiceImpl.h"

#include "AgentLogger.h"

#include <cerrno>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>
#include <chrono>
#include <filesystem>
#include <sys/stat.h>
#include <thread>

namespace {

constexpr std::uint32_t kDefaultStreamIntervalMs = 1000;
constexpr std::uint32_t kMinStreamIntervalMs = 1000;
constexpr std::uint32_t kMaxStreamIntervalMs = 60000;

std::string ReadFirstLine(const char* path) {
    std::ifstream in(path);
    std::string line;
    if (!in || !std::getline(in, line)) {
        return {};
    }
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
    return line;
}

std::string GetHostname() {
    char buffer[256];
    if (gethostname(buffer, sizeof(buffer)) != 0) {
        return std::string("unknown (") + std::strerror(errno) + ")";
    }
    buffer[sizeof(buffer) - 1] = '\0';
    return std::string(buffer);
}

std::string GetKernelVersion() {
    std::string version = ReadFirstLine("/proc/sys/kernel/osrelease");
    if (version.empty()) {
        return "unknown";
    }
    return version;
}

std::uint64_t GetUptimeSeconds() {
    std::string line = ReadFirstLine("/proc/uptime");
    if (line.empty()) {
        return 0;
    }
    std::istringstream iss(line);
    double uptime = 0.0;
    iss >> uptime;
    if (!iss) {
        return 0;
    }
    return static_cast<std::uint64_t>(uptime);
}

void FillSystemMetricsResponse(
    const SystemMetrics& metrics,
    edgescope::v1::GetSystemMetricsResponse* response) {
    response->set_cpu_usage_percent(metrics.cpu_usage_percent);
    response->set_memory_total_bytes(metrics.memory_total_bytes);
    response->set_memory_available_bytes(metrics.memory_available_bytes);
    response->set_memory_used_bytes(metrics.memory_used_bytes);
    response->set_load_average_1m(metrics.load_average_1m);
    response->set_load_average_5m(metrics.load_average_5m);
    response->set_load_average_15m(metrics.load_average_15m);
    response->set_uptime_seconds(metrics.uptime_seconds);
}

void FillProcessInfo(const ProcessInfo& process,
                     edgescope::v1::ProcessInfo* response) {
    response->set_pid(process.pid);
    response->set_ppid(process.ppid);
    response->set_name(process.name);
    response->set_state(process.state);
    response->set_command_line(process.command_line);
    response->set_cpu_usage_percent(process.cpu_usage_percent);
    response->set_resident_memory_bytes(process.resident_memory_bytes);
    response->set_virtual_memory_bytes(process.virtual_memory_bytes);
    response->set_thread_count(process.thread_count);
    response->set_read_bytes(process.read_bytes);
    response->set_write_bytes(process.write_bytes);
    response->set_start_time_ticks(process.start_time_ticks);
}

grpc::Status ProcessCollectorStatus(ProcessCollectorError error_code,
                                    const std::string& error) {
    switch (error_code) {
        case ProcessCollectorError::kInvalidArgument:
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, error);
        case ProcessCollectorError::kNotFound:
            return grpc::Status(grpc::StatusCode::NOT_FOUND, error);
        case ProcessCollectorError::kPermissionDenied:
            return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, error);
        case ProcessCollectorError::kParseError:
        case ProcessCollectorError::kIoError:
            return grpc::Status(grpc::StatusCode::INTERNAL, error);
        case ProcessCollectorError::kNone:
            break;
    }
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "unknown process collector error");
}

grpc::Status ProcessControlStatus(ProcessControlError error_code,
                                  const std::string& error) {
    switch (error_code) {
        case ProcessControlError::kInvalidArgument:
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, error);
        case ProcessControlError::kNotFound:
            return grpc::Status(grpc::StatusCode::NOT_FOUND, error);
        case ProcessControlError::kPermissionDenied:
            return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, error);
        case ProcessControlError::kInternal:
            return grpc::Status(grpc::StatusCode::INTERNAL, error);
        case ProcessControlError::kNone:
            break;
    }
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "unknown process control error");
}

grpc::Status LogStatus(LogCollectorError error_code, const std::string& error) {
    switch (error_code) {
        case LogCollectorError::kInvalidArgument:
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, error);
        case LogCollectorError::kNotFound:
            return grpc::Status(grpc::StatusCode::NOT_FOUND, error);
        case LogCollectorError::kPermissionDenied:
            return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, error);
        case LogCollectorError::kIoError:
            return grpc::Status(grpc::StatusCode::INTERNAL, error);
        case LogCollectorError::kNone:
            break;
    }
    return grpc::Status(grpc::StatusCode::INTERNAL, "unknown log error");
}

grpc::Status ServiceStatus(ServiceManagerError error_code,
                           const std::string& error) {
    switch (error_code) {
        case ServiceManagerError::kInvalidArgument:
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, error);
        case ServiceManagerError::kNotAllowed:
        case ServiceManagerError::kPermissionDenied:
            return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, error);
        case ServiceManagerError::kNotFound:
            return grpc::Status(grpc::StatusCode::NOT_FOUND, error);
        case ServiceManagerError::kCommandFailed:
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, error);
        case ServiceManagerError::kNone:
            break;
    }
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "unknown service manager error");
}

}  // namespace

EdgeScopeServiceImpl::EdgeScopeServiceImpl(const AgentConfig& config)
    : metrics_sampler_(config.sample_interval),
      log_collector_(config.log_file),
      diagnostic_collector_(config.diagnostic_directory, &process_collector_,
                            &network_collector_, &log_collector_),
      service_manager_(config.allowed_services) {}

grpc::Status EdgeScopeServiceImpl::GetAgentInfo(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::GetAgentInfoRequest* /*request*/,
    edgescope::v1::GetAgentInfoResponse* response) {
    response->set_hostname(GetHostname());
    response->set_kernel_version(GetKernelVersion());
    response->set_uptime_seconds(GetUptimeSeconds());
    response->set_agent_version("1.0.0");
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::ListProcesses(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::ListProcessesRequest* /*request*/,
    edgescope::v1::ListProcessesResponse* response) {
    std::vector<ProcessInfo> processes;
    std::string error;
    if (!process_collector_.ListProcesses(&processes, &error)) {
        return grpc::Status(grpc::StatusCode::INTERNAL, error);
    }

    for (const ProcessInfo& process : processes) {
        FillProcessInfo(process, response->add_processes());
    }
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::GetProcessDetails(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::GetProcessDetailsRequest* request,
    edgescope::v1::GetProcessDetailsResponse* response) {
    ProcessInfo process;
    ProcessCollectorError error_code = ProcessCollectorError::kNone;
    std::string error;
    if (!process_collector_.GetProcess(request->pid(), &process, &error_code,
                                       &error)) {
        return ProcessCollectorStatus(error_code, error);
    }

    FillProcessInfo(process, response->mutable_process());
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::ControlProcess(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::ControlProcessRequest* request,
    edgescope::v1::ControlProcessResponse* /*response*/) {
    ProcessControlAction action;
    switch (request->action()) {
        case edgescope::v1::PROCESS_ACTION_TERMINATE:
            action = ProcessControlAction::kTerminate;
            break;
        case edgescope::v1::PROCESS_ACTION_KILL:
            action = ProcessControlAction::kKill;
            break;
        case edgescope::v1::PROCESS_ACTION_STOP:
            action = ProcessControlAction::kStop;
            break;
        case edgescope::v1::PROCESS_ACTION_CONTINUE:
            action = ProcessControlAction::kContinue;
            break;
        case edgescope::v1::PROCESS_ACTION_UNSPECIFIED:
        default:
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "a supported process action is required");
    }

    ProcessControlError error_code = ProcessControlError::kNone;
    std::string error;
    if (!process_controller_.Control(request->pid(), action, &error_code,
                                     &error)) {
        AgentLogger::Warn("process control failed for pid " +
                          std::to_string(request->pid()) + ": " + error);
        return ProcessControlStatus(error_code, error);
    }

    AgentLogger::Info("process control succeeded for pid " +
                      std::to_string(request->pid()));
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::GetNetworkInterfaces(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::GetNetworkInterfacesRequest* /*request*/,
    edgescope::v1::GetNetworkInterfacesResponse* response) {
    std::vector<NetworkInterfaceInfo> interfaces;
    std::string error;
    if (!network_collector_.GetInterfaces(&interfaces, &error)) {
        return grpc::Status(grpc::StatusCode::INTERNAL, error);
    }
    for (const NetworkInterfaceInfo& interface : interfaces) {
        auto* output = response->add_interfaces();
        output->set_name(interface.name);
        for (const std::string& address : interface.ipv4_addresses) {
            output->add_ipv4_addresses(address);
        }
        for (const std::string& address : interface.ipv6_addresses) {
            output->add_ipv6_addresses(address);
        }
        output->set_rx_bytes(interface.rx_bytes);
        output->set_tx_bytes(interface.tx_bytes);
        output->set_state(interface.state);
    }
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::ListTcpConnections(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::ListTcpConnectionsRequest* /*request*/,
    edgescope::v1::ListTcpConnectionsResponse* response) {
    std::vector<TcpConnectionInfo> connections;
    std::string error;
    if (!network_collector_.ListTcpConnections(&connections, &error)) {
        return grpc::Status(grpc::StatusCode::INTERNAL, error);
    }
    for (const TcpConnectionInfo& connection : connections) {
        auto* output = response->add_connections();
        output->set_protocol(connection.protocol);
        output->set_local_address(connection.local_address);
        output->set_local_port(connection.local_port);
        output->set_remote_address(connection.remote_address);
        output->set_remote_port(connection.remote_port);
        output->set_state(connection.state);
        output->set_inode(connection.inode);
        output->set_pid(connection.pid);
    }
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::ListLogs(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::ListLogsRequest* /*request*/,
    edgescope::v1::ListLogsResponse* response) {
    for (const LogSourceInfo& log : log_collector_.ListLogs()) {
        auto* output = response->add_logs();
        output->set_id(log.id);
        output->set_display_name(log.display_name);
        output->set_available(log.available);
    }
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::ReadLog(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::ReadLogRequest* request,
    edgescope::v1::ReadLogResponse* response) {
    LogReadResult result;
    LogCollectorError error_code = LogCollectorError::kNone;
    std::string error;
    if (!log_collector_.ReadLog(request->log_id(), request->max_lines(),
                                request->keyword(), &result, &error_code,
                                &error)) {
        return LogStatus(error_code, error);
    }
    for (const std::string& line : result.lines) {
        response->add_lines(line);
    }
    response->set_truncated(result.truncated);
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::StreamLog(
    grpc::ServerContext* context,
    const edgescope::v1::StreamLogRequest* request,
    grpc::ServerWriter<edgescope::v1::LogLine>* writer) {
    if (request->keyword().size() > 256) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "keyword is too long");
    }
    std::string path;
    LogCollectorError error_code = LogCollectorError::kNone;
    std::string error;
    if (!log_collector_.ResolveLogPath(request->log_id(), &path, &error_code,
                                       &error)) {
        return LogStatus(error_code, error);
    }

    struct stat file_status {};
    if (stat(path.c_str(), &file_status) != 0) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND,
                            "log file is not available");
    }
    constexpr std::uint64_t kStreamReadSize = 64 * 1024;
    const std::uint64_t initial_size =
        static_cast<std::uint64_t>(file_status.st_size);
    std::uint64_t offset = request->start_at_end()
                               ? initial_size
                               : initial_size > kStreamReadSize
                                     ? initial_size - kStreamReadSize
                                     : 0;
    ino_t inode = file_status.st_ino;
    std::string partial_line;
    while (!context->IsCancelled()) {
        if (stat(path.c_str(), &file_status) != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
        }
        if (file_status.st_ino != inode ||
            static_cast<std::uint64_t>(file_status.st_size) < offset) {
            inode = file_status.st_ino;
            offset = 0;
            partial_line.clear();
        }
        if (static_cast<std::uint64_t>(file_status.st_size) > offset) {
            std::ifstream input(path, std::ios::binary);
            input.seekg(static_cast<std::streamoff>(offset));
            std::array<char, kStreamReadSize> buffer{};
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::size_t count = static_cast<std::size_t>(input.gcount());
            std::string appended(buffer.data(), count);
            offset += count;
            partial_line += appended;
            std::size_t newline = std::string::npos;
            while ((newline = partial_line.find('\n')) != std::string::npos) {
                std::string line = partial_line.substr(0, newline);
                partial_line.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.size() > 4096) line.resize(4096);
                if (!request->keyword().empty() &&
                    line.find(request->keyword()) == std::string::npos) {
                    continue;
                }
                edgescope::v1::LogLine output;
                output.set_line(LogCollector::SanitizeUtf8(line));
                if (!writer->Write(output)) return grpc::Status::OK;
            }
            if (partial_line.size() > 4096) partial_line.erase(0, partial_line.size() - 4096);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::ListServices(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::ListServicesRequest* /*request*/,
    edgescope::v1::ListServicesResponse* response) {
    std::vector<ServiceInfo> services;
    std::string error;
    if (!service_manager_.ListServices(&services, &error)) {
        AgentLogger::Error("failed to list services: " + error);
        return grpc::Status(grpc::StatusCode::INTERNAL, error);
    }
    for (const ServiceInfo& service : services) {
        auto* output = response->add_services();
        output->set_name(service.name);
        output->set_description(service.description);
        output->set_load_state(service.load_state);
        output->set_active_state(service.active_state);
        output->set_sub_state(service.sub_state);
    }
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::ControlService(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::ControlServiceRequest* request,
    edgescope::v1::ControlServiceResponse* /*response*/) {
    ServiceAction action;
    switch (request->action()) {
        case edgescope::v1::SERVICE_ACTION_START:
            action = ServiceAction::kStart;
            break;
        case edgescope::v1::SERVICE_ACTION_STOP:
            action = ServiceAction::kStop;
            break;
        case edgescope::v1::SERVICE_ACTION_RESTART:
            action = ServiceAction::kRestart;
            break;
        case edgescope::v1::SERVICE_ACTION_UNSPECIFIED:
        default:
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "a supported service action is required");
    }
    ServiceManagerError error_code = ServiceManagerError::kNone;
    std::string error;
    if (!service_manager_.ControlService(request->name(), action, &error_code,
                                         &error)) {
        AgentLogger::Warn("service control failed for " + request->name() +
                          ": " + error);
        return ServiceStatus(error_code, error);
    }
    AgentLogger::Info("service control succeeded for " + request->name());
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::CreateDiagnosticBundle(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::CreateDiagnosticBundleRequest* /*request*/,
    edgescope::v1::CreateDiagnosticBundleResponse* response) {
    SystemMetrics metrics;
    std::uint64_t generation = 0;
    std::string error;
    if (!metrics_sampler_.GetLatest(&metrics, &generation, &error)) {
        return grpc::Status(grpc::StatusCode::INTERNAL, error);
    }
    DiagnosticBundleInfo bundle;
    if (!diagnostic_collector_.CreateBundle(metrics, &bundle, &error)) {
        AgentLogger::Error("failed to create diagnostic bundle: " + error);
        return grpc::Status(grpc::StatusCode::INTERNAL, error);
    }
    response->set_bundle_id(bundle.id);
    response->set_filename(bundle.filename);
    response->set_total_size_bytes(bundle.size_bytes);
    response->set_created_at_unix_seconds(bundle.created_at_unix_seconds);
    AgentLogger::Info("created diagnostic bundle " + bundle.id + " (" +
                      std::to_string(bundle.size_bytes) + " bytes)");
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::DownloadDiagnosticBundle(
    grpc::ServerContext* context,
    const edgescope::v1::DownloadDiagnosticBundleRequest* request,
    grpc::ServerWriter<edgescope::v1::DiagnosticChunk>* writer) {
    if (request->bundle_id().empty()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "bundle_id is required");
    }
    std::filesystem::path path;
    std::uint64_t total_size = 0;
    std::string error;
    if (!diagnostic_collector_.GetBundlePath(request->bundle_id(), &path,
                                             &total_size, &error)) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, error);
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            "failed to open diagnostic bundle");
    }
    constexpr std::size_t kChunkSize = 64 * 1024;
    std::array<char, kChunkSize> buffer{};
    std::uint64_t offset = 0;
    while (!context->IsCancelled() && input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count <= 0) break;
        edgescope::v1::DiagnosticChunk chunk;
        chunk.set_data(buffer.data(), static_cast<std::size_t>(count));
        chunk.set_offset(offset);
        chunk.set_total_size_bytes(total_size);
        offset += static_cast<std::uint64_t>(count);
        chunk.set_eof(offset == total_size);
        if (!writer->Write(chunk)) return grpc::Status::OK;
    }
    if (context->IsCancelled()) return grpc::Status::OK;
    if (input.bad() || offset != total_size) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            "failed while reading diagnostic bundle");
    }
    AgentLogger::Info("downloaded diagnostic bundle " + request->bundle_id());
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::GetSystemMetrics(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::GetSystemMetricsRequest* /*request*/,
    edgescope::v1::GetSystemMetricsResponse* response) {
    SystemMetrics metrics;
    std::uint64_t generation = 0;
    std::string error;
    if (!metrics_sampler_.GetLatest(&metrics, &generation, &error)) {
        return grpc::Status(grpc::StatusCode::INTERNAL, error);
    }

    FillSystemMetricsResponse(metrics, response);
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::StreamSystemMetrics(
    grpc::ServerContext* context,
    const edgescope::v1::StreamSystemMetricsRequest* request,
    grpc::ServerWriter<edgescope::v1::GetSystemMetricsResponse>* writer) {
    std::uint32_t interval_ms = request->interval_ms();
    if (interval_ms == 0) {
        interval_ms = kDefaultStreamIntervalMs;
    }
    if (interval_ms < kMinStreamIntervalMs ||
        interval_ms > kMaxStreamIntervalMs) {
        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            "interval_ms must be between 1000 and 60000");
    }

    SystemMetrics metrics;
    std::uint64_t generation = 0;
    std::string error;
    if (!metrics_sampler_.GetLatest(&metrics, &generation, &error)) {
        return grpc::Status(grpc::StatusCode::INTERNAL, error);
    }

    edgescope::v1::GetSystemMetricsResponse response;
    FillSystemMetricsResponse(metrics, &response);
    if (!writer->Write(response)) {
        return grpc::Status::OK;
    }

    const std::uint64_t sampler_interval_ms =
        static_cast<std::uint64_t>(metrics_sampler_.sample_interval().count());
    const std::uint64_t samples_per_response =
        (interval_ms + sampler_interval_ms - 1) / sampler_interval_ms;
    std::uint64_t last_sent_generation = generation;
    while (!context->IsCancelled()) {
        if (!metrics_sampler_.WaitForNext(generation, &metrics, &generation,
                                          &error)) {
            return grpc::Status(grpc::StatusCode::INTERNAL, error);
        }
        if (generation - last_sent_generation < samples_per_response) {
            continue;
        }

        response.Clear();
        FillSystemMetricsResponse(metrics, &response);
        if (!writer->Write(response)) {
            return grpc::Status::OK;
        }
        last_sent_generation = generation;
    }

    return grpc::Status::OK;
}
