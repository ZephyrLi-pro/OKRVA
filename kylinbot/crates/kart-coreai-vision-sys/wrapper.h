// wrapper.h - 用于 bindgen 生成 Rust 绑定
// 包含 CoreAI Vision SDK 头文件（OCR + 主体分割）

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// SDK 头文件 (使用系统路径 /usr/include/kylin-ai/)
#include <coreai/vision/types.h>
#include <coreai/vision/error.h>
#include <coreai/vision/config.h>
#include <coreai/vision/textrecognition.h>
#include <coreai/vision/textrecognitionresult.h>
#include <coreai/vision/subjectsegmentation.h>
#include <coreai/vision/subjectsegmentationresult.h>
#include <coreai/vision/subjectsegmentationconfig.h>
