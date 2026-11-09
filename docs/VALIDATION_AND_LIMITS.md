# 当前验证与限制

当前GGML修改曾在K1从干净object构建通过。源码精度、混合路径、TCM和Q6的验证程序保留在`patches/spacemit-llama/tests/`，不保留旧实验产物或试验报告副本。

清理配置时，逐项核对K1实际生效的服务Environment、CPUAffinity和CPUWeight；旧命令和drop-in被合并后，参数保持一致。清理不重启或改动K1正式服务。

在Adapter目录运行`PYTHONPATH=src python3 -B -m unittest discover -s tests`；KWS策略测试在`kylinbot/tools/bianbu-kws`运行`python3 -B -m unittest test_policy`。系统和文档工具测试见其目录说明。根目录运行`python3 tools/verify_bundle.py`检查当前文件清单。

测试通过仍需配合实际麦克风、扬声器、工具副作用、手势、当前页与摘要文件的场景验收。

## 本次源码同步校验

2026-11-09在WSL Ubuntu 24.04（x86_64、Python 3.12）执行以下源码回归。手势测试使用仓库外的独立Python环境；测试依赖与构建缓存不纳入源码提交。这些结果验证接口、状态和文件处理逻辑，K1性能仍采用下节所列固定基准。

| 测试范围 | 结果 |
|---|---|
| Adapter完整回归 | 221项通过 |
| 手势与桌面动作 | 170项通过 |
| 系统、文档及PPT MCP | 54项通过 |
| 原生Impress桥 | 45项通过 |
| 阅读器核心页桥 | 14项通过 |
| 部署与安装辅助 | 34项通过 |
| KWS忙闲策略 | 6项通过 |
| KylinBot MCP transport及真实stdio子进程 | 43项通过 |

Adapter本地HTTP认证测试使用独立无代理客户端，并主动注入不可用代理验证环境隔离；生产认证检查和拒绝断言保持不变。`bash -n install.sh`及`git diff --check`通过。源码清单在同步前重新生成并校验。

Rust使用1.94.0。MCP两个改动文件的`rustfmt --edition 2024 --check`通过；实际MCP回归使用以下命令（在`kylinbot/`运行），包含超过30秒的工具调用与超时后迟到回包隔离：

```bash
CARGO_TARGET_DIR="$HOME/.cache/okrva-validation/target" \
CARGO_INCREMENTAL=0 CARGO_PROFILE_DEV_DEBUG=0 CARGO_PROFILE_TEST_DEBUG=0 \
  cargo test --locked -p kylinbot-tools --lib mcp_transport --message-format=short
```

首次默认诊断输出触发Rust1.94编译器诊断渲染崩溃；关闭增量/debug并采用短诊断后，43项MCP测试完整运行通过。未修改仓库的工具链配置。

整库`cargo fmt --all -- --check`仍报告192个既有文件的格式差异；严格`cargo clippy --locked -p kylinbot-tools --all-targets --message-format=short -- -D warnings`被未修改的config代码阻塞：`policy.rs:129`的`unused_mut`及`schema.rs:3849`的`derivable_impls`。这些检查未标为通过，也没有为同步源码批量改动无关配置或安全代码。


## 主要限制

- 1.7B模型的性能依据为[2026-11-06固定文本概括基准](reproducibility/reading-summary-16gb-20261106/README.md)：两次模型就绪请求的持续生成均值为2.582 token/s、首字16.09秒、完整文字32.36秒。输入、参数和原始记录随该目录保留。本次源码同步未重新测速，不把该独立基准当作新增演讲备注任务的端到端耗时。
- 完整语音备注任务还包括唤醒、转写、意图规划、模型切换、生成、工具写入、保存回读与播报。冷启动、输入长度及后台负载会影响实际耗时；缓存和线程配置不能消除这些阶段的成本。
- 原生文档文本提取不做OCR；纯图片PPT/扫描页、本地VLM未接入。OFD不在当前范围。
- 文档摘要为模型选取的原文要点；任务范围1–5个文件，受大小/类型/超时限制，非任意路径和自由执行。
- 系统工具默认只开放本机已验证的受限能力。Wi-Fi/蓝牙更改、亮度修改默认不注册；护眼、自动息屏、深色模式、麦克风设置等不能因参考机支持就说本机全部完成。
- 手势仍受光照、手掌完整入镜、中心复位和摄像头角度影响；图标闪烁表示反馈，不等于LibreOffice Impress必然翻页成功。
- PPT正式入口使用LibreOffice24.8.7原生Impress及保留的24.8用户配置，普通视图支持底部备注栏；后台UNO桥需已连接，当前文档须为已保存的本地演示文稿。演讲备注保存目前限PPTX；换文档或正文改变会使已有快照失效，不会将讲稿写到别的页面。浏览器手势标签页切换未实现。
- TTS支持中英混读及当前顿号停顿，但专名/复杂数字仍需听感验证。小K按已生成片段滚动合成/播放，若LLM或TTS未及时补充音频，无法凭缓存保证绝对不停顿。
- LibreOffice24.8运行库已在本机独立前缀验证；新设备安装器仅检查既有24.8启动器，不自动取得这套运行库。启动器版本通过不等于底部备注栏与实际文件保存已验收，完整新机还须验证GUI和UNO连接。
- 当前GGML修改已通过K1干净构建验证。新安装入口负责准备Qt、Rust、模型和外部资源；完整空白新机从零构建安装仍需在首台新设备上验收。

## 当前Qt开发环境检查

PPT工作流的源码回归结果见上节。既有原生K1验证包括KWS/VAD编译、模型与文本前端资源校验、1.7B模型重新量化一致性检查、示例PPT生成，以及Gateway、LLM、TTS、ASR运行自检。

手动使用系统Qt开发包时，曾出现Qt5Gui配置引用缺失GLX插件的问题。新安装入口将Qt GLES开发文件提取到独立SDK，不替换桌面GLES Qt运行库，也不从系统导入GLX插件配置。Bianbu依赖和该SDK的91项运行依赖已通过APT只读安装模拟，计划没有移除现有桌面组件。测试和限制详见[安装说明](../deploy/INSTALL.md)。
