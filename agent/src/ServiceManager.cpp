#include "ServiceManager.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr const char* kSystemctlPath = "/usr/bin/systemctl";
constexpr std::size_t kMaximumOutputBytes = 64 * 1024;

class PosixServiceCommandRunner final : public ServiceCommandRunner {
public:
    bool Run(const std::vector<std::string>& arguments, CommandResult* result,
             std::string* error) const override {
        if (result == nullptr || error == nullptr || arguments.empty()) {
            return false;
        }
        int pipe_fds[2];
        if (pipe(pipe_fds) != 0) {
            *error = std::string("pipe failed: ") + std::strerror(errno);
            return false;
        }
        const pid_t child = fork();
        if (child < 0) {
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            *error = std::string("fork failed: ") + std::strerror(errno);
            return false;
        }
        if (child == 0) {
            close(pipe_fds[0]);
            dup2(pipe_fds[1], STDOUT_FILENO);
            dup2(pipe_fds[1], STDERR_FILENO);
            close(pipe_fds[1]);
            std::vector<char*> argv;
            argv.reserve(arguments.size() + 2);
            argv.push_back(const_cast<char*>(kSystemctlPath));
            for (const std::string& argument : arguments) {
                argv.push_back(const_cast<char*>(argument.c_str()));
            }
            argv.push_back(nullptr);
            execv(kSystemctlPath, argv.data());
            _exit(127);
        }

        close(pipe_fds[1]);
        result->output.clear();
        std::array<char, 4096> buffer{};
        ssize_t count = 0;
        while ((count = read(pipe_fds[0], buffer.data(), buffer.size())) > 0) {
            const std::size_t remaining =
                kMaximumOutputBytes - result->output.size();
            result->output.append(buffer.data(),
                                  std::min<std::size_t>(count, remaining));
            if (result->output.size() >= kMaximumOutputBytes) {
                break;
            }
        }
        close(pipe_fds[0]);
        int status = 0;
        while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
        }
        result->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        error->clear();
        return true;
    }
};

std::string ValueFor(const std::string& output, const std::string& key) {
    std::istringstream lines(output);
    std::string line;
    const std::string prefix = key + "=";
    while (std::getline(lines, line)) {
        if (line.compare(0, prefix.size(), prefix) == 0) {
            return line.substr(prefix.size());
        }
    }
    return {};
}

}  // namespace

ServiceManager::ServiceManager(
    std::vector<std::string> allowed_services,
    std::shared_ptr<const ServiceCommandRunner> runner)
    : allowed_services_(std::move(allowed_services)),
      runner_(runner ? std::move(runner)
                     : std::make_shared<PosixServiceCommandRunner>()) {}

bool ServiceManager::ListServices(std::vector<ServiceInfo>* services,
                                  std::string* error) const {
    if (services == nullptr || error == nullptr) {
        return false;
    }
    services->clear();
    for (const std::string& name : allowed_services_) {
        CommandResult result;
        if (!runner_->Run({"show", "--no-pager", "--property=Description,LoadState,ActiveState,SubState", name},
                          &result, error)) {
            return false;
        }
        ServiceInfo info;
        info.name = name;
        info.description = ValueFor(result.output, "Description");
        info.load_state = ValueFor(result.output, "LoadState");
        info.active_state = ValueFor(result.output, "ActiveState");
        info.sub_state = ValueFor(result.output, "SubState");
        if (result.exit_code != 0 || info.load_state.empty()) {
            info.load_state = "unavailable";
            info.active_state = "unknown";
            info.sub_state = "unknown";
        }
        services->push_back(std::move(info));
    }
    error->clear();
    return true;
}

bool ServiceManager::ControlService(const std::string& name,
                                    ServiceAction action,
                                    ServiceManagerError* error_code,
                                    std::string* error) const {
    if (error_code == nullptr || error == nullptr) {
        return false;
    }
    *error_code = ServiceManagerError::kNone;
    error->clear();
    if (name.empty()) {
        *error_code = ServiceManagerError::kInvalidArgument;
        *error = "service name is required";
        return false;
    }
    if (!IsAllowed(name)) {
        *error_code = ServiceManagerError::kNotAllowed;
        *error = "service is not in the configured whitelist";
        return false;
    }
    const char* operation = nullptr;
    switch (action) {
        case ServiceAction::kStart: operation = "start"; break;
        case ServiceAction::kStop: operation = "stop"; break;
        case ServiceAction::kRestart: operation = "restart"; break;
    }
    CommandResult result;
    if (!runner_->Run({operation, "--no-ask-password", name}, &result, error)) {
        *error_code = ServiceManagerError::kCommandFailed;
        return false;
    }
    if (result.exit_code != 0) {
        *error_code = result.output.find("not found") != std::string::npos
                          ? ServiceManagerError::kNotFound
                      : result.output.find("permission") != std::string::npos ||
                                result.output.find("authentication") != std::string::npos
                          ? ServiceManagerError::kPermissionDenied
                          : ServiceManagerError::kCommandFailed;
        *error = result.output.empty() ? "systemctl operation failed"
                                       : result.output;
        return false;
    }
    return true;
}

bool ServiceManager::IsAllowed(const std::string& name) const {
    return std::find(allowed_services_.begin(), allowed_services_.end(), name) !=
           allowed_services_.end();
}
