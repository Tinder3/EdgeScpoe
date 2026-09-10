# EdgeScope

EdgeScope 是一个面向 Linux 设备的远程诊断与系统监控项目。

## 当前版本

v0.6 已实现：

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
