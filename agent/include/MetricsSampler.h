#pragma once

#include "SystemCollector.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

class MetricsSampler {
public:
    explicit MetricsSampler(
        std::chrono::milliseconds sample_interval = std::chrono::seconds(1));
    ~MetricsSampler();

    MetricsSampler(const MetricsSampler&) = delete;
    MetricsSampler& operator=(const MetricsSampler&) = delete;

    bool GetLatest(SystemMetrics* metrics, std::uint64_t* generation,
                   std::string* error) const;
    bool WaitForNext(std::uint64_t previous_generation, SystemMetrics* metrics,
                     std::uint64_t* generation, std::string* error);

private:
    void Sample();
    void Run();

    const std::chrono::milliseconds sample_interval_;
    SystemCollector collector_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    SystemMetrics latest_metrics_;
    std::string latest_error_;
    std::uint64_t generation_ = 0;
    bool has_snapshot_ = false;
    bool stopping_ = false;
    std::thread worker_;
};
