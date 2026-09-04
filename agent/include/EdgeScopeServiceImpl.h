#pragma once

#include "SystemCollector.h"
#include "edgescope.grpc.pb.h"

class EdgeScopeServiceImpl final : public edgescope::v1::EdgeScopeService::Service {
public:
    grpc::Status GetAgentInfo(
        grpc::ServerContext* context,
        const edgescope::v1::GetAgentInfoRequest* request,
        edgescope::v1::GetAgentInfoResponse* response) override;

    grpc::Status GetSystemMetrics(
        grpc::ServerContext* context,
        const edgescope::v1::GetSystemMetricsRequest* request,
        edgescope::v1::GetSystemMetricsResponse* response) override;

private:
    SystemCollector system_collector_;
};
