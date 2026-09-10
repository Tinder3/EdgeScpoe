#include "ProcessTableModel.h"

#include <QString>
#include <utility>

ProcessTableModel::ProcessTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int ProcessTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : processes_.size();
}

int ProcessTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : kColumnCount;
}

QVariant ProcessTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= processes_.size()) {
        return {};
    }

    const ProcessData& process = processes_.at(index.row());
    if (role == Qt::UserRole) {
        switch (index.column()) {
            case kPid:
                return process.pid;
            case kName:
                return process.name;
            case kCpu:
                return process.cpu_usage_percent;
            case kMemory:
                return static_cast<qulonglong>(process.resident_memory_bytes);
            case kState:
                return process.state;
            case kThreads:
                return process.thread_count;
            default:
                return {};
        }
    }
    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
        case kPid:
            return process.pid;
        case kName:
            return process.name;
        case kCpu:
            return QString::number(process.cpu_usage_percent, 'f', 1) + "%";
        case kMemory:
            return QString::number(process.resident_memory_bytes /
                                       (1024.0 * 1024.0),
                                   'f', 1) +
                   " MiB";
        case kState:
            return process.state;
        case kThreads:
            return process.thread_count;
        default:
            return {};
    }
}

QVariant ProcessTableModel::headerData(int section,
                                       Qt::Orientation orientation,
                                       int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
        case kPid:
            return "PID";
        case kName:
            return "Name";
        case kCpu:
            return "CPU";
        case kMemory:
            return "Memory";
        case kState:
            return "State";
        case kThreads:
            return "Threads";
        default:
            return {};
    }
}

void ProcessTableModel::SetProcesses(ProcessList processes) {
    beginResetModel();
    processes_ = std::move(processes);
    endResetModel();
}

const ProcessData* ProcessTableModel::ProcessAt(int row) const {
    if (row < 0 || row >= processes_.size()) {
        return nullptr;
    }
    return &processes_.at(row);
}
