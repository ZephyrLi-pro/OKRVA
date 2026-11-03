# PPT阅读桥接

连接 LibreOffice Impress、阅读挥手模式和小K已有的 `SetReadingContext` 接口。
原始PPT按只读方式打开，禁用宏和外部链接更新，使用独立LibreOffice配置目录；不接管用户已有文档窗口。

```sh
systemctl --user start riscv-ai-reader.service
systemctl --user stop riscv-ai-reader.service
```

也可在应用菜单打开“多模态PPT阅读”。当前演示配置使用用户提供的openKylin介绍PPT。
右挥到下一页，左挥到上一页，回起始位置只复位。问“这一页主要讲了什么”时使用当前页原生文字。
手势服务独立运行，关闭阅读窗口不会停止手势识别。没有阅读窗口时不会退回全局键盘事件。
关闭放映窗口回到Impress编辑视图后，跟随实际选中页；关闭整个演示文稿会结束阅读会话并清理语音上下文。

实现通过独立UNO工作线程控制特定文档，不依赖键盘焦点。页码经过读取确认后再响应手势调用。
文本按实际选中页获取，包括分组形状和表格。语音使用当前页标题作为页面标题，文档名另保留在阅读器状态中。语音快照上限2000个UTF-16编码单元，每10秒续期；
新语音进程启动时立即重新发布。没有原生文字的页明确标记不可读取，不假造OCR结果。
轮询实际页码也涵盖鼠标或键盘手动换页。页面文本留在内存，状态文件只记页码、字符数和哈希。

会话D-Bus接口：`org.riscv.AIReader`，对象 `/org/riscv/AIReader`，接口 `org.riscv.AIReader1`。
`Navigate("left"/"right")`、`GotoPage(页码)`、`GetState()`、`Close()`。
没有网络监听端口。依赖系统已有 `python3-uno`、`python3-dbus-next` 和 LibreOffice Impress。

直接运行其他文件：

```sh
python3 reader.py --file /absolute/path/manual.pptx --title 手册
python3 -B -m unittest discover -p 'test_reader_core.py'
```

接口参考：[LibreOffice XSlideShowController](https://api.libreoffice.org/docs/idl/ref/interfacecom_1_1sun_1_1star_1_1presentation_1_1XSlideShowController.html)。

阅读动作在独立线程中串行执行，最多保留一个等待动作；过期动作不会自动重试。大型PPT冷页可能需要等待绘制完成，识别线程不会因此暂停。

## 在其他同类环境复现

`packaging/`保留本机已验证的两个用户服务及应用菜单入口示例。先按实际安装目录、PPT路径、起始页和摄像头修改模板，再复制到用户的systemd/applications目录。
还需要对应版本的voice-assistant当前页上下文接口，以及ukui-air-gesture的reader_actions/queued_reading扩展；这些代码分别保存在各自源码项目中。没有调用麒麟AI子系统读取当前页。

```sh
mkdir -p ~/.config/systemd/user ~/.local/share/applications
cp packaging/*.service ~/.config/systemd/user/
cp packaging/riscv-ai-reader.desktop ~/.local/share/applications/
systemctl --user daemon-reload
systemctl --user start riscv-ai-reader.service
```

服务按需启动，模板没有启用开机自启。关闭整个阅读会话后，语音窗口中的历史会保留，但新的“这一页”问题会提示没有当前页面。

## 可选的独立阅读总结模型

Adapter可在主聊天后端配置 `reading_summary_backend`，指向另一个启用的聊天后端。
只有已经由模型判定为只读分析、且携带有效当前页快照的独立总结问题，才选择这个后端。
旧页面比较、具体细节追问和日志分析保留原有上下文处理路径。页面内容不会增加工具权限。

本机配置为：普通聊天和工具规划维持Qwen3-0.6B-Q4_K_M；当前页总结使用Qwen3-1.7B-Q4_0（2026-11-02部署）。原Q4_K_M文件保留用于回退，后文旧版本测试记录仍对应其原始配置。
总结模型设置 `managed=true`、`on_demand=true`、`required=false`，并与主模型共用串行的 `heavy_inference` 调度组。
请求取得调度名额后才启动模型。默认idle_timeout_seconds=0时，响应结束即卸载。本机配置idle_timeout_seconds=60：成功响应后短时保留模型及KV缓存，空闲60秒后自动卸载；失败或取消仍立即卸载。保留期间不占用推理调度名额，新请求会取消旧的空闲卸载计时器。它不会随Adapter启动而常驻，低可用内存退出保护保持启用。
需要配置健康检查，且此后端的并发数必须为1。

本机阅读模型单独关闭TCM分配，主模型和ASR的TCM配置不受影响。
`python3 -B -m riscv_ai_adapter.model_worker --startup-min-mib 2048 --runtime-min-mib 512 -- <推理服务命令>`
会检查可用内存，并在运行期间低于下限时停止自己创建的推理进程；不终止其他服务。

原Q4_K_M模型来源是[LM Studio community Qwen3-1.7B GGUF](https://huggingface.co/lmstudio-community/Qwen3-1.7B-GGUF/blob/main/Qwen3-1.7B-Q4_K_M.gguf)，
本机文件SHA256为 `e0801cbda7e2f3fd00bea4d73b53b422b14b13aa130e778f6414b6b641920b7e`，不是进迭时空发布的量化文件。
原生正文直接分段提供给阅读模型，模型生成总结，不预置PPT答案。等价的语音识别结尾标点会统一处理。

复现时应分别检查流式答案、真实当前页、事实关系、TTS完成和模型释放；正文关键词出现并不等于总结正确。
本机两页隔离测试约80–128秒，同一语音会话的两页正式回归，从提交问题到播报结束约159–197秒，含规划、模型启动、生成与TTS。本轮已完成实际当前页、换页回答、播报、模型卸载及普通0.6B聊天回归；用户确认新版表达合适。摘要会省略部分技术限定词，精确型号、规范和排名需核对原文。


## 阅读延迟优化验证（2026-10-29）

模型、量化文件、提示词和混合后端保持原样；本轮未采用大批处理或纯IME生成。正式语音窗口三轮实测：首次冷启动约202秒，换页后约104秒，同页再问约62秒，均包含播报。首个文字分别约105、42、6秒；这些是不同使用状态的单轮记录，不代表所有问题都能6秒出字。原每问卸载配置在同一AI功能页约159秒。首次冷启动没有因此变快。

模型进程RSS约2.6GiB，空闲保留时间限制为60秒。实际进程定时退出约61秒，监听端口也已关闭。保留配置需on_demand=true，范围0至120秒；0保持原来的立即卸载行为。同页重复生成的措辞会有变化，用户已确认本轮概括程度合适；精确技术限定词仍应核对原文。


## 模型内存轮换（2026-11-02）

当前 K1 有约 8GB 内存，PPT、ASR 和桌面会共同占用内存。仅限制推理并发不能释放空闲模型的权重；0.6B 与 1.7B 同时驻留时，阅读模型可能在加载阶段触发内存保护。

在 Adapter 的两个模型 backend 中配置互相引用的 `exclusive_with`：

```toml
# 普通聊天 backend
exclusive_with = ["k1-reading-summary"]
# 阅读总结 backend
exclusive_with = ["k1-qwen3-llama"]
```

这两个 backend 必须都是 managed chat，使用同一个并发为1的 resource_group，并保持每个 backend 的 max_concurrency=1 和 health_url。字段缺省为空，不改变其他后端的驻留策略。

请求取得共享调度名额后，先终止并回收空闲的对方模型，再启动所需模型并等待健康检查。后台监督器不会自动复活被替换的模型；已有请求会完成后才允许切换。普通聊天模型的 required 检查将这种有意卸载标记为 standby，实际故障仍报告不可用。

阅读模型最多保留60秒空闲缓存；期间若普通聊天或规划需要0.6B，则提前释放1.7B，取消其旧卸载计时器。后续阅读问题仍可自动重新加载1.7B。模型切换有重新加载的时间开销。ASR、TTS、手势和PPT进程不参与这两个LLM的轮换。

RAM保护仍保留：阅读模型启动前至少2048MiB可用，运行中低于512MiB会终止自身；错误日志增加实际余量和阈值。本轮当前页完整总结及切回普通聊天验证通过，总结期间可用内存最低约1296MiB。原有回归及新增切换检查共183项通过。

降低阅读器图片缓存从300000000字节到128MiB的现场试验未测到PSS下降，翻页时间增加，已恢复原值，未保留该调整。

同一问题通过小K窗口 SubmitExternalText 再次回归，当前页上下文、模型总结和TTS播放完整结束，约197秒；这是识别后文本注入，未重新测试物理麦克风。


## 1.7B生成线程分配（2026-11-02）

阅读模型的 environment 增加 `OMP_PROC_BIND = "TRUE"` 与 `GOMP_CPU_AFFINITY = "0 1 2 3"`，四个OpenMP计算线程分别使用核心0、1、2、3。仅设置在k1-reading-summary子进程，不改变普通聊天、ASR、TTS、手势或桌面的配置。

Qwen3-1.7B-Q4_K_M、4线程、原IME预填充/CPU生成、TCM关闭及现用Q6库均保持原样。与原配置在同一页、相同353个输入token、禁用提示缓存条件下交叉测试4次，四轮均生成41个token且回答逐字相同。持续生成均值0.897→1.125 token/s，约提升25.4%；首字53.25→49.50秒，整段文字98.96→86.13秒。这些是模型直连数据，不含模型加载、规划、ASR或TTS，并非所有页面的速度保证。

正式配置仍保留两个LLM按需轮换与RAM保护。线程分配与模型驻留是独立设置，当前只改变前者。实际子进程环境、线程掩码和动态库映射另有核对记录。

正式回归：小K窗口当前页总结和TTS完整结束（约168秒，含启动、规划、生成及播报；不作为隔离测速对照），切回0.6B回答7乘8得到56。ASR、TTS、手势及阅读器进程未重启。新配置已验收，自动回退定时器已解除。


## 2026-11-02: reading inference and wake-word activity

The KWS activity watcher now observes both managed LLM ports (18082 and 18084)
and Adapter heavy_inference activity. The previous watcher only recognized
18082, leaving wake-word inference active during text-based reading summaries.
Busy-time suppression and idle wake recovery passed regression checks; ASR,
TTS, reader and gesture processes are unchanged. The 1.7B model, four-thread
configuration, accepted CPU affinity and Q6 CPU library remain unchanged.

Controlled four-run ABBA, same resident model and identical page12 narrative:
1.244 to 1.364 token/s, first text52.56 to42.30seconds, complete text85.55 to
72.33seconds. These figures exclude model loading, planning, ASR and TTS.
All four answers are identical. This was not a new physical microphone test.
Details and exact source/backup live in
`/home/openkylin/text-generation-perf-20261102/NEXT-PERFORMANCE.md`.


## 2026-11-02: 1.7B Q4_0 reading deployment

The current reading-only backend uses Qwen3-1.7B-Q4_0, produced from the
existing Qwen3-1.7B-Q8_0 file. Ordinary0.6B chat/planning is unchanged.
The original Q4_K_M remains available. Four threads, existing CPU library,
accepted affinity, memory guards, KWS activity fix and residency policy remain.

Same-round4run ABBA, same353-token page12 narrative request, modelsready:
first text38.39 to22.41seconds, full text64.51 to47.93seconds, sustained
generation1.570 to1.685token/s (+7.35%), RSS3252.72 to2411.41MiB.
The benefit is principally first-text latency and memory, not a large decode
speedup. These model timings exclude loading, planning, ASR and TTS.
Candidate43outputtokens vs41baseline; eachmodel's repeats are identical,
not identical acrossquantizations.

Two private page summaries, arithmetic/knowledge, actual Adapter readingroute,
ordinary chat and a real voice-window/TTS test passed. The voice test submits
recognized text; no new physicalmicrophone test was performed. Its currentpage5
case took115.11seconds including planning/loading/generation/playback.
Summaries can omit names/statistics and paraphrase numeric scope; consult the
page for exact numbers. See `/home/openkylin/decode-deeper-20261102/PERFORMANCE.md`
and the accepted `model-v2-deployment.json` for sources, results and backup.
