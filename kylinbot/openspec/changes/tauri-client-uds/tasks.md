## 1. WebView WS-over-自定义-scheme 可行性 reproducer（P0）

- [ ] 1.1 在 Linux WebKitGTK（`apt show libwebkit2gtk-4.1-dev` 取版本）上构建一个最小 Tauri 2.0 demo：注册 `zeroclaw://gateway/` 协议，handler 内部建立到本地 echo WS server 的 `tokio-tungstenite` 桥；前端 `new WebSocket("zeroclaw://gateway/echo")`，发送 1 帧、断言收到 echo。
- [ ] 1.2 在 macOS WKWebView 上重复 1.1 的验证（Tauri 2.0 在 macOS 上使用 WKWebView 作为 WebView 后端，对自定义 scheme 的支持与 Linux WebKitGTK 不同）。把两个平台的结果分别记录在 `openspec/changes/tauri-client-uds/design.md` 决策 6 后追加 "Validation result"：(a) WS 升级是否在自定义 scheme 上工作；(b) 若不工作，报错信息与 WebView 引擎版本号。
- [ ] 1.3 根据 1.2 的结论选定 PR 主体路径：协议层 WS（首选）或 Tauri events fallback；后续任务按选定路径展开。

## 2. transport 抽象与状态字段

- [x] 2.1 在 `apps/tauri/src/state.rs` 中新增 `pub enum DesktopTransport { Uds { socket_path: PathBuf }, Tcp { base_url: String } }`，使用 `serde(rename_all = "lowercase", tag = "transport", content = "endpoint")`。
- [x] 2.2 修改 `pub struct AppState`：删除 `gateway_url: String`，新增 `transport: DesktopTransport`。
- [x] 2.3 实现 `AppState::display_endpoint(&self) -> String`：UDS 形态返回 `format!("unix://{}", socket_path.display())`，TCP 形态返回 `base_url.clone()`。日志与 UI 的字符串输出全部走该方法。
- [x] 2.4 修改 `AppState::default()`：所有 Unix 平台统一返回 `DesktopTransport::Uds { socket_path: default_socket_path() }`（`#[cfg(unix)]` 守门）；`default_socket_path()` 内部按平台分支解析路径（Linux: XDG → workspace，macOS: XDG → ~/Library/Application Support）。
- [x] 2.5 更新 `apps/tauri/src/state.rs` 的内联 `#[cfg(test)] mod tests`：删除 `default_state` 中对 `gateway_url` 的断言，改为断言 `transport` 是 UDS 且 `socket_path` 非空（`#[cfg(unix)]` 守门）。

## 3. UDS HTTP 客户端

- [x] 3.1 在 `apps/tauri/src/gateway_client.rs`（或新文件 `apps/tauri/src/transport.rs`）中实现 `pub fn default_socket_path() -> PathBuf`，用 `#[cfg(unix)]` 守门。内部按平台分支：Linux → 先读 `XDG_RUNTIME_DIR`（非空）→ `<XDG>/zeroclaw/gateway.sock`，否则 `<workspace_dir>/run/gateway.sock`（用 `std::env::current_dir()` 兜底）；macOS → 先读 `XDG_RUNTIME_DIR`（非空），否则回退到 `~/Library/Application Support/zeroclaw/gateway.sock`（用 `dirs::data_dir()` 或等价逻辑）。
- [x] 3.2 为 `default_socket_path` 写单元测试（`#[cfg(unix)]` 守门）：Linux 覆盖 `XDG_RUNTIME_DIR=/run/user/1000`、`XDG_RUNTIME_DIR=""`、`std::env::remove_var("XDG_RUNTIME_DIR")` 三种形态；macOS 覆盖无 `XDG_RUNTIME_DIR` 时回退到 `~/Library/Application Support/`。
- [x] 3.3 定义 `pub trait GatewayHttp: Send + Sync { async fn request(&self, req: http::Request<bytes::Bytes>) -> anyhow::Result<http::Response<bytes::Bytes>>; }`。
- [x] 3.4 实现 `pub struct UdsClient { socket_path: PathBuf }` 与 `impl GatewayHttp for UdsClient`（`#[cfg(unix)]` 守门）：每次请求 `tokio::net::UnixStream::connect(&self.socket_path)`，用 `hyper::client::conn::http1::handshake` 完成 HTTP/1.1 握手，发送请求，收响应到 `Bytes`。设 `Connection: close`。Linux 与 macOS 共享同一实现。
- [x] 3.5 实现 `pub struct TcpClient { client: reqwest::Client, base_url: String }` 与 `impl GatewayHttp for TcpClient`：把 `http::Request<Bytes>` 翻译到 `reqwest::Request`，再把响应翻回 `http::Response<Bytes>`。所有平台均编译。
- [x] 3.6 实现 `pub fn build_client(transport: &DesktopTransport, _token: Option<&str>) -> Box<dyn GatewayHttp>`（token 不再注入到 client，改由 handler 透传 `Authorization` header）。`DesktopTransport::Uds` 分支用 `#[cfg(unix)]` 守门；`DesktopTransport::Tcp` 分支在所有平台可用。
- [x] 3.7 重写 `pub struct GatewayClient { http: Box<dyn GatewayHttp>, token: Option<String>, endpoint_label: String }`：`new(transport: &DesktopTransport, token: Option<&str>)` 内部调用 `build_client`；`auth_header()` 不变；既有方法（`get_status` / `get_health` / `get_devices` / `requires_pairing` / `request_new_paircode` / `pair_with_code` / `validate_token` / `auto_pair` / `send_webhook_message`）改为构造 `http::Request`，调 `self.http.request(...)`，再 parse 响应。
- [x] 3.8 单元测试（`#[cfg(unix)]` 守门）：在 `apps/tauri/src/gateway_client.rs` 内联起一个 `tokio::net::UnixListener` mock server（返回固定 JSON），断言 `UdsClient::request` happy path、404、connect 失败三种路径都返回预期类型。Linux 与 macOS 均可跑。
- [x] 3.9 替换原文件中所有 `http://127.0.0.1:42617` / `http://127.0.0.1:1` / `http://localhost:8080` / `http://localhost` / `https://zeroclaw.example.com:9999` 测试用法：UDS 用例改为构造一个临时 socket（`#[cfg(unix)]` 守门）；TCP 用例改为构造 `TcpClient` 并指向无效端口；保留 reqwest 路径下的"无法连接"语义测试（所有平台可用）。
- [x] 3.10 在 `apps/tauri/Cargo.toml` 中把 `hyper`、`hyper-util`、`http`、`bytes`、`tokio-tungstenite` 依赖限定在 `[target.'cfg(unix)'.dependencies]`；`reqwest` 保留在通用 `[dependencies]`。

## 4. 调用点接线

- [x] 4.1 修改 `apps/tauri/src/lib.rs::auto_pair`：`url = state.read().await.gateway_url.clone()` 替换为 `transport = state.read().await.transport.clone()`；所有 `GatewayClient::new(&url, ...)` 调用改为 `GatewayClient::new(&transport, ...)`。
- [x] 4.2 修改 `apps/tauri/src/health.rs::spawn_health_poller`：把 `(s.gateway_url.clone(), s.token.clone())` 改为 `(s.transport.clone(), s.token.clone())`，构造 client 同上。
- [x] 4.3 修改 `apps/tauri/src/commands/gateway.rs`、`apps/tauri/src/commands/pairing.rs`、`apps/tauri/src/commands/agent.rs`、`apps/tauri/src/commands/channels.rs`：把 `GatewayClient::new(&s.gateway_url, s.token.as_deref())` 全部改为 `GatewayClient::new(&s.transport, s.token.as_deref())`。
- [x] 4.4 grep `apps/tauri/src/` 与 `apps/tauri/build.rs` 确认无任何 `gateway_url` 字段读写残留；如有，按相同模式修复。

## 5. 自定义 URI 协议代理

- [x] 5.1 在 `apps/tauri/src/lib.rs` 的 `tauri::Builder::default()...` 链上调用 `.register_uri_scheme_protocol("zeroclaw", handler)`，用 `#[cfg(unix)]` 守门——Linux 与 macOS 均注册。handler 闭包从 Tauri `AppHandle` 拿到 `SharedState`，再拿到 `transport`。
- [x] 5.2 实现 handler 主体：把 Tauri `http::Request` 解析出 `path` 与 `query`、复制 headers（除 `Host`）、拷贝 body；构造 `http::Request<Bytes>` 调用 `GatewayHttp::request`；把响应转回 Tauri 期望的 `http::Response<Vec<u8>>`。
- [ ] 5.3 在 handler 中识别 `Upgrade: websocket`：若决策 6 选定首选路径，分支到 `tokio-tungstenite` UDS 升级 + 帧 pump；若选定 fallback，让 handler 返回 `400 Upgrade Not Allowed`，并由 Tauri command + event 路径处理（见任务 6）。
- [ ] 5.4 单元测试 handler：mock 一个 `GatewayHttp` 假实现，断言 GET `/health`、POST `/api/agent/message` 带 `Authorization` header、`zeroclaw://gateway/_app/foo.js` 静态资源三种 case 的响应被原样转回。

## 6. WebSocket 桥接

- [ ] 6.1 [仅在协议层 WS 不可用时] 实现 `apps/tauri/src/commands/ws.rs`：`#[tauri::command] async fn ws_open(path: String) -> WsHandle`；内部用 `tokio-tungstenite::client_async` 在 UDS 上完成 WS 握手；返回一个句柄 ID。
- [ ] 6.2 [同上] 实现 `ws_send` 与 `ws_close` 命令；后端用 `app.emit("ws_msg", ...)` 把帧推回前端；提供 `apps/tauri/web/src/lib/uds-ws.ts`（或对应路径）封装成 `class UdsWebSocket` 暴露给 React。
- [ ] 6.3 [协议层 WS 路径] 在任务 5.3 的 handler 中，握手成功后启动两个 `tokio::spawn`：(a) WebView → UDS 帧 pump；(b) UDS → WebView 帧 pump；任一侧关闭时另一侧也关闭并释放 `UnixStream`。
- [ ] 6.4 单元测试：mock UDS WS server 回 echo，桥接 RT 一帧 round-trip 在 100ms 内完成。

## 7. 配置与资产

- [x] 7.1 修改 `apps/tauri/tauri.conf.json`：`build.devUrl` = `zeroclaw://gateway/_app/`、`build.frontendDist` = `zeroclaw://gateway/_app/`。Linux 与 macOS 共享同一配置（两个平台都注册了 `zeroclaw:` 协议、都默认走 UDS）。
- [x] 7.2 修改 `app.security.csp`：把 `http://127.0.0.1:*` 与 `ws://127.0.0.1:*` 来源整体替换为 `zeroclaw:`（注意 CSP 中 scheme 来源不带 `//`）。Linux 与 macOS 统一。结果各指令保留 `'self'`、`'unsafe-inline'`、`data:` 不变。
- [x] 7.3 审计 `apps/tauri/capabilities/*.json`：若需要为自定义协议加权限（Tauri 2.0 `core:protocol:allow-zeroclaw`），追加；不需要则在 PR 描述中说明。
- [x] 7.4 在前端代码中 grep `http://127.0.0.1:42617` / `ws://127.0.0.1:42617` / `127.0.0.1:42617`（覆盖 `web/`、`apps/tauri/` 与可能的 React 包），全部替换为 `zeroclaw://gateway`。

## 8. 跨平台守门

- [x] 8.1 在 `apps/tauri/src/lib.rs` 顶部加入 `#[cfg(target_os = "windows")] compile_error!("zeroclaw-desktop is not supported on Windows; supported platforms: Linux, macOS")`。
- [x] 8.2 保留 `apps/tauri/src/lib.rs::set_dock_icon` 函数及其 `#[cfg(target_os = "macos")]` 守门——macOS 是受支持平台。
- [x] 8.3 保留 `apps/tauri/Cargo.toml` 中 `[target.'cfg(target_os = "macos")'.dependencies]` 整段（含 `objc2`、`objc2-app-kit`、`objc2-foundation`）。
- [x] 8.4 确认所有 UDS 相关代码用 `#[cfg(unix)]` 守门：`UdsClient`、`default_socket_path`、`register_uri_scheme_protocol` 调用、WS 桥接代码——Linux 与 macOS 共享同一条 UDS 代码路径。
- [x] 8.5 调整 `.github/workflows/*` 中所有针对 `zeroclaw-desktop` / `apps/tauri` 的 job：macOS job 保留（走 UDS 路径，与 Linux 一致）；Windows job 删除或 `compile_error!` 已阻断；Linux job 走 UDS 路径。

## 9. 用户偏好持久化

- [x] 9.1 在 `apps/tauri/src/state.rs` 中加 `pub fn load_persisted_transport(store: &tauri_plugin_store::Store) -> Option<DesktopTransport>` 与 `pub fn save_persisted_transport(store: &tauri_plugin_store::Store, transport: &DesktopTransport)`。
- [x] 9.2 在 `apps/tauri/src/lib.rs::run` 的 `setup` 闭包中：早于 `auto_pair` 的位置读 store；若有持久化偏好则覆盖 `AppState::transport`，否则保留 `AppState::default()` 的 UDS 默认值。
- [x] 9.3 单元测试：mock store 含 `{"transport":"tcp","endpoint":"https://example.com"}` → 启动后 state 是 TCP；store 为空 → state 是 UDS 且 socket_path 非空。

## 10. 跨变更文本同步

- [x] 10.1 编辑 `openspec/changes/default-gateway-to-uds/proposal.md`：把"`apps/tauri` 桌面客户端将在本次变更 archive 之前由独立 change 移除"改为"`apps/tauri` 桌面客户端由 `tauri-client-uds` 同周期适配 UDS"。
- [x] 10.2 编辑 `openspec/changes/default-gateway-to-uds/design.md`：搜索"`apps/tauri`"出现的全部位置（含"利益相关方"、"风险"、"迁移计划第 4 步"、"发布耦合"），把"将由独立 change 移除"改为"由 `tauri-client-uds` 同周期适配"。
- [x] 10.3 编辑 `openspec/changes/default-gateway-to-uds/tasks.md`：6 节顶部注释中"将整体移除"语义改为"由 `tauri-client-uds` 适配"；保留 8.2 / 8.3 中 `--exclude zeroclaw-desktop` 的本机记录不动（这是历史事实，不修改）。
- [ ] 10.4 PR 描述中明确这两个 change 的 archive 顺序：`tauri-client-uds` 必须在 `default-gateway-to-uds` 之前或同 PR archive，避免后人读 archive 时困惑。

## 11. 集成测试

- [x] 11.1 新增 `apps/tauri/tests/uds_smoke.rs`：在测试 setup 中 `tokio::process::Command::new("cargo")` 启动 `zeroclaw-gateway`（指向临时 socket），等待 socket 出现，然后用 `UdsClient` 直接调 `/health` 断言 200。
- [ ] 11.2 [协议层 WS 选定时] 新增 `apps/tauri/tests/uds_ws_smoke.rs`：spawn gateway，再用 `tokio-tungstenite` 在 UDS 上对一个已知 WS 端点完成升级 + 1 帧 echo。
- [x] 11.3 在 `.github/workflows/*` 的桌面相关 job 中加上启 gateway 二进制的步骤；smoke 失败必须阻断 PR。

## 12. 文档与发布

- [x] 12.1 在 `docs/setup-guides/uds-default.md` 末尾追加 "Tauri 桌面客户端" 一节：说明默认 socket 路径、设置面板里如何切换到 TCP、自定义 URI 协议的存在与意图。
- [x] 12.2 更新 `README.md`：如有桌面 quickstart 片段，把 `http://127.0.0.1:42617` 占位改为 `unix:///run/user/<uid>/zeroclaw/gateway.sock` 形态，并用 `curl --unix-socket` 演示与 gateway 的最小交互。
- [x] 12.3 在 `CHANGELOG-next.md` 追加条目：(a) Linux 与 macOS 均默认 transport=uds、自定义 URI 协议 `zeroclaw:`；(b) macOS socket 路径回退到 `~/Library/Application Support/zeroclaw/gateway.sock`；(c) Windows 停止构建（`compile_error!`）；(d) `AppState::default()` 字段语义变化（默认 UDS）；(e) 引导用户如需 TCP 可在设置面板切回，或 `gateway.transport=tcp`。
- [x] 12.4 如 `apps/tauri` 自带的 README 或贡献文档存在，同步更新平台支持声明与拨号示例。

## 13. 验证关卡

- [x] 13.1 在 Linux 构建宿主上运行 `cargo fmt --all -- --check`。
- [x] 13.2 运行 `cargo clippy --workspace --all-targets -- -D warnings`，不允许新增 `#[allow]` 回退；`zeroclaw-desktop` 必须包含在 workspace 范围内（不再 exclude）。
- [x] 13.3 运行 `cargo test --workspace`，确认桌面端单元测试与 smoke 全绿。
- [ ] 13.4 在 Linux 桌面环境（含 WebKitGTK）手工跑 `cargo tauri dev`：(a) gateway 在 UDS 上启动后桌面 app 自动配对成功；(b) WebView 加载 `_app/` 资产；(c) 至少一个 WS 端点能收到事件；(d) 设置面板切到 TCP、重启后通过 reqwest 远端连接成功。
- [x] 13.5 在 macOS 上验证 `cargo build -p zeroclaw-desktop` 成功；`cargo test -p zeroclaw-desktop` 中 UDS 相关测试（`#[cfg(unix)]` 守门）通过。
- [ ] 13.6 在 macOS 上验证 `cargo tauri dev` 启动：gateway 在 UDS 上运行，桌面 app 通过 `zeroclaw://gateway/` 协议加载前端与 API。
- [ ] 13.7 在 Windows target 上验证 `cargo build -p zeroclaw-desktop` 失败信息含 `zeroclaw-desktop is not supported on Windows`。
