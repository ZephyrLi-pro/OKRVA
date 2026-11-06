# 16GB K1 PPT概括性能复现

本目录对应技术报告第4章的16GB K1测量，记录日期为2026年11月6日。材料包括固定输入、完整请求参数、三次模型请求的原始计时与回答，以及另一条Gateway请求的CPU核心组记录。两组请求的处理范围不同，分别统计。

## 文件

| 文件 | 内容 |
|---|---|
| `request.json` | 完整PPT页面原生正文、系统提示词、用户提问和采样参数 |
| `measurements.json` | 三次Adapter模型请求的原始timings、usage、完整回答和模型内存记录 |
| `benchmark.py` | 标准库SSE测速与离线统计复算工具，不修改服务配置 |
| `core_usage.py` | 标准库Linux核心组采样器，输出采样及整段统计所需的计数差值 |
| `gateway-cpu.json` | 独立Gateway PPT概括请求的原始汇总结果 |
| `gateway-cpu-samples.jsonl` | 上述Gateway请求的71条约1秒CPU样本 |

项目构建与服务部署见[部署说明](../../../deploy/INSTALL.md)；后端固定来源及补丁见[后端说明](../../../patches/README.md)。在已部署同款环境中运行本目录脚本；本目录不包含模型权重或用户凭据。

## 测量条件

- 设备：16GB SiPEED LPi3A K1，8核SpacemiT X60；Linux可见内存16266184 KiB，两组CPU采样频率均为1.6GHz。
- 系统：Bianbu 2.3.5、Linux 6.6.63、LXQt Wayland。
- 模型：Qwen3-1.7B-Q4_0，SHA256为 `81ccd6caca30f2a1d0ad1401bd0e3c453049ed7b41a4f327ff9447bf3fc28d25`。
- 模型配置：生成3线程、预填充4线程，工作线程放在CPU 0—3；ctx3072，batch128，ubatch128，parallel1，使用项目GGML优化库。
- 后台ASR、TTS与手势服务保持运行；测量时不做前台收音、回答播放或录屏，不切换服务配置。
- 首次请求前阅读模型尚未驻留，随后两次在模型已就绪状态下执行。每次输入353 token、输出42 token；`cache_prompt=false`，三次`cache_n`及`cached_tokens`均为0。
- 其余参数以`request.json`为准，包括`stream=true`、`max_tokens=128`、`temperature=0`、`seed=42`及关闭思考模式。PPT内容通过原生正文提供给语言模型。

## Adapter模型请求结果

计时从客户端发出请求到SSE文本事件：首字为第一个非空`content`，完整文字为流结束。该计时覆盖Adapter与模型处理，不包含唤醒、ASR、Gateway Agent规划和TTS播放。首次请求的首字和总耗时包含模型按需启动。

| 指标 | 首次未驻留请求 | 后两次模型就绪请求均值 |
|---|---:|---:|
| 持续生成 token/s | 2.565 | 2.582 |
| 预填充 token/s | 22.339 | 22.054 |
| 首字 秒 | 26.48 | 16.09 |
| 完整文字 秒 | 42.85 | 32.36 |

生成吞吐按每条记录的`predicted_n / predicted_ms × 1000`计算，预填充按`prompt_n / prompt_ms × 1000`计算。模型就绪均值取第2、3次各指标的算术平均，先用原始值运算，再舍入。记录中保留全部回答与完成状态。

连续三次请求后，实际`llama-server`子进程的Linux `VmHWM`为2412.1015625 MiB，报告取2412.10 MiB。它是该模型进程驻留内存的高水位，不是三次RSS的平均值，也不是整机内存。测速脚本查找实际`llama-server`子进程，不将Python管理包装进程当作模型进程。

### 离线复算

无需模型或运行中的服务，仅需Python 3：

```bash
cd docs/reproducibility/reading-summary-16gb-20261106
python3 benchmark.py --summarize measurements.json
```

### 重新测速

确认已部署`reading-summary`模型别名。关闭正在执行的前台问答，保持相同线程、模型、后台服务与录屏条件。等阅读模型按既定空闲策略卸载后执行，脚本会记录每轮开始时模型是否驻留；它不会强制卸载或重启服务。

```bash
python3 benchmark.py --output measurements.local.json
```

默认使用本机Adapter根地址`http://127.0.0.1:18080`，可通过`--base-url`指定实际配置；该地址是脚本参数，不是需要对外开放的服务要求。若接口启用Bearer鉴权，通过`--bearer-token-env`指定已有凭据的环境变量名，凭据不会保存到结果。结果文件必须不存在，以免覆盖原始记录。

脚本连续请求三次，保留实际timings、usage、回答、完成状态、首字和总耗时。它检查服务是否有活动请求，并区分未驻留与已就绪请求。模型、分词器、后端或提示词模板改变可能影响token数量，比较时核对这些条件。脚本只发出正常推理请求并读取本机进程状态，不更改线程或模型服务配置。

## Gateway请求的CPU核心组记录

`gateway-cpu.json`来自另一条实际Gateway PPT概括请求：完成时长72.048秒，前4核平均忙碌率80.745%，后4核33.754%，后4核1秒样本峰值78.372%，有效样本71个。它包含Gateway及模型处理阶段，不能把72.048秒与前面的32.36秒当作同一请求的不同阶段或直接比较。

前4核指CPU 0—3，后4核指CPU 4—7，每组满载分别为100%。CPU忙碌率不能当作IME矩阵单元内部利用率。读取`/proc/stat`前八项时间计数，busy为total扣除idle和iowait。原汇总的整段均值按请求起止计数差值加权；1秒样本用来观察过程与峰值，不用其算术平均代替整段均值。

在设备上启动采样器，再在另一个终端或小K界面执行PPT任务，任务完成后在采样终端按Ctrl+C停止：

```bash
python3 core_usage.py > core-usage.local.jsonl
```

脚本逐行输出`sample`，停止时输出`summary`。每行含两个核心组的`busy_percent`、`busy_ticks`和`total_ticks`；整段统计为 `100 × Σbusy_ticks / Σtotal_ticks`。为使计时覆盖范围一致，应将开始和停止与任务边界对齐，并记录是否包含语音采集、播报或录屏。脚本不读取用户文件或修改服务。
