# 当前方案范围

本仓库提交当前在K1上使用的调试完成方案，包括前端、KylinBot、Adapter、手势、系统/文档工具、SenseVoice包装、Matcha桥接及实际采用的GGML优化。

组件完整源码分别放在根目录对应文件夹。当前厂商后端修改放在`patches/`，包含应用补丁及修改后的完整文件；原始上游通过固定版本获取。当前已采用的Q6算子优化随GGML修改提交。

只保留一套有效运行配置：`deploy/systemd/`为整理后的当前服务配置；`deploy/templates/`为脱敏的应用配置。不会同时提供旧Melo/Ollama/Paraformer部署方案。

备份、修改前文件、工作树差异副本、导出快照、旧试验记录和未采用的算子分支不属于当前源码交付。当前功能需要的完整上游源码、兼容接口和测试继续保留；源码中实现的“备份工具”等正常功能不属于备份文件。

第三方ORT/EP、原始推理框架、模型、Qt/LibreOffice及SDK由依赖说明记录，不上传二进制和模型权重。组件基线版本见`dependencies/source-versions.json`；各原始许可证保留在组件和`licenses/`目录中。
