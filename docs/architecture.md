# EdgeScope v1.0 Architecture

## Purpose

EdgeScope lets an engineer diagnose a Linux device without manually running a
collection of SSH commands. The Agent owns all Linux-specific access. The Qt
Client only communicates through typed gRPC APIs.

## Data flow

```text
/proc, /sys, POSIX APIs, systemd, allowed logs
                    |
     Collectors / Controllers / Sampler
                    |
          EdgeScopeServiceImpl
                    |
             Protobuf + gRPC
                    |
          EdgeScopeClient / RpcWorker
                    |
              Qt Widgets UI
```

`EdgeScopeServiceImpl` translates between plain C++ domain structures and
protobuf messages. Linux parsing remains outside the RPC layer.

## Agent modules

- `SystemCollector`: one Linux system snapshot; CPU uses `/proc/stat` deltas.
- `MetricsSampler`: one RAII sampling thread and a mutex-protected latest value.
- `ProcessCollector`: resilient `/proc/[pid]` scan and process details.
- `ProcessController`: only TERM, KILL, STOP and CONT signals.
- `NetworkCollector`: interfaces, counters, TCP tables and best-effort PID map.
- `LogCollector`: whitelist lookup, bounded tail reads and UTF-8 sanitization.
- `ServiceManager`: exact service whitelist and fixed `systemctl` argv execution.
- `DiagnosticCollector`: controlled snapshot, tar.gz creation and bundle registry.
- `AgentConfig` / `AgentLogger`: validated settings and rotating spdlog output.

## Concurrency

The Agent has one metrics sampling thread; gRPC owns request execution threads.
The GUI main thread never performs synchronous RPC. Unary calls run in the
`RpcWorker` QThread. Log streaming uses one additional joinable thread so Stop
and Disconnect slots remain responsive. Closing the window cancels streaming,
joins it, stops the QThread and then destroys the channel.

## Security boundaries

- The default listener is loopback only. Remote listening requires config.
- There is no shell, script or arbitrary-command RPC.
- Process control accepts a PID and one fixed action.
- Logs and systemd services are resolved from Agent-side whitelists.
- Diagnostic contents and Agent paths are fixed; the Client receives only an ID.
- Diagnostic downloads are 64 KiB chunks with offset and size validation.
- TLS is intentionally left for a later release; do not expose an insecure Agent
  to an untrusted network.

## Failure behavior

Unary RPCs have deadlines. Missing processes and permission failures map to
specific gRPC status codes. Log rotation is detected by inode/size changes.
Partial diagnostic downloads are deleted. SIGINT/SIGTERM causes graceful Agent
shutdown and log flushing.
