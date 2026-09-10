#include "MetricsSampler.h"

#include <utility>

MetricsSampler::MetricsSampler(std::chrono::milliseconds sample_interval)
    : sample_interval_(sample_interval) {
    Sample();
    worker_ = std::thread(&MetricsSampler::Run, this);
}

MetricsSampler::~MetricsSampler() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    condition_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool MetricsSampler::GetLatest(SystemMetrics* metrics,
                               std::uint64_t* generation,
                               std::string* error) const {
    if (metrics == nullptr || generation == nullptr || error == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    *generation = generation_;
    *error = latest_error_;
    if (!has_snapshot_ || !latest_error_.empty()) {
        return false;
    }

    *metrics = latest_metrics_;
    return true;
}

bool MetricsSampler::WaitForNext(std::uint64_t previous_generation,
                                 SystemMetrics* metrics,
                                 std::uint64_t* generation,
                                 std::string* error) {
    if (metrics == nullptr || generation == nullptr || error == nullptr) {
        return false;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this, previous_generation] {
        return stopping_ || generation_ > previous_generation;
    });

    *generation = generation_;
    if (stopping_) {
        *error = "metrics sampler is stopping";
        return false;
    }

    *error = latest_error_;
    if (!has_snapshot_ || !latest_error_.empty()) {
        return false;
    }

    *metrics = latest_metrics_;
    return true;
}

void MetricsSampler::Sample() {
    SystemMetrics metrics;
    std::string error;
    const bool success = collector_.Collect(&metrics, &error);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++generation_;
        if (success) {
            latest_metrics_ = metrics;
            latest_error_.clear();
            has_snapshot_ = true;
        } else {
            latest_error_ = std::move(error);
        }
    }
    condition_.notify_all();
}

void MetricsSampler::Run() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!condition_.wait_for(lock, sample_interval_,
                                [this] { return stopping_; })) {
        lock.unlock();
        Sample();
        lock.lock();
    }
}
