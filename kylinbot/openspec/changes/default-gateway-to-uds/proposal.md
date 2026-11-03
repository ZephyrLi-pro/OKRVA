## 为什么 (Why)

`kylinbot-gateway` crate 当前通过 TCP 套接字暴露其 HTTP、SSE 和 WebSocket 端点（默认 `127.0.0.1:42617`），这意味着每个运行该 agent 的主机都会暴露一个开放端口——即使用户只通过本地的 CLI/TUI 进程访问 gateway。本地 agent 并不需要网络监听器；绑定 TCP 强制每位运维都要考虑防火墙规则、端口冲突、监听暴露告警和 TLS——这些对常见的"纯本地"场景没有任何价值。把默认值切换为 Unix 域套接字 (UDS) 可以为常见场景消除网络攻击面，依靠文件系统权限做访问控制，并消除一类常见隐患（误绑公网、端口冲突、被端口扫描器探测）。当需要远程访问时，TCP 仍可作为显式选项保留。

## 变更内容 (What Changes)

- 在 gateway 配置中新增 `transport` 选项，可选值为 `uds`（默认）和 `tcp`（显式启用）。
- 新增 `gateway.socket_path`（默认 `${XDG_RUNTIME_DIR}/kylinbot/gateway.sock`，缺失 `XDG_RUNTIME_DIR` 时回退到 `${workspace_dir}/run/gateway.sock`），通过 `tokio::net::UnixListener` 绑定。
- 通过 axum + hyper 在 UDS 上提供 HTTP/1.1、SSE 和 WebSocket 服务，将原本基于 TCP 的 `MakeService<SocketAddr>` 替换为支持 UDS 的连接信息类型。
- 自动创建套接字所在父目录、设置套接字权限为 `0600`（仅文件属主可读写），并在启动 bind 之前清理残留的套接字文件。
- 当 `transport = "uds"` 时禁用 TLS/mTLS；如果同时配置了两者，给出明确的配置错误，提示切回 `tcp` 或移除 TLS 配置块。
- 在 UDS 模式下，限流器、配对守卫（PairingGuard）和访问日志字段使用合成的对端标识（如基于 `SO_PEERCRED` 的 `uds:<peer-uid>`）替代 `SocketAddr`。
- 更新一方 Linux gateway 客户端（TUI、`install.sh`、健康检查探针），使其默认通过 UDS 访问，仅在显式配置时使用 TCP。容器化 / k8s / 非 Linux 分发包的 UDS 适配不在本次变更范围内（见下文"不在范围内"）。`apps/tauri` 桌面客户端由 `tauri-client-uds` 同周期适配 UDS，因此不在本次客户端更新清单中。
- **BREAKING**（Beta crate，按规则允许）：升级后默认配置下用户将不再看到 `http://127.0.0.1:42617`——必须通过 UDS 路径访问，或将 `gateway.transport` 设置为 `"tcp"`。隧道服务（Cloudflare、ngrok 等）需要 `transport = "tcp"`。
- **BREAKING**：当 `transport = "uds"` 时 `--host` / `--port` CLI 参数和 `KYLINBOT_GATEWAY_HOST` / `PORT` 环境变量将被忽略；新增 `--socket-path` CLI 参数和 `KYLINBOT_GATEWAY_SOCKET` 环境变量。
- 仅支持 Linux 目标平台；其他平台不在本次变更范围内。
- 在 `CHANGELOG-next.md` 中记录此次迁移，并在 `docs/setup-guides/` 中追加升级说明。

## 能力 (Capabilities)

### 新增能力 (New Capabilities)
- `gateway-transport`：HTTP/SSE/WebSocket gateway 的可配置传输层（UDS 默认，TCP 显式启用），涵盖套接字生命周期（创建、权限、清理）、对端身份识别、以及 TLS 与 UDS 互斥规则。

### 修改能力 (Modified Capabilities)
<!-- openspec/specs/ 中暂无现存 spec；本次变更为 gateway 传输边界引入第一份 spec。 -->

## 影响范围 (Impact)

- **代码**：`crates/kylinbot-gateway/src/lib.rs`（监听器与 accept 循环）、`crates/kylinbot-gateway/src/ws.rs` 与 `sse.rs`（连接信息类型）、`crates/kylinbot-gateway/src/tls.rs`（按 transport 进行门禁）、`crates/kylinbot-gateway/Cargo.toml`（无新增重型依赖；复用已有的 `tokio::net::UnixListener`）。
- **配置 schema**：`crates/kylinbot-config/src/schema.rs` —— 新增 `transport` 枚举、`socket_path`、校验规则（`uds` ⊕ `tls.enabled`）、环境变量覆盖与单元测试。
- **运行时 / CLI**：`src/main.rs` 和 `src/lib.rs` 接入新的 `--socket-path` 参数与 gateway 命令；`is_public_bind` 告警需要感知 transport。
- **客户端与运维（默认 UDS）**：`crates/kylinbot-tui/`、`install.sh`、`dist/aur`（Linux 分发包内嵌的健康检查脚本）——默认改为通过 UDS 访问。`apps/tauri` 不在此列：由 `tauri-client-uds` 同周期适配 UDS。
- **客户端与运维（仅注释/默认 TCP，不做 UDS 适配）**：`Dockerfile*`、`docker-compose.yml`、`dev/docker-compose*.yml`、`deploy-k8s/`——在镜像 `HEALTHCHECK` 注释、compose 环境变量、k8s manifest 注释中显式声明这些部署形态默认假设 `transport=tcp`，不修改 socket 卷挂载或跨容器 UDS 共享。
- **测试**：gateway 集成测试（如有 `crates/kylinbot-gateway/tests/`，否则使用 `tests/`）—— 新增 UDS 上 HTTP 与 WS 往返测试、TLS 拒绝测试、残留套接字清理测试、权限测试。
- **文档**：`docs/setup-guides/`、`docs/reference/`、`docs/ops/`、`README.md`、`CHANGELOG-next.md`。
- **稳定性等级**：`kylinbot-gateway` 处于 Experimental 等级（依据 `AGENTS.md`）；变更默认传输层是允许的，但需要 changelog 条目和迁移说明。
- **不在范围内**：非 Linux 平台支持（macOS、Windows、BSD 等）、跨主机集群、替换 gateway crate 之外的 channel webhook 监听器的 TCP 行为；**容器化部署（Docker、docker-compose）与 k8s 部署的 UDS 适配——这些场景应通过 `transport = "tcp"` 显式启用**，本次变更只在对应清单中加注释/环境变量提示，不引入 socket 卷挂载、sidecar、`emptyDir` 共享等运维方案；非 Linux 分发包（如 `dist/scoop`）的 UDS 适配同理。
