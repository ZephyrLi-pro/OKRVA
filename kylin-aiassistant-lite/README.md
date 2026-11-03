# kylin-aiassistant-lite

面向进迭时空 K1、RISC-V 与 Bianbu LXQt 环境的轻量化小K适配版本。代码基于
openKylin `kylin-aiassistant` 的 `openkylin/huanghe` 分支整理，重点降低非
x86/ARM 架构上的桌面组件和文档解析依赖。

## 主要适配

- 原生聊天界面不再导入 `QtWebEngine`，RISC-V 构建不会因为缺少 QML WebEngine
  模块导致 `Chat.qml` 整体加载失败。
- Web 管理页面拆到独立的 `WebKBotPage.qml`，仅在 amd64/arm64 架构启用
  QtWebEngine 与 QtWebChannel。
- `libkysdk-advanced-ai-dev`、QtWebEngine 相关开发包和运行库仅由
  amd64/arm64 架构依赖。
- `kysdk-ukuiwindowhelper` 改为编译期可选；缺失时使用 Qt/X11 窗口能力回退。
- 用独立 `DocumentReader` 替代 `ukui-file-metadata` 文档正文提取：
  - TXT/Markdown：`QFile` 直接读取；
  - PDF：调用 `pdftotext`；
  - DOC/DOCX/WPS/PPT/PPTX：LibreOffice headless 转换后提取文本。
- OCR、OFD 与重型本地 VLM 不属于本仓库当前阶段的文档解析范围。

## 架构边界

原生小K聊天与 KylinBot Agent 界面在 RISC-V 上可用。Web 管理页、上游本地模型
组件和知识库继续只为 amd64/arm64 构建。RISC-V 上的 LLM/ASR 推理建议通过独立
的 `riscv-ai-adapter` 与 KylinBot 服务提供。

这个 lite 版本减少了上述架构不兼容依赖，但并不表示完全移除了所有麒麟桌面与
AI SDK 依赖；实际依赖列表以 `debian/control` 为准。

## 构建

在满足 `debian/control` 的构建环境中执行：

```bash
debuild -b -us -uc
```

仅验证 qmake 工程时可执行：

```bash
qmake kylin-aiassistant.pro
make -j"$(nproc)"
```

生成的 Makefile、目标文件、MOC/UIC/RCC 文件、可执行文件和 Debian 构建缓存均
由 `.gitignore` 排除，不应提交到源码仓库。

## 相关项目

- 上游项目：<https://gitee.com/openkylin/kylin-aiassistant>
- RISC-V 模型适配层：<https://gitee.com/li-zhuohengq/riscv-ai-adapter>
