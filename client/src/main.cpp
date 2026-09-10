#include "EdgeScopeClient.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace {

void PrintSystemMetrics(
    const edgescope::v1::GetSystemMetricsResponse& metrics) {
    constexpr double kBytesPerGigabyte = 1024.0 * 1024.0 * 1024.0;
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
}

void PrintProcess(const edgescope::v1::ProcessInfo& process) {
    constexpr double kBytesPerMegabyte = 1024.0 * 1024.0;
    std::cout << std::left << std::setw(8) << process.pid() << std::setw(8)
              << process.ppid() << std::setw(20) << process.name().substr(0, 19)
              << std::setw(8) << process.state() << std::right << std::fixed
              << std::setprecision(1) << std::setw(8)
              << process.cpu_usage_percent() << std::setw(12)
              << process.resident_memory_bytes() / kBytesPerMegabyte
              << std::setw(9) << process.thread_count() << std::endl;
}

bool ParseControlAction(const std::string& value,
                        edgescope::v1::ProcessAction* action) {
    if (value == "term") {
        *action = edgescope::v1::PROCESS_ACTION_TERMINATE;
    } else if (value == "kill") {
        *action = edgescope::v1::PROCESS_ACTION_KILL;
    } else if (value == "stop") {
        *action = edgescope::v1::PROCESS_ACTION_STOP;
    } else if (value == "continue") {
        *action = edgescope::v1::PROCESS_ACTION_CONTINUE;
    } else {
        return false;
    }
    return true;
}

bool ParsePid(const std::string& value, std::int32_t* pid) {
    try {
        std::size_t parsed_characters = 0;
        const long long parsed = std::stoll(value, &parsed_characters);
        if (parsed_characters != value.size() || parsed <= 0 ||
            parsed > std::numeric_limits<std::int32_t>::max()) {
            return false;
        }
        *pid = static_cast<std::int32_t>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
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

    if (argc != 1) {
        if (argc != 4 || std::string(argv[1]) != "--control") {
            std::cerr << "Usage: " << argv[0]
                      << " [--control PID term|kill|stop|continue]" << std::endl;
            return 1;
        }

        std::int32_t pid = 0;
        edgescope::v1::ProcessAction action =
            edgescope::v1::PROCESS_ACTION_UNSPECIFIED;
        if (!ParsePid(argv[2], &pid) || !ParseControlAction(argv[3], &action)) {
            std::cerr << "Invalid process control arguments" << std::endl;
            return 1;
        }

        const grpc::Status control_status = client.ControlProcess(pid, action);
        if (!control_status.ok()) {
            std::cerr << "ControlProcess RPC failed: "
                      << control_status.error_message() << std::endl;
            return 1;
        }
        std::cout << "Process control request succeeded for PID " << pid
                  << std::endl;
        return 0;
    }

    edgescope::v1::GetSystemMetricsResponse metrics;
    const grpc::Status metrics_status = client.GetSystemMetrics(&metrics);
    if (!metrics_status.ok()) {
        std::cerr << "GetSystemMetrics RPC failed: "
                  << metrics_status.error_message() << std::endl;
        return 1;
    }

    std::cout << '\n' << "System Metrics" << std::endl;
    PrintSystemMetrics(metrics);

    std::cout << '\n' << "Streaming System Metrics (3 samples)" << std::endl;
    std::size_t stream_sample = 0;
    const grpc::Status stream_status = client.StreamSystemMetrics(
        1000, 3,
        [&stream_sample](
            const edgescope::v1::GetSystemMetricsResponse& streamed_metrics) {
            std::cout << '\n' << "Sample " << ++stream_sample << std::endl;
            PrintSystemMetrics(streamed_metrics);
        });
    if (!stream_status.ok()) {
        std::cerr << "StreamSystemMetrics RPC failed: "
                  << stream_status.error_message() << std::endl;
        return 1;
    }

    edgescope::v1::ListProcessesResponse process_list;
    const grpc::Status list_status = client.ListProcesses(&process_list);
    if (!list_status.ok()) {
        std::cerr << "ListProcesses RPC failed: " << list_status.error_message()
                  << std::endl;
        return 1;
    }

    std::cout << '\n' << "Processes (showing up to 10 of "
              << process_list.processes_size() << ")" << std::endl;
    std::cout << std::left << std::setw(8) << "PID" << std::setw(8) << "PPID"
              << std::setw(20) << "NAME" << std::setw(8) << "STATE"
              << std::right << std::setw(8) << "CPU%" << std::setw(12)
              << "RSS(MB)" << std::setw(9) << "THREADS" << std::endl;
    const int process_count = std::min(10, process_list.processes_size());
    for (int index = 0; index < process_count; ++index) {
        PrintProcess(process_list.processes(index));
    }

    if (process_list.processes_size() > 0) {
        edgescope::v1::GetProcessDetailsResponse details;
        const std::int32_t detail_pid = process_list.processes(0).pid();
        const grpc::Status detail_status =
            client.GetProcessDetails(detail_pid, &details);
        if (!detail_status.ok()) {
            std::cerr << "GetProcessDetails RPC failed: "
                      << detail_status.error_message() << std::endl;
            return 1;
        }

        const auto& process = details.process();
        std::cout << '\n' << "Process Details for PID " << process.pid()
                  << std::endl;
        std::cout << "Command: " << process.command_line() << std::endl;
        std::cout << "Virtual Memory: " << process.virtual_memory_bytes()
                  << " bytes" << std::endl;
        std::cout << "Read/Write: " << process.read_bytes() << " / "
                  << process.write_bytes() << " bytes" << std::endl;
        std::cout << "Start Time: " << process.start_time_ticks() << " ticks"
                  << std::endl;
    }

    edgescope::v1::GetNetworkInterfacesResponse interfaces;
    const grpc::Status interface_status =
        client.GetNetworkInterfaces(&interfaces);
    if (!interface_status.ok()) {
        std::cerr << "GetNetworkInterfaces RPC failed: "
                  << interface_status.error_message() << std::endl;
        return 1;
    }
    std::cout << '\n' << "Network Interfaces" << std::endl;
    for (const auto& interface : interfaces.interfaces()) {
        std::cout << interface.name() << " [" << interface.state() << "] RX="
                  << interface.rx_bytes() << " TX=" << interface.tx_bytes();
        if (interface.ipv4_addresses_size() > 0) {
            std::cout << " IPv4=" << interface.ipv4_addresses(0);
        }
        if (interface.ipv6_addresses_size() > 0) {
            std::cout << " IPv6=" << interface.ipv6_addresses(0);
        }
        std::cout << std::endl;
    }

    edgescope::v1::ListTcpConnectionsResponse connections;
    const grpc::Status connection_status =
        client.ListTcpConnections(&connections);
    if (!connection_status.ok()) {
        std::cerr << "ListTcpConnections RPC failed: "
                  << connection_status.error_message() << std::endl;
        return 1;
    }
    std::cout << "TCP Connections: " << connections.connections_size()
              << std::endl;
    const int displayed_connections =
        std::min(10, connections.connections_size());
    for (int index = 0; index < displayed_connections; ++index) {
        const auto& connection = connections.connections(index);
        std::cout << connection.protocol() << " " << connection.local_address()
                  << ":" << connection.local_port() << " -> "
                  << connection.remote_address() << ":"
                  << connection.remote_port() << " " << connection.state()
                  << " PID=" << connection.pid() << std::endl;
    }

    edgescope::v1::ListLogsResponse logs;
    const grpc::Status logs_status = client.ListLogs(&logs);
    if (!logs_status.ok()) {
        std::cerr << "ListLogs RPC failed: " << logs_status.error_message()
                  << std::endl;
        return 1;
    }
    std::cout << '\n' << "Allowed Logs" << std::endl;
    for (const auto& log : logs.logs()) {
        std::cout << log.id() << ": " << log.display_name() << " ["
                  << (log.available() ? "available" : "unavailable") << "]"
                  << std::endl;
    }
    for (const auto& log : logs.logs()) {
        if (!log.available()) {
            continue;
        }
        edgescope::v1::ReadLogResponse log_content;
        const grpc::Status read_status =
            client.ReadLog(log.id(), 5, "", &log_content);
        if (!read_status.ok()) {
            continue;
        }
        std::cout << "Last " << log_content.lines_size() << " lines from "
                  << log.id() << ":" << std::endl;
        for (const auto& line : log_content.lines()) {
            std::cout << line << std::endl;
        }
        break;
    }
    return 0;
}
