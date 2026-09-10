#pragma once

#include "ClientModels.h"
#include "EdgeScopeClient.h"

#include <QObject>
#include <QString>
#include <QtGlobal>

#include <memory>
#include <mutex>
#include <thread>

class RpcWorker : public QObject {
    Q_OBJECT

public:
    explicit RpcWorker(QObject* parent = nullptr);
    ~RpcWorker() override;

public slots:
    void ConnectToAgent(const QString& host, quint16 port);
    void DisconnectFromAgent();
    void RefreshMetrics();
    void RefreshProcesses();
    void GetProcessDetails(qint32 pid);
    void ControlProcess(qint32 pid, int action);
    void RefreshNetwork();
    void ListLogs();
    void ReadLog(const QString& log_id, quint32 max_lines,
                 const QString& keyword);
    void StartLogStream(const QString& log_id, const QString& keyword);
    void StopLogStream();
    void RefreshServices();
    void ControlService(const QString& name, int action);
    void CreateAndDownloadDiagnostic(const QString& destination);

signals:
    void Connected(const AgentInfoData& agent_info);
    void Disconnected();
    void MetricsReady(const SystemMetricsData& metrics);
    void ProcessesReady(const ProcessList& processes);
    void ProcessDetailsReady(const ProcessData& process);
    void NetworkReady(const NetworkInterfaceList& interfaces,
                      const TcpConnectionList& connections);
    void LogsReady(const LogSourceList& logs);
    void LogContentReady(const QStringList& lines, bool truncated);
    void LogLineReady(const QString& line);
    void LogStreamStopped();
    void ServicesReady(const ServiceList& services);
    void DiagnosticProgress(quint64 received, quint64 total);
    void DiagnosticReady(const QString& path, quint64 size_bytes);
    void OperationSucceeded(const QString& message);
    void RpcError(const QString& operation, const QString& message,
                  bool connection_lost);

private:
    void EmitRpcError(const QString& operation, const grpc::Status& status);

    std::shared_ptr<EdgeScopeClient> client_;
    std::mutex stream_mutex_;
    std::shared_ptr<grpc::ClientContext> stream_context_;
    std::thread stream_thread_;
};
