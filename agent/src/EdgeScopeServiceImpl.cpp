#include "EdgeScopeServiceImpl.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {

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

}  // namespace

grpc::Status EdgeScopeServiceImpl::GetAgentInfo(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::GetAgentInfoRequest* /*request*/,
    edgescope::v1::GetAgentInfoResponse* response) {
    response->set_hostname(GetHostname());
    response->set_kernel_version(GetKernelVersion());
    response->set_uptime_seconds(GetUptimeSeconds());
    response->set_agent_version("0.1.0");
    return grpc::Status::OK;
}

grpc::Status EdgeScopeServiceImpl::GetSystemMetrics(
    grpc::ServerContext* /*context*/,
    const edgescope::v1::GetSystemMetricsRequest* /*request*/,
    edgescope::v1::GetSystemMetricsResponse* response) {
    SystemMetrics metrics;
    std::string error;
    if (!system_collector_.Collect(&metrics, &error)) {
        return grpc::Status(grpc::StatusCode::INTERNAL, error);
    }

    response->set_cpu_usage_percent(metrics.cpu_usage_percent);
    response->set_memory_total_bytes(metrics.memory_total_bytes);
    response->set_memory_available_bytes(metrics.memory_available_bytes);
    response->set_memory_used_bytes(metrics.memory_used_bytes);
    response->set_load_average_1m(metrics.load_average_1m);
    response->set_load_average_5m(metrics.load_average_5m);
    response->set_load_average_15m(metrics.load_average_15m);
    response->set_uptime_seconds(metrics.uptime_seconds);
    return grpc::Status::OK;
}
