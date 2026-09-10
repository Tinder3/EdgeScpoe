#include "EdgeScopeClient.h"

#include <chrono>

namespace {

void SetUnaryDeadline(grpc::ClientContext* context) {
    context->set_deadline(std::chrono::system_clock::now() +
                          std::chrono::seconds(3));
}

}  // namespace

EdgeScopeClient::EdgeScopeClient(const std::string& target)
    : channel_(grpc::CreateChannel(target, grpc::InsecureChannelCredentials())),
      stub_(edgescope::v1::EdgeScopeService::NewStub(channel_)) {}

grpc::Status EdgeScopeClient::GetAgentInfo(edgescope::v1::GetAgentInfoResponse* response) {
    edgescope::v1::GetAgentInfoRequest request;
    grpc::ClientContext context;
    SetUnaryDeadline(&context);
    return stub_->GetAgentInfo(&context, request, response);
}

grpc::Status EdgeScopeClient::GetSystemMetrics(
    edgescope::v1::GetSystemMetricsResponse* response) {
    edgescope::v1::GetSystemMetricsRequest request;
    grpc::ClientContext context;
    SetUnaryDeadline(&context);
    return stub_->GetSystemMetrics(&context, request, response);
}

grpc::Status EdgeScopeClient::StreamSystemMetrics(
    std::uint32_t interval_ms, std::size_t max_samples,
    const std::function<void(
        const edgescope::v1::GetSystemMetricsResponse&)>& on_metrics) {
    edgescope::v1::StreamSystemMetricsRequest request;
    request.set_interval_ms(interval_ms);
    grpc::ClientContext context;
    std::unique_ptr<grpc::ClientReader<edgescope::v1::GetSystemMetricsResponse>>
        reader = stub_->StreamSystemMetrics(&context, request);

    edgescope::v1::GetSystemMetricsResponse response;
    std::size_t sample_count = 0;
    bool cancelled_after_limit = false;
    while (reader->Read(&response)) {
        on_metrics(response);
        ++sample_count;
        if (max_samples != 0 && sample_count >= max_samples) {
            cancelled_after_limit = true;
            context.TryCancel();
            break;
        }
    }

    const grpc::Status status = reader->Finish();
    if (cancelled_after_limit &&
        status.error_code() == grpc::StatusCode::CANCELLED) {
        return grpc::Status::OK;
    }
    return status;
}

grpc::Status EdgeScopeClient::ListProcesses(
    edgescope::v1::ListProcessesResponse* response) {
    edgescope::v1::ListProcessesRequest request;
    grpc::ClientContext context;
    SetUnaryDeadline(&context);
    return stub_->ListProcesses(&context, request, response);
}

grpc::Status EdgeScopeClient::GetProcessDetails(
    std::int32_t pid, edgescope::v1::GetProcessDetailsResponse* response) {
    edgescope::v1::GetProcessDetailsRequest request;
    request.set_pid(pid);
    grpc::ClientContext context;
    SetUnaryDeadline(&context);
    return stub_->GetProcessDetails(&context, request, response);
}

grpc::Status EdgeScopeClient::ControlProcess(
    std::int32_t pid, edgescope::v1::ProcessAction action) {
    edgescope::v1::ControlProcessRequest request;
    request.set_pid(pid);
    request.set_action(action);
    edgescope::v1::ControlProcessResponse response;
    grpc::ClientContext context;
    SetUnaryDeadline(&context);
    return stub_->ControlProcess(&context, request, &response);
}

grpc::Status EdgeScopeClient::GetNetworkInterfaces(
    edgescope::v1::GetNetworkInterfacesResponse* response) {
    edgescope::v1::GetNetworkInterfacesRequest request;
    grpc::ClientContext context;
    SetUnaryDeadline(&context);
    return stub_->GetNetworkInterfaces(&context, request, response);
}

grpc::Status EdgeScopeClient::ListTcpConnections(
    edgescope::v1::ListTcpConnectionsResponse* response) {
    edgescope::v1::ListTcpConnectionsRequest request;
    grpc::ClientContext context;
    SetUnaryDeadline(&context);
    return stub_->ListTcpConnections(&context, request, response);
}

grpc::Status EdgeScopeClient::ListLogs(
    edgescope::v1::ListLogsResponse* response) {
    edgescope::v1::ListLogsRequest request;
    grpc::ClientContext context;
    SetUnaryDeadline(&context);
    return stub_->ListLogs(&context, request, response);
}

grpc::Status EdgeScopeClient::ReadLog(
    const std::string& log_id, std::uint32_t max_lines,
    const std::string& keyword, edgescope::v1::ReadLogResponse* response) {
    edgescope::v1::ReadLogRequest request;
    request.set_log_id(log_id);
    request.set_max_lines(max_lines);
    request.set_keyword(keyword);
    grpc::ClientContext context;
    SetUnaryDeadline(&context);
    return stub_->ReadLog(&context, request, response);
}
