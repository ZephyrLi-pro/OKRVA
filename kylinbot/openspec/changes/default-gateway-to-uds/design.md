## 背景 (Context)

`kylinbot-gateway` crate 是 agent 运行时的 HTTP/SSE/WebSocket 入口。当前它始终通过 `crates/kylinbot-gateway/src/lib.rs:436` 的 `tokio::net::TcpListener::bind(format!("{host}:{port}"))` 绑定 TCP 监听器，可选地用 `tokio_rustls::TlsAcceptor` 包裹每条连接，然后把连接送入类型为 `MakeService<SocketAddr, ...>` 的 axum `Router`。WebSocket 升级、SSE 流、限流器、配对守卫和访问日志全都假设使用 `SocketAddr` / 基于 IP 的对端识别。

默认工作负载是单用户、单主机的 agent：CLI、TUI、桌面 UI 以及 `kylinbot-tools` 调用的 shell/MCP 都在同一台机器上。对该场景而言，TCP 监听器明显过度设计且带来风险：

- 即便是 localhost 监听，仍会出现在 `ss -tlnp` 中，可能在共享主机上被其他用户劫持，并触发操作系统提示（防火墙、杀毒）。
- 每位运维都得自己判断 `is_public_bind`、端口冲突、是否启用 TLS。
- 现有的 `is_public_bind` 告警，本质上是给"过度宽松的默认值"打的补丁。

远程访问的需求是真实存在的，但只占少数（隧道服务商、`dist/` 云端示例）。这些场景在显式开启 `transport = "tcp"` 后仍可正常工作。

利益相关方与影响面：
- `crates/kylinbot-gateway`（Experimental 等级——允许有 changelog 的破坏性变更）。
- `crates/kylinbot-config`（Beta 等级——MINOR 版本中允许有说明的破坏性 schema 变更）。
- `crates/kylinbot-tui`、`install.sh`、`dist/aur` —— 一方 Linux 客户端与分发，需要默认走 UDS。
- `apps/tauri` —— 由 `tauri-client-uds` 同周期适配 UDS，因此**不在本次客户端更新范围内**。本次变更不写 `apps/tauri` 的 UDS 适配代码。
- `Dockerfile*`、`docker-compose.yml`、`dev/docker-compose*.yml`、`deploy-k8s/`、`dist/scoop`、`setup.bat` —— 容器化 / k8s / 非 Linux 形态，本次变更**不做 UDS 适配**，仅在对应文件中加注释或环境变量声明默认 `transport=tcp`（详见非目标）。
- `crates/kylinbot-runtime` 中的 security/pairing 模块——对端身份现在随 transport 而变。

## 目标 / 非目标 (Goals / Non-Goals)

**目标 (Goals):**
- 在 Unix 平台上将 HTTP、SSE、WebSocket 默认改为 UDS，附带合理的权限（`0600`）与生命周期（清理残留套接字、关停时移除）。
- 在显式启用（`transport = "tcp"`）时保留完整的 TCP 路径，包括 TLS/mTLS、公网绑定告警和隧道集成。
- 保证完整的路由/中间件对等：每个 axum handler、每一层（timeout、body limit、CORS、限流器）以及每个 WebSocket 路由在两种 transport 下行为一致。
- 用 transport 感知的对端键替换 `SocketAddr`，且不需要分叉 handler 代码。
- 在同一变更中提供一份破坏性变更迁移说明，并同步更新一方客户端（CLI、TUI、安装脚本、Docker、k8s）。一方客户端与 runtime 同发布周期推出，不存在跨版本兼容窗口。

**非目标 (Non-Goals):**
- 任何非 Linux 平台（macOS、Windows、BSD 等）；仅支持 Linux。
- 多进程 / 集群部署（不在范围；通过隧道方案解决）。
- 同一进程同时跑 UDS 和 TCP。
- 替换 gateway 之外的 TCP 监听器（channel webhook、MCP inspector 等）。
- 通用 IPC 帧格式——我们仍是在套接字上跑 HTTP/1.1，而非自定义协议。
- **容器化（Docker / docker-compose）与 k8s 部署的 UDS 适配**。本项目的 supported scope 是 Linux 桌面单机；容器化与 k8s 部署形态在仓库中作为可选资产保留，但其 UDS 适配（socket 卷挂载、sidecar 模式、`emptyDir` 共享、跨容器 peer-cred 语义等）显著超出桌面单机模型。这些部署形态在本次变更中应继续走 `transport = "tcp"`，仅在对应清单中加注释/环境变量提示。如未来重新把容器化纳入 supported scope，应通过独立的 change proposal 推进。
- 非 Linux 分发包（`dist/scoop`、`setup.bat` 等 Windows 资产）的 UDS 适配——它们同理由 `transport = "tcp"` 兜底，且本次变更不主动修改它们。

## 决策 (Decisions)

### 决策 1：每次只跑一种 transport，由 `gateway.transport` 选择
- **选择**：新增单一枚举 `gateway.transport`（默认 `"uds"`，可选 `"tcp"`）。每个进程恰好运行一种监听器。
- **理由**：同时运行两种 transport 会让每个中间件（限流键、配对作用域、访问日志）的关注面翻倍，收益微乎其微。需要两者并存的运维可以跑两个 gateway 进程。
- **考虑过的替代方案**：
  - *始终绑定两者*：拒绝——状态翻倍、配对作用域复杂、与 TLS 配置冲突。
  - *自动检测（存在 `XDG_RUNTIME_DIR` 则使用 UDS）*：拒绝——在精简容器和 CI 中行为难以预测。

### 决策 2：使用 `axum::serve` 配合自定义 `Listener` 实现
- **选择**：将 `tokio::net::UnixListener` 用一个轻量适配器包装，使其实现 `axum::serve::Listener`，并通过 `axum::serve(listener, app.into_make_service_with_connect_info::<UdsConnectInfo>())` 提供服务，其中 `UdsConnectInfo` 是 transport 感知的 extractor。
- **理由**：axum 0.8 已支持自定义 listener；这样无需把现有的手写 TLS accept 循环再分叉一份给 UDS。我们保留单一 `Router` 定义和单一连接信息类型，handler 通过我们自有的 trait 抽取。
- **考虑过的替代方案**：
  - *为 UDS 手写 hyper accept 循环*：拒绝——会把我们想保持精简的 TLS 路径再复制一份。仅在 TLS 分支（仅限 TCP）保留这种写法。
  - *axum `serve` 配合 TCP 类型的 `MakeService` 加上 trait-object listener*：拒绝——会让连接信息类型复杂化，并在 handler 中破坏 `ConnectInfo<SocketAddr>` 抽取。

### 决策 3：用 `ConnectInfo<PeerIdentity>` 替代 `ConnectInfo<SocketAddr>`
- **选择**：引入 `PeerIdentity` 枚举：
  ```rust
  pub enum PeerIdentity {
      Tcp { addr: SocketAddr },
      Uds { uid: Option<u32>, pid: Option<i32> },
  }
  ```
  Handler、限流器、配对守卫直接接受 `PeerIdentity`。`to_rate_limit_key()` 方法返回 `"127.0.0.1"` / `"uds:1000"` 之类的字符串。
- **理由**：保持 handler 签名唯一。把当前格式化 `SocketAddr` 用于日志/键的调用点显式暴露出来——编译期捕获。
- **考虑过的替代方案**：
  - *为 UDS 合成 `127.255.255.255:<peer-uid>`*：拒绝——看起来像真实地址、破坏日志解析、隐藏意图。
  - *两套并行的 handler 树*：拒绝——维护面翻倍。

### 决策 4：UDS 模式下在配置加载阶段禁用 TLS
- **选择**：`Config::validate()` 在 `transport == "uds"` 且任意 TLS 字段启用时返回错误。不静默覆盖。
- **理由**：本地套接字依赖文件系统权限；混入 TLS 是一个额外的隐患，会带来延迟、证书管理与轮换工作，却没有任何收益。在启动时给出响亮的错误，强制运维做显式决策。
- **考虑过的替代方案**：
  - *静默忽略 TLS*：拒绝——掩盖运维意图、调试困难。
  - *允许在 UDS 上跑 TLS 用于"零信任"部署*：拒绝——超出范围；如有真实用户提出再讨论。

### 决策 5：套接字路径默认为 `$XDG_RUNTIME_DIR/kylinbot/gateway.sock`
- **选择**：解析顺序：显式 `gateway.socket_path` → `$XDG_RUNTIME_DIR/kylinbot/gateway.sock` → `${workspace_dir}/run/gateway.sock`。
- **理由**：`XDG_RUNTIME_DIR` 由 systemd 在登录时为每位用户创建，权限 `0700`、按用户隔离，是 Linux 上的标准"按用户运行时目录"。在没有 systemd-logind 的最小化容器或 CI 环境（变量不存在）时回退到 workspace 内路径，仍保留可工作的默认值。
- **考虑过的替代方案**：
  - *`/tmp/kylinbot-<uid>.sock`*：拒绝——`/tmp` 全局可写；恶意本地用户即便 `0600` 也可以赢得竞态。
  - *`/var/run/kylinbot/...`*：拒绝——需要 root，对非特权安装不友好。
  - *只用 workspace 路径，不读 XDG*：拒绝——未对齐 Linux 平台规范，且把套接字落在用户数据目录下不利于权限收敛。

### 决策 6：bind 之后通过 `chmod` 强制设定套接字权限
- **选择**：`UnixListener::bind` 之后调用 `std::fs::set_permissions(path, Permissions::from_mode(0o600))`，不依赖进程 umask。
- **理由**：bind 受当前 umask 影响，在 umask 错误配置的系统上可能比 `0600` 更宽松。bind 后再 `chmod` 让结果具备确定性。
- **考虑过的替代方案**：
  - *bind 前调用 `umask(0o077)`*：拒绝——存在竞态，且会全进程修改 umask。

### 决策 7：启动时清理残留套接字
- **选择**：bind 前尝试 `std::fs::remove_file(path)` 并忽略 `NotFound`；其它错误则报错退出。清理成功时按 INFO 级别记录日志。
- **理由**：异常退出（SIGKILL、OOM kill）会留下残留套接字文件，导致 bind 失败。没有自动清理，重启就需要人工 `rm`。
- **考虑过的替代方案**：
  - *先 ping 现有套接字判断是否有活进程*：拒绝——会增加复杂度并在启动前引入超时窗口。如果有人在同一套接字上跑两个 gateway，这本身就是更严重的问题，应该让 bind 直接抛 `EADDRINUSE`，我们透传给运维。

### 决策 8：使用隧道则要求 TCP（硬性错误）
- **选择**：当 `tunnel.provider != "none"` 且 `transport == "uds"` 时，配置校验失败，给出可操作的提示。
- **理由**：Cloudflare/ngrok 都基于 TCP。静默禁用隧道比启动时报错更糟糕。

### 决策 9：UDS 下被忽略的 `--host` / `--port` 每次启动都告警
- **选择**：在 UDS transport 下解析到 `--host` / `--port` / `KYLINBOT_GATEWAY_HOST` / `PORT` / `HOST` / `PORT` 时，**每次启动**都按 WARN 级别输出一次（每进程一次）告警，不做"首次启动后静默"的状态记忆。
- **理由**：把"配置项被忽略"这个事实长期可见，避免运维在排障时看到旧的 `host`/`port` 仍然写在配置文件里却无效，却找不到日志线索。每进程一次的频率不会污染日志。
- **考虑过的替代方案**：
  - *仅首次启动告警，之后写入状态文件抑制*：拒绝——增加状态文件、跨进程同步与清理负担，对一个一次性提示得不偿失。
  - *降级为 INFO*：拒绝——配置被忽略是配置错误的一种，WARN 级别更准确反映"运维意图未生效"。

### 决策 10：暴露 `KYLINBOT_GATEWAY_TRANSPORT` 环境变量
- **选择**：除 `gateway.transport` 配置项与 `--transport` CLI 参数外，新增 `KYLINBOT_GATEWAY_TRANSPORT` 环境变量（取值 `uds` / `tcp`），优先级低于 CLI、高于配置文件。
- **理由**：与现有 `KYLINBOT_GATEWAY_HOST` / `PORT` / `HOST` / `PORT` 体验一致；CI、容器编排和一次性运维（如 `KYLINBOT_GATEWAY_TRANSPORT=tcp kylinbot gateway`）一行覆盖即可，免去临时改配置文件。
- **考虑过的替代方案**：
  - *仅暴露 CLI 参数*：拒绝——容器化部署不便于注入 CLI 参数，环境变量是 12-factor 标准入口。
  - *复用 `KYLINBOT_GATEWAY_HOST=unix:/path` 风格*：拒绝——`HOST` 字段含义被覆写会破坏现有解析与文档。

### 决策 11：非 Linux 平台在编译期阻断
- **选择**：在 `crates/kylinbot-gateway/Cargo.toml` 加 `[target.'cfg(target_os = "linux")'.dependencies]` 把 UDS 相关依赖限定为 Linux only；并在 `crates/kylinbot-gateway/src/lib.rs` 顶部加 `#[cfg(not(target_os = "linux"))] compile_error!("kylinbot-gateway is Linux-only")`。
- **理由**：编译期失败比运行期错误更容易被开发者注意到，且能让任何跨平台子项目在尝试把 gateway 当依赖编进去时立即报错。同时避免把 UDS-only 代码塞进 macOS / Windows 构建产物的二进制大小。
- **考虑过的替代方案**：
  - *仅运行期报错*：拒绝——错误延迟到用户启动才暴露，调试反馈环长。
  - *用 `#[cfg(unix)]` 而非 `target_os = "linux"`*：拒绝——`unix` 包含 macOS / BSD 等，与"仅支持 Linux"的非目标矛盾，未来可能引入 macOS-specific 兼容代码的诱惑。

## 风险 / 取舍 (Risks / Trade-offs)

- **风险：硬编码 `http://127.0.0.1:42617` 的一方 Linux 客户端在升级后失效** → 缓解：一方客户端与 runtime 同发布（已确认），所以审计 supported scope 内所有客户端调用点（`crates/kylinbot-tui/`、`install.sh`、`dist/aur`、健康检查探针）后在同一 PR 中更新；用户升级时客户端与 runtime 一同跨版本，不存在跨版本错配窗口。CHANGELOG 加一行迁移说明，给出 `transport = "tcp"` 的回退方法。`apps/tauri` 不在审计列表内：它由 `tauri-client-uds` 同周期适配 UDS。
- **风险：使用 Docker / docker-compose / k8s 部署的用户在升级后默认 UDS 静默失败** → 缓解：在 `Dockerfile*` 的 `HEALTHCHECK` 注释、`docker-compose.yml` 的环境变量（默认 `KYLINBOT_GATEWAY_TRANSPORT=tcp`）、`deploy-k8s/*.yaml` 注释中显式声明这些部署形态的默认假设是 `transport=tcp`；CHANGELOG 中作为 BREAKING 条目专门列出"容器化/ k8s 用户必须显式 `transport=tcp`"。本次变更不引入 socket 卷挂载、sidecar、`emptyDir` 共享等运维方案——这些超出 Linux 桌面单机的 supported scope，需要独立的 change proposal。
- **风险：第三方脚本 / 基于 curl 的监控静默失败** → 缓解：CHANGELOG 条目标记 `BREAKING`。`/health` 端点在 UDS 上仍可通过 `curl --unix-socket /run/user/1000/kylinbot/gateway.sock http://x/health` 访问——文档示例化此片段。
- **风险：最小化容器（无 systemd）缺失 `$XDG_RUNTIME_DIR`，回退到磁盘上的 workspace 路径** → 缓解：workspace 回退路径 `${workspace_dir}/run/` 按安装独立、可写、与现有数据目录的权限模型一致；启动 INFO 日志记录实际选用的路径，避免运维猜测。也允许通过 `gateway.socket_path` 显式覆盖。
- **风险：基于浏览器的仪表盘无法访问 UDS** → 缓解：这是符合预期的——TUI 直接连 UDS；嵌入式仪表盘由 gateway 在同一 UDS 上提供，本地用户通过 `curl --unix-socket` 或文件系统层面访问。浏览器用户切换到 `transport = "tcp"`（也就是隧道方案走的路径）。
- **风险：UDS 上的 SSE 长连接在 half-close 行为上可能与 TCP 不同** → 缓解：集成测试覆盖 SSE 断连，包括客户端发起的 `shutdown(Read)`；验证清理时延与 TCP 路径一致。
- **风险：`SO_PEERCRED` 在容器内由命名空间影响，可能拿到的是容器内 UID 而非宿主 UID** → 缓解：UID 仅用作对端键和访问日志字段，不用于授权决策；UDS 自身的访问控制依赖文件系统权限（`0600`）。日志中明确标注 `uds:<uid>` 的 UID 来源是对端命名空间。
- **取舍：单 listener 意味着同时需要本地 + 远程访问时要起两个进程** → 可接受。这个不变量最容易推理；同时需要本地与远程访问的用户可以让隧道终止于一个仅 TCP 的远程 gateway，并代理到本地 UDS。

## 迁移计划 (Migration Plan)

1. **schema 迁移**（暂不引入行为变化）：在配置 schema 中增加 `transport`、`socket_path`，`transport` 默认为 `"uds"`；新增校验规则。所有带 `host`/`port` 的现有配置仍可解析；除非用户显式设置 `transport = "tcp"`，否则会被 UDS 静默覆盖。第一次启动时给出响亮的 INFO 日志，说明选择了哪种 transport 与原因。
2. **listener 拆分**：重构 `run_gateway`，让 bind 步骤返回 `GatewayListener::{Tcp(TcpListener), Uds(UnixListener)}` 枚举，并提供统一的 `serve(app, shutdown)` 方法。TLS 分支保持仅 TCP。
3. **连接信息重构**：引入 `PeerIdentity`，将 `ConnectInfo<SocketAddr>` 换为 `ConnectInfo<PeerIdentity>`，按编译错误指引修复所有 handler 与中间件。
4. **客户端更新**：CLI、TUI、`install.sh`、`dist/aur` 内嵌健康检查默认走 UDS，并提供 `--tcp` / `transport=tcp` 的回退选项。`apps/tauri` 不在更新范围内：由 `tauri-client-uds` 同周期适配 UDS。`Dockerfile*` / `docker-compose.yml` / `deploy-k8s/` 不做 UDS 适配，仅在注释或环境变量中显式声明默认 `transport=tcp`（理由见非目标）。
5. **测试**：新增 UDS 集成测试（HTTP、SSE、WS）、TLS 拒绝测试、残留套接字测试、权限测试、隧道拒绝测试。保留所有现有 TCP 测试；在测试夹具中通过 `transport = "tcp"` 配置启用它们。
6. **文档**：在 `docs/setup-guides/` 新增章节；更新 `README.md` 的 curl 示例；在 `CHANGELOG-next.md` 加 `BREAKING` 条目。
7. **回滚**：单行配置变更（`gateway.transport = "tcp"`）即可恢复旧行为。TCP 代码路径无限期保留——它没有被废弃，只是不再是默认。
8. **发布耦合**：本变更与 `tauri-client-uds` 适配变更需在同一发布周期内推出，且 `tauri-client-uds` 应先合入主线（或同 PR archive），避免 archive 时仍有未适配 UDS 的桌面客户端在产物中。本变更不需要分阶段灰度或编译期默认值切换。

## 待解决问题 (Open Questions)

无。所有事项已在决策 9–11 与发布耦合（迁移计划第 8 步）中落定。如实施过程中暴露新的不确定性，应回填到本节，并视影响面决定是否触发新的 spec/tasks 修订。
