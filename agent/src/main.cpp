#include "AgentConfig.h"
#include "AgentLogger.h"
#include "EdgeScopeServiceImpl.h"

#include <grpcpp/grpcpp.h>
#include <grpc/impl/codegen/grpc_types.h>

#include <iostream>
#include <memory>
#include <csignal>
#include <string>
#include <pthread.h>

int main(int argc, char* argv[]) {
    sigset_t shutdown_signals;
    sigemptyset(&shutdown_signals);
    sigaddset(&shutdown_signals, SIGINT);
    sigaddset(&shutdown_signals, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &shutdown_signals, nullptr) != 0) {
        std::cerr << "Failed to configure shutdown signal handling" << std::endl;
        return 1;
    }

    AgentConfig config;
    if (argc == 3 && std::string(argv[1]) == "--config") {
        std::string error;
        if (!AgentConfigLoader::Load(argv[2], &config, &error)) {
            std::cerr << "Failed to load Agent config: " << error << std::endl;
            return 1;
        }
    } else if (argc != 1) {
        std::cerr << "Usage: " << argv[0] << " [--config FILE]" << std::endl;
        return 1;
    }

    std::string log_error;
    if (!AgentLogger::Initialize(config.log_file, &log_error)) {
        std::cerr << "Failed to initialize Agent logging: " << log_error
                  << std::endl;
        return 1;
    }

    EdgeScopeServiceImpl service(config);
    grpc::ServerBuilder builder;
    builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 0);
    builder.AddListeningPort(config.listen_address,
                             grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
    if (!server) {
        AgentLogger::Error("failed to start Agent on " +
                           config.listen_address);
        AgentLogger::Shutdown();
        return 1;
    }

    AgentLogger::Info("EdgeScope Agent 1.0.0 listening on " +
                      config.listen_address);
    int received_signal = 0;
    sigwait(&shutdown_signals, &received_signal);
    AgentLogger::Info("shutdown signal received: " +
                      std::to_string(received_signal));
    server->Shutdown();
    server->Wait();
    AgentLogger::Info("EdgeScope Agent stopped");
    AgentLogger::Shutdown();
    return 0;
}
