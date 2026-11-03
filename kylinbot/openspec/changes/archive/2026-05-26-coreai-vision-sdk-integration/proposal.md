## Why

KylinBot 目前缺少图像理解类工具。麒麟 CoreAI Vision SDK（`libkysdk-coreai-vision`）已在系统中安装，提供 OCR 文字识别和主体分割两项端侧能力。将这两项能力以独立 tool 的形式接入，可让 agent 直接处理图片中的文字提取和前景主体提取，无需依赖云端服务。

## What Changes

- 新增 `kart-coreai-vision-sys` crate：对 `libkysdk-coreai-vision.so` 的 Rust FFI 绑定和安全封装，覆盖 OCR 和主体分割两组 API
- 新增 `ocr` tool：接受图片文件路径，返回识别出的文本内容及各行文本的边界框坐标
- 新增 `subject_segmentation` tool：接受图片文件路径和模式参数（portrait/general），返回主体分割后的前景 PNG 文件路径

## Capabilities

### New Capabilities
- `kart-coreai-vision-binding`: 麒麟 CoreAI Vision SDK 的 Rust 安全绑定层（OCR + 主体分割，动态加载、回调桥接）
- `ocr-tool`: OCR 文字识别工具，从图片中提取文本和位置信息
- `subject-segmentation-tool`: 主体分割工具，从图片中提取前景主体并保存为透明背景 PNG

### Modified Capabilities

（无现有 spec 需要修改）

## Impact

- **新增 crate**: `crates/kart-coreai-vision-sys/`（FFI 绑定）
- **修改 crate**: `kylinbot-tools`（新增 OcrTool、SubjectSegmentationTool）、`kylinbot-config`（新增工具配置段）
- **系统依赖**: 运行时需要 `libkysdk-coreai-vision.so`（通过 `libloading` 动态加载）
- **构建依赖**: `libkysdk-coreai-vision-dev` 提供头文件用于 bindgen
- **API 兼容**: 纯新增 tool，不影响现有 tool
