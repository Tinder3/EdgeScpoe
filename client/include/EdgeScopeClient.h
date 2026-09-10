#pragma once

#include "edgescope.grpc.pb.h"

#include <grpcpp/grpcpp.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class EdgeScopeClient {
public:
    explicit EdgeScopeClient(const std::string& target);

    grpc::Status GetAgentInfo(edgescope::v1::GetAgentInfoResponse* response);
    grpc::Status GetSystemMetrics(
        edgescope::v1::GetSystemMetricsResponse* response);
    grpc::Status StreamSystemMetrics(
        std::uint32_t interval_ms, std::size_t max_samples,
        const std::function<void(
            const edgescope::v1::GetSystemMetricsResponse&)>& on_metrics);
    grpc::Status ListProcesses(
        edgescope::v1::ListProcessesResponse* response);
    grpc::Status GetProcessDetails(
        std::int32_t pid,
        edgescope::v1::GetProcessDetailsResponse* response);
    grpc::Status ControlProcess(std::int32_t pid,
                                edgescope::v1::ProcessAction action);
    grpc::Status GetNetworkInterfaces(
        edgescope::v1::GetNetworkInterfacesResponse* response);
    grpc::Status ListTcpConnections(
        edgescope::v1::ListTcpConnectionsResponse* response);
    grpc::Status ListLogs(edgescope::v1::ListLogsResponse* response);
    grpc::Status ReadLog(const std::string& log_id, std::uint32_t max_lines,
                         const std::string& keyword,
                         edgescope::v1::ReadLogResponse* response);
    grpc::Status StreamLog(
        const std::string& log_id, const std::string& keyword,
        grpc::ClientContext* context,
        const std::function<void(const std::string&)>& on_line);
    grpc::Status ListServices(
        edgescope::v1::ListServicesResponse* response);
    grpc::Status ControlService(const std::string& name,
                                edgescope::v1::ServiceAction action);
    grpc::Status CreateDiagnosticBundle(
        edgescope::v1::CreateDiagnosticBundleResponse* response);
    grpc::Status DownloadDiagnosticBundle(
        const std::string& bundle_id,
        const std::function<bool(const edgescope::v1::DiagnosticChunk&)>&
            on_chunk);

private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<edgescope::v1::EdgeScopeService::Stub> stub_;
};
