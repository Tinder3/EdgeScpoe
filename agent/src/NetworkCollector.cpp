#include "NetworkCollector.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

bool ParseUnsigned(const std::string& text, int base, std::uint64_t* value) {
    if (text.empty() || value == nullptr) {
        return false;
    }
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, *value, base);
    return result.ec == std::errc{} && result.ptr == end;
}

bool ParseEndpoint(const std::string& text, bool ipv6, std::string* address,
                   std::uint32_t* port) {
    const std::size_t separator = text.find(':');
    if (separator == std::string::npos) {
        return false;
    }
    const std::string address_hex = text.substr(0, separator);
    const std::string port_hex = text.substr(separator + 1);
    std::uint64_t parsed_port = 0;
    if (!ParseUnsigned(port_hex, 16, &parsed_port) || parsed_port > 65535) {
        return false;
    }
    *port = static_cast<std::uint32_t>(parsed_port);

    std::array<unsigned char, 16> bytes{};
    const std::size_t expected_size = ipv6 ? 32 : 8;
    if (address_hex.size() != expected_size) {
        return false;
    }

    const std::size_t word_count = ipv6 ? 4 : 1;
    for (std::size_t word = 0; word < word_count; ++word) {
        for (std::size_t byte = 0; byte < 4; ++byte) {
            const std::size_t source = word * 8 + (3 - byte) * 2;
            std::uint64_t parsed_byte = 0;
            if (!ParseUnsigned(address_hex.substr(source, 2), 16,
                               &parsed_byte)) {
                return false;
            }
            bytes[word * 4 + byte] = static_cast<unsigned char>(parsed_byte);
        }
    }

    char buffer[INET6_ADDRSTRLEN]{};
    const int family = ipv6 ? AF_INET6 : AF_INET;
    if (inet_ntop(family, bytes.data(), buffer, sizeof(buffer)) == nullptr) {
        return false;
    }
    *address = buffer;
    return true;
}

std::string TcpStateName(const std::string& state) {
    static const std::map<std::string, std::string> names = {
        {"01", "ESTABLISHED"}, {"02", "SYN_SENT"},
        {"03", "SYN_RECV"},    {"04", "FIN_WAIT1"},
        {"05", "FIN_WAIT2"},   {"06", "TIME_WAIT"},
        {"07", "CLOSE"},       {"08", "CLOSE_WAIT"},
        {"09", "LAST_ACK"},    {"0A", "LISTEN"},
        {"0B", "CLOSING"},     {"0C", "NEW_SYN_RECV"},
    };
    const auto iterator = names.find(state);
    return iterator == names.end() ? "UNKNOWN(" + state + ")"
                                   : iterator->second;
}

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string value;
    if (input && std::getline(input, value)) {
        return value;
    }
    return {};
}

std::uint64_t ReadCounter(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::uint64_t value = 0;
    return input && (input >> value) ? value : 0;
}

std::unordered_map<std::uint64_t, std::int32_t> BuildSocketPidMap() {
    std::unordered_map<std::uint64_t, std::int32_t> result;
    std::error_code error;
    std::filesystem::directory_iterator process_iterator("/proc", error);
    const std::filesystem::directory_iterator end;
    while (!error && process_iterator != end) {
        const std::string name = process_iterator->path().filename().string();
        std::uint64_t pid_value = 0;
        if (ParseUnsigned(name, 10, &pid_value) && pid_value > 0 &&
            pid_value <=
                static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
            const std::filesystem::path fd_directory =
                process_iterator->path() / "fd";
            std::error_code fd_error;
            std::filesystem::directory_iterator fd_iterator(fd_directory,
                                                            fd_error);
            while (!fd_error && fd_iterator != end) {
                std::error_code link_error;
                const std::string target =
                    std::filesystem::read_symlink(fd_iterator->path(), link_error)
                        .string();
                if (!link_error && target.rfind("socket:[", 0) == 0 &&
                    target.back() == ']') {
                    std::uint64_t inode = 0;
                    const std::string inode_text =
                        target.substr(8, target.size() - 9);
                    if (ParseUnsigned(inode_text, 10, &inode)) {
                        result.emplace(inode, static_cast<std::int32_t>(pid_value));
                    }
                }
                fd_iterator.increment(fd_error);
            }
        }
        error.clear();
        process_iterator.increment(error);
    }
    return result;
}

bool ReadTcpFile(const char* path, bool ipv6,
                 std::vector<TcpConnectionInfo>* connections,
                 std::string* error) {
    std::ifstream input(path);
    if (!input) {
        *error = std::string("failed to open ") + path;
        return false;
    }

    std::string line;
    std::getline(input, line);
    while (std::getline(input, line)) {
        TcpConnectionInfo connection;
        std::string line_error;
        if (NetworkCollector::ParseTcpLine(line, ipv6, &connection,
                                           &line_error)) {
            connections->push_back(std::move(connection));
        }
    }
    return true;
}

}  // namespace

bool NetworkCollector::GetInterfaces(
    std::vector<NetworkInterfaceInfo>* interfaces, std::string* error) const {
    if (interfaces == nullptr || error == nullptr) {
        return false;
    }
    interfaces->clear();
    error->clear();

    ifaddrs* address_list = nullptr;
    if (getifaddrs(&address_list) != 0) {
        *error = std::string("getifaddrs failed: ") + std::strerror(errno);
        return false;
    }

    std::map<std::string, NetworkInterfaceInfo> by_name;
    for (const ifaddrs* entry = address_list; entry != nullptr;
         entry = entry->ifa_next) {
        if (entry->ifa_name == nullptr) {
            continue;
        }
        NetworkInterfaceInfo& interface = by_name[entry->ifa_name];
        interface.name = entry->ifa_name;
        if (entry->ifa_addr == nullptr) {
            continue;
        }

        const int family = entry->ifa_addr->sa_family;
        char buffer[INET6_ADDRSTRLEN]{};
        const void* source = nullptr;
        if (family == AF_INET) {
            source = &reinterpret_cast<const sockaddr_in*>(entry->ifa_addr)
                          ->sin_addr;
        } else if (family == AF_INET6) {
            source = &reinterpret_cast<const sockaddr_in6*>(entry->ifa_addr)
                          ->sin6_addr;
        }
        if (source != nullptr &&
            inet_ntop(family, source, buffer, sizeof(buffer)) != nullptr) {
            if (family == AF_INET) {
                interface.ipv4_addresses.emplace_back(buffer);
            } else {
                interface.ipv6_addresses.emplace_back(buffer);
            }
        }
    }
    freeifaddrs(address_list);

    for (auto& pair : by_name) {
        NetworkInterfaceInfo& interface = pair.second;
        const std::filesystem::path base =
            std::filesystem::path("/sys/class/net") / interface.name;
        interface.rx_bytes = ReadCounter(base / "statistics/rx_bytes");
        interface.tx_bytes = ReadCounter(base / "statistics/tx_bytes");
        interface.state = ReadTextFile(base / "operstate");
        if (interface.state.empty()) {
            interface.state = "unknown";
        }
        interfaces->push_back(std::move(interface));
    }
    return true;
}

bool NetworkCollector::ParseTcpLine(const std::string& line, bool ipv6,
                                    TcpConnectionInfo* connection,
                                    std::string* error) {
    if (connection == nullptr || error == nullptr) {
        return false;
    }
    std::istringstream stream(line);
    std::vector<std::string> fields;
    std::string field;
    while (stream >> field) {
        fields.push_back(field);
    }
    if (fields.size() < 10) {
        *error = "not enough fields in /proc/net/tcp line";
        return false;
    }

    if (!ParseEndpoint(fields[1], ipv6, &connection->local_address,
                       &connection->local_port) ||
        !ParseEndpoint(fields[2], ipv6, &connection->remote_address,
                       &connection->remote_port)) {
        *error = "invalid endpoint in /proc/net/tcp line";
        return false;
    }
    if (!ParseUnsigned(fields[9], 10, &connection->inode)) {
        *error = "invalid inode in /proc/net/tcp line";
        return false;
    }
    connection->protocol = ipv6 ? "tcp6" : "tcp4";
    connection->state = TcpStateName(fields[3]);
    return true;
}

bool NetworkCollector::ListTcpConnections(
    std::vector<TcpConnectionInfo>* connections, std::string* error) const {
    if (connections == nullptr || error == nullptr) {
        return false;
    }
    connections->clear();
    error->clear();
    if (!ReadTcpFile("/proc/net/tcp", false, connections, error)) {
        return false;
    }

    std::string ipv6_error;
    ReadTcpFile("/proc/net/tcp6", true, connections, &ipv6_error);
    const auto socket_pids = BuildSocketPidMap();
    for (TcpConnectionInfo& connection : *connections) {
        const auto iterator = socket_pids.find(connection.inode);
        if (iterator != socket_pids.end()) {
            connection.pid = iterator->second;
        }
    }
    return true;
}
