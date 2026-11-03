# PPT阅读桥接

连接 LibreOffice Impress、阅读挥手模式和小K已有的 `SetReadingContext` 接口。
原始PPT按只读方式打开，禁用宏和外部链接更新，使用独立LibreOffice配置目录；不接管用户已有文档窗口。

```sh
systemctl --user start riscv-ai-reader.service
systemctl --user stop riscv-ai-reader.service
```

也可在应用菜单打开“多模态PPT阅读”。当前演示配置使用用户提供的openKylin介绍PPT。
右挥到下一页，左挥到上一页，回起始位置只复位。问“这一页主要讲了什么”时使用当前页原生文字。
手势服务独立运行，关闭阅读窗口不会停止手势识别。没有阅读窗口时不会退回全局键盘事件。
关闭放映窗口回到Impress编辑视图后，跟随实际选中页；关闭整个演示文稿会结束阅读会话并清理语音上下文。

实现通过独立UNO工作线程控制特定文档，不依赖键盘焦点。页码经过读取确认后再响应手势调用。
文本按实际选中页获取，包括分组形状和表格。语音使用当前页标题作为页面标题，文档名另保留在阅读器状态中。语音快照上限2000个UTF-16编码单元，每10秒续期；
新语音进程启动时立即重新发布。没有原生文字的页明确标记不可读取，不假造OCR结果。
轮询实际页码也涵盖鼠标或键盘手动换页。页面文本留在内存，状态文件只记页码、字符数和哈希。

会话D-Bus接口：`org.riscv.AIReader`，对象 `/org/riscv/AIReader`，接口 `org.riscv.AIReader1`。
`Navigate("left"/"right")`、`GotoPage(页码)`、`GetState()`、`Close()`。
没有网络监听端口。依赖系统已有 `python3-uno`、`python3-dbus-next` 和 LibreOffice Impress。

直接运行其他文件：

```sh
python3 reader.py --file /absolute/path/manual.pptx --title 手册
python3 -B -m unittest discover -p 'test_reader_core.py'
```

接口参考：[LibreOffice XSlideShowController](https://api.libreoffice.org/docs/idl/ref/interfacecom_1_1sun_1_1star_1_1presentation_1_1XSlideShowController.html)。

阅读动作在独立线程中串行执行，最多保留一个等待动作；过期动作不会自动重试。大型PPT冷页可能需要等待绘制完成，识别线程不会因此暂停。

## 当前部署

当前服务和菜单入口分别见仓库根目录`deploy/systemd/riscv-ai-reader.service`与本目录`packaging/riscv-ai-reader.desktop`。先独立启动手势，再打开阅读器；关闭阅读器不会停止手势服务。

普通对话使用0.6B，当前页总结路由到1.7B-Q4_0。阅读模型生成3线程/预填充4线程，CPU0–3、TCM关闭、ctx3072、batch/ubatch128；与主模型互斥驻留，空闲60秒卸载。模型轮换保留启动及运行期内存保护。

同页上下文和KV短时复用可减少等待，但首次加载及长页总结仍慢。当前页必须含可提取原生文字；本模块不执行OCR或VLM。精确数据应核对PPT原文。

测试：`python3 -B -m unittest discover -p 'test_reader_core.py'`；Office缓存测试位于`tests/`。
