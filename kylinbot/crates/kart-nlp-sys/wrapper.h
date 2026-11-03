// wrapper.h - 用于 bindgen 生成 Rust 绑定
// 包含所有需要绑定的 SDK 头文件

// 首先包含必要的 C 标准库头文件
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// SDK 头文件 (使用系统路径 /usr/include/kylin-ai/)
#include <genai/text/chat.h>
#include <genai/text/result.h>
#include <genai/text/config.h>
#include <genai/text/chatmessage.h>
#include <genai/text/modelinfo.h>
#include <genai/text/error.h>
#include <genai/text/toolchoice.h>
