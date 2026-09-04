#include "EdgeScopeServiceImpl.h"

#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <string>

int main() {
    const std::string address = "127.0.0.1:50051";

    EdgeScopeServiceImpl service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
    if (!server) {
        std::cerr << "Failed to start EdgeScope Agent on " << address << std::endl;
        return 1;
    }

    std::cout << "EdgeScope Agent listening on " << address << std::endl;
    server->Wait();
    return 0;
}
