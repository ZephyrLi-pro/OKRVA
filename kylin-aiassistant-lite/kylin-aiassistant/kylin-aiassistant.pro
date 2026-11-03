QT += core gui printsupport sql dbus multimedia x11extras svg KGlobalAccel KWaylandClient concurrent websockets network qml quick

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets quickwidgets

CONFIG += c++11 link_pkgconfig

# Qt WebEngine embeds the KylinBot web administration UI. It is optional so
# the native QML chat and Unix-socket KylinBot client can build on riscv64.
WEBENGINE_TARGET_ARCH = $$QT_ARCH
isEmpty(WEBENGINE_TARGET_ARCH): WEBENGINE_TARGET_ARCH = $$QMAKE_TARGET.arch
isEmpty(WEBENGINE_TARGET_ARCH): WEBENGINE_TARGET_ARCH = $$QMAKE_HOST.arch

WEBENGINE_ENABLED = false
contains(WEBENGINE_TARGET_ARCH, "^(x86_64|amd64|aarch64|arm64)$") {
    WEBENGINE_ENABLED = true
}

equals(WEBENGINE_ENABLED, true) {
    QT += webengine webchannel
    DEFINES += KYLIN_AIASSISTANT_HAS_WEBENGINE
    message("Qt WebEngine enabled for $$WEBENGINE_TARGET_ARCH")
} else {
    message("Qt WebEngine disabled for $$WEBENGINE_TARGET_ARCH; native chat remains available")
}

# ccache 支持
# 默认禁用；通过 qmake CONFIG+=use_ccache 启用
# 启用后会使用 ccache 包裹编译器，显著加速增量编译
use_ccache {
    message("ccache is enabled")
    QMAKE_CC  = ccache $$QMAKE_CC
    QMAKE_CXX = ccache $$QMAKE_CXX
} else {
    message("ccache is disabled")
}

PKGCONFIG += gsettings-qt kysdk-qtwidgets glib-2.0 gmodule-2.0 kysdk-datacollect


# The following define makes your compiler Q_EMIT warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
#DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += /usr/include/KF5/KWindowSystem/
INCLUDEPATH += /usr/include/KF5/


LIBS += -lukui-log4qt -lKF5WindowSystem -lkysdk-qtwidgets -lX11 -lXtst -lxcb -lcmark -lSDL2
equals(WEBENGINE_ENABLED, true) {
    # Qt WebEngine -> libavcodec -> librav1e；Launchpad 需显式链接（soname 随版本变化：0.7=so.0，0.8=so.0.8）
    rav1e_libdir = /usr/lib/$$QMAKE_HOST.arch-linux-gnu
    exists($$rav1e_libdir/librav1e.so.0.8) {
        LIBS += -L$$rav1e_libdir -l:librav1e.so.0.8
    } else:exists($$rav1e_libdir/librav1e.so.0) {
        LIBS += -L$$rav1e_libdir -l:librav1e.so.0
    } else {
        warning("rav1e library not found in $$rav1e_libdir, link may fail")
        LIBS += -lrav1e
    }
}


TRANSLATIONS += translations/Internationalization_zh_CN.ts \
                translations/Internationalization_zh_HK.ts \
                translations/Internationalization_bo_CN.ts \
                translations/Internationalization_mn.ts \
                translations/Internationalization_ug.ts \
                translations/Internationalization_ky.ts \
                translations/Internationalization_kk.ts \
                translations/Internationalization_ms.ts \
                translations/Internationalization_th.ts \
                translations/Internationalization_vi.ts

# Generate translations in build
TRANSLATIONS_FILES =

qtPrepareTool(LRELEASE, lrelease)
for(tsfile, TRANSLATIONS) {
    qmfile = $$shadowed($$tsfile)
    qmfile ~= s,.ts$,.qm,
    qmdir = $$dirname(qmfile)
    !exists($$qmdir) {
        mkpath($$qmdir)|error("Aborting.")
    }
    command = $$LRELEASE -removeidentical $$tsfile -qm $$qmfile
    system($$command)|error("Failed to run: $$command")
    TRANSLATIONS_FILES += $$qmfile
}


SOURCES += \
    aiservicedialog.cpp \
    closemodewidget.cpp \
    cmainwnd.cpp \
    chatdatabase.cpp \
    docparse/customtoolbutton.cpp \
    documentreferencedatabase.cpp \
    commandtemplatelist.cpp \
    configmanager.cpp \
    custommessagebox.cpp \
    dbus/kadbusoperator.cpp \
    docparse/docparsethread.cpp \
    docparse/documentitemdelegate.cpp \
    docparse/documentitemwidget.cpp \
    docparse/documentlistmodel.cpp \
    docparse/documentlistview.cpp \
    docparse/kbdropfilebutton.cpp \
    docparse/paragraphmanager.cpp \
    docparse/replaceconfirmdialog.cpp \
    followupmanager.cpp \
    gatewaywebsocketmanager.cpp \
    unixsocketwebsocket.cpp \
    webapibridge.cpp \
    scheduled/kybotcronclient.cpp \
    scheduled/kybotcronsyncmanager.cpp \
    tasksessionmanager.cpp \
    global/documentreader.cpp \
    global/officetextextractor.cpp \
    global/pdftextextractor.cpp \
    global/filedialoghelper.cpp \
    global/kaglobal.cpp \
    global/voiceusingmanager.cpp \
    main.cpp \
    modelitemlistmodel.cpp \
    msgpane.cpp \
    readercontrol.cpp \
    readercontrolproxy.cpp \
    recordercontrol.cpp \
    recordercontrolproxy.cpp \
    setting/clickfixlabel.cpp \
    setting/commandinstructionmodel.cpp \
    setting/scribewordpage.cpp \
    systemchat.cpp \
    texteditmenuprovider.cpp \
    textselect/clipboard.cpp \
    textselect/mypushbutton.cpp \
    textselect/selectcommandlistmodel.cpp \
    textselect/selectcontrol.cpp \
    textselect/selection.cpp \
    textselect/selectmorecommand.cpp \
    voicechat/sdl2audioplayerthread.cpp \
    voice-kyling/aiballmanager.cpp \
    voice-kyling/aiballcontroller.cpp \
    voice-kyling/speecherrormessage.cpp \
    voice-kyling/speechrecognitioncontroller.cpp \
    window/ukuiwindowmanager.cpp \
    window/window.cpp \
    window/windowcontroller.cpp \
    chatmessage.cpp \
    x11windowhelper/x11windowhelper.cpp \
    notificationnotify.cpp

HEADERS += \
    aiservicedialog.h \
    closemodewidget.h \
    cmainwnd.h \
    chatdatabase.h \
    docparse/customtoolbutton.h \
    documentreferencedatabase.h \
    commandtemplatelist.h \
    configmanager.h \
    custommessagebox.h \
    dbus/kadbusoperator.h \
    docparse/docparsethread.h \
    docparse/documentitemdelegate.h \
    docparse/documentitemwidget.h \
    docparse/documentlistmodel.h \
    docparse/documentlistview.h \
    docparse/kbdropfilebutton.h \
    docparse/paragraphmanager.h \
    docparse/replaceconfirmdialog.h \
    followupmanager.h \
    gatewaywebsocketmanager.h \
    unixsocketwebsocket.h \
    webapibridge.h \
    scheduled/kybotcronclient.h \
    scheduled/kybotcronsyncmanager.h \
    tasksessionmanager.h \
    tasksession.h \
    global/documentcontent.h \
    global/documentreader.h \
    global/officetextextractor.h \
    global/pdftextextractor.h \
    global/filedialoghelper.h \
    global/kaglobal.h \
    global/voiceusingmanager.h \
    modelitemlistmodel.h \
    msgpane.h \
    notificationnotify.h \
    readercontrol.h \
    readercontrolproxy.h \
    recordercontrol.h \
    recordercontrolproxy.h \
    setting/clickfixlabel.h \
    setting/commandinstructionmodel.h \
    setting/scribewordpage.h \
    systemchat.h \
    texteditmenuprovider.h \
    textselect/clipboard.h \
    textselect/mypushbutton.h \
    textselect/selectcommandlistmodel.h \
    textselect/selectcontrol.h \
    textselect/selectionpopupguard.h \
    textselect/selection.h \
    textselect/selectmorecommand.h \
    voicechat/sdl2audioplayerthread.h \
    voice-kyling/aiballmanager.h \
    voice-kyling/aiballcontroller.h \
    voice-kyling/speecherrormessage.h \
    voice-kyling/speechrecognitioncontroller.h \
    window/ukuiwindowmanager.h \
    window/windowcontroller.h \
    windowhelper.h \
    chatmessage.h \
    x11windowhelper/x11windowhelper.h

RESOURCES += \
    res/msgpane.qrc \
    voice-kyling/voice-kyling.qrc

equals(WEBENGINE_ENABLED, true) {
    RESOURCES += \
        res/webkbot.qrc \
        res/webengine.qrc
}

CONFIG += link_pkgconfig
# When building without the kylin-ai SDK libraries, add CONFIG+=use_sdk_stubs to the
# qmake command line (or to a .qmake.conf file):
#
#   qmake CONFIG+=use_sdk_stubs
#
use_sdk_stubs {
    include($$PWD/stub/stub.pri)
    # Only link the packages that ARE available on this build host
    PKGCONFIG += kysdk-qtwidgets librsvg-2.0 kysdk-ukenv kysdk-alm
} else {
    PKGCONFIG += kysdk-qtwidgets librsvg-2.0 kyai-config kyai-assistant kysdk-coreai-speech kysdk-ukenv kysdk-ai-common kysdk-alm \
        kysdk-genai-nlp
}

#当前V11操作系统版本额外添加对kysdk-waylandhelper的支持及宏定义
PKGCONFIG += kysdk-waylandhelper
packagesExist(kysdk-ukuiwindowhelper) {
    PKGCONFIG += kysdk-ukuiwindowhelper
    DEFINES += UKUI_WINDOWHELPER_SUPPORT
} else {
    message("kysdk-ukuiwindowhelper unavailable; using Qt/X11 window fallback")
}

# 判断是否支持PDF模块
# 可通过 qmake CONFIG+=no_pdf 手动禁用（适用于无PDF依赖的编译平台）
# 申威架构（sw_64）同样不支持PDF模块
no_pdf | contains(QT_ARCH, sw_64) {
    message("PDF module disabled for architecture: $$QT_ARCH")
} else {
    # 使用系统 pdftotext 命令提取 PDF 文本（解决 libpoppler-qt5-dev 包不完整的问题）
    # PDF 文本提取通过 QProcess 调用 pdftotext 实现
    DEFINES += PDF_SUPPORT_ENABLED
    message("PDF_SUPPORT_ENABLED is defined for architecture: $$QT_ARCH, using pdftotext")
}

# 判断是否支持本地模型（仅在x86_64或arm64架构下支持，排除龙芯和申威）
# 龙芯架构：loongarch64, mips64el
# 申威架构：sw_64
# 明确排除不支持的架构
contains(QT_ARCH, x86_64) | contains(QT_ARCH, arm64) {
    message("LOCAL_MODEL_SUPPORT_ENABLED is defined for architecture: $$QT_ARCH")
    DEFINES += LOCAL_MODEL_SUPPORT_ENABLED
}

# 判断是否支持知识库（仅在x86_64或arm64架构下支持，排除龙芯和申威）
# 龙芯架构：loongarch64, mips64el
# 申威架构：sw_64
# 明确排除不支持的架构
contains(QT_ARCH, x86_64) | contains(QT_ARCH, arm64) {
DEFINES += KB_SUPPORT_ENABLED
message("KB_SUPPORT_ENABLED is defined for architecture: $$QT_ARCH")

# 知识库相关的 PKGCONFIG（仅在支持的架构下添加，且不使用stub时）
!use_sdk_stubs {
    PKGCONFIG += kysdk-advai-knowledgebase
}

# 知识库相关的源文件（仅在支持的架构下编译）
SOURCES += \
    kb/filelistmodel.cpp \
    kb/knowledgebasechat.cpp \
    kb/knowledgebasedatabase.cpp \
    kb/knowledgebasefilterproxymodel.cpp \
    kb/progressquerytask.cpp \
    kb/uploadtask.cpp \
    kb/knowledgebasespacemanager.cpp

HEADERS += \
    kb/filelistmodel.h \
    kb/knowledgebasechat.h \
    kb/knowledgebasedatabase.h \
    kb/knowledgebasefilterproxymodel.h \
    kb/progressquerytask.h \
    kb/uploadtask.h \
    kb/knowledgebasespacemanager.h
}

TARGET = kylin-aiassistant
TEMPLATE = app

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# installs
unix:!macx {
    isEmpty(PREFIX) {
      packaging {
        PREFIX = /usr
      } else {
        PREFIX = /usr/local
      }
    }

    DEFINES += APP_PREFIX=\\\"$$PREFIX\\\"

    target.path = $${PREFIX}/bin/

    guide.files = $$PWD/data/guide/kylin-aiassistant/
    guide.path = $${PREFIX}/share/kylin-user-guide/data/guide/

    qmfile.path = $${PREFIX}/share/kylin-aiassistant/translations/
    qmfile.files = $${TRANSLATIONS_FILES}

    # IK8E9A 临时方案：不再安装开机自启，避免登录后助手拉起 kytensor 占用大量内存。
    # 恢复自启时取消下面两行注释，并把 autostart 加回 INSTALLS。
    # autostart.files = kylin-aiassistant-autostart.desktop
    # autostart.path = /etc/xdg/autostart/

#    shortcut.files = kylin-aiassistant-shortcut.desktop
#    shortcut.path = /usr/share/applications


#    schemas.files = data/org.kylin.aiassistant.gschema.xml
#    schemas.path = /usr/share/glib-2.0/schemas

    contains(DEFINES, BUILD_KAIMING) {
        icon.path = $${PREFIX}/share/icons/hicolor/scalable
        icon.files = res/icon/AI-Writing.svg
    } else {
        icon.path = /usr/share/pixmaps
        icon.files = res/icon/AI-Writing.svg \
                     res/icon/AI-Writing.png
    }

    contains(DEFINES, BUILD_KAIMING) {
        service.files = data/org.ukui.kylin_aiassistant.service
    } else {
        service.files = debdata/org.ukui.kylin_aiassistant.service
    }

    service.path = $${PREFIX}/share/dbus-1/services


    desktopentry.path = $${PREFIX}/share/applications
    desktopentry.files = kylin-aiassistant.desktop

    plugin_schema.files = $$PWD/data/org.kylin.SettingsDaemon.plugins.media-keys.kylin-aiassistant.gschema.xml
    plugin_schema.path = $${PREFIX}/share/glib-2.0/schemas/

    doctemplate.files = res/html/custom-reference.docx
    doctemplate.path = $${PREFIX}/share/kylin-aiassistant/

    notification.files = res/kylin-aiassistant.json
    notification.path = /etc/ukui/ukui-notification/default-app/

    INSTALLS += target qmfile icon desktopentry plugin_schema doctemplate guide service notification

}

DISTFILES += \
    data/guide/kylin-aiassistant/zh_CN/image/会议助手入口.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/侧边栏模式.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/关于.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/切换模板-更多.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/划词悬浮条-1.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/划词悬浮条-右键菜单.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/划词悬浮条.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/历史对话清除1.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/图片处理.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/复制.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/左侧导航栏.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/帮助.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/开启新话题.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/开始菜单打开AI助手.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/录制页.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/托盘区图标.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/指令模板.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/文本对话.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/文档拖入.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/文生图.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/日程添加1.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/日程添加2.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/朗读.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/本地模型.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/模型配置.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/清空上下文.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/点赞点踩.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/用户昵称过长.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/系统控制.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/自选模型.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/视图模式.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/设置.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/语音输入.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/重新回答.jpg \
    data/guide/kylin-aiassistant/zh_CN/image/首页.jpg \
    data/guide/kylin-aiassistant/zh_CN/index.md \
    data/org.kylin.SettingsDaemon.plugins.media-keys.kylin-aiassistant.gschema.xml \
    data/org.kylin.aiassistant.policy
    data/org.ukui.kylin_aiassistant.service

FORMS +=
