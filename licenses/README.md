# 许可证与来源索引

本包是多个项目的源码集合，不对整包统一改授MIT或宣称独占原创。保留各源码文件版权头、各项目许可证、NOTICE和作者信息。

| 组件 | 原有许可/归属材料 |
|---|---|
| 小K Lite | 基于麒麟AI助手；源码头包含KylinSoft版权及GPL-3.0-or-later；保留debian/copyright，附GPL-3全文。原debian/copyright信息不完整，不代替逐文件版权头 |
| KylinBot | 保留项目LICENSE-MIT、LICENSE-APACHE、NOTICE及上游贡献者信息 |
| ukui-air-gesture | 保留COPYING(GPL3)及源码头；OpenCV Zoo代码另保留Apache许可文件 |
| SpacemiT llama.cpp/GGML修改 | 基于vendor固定版本；原MIT LICENSE保留在本目录；修改后的完整文件保留原版权信息 |
| SpacemiT TTS修改 | 原Apache-2.0 LICENSE/NOTICE保留；4文件局部修改并未使整个组件变成本项目原创 |
| Adapter/独立工具/桥接 | 文件内已有SPDX按原样保留；未有根级许可证的自有代码，本包不擅自补成另一许可证。若比赛需要统一开源授权，应由作者另行决定适用范围 |
| 模型、ORT/EP、sherpa、Qt/LibreOffice | 不打包其二进制/权重；按原来源和发行许可获取 |

补丁同时包含少量原始基线文件供校验，其原许可不变。算子分支的原始仓库为`https://github.com/ZephyrLi-pro/llama.cpp`，详细commit见`optimizations/rvv/branches.json`。独立补丁是源代码成果，性能数字不作为许可证或原创范围依据。
