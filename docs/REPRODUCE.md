# 源码复现

本包以当前K1/Bianbu环境为参考，不会自动覆盖系统包、用户配置或运行服务。完整复现需要准备外部运行库、模型和开发包；模板中的`@HOME@`需要替换，systemd参考中的`%h`是用户home说明符。

## 1. 获取并校验源码

```sh
git clone git@github.com:ZephyrLi-pro/OKRVA.git
cd OKRVA
python3 tools/verify_bundle.py
```

## 2. 获取外部vendor源并应用本项目修改

在可联网、已安装git的Linux环境执行，目标目录必须不存在：

```sh
python3 tools/fetch_vendor.py llama /your/work/vendor-llama
sh tools/build_llama_backend.sh /your/work/vendor-llama /your/work/llama-build
```

该脚本校验固定commit及补丁基线的SHA，默认仅构建GGML共享库，不安装；本包此次已在K1完成全新object构建。需要独立server/quantize时，可在相同CMake目录另执行：

```sh
cmake --build /your/work/llama-build --parallel 2 --target llama-server llama-quantize
```

这两个额外target未在本次打包检查中完整重建，部署时需验证与系统工具版本/ABI一致。系统现有server可配合匹配版本的私有GGML库使用，不覆盖`/usr/lib`。运行配置详见Adapter模板。主模型采用修复后的IME预填充/CPU逐token生成；阅读模型单独关闭TCM并限制内存，不能把所有模型默认TCM常驻。

TTS组件被有意作为外部依赖，不在组件目录内复制整棵第三方依赖树。先填充构建脚本期望的位置，再应用补丁：

```sh
python3 tools/fetch_vendor.py tts riscv-ai-adapter/scripts/third_party/spacemit-tts
python3 tools/apply_backend_changes.py tts riscv-ai-adapter/scripts/third_party/spacemit-tts
sh riscv-ai-adapter/scripts/build_matcha_native.sh
```

需要ORT头文件/库、espeak-ng开发文件、CMake及C++17、vendor的FST依赖。保留`TTS_USE_FST=ON`、`USE_SPACEMIT_EP=OFF`。依赖下载由vendor构建定义管理；缺失时按其固定版本补齐。当前CPU Matcha不是早期失败的TTS EP加速路线。

ASR使用未经跟踪修改的官方组件：

```sh
python3 tools/fetch_vendor.py asr /your/work/model-zoo-asr
```

按该vendor的RISC-V README构建Python绑定，向`sensevoice_server.py`提供绑定的PYTHONPATH和SpacemiT ORT库。正式参数参考`deploy/systemd-reference/kylinbot-asr-sensevoice.service`。ASR bearer token由本机新建的环境文件提供，并与客户端设置保持一致；本包不包含旧token。

## 3. 构建本项目

- Adapter：`python3 -m pip install ./riscv-ai-adapter`，或在隔离venv中安装。核心是Python标准库；阅读器另用系统`python3-uno`、D-Bus等。
- 小K：按`kylin-aiassistant-lite/README.md`和真实`debian/control`安装开发依赖后，在该目录执行`debuild -b -us -uc`。RISC-V原生界面不导入QtWebEngine；仍有部分麒麟SDK链接，不能在纯净Bianbu上跳过所有SDK。React/Web Admin的生成`dist`未打包，支持该可选页的架构须按其package-lock构建前端。
- 文档CLI：在`kylin-aiassistant-lite/tools/document-reader-cli`执行`qmake reader-cli.pro`、`make -j1`；向文档MCP指定生成的可执行文件。
- KylinBot：遵循其README/Cargo.lock在RISC-V构建，保留所需Gateway/MCP/语音特性；例如在该项目执行`cargo build --release --locked`。本包保留完整源码和原有features，不凭猜测改变特性组合。
- KWS/VAD辅助程序：按`kylinbot/tools/bianbu-kws/README.md`和`tools/bianbu-vad`说明，链接匹配的sherpa C API；C源码已提供，不把二进制当源码。
- 手势：按`ukui-air-gesture`的CMake/README构建，另准备OpenCV、ORT Python与对应ONNX。摄像头要按真实设备校准。手势服务独立启动，阅读器按需打开。
- 系统和文档MCP：以桌面普通用户启动`system-tools`内脚本，设置本机Documents/Desktop根和reader-cli路径，接入KylinBot的sys/docs stdio服务器。不要放开任意shell/提权权限。

## 4. 配置与启动

`deploy/templates`保存本机有效配置的脱敏参考；`deploy/systemd-reference`保存主unit及drop-in的合并参考，包含历史覆盖顺序，不是直接复制到另一台机器的安装器。需要根据自己的稳定安装路径整理ExecStart、EnvironmentFile、库路径、模型路径、摄像头、演示文件路径；不要依赖本机历史实验目录。

首先单独验证ASR/TTS健康与模型；再启动Adapter和KylinBot，配置小K连接地址和新配对token；最后启用语音和手势。仅查看健康页不代表麦克风/扬声器/窗口已经通过。`provenance/running-service-commands.json`给出快照时实际进程命令，便于区分unit中被后续drop-in覆盖的旧命令。

各模型互斥驻留与内存保护保持启用：阅读ctx3072、batch128、ubatch128、parallel1，阅读生成3线程/预填充4线程，CPU0–3；按需模型空闲60秒卸载。不要为了少等待而把0.6B和1.7B无条件同时常驻。

## 5. 验证

先运行适配器单元测试（在`riscv-ai-adapter`内）：

```sh
PYTHONPATH=src python3 -B -m unittest discover -s tests -p 'test_*.py'
```

工具测试使用替身，详见各项目测试说明；不要将真实用户Documents用于破坏性测试。随后按五场景逐项人工验证实际副作用、当前页、答案、播报和历史。场景四应得到三个独立摘要文件；扫描页不应假称有原生文字。场景五必须从专用阅读器打开自己的PPT。
