#include "ProcessCollector.h"
#include "ProcessController.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <unistd.h>

TEST(ProcessCollectorTest, ParsesStatWithSpacesAndParenthesesInName) {
    const std::string stat =
        "123 (worker (test) name) R 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 "
        "16 2 17 1800";
    ProcessInfo process;
    std::uint64_t cpu_ticks = 0;
    std::string error;

    ASSERT_TRUE(ProcessCollector::ParseStatLine(stat, &process, &cpu_ticks,
                                                &error))
        << error;
    EXPECT_EQ(process.pid, 123);
    EXPECT_EQ(process.ppid, 1);
    EXPECT_EQ(process.name, "worker (test) name");
    EXPECT_EQ(process.state, "R");
    EXPECT_EQ(process.thread_count, 2U);
    EXPECT_EQ(process.start_time_ticks, 1800U);
    EXPECT_EQ(cpu_ticks, 23U);
}

TEST(ProcessCollectorTest, RejectsIncompleteStat) {
    ProcessInfo process;
    std::uint64_t cpu_ticks = 0;
    std::string error;

    EXPECT_FALSE(ProcessCollector::ParseStatLine(
        "123 (incomplete) S 1 2 3", &process, &cpu_ticks, &error));
    EXPECT_FALSE(error.empty());
}

TEST(ProcessCollectorTest, ParsesStatusMemoryAndThreads) {
    const std::string status =
        "Name:\tworker\n"
        "VmSize:\t2048 kB\n"
        "VmRSS:\t512 kB\n"
        "Threads:\t4\n";
    ProcessInfo process;
    std::string error;

    ASSERT_TRUE(ProcessCollector::ParseStatus(status, &process, &error))
        << error;
    EXPECT_EQ(process.virtual_memory_bytes, 2048U * 1024U);
    EXPECT_EQ(process.resident_memory_bytes, 512U * 1024U);
    EXPECT_EQ(process.thread_count, 4U);
}

TEST(ProcessCollectorTest, RejectsInvalidStatusUnit) {
    ProcessInfo process;
    std::string error;

    EXPECT_FALSE(ProcessCollector::ParseStatus("VmRSS: 512 MB\n", &process,
                                               &error));
    EXPECT_FALSE(error.empty());
}

TEST(ProcessCollectorTest, ParsesIoCounters) {
    ProcessInfo process;
    std::string error;

    ASSERT_TRUE(ProcessCollector::ParseIo(
        "rchar: 1\nread_bytes: 4096\nwrite_bytes: 8192\n", &process,
        &error));
    EXPECT_EQ(process.read_bytes, 4096U);
    EXPECT_EQ(process.write_bytes, 8192U);
}

TEST(ProcessCollectorTest, ReadsCurrentProcessFromProc) {
    ProcessCollector collector;
    ProcessInfo process;
    ProcessCollectorError error_code = ProcessCollectorError::kNone;
    std::string error;

    ASSERT_TRUE(collector.GetProcess(static_cast<std::int32_t>(getpid()),
                                     &process, &error_code, &error))
        << error;
    EXPECT_EQ(process.pid, static_cast<std::int32_t>(getpid()));
    EXPECT_FALSE(process.name.empty());
    EXPECT_FALSE(process.command_line.empty());
}

TEST(ProcessControllerTest, RejectsProtectedPid) {
    ProcessController controller;
    ProcessControlError error_code = ProcessControlError::kNone;
    std::string error;

    EXPECT_FALSE(controller.Control(1, ProcessControlAction::kTerminate,
                                    &error_code, &error));
    EXPECT_EQ(error_code, ProcessControlError::kInvalidArgument);
    EXPECT_FALSE(error.empty());
}

TEST(ProcessControllerTest, RejectsUnknownAction) {
    ProcessController controller;
    ProcessControlError error_code = ProcessControlError::kNone;
    std::string error;

    EXPECT_FALSE(controller.Control(
        123456789, static_cast<ProcessControlAction>(999), &error_code,
        &error));
    EXPECT_EQ(error_code, ProcessControlError::kInvalidArgument);
    EXPECT_FALSE(error.empty());
}
