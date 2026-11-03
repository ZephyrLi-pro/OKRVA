# RISC-V AI Adapter

`riscv-ai-adapter` 是面向 K1/K3 RISC-V 边缘设备的轻量模型适配与资源控制层。它不实现模型计算，也不复制麒麟 AI 子系统；它把 KylinBot/小K使用的稳定接口路由到 llama.cpp、sherpa-onnx、ONNX Runtime 等开源后端。

守护进程名为 `riscv-ai-adapterd`，管理命令为 `riscv-ai-adapterctl`。

## 比赛版边界

```text
小K / KylinBot
      │ OpenAI-compatible HTTP
      ▼
riscv-ai-adapterd
      ├── chat          → llama.cpp server
      ├── transcription → sherpa-onnx ASR（现有 127.0.0.1:18081）
      ├── speech         → sherpa-onnx/Piper TTS（可选）
      ├── ocr            → ONNX Runtime OCR worker（可选）
      └── embedding      → ONNX Runtime embedding worker（可选）
```

手势识别仍由 `ukui-air-gesture` 独立完成，不经过 Adapter。当前页上下文属于 KylinBot/阅读器集成，也不进入模型调度层。

第一阶段的可运行闭环是：

```text
键盘/语音输入
   ├── 文本 ───────────────────────────┐
   └── 语音 → sherpa-onnx ASR → 文本 ──┤
                                      ▼
                              KylinBot 上下文与规划
                                      │ OpenAI Tool Calling
                                      ▼
                  RISC-V AI Adapter → llama.cpp/Qwen3
                                      │
                                      ▼
                           KylinBot 工具执行与反馈
```

`ukui-air-gesture` 继续直接负责翻页；阅读器把“当前页原生文本”送入 KylinBot 上下文。OCR、VLM 和 TTS 都保留扩展位，但不作为第一阶段启动条件。

## 借鉴的开源设计

- 北向接口采用 OpenAI-compatible 路径，与 KylinBot 和 llama.cpp server 对接。
- 健康检查和模型就绪状态采用 KServe V2 风格的路径。
- 模型生命周期、显式加载和资源组概念借鉴 Triton，但首版不依赖 Triton。
- Adapter 是轻量控制面；张量计算仍在 SpacemiT llama.cpp/ONNX Runtime EP 等原生后端中完成。

## 当前接口

| 方法 | 路径 | 用途 |
|---|---|---|
| POST | `/v1/chat/completions` | LLM 对话代理 |
| POST | `/v1/audio/transcriptions` | ASR 代理 |
| POST | `/v1/audio/speech` | TTS 代理 |
| POST | `/v1/ocr` | OCR 代理 |
| POST | `/v1/embeddings` | Embedding 代理 |
| GET | `/v1/models` | 模型别名和 Backend 列表 |
| GET | `/health` | Adapter 与 Backend 汇总状态 |
| GET | `/v2/health/live` | KServe 风格存活检查 |
| GET | `/v2/health/ready` | KServe 风格就绪检查 |
| GET | `/v2/models/{model}/ready` | 模型就绪检查 |
| GET | `/metrics` | Prometheus 文本指标 |
| GET | `/v1/system/info` | K1 CPU、内存、温度和 Backend 进程状态 |
| GET | `/v1/scheduler` | 资源组与请求队列状态 |
| GET | `/admin/backends` | 托管 Backend 生命周期状态 |
| POST | `/admin/backends/{name}/start` | 启动托管 Backend |
| POST | `/admin/backends/{name}/stop` | 停止托管 Backend |
| POST | `/admin/backends/{name}/restart` | 重启托管 Backend |

这些 `/v2` 路径只实现状态子集，暂不声明完整 KServe V2 推理协议兼容。

## K1 快速运行

Adapter 本身不需要第三方 Python 包，要求 Python 3.11 或更高版本。K1 比赛配置使用进迭时空优化版 `llama.cpp`：

```bash
sudo apt update
sudo apt install llama.cpp-tools-spacemit
mkdir -p ~/.cache/models/llm
wget -c https://www.modelscope.cn/models/unsloth/Qwen3-0.6B-GGUF/resolve/master/Qwen3-0.6B-Q4_0.gguf \
  -P ~/.cache/models/llm
```

运行比赛配置：

```bash
cp config/k1.competition.toml config/config.toml
PYTHONPATH=src python3 -m riscv_ai_adapter --config config/config.toml
```

另一个终端检查：

```bash
PYTHONPATH=src python3 -m riscv_ai_adapter.cli health
PYTHONPATH=src python3 -m riscv_ai_adapter.cli wait-ready
PYTHONPATH=src python3 -m riscv_ai_adapter.cli system
PYTHONPATH=src python3 -m riscv_ai_adapter.cli backends
curl http://127.0.0.1:18080/v1/models
```

比赛最小闭环测试（就绪状态 + 本地 LLM Tool Calling）：

```bash
python3 scripts/competition_smoke_test.py
# 如有 WAV 样本，再同时验证本地 ASR：
python3 scripts/competition_smoke_test.py --audio /path/to/test.wav
```

现有 KylinBot 的 ASR 地址可从：

```text
http://127.0.0.1:18081/v1/audio/transcriptions
```

改为 Adapter 北向地址：

```text
http://127.0.0.1:18080/v1/audio/transcriptions
```

当前 ASR bridge 使用 Bearer Token；Adapter 直接读取现有
`KYLINBOT_ASR_TOKEN` 环境变量，避免把密钥写进 `config.toml`。

LLM Provider 的 base URL 则改为：

```text
http://127.0.0.1:18080/v1
```

当前 KylinBot 支持 `custom:<URL>` 的 OpenAI-compatible Provider。比赛配置应使用：

```toml
[providers]
fallback = "custom:http://127.0.0.1:18080/v1"

[providers.models.default]
name = "custom:http://127.0.0.1:18080/v1"
base_url = "http://127.0.0.1:18080/v1"
model = "qwen-local"
timeout_secs = 600
```

仓库提供了保留其他配置项并自动备份的接入脚本：

```bash
python3 scripts/configure_kylinbot.py
install -Dm644 packaging/systemd/kylinbot.service.d/riscv-ai-adapter.conf \
  ~/.config/systemd/user/kylinbot.service.d/riscv-ai-adapter.conf
systemctl --user daemon-reload
systemctl --user restart kylinbot.service
```

脚本默认同时把 KylinBot Gateway 切到仅当前用户可访问的 Unix Socket：

```text
$XDG_RUNTIME_DIR/kylin-bot/gateway.sock
```

这是 `kylin-aiassistant-lite` 原生 Agent 聊天和 `voice-assistant` 使用的本地通道；模型请求仍由 KylinBot 转发到 Adapter 的 `127.0.0.1:18080/v1`。若只做独立 KylinBot 调试，可用 `--gateway-transport tcp` 保留 TCP Gateway，但小K比赛链路应使用默认 UDS 配置。

首次运行会把原配置保存为 `~/.kylinbot/config.toml.pre-rvai`。需要回退时执行：

```bash
python3 scripts/configure_kylinbot.py --restore
systemctl --user restart kylinbot.service
```

不要把 Adapter 配成 `ollama` Provider；Adapter 的北向协议是 OpenAI-compatible，不是 Ollama `/api/chat` 协议。

接入脚本只给 `shell` 自动批准，但 KylinBot 的命令白名单、危险命令拦截、路径保护仍然生效；它额外开放 `wpctl`、`pcmanfm-qt`、`lxqt-config`、`xdg-open`，并仅把 XDG 文档和桌面目录加入允许根目录，以覆盖比赛中的音量、应用和文档场景。

## 资源控制

### 本地 SpacemiT Ollama 备选后端

此接入目前是实验方案，尚未通过比赛多步 Agent 闭环的稳定性和延迟验收。
单次工具调用成功或单元测试通过，不代表实际用户任务全部正确；不要直接替换
正式配置。2026-10-10 的对照仍发现复述、上下文回答错误和工具参数错误。

K1 的另一条接入路径是本机 `ollama 0.6.8+spacemit`，仍通过 Adapter 的
OpenAI-compatible 北向接口给 KylinBot 使用，无须改成远程推理或 Ollama Provider。
`config/k1.ollama-chat.toml` 是仅替换 **chat** 后端的片段；不要用它覆盖含 ASR
配置的整个文件。北向模型名仍为 `qwen-local`，南向使用 `k1-qwen3-06b`。

将兼容的 Ollama 程序放在
`~/.local/share/riscv-ai-adapter/ollama/bin/ollama`，模型存储放在同级 `models/`
目录。编辑 `config/ollama/Qwen3-0.6B.Modelfile` 的 `FROM` 路径后，用该本机服务
的 `OLLAMA_HOST=127.0.0.1:18085` 导入模型：

```bash
OLLAMA_HOST=127.0.0.1:18085 \
  ~/.local/share/riscv-ai-adapter/ollama/bin/ollama create k1-qwen3-06b \
  -f config/ollama/Qwen3-0.6B.Modelfile
```

服务文件位于 `packaging/systemd/riscv-ai-ollama.service`，Adapter 的可选依赖
位于 `packaging/systemd/riscv-ai-adapter.service.d/ollama.conf`。服务仅监听本机
18085 端口，限制单个常驻模型、单并发；18084 仍保留给 OCR。独立模型存储目录
须事先创建。首次启动 Ollama 会生成用户签名密钥；使用只读 HOME 的服务前，应
先以同一用户启动一次初始化，或按部署环境设置精确的可写目录。

此方案源于同一 GGUF 的运行路径对照：已安装 llama.cpp 的 `--no-repack`
路径曾稳定出现错误答案，不能直接归咎于 0.6B 的参数量。更换运行时不保证
任意问题、严格输出格式和多步工具任务都正确；必须继续做真实问答及工具结果
回传验收。优化程序自行选择 CPU 集合，不应强制迁移推理线程到其他核心。
唤醒识别与 LLM 同时运行的资源竞争也需要单独测量。
该用户服务使用 `CPUWeight=1000` 提高发生 CPU 竞争时的推理份额；这不是 CPU
核心绑定，也不会在空闲时限制唤醒识别。是否满足语音实时性仍需在目标设备验收。

Adapter 使用 `read1()` 及时转发小型 SSE 事件；客户端是否逐段显示，还取决于
KylinBot 和前端是否启用并消费流式响应。流式转发不会修复模型内容错误。
已发送响应头后遇到后端超时，会中断连接而不是在流中再拼接一个 HTTP 502；
客户端取消时也不会继续尝试回写错误响应。旧 Ollama 在携带工具时可能自行
缓冲生成内容，即便 Adapter 已正确转发，也不能保证每个 token 都立即可见。

Ollama 0.6.8 的 OpenAI 转换器会丢掉 `content=""` 的 assistant 消息中的
工具调用。该后端启用 `ollama_tool_call_compat=true`，仅将带工具调用的空
assistant 内容改成 `null`，保留调用 ID、参数、工具结果和完整历史。默认关闭，
不影响其他 OpenAI 后端；带非空文本和工具调用的混合消息不在此兼容补丁范围内。

LXQt 桌面工具还需检查 KylinBot 的执行环境。若 `shell` 保持环境清理，按需在
KylinBot 配置已有 `[autonomy]` 中设置 `shell_env_passthrough` 为
`["XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS", "DISPLAY", "WAYLAND_DISPLAY"]`。
仅透传父进程实际具有的变量；不要关闭环境清理或透传整套环境。PipeWire 的
`wpctl` 缺少运行时目录会连接失败，GUI 程序也需对应的桌面显示环境。仍须保留
命令白名单、执行审批和路径保护，配置保存后确认进程已重载再验收。

`resource_groups` 提供跨 Backend 的并发互斥。例如 K1 上可把 LLM 和 OCR 放入同一个 `heavy_inference` 组，避免同时争抢内存带宽；ASR/TTS 放入 `speech_pipeline` 组。

每个 Backend 还支持：

- `max_concurrency`
- `acquire_timeout_seconds`
- `priority`（0–100；越大越优先，可被请求头 `X-RVAI-Priority` 覆盖）
- `allowed_tools`（仅保留指定的 OpenAI Function Tool Schema；空数组表示不过滤）
- `max_system_prompt_chars`（在模型边界再次限制系统提示；`0` 表示不限制）
- `system_prompt_override`（为小参数本地模型替换过大的上游系统提示；空字符串表示保留）
- `timeout_seconds`
- `required`
- `health_url`
- 可选的 `managed`＋`command` 子进程管理
- `restart_on_failure`、`restart_backoff_seconds`、`max_restarts`

`command` 使用参数数组直接启动，不经过 Shell。

当前调度是“排队优先级”，不会中断一个已经开始的推理。比赛默认让 ASR 使用 90、LLM 使用 50、TTS 使用 70、OCR 使用 20；同一资源组内优先保障实时交互。

KylinBot 0.7.4 默认工具目录会让首轮请求接近 12k tokens，且 Gateway Agent 路径可能绕过自身的系统提示长度设置。`k1.competition.toml` 只向 0.6B 模型传递比赛需要的 Shell 与文件工具，用面向 K1 演示的短提示替换上游大提示，并把 llama.cpp 上下文设为 8K；接入脚本同时压缩 KylinBot 历史上限，并关闭会额外触发 LLM 推理的后台长期记忆归纳。会话历史仍由 KylinBot 保存。KylinBot 仍负责工具的实际执行与安全检查，Adapter 只做模型请求整形。

启用 `allowed_tools` 和 `system_prompt_override` 的 K1 预算策略后，Adapter 对精确匹配的问候、身份问题（如“你好”“你是谁”）以及孤立的“我”“你”片段使用无工具短对话：仅传递当前问题和简短身份提示，最多输出 80 tokens。它不是通用意图分类器，不会把含系统操作的复合请求、图片、工具结果或强制指定的工具调用降为聊天。片段输入要求模型询问补充，不猜测系统任务。上层会话历史仍保留，但这些无状态问候不向模型传递历史。

## 测试

```bash
python3 -m unittest discover -s tests -v
```
