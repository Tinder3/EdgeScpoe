#pragma once

#include <string>

class AgentLogger {
public:
    static bool Initialize(const std::string& log_file, std::string* error);
    static void Info(const std::string& message);
    static void Warn(const std::string& message);
    static void Error(const std::string& message);
    static void Shutdown();
};
