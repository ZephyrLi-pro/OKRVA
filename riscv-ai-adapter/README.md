# RISC-V AI Adapter：当前K1方案

Python3.11+模型服务适配与调度层，为小K和KylinBot提供统一的本地接口。当前模型为Qwen3-0.6B普通对话/工具规划和Qwen3-1.7B-Q4_0阅读总结，语音为SenseVoice识别及Matcha中英播报。

## 当前功能

- OpenAI兼容的聊天、流式文字、语音转写和语音合成接口。
- 由模型选择受限语义计划，参数/上下文约束与真实工具结果校验；KylinBot/MCP执行系统和文件操作。
- 未发送日志草稿及PPT当前页上下文，独立阅读总结模型。
- 最近文档读取、逐文档摘要与分别保存、受限摘要清理的工作流。
- 模型互斥驻留、空闲卸载、内存保护、推理调度、取消和健康状态。
- 原生LibreOffice Impress的UNO/D-Bus后台控制桥；Matcha HTTP/C ABI播报桥接。

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

## 独立问候与身份问答

配置了工具预算和助手身份提示时，完整、明确的问候或身份提问在模型规划前进入精简对话请求，由原有模型生成回答，最多80个输出token。旧语音客户端附带的参考资料封装仅提取实际提问，无关正文和历史不会进入这次推理；参考资料中的指令不能选择此路径。复合操作、工具结果、强制工具请求、图片及依赖上下文的问题保留原有路径。回归见`tests/test_standalone_conversation_route.py`。

## 连续系统操作

每轮计划绑定当前用户消息、模型、工具目录及已发出的原生调用。KylinBot在连续对话中裁剪前面的旧历史时，Adapter接受原历史的后缀，并只允许裁剪位置向前移动；修改、插入、重排或恢复旧消息不能通过校验。工具参数、调用ID及执行结果仍需核对后才推进下一步，避免旧历史裁剪造成误报失败或中断后续操作。回归见`tests/test_system_plan_history.py`。

## 精简受限规划

规划提示按可用工具目录与参数校验语法说明操作能力，保留必要的操作、教学和分析示例，使用紧凑JSON传递数值引用。模型仍选择意图、操作和顺序；原有来源、单位、否定范围、工具目录和真实执行回执校验不变。文档专用参数域不携带无关音量操作示例。

带显式参考资料的语音问题由同一次受限规划选择分析或操作，不再先运行一次独立意图分类。操作授权只来自实际提问，参考资料中的指令不进入操作规划；没有可执行工具或模型选择聊天/分析时，参考资料通过无工具的只读请求处理。回归见`tests/test_reference_context.py`。

## PPT语义操作

完整PPT工具目录接入后，规划schema始终提供打开演示文稿与当前页演讲备注保存两类能力。模型根据本轮完整语义选择`open`或`notes`及参数；指定文件名绑定本轮原文，泛指打开使用工具验证的最近真实历史。

执行工作流核对本轮授权、参数schema、工具调用ID、文档会话、原始页码和正文哈希，保存后验证实际回读结果。明确禁止副作用、纯引用或教学请求仍受授权校验。`semantic_only`普通聊天回退只保留PPT读取工具，打开和保存统一经过受控工作流。回归见`tests/test_presentation_workflow.py`与`tests/test_presentation_fallback.py`。

## 文档摘要复用

最近文档任务的原文摘要选择使用有界内存缓存，最多32份，存活10分钟。缓存键包含模型名、选择协议版本和完整原文文本的SHA256；内容或模型变化后重新运行摘要选择。进程重启清空缓存，不写入磁盘。

每次任务仍由模型选择计划，并通过原生文档工具重新查找、读取、绑定当前文件身份；保存时仍生成新的独立文件、回读并验证回执。命中缓存只省去同一原文的重复摘要推理，首次处理仍运行模型。缓存不复用操作结果或保存回执。取消、过期、模型错误及缓存污染隔离的回归测试见`tests/test_summary_cache.py`。
