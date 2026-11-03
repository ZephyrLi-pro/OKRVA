# RISC-V AI Adapter：当前K1方案

Python3.11+模型服务适配与调度层，为小K和KylinBot提供统一的本地接口。当前模型为Qwen3-0.6B普通对话/工具规划和Qwen3-1.7B-Q4_0阅读总结，语音为SenseVoice识别及Matcha中英播报。

## 当前功能

- OpenAI兼容的聊天、流式文字、语音转写和语音合成接口。
- 由模型选择受限语义计划，参数/上下文约束与真实工具结果校验；KylinBot/MCP执行系统和文件操作。
- 未发送日志草稿及PPT当前页上下文，独立阅读总结模型。
- 最近文档读取、逐文档摘要与分别保存、受限摘要清理的工作流。
- 模型互斥驻留、空闲卸载、内存保护、推理调度、取消和健康状态。
- LibreOffice UNO/D-Bus专用PPT阅读器；Matcha HTTP/C ABI播报桥接。

当前配置统一保存在仓库根目录[`deploy/`](../deploy/)，旧Ollama、Melo和Paraformer配置不另行收录。系统/文档MCP使用[`system-tools/`](../system-tools/)；手势使用独立[`ukui-air-gesture/`](../ukui-air-gesture/)。

## 运行与测试

```sh
python3 -m pip install .
python3 -m riscv_ai_adapter --config /home/openkylin/.config/riscv-ai-adapter/config.toml
riscv-ai-adapterctl health
PYTHONPATH=src python3 -B -m unittest discover -s tests -p 'test_*.py'
```

当前服务只监听127.0.0.1:18080；ASR、主LLM、TTS、阅读LLM分别使用18081、18082、18083、18084。完整安装、模型来源和当前运行参数见根目录[复现说明](../docs/REPRODUCE.md)和[依赖说明](../dependencies/README.md)。

目录：`src/riscv_ai_adapter`为核心，`scripts`为当前Matcha桥接和运行辅助，`tools/reading`为PPT阅读，`tests`为回归测试。通用接口中保留的兼容分支属于程序实现；部署仅采用上述当前方案。
