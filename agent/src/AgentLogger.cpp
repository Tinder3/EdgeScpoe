#include "AgentLogger.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <exception>
#include <memory>
#include <vector>

namespace {
constexpr std::size_t kMaximumLogSize = 5 * 1024 * 1024;
constexpr std::size_t kRotatedFileCount = 3;
constexpr const char* kLoggerName = "edgescope-agent";
}

bool AgentLogger::Initialize(const std::string& log_file, std::string* error) {
    if (error == nullptr) {
        return false;
    }
    try {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file, kMaximumLogSize, kRotatedFileCount));
        auto logger = std::make_shared<spdlog::logger>(
            kLoggerName, sinks.begin(), sinks.end());
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
        logger->flush_on(spdlog::level::warn);
        spdlog::set_default_logger(std::move(logger));
        error->clear();
        return true;
    } catch (const std::exception& exception) {
        *error = exception.what();
        return false;
    }
}

void AgentLogger::Info(const std::string& message) {
    spdlog::info(message);
    spdlog::default_logger()->flush();
}
void AgentLogger::Warn(const std::string& message) { spdlog::warn(message); }
void AgentLogger::Error(const std::string& message) { spdlog::error(message); }
void AgentLogger::Shutdown() { spdlog::shutdown(); }
