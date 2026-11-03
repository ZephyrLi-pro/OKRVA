## Why

当前文生图功能仅依赖 fal.ai 云端服务，存在网络延迟、隐私风险和费用问题。麒麟 GenAI Vision SDK（`libkysdk-genai-vision`）提供端侧/私有化部署的文生图能力，需要将其接入现有架构作为平行的图像生成后端，使系统在离线/内网环境下也具备文生图能力。

## What Changes

- 新增 `kart-vision-sys` crate：对 `libkysdk-genai-vision.so` 的 Rust FFI 绑定和安全封装
- 在 `kylinbot-channels` 中新增 Kart Vision 图像生成 provider 实现
- 扩展 `kylinbot-config` 配置 schema，支持 Kart Vision 后端配置
- 现有 `ImageGenTool` 增加 Kart 后端路由，支持根据配置选择 fal.ai 或 Kart SDK

## Capabilities

### New Capabilities
- `kart-vision-binding`: 麒麟文生图 SDK 的 Rust 安全绑定层（FFI、动态加载、回调桥接）
- `kart-image-gen-provider`: 基于麒麟 SDK 的图像生成 provider，平行于现有 fal.ai provider

### Modified Capabilities

（无现有 spec 需要修改）

## Impact

- **新增 crate**: `crates/kart-vision-sys/`（FFI 绑定）
- **修改 crate**: `kylinbot-channels`（新增 provider）、`kylinbot-config`（配置扩展）、`kylinbot-tools`（路由逻辑）
- **系统依赖**: 运行时需要 `libkysdk-genai-vision.so`（通过 `libloading` 动态加载，feature-gate 保证无 SDK 环境可编译）
- **构建依赖**: 开发时需要 `libkysdk-genai-vision-dev` 提供头文件用于 bindgen
- **API 兼容**: 对外 tool 接口不变，新增 `provider` 字段可选择后端
