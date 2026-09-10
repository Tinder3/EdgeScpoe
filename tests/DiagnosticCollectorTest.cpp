#include "DiagnosticCollector.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace {

TEST(DiagnosticCollectorTest, CreatesControlledGzipBundleAndFindsItById) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("edgescope-diagnostic-test-" + std::to_string(getpid()));
    std::filesystem::remove_all(root);
    ProcessCollector processes;
    NetworkCollector network;
    LogCollector logs("/tmp/edgescope-test-agent.log");
    DiagnosticBundleInfo bundle;
    std::filesystem::path stored_path;
    {
        DiagnosticCollector collector(root, &processes, &network, &logs);
        SystemMetrics metrics;
        metrics.memory_total_bytes = 1024;
        std::string error;
        ASSERT_TRUE(collector.CreateBundle(metrics, &bundle, &error)) << error;
        std::uint64_t stored_size = 0;
        ASSERT_TRUE(collector.GetBundlePath(bundle.id, &stored_path,
                                            &stored_size, &error));
        EXPECT_EQ(stored_size, bundle.size_bytes);
        EXPECT_GT(stored_size, 0U);
        std::ifstream input(stored_path, std::ios::binary);
        unsigned char header[2]{};
        input.read(reinterpret_cast<char*>(header), 2);
        EXPECT_EQ(header[0], 0x1f);
        EXPECT_EQ(header[1], 0x8b);
    }
    EXPECT_FALSE(std::filesystem::exists(stored_path));
    std::filesystem::remove_all(root);
}

TEST(DiagnosticCollectorTest, RejectsUnknownBundleId) {
    ProcessCollector processes;
    NetworkCollector network;
    LogCollector logs;
    DiagnosticCollector collector("/tmp/edgescope-diagnostic-unknown-test",
                                  &processes, &network, &logs);
    std::filesystem::path path;
    std::uint64_t size = 0;
    std::string error;
    EXPECT_FALSE(collector.GetBundlePath("../../etc/shadow", &path, &size,
                                         &error));
    EXPECT_EQ(error, "diagnostic bundle not found");
}

}  // namespace
