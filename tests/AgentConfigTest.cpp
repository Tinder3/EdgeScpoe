#include "AgentConfig.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace {

class TemporaryConfig {
public:
    explicit TemporaryConfig(const std::string& contents) {
        path_ = std::filesystem::temp_directory_path() /
                ("edgescope-config-" + std::to_string(::getpid()) + ".conf");
        std::ofstream(path_) << contents;
    }
    ~TemporaryConfig() { std::filesystem::remove(path_); }
    std::string path() const { return path_.string(); }

private:
    std::filesystem::path path_;
};

TEST(AgentConfigTest, LoadsSupportedValues) {
    TemporaryConfig file(
        "listen_address=0.0.0.0:50052\n"
        "sample_interval_ms=2500\n"
        "log_file=/tmp/test.log\n"
        "diagnostic_directory=/tmp/test-diagnostics\n"
        "allowed_services=ssh.service, cron.service\n");
    AgentConfig config;
    std::string error;
    ASSERT_TRUE(AgentConfigLoader::Load(file.path(), &config, &error)) << error;
    EXPECT_EQ(config.listen_address, "0.0.0.0:50052");
    EXPECT_EQ(config.sample_interval.count(), 2500);
    EXPECT_EQ(config.allowed_services.size(), 2U);
    EXPECT_EQ(config.diagnostic_directory, "/tmp/test-diagnostics");
}

TEST(AgentConfigTest, RejectsUnknownKeys) {
    TemporaryConfig file("execute_command=anything\n");
    AgentConfig config;
    std::string error;
    EXPECT_FALSE(AgentConfigLoader::Load(file.path(), &config, &error));
    EXPECT_NE(error.find("unknown config key"), std::string::npos);
}

TEST(AgentConfigTest, RejectsUnsafeServiceNames) {
    TemporaryConfig file("allowed_services=ssh.service;reboot\n");
    AgentConfig config;
    std::string error;
    EXPECT_FALSE(AgentConfigLoader::Load(file.path(), &config, &error));
}

}  // namespace
