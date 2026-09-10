# EdgeScope

EdgeScope 是一个面向 Linux 设备的远程诊断与系统监控项目。

## 当前版本

v1.0 已实现：

- 独立的 `edgescope-agent` 和 `edgescope-client` 进程
- gRPC/Protobuf 通信
- Agent 基本信息查询
- CPU、内存、Load Average 和 Uptime 采集
- Agent 后台统一指标采样
- Unary 和 Server Streaming 系统指标 RPC
- 进程列表和进程详情查询
- 受限的 TERM、KILL、STOP 和 CONTINUE 进程控制
- Qt6 Widgets Desktop Client
- Overview 实时系统指标页面
- 支持搜索、排序、详情和控制的 Processes 页面
- `QThread` 后台 RPC worker，避免阻塞 GUI 主线程
- Connect、Disconnect 和 Reconnect
- Network 页面：网络接口、IPv4/IPv6、RX/TX 和 TCP 连接
- TCP socket inode 到 PID 的尽力映射
- Logs 页面：白名单日志 Tail 和关键字过滤
- 日志行数、行长和扫描字节上限
- Agent 配置文件（监听地址、采样间隔、日志路径、服务白名单）
- 基于 spdlog 的 Agent 控制台与滚动文件日志
- Agent 自身日志可在 Logs 页面通过 `edgescope-agent` 白名单项查看
- Services 页面：systemd 服务状态、启动、停止和重启
- systemd 严格白名单，拒绝任意服务名和任意 shell 命令
- 一键生成系统、进程、网络和近期日志诊断包
- 诊断包保存在权限受控的 Agent 临时目录，最多保留 5 个
- gRPC Server Streaming 以 64 KiB chunk 下载，不整体载入内存
- Diagnostics 页面显示下载进度、保存路径和错误
- 日志 Server Streaming、关键字过滤、rotation 检测和主动停止
- 可选 AddressSanitizer / UndefinedBehaviorSanitizer 构建

## 技术栈

- C++17
- CMake 3.16+
- Protobuf
- gRPC
- Qt6 Widgets

Ubuntu 上的 Qt 开发依赖：

```bash
sudo apt-get install qt6-base-dev
```

Agent 日志依赖 spdlog。CMake 优先使用系统安装；缺失时拉取固定的
v1.14.1。也可以安装系统包：

```bash
sudo apt-get install libspdlog-dev
```

## 构建

```bash
cmake -S . -B build
cmake --build build -j
```

## 运行

先启动 Agent：

```bash
./build/agent/edgescope-agent
```

使用配置文件启动：

```bash
cp edgescope.conf.example edgescope.conf
./build/agent/edgescope-agent --config ./edgescope.conf
```

默认值仍然监听 `127.0.0.1:50051`，日志写入
`/tmp/edgescope-agent.log`。只有配置在 `allowed_services` 中的
`.service` 才会出现在 Services 页面并允许操作；实际控制权限仍由
Linux/systemd（例如 polkit 或 root 权限）决定。

再在另一个终端启动 Qt Client：

```bash
./build/client/edgescope-client
```

Agent 默认只监听 `127.0.0.1:50051`。Client 会请求一次当前指标，
并定时刷新 Overview 和 Processes 页面。Network 页面使用手动刷新，
Logs 页面只允许读取 Agent 预先定义的日志 ID，不接受任意文件路径。

原命令行 Client 保留为 RPC smoke test：

```bash
./build/client/edgescope-cli
```

CLI 也可以指定其他 Agent 地址：

```bash
./build/client/edgescope-cli --target 127.0.0.1:50052
```

通过 CLI 创建并下载诊断包：

```bash
./build/client/edgescope-cli --diagnostic ./edgescope-diagnostic.tar.gz
```

只有显式传入 PID 和固定操作时才会进行进程控制：

```bash
./build/client/edgescope-cli --control PID term
./build/client/edgescope-cli --control PID stop
./build/client/edgescope-cli --control PID continue
./build/client/edgescope-cli --control PID kill
```

## 测试

```bash
cmake -S . -B build -DEDGESCOPE_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

如果系统未安装 GoogleTest，开启测试时 CMake 会将固定版本下载到
build 目录，不会向源码仓库写入第三方产物。

Sanitizer 构建：

```bash
cmake -S . -B build-sanitize \
  -DEDGESCOPE_BUILD_TESTS=ON \
  -DEDGESCOPE_ENABLE_ASAN=ON \
  -DEDGESCOPE_ENABLE_UBSAN=ON
cmake --build build-sanitize -j
ctest --test-dir build-sanitize --output-on-failure
```

测试目标在受 ptrace 管理的开发容器中会关闭 LeakSanitizer，因为 LSan
无法在该环境运行；ASan 的越界/释放后使用检测及 UBSan 仍然启用。

## 安全说明

EdgeScope 不提供执行任意命令、脚本或读取任意文件的 RPC。进程操作只
接受固定信号，日志和 systemd 服务都经过 Agent 白名单，诊断包也只
包含预定义信息。默认只监听回环地址；当前 v1.0 未实现 TLS，不应直接
暴露到不可信网络。

## 项目文档

- [架构和线程模型](docs/architecture.md)
- [面试讲解与演示流程](docs/interview-guide.md)
