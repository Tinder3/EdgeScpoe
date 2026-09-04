#pragma once

#include <cstdint>
#include <mutex>
#include <string>

struct SystemMetrics {
    double cpu_usage_percent = 0.0;
    std::uint64_t memory_total_bytes = 0;
    std::uint64_t memory_available_bytes = 0;
    std::uint64_t memory_used_bytes = 0;
    double load_average_1m = 0.0;
    double load_average_5m = 0.0;
    double load_average_15m = 0.0;
    std::uint64_t uptime_seconds = 0;
};

class SystemCollector {
public:
    SystemCollector();

    bool Collect(SystemMetrics* metrics, std::string* error);

private:
    struct CpuTimes {
        std::uint64_t user = 0;
        std::uint64_t nice = 0;
        std::uint64_t system = 0;
        std::uint64_t idle = 0;
        std::uint64_t iowait = 0;
        std::uint64_t irq = 0;
        std::uint64_t softirq = 0;
        std::uint64_t steal = 0;

        std::uint64_t Total() const;
        std::uint64_t Idle() const;
    };

    static bool ReadCpuTimes(CpuTimes* times);
    bool CollectCpuUsage(double* usage_percent, std::string* error);

    CpuTimes previous_cpu_times_;
    bool has_previous_cpu_times_ = false;
    std::mutex cpu_mutex_;
};
