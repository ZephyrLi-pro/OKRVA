## ADDED Requirements

### Requirement: 桌面客户端默认通过 UDS 拨号 (Desktop client defaults to UDS transport)
当 Tauri 桌面客户端（`zeroclaw-desktop`）启动时未持久化用户偏好的 transport，客户端 SHALL 默认使用 Unix 域套接字（UDS）与 gateway 通信，并 SHALL 把套接字路径解析顺序按平台适配。本要求适用于所有 Unix 目标平台（Linux 与 macOS）。

#### Scenario: 全新启动默认走 UDS
- **WHEN** 用户在 Linux 或 macOS 上首次启动 `zeroclaw-desktop` 且 `tauri-plugin-store` 中没有 `transport` 偏好
- **THEN** 客户端 MUST 把 `AppState::transport` 初始化为 `DesktopTransport::Uds { socket_path: <解析结果> }`
- **AND** MUST NOT 创建任何 `reqwest::Client` 形态的 TCP 客户端（UDS 场景下）
- **AND** MUST 在启动日志中按 INFO 级别记录 `transport=uds`、最终选用的 socket_path 以及解析来源

#### Scenario: 默认 socket 路径解析（Linux）
- **WHEN** 客户端在 Linux 上解析默认 socket 路径
- **THEN** 客户端 MUST 按以下优先级解析：(1) `$XDG_RUNTIME_DIR/zeroclaw/gateway.sock`（当 `XDG_RUNTIME_DIR` 已设置且非空时）；(2) `${workspace_dir}/run/gateway.sock`（当 `XDG_RUNTIME_DIR` 缺失或为空时）
- **AND** MUST 与 gateway 默认值在以下三种环境下逐字节相同：`XDG_RUNTIME_DIR=/run/user/1000`、`XDG_RUNTIME_DIR=""`、`unset XDG_RUNTIME_DIR`

#### Scenario: 默认 socket 路径解析（macOS）
- **WHEN** 客户端在 macOS 上解析默认 socket 路径
- **THEN** 客户端 MUST 按以下优先级解析：(1) `$XDG_RUNTIME_DIR/zeroclaw/gateway.sock`（当 `XDG_RUNTIME_DIR` 已设置且非空时）；(2) `~/Library/Application Support/zeroclaw/gateway.sock`（当 `XDG_RUNTIME_DIR` 缺失或为空时）

#### Scenario: 用户偏好的 transport 持久化覆盖默认值
- **WHEN** `tauri-plugin-store` 中已存在 `{"transport": "tcp", "tcp_url": "https://example.com"}` 偏好
- **THEN** 客户端 MUST 把 `AppState::transport` 初始化为 `DesktopTransport::Tcp { base_url: "https://example.com" }`
- **AND** MUST NOT 创建 UDS 客户端

### Requirement: WebView 通过自定义 URI 协议代理访问 UDS gateway (WebView reaches UDS gateway via custom URI scheme)
WebView 中的 React 前端 SHALL 通过 `zeroclaw://gateway/<path>` 形式的自定义 URI 协议访问 gateway 资产与 API；Rust 后端 SHALL 注册该协议的 handler，并以透明转译的方式把请求转发到 UDS 上的 gateway。

#### Scenario: HTTP GET 透明代理
- **WHEN** WebView 发起 `GET zeroclaw://gateway/health` 请求且本地 gateway 已就绪
- **THEN** Rust handler MUST 用 UDS HTTP 客户端向 gateway 发出 `GET /health HTTP/1.1`
- **AND** MUST 把 gateway 返回的 status code、所有 response headers、response body 原样写回 WebView
- **AND** MUST NOT 改写 `Authorization`、`Cookie`、`Content-Type` 等头部
- **AND** MUST NOT 缓存响应

#### Scenario: HTTP POST 携带 Authorization
- **WHEN** WebView 发起 `POST zeroclaw://gateway/api/agent/message`，请求头含 `Authorization: Bearer zc_xxx`
- **THEN** Rust handler MUST 把该 header 原样转发到 UDS 上的 `POST /api/agent/message`
- **AND** MUST 把请求 body 原样转发，不做 JSON 重新序列化

#### Scenario: gateway 不可达时的失败语义
- **WHEN** WebView 发起 `GET zeroclaw://gateway/health` 但 socket 路径不存在或 connect 失败
- **THEN** Rust handler MUST 返回一个合法的 HTTP 错误响应（例如 502 Bad Gateway）给 WebView
- **AND** MUST 在 Rust 端按 WARN 级别日志记录拨号失败与 socket 路径
- **AND** MUST NOT 让 Tauri 进程崩溃

### Requirement: WebSocket 桥接通过自定义 URI 协议或 Tauri events fallback (WebSocket bridging via custom scheme or events fallback)
WebView 发起 `zeroclaw://gateway/<ws-path>` 的 WebSocket 升级 SHALL 由 Rust 后端桥接到 UDS 上的 gateway。Rust 后端 SHALL 实现以下任一路径：(1) 在自定义 URI 协议 handler 中识别 `Upgrade: websocket` 并完成 UDS WS 握手；(2) 当目标 WebKitGTK 不支持自定义 scheme 上的 WS 升级时，提供 Tauri command + event 形态的 fallback 桥。

#### Scenario: WS 帧双向转发
- **WHEN** WebView 通过 `new WebSocket("zeroclaw://gateway/ws/events")` 建立连接，且 Rust 端已与 UDS 上的 gateway 完成 WS 握手
- **THEN** WebView 发出的每一帧 MUST 被原样转发到 gateway
- **AND** gateway 推送的每一帧 MUST 被原样转发到 WebView
- **AND** ping/pong、close 码、帧大小限制 MUST 与同一 gateway 的 TCP 路径行为一致

#### Scenario: WS 关闭语义
- **WHEN** WebView 端或 gateway 端任一侧关闭连接
- **THEN** Rust 桥接 MUST 把 close 码与原因透传给另一侧
- **AND** MUST 释放与该连接关联的 `tokio::net::UnixStream`，不留资源泄漏

### Requirement: 客户端 transport 抽象支持 UDS 与 TCP 两条对等路径 (Client transport abstraction supports parity between UDS and TCP)
`apps/tauri/src/gateway_client.rs` 中的所有 gateway 调用点 SHALL 通过 `GatewayHttp` trait 进行；trait SHALL 提供 UDS 与 TCP 两份实现，调用方 MUST NOT 直接持有 `reqwest::Client` 或裸 `tokio::net::UnixStream`。

#### Scenario: UDS 实现处理 happy path
- **WHEN** 调用方持有 `Box<dyn GatewayHttp>` 实际类型为 `UdsClient`，并发起对 `/api/status` 的请求
- **THEN** 客户端 MUST 通过 `tokio::net::UnixStream::connect(socket_path)` 打开连接
- **AND** MUST 用 `hyper::client::conn::http1::handshake` 完成 HTTP/1.1 握手
- **AND** MUST 返回 `http::Response<Bytes>`，调用方解析 body 与 headers 的代码 MUST 与 TCP 实现完全一致

#### Scenario: TCP 实现处理远端 gateway
- **WHEN** 调用方持有 `Box<dyn GatewayHttp>` 实际类型为 `TcpClient`，且 `base_url = "https://example.com"`
- **THEN** 客户端 MUST 通过 `reqwest::Client` 发起请求
- **AND** MUST 透传 `Authorization` 头部
- **AND** MUST 返回与 UDS 实现等价的 `http::Response<Bytes>` 形态

### Requirement: 自动配对在 UDS 形态下保持既有触发逻辑 (Auto-pair behaves identically under UDS)
`apps/tauri/src/lib.rs::auto_pair` 在 UDS transport 形态下 SHALL 沿用与 TCP 形态相同的触发条件、token 持久化逻辑与 WebView 注入路径，仅底层拨号方式不同。

#### Scenario: gateway 启用 pairing 时自动获取 token
- **WHEN** 客户端在 UDS 上调用 `requires_pairing()` 返回 `true` 且 `AppState::token` 为 `None`
- **THEN** 客户端 MUST 调用 `auto_pair()` 通过 UDS 请求新 paircode 并交换 token
- **AND** MUST 把 token 写入 `AppState::token` 与 `tauri-plugin-store`
- **AND** MUST 调用 `inject_token_into_webview` 把 token 写到 WebView 的 `localStorage.zeroclaw_token`

#### Scenario: gateway 暂未就绪不抛异常
- **WHEN** 客户端在 UDS 上调用 `auto_pair()` 但 socket 文件不存在
- **THEN** `auto_pair()` MUST 返回 `None`
- **AND** MUST 不让 Tauri 启动流程失败
- **AND** `health::spawn_health_poller` MUST 继续以既有间隔重试

### Requirement: CSP 与 frontendDist 配置匹配自定义 URI 协议 (CSP and frontendDist match the custom URI scheme)
`apps/tauri/tauri.conf.json` 的 `app.security.csp` SHALL 把 `http://127.0.0.1:*` / `ws://127.0.0.1:*` 替换为 `zeroclaw:` 来源；`build.frontendDist` SHALL 指向 `zeroclaw://gateway/_app/`。

#### Scenario: CSP 来源段替换
- **WHEN** Tauri 启动并加载 WebView
- **THEN** `default-src` / `connect-src` / `script-src` / `style-src` / `img-src` 各指令 MUST 包含 `zeroclaw:` 作为来源
- **AND** MUST NOT 包含 `http://127.0.0.1:*` 或 `ws://127.0.0.1:*`

#### Scenario: frontendDist 指向自定义协议
- **WHEN** Tauri 解析 `tauri.conf.json` 的 `build.frontendDist`
- **THEN** 值 MUST 为 `zeroclaw://gateway/_app/`
- **AND** WebView 启动时 MUST 通过自定义 URI 协议 handler 拉取 React 资产

### Requirement: 桌面 crate 支持 Linux 与 macOS，编译期阻断 Windows (Desktop crate supports Linux and macOS, blocks Windows at compile time)
`zeroclaw-desktop` crate SHALL 在 Linux 与 macOS 目标平台上支持构建；Windows 不在支持范围内，并 SHALL 在编译期阻断。UDS 相关代码 SHALL 在所有 Unix 平台上编译（`#[cfg(unix)]` 守门）。

#### Scenario: Windows 目标编译失败
- **WHEN** 在 Windows target 上尝试 `cargo build -p zeroclaw-desktop`
- **THEN** 构建系统 MUST 通过 `#[cfg(target_os = "windows")] compile_error!` 阻止编译
- **AND** 错误信息 MUST 说明 `zeroclaw-desktop is not supported on Windows`，并列出受支持平台（Linux, macOS）

#### Scenario: macOS 构建成功且包含 UDS 代码
- **WHEN** 在 macOS target 上运行 `cargo build -p zeroclaw-desktop`
- **THEN** 构建 MUST 成功
- **AND** macOS-specific 依赖（`objc2`、`objc2-app-kit`、`objc2-foundation`）MUST 被编译与链接
- **AND** UDS 客户端代码（`UdsClient`、`default_socket_path`、自定义 URI 协议）MUST 包含在 macOS 构建中（`#[cfg(unix)]` 守门，macOS 是 Unix 平台）

#### Scenario: UDS 代码在 Unix 平台编译
- **WHEN** 审计 `apps/tauri/src/` 中的 UDS 相关代码
- **THEN** 所有 UDS 客户端代码 MUST 用 `#[cfg(unix)]` 守门
- **AND** Linux 与 macOS 构建中 MUST 包含对 `tokio::net::UnixStream` / `UnixListener` / `default_socket_path` 的引用

### Requirement: 用户可通过设置持久化切换到 TCP 形态 (Users can persist a TCP transport preference)
`zeroclaw-desktop` SHALL 支持把用户选择的 `transport` + `tcp_url` 写入 `tauri-plugin-store`，并 SHALL 在下次启动时优先采用持久化值。

#### Scenario: 持久化 TCP 偏好
- **WHEN** 用户通过设置面板选择 `transport = tcp`、`tcp_url = "https://gateway.example.com"`
- **THEN** 客户端 MUST 把该偏好写入 `tauri-plugin-store`
- **AND** 下一次启动 MUST 跳过 UDS 默认初始化，直接使用持久化的 TCP 配置

#### Scenario: 清除偏好回到 UDS 默认值
- **WHEN** 用户从设置面板清除持久化偏好
- **THEN** 下一次启动 MUST 把 `transport` 重新初始化为 UDS 默认值（按"默认 socket 路径解析"逻辑）
