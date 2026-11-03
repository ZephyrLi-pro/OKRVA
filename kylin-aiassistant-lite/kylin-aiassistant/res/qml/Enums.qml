import QtQuick 2.12

// 存储 “仅前端” 使用的枚举值，
Item {
    property var uploadedFileStatus: Object.freeze({
        NONE: 0,    // 控制前端，不显示
        PREVIEW: 1, // 显示预览区
        SENT: 2     // 显示挂载区
    })
}
