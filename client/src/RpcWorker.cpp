#include "RpcWorker.h"

#include <QStringList>
#include <QFile>

#include <fstream>
#include <string>
#include <utility>

namespace {

ProcessData ConvertProcess(const edgescope::v1::ProcessInfo& process) {
    ProcessData result;
    result.pid = process.pid();
    result.ppid = process.ppid();
    result.name = QString::fromStdString(process.name());
    result.state = QString::fromStdString(process.state());
    result.command_line = QString::fromStdString(process.command_line());
    result.cpu_usage_percent = process.cpu_usage_percent();
    result.resident_memory_bytes = process.resident_memory_bytes();
    result.virtual_memory_bytes = process.virtual_memory_bytes();
    result.thread_count = process.thread_count();
    result.read_bytes = process.read_bytes();
    result.write_bytes = process.write_bytes();
    result.start_time_ticks = process.start_time_ticks();
    return result;
}

}  // namespace

RpcWorker::RpcWorker(QObject* parent) : QObject(parent) {}

RpcWorker::~RpcWorker() { StopLogStream(); }

void RpcWorker::ConnectToAgent(const QString& host, quint16 port) {
    StopLogStream();
    const std::string target =
        host.toStdString() + ":" + std::to_string(port);
    client_ = std::make_shared<EdgeScopeClient>(target);

    edgescope::v1::GetAgentInfoResponse response;
    const grpc::Status status = client_->GetAgentInfo(&response);
    if (!status.ok()) {
        client_.reset();
        EmitRpcError("Connect", status);
        return;
    }

    AgentInfoData agent_info;
    agent_info.hostname = QString::fromStdString(response.hostname());
    agent_info.kernel_version =
        QString::fromStdString(response.kernel_version());
    agent_info.uptime_seconds = response.uptime_seconds();
    agent_info.agent_version = QString::fromStdString(response.agent_version());
    emit Connected(agent_info);
}

void RpcWorker::DisconnectFromAgent() {
    StopLogStream();
    client_.reset();
    emit Disconnected();
}

void RpcWorker::StartLogStream(const QString& log_id, const QString& keyword) {
    StopLogStream();
    if (!client_) return;
    auto context = std::make_shared<grpc::ClientContext>();
    auto client = client_;
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        stream_context_ = context;
    }
    stream_thread_ = std::thread([this, client, context, log_id, keyword] {
        const grpc::Status status = client->StreamLog(
            log_id.toStdString(), keyword.toStdString(), context.get(),
            [this](const std::string& line) {
                emit LogLineReady(QString::fromStdString(line));
            });
        if (!status.ok() && status.error_code() != grpc::StatusCode::CANCELLED) {
            const bool lost = status.error_code() == grpc::StatusCode::UNAVAILABLE;
            emit RpcError("StreamLog",
                          QString::fromStdString(status.error_message()), lost);
        }
        emit LogStreamStopped();
    });
}

void RpcWorker::StopLogStream() {
    std::shared_ptr<grpc::ClientContext> context;
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        context = stream_context_;
    }
    if (context) context->TryCancel();
    if (stream_thread_.joinable()) stream_thread_.join();
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        stream_context_.reset();
    }
}

void RpcWorker::RefreshMetrics() {
    if (!client_) {
        return;
    }

    edgescope::v1::GetSystemMetricsResponse response;
    const grpc::Status status = client_->GetSystemMetrics(&response);
    if (!status.ok()) {
        EmitRpcError("GetSystemMetrics", status);
        return;
    }

    SystemMetricsData metrics;
    metrics.cpu_usage_percent = response.cpu_usage_percent();
    metrics.memory_total_bytes = response.memory_total_bytes();
    metrics.memory_available_bytes = response.memory_available_bytes();
    metrics.memory_used_bytes = response.memory_used_bytes();
    metrics.load_average_1m = response.load_average_1m();
    metrics.load_average_5m = response.load_average_5m();
    metrics.load_average_15m = response.load_average_15m();
    metrics.uptime_seconds = response.uptime_seconds();
    emit MetricsReady(metrics);
}

void RpcWorker::RefreshProcesses() {
    if (!client_) {
        return;
    }

    edgescope::v1::ListProcessesResponse response;
    const grpc::Status status = client_->ListProcesses(&response);
    if (!status.ok()) {
        EmitRpcError("ListProcesses", status);
        return;
    }

    ProcessList processes;
    processes.reserve(response.processes_size());
    for (const auto& process : response.processes()) {
        processes.push_back(ConvertProcess(process));
    }
    emit ProcessesReady(processes);
}

void RpcWorker::GetProcessDetails(qint32 pid) {
    if (!client_) {
        return;
    }

    edgescope::v1::GetProcessDetailsResponse response;
    const grpc::Status status = client_->GetProcessDetails(pid, &response);
    if (!status.ok()) {
        EmitRpcError("GetProcessDetails", status);
        return;
    }
    emit ProcessDetailsReady(ConvertProcess(response.process()));
}

void RpcWorker::ControlProcess(qint32 pid, int action) {
    if (!client_) {
        return;
    }

    const auto process_action =
        static_cast<edgescope::v1::ProcessAction>(action);
    const grpc::Status status = client_->ControlProcess(pid, process_action);
    if (!status.ok()) {
        EmitRpcError("ControlProcess", status);
        return;
    }

    emit OperationSucceeded(QString("Process %1 operation succeeded").arg(pid));
    RefreshProcesses();
}

void RpcWorker::RefreshNetwork() {
    if (!client_) {
        return;
    }

    edgescope::v1::GetNetworkInterfacesResponse interface_response;
    grpc::Status status = client_->GetNetworkInterfaces(&interface_response);
    if (!status.ok()) {
        EmitRpcError("GetNetworkInterfaces", status);
        return;
    }
    edgescope::v1::ListTcpConnectionsResponse connection_response;
    status = client_->ListTcpConnections(&connection_response);
    if (!status.ok()) {
        EmitRpcError("ListTcpConnections", status);
        return;
    }

    NetworkInterfaceList interfaces;
    interfaces.reserve(interface_response.interfaces_size());
    for (const auto& interface : interface_response.interfaces()) {
        NetworkInterfaceData data;
        data.name = QString::fromStdString(interface.name());
        QStringList ipv4;
        for (const auto& address : interface.ipv4_addresses()) {
            ipv4.push_back(QString::fromStdString(address));
        }
        QStringList ipv6;
        for (const auto& address : interface.ipv6_addresses()) {
            ipv6.push_back(QString::fromStdString(address));
        }
        data.ipv4_addresses = ipv4.join(", ");
        data.ipv6_addresses = ipv6.join(", ");
        data.rx_bytes = interface.rx_bytes();
        data.tx_bytes = interface.tx_bytes();
        data.state = QString::fromStdString(interface.state());
        interfaces.push_back(std::move(data));
    }

    TcpConnectionList connections;
    connections.reserve(connection_response.connections_size());
    for (const auto& connection : connection_response.connections()) {
        TcpConnectionData data;
        data.protocol = QString::fromStdString(connection.protocol());
        const QString local_address =
            QString::fromStdString(connection.local_address());
        const QString remote_address =
            QString::fromStdString(connection.remote_address());
        data.local_endpoint = QString("%1:%2")
                                  .arg(local_address)
                                  .arg(connection.local_port());
        data.remote_endpoint = QString("%1:%2")
                                   .arg(remote_address)
                                   .arg(connection.remote_port());
        data.state = QString::fromStdString(connection.state());
        data.inode = connection.inode();
        data.pid = connection.pid();
        connections.push_back(std::move(data));
    }
    emit NetworkReady(interfaces, connections);
}

void RpcWorker::ListLogs() {
    if (!client_) {
        return;
    }
    edgescope::v1::ListLogsResponse response;
    const grpc::Status status = client_->ListLogs(&response);
    if (!status.ok()) {
        EmitRpcError("ListLogs", status);
        return;
    }

    LogSourceList logs;
    logs.reserve(response.logs_size());
    for (const auto& log : response.logs()) {
        logs.push_back({QString::fromStdString(log.id()),
                        QString::fromStdString(log.display_name()),
                        log.available()});
    }
    emit LogsReady(logs);
}

void RpcWorker::ReadLog(const QString& log_id, quint32 max_lines,
                        const QString& keyword) {
    if (!client_) {
        return;
    }
    edgescope::v1::ReadLogResponse response;
    const grpc::Status status =
        client_->ReadLog(log_id.toStdString(), max_lines,
                         keyword.toStdString(), &response);
    if (!status.ok()) {
        EmitRpcError("ReadLog", status);
        return;
    }
    QStringList lines;
    lines.reserve(response.lines_size());
    for (const auto& line : response.lines()) {
        lines.push_back(QString::fromStdString(line));
    }
    emit LogContentReady(lines, response.truncated());
}

void RpcWorker::RefreshServices() {
    if (!client_) {
        return;
    }
    edgescope::v1::ListServicesResponse response;
    const grpc::Status status = client_->ListServices(&response);
    if (!status.ok()) {
        EmitRpcError("ListServices", status);
        return;
    }
    ServiceList services;
    services.reserve(response.services_size());
    for (const auto& service : response.services()) {
        services.push_back({QString::fromStdString(service.name()),
                            QString::fromStdString(service.description()),
                            QString::fromStdString(service.load_state()),
                            QString::fromStdString(service.active_state()),
                            QString::fromStdString(service.sub_state())});
    }
    emit ServicesReady(services);
}

void RpcWorker::ControlService(const QString& name, int action) {
    if (!client_) {
        return;
    }
    const grpc::Status status = client_->ControlService(
        name.toStdString(), static_cast<edgescope::v1::ServiceAction>(action));
    if (!status.ok()) {
        EmitRpcError("ControlService", status);
        return;
    }
    emit OperationSucceeded("Service operation succeeded: " + name);
    RefreshServices();
}

void RpcWorker::CreateAndDownloadDiagnostic(const QString& destination) {
    if (!client_) return;
    edgescope::v1::CreateDiagnosticBundleResponse created;
    grpc::Status status = client_->CreateDiagnosticBundle(&created);
    if (!status.ok()) {
        EmitRpcError("CreateDiagnosticBundle", status);
        return;
    }

    std::ofstream output(destination.toStdString(),
                         std::ios::binary | std::ios::trunc);
    if (!output) {
        emit RpcError("DownloadDiagnosticBundle",
                      "Cannot open the selected local file", false);
        return;
    }
    std::uint64_t received = 0;
    bool local_write_failed = false;
    status = client_->DownloadDiagnosticBundle(
        created.bundle_id(),
        [&](const edgescope::v1::DiagnosticChunk& chunk) {
            if (chunk.offset() != received ||
                chunk.total_size_bytes() != created.total_size_bytes()) {
                local_write_failed = true;
                return false;
            }
            output.write(chunk.data().data(),
                         static_cast<std::streamsize>(chunk.data().size()));
            if (!output) {
                local_write_failed = true;
                return false;
            }
            received += chunk.data().size();
            emit DiagnosticProgress(received, chunk.total_size_bytes());
            return true;
        });
    output.close();
    if (local_write_failed || !status.ok() ||
        received != created.total_size_bytes()) {
        QFile::remove(destination);
        if (local_write_failed) {
            emit RpcError("DownloadDiagnosticBundle",
                          "Local write failed or chunk sequence was invalid",
                          false);
        } else if (!status.ok()) {
            EmitRpcError("DownloadDiagnosticBundle", status);
        } else {
            emit RpcError("DownloadDiagnosticBundle",
                          "Downloaded size does not match bundle size", false);
        }
        return;
    }
    emit DiagnosticReady(destination, received);
}

void RpcWorker::EmitRpcError(const QString& operation,
                             const grpc::Status& status) {
    const bool connection_lost =
        status.error_code() == grpc::StatusCode::UNAVAILABLE ||
        status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED;
    if (connection_lost) {
        client_.reset();
    }
    emit RpcError(operation, QString::fromStdString(status.error_message()),
                  connection_lost);
}
