# LibreOffice Impress语音与手势桥接

连接 LibreOffice Impress、阅读挥手模式和小K已有的 `SetReadingContext` 接口。
直接使用正式的LibreOffice 24.8 Impress、持久用户配置和普通可编辑窗口；Python仅在后台提供UNO/D-Bus控制及当前页上下文，不另建只读查看器。打开文档时禁用宏和外部链接更新。

```sh
systemctl --user start riscv-ai-reader.service
systemctl --user stop riscv-ai-reader.service
```

可直接在原有LibreOffice Impress打开PPT；菜单“LibreOffice Impress（语音与手势）”启动同一应用的后台控制桥。后台桥以无文件方式懒启动，仅从自己的受控状态恢复最近真实打开路径为closed上下文；首次收到明确打开请求后才加载Impress。
右挥到下一页，左挥到上一页，回起始位置只复位。问“这一页主要讲了什么”时使用当前页原生文字。
手势服务独立运行，关闭Impress文档不会停止手势识别。没有可操作的Impress文档时，不会退回全局键盘事件。停止后台桥只清理语音上下文，不关闭用户的LibreOffice或演示文稿。
放映、普通编辑视图和备注视图均跟随真实当前页；用户在Impress中切换演示文稿时更新绑定会话与上下文。取得当前页快照后，后续翻页不会把生成的讲稿保存到新页面。

实现通过独立UNO工作线程控制特定文档，不依赖键盘焦点。页码经过读取确认后再响应手势调用。
文本按实际选中页获取，包括分组形状和表格。语音使用当前页标题作为页面标题，文档名另保留在后台桥状态中。语音快照上限2000个UTF-16编码单元，每10秒续期；
新语音进程启动时立即重新发布。没有原生文字的页明确标记不可读取，不假造OCR结果。
轮询实际页码也涵盖鼠标或键盘手动换页。页面文本留在内存，状态文件只记页码、字符数和哈希。

会话D-Bus接口：`org.riscv.AIReader`，对象 `/org/riscv/AIReader`，接口 `org.riscv.AIReader1`。
`Navigate("left"/"right")`、`GotoPage(页码)`、`GetState()`、`OpenPresentation(路径)`、`GetSlideSnapshot()`、`SaveSpeakerNotes(会话,页码,正文哈希,备注)`、`Close()`。其中`Close()`仅断开后台控制，不关闭用户文档。
`GetState()`附带`presentation_visible`、`visibility_source`和`visibility_checked_at`。后台桥按真实受管窗口的完整标题与`libreoffice-impress` app_id精确查询Wayland；标题须唯一，命令限时并短暂缓存。时间戳使用本机monotonic秒，标记查询开始时刻；未知、无标题或未确认时不报告可见。手势返回桌面后，手动恢复该PPT窗口或语音重新打开均可通过新的可见性确认解除暂停，隐藏窗口不会因旧缓存或会话变化而恢复翻页。
没有网络监听端口。依赖系统已有 `python3-uno`、`python3-dbus-next`、LibreOffice Impress及`/usr/bin/wlrctl`。Wayland下原生窗口恢复和返回桌面使用`wlrctl`，并读取合成器窗口状态确认；GTK3的UNO `IsMinimized`不能作为合成器最小化状态的可靠依据。部署基础依赖包含该工具，`doctor`会检查其文件及动态库。

后台桥懒启动或显式打开文件：

```sh
python3 reader.py
# 显式指定文件时才打开该演示文稿
python3 reader.py --file /absolute/path/manual.pptx --title 手册
python3 -B -m unittest discover -p 'test_reader_core.py'
```

接口参考：[LibreOffice XSlideShowController](https://api.libreoffice.org/docs/idl/ref/interfacecom_1_1sun_1_1star_1_1presentation_1_1XSlideShowController.html)。

阅读动作在独立线程中串行执行，最多保留一个等待动作；过期动作不会自动重试。大型PPT冷页可能需要等待绘制完成，识别线程不会因此暂停。

## 当前部署

内部服务名保留`riscv-ai-reader.service`，服务与菜单配置分别见仓库根目录`deploy/systemd/riscv-ai-reader.service`和本目录`packaging/riscv-ai-reader.desktop`。先独立启动手势，再启动Impress后台控制桥。服务使用`KillMode=process`，停止桥不终止普通LibreOffice进程。

正式入口默认是`~/.local/bin/libreoffice-24.8`，UNO管道默认`rvai_trial_248_1000`（为复用原实例保留内部名称）。服务使用`RVAI_LIBREOFFICE_LAUNCHER`和`RVAI_IMPRESS_PIPE`明确选择；连接后读取实际进程版本，拒绝低于24.8的实例。Impress菜单及PPT/PPTX/ODP默认关联使用该入口，系统Writer/Calc仍使用各自原有入口。大型演示文稿冷启动可超过两分钟，服务启动期限为300秒，PPT MCP工具期限为600秒。

可说“打开PPT”“打开刚才那个PPT”或指定文档、桌面中的PPT名称，然后说“根据当前页生成演讲备注并保存”。这些句子是意图示例：完整PPT工具目录可用时，规划模型始终看到PPT能力，按整句语义选择打开或备注保存动作及参数；执行阶段验证参数出处、快照绑定和实际回执。泛指打开只用最近真实打开记录，包括已关闭文档；无有效历史或历史文件已不存在时提示先打开一份PPT。正式服务启动不预先选择或打开演示文件。PPT MCP查找本地文件、读取真实当前页，1.7B模型生成讲稿，再通过UNO追加到该页备注区并保存。保留已有备注，直接保存，不创建备份；保存后回读磁盘PPTX并显示普通大幻灯片及底部备注栏。不默认切到备注整页视图。备注保存目前仅支持PPTX；PPT/ODP仍可打开与翻页。

普通对话使用0.6B，当前页总结路由到1.7B-Q4_0。阅读模型生成3线程/预填充4线程，CPU0–3、TCM关闭、ctx3072、batch/ubatch128；与主模型互斥驻留，空闲60秒卸载。模型轮换保留启动及运行期内存保护。

同页上下文和KV短时复用可减少等待，但首次加载及长页总结仍慢。当前页必须含可提取原生文字；本模块不执行OCR或VLM。精确数据应核对PPT原文。

测试：`python3 -B -m unittest discover -p 'test_reader_core.py'`；Office缓存测试位于`tests/`。
