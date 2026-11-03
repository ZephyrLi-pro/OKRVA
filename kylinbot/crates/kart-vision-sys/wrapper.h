// wrapper.h - 用于 bindgen 生成 Rust 绑定
// 包含所有需要绑定的 SDK 头文件

// 首先包含必要的 C 标准库头文件
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// SDK 头文件 (使用系统路径 /usr/include/kylin-ai/)
#include <genai/vision/image.h>
#include <genai/vision/imageconfig.h>
#include <genai/vision/imageresult.h>
#include <genai/vision/imageformat.h>
#include <genai/vision/error.h>
#include <common/error.h>