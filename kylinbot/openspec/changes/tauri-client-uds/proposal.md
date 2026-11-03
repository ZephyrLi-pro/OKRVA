## 为什么 (Why)

`default-gateway-to-uds` 变更将 `zeroclaw-gateway` 在 Linux 上的默认传输切换为 Unix 域套接字（UDS），并明确把 `apps/tauri`（`zeroclaw-desktop`）排除在客户端适配清单之外——理由是该桌面客户端原计划在 archive 之前由独立 change 整体移除。产品方向已经反转：`apps/tauri` 不再会被移除，而需要在同一发布周期内跟上 gateway 的新默认传输。当前 Tauri 客户端的 Rust 后端硬编码 `http://127.0.0.1:42617`、WebView 通过 `frontendDist: "http://127.0.0.1:42617/_app/"` 拉取前端、CSP 仅放行 `http://127.0.0.1:*` / `ws://127.0.0.1:*`——只要 gateway 切到 UDS 默认值，Tauri 桌面就在 Linux 上彻底无法启动。

## 变更内容 (What Changes)

- 在 `apps/tauri/src/state.rs` 中以 `transport`（`uds`/`tcp`）+ `socket_path` + `gateway_url` 三元组替换裸的 `gateway_url: String`，默认 `transport = "uds"`，`socket_path` 解析顺序与 gateway 对齐：`$XDG_RUNTIME_DIR/zeroclaw/gateway.sock` → `${workspace_dir}/run/gateway.sock`。
- 在 `apps/tauri/src/gateway_client.rs` 中以基于 `hyper` + `hyper-util` + `tokio::net::UnixStream` 的客户端取代 `reqwest::Client` 的默认 TCP 连接器；保留 `reqwest` 仅用于显式 `transport = "tcp"` 场景（远程 gateway / 隧道兜底）。WebView 不直接读 UDS——所有桌面后端的 HTTP 调用都走 UDS 客户端。
- 注册一个 Tauri 自定义 URI 协议 `zeroclaw://gateway/...`（Tauri 2.0 `register_uri_scheme_protocol`），由 Rust 处理器把 WebView 的 HTTP 请求逐字节转译为 UDS 上对 gateway 的 HTTP/1.1 请求并把响应以原状转回 WebView；不解析正文，不修改头部（除补齐 `Authorization` 注入逻辑沿用既有 token 机制）。
- 增加 WebSocket 桥接：WebView 仍使用 `new WebSocket("zeroclaw://gateway/ws/...")` 形式发起连接，由 Rust 端通过 `tokio-tungstenite` 与 UDS 上的 gateway 建立 WS，并以双向通道（Tauri command + event 流，或 Tauri 2.0 的协议级 WS 升级——见 design.md）将帧透传。
- 更新 `apps/tauri/tauri.conf.json`：`build.devUrl` 与 `build.frontendDist` 从 `http://127.0.0.1:42617/_app/` 改为 `zeroclaw://gateway/_app/`；`app.security.csp` 中以 `zeroclaw:` 替换 `http://127.0.0.1:*` / `ws://127.0.0.1:*` 来源（保留 `data:` 与 `'self'`、`'unsafe-inline'` 既有放行不动）。
- 更新 `apps/tauri/src/lib.rs::auto_pair`、`apps/tauri/src/health.rs` 的健康轮询、`apps/tauri/src/commands/{agent,channels,gateway,pairing}.rs`：所有 `GatewayClient::new(&s.gateway_url, ...)` 改为通过新的 transport 感知工厂构建客户端。
- **跨平台 transport 默认值**：Linux 与 macOS 均默认 `transport = "uds"`，socket_path 解析顺序按平台适配（Linux: `$XDG_RUNTIME_DIR/zeroclaw/gateway.sock` → `${workspace_dir}/run/gateway.sock`；macOS: `$XDG_RUNTIME_DIR/zeroclaw/gateway.sock` → `~/Library/Application Support/zeroclaw/gateway.sock`）。`AppState::default()` 在所有 Unix 平台返回 UDS；`transport = "tcp"` 仅作为显式回退（远程 gateway / 隧道）。UDS 客户端代码用 `#[cfg(unix)]` 守门，Linux 与 macOS 共享。Windows 不在支持范围内（`compile_error!` 阻断）。macOS 专属代码（`set_dock_icon`、`objc2*` 依赖）保留不变。
- **BREAKING**（Experimental crate `zeroclaw-desktop`，按规则允许）：`AppState::default()` 不再返回 `gateway_url = "http://127.0.0.1:42617"`；改为 `transport = uds, socket_path = <平台默认值>`。
- 测试：新增 `gateway_client` UDS 单元测试（启一个 `tokio::net::UnixListener` 上的 mock HTTP server）、自定义 URI 协议处理器单元测试、以及一个能在 CI 上跑的 UDS 端到端 smoke（spawn `zeroclaw-gateway` 二进制，桌面客户端拨号 `/health`）。
- 文档：在 `docs/setup-guides/uds-default.md` 增加 Tauri 桌面专属一节；`CHANGELOG-next.md` 追加 BREAKING 条目（默认状态字段、平台收敛、自定义 URI 协议）。
- **与 `default-gateway-to-uds` 的协调**：本变更必须与该变更在同一发布周期内推出（`zeroclaw-desktop` 与 `zeroclaw-gateway` 同步发布）。同时更新 `default-gateway-to-uds/proposal.md` 与 `design.md` 中"`apps/tauri` 将被独立 change 移除"的措辞，改为"`apps/tauri` 由 `tauri-client-uds` 适配 UDS"，避免 archive 时两份文档相互矛盾。

## 能力 (Capabilities)

### 新增能力 (New Capabilities)
- `tauri-gateway-client`：Tauri 桌面客户端到 gateway 的传输层（Linux 与 macOS 均默认 UDS、TCP 显式回退）、WebView ↔ UDS 桥接（自定义 URI 协议、WebSocket 桥接）、跨平台 socket_path 解析、状态字段语义、自动配对在 UDS 下的行为。

### 修改能力 (Modified Capabilities)
<!-- `gateway-transport` spec 仅描述 gateway 服务端行为；客户端是新表面，单独建 spec。-->

## 影响范围 (Impact)

- **代码**：`apps/tauri/src/gateway_client.rs`（HTTP 客户端核心重写）、`apps/tauri/src/state.rs`（状态字段与默认值）、`apps/tauri/src/lib.rs`（自定义 URI 协议注册、`auto_pair`）、`apps/tauri/src/health.rs`（健康轮询拨号方式）、`apps/tauri/src/commands/{agent,channels,gateway,pairing}.rs`（client 构造点）、`apps/tauri/Cargo.toml`（新增 `hyper`/`hyper-util`/`tokio-tungstenite` 等 `#[cfg(unix)]` 依赖）、`apps/tauri/tauri.conf.json`（`devUrl` / `frontendDist` / CSP）。
- **跨变更协调**：`openspec/changes/default-gateway-to-uds/{proposal.md,design.md,tasks.md}` 中关于"`apps/tauri` 将被移除"的段落需要在本变更同步更新（小规模文本编辑，不引入新行为）。
- **CI / 构建**：`.github/workflows/*` 中 macOS 构建 job 保留（macOS 走 UDS 路径，与 Linux 一致）；Windows 构建 job 删除或 `compile_error!` 阻断。
- **测试**：`apps/tauri/tests/` 新增 UDS 客户端单元 + smoke；`apps/tauri/src/gateway_client.rs` 内联 `#[cfg(test)] mod tests` 覆盖 UDS happy path / 错误路径。
- **文档**：`docs/setup-guides/uds-default.md`（新增 Tauri 节）、`README.md`（如有桌面客户端 quickstart 片段）、`CHANGELOG-next.md`。
- **稳定性等级**：`zeroclaw-desktop` 不在 `AGENTS.md` 列出的 workspace crate stability 表中（属于 `apps/`，不是 `crates/`）；按 Experimental 等级处理，BREAKING 默认值切换允许，需 changelog 条目。
- **不在范围内**：
  - Windows 桌面客户端的重新启用（macOS 保留支持；Windows 仍被 `compile_error!` 阻断）。
  - 移除已不再使用的 reqwest 依赖（保留作为 `transport = "tcp"` 与远端 gateway 通信的兜底，避免本次变更引入双重运动）。
  - 重写前端 React 应用以直接使用 Tauri `invoke` 命令（本次仅做 URI 协议代理；`invoke` 化是更大的、独立的演进方向）。
  - WS-over-自定义-protocol 的浏览器规范支持（如某 WebView 实现不支持自定义协议的 WebSocket 升级，本变更接受降级到通过 Tauri events 转发的 fallback——具体策略见 design.md 决策）。
  - Tauri Android / iOS 移动端（`apps/tauri/gen/{android,apple}/` 现存）——保持现状，不在本次范围；如未来重新启用应通过独立 change。
