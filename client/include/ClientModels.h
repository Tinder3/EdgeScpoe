#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct AgentInfoData {
    QString hostname;
    QString kernel_version;
    quint64 uptime_seconds = 0;
    QString agent_version;
};

struct SystemMetricsData {
    double cpu_usage_percent = 0.0;
    quint64 memory_total_bytes = 0;
    quint64 memory_available_bytes = 0;
    quint64 memory_used_bytes = 0;
    double load_average_1m = 0.0;
    double load_average_5m = 0.0;
    double load_average_15m = 0.0;
    quint64 uptime_seconds = 0;
};

struct ProcessData {
    qint32 pid = 0;
    qint32 ppid = 0;
    QString name;
    QString state;
    QString command_line;
    double cpu_usage_percent = 0.0;
    quint64 resident_memory_bytes = 0;
    quint64 virtual_memory_bytes = 0;
    quint32 thread_count = 0;
    quint64 read_bytes = 0;
    quint64 write_bytes = 0;
    quint64 start_time_ticks = 0;
};

using ProcessList = QVector<ProcessData>;

struct NetworkInterfaceData {
    QString name;
    QString ipv4_addresses;
    QString ipv6_addresses;
    quint64 rx_bytes = 0;
    quint64 tx_bytes = 0;
    QString state;
};

struct TcpConnectionData {
    QString protocol;
    QString local_endpoint;
    QString remote_endpoint;
    QString state;
    quint64 inode = 0;
    qint32 pid = 0;
};

struct LogSourceData {
    QString id;
    QString display_name;
    bool available = false;
};

using NetworkInterfaceList = QVector<NetworkInterfaceData>;
using TcpConnectionList = QVector<TcpConnectionData>;
using LogSourceList = QVector<LogSourceData>;

Q_DECLARE_METATYPE(AgentInfoData)
Q_DECLARE_METATYPE(SystemMetricsData)
Q_DECLARE_METATYPE(ProcessData)
Q_DECLARE_METATYPE(ProcessList)
Q_DECLARE_METATYPE(NetworkInterfaceList)
Q_DECLARE_METATYPE(TcpConnectionList)
Q_DECLARE_METATYPE(LogSourceList)
