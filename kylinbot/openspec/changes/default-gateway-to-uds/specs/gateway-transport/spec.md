## ADDED Requirements

### Requirement: 默认使用 UDS 传输 (Default UDS transport)
当 `gateway.transport` 未设置时，gateway SHALL 默认采用 Unix 域套接字 (UDS) 传输，绑定到 `gateway.socket_path` 而非 TCP `host:port`。本规范仅适用于 Linux 目标平台。

#### Scenario: 全新安装默认使用 UDS
- **WHEN** 运维在 Linux 上运行 `kylinbot gateway` 且未设置 `gateway.transport`
- **THEN** gateway MUST 在解析后的 `socket_path` 处绑定一个 Unix 套接字
- **AND** MUST NOT 打开任何 TCP 监听器
- **AND** MUST 在启动时记录套接字的绝对路径（如 `🦀 KylinBot Gateway listening on unix:///run/user/1000/kylinbot/gateway.sock`）

#### Scenario: 默认套接字路径解析
- **WHEN** `gateway.socket_path` 未设置
- **THEN** gateway MUST 按以下顺序解析路径：`$XDG_RUNTIME_DIR/kylinbot/gateway.sock`，`XDG_RUNTIME_DIR` 未设置或为空时回退到 `${workspace_dir}/run/gateway.sock`
- **AND** 如果父目录不存在，MUST 以 `0700` 模式创建
- **AND** MUST 在启动时按 INFO 级别记录最终选用的路径与解析来源（XDG 或 workspace 回退）

### Requirement: 可配置的传输层选择 (Configurable transport selection)
gateway SHALL 接受 `gateway.transport` 取值 `"uds"` 或 `"tcp"`，并 SHALL 在配置加载阶段拒绝任何其他值。

#### Scenario: 显式选择 TCP
- **WHEN** 运维设置 `gateway.transport = "tcp"`，并提供 `gateway.host` / `gateway.port`
- **THEN** gateway MUST 在配置的 `host:port` 上绑定 TCP 监听器
- **AND** MUST NOT 创建任何 Unix 套接字
- **AND** 既有的 TCP-only 行为（TLS、mTLS、公网绑定告警、隧道集成）MUST 仍可使用

#### Scenario: 非法 transport 取值
- **WHEN** 加载到 `gateway.transport = "named-pipe"`（或任何不支持的取值）
- **THEN** 配置校验 MUST 失败，错误信息要点出非法值并列出可接受的取值

### Requirement: TLS 与 UDS 互斥 (TLS and UDS are mutually exclusive)
当 `gateway.transport = "uds"` 时，如果同时启用了任意 TLS 配置（`gateway.tls.enabled = true` 或 `client_auth.enabled = true`），gateway SHALL 拒绝启动，并 SHALL 提供可操作的错误信息。

#### Scenario: 启动时拒绝 UDS + TLS
- **WHEN** 运维同时设置 `gateway.transport = "uds"` 与 `gateway.tls.enabled = true`
- **THEN** gateway MUST 启动失败，错误信息为 `TLS cannot be combined with transport = "uds"; either set transport = "tcp" or remove the [gateway.tls] block`
- **AND** MUST NOT 绑定任何套接字
- **AND** 进程退出码 MUST 非零

### Requirement: 套接字生命周期与权限 (Socket lifecycle and permissions)
gateway SHALL 保证 Unix 套接字文件仅文件属主可读写、每次启动重建、并在优雅停机时移除。

#### Scenario: 残留套接字清理
- **WHEN** gateway 启动时 `socket_path` 处已存在上次运行留下的文件
- **THEN** gateway MUST 在 bind 之前尝试 `unlink(socket_path)`
- **AND** 如果 unlink 失败且原因不是 "not found"，启动 MUST 失败并给出明确错误

#### Scenario: 套接字权限为 0600
- **WHEN** gateway 成功绑定 Unix 套接字
- **THEN** `socket_path` 的文件权限模式 MUST 严格为 `0600`（属主可读写、组无、其它无）
- **AND** 如果当前平台的 `umask` 会带来更宽松的权限，gateway MUST 在 bind 之后将文件 `chmod` 为 `0600`

#### Scenario: 优雅停机时移除套接字
- **WHEN** gateway 收到优雅停机信号并完成连接 drain
- **THEN** gateway MUST 在退出前从文件系统中移除套接字文件
- **AND** 如果进程被强杀（SIGKILL），下次启动 MUST 清理残留文件（由"残留套接字清理"覆盖）

### Requirement: HTTP、SSE 与 WebSocket 在 UDS 上对等 (HTTP, SSE, and WebSocket parity over UDS)
gateway SHALL 在 UDS 上以与 TCP 路径相同的请求语义提供所有 HTTP/1.1 路由、Server-Sent Events 流和 WebSocket 升级。

#### Scenario: 通过 UDS 发起 HTTP 请求
- **WHEN** 客户端打开 Unix 套接字并发送 `GET /health HTTP/1.1`
- **THEN** gateway MUST 返回与 TCP 路径相同的 JSON body
- **AND** axum 中间件（timeout、body limit、CORS、rate limit）MUST 同样生效

#### Scenario: 通过 UDS 升级 WebSocket
- **WHEN** 客户端连接到 Unix 套接字并对一个注册了 WS 的路由发起 WebSocket 升级
- **THEN** 升级 MUST 成功
- **AND** 双向帧 MUST 持续流动直到客户端或服务端关闭
- **AND** ping/pong、close 码、帧大小限制 MUST 与 TCP 路径行为一致

#### Scenario: 通过 UDS 推送 SSE
- **WHEN** 客户端通过 Unix 套接字打开 SSE 端点
- **THEN** gateway MUST 以 `Content-Type: text/event-stream` 推送事件
- **AND** 断连检测 MUST 在与 TCP 路径相同的时间界限内触发每连接状态的清理

### Requirement: UDS 上的对端身份识别 (Peer identification on UDS)
在 UDS 上，gateway SHALL 用基于 `SO_PEERCRED`（对端 UID/PID）派生的合成对端键替代 `SocketAddr`，并在限流、配对检查与访问日志中使用。

#### Scenario: 限流器使用对端键
- **WHEN** UDS 客户端发起请求且限流器记录之
- **THEN** 限流键 MUST 为 `uds:<peer-uid>`（不是 `127.0.0.1` 或任何 TCP 地址）
- **AND** 限流窗口与淘汰策略 MUST 与 TCP 路径行为一致

#### Scenario: 访问日志包含对端身份
- **WHEN** gateway 记录一条通过 UDS 接收的请求日志
- **THEN** 访问日志条目 MUST 包含对端 UID 与 PID（在可获取时）
- **AND** MUST NOT 记录 TCP 地址

### Requirement: CLI 与环境变量界面 (CLI and environment-variable surface)
gateway CLI 与环境变量 SHALL 暴露 `--socket-path` / `KYLINBOT_GATEWAY_SOCKET` 用于 UDS，并 SHALL 在 `transport = "uds"` 时忽略 `--host` / `--port` / `KYLINBOT_GATEWAY_HOST` / `KYLINBOT_GATEWAY_PORT` / `HOST` / `PORT`，同时给出告警。

#### Scenario: --socket-path 覆盖配置
- **WHEN** 运维运行 `kylinbot gateway --socket-path /tmp/zc.sock`
- **THEN** gateway MUST 绑定 `/tmp/zc.sock`，无视 `gateway.socket_path` 的配置值

#### Scenario: UDS 下忽略 TCP 参数并发出告警
- **WHEN** 运维在 `gateway.transport = "uds"` 时运行 `kylinbot gateway --host 0.0.0.0 --port 8080`
- **THEN** gateway MUST 输出一条 WARN 级别日志，说明在 UDS transport 下 `--host`/`--port` 被忽略
- **AND** MUST 继续按好像未提供这些参数的方式绑定 Unix 套接字

### Requirement: 平台支持范围 (Platform support scope)
本能力 SHALL 仅在 Linux 目标平台上支持；其他平台（macOS、Windows、BSD 等）的支持不在范围内。

#### Scenario: 仅在 Linux 构建 gateway crate
- **WHEN** 在非 Linux 目标（如 macOS / Windows）上尝试编译 `kylinbot-gateway` crate
- **THEN** 构建系统 MUST 通过 Cargo `cfg(target_os = "linux")` 约束阻止编译，或在 `lib.rs` 顶部以 `compile_error!` 给出明确信息
- **AND** 错误信息 MUST 说明 gateway 仅支持 Linux，并指向相关文档

### Requirement: 隧道与远程访问兼容性 (Tunnel and remote-access compatibility)
对任何隧道服务（Cloudflare Tunnel、ngrok 等）以及任何远程访问配对流程，gateway SHALL 要求 `transport = "tcp"`；如果在 UDS 下配置了隧道，SHALL 以可操作的错误信息启动失败。

#### Scenario: UDS 下使用隧道被拒绝
- **WHEN** `tunnel.provider != "none"` 且 `gateway.transport = "uds"`
- **THEN** gateway MUST 启动失败，错误信息要求运维设置 `gateway.transport = "tcp"` 才能使用隧道
