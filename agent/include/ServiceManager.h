#pragma once

#include <memory>
#include <string>
#include <vector>

struct ServiceInfo {
    std::string name;
    std::string description;
    std::string load_state;
    std::string active_state;
    std::string sub_state;
};

enum class ServiceAction { kStart, kStop, kRestart };
enum class ServiceManagerError {
    kNone,
    kInvalidArgument,
    kNotAllowed,
    kNotFound,
    kPermissionDenied,
    kCommandFailed,
};

struct CommandResult {
    int exit_code = -1;
    std::string output;
};

class ServiceCommandRunner {
public:
    virtual ~ServiceCommandRunner() = default;
    virtual bool Run(const std::vector<std::string>& arguments,
                     CommandResult* result, std::string* error) const = 0;
};

class ServiceManager {
public:
    explicit ServiceManager(
        std::vector<std::string> allowed_services,
        std::shared_ptr<const ServiceCommandRunner> runner = nullptr);

    bool ListServices(std::vector<ServiceInfo>* services,
                      std::string* error) const;
    bool ControlService(const std::string& name, ServiceAction action,
                        ServiceManagerError* error_code,
                        std::string* error) const;

private:
    bool IsAllowed(const std::string& name) const;
    std::vector<std::string> allowed_services_;
    std::shared_ptr<const ServiceCommandRunner> runner_;
};
