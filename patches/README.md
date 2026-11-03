# 当前后端修改

本目录只保留当前K1方案采用的修改。原始第三方源码通过固定commit获取，不保留修改前源码副本。

| 子目录 | 当前内容 |
|---|---|
| `spacemit-llama/` | Q4_K精度修复、IME预填充/CPU逐token生成、K1 TCM分配修复、Q6 RVV点积与16路重排快路径 |
| `spacemit-tts/` | 当前Matcha CPU服务需要的构建桥接、缓存路径、线程等待及文本归一化适配 |

各目录的`overrides/`是修改后的完整文件，供评审阅读；`.patch`用于应用到`source-manifest.json`指定的第三方基线，清单保留修改前/后的哈希。应用程序会直接检查下载的基线文件，无需在仓库中保存旧文件。

用户算子优化中，当前采用的是第1项Q6_K宽乘加点积和第17项Q6_K 16x1 GEMV/GEMM重排快路径，已合并到`spacemit-llama/current-runtime.patch`及其完整源码。其余独立分支不属于当前运行方案，保留在作者自己的llama.cpp算子仓库，不复制到本仓库。

`spacemit-llama/tests`验证当前布局、精度、生命周期和Q6性能；测试是当前实现的验证代码。构建与应用步骤见[复现说明](../docs/REPRODUCE.md)。
