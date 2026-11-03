// 单例，管理模型切换按钮、深度思考按钮、联网搜索按钮的共享状态
pragma Singleton
import QtQuick 2.12
import Qt.labs.settings 1.0

QtObject {
    id: buttonStateManager

    property bool deepThinkingisActive: false  // 深度思考是否选中
    property bool deepThinkingEnabled: false  // 深度思考是否支持
    property bool deepThinkingSwitched: false // 深度思考是否可切换
    property bool isDeepThinking: false  // 标志当前是否深度思考

    property bool webSearchisActive: false  // 联网搜索是否选中
    property bool webSearchEnabled: false  // 联网搜索是否支持
    property var storageWebSearchEnabled: null  //暂存联网搜索按钮的状态，用于置灰后的恢复
    property bool isWebSearch: false  // 标志当前是否联网搜索


    property string currentModel: msgPane.getNLPCurModel() // 当前模型
    property string selectedModel: currentModel // 下拉框选中的模型
    property string modelButtonIcon: "" // 模型切换按钮图标
    property bool isCurrentModelDeleted: !msgPane.isCurrentModelAvailable() // 标志当前选中的模型是否被删除了

    property int chatMode: 1  // 0=快问快答, 1=智能体
    property bool isTaskMode: true  // 是否处于智能体

    // 添加Settings以记录用户选择，便于重启应用恢复按钮状态
    property var _settings: Settings {
        category: "ButtonState"

        property alias deepThinkingisActive: buttonStateManager.deepThinkingisActive
        property alias isDeepThinking: buttonStateManager.isDeepThinking
        property alias webSearchisActive: buttonStateManager.webSearchisActive
        property alias isWebSearch: buttonStateManager.isWebSearch
        property alias modelButtonIcon: buttonStateManager.modelButtonIcon
        property alias chatMode: buttonStateManager.chatMode
        property alias isTaskMode: buttonStateManager.isTaskMode
    }
}
