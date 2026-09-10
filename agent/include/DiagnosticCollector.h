#pragma once

#include "LogCollector.h"
#include "NetworkCollector.h"
#include "ProcessCollector.h"
#include "SystemCollector.h"

#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>

struct DiagnosticBundleInfo {
    std::string id;
    std::string filename;
    std::uint64_t size_bytes = 0;
    std::uint64_t created_at_unix_seconds = 0;
};

class DiagnosticCollector {
public:
    DiagnosticCollector(std::filesystem::path root_directory,
                        const ProcessCollector* process_collector,
                        const NetworkCollector* network_collector,
                        const LogCollector* log_collector);
    ~DiagnosticCollector();

    DiagnosticCollector(const DiagnosticCollector&) = delete;
    DiagnosticCollector& operator=(const DiagnosticCollector&) = delete;

    bool CreateBundle(const SystemMetrics& metrics,
                      DiagnosticBundleInfo* bundle, std::string* error);
    bool GetBundlePath(const std::string& id, std::filesystem::path* path,
                       std::uint64_t* size_bytes, std::string* error) const;

private:
    struct StoredBundle {
        DiagnosticBundleInfo info;
        std::filesystem::path path;
    };

    bool PrepareRoot(std::string* error) const;
    void RemoveOldBundles();

    const std::filesystem::path root_directory_;
    const ProcessCollector* process_collector_;
    const NetworkCollector* network_collector_;
    const LogCollector* log_collector_;
    mutable std::mutex mutex_;
    std::deque<StoredBundle> bundles_;
    std::uint64_t sequence_ = 0;
};
