# 提交范围和成果归属

## 本次需要上传

本比赛仓库直接收录以下三类源码成果：

1. **系统集成与新模块**：Adapter、MCP系统/文档工具、ASR HTTP包装、TTS桥接与播放衔接、KWS资源策略、PPT阅读与上下文桥接。
2. **基于上游项目的适配**：小K Lite、KylinBot K1、ukui-air-gesture Bianbu的完整源码。不是只交几个补丁文件，便于评审直接浏览与构建；原有上游代码和许可证保留。
3. **底层优化与修复**：实际部署的GGML九文件补丁、TTS四文件补丁，以及用户独立17项RVV优化对应15个分支的代码和补丁。不能因为底层框架来自进迭而漏交自己的修改。

`provenance/source-origins.json`记录四个项目的Git HEAD、分支和远程地址。该HEAD只是快照基线，**不表示所有交付代码都已提交到该HEAD**。完整组件工作区分别在根目录对应文件夹中；`provenance/working-tree-deltas`补充已跟踪文件相对HEAD的差异，新建源文件在各组件的完整源码目录中。评审或复现应以本包源码和SHA256为准。

`optimizations/rvv/branches.json`记录各独立算子分支基线/头版本及文件哈希；它们基于不同上游版本，不应把15个补丁不加区分地叠到当前vendor树上。当前实际集成的是Q6相关第1、17项。

## 外部依赖，当前不打包

| 项目 | 本包提供的替代材料 |
|---|---|
| 原始SpacemiT llama.cpp/GGML框架 | 固定commit、下载/获取脚本、我们的补丁及完整改动文件 |
| ORT、SpacemiT EP、model-zoo-asr、sherpa-onnx、model-zoo-tts原始组件 | 来源/版本/构建说明；TTS实际修改另附补丁 |
| Qwen/SenseVoice/Matcha/Vocos/KWS/VAD/手势预训练权重 | 模型清单、已安装文件的SHA256、来源和量化步骤 |
| Bianbu/Qt/LibreOffice/桌面SDK及系统软件包 | `dependencies/system-packages.tsv`和平台信息 |
| 用户演示PPT、Documents/Desktop真实文件 | 不提交个人材料；按步骤使用自己的PPT和测试文档 |
| 二进制、DEB、构建目录、虚拟环境、缓存 | 由源码与外部依赖重新构建，不充当原创源码 |

不把第三方原始实现或预训练模型列为自主研发。比赛文字建议表述为“基于KylinBot和麒麟开源前端进行RISC-V/Bianbu适配，开发统一Adapter和多模态上下文/工具执行链，并完成部分GGML RVV算子优化及SpacemiT后端适配修复”。

## 提交前检查

- 运行`python3 tools/verify_bundle.py`，结果应为通过。
- 保留根目录各组件源码、README、patches、optimizations、deploy、dependencies、provenance、licenses、validation及SHA256SUMS。
- 不另行加入真实token、ASR env文件、`settings.ini`、`.kylinbot`对话数据库、用户音频、模型权重和build/target。
- 通过OKRVA仓库提交比赛源码，原始组件仓库版本和许可证均保留。
