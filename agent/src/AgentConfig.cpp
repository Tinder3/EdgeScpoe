#include "AgentConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>

namespace {

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char character) {
        return !std::isspace(character);
    };
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(),
                value.end());
    return value;
}

bool IsValidAddress(const std::string& address) {
    const std::size_t separator = address.rfind(':');
    if (separator == std::string::npos || separator == 0 ||
        separator + 1 == address.size()) {
        return false;
    }
    try {
        std::size_t parsed = 0;
        const unsigned long port = std::stoul(address.substr(separator + 1),
                                              &parsed);
        return parsed == address.size() - separator - 1 && port > 0 &&
               port <= 65535;
    } catch (const std::exception&) {
        return false;
    }
}

bool IsValidServiceName(const std::string& name) {
    if (name.empty() || name.size() > 128 ||
        name.find(".service") != name.size() - 8) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '-' || character == '_' ||
               character == '.' || character == '@';
    });
}

std::vector<std::string> SplitServices(const std::string& value) {
    std::vector<std::string> services;
    std::istringstream stream(value);
    std::string service;
    while (std::getline(stream, service, ',')) {
        service = Trim(service);
        if (!service.empty()) {
            services.push_back(service);
        }
    }
    return services;
}

}  // namespace

bool AgentConfigLoader::Load(const std::string& path, AgentConfig* config,
                             std::string* error) {
    if (config == nullptr || error == nullptr) {
        return false;
    }
    std::ifstream input(path);
    if (!input) {
        *error = "failed to open config file: " + path;
        return false;
    }

    AgentConfig parsed;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        line = Trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            *error = "invalid config line " + std::to_string(line_number);
            return false;
        }
        const std::string key = Trim(line.substr(0, separator));
        const std::string value = Trim(line.substr(separator + 1));
        if (key == "listen_address") {
            if (!IsValidAddress(value)) {
                *error = "invalid listen_address on line " +
                         std::to_string(line_number);
                return false;
            }
            parsed.listen_address = value;
        } else if (key == "sample_interval_ms") {
            try {
                std::size_t consumed = 0;
                const unsigned long interval = std::stoul(value, &consumed);
                if (consumed != value.size() || interval < 100 ||
                    interval > 60000) {
                    throw std::out_of_range("sample interval");
                }
                parsed.sample_interval =
                    std::chrono::milliseconds(interval);
            } catch (const std::exception&) {
                *error = "sample_interval_ms must be between 100 and 60000";
                return false;
            }
        } else if (key == "log_file") {
            if (value.empty() || value.front() != '/') {
                *error = "log_file must be an absolute path";
                return false;
            }
            parsed.log_file = value;
        } else if (key == "diagnostic_directory") {
            if (value.empty() || value.front() != '/') {
                *error = "diagnostic_directory must be an absolute path";
                return false;
            }
            parsed.diagnostic_directory = value;
        } else if (key == "allowed_services") {
            parsed.allowed_services = SplitServices(value);
            if (!std::all_of(parsed.allowed_services.begin(),
                             parsed.allowed_services.end(),
                             IsValidServiceName)) {
                *error = "allowed_services contains an invalid service name";
                return false;
            }
        } else {
            *error = "unknown config key on line " +
                     std::to_string(line_number) + ": " + key;
            return false;
        }
    }
    *config = std::move(parsed);
    error->clear();
    return true;
}
