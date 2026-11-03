# ukui-air-gesture

## 介绍
应用于UKUI桌面环境的隔空手势交互组件


## 安装教程
### 通过debuild编包并安装
1. 安装编包工具
```
sudo apt-get install devscripts
```
2. 编包
```
debuild
```
3. 安装deb包
```
dpkg -i ../<.deb包>
```
### 通过make install安装
1. 检查依赖
```
dpkg-checkbuilddeps
```
2. 安装依赖
```
sudo apt-get install <提示缺少的依赖>
```
3. 编译
```
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib/<架构目录>
make
```
4. 安装
```
sudo make install
```

## 使用说明
直接执行：
```
ukui-air-gesture
```
