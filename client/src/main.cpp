#include "EdgeScopeClient.h"

#include <iomanip>
#include <iostream>
#include <string>

int main() {
    EdgeScopeClient client("127.0.0.1:50051");
    edgescope::v1::GetAgentInfoResponse agent_info;
    const grpc::Status agent_status = client.GetAgentInfo(&agent_info);

    if (!agent_status.ok()) {
        std::cerr << "GetAgentInfo RPC failed: " << agent_status.error_message()
                  << std::endl;
        return 1;
    }

    std::cout << "Connected to EdgeScope Agent" << std::endl;
    std::cout << "Hostname: " << agent_info.hostname() << std::endl;
    std::cout << "Kernel: " << agent_info.kernel_version() << std::endl;
    std::cout << "Uptime: " << agent_info.uptime_seconds() << " seconds" << std::endl;
    std::cout << "Agent Version: " << agent_info.agent_version() << std::endl;

    edgescope::v1::GetSystemMetricsResponse metrics;
    const grpc::Status metrics_status = client.GetSystemMetrics(&metrics);
    if (!metrics_status.ok()) {
        std::cerr << "GetSystemMetrics RPC failed: "
                  << metrics_status.error_message() << std::endl;
        return 1;
    }

    constexpr double kBytesPerGigabyte = 1024.0 * 1024.0 * 1024.0;
    std::cout << '\n' << "System Metrics" << std::endl;
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "CPU Usage: " << metrics.cpu_usage_percent() << "%" << std::endl;
    std::cout << "Memory Used: "
              << metrics.memory_used_bytes() / kBytesPerGigabyte << " GB / "
              << metrics.memory_total_bytes() / kBytesPerGigabyte << " GB"
              << std::endl;
    std::cout << "Memory Available: "
              << metrics.memory_available_bytes() / kBytesPerGigabyte << " GB"
              << std::endl;
    std::cout << std::setprecision(2);
    std::cout << "Load Average: " << metrics.load_average_1m() << " / "
              << metrics.load_average_5m() << " / "
              << metrics.load_average_15m() << std::endl;
    std::cout << "Uptime: " << metrics.uptime_seconds() << " seconds" << std::endl;
    return 0;
}
