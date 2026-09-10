#include "LogCollector.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct AllowedLog {
    const char* id;
    const char* display_name;
    const char* path;
};

constexpr std::array<AllowedLog, 3> kAllowedLogs = {{
    {"system", "System log", "/var/log/syslog"},
    {"packages", "Package manager log", "/var/log/dpkg.log"},
    {"apt-history", "APT history", "/var/log/apt/history.log"},
}};

constexpr std::uint32_t kMaximumLines = 1000;
constexpr std::size_t kMaximumKeywordLength = 256;
constexpr std::size_t kMaximumLineLength = 4096;
constexpr std::size_t kReadBlockSize = 64 * 1024;
constexpr std::size_t kMaximumScanBytes = 8 * 1024 * 1024;

const AllowedLog* FindAllowedLog(const std::string& id) {
    const auto iterator = std::find_if(
        kAllowedLogs.begin(), kAllowedLogs.end(),
        [&id](const AllowedLog& log) { return id == log.id; });
    return iterator == kAllowedLogs.end() ? nullptr : &*iterator;
}

LogCollectorError OpenError() {
    if (errno == EACCES || errno == EPERM) {
        return LogCollectorError::kPermissionDenied;
    }
    if (errno == ENOENT) {
        return LogCollectorError::kNotFound;
    }
    return LogCollectorError::kIoError;
}

}  // namespace

std::string LogCollector::SanitizeUtf8(const std::string& input) {
    constexpr char kReplacement[] = "\xEF\xBF\xBD";
    std::string output;
    output.reserve(input.size());
    std::size_t index = 0;
    while (index < input.size()) {
        const auto first = static_cast<unsigned char>(input[index]);
        if (first < 0x80) {
            output.push_back(input[index++]);
            continue;
        }

        std::size_t length = 0;
        std::uint32_t code_point = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xE0) == 0xC0) {
            length = 2;
            code_point = first & 0x1F;
            minimum = 0x80;
        } else if ((first & 0xF0) == 0xE0) {
            length = 3;
            code_point = first & 0x0F;
            minimum = 0x800;
        } else if ((first & 0xF8) == 0xF0) {
            length = 4;
            code_point = first & 0x07;
            minimum = 0x10000;
        } else {
            output.append(kReplacement);
            ++index;
            continue;
        }

        bool valid = index + length <= input.size();
        for (std::size_t offset = 1; valid && offset < length; ++offset) {
            const auto continuation =
                static_cast<unsigned char>(input[index + offset]);
            if ((continuation & 0xC0) != 0x80) {
                valid = false;
            } else {
                code_point = (code_point << 6) | (continuation & 0x3F);
            }
        }
        valid = valid && code_point >= minimum && code_point <= 0x10FFFF &&
                !(code_point >= 0xD800 && code_point <= 0xDFFF);
        if (!valid) {
            output.append(kReplacement);
            ++index;
            continue;
        }

        output.append(input, index, length);
        index += length;
    }
    return output;
}

std::vector<LogSourceInfo> LogCollector::ListLogs() const {
    std::vector<LogSourceInfo> result;
    result.reserve(kAllowedLogs.size());
    for (const AllowedLog& log : kAllowedLogs) {
        std::error_code error;
        const bool available = std::filesystem::is_regular_file(log.path, error);
        result.push_back({log.id, log.display_name, available && !error});
    }
    return result;
}

bool LogCollector::ReadLog(const std::string& log_id, std::uint32_t max_lines,
                           const std::string& keyword, LogReadResult* result,
                           LogCollectorError* error_code,
                           std::string* error) const {
    if (result == nullptr || error_code == nullptr || error == nullptr) {
        return false;
    }
    *result = LogReadResult{};
    *error_code = LogCollectorError::kNone;
    error->clear();

    const AllowedLog* allowed_log = FindAllowedLog(log_id);
    if (allowed_log == nullptr) {
        *error_code = LogCollectorError::kInvalidArgument;
        *error = "unknown log id";
        return false;
    }
    if (max_lines == 0 || max_lines > kMaximumLines) {
        *error_code = LogCollectorError::kInvalidArgument;
        *error = "max_lines must be between 1 and 1000";
        return false;
    }
    if (keyword.size() > kMaximumKeywordLength) {
        *error_code = LogCollectorError::kInvalidArgument;
        *error = "keyword is too long";
        return false;
    }

    errno = 0;
    std::ifstream input(allowed_log->path, std::ios::binary);
    if (!input) {
        *error_code = OpenError();
        *error = "log file is not available";
        return false;
    }
    input.seekg(0, std::ios::end);
    const std::streamoff end = input.tellg();
    if (end < 0) {
        *error_code = LogCollectorError::kIoError;
        *error = "failed to determine log file size";
        return false;
    }

    std::streamoff position = end;
    std::size_t scanned_bytes = 0;
    std::size_t newline_count = 0;
    std::vector<std::string> reverse_chunks;
    while (position > 0 && scanned_bytes < kMaximumScanBytes) {
        const std::size_t block_size = static_cast<std::size_t>(
            std::min<std::streamoff>(position, kReadBlockSize));
        position -= static_cast<std::streamoff>(block_size);
        input.seekg(position);
        std::string chunk(block_size, '\0');
        input.read(chunk.data(), static_cast<std::streamsize>(block_size));
        if (input.gcount() != static_cast<std::streamsize>(block_size)) {
            *error_code = LogCollectorError::kIoError;
            *error = "failed while reading log file";
            return false;
        }
        newline_count +=
            static_cast<std::size_t>(std::count(chunk.begin(), chunk.end(), '\n'));
        scanned_bytes += block_size;
        reverse_chunks.push_back(std::move(chunk));
        if (keyword.empty() && newline_count > max_lines) {
            break;
        }
    }

    std::string contents;
    contents.reserve(scanned_bytes);
    for (auto iterator = reverse_chunks.rbegin();
         iterator != reverse_chunks.rend(); ++iterator) {
        contents += *iterator;
    }

    std::deque<std::string> selected_lines;
    std::istringstream lines(contents);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!keyword.empty() && line.find(keyword) == std::string::npos) {
            continue;
        }
        if (line.size() > kMaximumLineLength) {
            line.resize(kMaximumLineLength);
            result->truncated = true;
        }
        line = SanitizeUtf8(line);
        selected_lines.push_back(std::move(line));
        if (selected_lines.size() > max_lines) {
            selected_lines.pop_front();
            result->truncated = true;
        }
    }

    result->truncated = result->truncated || position > 0;
    result->lines.assign(std::make_move_iterator(selected_lines.begin()),
                         std::make_move_iterator(selected_lines.end()));
    return true;
}
