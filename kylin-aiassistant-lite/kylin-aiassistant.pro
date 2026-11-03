TEMPLATE = subdirs

SUBDIRS += kylin-aiassistant \
           voice-assistant

message("Building for architecture " $$QT_ARCH)


# 目前只有X86架构个ARM结构支持本地模型。所以其他架构不应该安装AI助手的文管右键插件

contains(QT_ARCH, x86_64) | contains(QT_ARCH, arm64) {
    message("Building for x86_64 or ARM64 architecture")
    SUBDIRS += peony-menu-plugin-kylin-aiassistant
}
