#pragma once

#include <chrono>
#include <string>
#include <vector>

struct AgentConfig {
    std::string listen_address = "127.0.0.1:50051";
    std::chrono::milliseconds sample_interval{1000};
    std::string log_file = "/tmp/edgescope-agent.log";
    std::string diagnostic_directory = "/tmp/edgescope-diagnostics";
    std::vector<std::string> allowed_services{"ssh.service", "cron.service"};
};

class AgentConfigLoader {
public:
    static bool Load(const std::string& path, AgentConfig* config,
                     std::string* error);
};
