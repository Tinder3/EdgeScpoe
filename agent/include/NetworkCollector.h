#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct NetworkInterfaceInfo {
    std::string name;
    std::vector<std::string> ipv4_addresses;
    std::vector<std::string> ipv6_addresses;
    std::uint64_t rx_bytes = 0;
    std::uint64_t tx_bytes = 0;
    std::string state;
};

struct TcpConnectionInfo {
    std::string protocol;
    std::string local_address;
    std::uint32_t local_port = 0;
    std::string remote_address;
    std::uint32_t remote_port = 0;
    std::string state;
    std::uint64_t inode = 0;
    std::int32_t pid = 0;
};

class NetworkCollector {
public:
    bool GetInterfaces(std::vector<NetworkInterfaceInfo>* interfaces,
                       std::string* error) const;
    bool ListTcpConnections(std::vector<TcpConnectionInfo>* connections,
                            std::string* error) const;

    static bool ParseTcpLine(const std::string& line, bool ipv6,
                             TcpConnectionInfo* connection,
                             std::string* error);
};
