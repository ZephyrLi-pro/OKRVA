## 1. 配置 schema 与校验

- [x] 1.1 在 `crates/kylinbot-config/src/schema.rs` 中新增 `Transport` 枚举（`Uds`、`Tcp`），使用 `serde(rename_all = "lowercase")`。
- [x] 1.2 新增 `gateway.transport` 字段，默认值 `Uds`，并新增 `gateway.socket_path: Option<PathBuf>`。
- [x] 1.3 实现 `default_gateway_socket_path()` 辅助函数，按 `$XDG_RUNTIME_DIR/kylinbot/gateway.sock` → `${workspace_dir}/run/gateway.sock` 顺序解析（`XDG_RUNTIME_DIR` 未设置或为空时回退）。
- [x] 1.4 扩展 `Config::validate()`：当 `transport = "uds"` 与 `gateway.tls.enabled = true` 或 `client_auth.enabled = true` 同时出现时拒绝启动，错误信息严格使用 spec 中给出的字符串。
- [x] 1.5 扩展 `Config::validate()`：当 `transport = "uds"` 与 `tunnel.provider != "none"` 同时出现时拒绝启动。
- [x] 1.6 在 `crates/kylinbot-gateway/Cargo.toml` 或 `lib.rs` 顶部加入非 Linux 编译期约束（`#[cfg(not(target_os = "linux"))]` 触发 `compile_error!`），明确表示仅支持 Linux。
- [x] 1.7 新增环境变量覆盖：`KYLINBOT_GATEWAY_TRANSPORT`、`KYLINBOT_GATEWAY_SOCKET`；保留 `KYLINBOT_GATEWAY_HOST` / `PORT` / `HOST` / `PORT` 仍生效，但在 UDS 下被忽略时输出 WARN。
- [x] 1.8 在 `crates/kylinbot-config/src/schema.rs` 中新增单元测试：合法 `transport=uds`、`transport=tcp`、非法值、UDS+TLS 拒绝、UDS+tunnel 拒绝、默认值、环境变量覆盖。

## 2. 对端身份重构

- [x] 2.1 在 `crates/kylinbot-gateway/src/lib.rs`（或新文件 `peer.rs`）中定义 `pub enum PeerIdentity { Tcp { addr: SocketAddr }, Uds { uid: Option<u32>, pid: Option<i32> } }`。
- [x] 2.2 添加 `PeerIdentity::rate_limit_key(&self) -> String`：TCP 返回 `addr.ip().to_string()`，UDS 返回 `format!("uds:{uid}")`（`uid` 为 `None` 时使用 `"uds:?"`）。
- [x] 2.3 添加 `PeerIdentity::display_for_log(&self) -> String`，并在所有当前记录 `remote_addr` 的位置改用之。
- [x] 2.4 将所有 handler 中的 `ConnectInfo<SocketAddr>` extractor（`api.rs`、`api_pairing.rs`、`api_webauthn.rs`、`auth_rate_limit.rs`、`nodes.rs`、`node_tool.rs`、`sse.rs`、`static_files.rs`、`voice_duplex.rs`、`ws.rs`）替换为 `ConnectInfo<PeerIdentity>`。
- [x] 2.5 更新 `SlidingWindowRateLimiter` 的调用点，使用 `PeerIdentity::rate_limit_key`。
- [x] 2.6 更新 `PairingGuard` 与 `is_public_bind` 告警逻辑，使其感知 transport（UDS 完全跳过 `is_public_bind`）。
- [x] 2.7 更新访问日志字段，在 UDS 下记录 `peer_uid` / `peer_pid` 替代（或附加于）`remote_addr`。

## 3. Listener 抽象

- [x] 3.1 在 `crates/kylinbot-gateway/src/lib.rs` 中引入 `enum GatewayListener { Tcp(TcpListener), Uds(UnixListener) }`。
- [x] 3.2 实现 `GatewayListener::bind(config: &GatewayConfig) -> Result<Self>`，按 `config.transport` 分支。
- [x] 3.3 在 `Uds` 分支：以 `0700` 创建父目录、`remove_file` 残留套接字（忽略 `NotFound`）、绑定，然后将文件 `chmod 0600`。任何非 `NotFound` 的清理错误均按上下文报错。
- [x] 3.4 实现 `GatewayListener::serve(self, app: Router, shutdown: watch::Receiver<()>) -> Result<()>`，分发到 TCP/TLS/UDS 各自的代码路径。
- [x] 3.5 UDS 路径下，构建 `app.into_make_service_with_connect_info::<PeerIdentity>()`，通过 `axum::serve(listener, ...).with_graceful_shutdown(...)` 提供服务。使用一个轻量的 `axum::serve::Listener` 适配器封装 `UnixListener`。
- [x] 3.6 实现每连接 `PeerIdentity` 抽取，调用 `UnixStream::peer_cred()` 并存储 `uid`/`pid`（尽力获取；不支持的平台为 `None`）。
- [x] 3.7 在优雅停机阶段注册一步 `std::fs::remove_file(socket_path)`，于 `axum::serve` 返回后执行；INFO 级别日志记录。尽力执行——失败时不让进程崩溃。
- [x] 3.8 TLS 分支保留在 TCP arm；通过编译期 / 运行期断言确保 UDS arm 不会进入 TLS 路径。
- [x] 3.9 更新启动横幅：UDS 下打印 `unix://<path>`，TCP 下打印 `http(s)://host:port`，含已配置的 path-prefix。

## 4. CLI 与入口接线

- [x] 4.1 在 `src/lib.rs` / `src/main.rs` 的 gateway CLI 中新增 `--socket-path <PATH>` 与 `--transport <uds|tcp>` 参数。
- [x] 4.2 当解析后的 transport 为 `uds` 时，`--host` / `--port` 输出 WARN 并被忽略（每次进程启动只告警一次）。
- [x] 4.3 修改 `run_gateway(...)` 签名：以 `GatewayBindOptions` 结构体（transport、host、port、socket_path）替代裸的 `host`、`port` 参数，消除歧义。
- [x] 4.4 将新选项贯通到 `src/main.rs` 的命令分发。

## 5. 集成测试

- [x] 5.1 新增 `crates/kylinbot-gateway/tests/uds_http.rs`：绑定 UDS，通过 Unix-socket 连接器用 `hyper`/`reqwest` 发送 `GET /health`，断言 JSON body 与 TCP 路径一致。
- [x] 5.2 新增 UDS WebSocket 往返测试（`uds_ws.rs`）：建立连接、双向各发一帧、观察关闭。
- [x] 5.3 新增 UDS SSE 测试：打开流、收到 ≥1 个事件、关闭、断言每连接清理在与 TCP 测试相同的时间界限内完成。
- [x] 5.4 新增 `uds_lifecycle.rs`：验证残留套接字清理（提前在套接字路径放一个普通文件）、bind 后权限为 `0600`、优雅停机时被移除。
- [x] 5.5 新增 `uds_tls_rejected.rs`：加载 `transport=uds` 且 `tls.enabled=true` 的配置，断言 `Config::validate()` 返回文档化的错误。
- [x] 5.6 新增 `uds_tunnel_rejected.rs`：同样形态，但是 `tunnel.provider="cloudflare"`。
- [x] 5.7 任何需要 TCP 的测试通过设置 `transport=tcp` 的夹具构造器进行兜底，使现有测试继续通过。

## 6. 一方客户端与运维

> 注：`apps/tauri` 桌面客户端由 `tauri-client-uds` 同周期适配 UDS，因此**不在本节范围内**。本节只涵盖留下的 Linux 客户端与运维资产。

- [x] 6.2 更新 `crates/kylinbot-tui/` 中调用 gateway HTTP API 的位置，使其通过 UDS 访问（或暴露连接模式设置项）。
- [x] 6.3 更新 `install.sh`：确保父目录（`$XDG_RUNTIME_DIR/kylinbot/`）存在；默认健康检查使用 `curl --unix-socket ...`。`setup.bat` 等非 Linux 安装脚本不在本次变更范围内。
- [x] 6.4 在 `Dockerfile`、`Dockerfile.debian`、`Dockerfile.ci`、`Dockerfile.debian.ci` 的 `HEALTHCHECK` 行上方加注释：声明镜像默认假设 `transport=tcp`（即 `ENV KYLINBOT_GATEWAY_TRANSPORT=tcp` 或在文档中说明），保留现有 `curl http://...:42617/health` 形态。**不**引入 UDS 形态的 `curl --unix-socket`、不挂载 socket 卷。理由：容器化部署的 UDS 适配属于本次变更非目标。
- [x] 6.5 更新 `docker-compose.yml` 与 `dev/docker-compose.yml`、`dev/docker-compose.ci.yml`：在 `environment:` 块中显式设置 `KYLINBOT_GATEWAY_TRANSPORT=tcp`，并在文件顶部注释中说明"compose 默认走 TCP，UDS 默认仅适用于 Linux 桌面单机"。**不**引入 socket 卷共享或 sidecar 配置。
- [x] 6.6 更新 `deploy-k8s/*.yaml`（特别是 `configmap-sample.yaml`、`deployment-sample.yaml`）：在 ConfigMap 中显式设置 `transport: tcp`（或 `KYLINBOT_GATEWAY_TRANSPORT=tcp` env），并在 manifest 注释中说明"k8s service 基于网络抽象，必须使用 TCP；UDS 不在 k8s 部署的 supported scope 内"。
- [x] 6.7 审计 `dist/aur`、`dist/scoop` 与其他打包资产：`dist/aur`（Linux 桌面）若内嵌健康检查则同步切换到 UDS 形态；`dist/scoop`（Windows）不在 UDS 范围内，保留 TCP 行为，必要时加注释。
- [x] 6.8 审计 `crates/kylinbot-runtime/src/security/pairing.rs`，移除或限定 UDS 下的 `is_public_bind` 告警范围。

## 7. 文档与发布

- [x] 7.1 新增 `docs/setup-guides/uds-default.md`，说明：如何找到套接字、如何用 `curl` 访问、如何切回 TCP。
- [x] 7.2 更新 `README.md` quickstart，附带 UDS 形态的 curl 片段。
- [x] 7.3 在 `CHANGELOG-next.md` 新增 `BREAKING:` 条目，描述默认变更、迁移开关、`--socket-path` 参数；**单独列出**"使用 Docker / docker-compose / k8s / 非 Linux 分发包的用户必须显式设置 `transport=tcp` 或 `KYLINBOT_GATEWAY_TRANSPORT=tcp`"。
- [x] 7.4 更新 `docs/reference/configuration.md`（或同等文档），覆盖 `gateway.transport`、`gateway.socket_path` 与校验规则。
- [x] 7.5 更新 `docs/ops/` 中提到 `42617` 端口的运维手册条目。

## 8. 验证关卡

- [x] 8.1 运行 `cargo fmt --all -- --check`。
- [x] 8.2 运行 `cargo clippy --all-targets -- -D warnings` 并修复所有新增告警（不允许新增 `#[allow]` 回退）。本机以 `--workspace --exclude kylinbot-desktop` 跑通过；`apps/tauri` 即将由独立 change 移除，不在 supported scope 内。
- [x] 8.3 运行 `cargo test`（workspace），确认 UDS 与 TCP gateway 测试全部通过。本机以 `--workspace --exclude kylinbot-desktop` 跑通过（exit 0）；`apps/tauri` 同上。
- [ ] 8.4 手工验证：Linux 全新安装启动后默认走 UDS；`gateway.transport=tcp` 能逐字节复现旧行为；两种 transport 下配对流程都正常。
