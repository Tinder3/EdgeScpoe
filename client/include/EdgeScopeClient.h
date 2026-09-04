#pragma once

#include "edgescope.grpc.pb.h"

#include <grpcpp/grpcpp.h>

#include <memory>
#include <string>

class EdgeScopeClient {
public:
    explicit EdgeScopeClient(const std::string& target);

    grpc::Status GetAgentInfo(edgescope::v1::GetAgentInfoResponse* response);
    grpc::Status GetSystemMetrics(
        edgescope::v1::GetSystemMetricsResponse* response);

private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<edgescope::v1::EdgeScopeService::Stub> stub_;
};
