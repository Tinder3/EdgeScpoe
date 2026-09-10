#pragma once

#include "AgentConfig.h"
#include "DiagnosticCollector.h"
#include "LogCollector.h"
#include "MetricsSampler.h"
#include "NetworkCollector.h"
#include "ProcessCollector.h"
#include "ProcessController.h"
#include "ServiceManager.h"
#include "edgescope.grpc.pb.h"

class EdgeScopeServiceImpl final : public edgescope::v1::EdgeScopeService::Service {
public:
    explicit EdgeScopeServiceImpl(const AgentConfig& config = AgentConfig{});

    grpc::Status GetAgentInfo(
        grpc::ServerContext* context,
        const edgescope::v1::GetAgentInfoRequest* request,
        edgescope::v1::GetAgentInfoResponse* response) override;

    grpc::Status GetSystemMetrics(
        grpc::ServerContext* context,
        const edgescope::v1::GetSystemMetricsRequest* request,
        edgescope::v1::GetSystemMetricsResponse* response) override;

    grpc::Status StreamSystemMetrics(
        grpc::ServerContext* context,
        const edgescope::v1::StreamSystemMetricsRequest* request,
        grpc::ServerWriter<edgescope::v1::GetSystemMetricsResponse>* writer)
        override;

    grpc::Status ListProcesses(
        grpc::ServerContext* context,
        const edgescope::v1::ListProcessesRequest* request,
        edgescope::v1::ListProcessesResponse* response) override;

    grpc::Status GetProcessDetails(
        grpc::ServerContext* context,
        const edgescope::v1::GetProcessDetailsRequest* request,
        edgescope::v1::GetProcessDetailsResponse* response) override;

    grpc::Status ControlProcess(
        grpc::ServerContext* context,
        const edgescope::v1::ControlProcessRequest* request,
        edgescope::v1::ControlProcessResponse* response) override;

    grpc::Status GetNetworkInterfaces(
        grpc::ServerContext* context,
        const edgescope::v1::GetNetworkInterfacesRequest* request,
        edgescope::v1::GetNetworkInterfacesResponse* response) override;

    grpc::Status ListTcpConnections(
        grpc::ServerContext* context,
        const edgescope::v1::ListTcpConnectionsRequest* request,
        edgescope::v1::ListTcpConnectionsResponse* response) override;

    grpc::Status ListLogs(
        grpc::ServerContext* context,
        const edgescope::v1::ListLogsRequest* request,
        edgescope::v1::ListLogsResponse* response) override;

    grpc::Status ReadLog(
        grpc::ServerContext* context,
        const edgescope::v1::ReadLogRequest* request,
        edgescope::v1::ReadLogResponse* response) override;

    grpc::Status StreamLog(
        grpc::ServerContext* context,
        const edgescope::v1::StreamLogRequest* request,
        grpc::ServerWriter<edgescope::v1::LogLine>* writer) override;

    grpc::Status ListServices(
        grpc::ServerContext* context,
        const edgescope::v1::ListServicesRequest* request,
        edgescope::v1::ListServicesResponse* response) override;

    grpc::Status ControlService(
        grpc::ServerContext* context,
        const edgescope::v1::ControlServiceRequest* request,
        edgescope::v1::ControlServiceResponse* response) override;

    grpc::Status CreateDiagnosticBundle(
        grpc::ServerContext* context,
        const edgescope::v1::CreateDiagnosticBundleRequest* request,
        edgescope::v1::CreateDiagnosticBundleResponse* response) override;

    grpc::Status DownloadDiagnosticBundle(
        grpc::ServerContext* context,
        const edgescope::v1::DownloadDiagnosticBundleRequest* request,
        grpc::ServerWriter<edgescope::v1::DiagnosticChunk>* writer) override;

private:
    MetricsSampler metrics_sampler_;
    ProcessCollector process_collector_;
    ProcessController process_controller_;
    NetworkCollector network_collector_;
    LogCollector log_collector_;
    DiagnosticCollector diagnostic_collector_;
    ServiceManager service_manager_;
};
