#include "ServiceManager.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

class FakeRunner final : public ServiceCommandRunner {
public:
    bool Run(const std::vector<std::string>& arguments, CommandResult* result,
             std::string* error) const override {
        calls.push_back(arguments);
        result->exit_code = exit_code;
        result->output = output;
        error->clear();
        return true;
    }

    mutable std::vector<std::vector<std::string>> calls;
    int exit_code = 0;
    std::string output =
        "Description=OpenSSH server\nLoadState=loaded\n"
        "ActiveState=active\nSubState=running\n";
};

TEST(ServiceManagerTest, ListsOnlyConfiguredServices) {
    auto runner = std::make_shared<FakeRunner>();
    ServiceManager manager({"ssh.service"}, runner);
    std::vector<ServiceInfo> services;
    std::string error;
    ASSERT_TRUE(manager.ListServices(&services, &error));
    ASSERT_EQ(services.size(), 1U);
    EXPECT_EQ(services[0].name, "ssh.service");
    EXPECT_EQ(services[0].active_state, "active");
    ASSERT_EQ(runner->calls.size(), 1U);
    EXPECT_EQ(runner->calls[0].back(), "ssh.service");
}

TEST(ServiceManagerTest, RejectsServiceOutsideWhitelistWithoutExecution) {
    auto runner = std::make_shared<FakeRunner>();
    ServiceManager manager({"ssh.service"}, runner);
    ServiceManagerError error_code = ServiceManagerError::kNone;
    std::string error;
    EXPECT_FALSE(manager.ControlService("evil.service", ServiceAction::kStart,
                                        &error_code, &error));
    EXPECT_EQ(error_code, ServiceManagerError::kNotAllowed);
    EXPECT_TRUE(runner->calls.empty());
}

TEST(ServiceManagerTest, UsesFixedSystemctlAction) {
    auto runner = std::make_shared<FakeRunner>();
    ServiceManager manager({"cron.service"}, runner);
    ServiceManagerError error_code = ServiceManagerError::kNone;
    std::string error;
    ASSERT_TRUE(manager.ControlService("cron.service", ServiceAction::kRestart,
                                       &error_code, &error));
    ASSERT_EQ(runner->calls.size(), 1U);
    EXPECT_EQ(runner->calls[0][0], "restart");
    EXPECT_EQ(runner->calls[0].back(), "cron.service");
}

}  // namespace
