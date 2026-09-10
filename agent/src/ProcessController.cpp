#include "ProcessController.h"

#include <cerrno>
#include <csignal>
#include <string>
#include <unistd.h>

bool ProcessController::Control(std::int32_t pid, ProcessControlAction action,
                                ProcessControlError* error_code,
                                std::string* error) const {
    if (error_code == nullptr || error == nullptr) {
        return false;
    }
    *error_code = ProcessControlError::kNone;
    error->clear();

    if (pid <= 1) {
        *error_code = ProcessControlError::kInvalidArgument;
        *error = "pid must be greater than 1";
        return false;
    }
    if (pid == static_cast<std::int32_t>(getpid())) {
        *error_code = ProcessControlError::kInvalidArgument;
        *error = "the agent cannot control itself";
        return false;
    }

    int signal_number = 0;
    switch (action) {
        case ProcessControlAction::kTerminate:
            signal_number = SIGTERM;
            break;
        case ProcessControlAction::kKill:
            signal_number = SIGKILL;
            break;
        case ProcessControlAction::kStop:
            signal_number = SIGSTOP;
            break;
        case ProcessControlAction::kContinue:
            signal_number = SIGCONT;
            break;
        default:
            *error_code = ProcessControlError::kInvalidArgument;
            *error = "unsupported process control action";
            return false;
    }

    if (kill(pid, signal_number) == 0) {
        return true;
    }

    if (errno == ESRCH) {
        *error_code = ProcessControlError::kNotFound;
        *error = "process does not exist";
    } else if (errno == EPERM) {
        *error_code = ProcessControlError::kPermissionDenied;
        *error = "permission denied while controlling process";
    } else {
        *error_code = ProcessControlError::kInternal;
        *error = "failed to control process";
    }
    return false;
}
