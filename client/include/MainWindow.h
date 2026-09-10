#pragma once

#include "ClientModels.h"

#include <QMainWindow>
#include <QStringList>
#include <QThread>

class QLabel;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QProgressBar;
class QSortFilterProxyModel;
class QSpinBox;
class QTableView;
class QTableWidget;
class QTimer;
class ProcessTableModel;
class RpcWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

signals:
    void RequestConnect(const QString& host, quint16 port);
    void RequestDisconnect();
    void RequestMetrics();
    void RequestProcesses();
    void RequestProcessDetails(qint32 pid);
    void RequestProcessControl(qint32 pid, int action);
    void RequestNetwork();
    void RequestLogs();
    void RequestLogContent(const QString& log_id, quint32 max_lines,
                           const QString& keyword);
    void RequestStartLogStream(const QString& log_id, const QString& keyword);
    void RequestStopLogStream();
    void RequestServices();
    void RequestServiceControl(const QString& name, int action);
    void RequestDiagnostic(const QString& destination);

private slots:
    void ConnectToAgent();
    void DisconnectFromAgent();
    void ReconnectToAgent();
    void HandleConnected(const AgentInfoData& agent_info);
    void HandleDisconnected();
    void UpdateMetrics(const SystemMetricsData& metrics);
    void UpdateProcesses(const ProcessList& processes);
    void UpdateProcessDetails(const ProcessData& process);
    void UpdateNetwork(const NetworkInterfaceList& interfaces,
                       const TcpConnectionList& connections);
    void UpdateLogs(const LogSourceList& logs);
    void UpdateLogContent(const QStringList& lines, bool truncated);
    void AppendLogLine(const QString& line);
    void HandleLogStreamStopped();
    void UpdateServices(const ServiceList& services);
    void UpdateDiagnosticProgress(quint64 received, quint64 total);
    void DiagnosticCompleted(const QString& path, quint64 size_bytes);
    void HandleRpcError(const QString& operation, const QString& message,
                        bool connection_lost);
    void ShowSelectedProcessDetails();

private:
    void BuildUi();
    void SetConnected(bool connected);
    qint32 SelectedPid() const;
    void ConfirmProcessControl(int action, const QString& action_name);
    void ConfirmServiceControl(int action, const QString& action_name);
    void CreateDiagnostic();

    QLineEdit* host_edit_ = nullptr;
    QSpinBox* port_spin_ = nullptr;
    QPushButton* connect_button_ = nullptr;
    QPushButton* disconnect_button_ = nullptr;
    QPushButton* reconnect_button_ = nullptr;
    QLabel* connection_status_ = nullptr;

    QLabel* hostname_value_ = nullptr;
    QLabel* kernel_value_ = nullptr;
    QLabel* agent_version_value_ = nullptr;
    QLabel* uptime_value_ = nullptr;
    QLabel* cpu_value_ = nullptr;
    QLabel* memory_value_ = nullptr;
    QLabel* available_memory_value_ = nullptr;
    QLabel* load_value_ = nullptr;

    QLineEdit* process_filter_ = nullptr;
    QTableView* process_table_ = nullptr;
    QPlainTextEdit* process_details_ = nullptr;
    ProcessTableModel* process_model_ = nullptr;
    QSortFilterProxyModel* process_proxy_ = nullptr;

    QTableWidget* interface_table_ = nullptr;
    QTableWidget* connection_table_ = nullptr;
    QPushButton* network_refresh_button_ = nullptr;

    QComboBox* log_source_combo_ = nullptr;
    QSpinBox* log_lines_spin_ = nullptr;
    QLineEdit* log_keyword_edit_ = nullptr;
    QPushButton* log_refresh_button_ = nullptr;
    QPushButton* log_stream_start_button_ = nullptr;
    QPushButton* log_stream_stop_button_ = nullptr;
    QPlainTextEdit* log_output_ = nullptr;

    QTableWidget* service_table_ = nullptr;
    QPushButton* service_refresh_button_ = nullptr;

    QPushButton* diagnostic_create_button_ = nullptr;
    QProgressBar* diagnostic_progress_ = nullptr;
    QLabel* diagnostic_status_ = nullptr;

    QTimer* metrics_timer_ = nullptr;
    QTimer* processes_timer_ = nullptr;
    QThread rpc_thread_;
    RpcWorker* rpc_worker_ = nullptr;
};
