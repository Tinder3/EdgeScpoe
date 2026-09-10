#pragma once

#include "ClientModels.h"

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QVariant>

class ProcessTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit ProcessTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void SetProcesses(ProcessList processes);
    const ProcessData* ProcessAt(int row) const;

private:
    enum Column {
        kPid,
        kName,
        kCpu,
        kMemory,
        kState,
        kThreads,
        kColumnCount,
    };

    ProcessList processes_;
};
