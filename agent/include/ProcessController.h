#pragma once

#include <cstdint>
#include <string>

enum class ProcessControlAction {
    kTerminate,
    kKill,
    kStop,
    kContinue,
};

enum class ProcessControlError {
    kNone,
    kInvalidArgument,
    kNotFound,
    kPermissionDenied,
    kInternal,
};

class ProcessController {
public:
    bool Control(std::int32_t pid, ProcessControlAction action,
                 ProcessControlError* error_code, std::string* error) const;
};
