#include "ProcessTableModel.h"

#include <gtest/gtest.h>

#include <Qt>

TEST(ProcessTableModelTest, ReplacesRowsWithoutRecreatingModel) {
    ProcessTableModel model;
    ProcessData first;
    first.pid = 42;
    first.name = "worker";
    first.cpu_usage_percent = 12.5;
    first.resident_memory_bytes = 4U * 1024U * 1024U;
    first.state = "S";
    first.thread_count = 3;

    ProcessList processes;
    processes.push_back(first);
    model.SetProcesses(processes);

    ASSERT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.columnCount(), 6);
    EXPECT_EQ(model.data(model.index(0, 0), Qt::DisplayRole).toInt(), 42);
    EXPECT_EQ(model.data(model.index(0, 1), Qt::DisplayRole).toString(),
              "worker");
    EXPECT_EQ(model.data(model.index(0, 2), Qt::UserRole).toDouble(), 12.5);

    model.SetProcesses({});
    EXPECT_EQ(model.rowCount(), 0);
}
