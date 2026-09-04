#include "EdgeScopeClient.h"

EdgeScopeClient::EdgeScopeClient(const std::string& target)
    : channel_(grpc::CreateChannel(target, grpc::InsecureChannelCredentials())),
      stub_(edgescope::v1::EdgeScopeService::NewStub(channel_)) {}

grpc::Status EdgeScopeClient::GetAgentInfo(edgescope::v1::GetAgentInfoResponse* response) {
    edgescope::v1::GetAgentInfoRequest request;
    grpc::ClientContext context;
    return stub_->GetAgentInfo(&context, request, response);
}

grpc::Status EdgeScopeClient::GetSystemMetrics(
    edgescope::v1::GetSystemMetricsResponse* response) {
    edgescope::v1::GetSystemMetricsRequest request;
    grpc::ClientContext context;
    return stub_->GetSystemMetrics(&context, request, response);
}
