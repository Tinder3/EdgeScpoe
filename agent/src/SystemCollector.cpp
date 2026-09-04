#include "SystemCollector.h"

#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace {

bool ReadMemoryMetrics(SystemMetrics* metrics, std::string* error) {
    std::ifstream input("/proc/meminfo");
    if (!input) {
        *error = "failed to open /proc/meminfo";
        return false;
    }

    std::uint64_t total_kb = 0;
    std::uint64_t available_kb = 0;
    bool found_total = false;
    bool found_available = false;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream stream(line);
        std::string key;
        std::uint64_t value = 0;
        std::string unit;
        if (!(stream >> key >> value >> unit) || unit != "kB") {
            continue;
        }
        if (key == "MemTotal:") {
            total_kb = value;
            found_total = true;
        } else if (key == "MemAvailable:") {
            available_kb = value;
            found_available = true;
        }
    }

    constexpr std::uint64_t kBytesPerKilobyte = 1024;
    const std::uint64_t max_kb =
        std::numeric_limits<std::uint64_t>::max() / kBytesPerKilobyte;
    if (!found_total || !found_available || total_kb > max_kb ||
        available_kb > total_kb) {
        *error = "invalid memory values in /proc/meminfo";
        return false;
    }

    metrics->memory_total_bytes = total_kb * kBytesPerKilobyte;
    metrics->memory_available_bytes = available_kb * kBytesPerKilobyte;
    metrics->memory_used_bytes =
        metrics->memory_total_bytes - metrics->memory_available_bytes;
    return true;
}

bool ReadLoadAverage(SystemMetrics* metrics, std::string* error) {
    std::ifstream input("/proc/loadavg");
    if (!input || !(input >> metrics->load_average_1m >> metrics->load_average_5m >>
                    metrics->load_average_15m) ||
        !std::isfinite(metrics->load_average_1m) ||
        !std::isfinite(metrics->load_average_5m) ||
        !std::isfinite(metrics->load_average_15m) ||
        metrics->load_average_1m < 0.0 || metrics->load_average_5m < 0.0 ||
        metrics->load_average_15m < 0.0) {
        *error = "failed to parse /proc/loadavg";
        return false;
    }
    return true;
}

bool ReadUptime(SystemMetrics* metrics, std::string* error) {
    std::ifstream input("/proc/uptime");
    double uptime = 0.0;
    if (!input || !(input >> uptime) || !std::isfinite(uptime) || uptime < 0.0 ||
        uptime > static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
        *error = "failed to parse /proc/uptime";
        return false;
    }

    metrics->uptime_seconds = static_cast<std::uint64_t>(uptime);
    return true;
}

}  // namespace

std::uint64_t SystemCollector::CpuTimes::Total() const {
    return user + nice + system + idle + iowait + irq + softirq + steal;
}

std::uint64_t SystemCollector::CpuTimes::Idle() const {
    return idle + iowait;
}

SystemCollector::SystemCollector()
    : has_previous_cpu_times_(ReadCpuTimes(&previous_cpu_times_)) {}

bool SystemCollector::ReadCpuTimes(CpuTimes* times) {
    std::ifstream input("/proc/stat");
    std::string line;
    if (!input || !std::getline(input, line)) {
        return false;
    }

    std::istringstream stream(line);
    std::string label;
    return static_cast<bool>(stream >> label >> times->user >> times->nice >>
                             times->system >> times->idle >> times->iowait >>
                             times->irq >> times->softirq >> times->steal) &&
           label == "cpu";
}

bool SystemCollector::CollectCpuUsage(double* usage_percent, std::string* error) {
    std::lock_guard<std::mutex> lock(cpu_mutex_);
    CpuTimes current;
    if (!ReadCpuTimes(&current)) {
        *error = "failed to parse /proc/stat";
        return false;
    }

    *usage_percent = 0.0;
    if (has_previous_cpu_times_) {
        const std::uint64_t previous_total = previous_cpu_times_.Total();
        const std::uint64_t current_total = current.Total();
        const std::uint64_t previous_idle = previous_cpu_times_.Idle();
        const std::uint64_t current_idle = current.Idle();

        if (current_total >= previous_total && current_idle >= previous_idle) {
            const std::uint64_t total_delta = current_total - previous_total;
            const std::uint64_t idle_delta = current_idle - previous_idle;
            if (total_delta != 0 && idle_delta <= total_delta) {
                *usage_percent =
                    static_cast<double>(total_delta - idle_delta) /
                    static_cast<double>(total_delta) * 100.0;
            }
        }
    }

    previous_cpu_times_ = current;
    has_previous_cpu_times_ = true;
    return true;
}

bool SystemCollector::Collect(SystemMetrics* metrics, std::string* error) {
    if (metrics == nullptr || error == nullptr) {
        return false;
    }

    *metrics = SystemMetrics{};
    error->clear();
    return CollectCpuUsage(&metrics->cpu_usage_percent, error) &&
           ReadMemoryMetrics(metrics, error) && ReadLoadAverage(metrics, error) &&
           ReadUptime(metrics, error);
}
