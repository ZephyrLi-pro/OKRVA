// wrapper.h - 用于 bindgen 生成 Rust 绑定
// 包含所有需要绑定的 SDK 头文件

// 首先包含必要的 C 标准库头文件
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// SDK 头文件 (使用系统路径 /usr/include/kylin-ai/)
#include <coreai/speech/synthesizer.h>
#include <coreai/speech/recognizer.h>
#include <coreai/speech/config.h>
#include <coreai/speech/audioconfig.h>
#include <coreai/speech/result.h>
#include <coreai/speech/audiodatastream.h>
#include <common/error.h>