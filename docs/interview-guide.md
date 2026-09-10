# EdgeScope Interview Guide

## Two-minute introduction

EdgeScope is a C++17 remote Linux diagnostics tool with a Linux Agent and a Qt6
desktop Client. It collects system, process and network data from kernel
interfaces, exposes typed gRPC APIs, performs controlled process/service actions,
streams logs, and downloads diagnostic bundles without becoming a remote shell.

## Engineering decisions to explain

1. CPU percentage cannot come from one `/proc/stat` read because the fields are
   cumulative counters. EdgeScope keeps snapshots and computes deltas.
2. One `MetricsSampler` prevents every Client from maintaining an independent
   CPU baseline and protects the latest snapshot with a mutex/condition variable.
3. `/proc` is inherently racy: a PID can disappear during scanning. One failed
   process is skipped rather than failing the entire list.
4. Qt's GUI thread must not block on synchronous gRPC. A QThread owns unary work,
   while a cancelable joinable thread handles the long-lived log stream.
5. Security is capability-based: fixed enums, PID validation, log/service
   whitelists, controlled diagnostic contents, and no arbitrary shell RPC.
6. Large data is bounded: logs use reverse block reads; diagnostics use streaming
   chunks and verify offsets and total size.

## Demo sequence

1. Start the Agent with `edgescope.conf.example`.
2. Connect the GUI and show live Overview values.
3. Search a process, inspect it, and explain confirmation around signals.
4. Show interfaces and TCP inode-to-PID mapping.
5. Tail a log, start streaming, generate a new Agent event, then stop streaming.
6. Show the service whitelist and status (avoid changing important services).
7. Create a diagnostic bundle and inspect its fixed files.
8. Stop the Agent and show that the Client reports a timeout/refusal without
   freezing or crashing.

## Honest limitations

- Transport is insecure gRPC and loopback-only by default; TLS is a future item.
- Process CPU is currently a lifetime average, not a short-interval delta.
- Socket-to-PID mapping is best effort and depends on permissions and timing.
- systemd actions depend on host polkit/root policy.
- Bundle metadata is in memory and bundles are removed when the Agent exits.
