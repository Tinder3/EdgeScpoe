#include "LogCollector.h"
#include "NetworkCollector.h"

#include <gtest/gtest.h>

#include <string>

TEST(NetworkCollectorTest, ParsesIpv4TcpConnection) {
    const std::string line =
        "0: 0100007F:1F90 00000000:0000 0A 00000000:00000000 "
        "00:00000000 00000000 1000 0 12345";
    TcpConnectionInfo connection;
    std::string error;

    ASSERT_TRUE(NetworkCollector::ParseTcpLine(line, false, &connection,
                                               &error))
        << error;
    EXPECT_EQ(connection.protocol, "tcp4");
    EXPECT_EQ(connection.local_address, "127.0.0.1");
    EXPECT_EQ(connection.local_port, 8080U);
    EXPECT_EQ(connection.remote_address, "0.0.0.0");
    EXPECT_EQ(connection.state, "LISTEN");
    EXPECT_EQ(connection.inode, 12345U);
}

TEST(NetworkCollectorTest, ParsesIpv6Loopback) {
    const std::string line =
        "0: 00000000000000000000000001000000:01BB "
        "00000000000000000000000000000000:0000 0A "
        "00000000:00000000 00:00000000 00000000 1000 0 54321";
    TcpConnectionInfo connection;
    std::string error;

    ASSERT_TRUE(NetworkCollector::ParseTcpLine(line, true, &connection,
                                               &error))
        << error;
    EXPECT_EQ(connection.protocol, "tcp6");
    EXPECT_EQ(connection.local_address, "::1");
    EXPECT_EQ(connection.local_port, 443U);
    EXPECT_EQ(connection.inode, 54321U);
}

TEST(NetworkCollectorTest, RejectsMalformedTcpLine) {
    TcpConnectionInfo connection;
    std::string error;
    EXPECT_FALSE(NetworkCollector::ParseTcpLine("invalid", false, &connection,
                                                &error));
    EXPECT_FALSE(error.empty());
}

TEST(LogCollectorTest, RejectsUnknownLogId) {
    LogCollector collector;
    LogReadResult result;
    LogCollectorError error_code = LogCollectorError::kNone;
    std::string error;

    EXPECT_FALSE(collector.ReadLog("../../etc/shadow", 10, "", &result,
                                   &error_code, &error));
    EXPECT_EQ(error_code, LogCollectorError::kInvalidArgument);
}

TEST(LogCollectorTest, RejectsExcessiveLineCount) {
    LogCollector collector;
    LogReadResult result;
    LogCollectorError error_code = LogCollectorError::kNone;
    std::string error;

    EXPECT_FALSE(collector.ReadLog("packages", 1001, "", &result,
                                   &error_code, &error));
    EXPECT_EQ(error_code, LogCollectorError::kInvalidArgument);
}

TEST(LogCollectorTest, ReplacesInvalidUtf8) {
    const std::string input = std::string("valid ") +
                              static_cast<char>(0xFF) + " text";
    EXPECT_EQ(LogCollector::SanitizeUtf8(input),
              std::string("valid \xEF\xBF\xBD text"));
}
