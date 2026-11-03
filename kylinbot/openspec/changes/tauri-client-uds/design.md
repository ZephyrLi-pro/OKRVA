## 背景 (Context)

`apps/tauri`（crate 名 `zeroclaw-desktop`）是 ZeroClaw 的桌面壳：一个 Tauri 2.0 应用，用 Rust 后端 + React 前端把系统托盘、自动配对、健康监控以及面向 gateway 的若干 HTTP 命令包成一个可分发的桌面 GUI。它通过 `apps/tauri/src/gateway_client.rs` 中基于 `reqwest` 的 HTTP 客户端在 `http://127.0.0.1:42617` 上调用 gateway，并把同一个 URL 作为 `tauri.conf.json` 的 `frontendDist`，让 WebView 直接从 gateway 拉取 React 前端资产；CSP 也据此放行 `http://127.0.0.1:*` / `ws://127.0.0.1:*`。

`default-gateway-to-uds` 变更已经在 Linux 上把 gateway 的默认传输切到 UDS（`$XDG_RUNTIME_DIR/zeroclaw/gateway.sock`，`0600`），TCP 仅在显式 `transport = "tcp"` 时启用，且 gateway crate 通过 `compile_error!` 限定为仅 Linux 构建。该变更的 proposal/design 明文写入"`apps/tauri` 不在更新范围内：将由独立 change 在本次 archive 之前整体移除"。

产品方向已经反转：`apps/tauri` 不再被移除，且 macOS 同样首选 UDS——Tauri 桌面客户端在 Linux 与 macOS 上均默认通过 UDS 与本地 gateway 通信。macOS 上的 gateway 可通过 Homebrew / 独立安装获得（或由独立 change 在未来解除 gateway crate 的 Linux-only 限制），桌面客户端不应预设"macOS 上没有本地 gateway"。

直接的难点是：UDS 是文件系统接口，浏览器 WebView 内部的 `fetch()` / `WebSocket` 不能直接访问。Tauri 2.0 提供 `tauri::Builder::register_uri_scheme_protocol` 可以注册自定义 URI 协议，把 WebView 发起的 `zeroclaw://gateway/...` 请求路由到 Rust 端处理；这是把 WebView 接入 UDS 的标准受支持方式。

利益相关方与影响面：
- `apps/tauri`（`zeroclaw-desktop`，Experimental，不在 `AGENTS.md` 工作区 crate 表里——按 Experimental 处理）。
- `crates/zeroclaw-gateway` —— 服务端不变，但本次变更需要它已实现 `default-gateway-to-uds` 的全部要求（同一发布周期）。
- `openspec/changes/default-gateway-to-uds/` —— 文本中"`apps/tauri` 将被移除"的措辞需要在本次同步修订。
- macOS 桌面用户 —— macOS 首选 UDS（与 Linux 一致），macOS 专属代码（`set_dock_icon`、`objc2*` 依赖）保留不动。macOS 上的 socket 路径回退逻辑与 Linux 不同（无 `XDG_RUNTIME_DIR` 时回退到 `~/Library/Application Support/zeroclaw/gateway.sock`）。
- `.github/workflows/*` —— macOS job 保留；Windows job 删除或 `compile_error!` 阻断。
- 用户文档 `docs/setup-guides/uds-default.md`、`README.md`、`CHANGELOG-next.md`。

## 目标 / 非目标 (Goals / Non-Goals)

**目标 (Goals):**
- Linux 与 macOS 桌面客户端均默认通过 UDS 与 gateway 通信，套接字路径解析按平台适配；用户在不动任何配置的前提下，升级到 UDS 默认 gateway 的同一发布即可继续使用桌面客户端。
- WebView 中既有的 React 前端无需重写为 Tauri `invoke` 命令即可走 UDS：通过自定义 URI 协议代理把 `fetch` / `WebSocket` 透明转译到 UDS。
- 保留 `transport = "tcp"` 作为显式回退（远程 gateway / 隧道部署），客户端代码路径与 UDS 路径对等，不引入"伪默认"。
- 在同一发布周期内推出，确保不存在"gateway 已切 UDS 但 desktop 未适配"的中间版本组合。

**非目标 (Non-Goals):**
- Windows 桌面构建的重新启用——本次通过 `compile_error!` 仅阻断 Windows；macOS 与 Linux 均为受支持平台。
- Tauri 移动端（`apps/tauri/gen/{android,apple}/`）的 UDS 适配——保持现状，不在范围。
- 重写 React 前端为纯 Tauri `invoke` 命令（"完全 IPC 化"是另一条迁移路径，本次不做）。
- 自定义 URI 协议下的 HTTP/2 / HTTP/3 / TLS——gateway 在 UDS 路径上仅运行 HTTP/1.1（`default-gateway-to-uds` 决策 4 已禁 TLS），桥接侧也只实现 HTTP/1.1。
- 给 WebView 提供"绕过协议代理直接拨 UDS"的额外通道——浏览器无原生 UDS 能力，自定义协议是唯一方案。
- 重做 Tauri 的 token 注入策略 (`localStorage.setItem('zeroclaw_token', ...)`)——本次保持既有形态，仅切换底层传输。
- 移除 `reqwest` 依赖——保留作为 `transport = "tcp"` 的实现兜底，避免本次变更同时做"切 UDS"和"换 HTTP 客户端栈"两件事。

## 决策 (Decisions)

### 决策 1：通过自定义 URI 协议 `zeroclaw://gateway/<path>` 把 WebView 桥接到 UDS

- **选择**：用 `tauri::Builder::register_uri_scheme_protocol("zeroclaw", handler)` 注册自定义 URI 协议；`zeroclaw://gateway/<path>` 形式的 WebView 请求由 Rust 端 handler 接管，handler 用本变更新建的 UDS HTTP 客户端把请求逐字节转发给 gateway，并把响应（含 status、headers、body）以原状回填给 WebView。
- **理由**：
  - 这是 Tauri 2.0 官方支持的 WebView ↔ Rust 桥接机制，所有主流 WebView 引擎（WebKit on Linux、WebView2 已不在范围）都能识别。
  - 对 React 前端的改动最小：只把所有 `http://127.0.0.1:42617` 替换成 `zeroclaw://gateway`，CSP 配套放行新 scheme，业务逻辑零改动。
  - WebView 的 cookie / fetch / `XMLHttpRequest` 行为按"同源"对待 `zeroclaw://gateway`，已有的 `Authorization: Bearer ...` 头部、JSON 序列化、错误处理都直接复用。
- **考虑过的替代方案**：
  - *把所有 fetch 改成 Tauri `invoke`*：拒绝——前端表面要重写，且 `invoke` 不天然支持流式响应、SSE、文件下载这些已存在的 HTTP 语义；本次想做"切传输"而不是"换交互模型"。
  - *Tauri 内部跑一个 127.0.0.1:RANDOM_PORT 的反向代理*：拒绝——重新引入了一个 TCP 监听器，与 UDS 加固目标直接冲突；任何同主机非特权用户都可能扫到该端口。
  - *把前端打包后嵌入 Tauri 二进制（不再从 gateway 拉取）*：可与决策 1 正交——两者都做能进一步减少对 gateway 的启动依赖，但 frontendDist 重新打包是更大的工程，本次变更先只做 URI 协议代理，前端资产仍由 gateway 经协议代理供给。

### 决策 2：UDS HTTP 客户端用 `hyper` + `hyper-util` + `tokio::net::UnixStream` 自实现，不引入 `hyperlocal`

- **选择**：在 `apps/tauri/src/gateway_client.rs` 中实现一个 ~150 行的 `UdsHttpClient`，每次请求开一个 `tokio::net::UnixStream::connect(path)`，用 `hyper::client::conn::http1::handshake` 做单连接 HTTP/1.1 客户端；保留 `keep-alive` 池为 `Option<Vec<UnixStream>>`，初始仅短连接（`Connection: close`）。请求/响应类型用 `http::Request<bytes::Bytes>` / `http::Response<bytes::Bytes>`，便于和自定义 URI 协议 handler 共享同一结构。
- **理由**：
  - 已经在 workspace 间接依赖 `hyper` / `hyper-util` / `tokio`（gateway crate 用），不引入新顶层依赖。
  - `hyperlocal` crate 长期处于个人维护、版本节奏不稳；它做的事我们 ~150 行就能写完，且与项目其它部分（gateway crate 的 UDS listener 适配器）在依赖树上一致。
  - 桌面客户端的请求量（健康轮询 + 配对 + 偶发命令）不会成为短连接的瓶颈；后续若要加 keep-alive 池，扩展面是单文件。
- **考虑过的替代方案**：
  - *给 `reqwest::Client::builder()` 装一个 UDS 连接器（reqwest 0.12 `connect`）*：拒绝——需要把 reqwest 的 connector trait 接到 hyper-util，绕路且对 reqwest 私有 API 偶发依赖。
  - *引入 `hyperlocal` crate*：拒绝——上一条理由，外加我们的需求其实只覆盖 hyperlocal 的一小部分。
  - *自实现 HTTP/1.1 解析*：拒绝——重复造轮子，且容易出 framing bug。

### 决策 3：状态字段从单一 `gateway_url` 拆为 `transport` + `socket_path` + `tcp_url`

- **选择**：`AppState` 结构体改造为：
  ```rust
  pub enum DesktopTransport {
      Uds { socket_path: PathBuf },
      Tcp { base_url: String },
  }

  pub struct AppState {
      pub transport: DesktopTransport,
      pub token: Option<String>,
      pub connected: bool,
      pub agent_status: AgentStatus,
  }
  ```
  `AppState::default()` 在所有 Unix 平台统一返回 `DesktopTransport::Uds { socket_path: default_socket_path() }`，`default_socket_path()` 内部按平台分支解析路径（见决策 4）。
  保留 `pub fn display_endpoint(&self) -> String`，UDS 形态返回 `unix://<socket_path>`、TCP 形态返回 `base_url`，仅用于日志与 UI。
- **理由**：跟 gateway 的 `Transport` 枚举一一对齐，让"客户端如何拨号"的语义在编译期被穷尽；不再让 `String` 形态的 `gateway_url` 同时承载 URL 与文件路径两种角色，避免 `auto_pair` / `health` / 命令各自再 parse 一次。macOS 与 Linux 共享 UDS 首选路径，简化代码——不存在"按平台选默认 transport"的分支。
- **考虑过的替代方案**：
  - *macOS 默认 TCP，Linux 默认 UDS*：拒绝——用户明确要求 macOS 首选 UDS；且 macOS 上同样可以运行本地 gateway（未来解除 gateway crate 的 Linux-only 限制后更自然）。
  - *保留 `gateway_url: String` 但允许 `unix:///path` 前缀*：拒绝——把"是 URL 还是路径"的判断分散到所有调用方，且 `unix://` 不是 IETF URI scheme，多个库的解析行为不一致。
  - *只换底层 client，状态字段不动*：拒绝——`AppState::default()` 的硬编码 `http://127.0.0.1:42617` 会在第一帧就被读到，必须同步迁移。

### 决策 4：默认 `socket_path` 按平台解析，与 gateway 逻辑对齐

- **选择**：`apps/tauri/src/gateway_client.rs`（或新文件 `apps/tauri/src/transport.rs`）中实现 `default_socket_path() -> PathBuf`，用 `#[cfg(unix)]` 守门，内部按平台分支：
  - **Linux**：先读 `XDG_RUNTIME_DIR`，非空则 `<XDG>/zeroclaw/gateway.sock`，否则按 `current_exe()` 父级或 `std::env::current_dir()` 推算 `${workspace_dir}/run/gateway.sock`——与 gateway 的 `default_gateway_socket_path()` 逐字节一致。
  - **macOS**：先读 `XDG_RUNTIME_DIR`（Homebrew / systemd 用户可能设置），非空则 `<XDG>/zeroclaw/gateway.sock`；否则回退到 `~/Library/Application Support/zeroclaw/gateway.sock`（macOS 标准按用户数据目录；`~/Library/Application Support/` 由 `dirs::data_dir()` 或等价逻辑得到，权限 `0700` 由 OS 保证）。
  启动时按 INFO 级别记录最终路径与解析来源。**不**直接依赖 `crates/zeroclaw-gateway` 来共享这段代码。
- **理由**：
  - `zeroclaw-desktop` 不在 workspace 的 `[dependencies]` 路径上引用 `zeroclaw-gateway`（gateway 是 binary crate；`apps/` 与 `crates/` 之间的依赖方向当前是 apps → crates，但本次不想为 12 行字符串拼接拉一个 100k 行依赖）。
  - Linux 路径与 gateway 完全对齐，用户零配置可用。
  - macOS 没有 `XDG_RUNTIME_DIR` 的默认值（除非用户手动设置），回退到 `~/Library/Application Support/` 是 Apple 平台的标准数据目录，权限模型可靠。
  - 解析逻辑只有十几行，单元测试覆盖各种 env 形态足够稳定。
- **考虑过的替代方案**：
  - *macOS 也回退到 `${workspace_dir}/run/gateway.sock`*：拒绝——`run/` 不是 macOS 惯用目录，且 `current_dir()` 在 macOS .app bundle 中指向 `/`，不可写。
  - *把解析函数提到 `crates/zeroclaw-config` 里共享*：可接受但超出本次范围；列入 Open Questions 跟踪是否拆库。
  - *硬编码默认路径，不读 XDG*：拒绝——与 gateway 默认值不对齐，用户必须手动配置，违背"零配置"目标。

### 决策 5：自定义 URI 协议 handler 是同步翻译器，不做缓存 / 不做鉴权

- **选择**：`zeroclaw://gateway/<path>` 的 handler 代码路径上**只做**：解析 `path` + query → 构造 `http::Request` → 调用 `UdsHttpClient::send` → 返回 `http::Response` 给 Tauri。**不做**：响应缓存、headers 改写（除 `Host: localhost`，`Authorization` 由前端原样带过来）、404 兜底页、错误页美化。
- **理由**：
  - 把代理逻辑保持透明，避免 WebView 看到的语义与 Rust 后端命令调用的语义出现分叉。
  - 鉴权完全沿用既有方案：前端从 `localStorage.getItem('zeroclaw_token')` 拿 token，写到 `Authorization` 头；Rust 端的 `auto_pair` / `inject_token_into_webview` 链路保持原样，只是底层拨号变 UDS。
  - 不做缓存可以避免一类难以排查的"状态在两个地方"问题（HTTP 缓存 vs Tauri 协议缓存）。
- **考虑过的替代方案**：
  - *handler 内部追加 `Authorization`*：拒绝——把鉴权秘密集中在 Rust 端"听起来更安全"，但绕过了已经有用户测试覆盖的前端 token 流；且重启时机不同会引入空窗。
  - *handler 内部做 `_app/*` 静态资源缓存*：拒绝——gateway 端已有 `Cache-Control` header 经验，本次变更不重写它。

### 决策 6：WebSocket 走"自定义协议 + WS 升级"，必要时回退 Tauri events

- **选择**：首选实现路径——`zeroclaw://gateway/ws/...` 的 WebSocket 升级请求由自定义 URI 协议 handler 识别（`Upgrade: websocket`），Rust 端用 `tokio-tungstenite` 与 gateway 在 UDS 上完成 WS 握手，然后双向 pump 帧。如果 Tauri 当前版本（2.x）的 `register_uri_scheme_protocol` 在 Linux WebKitGTK 上无法承载 WS 升级（实测中存在报告），回退方案：前端改用 `@tauri-apps/api/event` 通道，配合一个 `invoke('ws_open', { path })` 命令打开 Rust 端的 WS 桥，`emit("ws_msg", ...)` 双向转发帧。
- **理由**：
  - 桌面客户端目前直接发起 WS 的端点不多（实时事件、agent 输出流），fallback 方案的代码量是有界的。
  - 把"协议层 WS"放在首选位置可以零前端改动；但显式声明 fallback 路径让我们不被 WebView 实现细节阻塞。
- **考虑过的替代方案**：
  - *只用 fallback*：拒绝——前端要为 WS 维护两套代码（开发模式 vs 生产模式），与"切传输不改前端"目标背离。
  - *只用首选*：拒绝——若 Linux WebKitGTK 在自定义协议上的 WS 升级行为异常，开发期不能交付。
- **决策守则**：实施时第一步是用一个最小 reproducer 在目标 WebKitGTK 版本上验证 WS-over-custom-scheme 的可用性；结果决定哪条路径成为 PR 主体。本决策结果会回填到本节，不另开 change。

### 决策 7：macOS 保留支持、UDS 代码用 `#[cfg(unix)]` 守门、Windows 编译期阻断

- **选择**：在 `apps/tauri/src/lib.rs` 顶部加 `#[cfg(target_os = "windows")] compile_error!("zeroclaw-desktop is not supported on Windows; supported platforms: Linux, macOS")`；**保留** `set_dock_icon` 函数及 `objc2*` 三个 macOS 依赖；**保留** `apps/tauri/Cargo.toml` 的 `[target.'cfg(target_os = "macos")'.dependencies]` 块。UDS 相关代码（`UdsClient`、`default_socket_path`、自定义 URI 协议注册）用 `#[cfg(unix)]` 守门，Linux 与 macOS 共享同一条 UDS 代码路径。
- **理由**：
  - macOS 首选 UDS，必须编译 UDS 客户端代码——用 `#[cfg(unix)]` 让两个平台共享同一条代码路径，避免 `target_os = "linux"` 导致 macOS 上 UDS 代码不编译。
  - `tokio::net::UnixStream` / `UnixListener` 在 macOS 上同样可用（都是 Unix），无编译障碍。
  - `set_dock_icon` / `objc2*` 在 macOS 构建中是活跃代码，不是死代码。
  - Windows 不在支持范围：编译期阻断比运行期错误更早暴露。
- **考虑过的替代方案**：
  - *UDS 代码用 `#[cfg(target_os = "linux")]` 守门，macOS 上不编译*：拒绝——macOS 首选 UDS，必须在 macOS 上编译 UDS 客户端。
  - *所有平台都编译 UDS 代码，运行期按平台选择*：拒绝——`tokio::net::UnixStream` 在 Windows 上不存在，编译会失败。
  - *所有平台都 Linux-only，删除 macOS 支持*：拒绝——用户明确要求 macOS 保留且首选 UDS。

### 决策 8：CSP 统一替换 origin 段，Linux 与 macOS 均用 `zeroclaw:`

- **选择**：`apps/tauri/tauri.conf.json` 的 `app.security.csp` 中，把所有 `http://127.0.0.1:*` 与 `ws://127.0.0.1:*` 来源逐条替换为 `zeroclaw:`（注意：URI scheme 在 CSP 里不带 `//`，写法为 `zeroclaw:`）；保留 `'self'`、`'unsafe-inline'`、`data:` 不动。Linux 与 macOS 共享同一份 CSP——因为两个平台都注册了 `zeroclaw:` 自定义 URI 协议、都默认走 UDS。
- **理由**：两个平台共享同一套 WebView → UDS 桥接路径，CSP 可以统一替换，不需要条件化。
- **考虑过的替代方案**：
  - *顺手收紧 `'unsafe-inline'` / 加 nonce*：拒绝——是独立的安全加固方向，单独提案。
  - *允许 `zeroclaw://*` 通配*：CSP 中 scheme 来源默认覆盖该 scheme 全部 origin，无需也不应再加通配。

### 决策 9：`reqwest` 路径仅服务于显式 `transport = "tcp"`，且与 UDS 路径走同一接口 trait

- **选择**：在 `apps/tauri/src/gateway_client.rs` 中定义 `pub trait GatewayHttp { async fn request(&self, req: http::Request<Bytes>) -> Result<http::Response<Bytes>>; }`；提供两份实现 `UdsClient`（默认）与 `TcpClient`（基于 reqwest）。`GatewayClient::new(transport: &DesktopTransport, token: ...)` 按 transport 选择实现并装箱。所有调用点（`get_status` / `get_health` / `auto_pair` / 等）只看 trait 方法，不再直接持有 reqwest 客户端。
- **理由**：
  - trait 化让两条路径在 handler 层完全等价，便于把"切回 TCP"做成单元测试可覆盖的开关。
  - 保留 reqwest 让"远端隧道 gateway"场景在未来能直接用，不引入第二次重写。
- **考虑过的替代方案**：
  - *直接砍掉 reqwest*：拒绝——本次范围只切默认值；远端 TCP 场景在 `default-gateway-to-uds` 中是被允许的运行模式。
  - *枚举替代 trait*：可接受，但 trait + 装箱在 mock 测试中更易构造假实现。

### 决策 10：与 `default-gateway-to-uds` 协调通过文本编辑而非依赖关系

- **选择**：本变更落 PR 时同步把 `openspec/changes/default-gateway-to-uds/{proposal.md,design.md,tasks.md}` 中所有"`apps/tauri` 不在范围 / 将由独立 change 移除"的措辞改为"`apps/tauri` 由 `tauri-client-uds` 适配 UDS"。两个 change 不通过 OpenSpec 的 `dependsOn` 关系硬绑定，但发布耦合在 changelog 与 archive 顺序中体现：`tauri-client-uds` 必须在 `default-gateway-to-uds` 之前 archive，或两者同发布合并 archive。
- **理由**：
  - OpenSpec 当前没有跨 change 文本同步机制；档案前的最后一次校验是人工的。
  - 不引入循环依赖（`tauri-client-uds` 行为依赖 `default-gateway-to-uds` 已实现的 gateway UDS——这是事实的实现序，不是 OpenSpec 工件序）。
- **考虑过的替代方案**：
  - *直接 archive `default-gateway-to-uds` 中的 `apps/tauri` 段落让其失效*：拒绝——archive 是 spec 演化的事件，不是文本删除；正确做法是在源 change 里改文本，避免后人读 archive 时困惑。

## 风险 / 取舍 (Risks / Trade-offs)

- **风险：自定义 URI 协议在 Linux WebKitGTK 上对 WebSocket 升级支持不完整** → 缓解：决策 6 已规定 fallback 路径（Tauri events + invoke）；实施时把"目标 WebKitGTK 版本上的 WS-over-custom-scheme 可用性 reproducer"列为 P0 任务，结果在 PR 描述中给出，并把决策 6 的最终路径回填本文档。
- **风险：自动配对（`auto_pair`）在 UDS 下若 socket 还未就绪会快速失败循环** → 缓解：保留现有"健康轮询会重试"行为，把 `auto_pair` 内部的 `unwrap_or(false)` 容错路径在 UDS 形态下保持不变；首启时若 UDS 文件不存在直接返回 None，由 `health::spawn_health_poller` 在 5s 间隔继续探测；增加一条 INFO 日志说明等待原因。
- **风险：用户在 `gateway.transport = "tcp"` 的远端形态下使用桌面客户端，但 `AppState` 默认仍是 UDS** → 缓解：在 `tauri-plugin-store` 持久化 `transport` 用户偏好；首次安装无偏好时按 UDS 默认；用户通过设置面板（已存在）显式切到 TCP 后写回 store。提供一条托盘菜单项 "Connection settings…" 进入设置页（如未来面板缺这部分，由后续 change 补齐——本次至少把字段持久化通路打通）。
- **风险：CSP 中 `zeroclaw:` scheme 在某些 WebKitGTK 版本上被当成"unknown scheme"过滤** → 缓解：实施前用最小 React 页面（`<img src="zeroclaw://gateway/..."/>` + `fetch("zeroclaw://gateway/health")`）跑一次目标 WebKitGTK；如果命中已知 webkit bug，调整 scheme 到已知工作的形态（如 `zclaw`）并同步更新 CSP 与代码常量。
- **风险：`hyper-util` 主要面向服务端 / 长连接场景，作为短连接客户端可能多消耗一点握手成本** → 接受：桌面客户端的请求量级远低于性能门槛；后续若证实瓶颈，单文件加 keep-alive 池（决策 2 已埋好抽象）。
- **风险：macOS 上 `XDG_RUNTIME_DIR` 通常未设置，回退到 `~/Library/Application Support/` 可能与用户预期不符** → 缓解：启动 INFO 日志记录实际路径与解析来源；文档中说明 macOS 默认 socket 位置；`gateway.socket_path` 可显式覆盖。
- **风险：macOS 上自定义 URI 协议在 WKWebView 上的 WebSocket 升级行为可能与 Linux WebKitGTK 不同** → 缓解：WS 桥接的 reproducer（任务 1.x）同时在 Linux WebKitGTK 和 macOS WKWebView 上验证；如果任一平台不支持，fallback 路径（Tauri events + invoke）覆盖两个平台。
- **风险：macOS 上 gateway crate 当前通过 `compile_error!` 阻断 macOS 编译，桌面客户端 UDS 默认值在"无本地 gateway"时自动配对会静默失败** → 缓解：与 `default-gateway-to-uds` 同周期推出；`auto_pair` 在 socket 不存在时返回 `None` 不崩溃，健康轮询会重试；如果未来解除 gateway 的 Linux-only 限制，桌面端无需改动。
- **风险：`tauri.conf.json` 的 `frontendDist` 改成 `zeroclaw://gateway/_app/` 后，Tauri devtools / 开发者工具的 source map 路径解析可能崩** → 缓解：开发模式（`devUrl`）保留 `http://127.0.0.1:42617/_app/` 作为可选项，由 `tauri.conf.json` 的环境覆盖配置；生产模式始终走 `zeroclaw:`。把这一条作为开发者文档中的一节。
- **风险：socket 路径回退到 `${workspace_dir}/run/gateway.sock` 时，桌面 app 的 `${workspace_dir}` 解析与 gateway 二进制不一致** → 缓解：决策 4 中的 `default_socket_path()` 用 `std::env::current_dir()` 作为最后兜底（与 gateway 一致），并打 INFO 日志暴露选用结果；在文档示例中要求用户显式设置 `XDG_RUNTIME_DIR`，避免依赖 `${workspace_dir}` 隐式解析。
- **取舍：保留 reqwest 依赖**（决策 9） → 接受：本次变更只切默认值，不做 HTTP 客户端栈大重构；`reqwest` 在 TCP 路径下继续提供成熟的 redirect / cookie / TLS 行为。

## 迁移计划 (Migration Plan)

1. **状态与默认值迁移**：先动 `apps/tauri/src/state.rs` 与 `apps/tauri/src/gateway_client.rs`：引入 `DesktopTransport` 枚举、`default_socket_path()`、`GatewayHttp` trait 与 `UdsClient` / `TcpClient` 两份实现。所有调用点切到 trait。本步骤通过单元测试（mock UDS server）即可验证。
2. **自定义 URI 协议注册**：在 `apps/tauri/src/lib.rs` 的 `tauri::Builder` 调用链上注册 `zeroclaw` scheme handler，handler 内部复用步骤 1 的 `UdsClient`；先只支持 GET/POST 的 HTTP/1.1 请求，覆盖 `/_app/`、`/health`、`/api/*`、`/admin/*`、`/pair`、`/webhook`。
3. **WS 桥接首选路径**：扩展 handler 识别 `Upgrade: websocket`；用 `tokio-tungstenite` 在 UDS 上完成升级；把帧 pump 通过 Tauri 协议层回传给 WebView。
4. **WS 桥接 fallback**：若步骤 3 实测在目标 WebKitGTK 上不可用，按决策 6 fallback 路径实现 `invoke('ws_open')` + 事件 pump，并提供前端 helper 包（`apps/tauri/web` 或 React 侧的 `lib/ws.ts`）。
5. **配置 / 资产 / CSP**：更新 `apps/tauri/tauri.conf.json`：`devUrl` / `frontendDist` 切到 `zeroclaw://gateway/_app/`；CSP 替换 origin 来源；`apps/tauri/capabilities/*.json` 检查是否需要新增 protocol 权限。
6. **跨平台守门**：在 `apps/tauri/src/lib.rs` 顶部加 `#[cfg(target_os = "windows")] compile_error!`；UDS 代码用 `#[cfg(unix)]` 守门（Linux + macOS 共享）；保留 macOS 专属代码与依赖；调整 `.github/workflows/*` 中 desktop job（macOS 保留、Windows 删除或阻断）。
7. **跨变更文本同步**：编辑 `openspec/changes/default-gateway-to-uds/{proposal.md,design.md,tasks.md}`，把"`apps/tauri` 将被移除"的段落改为"由 `tauri-client-uds` 适配"。
8. **测试**：单元（UDS HTTP 客户端、URI handler 路由、WS 桥接帧 pump）、集成（启 `zeroclaw-gateway` 二进制、桌面客户端 `auto_pair` + `/health` 往返）、smoke（`cargo tauri build` 在 Linux runner 上能产出可启动的二进制）。
9. **文档与发布**：`docs/setup-guides/uds-default.md` 新增 Tauri 节、`README.md` 桌面 quickstart 同步更新、`CHANGELOG-next.md` 加 BREAKING 条目。
10. **回滚**：本变更若需要回滚，可保留 trait 抽象但把 `AppState::default()` 切回 TCP + `frontendDist` 切回 `http://127.0.0.1:42617/_app/`；自定义 URI 协议 handler 不注册即等于不存在。CSP 与平台收敛是耦合的，回滚需要同步还原。

## 待解决问题 (Open Questions)

- **WS-over-自定义-scheme 在 Linux WebKitGTK 与 macOS WKWebView 上的可用性**：实施第一步需要在两个平台上分别给出实测结论，决定决策 6 走首选路径还是 fallback；结论回填本文档对应章节。两个平台的 WebView 引擎不同（WebKitGTK vs WKWebView），需要分别验证。
- **是否把 `default_socket_path()` 提到 `crates/zeroclaw-config` 共享**：当前在桌面与 gateway 各实现一份；未来若再加第二个客户端（CLI、TUI 已有自己的 IPC 路径），可考虑统一到 config crate。本次先保留两份独立实现。
- **Tauri 设置面板（"Connection settings…"）是否在本次范围内**：风险段已说明用 `tauri-plugin-store` 把 `transport` 持久化即可；UI 入口若未实现，可由后续 change 补齐，不阻塞本次发布。
