#include "MainWindow.h"

#include "ProcessTableModel.h"
#include "RpcWorker.h"
#include "edgescope.pb.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QString FormatBytes(quint64 bytes) {
    constexpr double kBytesPerGibibyte = 1024.0 * 1024.0 * 1024.0;
    constexpr double kBytesPerMebibyte = 1024.0 * 1024.0;
    if (bytes >= kBytesPerGibibyte) {
        return QString::number(bytes / kBytesPerGibibyte, 'f', 1) + " GiB";
    }
    return QString::number(bytes / kBytesPerMebibyte, 'f', 1) + " MiB";
}

QLabel* AddValueRow(QFormLayout* layout, const QString& label) {
    auto* value = new QLabel("-");
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addRow(label, value);
    return value;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    qRegisterMetaType<AgentInfoData>();
    qRegisterMetaType<SystemMetricsData>();
    qRegisterMetaType<ProcessData>();
    qRegisterMetaType<ProcessList>();
    qRegisterMetaType<NetworkInterfaceList>();
    qRegisterMetaType<TcpConnectionList>();
    qRegisterMetaType<LogSourceList>();

    BuildUi();

    rpc_worker_ = new RpcWorker;
    rpc_worker_->moveToThread(&rpc_thread_);
    connect(&rpc_thread_, &QThread::finished, rpc_worker_, &QObject::deleteLater);
    connect(this, &MainWindow::RequestConnect, rpc_worker_,
            &RpcWorker::ConnectToAgent);
    connect(this, &MainWindow::RequestDisconnect, rpc_worker_,
            &RpcWorker::DisconnectFromAgent);
    connect(this, &MainWindow::RequestMetrics, rpc_worker_,
            &RpcWorker::RefreshMetrics);
    connect(this, &MainWindow::RequestProcesses, rpc_worker_,
            &RpcWorker::RefreshProcesses);
    connect(this, &MainWindow::RequestProcessDetails, rpc_worker_,
            &RpcWorker::GetProcessDetails);
    connect(this, &MainWindow::RequestProcessControl, rpc_worker_,
            &RpcWorker::ControlProcess);
    connect(this, &MainWindow::RequestNetwork, rpc_worker_,
            &RpcWorker::RefreshNetwork);
    connect(this, &MainWindow::RequestLogs, rpc_worker_, &RpcWorker::ListLogs);
    connect(this, &MainWindow::RequestLogContent, rpc_worker_,
            &RpcWorker::ReadLog);
    connect(rpc_worker_, &RpcWorker::Connected, this,
            &MainWindow::HandleConnected);
    connect(rpc_worker_, &RpcWorker::Disconnected, this,
            &MainWindow::HandleDisconnected);
    connect(rpc_worker_, &RpcWorker::MetricsReady, this,
            &MainWindow::UpdateMetrics);
    connect(rpc_worker_, &RpcWorker::ProcessesReady, this,
            &MainWindow::UpdateProcesses);
    connect(rpc_worker_, &RpcWorker::ProcessDetailsReady, this,
            &MainWindow::UpdateProcessDetails);
    connect(rpc_worker_, &RpcWorker::NetworkReady, this,
            &MainWindow::UpdateNetwork);
    connect(rpc_worker_, &RpcWorker::LogsReady, this, &MainWindow::UpdateLogs);
    connect(rpc_worker_, &RpcWorker::LogContentReady, this,
            &MainWindow::UpdateLogContent);
    connect(rpc_worker_, &RpcWorker::RpcError, this,
            &MainWindow::HandleRpcError);
    connect(rpc_worker_, &RpcWorker::OperationSucceeded, this,
            [this](const QString& message) {
                statusBar()->showMessage(message, 5000);
            });
    rpc_thread_.start();
    SetConnected(false);
}

MainWindow::~MainWindow() {
    metrics_timer_->stop();
    processes_timer_->stop();
    if (rpc_thread_.isRunning()) {
        QMetaObject::invokeMethod(rpc_worker_, "DisconnectFromAgent",
                                  Qt::BlockingQueuedConnection);
        rpc_thread_.quit();
        rpc_thread_.wait();
    }
}

void MainWindow::BuildUi() {
    setWindowTitle("EdgeScope v0.5");
    resize(1000, 680);

    auto* central = new QWidget(this);
    auto* main_layout = new QVBoxLayout(central);

    auto* connection_layout = new QHBoxLayout;
    host_edit_ = new QLineEdit("127.0.0.1");
    host_edit_->setPlaceholderText("Agent IP or hostname");
    port_spin_ = new QSpinBox;
    port_spin_->setRange(1, 65535);
    port_spin_->setValue(50051);
    connect_button_ = new QPushButton("Connect");
    disconnect_button_ = new QPushButton("Disconnect");
    reconnect_button_ = new QPushButton("Reconnect");
    connection_status_ = new QLabel("Disconnected");
    connection_layout->addWidget(new QLabel("Agent:"));
    connection_layout->addWidget(host_edit_, 1);
    connection_layout->addWidget(new QLabel("Port:"));
    connection_layout->addWidget(port_spin_);
    connection_layout->addWidget(connect_button_);
    connection_layout->addWidget(disconnect_button_);
    connection_layout->addWidget(reconnect_button_);
    connection_layout->addWidget(connection_status_);
    main_layout->addLayout(connection_layout);

    auto* tabs = new QTabWidget;
    auto* overview = new QWidget;
    auto* overview_layout = new QFormLayout(overview);
    hostname_value_ = AddValueRow(overview_layout, "Hostname:");
    kernel_value_ = AddValueRow(overview_layout, "Kernel:");
    agent_version_value_ = AddValueRow(overview_layout, "Agent version:");
    uptime_value_ = AddValueRow(overview_layout, "Uptime:");
    cpu_value_ = AddValueRow(overview_layout, "CPU usage:");
    memory_value_ = AddValueRow(overview_layout, "Memory:");
    available_memory_value_ =
        AddValueRow(overview_layout, "Memory available:");
    load_value_ = AddValueRow(overview_layout, "Load average:");
    tabs->addTab(overview, "Overview");

    auto* processes = new QWidget;
    auto* process_layout = new QVBoxLayout(processes);
    process_filter_ = new QLineEdit;
    process_filter_->setPlaceholderText("Search PID or process name");
    process_layout->addWidget(process_filter_);

    process_model_ = new ProcessTableModel(this);
    process_proxy_ = new QSortFilterProxyModel(this);
    process_proxy_->setSourceModel(process_model_);
    process_proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    process_proxy_->setFilterKeyColumn(-1);
    process_proxy_->setSortRole(Qt::UserRole);
    process_table_ = new QTableView;
    process_table_->setModel(process_proxy_);
    process_table_->setSortingEnabled(true);
    process_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    process_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    process_table_->horizontalHeader()->setStretchLastSection(true);
    process_table_->verticalHeader()->setVisible(false);
    process_layout->addWidget(process_table_, 1);

    auto* process_buttons = new QHBoxLayout;
    auto* details_button = new QPushButton("Details");
    auto* terminate_button = new QPushButton("Terminate");
    auto* kill_button = new QPushButton("Kill");
    auto* stop_button = new QPushButton("Stop");
    auto* continue_button = new QPushButton("Continue");
    process_buttons->addWidget(details_button);
    process_buttons->addStretch();
    process_buttons->addWidget(terminate_button);
    process_buttons->addWidget(kill_button);
    process_buttons->addWidget(stop_button);
    process_buttons->addWidget(continue_button);
    process_layout->addLayout(process_buttons);

    process_details_ = new QPlainTextEdit;
    process_details_->setReadOnly(true);
    process_details_->setPlaceholderText("Select a process and click Details");
    process_details_->setMaximumBlockCount(100);
    process_layout->addWidget(process_details_);
    tabs->addTab(processes, "Processes");

    auto* network = new QWidget;
    auto* network_layout = new QVBoxLayout(network);
    network_refresh_button_ = new QPushButton("Refresh network");
    network_layout->addWidget(network_refresh_button_, 0, Qt::AlignLeft);
    network_layout->addWidget(new QLabel("Network interfaces"));
    interface_table_ = new QTableWidget;
    interface_table_->setColumnCount(6);
    interface_table_->setHorizontalHeaderLabels(
        {"Interface", "IPv4", "IPv6", "State", "RX", "TX"});
    interface_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    interface_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    interface_table_->horizontalHeader()->setStretchLastSection(true);
    network_layout->addWidget(interface_table_);
    network_layout->addWidget(new QLabel("TCP connections"));
    connection_table_ = new QTableWidget;
    connection_table_->setColumnCount(6);
    connection_table_->setHorizontalHeaderLabels(
        {"Protocol", "Local", "Remote", "State", "PID", "Inode"});
    connection_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connection_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connection_table_->horizontalHeader()->setStretchLastSection(true);
    network_layout->addWidget(connection_table_, 1);
    tabs->addTab(network, "Network");

    auto* logs = new QWidget;
    auto* logs_layout = new QVBoxLayout(logs);
    auto* log_controls = new QHBoxLayout;
    log_source_combo_ = new QComboBox;
    log_lines_spin_ = new QSpinBox;
    log_lines_spin_->setRange(1, 1000);
    log_lines_spin_->setValue(200);
    log_keyword_edit_ = new QLineEdit;
    log_keyword_edit_->setMaxLength(256);
    log_keyword_edit_->setPlaceholderText("Optional keyword");
    log_refresh_button_ = new QPushButton("Read tail");
    log_controls->addWidget(new QLabel("Log:"));
    log_controls->addWidget(log_source_combo_, 1);
    log_controls->addWidget(new QLabel("Lines:"));
    log_controls->addWidget(log_lines_spin_);
    log_controls->addWidget(log_keyword_edit_, 1);
    log_controls->addWidget(log_refresh_button_);
    logs_layout->addLayout(log_controls);
    log_output_ = new QPlainTextEdit;
    log_output_->setReadOnly(true);
    log_output_->setMaximumBlockCount(2000);
    logs_layout->addWidget(log_output_, 1);
    tabs->addTab(logs, "Logs");

    main_layout->addWidget(tabs, 1);
    setCentralWidget(central);

    metrics_timer_ = new QTimer(this);
    metrics_timer_->setInterval(1000);
    processes_timer_ = new QTimer(this);
    processes_timer_->setInterval(3000);

    connect(connect_button_, &QPushButton::clicked, this,
            &MainWindow::ConnectToAgent);
    connect(disconnect_button_, &QPushButton::clicked, this,
            &MainWindow::DisconnectFromAgent);
    connect(reconnect_button_, &QPushButton::clicked, this,
            &MainWindow::ReconnectToAgent);
    connect(metrics_timer_, &QTimer::timeout, this,
            &MainWindow::RequestMetrics);
    connect(processes_timer_, &QTimer::timeout, this,
            &MainWindow::RequestProcesses);
    connect(process_filter_, &QLineEdit::textChanged, process_proxy_,
            &QSortFilterProxyModel::setFilterFixedString);
    connect(details_button, &QPushButton::clicked, this,
            &MainWindow::ShowSelectedProcessDetails);
    connect(process_table_, &QTableView::doubleClicked, this,
            [this](const QModelIndex&) { ShowSelectedProcessDetails(); });
    connect(terminate_button, &QPushButton::clicked, this, [this] {
        ConfirmProcessControl(edgescope::v1::PROCESS_ACTION_TERMINATE,
                              "terminate");
    });
    connect(kill_button, &QPushButton::clicked, this, [this] {
        ConfirmProcessControl(edgescope::v1::PROCESS_ACTION_KILL, "kill");
    });
    connect(stop_button, &QPushButton::clicked, this, [this] {
        ConfirmProcessControl(edgescope::v1::PROCESS_ACTION_STOP, "stop");
    });
    connect(continue_button, &QPushButton::clicked, this, [this] {
        ConfirmProcessControl(edgescope::v1::PROCESS_ACTION_CONTINUE,
                              "continue");
    });
    connect(network_refresh_button_, &QPushButton::clicked, this,
            &MainWindow::RequestNetwork);
    connect(log_refresh_button_, &QPushButton::clicked, this, [this] {
        const QString log_id = log_source_combo_->currentData().toString();
        if (log_id.isEmpty()) {
            QMessageBox::information(this, "Logs", "No log source selected.");
            return;
        }
        emit RequestLogContent(log_id,
                               static_cast<quint32>(log_lines_spin_->value()),
                               log_keyword_edit_->text());
    });
}

void MainWindow::ConnectToAgent() {
    const QString host = host_edit_->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::warning(this, "Invalid address",
                             "Agent address cannot be empty.");
        return;
    }
    connection_status_->setText("Connecting...");
    connect_button_->setEnabled(false);
    emit RequestConnect(host, static_cast<quint16>(port_spin_->value()));
}

void MainWindow::DisconnectFromAgent() {
    metrics_timer_->stop();
    processes_timer_->stop();
    emit RequestDisconnect();
}

void MainWindow::ReconnectToAgent() {
    metrics_timer_->stop();
    processes_timer_->stop();
    connection_status_->setText("Reconnecting...");
    emit RequestDisconnect();
    emit RequestConnect(host_edit_->text().trimmed(),
                        static_cast<quint16>(port_spin_->value()));
}

void MainWindow::HandleConnected(const AgentInfoData& agent_info) {
    hostname_value_->setText(agent_info.hostname);
    kernel_value_->setText(agent_info.kernel_version);
    agent_version_value_->setText(agent_info.agent_version);
    uptime_value_->setText(QString::number(agent_info.uptime_seconds) + " s");
    SetConnected(true);
    metrics_timer_->start();
    processes_timer_->start();
    emit RequestMetrics();
    emit RequestProcesses();
    emit RequestNetwork();
    emit RequestLogs();
}

void MainWindow::HandleDisconnected() {
    SetConnected(false);
    statusBar()->showMessage("Disconnected from Agent", 5000);
}

void MainWindow::UpdateMetrics(const SystemMetricsData& metrics) {
    cpu_value_->setText(QString::number(metrics.cpu_usage_percent, 'f', 1) + "%");
    memory_value_->setText(FormatBytes(metrics.memory_used_bytes) + " / " +
                           FormatBytes(metrics.memory_total_bytes));
    available_memory_value_->setText(
        FormatBytes(metrics.memory_available_bytes));
    load_value_->setText(QString("%1 / %2 / %3")
                             .arg(metrics.load_average_1m, 0, 'f', 2)
                             .arg(metrics.load_average_5m, 0, 'f', 2)
                             .arg(metrics.load_average_15m, 0, 'f', 2));
    uptime_value_->setText(QString::number(metrics.uptime_seconds) + " s");
}

void MainWindow::UpdateProcesses(const ProcessList& processes) {
    process_model_->SetProcesses(processes);
    statusBar()->showMessage(
        QString("Loaded %1 processes").arg(processes.size()), 3000);
}

void MainWindow::UpdateProcessDetails(const ProcessData& process) {
    process_details_->setPlainText(
        QString("PID: %1\nPPID: %2\nName: %3\nState: %4\nCommand: %5\n"
                "CPU: %6%\nRSS: %7\nVmSize: %8\nThreads: %9\n"
                "Read bytes: %10\nWrite bytes: %11\nStart ticks: %12")
            .arg(process.pid)
            .arg(process.ppid)
            .arg(process.name)
            .arg(process.state)
            .arg(process.command_line)
            .arg(process.cpu_usage_percent, 0, 'f', 1)
            .arg(FormatBytes(process.resident_memory_bytes))
            .arg(FormatBytes(process.virtual_memory_bytes))
            .arg(process.thread_count)
            .arg(process.read_bytes)
            .arg(process.write_bytes)
            .arg(process.start_time_ticks));
}

void MainWindow::UpdateNetwork(
    const NetworkInterfaceList& interfaces,
    const TcpConnectionList& connections) {
    interface_table_->setRowCount(interfaces.size());
    for (int row = 0; row < interfaces.size(); ++row) {
        const NetworkInterfaceData& interface = interfaces.at(row);
        const QStringList values = {
            interface.name, interface.ipv4_addresses, interface.ipv6_addresses,
            interface.state, FormatBytes(interface.rx_bytes),
            FormatBytes(interface.tx_bytes)};
        for (int column = 0; column < values.size(); ++column) {
            interface_table_->setItem(row, column,
                                      new QTableWidgetItem(values.at(column)));
        }
    }
    interface_table_->resizeColumnsToContents();

    connection_table_->setRowCount(connections.size());
    for (int row = 0; row < connections.size(); ++row) {
        const TcpConnectionData& connection = connections.at(row);
        const QStringList values = {
            connection.protocol,
            connection.local_endpoint,
            connection.remote_endpoint,
            connection.state,
            connection.pid == 0 ? "-" : QString::number(connection.pid),
            QString::number(connection.inode)};
        for (int column = 0; column < values.size(); ++column) {
            connection_table_->setItem(row, column,
                                       new QTableWidgetItem(values.at(column)));
        }
    }
    connection_table_->resizeColumnsToContents();
    statusBar()->showMessage(
        QString("Loaded %1 interfaces and %2 TCP connections")
            .arg(interfaces.size())
            .arg(connections.size()),
        4000);
}

void MainWindow::UpdateLogs(const LogSourceList& logs) {
    log_source_combo_->clear();
    for (const LogSourceData& log : logs) {
        const QString label = log.display_name +
                              (log.available ? QString() : " (unavailable)");
        log_source_combo_->addItem(label, log.id);
    }
}

void MainWindow::UpdateLogContent(const QStringList& lines, bool truncated) {
    log_output_->setPlainText(lines.join('\n'));
    statusBar()->showMessage(
        QString("Loaded %1 log lines%2")
            .arg(lines.size())
            .arg(truncated ? " (result truncated)" : ""),
        5000);
}

void MainWindow::HandleRpcError(const QString& operation,
                                const QString& message,
                                bool connection_lost) {
    statusBar()->showMessage(operation + ": " + message, 8000);
    if (connection_lost) {
        metrics_timer_->stop();
        processes_timer_->stop();
        SetConnected(false);
        QMessageBox::warning(this, "Agent connection lost", message);
    } else {
        QMessageBox::warning(this, operation + " failed", message);
        if (operation == "Connect") {
            SetConnected(false);
        }
    }
}

void MainWindow::ShowSelectedProcessDetails() {
    const qint32 pid = SelectedPid();
    if (pid <= 0) {
        QMessageBox::information(this, "Process details",
                                 "Select a process first.");
        return;
    }
    emit RequestProcessDetails(pid);
}

void MainWindow::SetConnected(bool connected) {
    connection_status_->setText(connected ? "Connected" : "Disconnected");
    connect_button_->setEnabled(!connected);
    disconnect_button_->setEnabled(connected);
    reconnect_button_->setEnabled(connected);
    network_refresh_button_->setEnabled(connected);
    log_refresh_button_->setEnabled(connected);
    host_edit_->setEnabled(!connected);
    port_spin_->setEnabled(!connected);
    if (!connected) {
        metrics_timer_->stop();
        processes_timer_->stop();
    }
}

qint32 MainWindow::SelectedPid() const {
    const QModelIndex proxy_index = process_table_->currentIndex();
    if (!proxy_index.isValid()) {
        return 0;
    }
    const QModelIndex source_index = process_proxy_->mapToSource(proxy_index);
    const ProcessData* process = process_model_->ProcessAt(source_index.row());
    return process == nullptr ? 0 : process->pid;
}

void MainWindow::ConfirmProcessControl(int action,
                                       const QString& action_name) {
    const qint32 pid = SelectedPid();
    if (pid <= 0) {
        QMessageBox::information(this, "Process control",
                                 "Select a process first.");
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, "Confirm process operation",
        QString("Really %1 process %2?").arg(action_name).arg(pid),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes) {
        emit RequestProcessControl(pid, action);
    }
}
