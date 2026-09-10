#include "EdgeScopeClient.h"

#include <algorithm>
#include <iomanip>
#include <fstream>
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
    std::string target = "127.0.0.1:50051";
    int argument_index = 1;
    if (argc >= 3 && std::string(argv[1]) == "--target") {
        target = argv[2];
        argument_index = 3;
    }
    EdgeScopeClient client(target);
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

    if (argument_index != argc) {
        if (argc - argument_index == 2 &&
            std::string(argv[argument_index]) == "--diagnostic") {
            edgescope::v1::CreateDiagnosticBundleResponse created;
            grpc::Status status = client.CreateDiagnosticBundle(&created);
            if (!status.ok()) {
                std::cerr << "CreateDiagnosticBundle RPC failed: "
                          << status.error_message() << std::endl;
                return 1;
            }
            std::ofstream output(argv[argument_index + 1],
                                 std::ios::binary | std::ios::trunc);
            if (!output) {
                std::cerr << "Failed to open diagnostic output file" << std::endl;
                return 1;
            }
            std::uint64_t received = 0;
            status = client.DownloadDiagnosticBundle(
                created.bundle_id(),
                [&](const edgescope::v1::DiagnosticChunk& chunk) {
                    if (chunk.offset() != received) return false;
                    output.write(chunk.data().data(),
                                 static_cast<std::streamsize>(chunk.data().size()));
                    received += chunk.data().size();
                    return static_cast<bool>(output);
                });
            output.close();
            if (!status.ok() || received != created.total_size_bytes()) {
                std::cerr << "DownloadDiagnosticBundle failed: "
                          << status.error_message() << std::endl;
                return 1;
            }
            std::cout << "Diagnostic bundle saved to "
                      << argv[argument_index + 1] << " (" << received
                      << " bytes)" << std::endl;
            return 0;
        }
        if (argc - argument_index == 3 &&
            std::string(argv[argument_index]) == "--stream-log") {
            std::size_t wanted = 0;
            try {
                wanted = std::stoul(argv[argument_index + 2]);
            } catch (const std::exception&) {
                wanted = 0;
            }
            if (wanted == 0 || wanted > 1000) {
                std::cerr << "Log stream count must be between 1 and 1000"
                          << std::endl;
                return 1;
            }
            grpc::ClientContext context;
            std::size_t received = 0;
            const grpc::Status status = client.StreamLog(
                argv[argument_index + 1], "", &context,
                [&](const std::string& line) {
                    std::cout << line << std::endl;
                    if (++received >= wanted) context.TryCancel();
                });
            if (!status.ok() && status.error_code() != grpc::StatusCode::CANCELLED) {
                std::cerr << "StreamLog RPC failed: " << status.error_message()
                          << std::endl;
                return 1;
            }
            return 0;
        }
        if (argc - argument_index != 3 ||
            std::string(argv[argument_index]) != "--control") {
            std::cerr << "Usage: " << argv[0]
                      << " [--target HOST:PORT]"
                         " [--control PID term|kill|stop|continue |"
                         " --diagnostic OUTPUT.tar.gz | --stream-log ID COUNT]"
                      << std::endl;
            return 1;
        }

        std::int32_t pid = 0;
        edgescope::v1::ProcessAction action =
            edgescope::v1::PROCESS_ACTION_UNSPECIFIED;
        if (!ParsePid(argv[argument_index + 1], &pid) ||
            !ParseControlAction(argv[argument_index + 2], &action)) {
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

    edgescope::v1::ListServicesResponse services;
    const grpc::Status services_status = client.ListServices(&services);
    if (!services_status.ok()) {
        std::cerr << "ListServices RPC failed: "
                  << services_status.error_message() << std::endl;
        return 1;
    }
    std::cout << '\n' << "Allowed Services" << std::endl;
    for (const auto& service : services.services()) {
        std::cout << service.name() << " [" << service.load_state() << "/"
                  << service.active_state() << "/" << service.sub_state()
                  << "] " << service.description() << std::endl;
    }
    return 0;
}
