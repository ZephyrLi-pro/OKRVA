# ukui-air-gesture：当前K1/Bianbu手势方案

当前以独立Qt服务运行，使用OpenCV Zoo手掌/关键点ONNX模型和设备的ONNX Runtime Python绑定。没有使用Python MediaPipe、JAX或UKUI控制中心插件。

## 当前行为

- 使用`deploy/systemd/ukui-air-gesture.service`指定的内置摄像头。
- `AIR_GESTURE_PROFILE=reading`采用已校准的左右挥手和中心复位；`AIR_GESTURE_READING_TARGET=reader`定向调用PPT阅读器。
- 右挥下一页，左挥上一页；回到中间只复位。
- 阅读器模式：中央张开手停稳后握拳保持约1.2秒返回桌面；重新语音打开PPT后恢复翻页。legacy握拳截图不变。
- 识别到手显示桌面小手图标，移开后隐藏；可按需启用完整画面校准。
- 手势与PPT服务独立启停，阅读器未打开时不会自动向其他窗口发送翻页按键。

## 构建和依赖

原生程序构建依赖Qt5 Core/Widgets/DBus、libevdev、CMake和C++编译器。运行使用发行版`python3-opencv`、`python3-numpy`、`python3-dbus-next`和`python3-spacemit-ort`。Python绑定由设备厂商/系统包提供，不要求安装PyPI版MediaPipe/JAX。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build --parallel 1
```

安装前把`MODELS.sha256`指定的两份ONNX放到`app/python/`；模型外部获取，不提交权重。CMake安装规则把它们放到共享models目录。构建完成后按需要执行`cmake --install build`，安装到系统目录时需对应权限。Debian编包以当前`debian/control`的riscv64依赖为准。

程序入口和相关模块位于`app/`；所有当前安装的Python模块继续保留。`virtual_camera.py`也提供当前读取物理摄像头所需的发现函数，不应仅因文件名删除。当前K1直接使用物理摄像头，不加载v4l2loopback，也不调用旧的特权摄像头重绑脚本。

根目录[`deploy/systemd/`](../deploy/systemd/)保存当前服务配置。详细使用见[阅读模式](READING_MODE.md)。

## 验证

```sh
python3 -B -m unittest discover -s tests -p 'test_*.py'
```

测试不会开启摄像头或执行系统控制；真人挥手、实际页码及小手图标还需配合现场验证。
