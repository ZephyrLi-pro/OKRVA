/*
 * Copyright (C) 2020, KylinSoft Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "cmainwnd.h"
#include "msgpane.h"
#include "scheduled/kybotcronclient.h"
#include "scheduled/kybotcronsyncmanager.h"
#include "texteditmenuprovider.h"
#include "global/kaglobal.h"
#include "global/filedialoghelper.h"
#include "configmanager.h"
#include "webapibridge.h"
#include "global/voiceusingmanager.h"
#ifdef KB_SUPPORT_ENABLED
#include "kb/filelistmodel.h"
#include "kb/knowledgebasespacemanager.h"
#endif

#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <QTranslator>
#include <QLibraryInfo>
//窗口显示在屏幕中心
#include <QWidget>
#include <QCommandLineParser>
#include <QScreen>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QSqlDatabase>
#include <QSqlError>
#include <QStandardPaths>
#include <QtQml>
#include <QSessionManager>
#include <QtQuickControls2/QtQuickControls2>
#include <QElapsedTimer>
#include <QProcess>
#include <QThread>
#include <QAbstractEventDispatcher>
#include <QtGlobal>
#ifdef KYLIN_AIASSISTANT_HAS_WEBENGINE
#include <QtWebEngine/QtWebEngine>
#endif
#include <QDebug>

#include <windowmanager/windowmanager.h>
#include <kylin-ai/common/common.h>
#include <aiservicedialog.h>
#include <singleapplication.h>
#ifdef KB_SUPPORT_ENABLED
#include <kb/knowledgebasefilterproxymodel.h>
#endif
#include "window/windowcontroller.h"
#include "x11windowhelper/x11windowhelper.h"
#include "textselect/selectcontrol.h"
#include "textselect/selectcommandlistmodel.h"
#include "setting/commandinstructionmodel.h"
#include "readercontrol.h"
#include "readercontrolproxy.h"
#include <kysdk/diagnosetest/libkydatacollect.h>
#include "recordercontrol.h"
#include "recordercontrolproxy.h"
#include "dbus/kadbusoperator.h"

const QString CONFIGPATH = "/.config/kylin-aiassistant/kylin-aiassistant.conf";
const QString BOOTCMDLINE = "/proc/cmdline";
const QString INSTALLEREXITSCOMMAND = " ps aux |grep kylin-os-installer | grep -v grep";

QQmlApplicationEngine *g_qmlEngine = nullptr;

QStringList translationsPaths() {
    QString binaryPath = QFileInfo(qApp->applicationDirPath())
            .absoluteFilePath();

    QString trPath = QDir::toNativeSeparators(binaryPath + "/translations") ;
#if defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
    return QStringList()
            << QStringLiteral(APP_PREFIX) + "/share/kylin-aiassistant/translations"
            << trPath
            << QStringLiteral("/usr/share/kylin-aiassistant/translations")
            << QStringLiteral("/usr/local/share/kylin-aiassistant/translations");
#elif defined(Q_OS_WIN)
    return QStringList()
            << trPath;
#endif
}

bool isContainsLive(){
    // 打开/proc/cmdline文件
    QFile file(BOOTCMDLINE);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    // 读取文件内容
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    // 使用正则表达式精确匹配完整参数，避免部分匹配
    // 匹配规则：参数前后为空格或字符串开头/结尾
    QRegularExpression regex(QStringLiteral("(^|\\s)%1(\\s|$)").arg(QRegularExpression::escape("boot=casper")));
    bool isExit = regex.match(content).hasMatch();
    KyInfo() << "当前/proc/cmdline中是否存在boot=casper字段" << isExit;
    return isExit;
}

bool isKylinInstallerRunning(){
    QProcess process;
    // 执行命令，使用/bin/sh解析管道等特殊符号
    process.start("/bin/sh", QStringList() << "-c" << INSTALLEREXITSCOMMAND);
    // 等待命令执行完成，超时时间设置为3秒
    process.waitForFinished(3000);

    // 读取标准输出和标准错误
    QString output = process.readAllStandardOutput();
    QString error = process.readAllStandardError();

    // 如果有错误信息，将错误信息也包含到结果中
    if (!error.isEmpty()) {
        output += "\nError: " + error;
    }
    KyInfo() << "current is Installer exits result:" << output;
    return !output.trimmed().isEmpty();
}


static void initDataDir()
{
    QString confFile = QDir::homePath() + CONFIGPATH;
    QSettings settings(confFile, QSettings::IniFormat);
    settings.setIniCodec("utf8");

    QString storedDirPath = settings.value("directories/baseDir", "").toString();

    QString dirPath;
    if (storedDirPath.isEmpty()) {
        // 使用默认路径
        QString docPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        dirPath = docPath + "/kylin-aiassistant";

        settings.setValue("directories/baseDir", dirPath);
        KyInfo() << "使用默认路径" << dirPath;
    } else {
        // 使用存储的路径
        dirPath = storedDirPath;
        KyInfo() << "使用存储的路径" << dirPath;
    }

    QDir dir;
    if (!dir.exists(dirPath)) {
        if (!dir.mkpath(dirPath)) {
            KyInfo() << "创建助手目录失败";
            return;
        } else {
            KyInfo() << "创建助手目录成功 dirPath === " << dirPath;
        }
    }
    Globals::defaultPath = dirPath;

    QString meetingPath = dirPath + "/meeting";
    if (!dir.exists(meetingPath)) {
        if (!dir.mkpath(meetingPath)) {
            KyInfo() << "创建会议目录失败";
            return;
        } else {
            KyInfo() << "创建会议目录成功 dirPath === " << meetingPath;
        }
    }

    QString readPath = dirPath + "/.read";
    if (!dir.exists(readPath)) {
        if (!dir.mkpath(readPath)) {
            KyInfo() << "创建朗读目录失败";
            return;
        } else {
            KyInfo() << "创建朗读目录成功 dirPath === " << readPath;
        }
    }

    KyInfo() << "所有文件和目录初始化成功";
    return;
}

int main(int argc, char *argv[])
{
    // Qt 5.12 WebEngine:部分aarch64/libGL下Chromium GPU初始化glGetString段错误
#ifdef KYLIN_AIASSISTANT_HAS_WEBENGINE
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    if (!qEnvironmentVariableIsSet("QTWEBENGINE_CHROMIUM_FLAGS")) {
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", QByteArrayLiteral("--disable-gpu"));
    }
#endif
#endif



    // wayland适配
    QString env = qgetenv("XDG_SESSION_TYPE");
    if(env == "wayland"){
        isWayland = true;
    }

    // 修改分辨率的显示
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    // Qt WebEngine is optional. Keep its headers and initialization out of
    // riscv64 builds, where Qt WebEngine/Chromium is unavailable.
#ifdef KYLIN_AIASSISTANT_HAS_WEBENGINE
    QtWebEngine::initialize();
#endif

    QtSingleApplication app("aiassistant", argc, argv);

#ifndef QT_DEBUG
    // The logger creates Qt timers internally, so initialize it only after
    // QApplication has installed the main thread event dispatcher.
    initUkuiLog4qt("kylin-aiassistant");
#endif

    QElapsedTimer allTimer;
    QElapsedTimer t;
    allTimer.start();
    t.start();

    qRegisterMetaType<QMap<int,QString>>("QMap<int,QString>");
    qRegisterMetaType<InnerMessage>("InnerMessage");
    // AI助手不跟随主题切换，只采用浅色主题
    qApp->setProperty("useLightTheme", true);
    qApp->setStyle("ukui");

    // 发言人重命名后报错 QQuickWidget cannot be used as a native child widget. Consider setting Qt::AA_DontCreateNativeWidgetSiblings
    app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    QCommandLineParser parser;
    parser.setApplicationDescription("kylin-aiassistant");
    parser.addHelpOption();

    QCommandLineOption silenceOption(QStringList() << "s" << "silence",
            QCoreApplication::translate("main", "Start silently."));
    QCommandLineOption fileOption(QStringList() << "f" << "file",
                QCoreApplication::translate("main", "Start with send file."));

    parser.addOption(silenceOption);
    parser.addOption(fileOption);

    parser.process(app);

    const QStringList args = parser.positionalArguments();

    bool silence = parser.isSet(silenceOption);
    bool isFile = parser.isSet(fileOption);

    if (app.isRunning()) {
        if (isFile) {
            app.sendMessage(args.first(), 1000);
        } else if (!silence) {
            // 非首次启动行为为激活AI助手，弹出AI助手的UI界面
            app.sendMessage("running" , 4000);
        }

        return 0;
    }
    KyInfo() << "****** 环境变量，命令行处理 执行时间： " << t.elapsed();
    t.start();

    /* 加载翻译文件 */
    QString kylinAiAssistantTransPath = "/usr/share/kylin-aiassistant/translations";
    QString qtTransPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
    QString locale = QLocale::system().name();

    KyInfo()<<"kylinAiAssistantTransPath"<<kylinAiAssistantTransPath;
    KyInfo()<<"qtTransPath"<<qtTransPath;
    KyInfo()<<"locale"<<locale;

    QTranslator trans_global , trans_qt;
    QTranslator trans_SDK;

    if (!trans_global.load(QLocale(), "Internationalization", "_", kylinAiAssistantTransPath)) {
        KyInfo() << "Load global translations file" <<QLocale() << "failed!";
    } else {
        app.installTranslator(&trans_global);
        KyInfo()<<"success!";
    }

    if (!trans_qt.load(QLocale(), "qt", "_", qtTransPath)) {
        KyInfo() << "Load qt translations file" <<QLocale() << "failed!";
    } else {
        app.installTranslator(&trans_qt);
    }

    QString path = QString(":/translations/gui_%1.qm").arg(locale);
    if (trans_SDK.load(path)) {
        app.installTranslator(&trans_SDK);
    }

    // The Lite/RISC-V port can use KylinBot Gateway as its task backend even
    // when the legacy kylin-ai-subsystem package is unavailable on the distro.
    // Keep the legacy version checks when that subsystem exists, but do not
    // block the whole native QML application when a healthy Gateway is present.
    const bool aiSubsystemAvailable = is_ai_subsystem_inited();
    WebApiBridge gatewayProbe;
    const bool kylinBotGatewayAvailable = gatewayProbe.isGatewayReachable();
    KyInfo() << "AI subsystem available:" << aiSubsystemAvailable
             << "KylinBot Gateway available:" << kylinBotGatewayAvailable;

    const bool backendUnavailable = !aiSubsystemAvailable && !kylinBotGatewayAvailable;
    bool subsystemVersionBelowMin = aiSubsystemAvailable
            && Globals::isAISubsystemVersionTooLow(QStringLiteral("1.3.0.0"));
    bool subsystemVersionCompatLow = aiSubsystemAvailable
            && Globals::isAISubsystemVersionCompatLow();

    if (backendUnavailable) {
       // 静默启动（开机自启）时，子系统未安装则静默退出，不弹窗，
       // 避免卸载AI子系统后重启系统时桌面自动弹出"AI服务不可用"弹窗。
       // 仅在用户主动打开助手时才提示。
       if(silence){
          return 0;
       }
       AiServiceDialog dialog;
       dialog.exec();
       return 0;
    }

    // 子系统版本 < 1.3.0.0：弹窗后不启动助手
    if (subsystemVersionBelowMin) {
        if (silence) {
            return 0;
        }
        AiServiceDialog dialog;
        dialog.setLowVersionText();
        dialog.exec();
        return 0;
    }

    // 1.3.0.0 <= 版本 < 1.3.0.0-0k0.4：首次弹窗提示，但不阻止进入助手（设置页会显示橙色提示条）
    if (subsystemVersionCompatLow) {
        const QString lowVersionTipKey = "is-lowversion-shown";
        const QString lowVersionAtKey = "lowversion-popup-at";
        const QString currentSubsystemVersion = Globals::getAISubsystemVersion();
        const bool alreadyShown = ConfigManager::getInstance()->getConfigValue(lowVersionTipKey, false).toBool();
        const QString shownAtVersion = ConfigManager::getInstance()->getConfigValue(lowVersionAtKey, QString()).toString();
        const bool needShowPopup = !silence
                && (!alreadyShown || shownAtVersion != currentSubsystemVersion);
        KyInfo() << "子系统版本过低，当前版本:" << currentSubsystemVersion
                 << "是否显示过弹窗:" << alreadyShown
                 << "上次弹窗版本:" << shownAtVersion
                 << "本次是否弹窗:" << needShowPopup;
        if (needShowPopup) {
            KADBusOperator dbusOperator;
            AiServiceDialog lowVersionDialog;
            lowVersionDialog.setLowVersionText();

            QObject::connect(&dbusOperator, &KADBusOperator::sigShowOrHideView, &lowVersionDialog, [&lowVersionDialog](){
                if (!lowVersionDialog.isVisible()) {
                    return;
                }
                lowVersionDialog.show();
                lowVersionDialog.raise();
                lowVersionDialog.activateWindow();
            });
            QObject::connect(&app, &QtSingleApplication::messageReceived, &lowVersionDialog, [&lowVersionDialog](const QString &){
                if (!lowVersionDialog.isVisible()) {
                    return;
                }
                lowVersionDialog.show();
                lowVersionDialog.raise();
                lowVersionDialog.activateWindow();
            });

            lowVersionDialog.show();
            lowVersionDialog.raise();
            lowVersionDialog.activateWindow();
            lowVersionDialog.exec();
            ConfigManager::getInstance()->updateConfig(lowVersionTipKey, true);
            ConfigManager::getInstance()->updateConfig(lowVersionAtKey, currentSubsystemVersion);
            silence = false;
        }
    }

    initDataDir();

    app.setOrganizationName(QStringLiteral("openKylin"));
    app.setApplicationName("kylin-aiassistant");
    app.setWindowIcon(QIcon::fromTheme("kylin-ai-assistant"));

    //试用模式下助手不启动
    if(isContainsLive() || isKylinInstallerRunning()){
        return 0;
    }
    KyInfo() << "****** 加载翻译，创建文件夹 执行时间： " << t.elapsed();
    t.start();

    // 初始化Globals::isKnowledgeBaseActive（需要在ConfigManager初始化后）
#ifdef KB_SUPPORT_ENABLED
    Globals::isKnowledgeBaseActive = ConfigManager::getInstance()->getConfigValue("is-knowledgeBase-active").toBool();
#else
    Globals::isKnowledgeBaseActive = false;
#endif

    // 1. 创建后端实例
    CMainWnd *wnd = CMainWnd::getInstance();
    CMsgPane *msgPane = new CMsgPane;

    // Native Agent mode also needs a Gateway token.  On Lite builds there is
    // no embedded Web administration page whose tab switch would otherwise
    // trigger pairing, so pair during startup whenever Gateway is available.
    // syncGatewayPairing() is idempotent and returns immediately for an
    // existing token.
    if (kylinBotGatewayAvailable) {
        msgPane->syncGatewayPairing();
    }

    // KylinBot cron 历史同步：把外部 agent 类型定时任务的执行结果导入助手历史
    // （pull-based，启动补齐 + 每 5s 轮询，详见计划 kybot-cron-history-sync）
    KybotCronClient *kybotCronClient = new KybotCronClient(msgPane);
    KybotCronSyncManager *kybotCronSync = new KybotCronSyncManager(
        kybotCronClient, msgPane, /*intervalMs*/ 5 * 1000, msgPane);
    QObject::connect(kybotCronSync, &KybotCronSyncManager::runImported,
                     msgPane, &CMsgPane::onKybotCronRunImported);
    // 写一个WindowControl类，处理窗口类事件，转发到QML前端
    WindowController *windowCtrl = new WindowController(wnd);
    FileDialogHelper *fileDialogHelper = new FileDialogHelper();
    VoiceUsingManager *voiceManager = VoiceUsingManager::getInstance(wnd);

    Clipboard *selectclip = new Clipboard(); // 剪切板
    ReaderControl *readerControl = new ReaderControl(); // 新版朗读组件（在工作线程中）
    
    // 创建音频输入输出的工作线程
    QThread *mediaThread = new QThread();

    // 将 ReaderControl 移动到工作线程
    readerControl->moveToWorkerThread(mediaThread);
    // 创建代理对象（在主线程中），用于 QML 异步调用
    ReaderControlProxy *readerControlProxy = new ReaderControlProxy(readerControl);
    
    RecorderControl *recorderControl = new RecorderControl(); // 新版录音组件（在工作线程中）
    
    // 将 RecorderControl 移动到工作线程
    recorderControl->moveToWorkerThread(mediaThread);
    
    // 创建代理对象（在主线程中），用于 QML 异步调用
    RecorderControlProxy *recorderControlProxy = new RecorderControlProxy(recorderControl);

    KyInfo() << "****** 构造C++类 执行时间： " << t.elapsed();
    t.start();

    // 2. 创建 QML 引擎
    QQmlApplicationEngine *engine = new QQmlApplicationEngine;

    g_qmlEngine = engine;
    // 3. 将后端实例暴露给 QML
    qmlRegisterType<TextEditMenuProvider>("TextEditMenu", 1, 0, "TextEditMenuProvider");
#ifdef KB_SUPPORT_ENABLED
    qmlRegisterType<FileListModel>("FileListModelProvider", 1, 0, "FileListModel");
    qmlRegisterType<KnowledgeBaseFilterProxyModel>("KnowledgeBaseFilterProxyModelProvider", 1, 0, "KnowledgeBaseFilterProxyModel");
#endif
    engine->rootContext()->setContextProperty("windowCtrl", windowCtrl);
    // 注册 X11WindowHelper 用于 Linux X11 无边框窗口
    X11WindowHelper *x11WindowHelper = new X11WindowHelper();
    engine->rootContext()->setContextProperty("x11WindowHelper", x11WindowHelper);
    engine->rootContext()->setContextProperty("msgPane", msgPane);
    engine->rootContext()->setContextProperty("configManager", ConfigManager::getInstance());

#ifdef KYLIN_AIASSISTANT_HAS_WEBENGINE
    const bool webEngineAvailable = true;
#else
    const bool webEngineAvailable = false;
#endif
    engine->rootContext()->setContextProperty("webEngineAvailable", webEngineAvailable);

    // WebApiBridge: QWebChannel 桥接，供 WebEngineView 中的 JS 通过 Unix Socket 调用 gateway API
    WebApiBridge *webApiBridge = new WebApiBridge(&app);
    engine->rootContext()->setContextProperty("webApiBridge", webApiBridge);

    // 退出前先停掉 cron 轮询与 gateway 桥接，避免主线程同步 socket 阻塞 WebEngine 清理
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [=]() {
        qWarning() << "[SHUTDOWN] C++ aboutToQuit begin";
        kdk_dia_upload_program_exit();
        kybotCronSync->stop();
        qWarning() << "[SHUTDOWN] kybotCronSync stopped";
        kybotCronClient->prepareForShutdown();
        qWarning() << "[SHUTDOWN] kybotCronClient prepared";
        webApiBridge->prepareForShutdown();
        qWarning() << "[SHUTDOWN] webApiBridge prepared";
        if (mediaThread->isRunning()) {
            qWarning() << "[SHUTDOWN] mediaThread quit begin";
            mediaThread->quit();
            if (!mediaThread->wait(3000)) {
                qWarning() << "[SHUTDOWN] mediaThread wait TIMEOUT";
            } else {
                qWarning() << "[SHUTDOWN] mediaThread quit ok";
            }
        }
        qWarning() << "[SHUTDOWN] C++ aboutToQuit end";
    });
    engine->rootContext()->setContextProperty("mainWnd",wnd);
    engine->rootContext()->setContextProperty("voiceManager",voiceManager);
    engine->rootContext()->setContextProperty("readerControl", readerControlProxy);
    engine->rootContext()->setContextProperty("recorderControl", recorderControlProxy);
    // 朗读时限制锁屏和睡眠：开始朗读添加抑制，结束朗读解除抑制
    QObject::connect(readerControlProxy, &ReaderControlProxy::sigPlayingStart, msgPane, &CMsgPane::readingInhibit);
    QObject::connect(readerControlProxy, &ReaderControlProxy::sigReadEnd, msgPane, &CMsgPane::readingUnInhibit);
    static VoiceUsingManager* s_voiceManagerInstance = nullptr;
    s_voiceManagerInstance = voiceManager;
    qmlRegisterSingletonType<VoiceUsingManager>("com.kylin.VoiceUsingManager", 1, 0, "VoiceUsingManager",
        [](QQmlEngine*, QJSEngine*) -> QObject* {
            QQmlEngine::setObjectOwnership(s_voiceManagerInstance, QQmlEngine::CppOwnership);
            return s_voiceManagerInstance;
        });
    engine->rootContext()->setContextProperty("selectControl", SelectControl::getInstance());

    qmlRegisterUncreatableType<SelectControl>(
        "com.kylin.SelectControl",
        1, 0,
        "SelectControl",
        "SelectControl is a singleton, use selectControl context property instead"
    );
    engine->rootContext()->setContextProperty("globals", Globals::getinstance());
    engine->rootContext()->setContextProperty("commandTemplateListModel", CommandTemplateList::getInstance());
    engine->rootContext()->setContextProperty("modelItemModel", ModelItemListModel::getInstance());
    engine->rootContext()->setContextProperty("selectclip", selectclip);
    engine->rootContext()->setContextProperty("fileDialogHelper", fileDialogHelper);
#ifdef KB_SUPPORT_ENABLED
    engine->rootContext()->setContextProperty("kbDatabase", KnowledgeBaseDatabase::getInstance());
#endif

    // 注册单例
    qmlRegisterSingletonType(QUrl("qrc:/qml/qml/ButtonStateManager.qml"), "ButtonState", 1, 0, "ButtonState");
    
    // 创建IntroductionViewStateManager实例并通过context property暴露
    QQmlComponent introductionViewStateComponent(engine, QUrl("qrc:/qml/qml/IntroductionViewStateManager.qml"));
    if (introductionViewStateComponent.isReady()) {
        QObject *introductionViewState = introductionViewStateComponent.create();
        if (introductionViewState) {
            engine->rootContext()->setContextProperty("IntroductionViewState", introductionViewState);
        }
    }

    
    // 注册指令模型
    qmlRegisterType<CommandInstructionModel>("CommandInstructionModel", 1, 0, "CommandInstructionModel");
    engine->rootContext()->setContextProperty("commandInstructionModel", CommandInstructionModel::getInstance());
    
    // 注册划词指令列表模型
    engine->rootContext()->setContextProperty("selectCommandListModel", SelectCommandListModel::getInstance());
    
#ifdef KB_SUPPORT_ENABLED
    // 注册知识库空间管理器单实例
    engine->rootContext()->setContextProperty("knowledgeBaseSpaceManager", KnowledgeBaseSpaceManager::getInstance());
#endif

    KyInfo() << "****** 注册qml类和环境变量 执行时间： " << t.elapsed();
    t.start();

    // 4. 仅加载 main.qml，Loading 逻辑在 main.qml 内部用 Loader 控制
    const QUrl url("qrc:/qml/qml/main.qml");

    const bool windowShouldShow = !silence;
    const bool startupLoadsWindow = !silence || isFile;
    bool pendingStartupVisibilityApply = startupLoadsWindow;

    QObject::connect(engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url, windowCtrl, kybotCronSync, windowShouldShow, &pendingStartupVisibilityApply](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
        if (obj && url == objUrl) {
            QQuickWindow *window = qobject_cast<QQuickWindow*>(obj);
            if (window) {
                if (pendingStartupVisibilityApply) {
                    window->setVisible(windowShouldShow);
                    pendingStartupVisibilityApply = false;
                }
                kybotCronSync->start();
                kdk_dia_upload_program_startup();
            }
        }
    }, Qt::QueuedConnection);

    // 划词功能：启动时立即初始化（静默模式下也可用）
    SelectControl::getInstance()->initSelectControl();

    if (!silence) {
        engine->load(url);
    } else if (isFile) {
        // 静默模式+文件：需要加载 QML 以处理文件，但窗口保持隐藏（windowShouldShow = false）
        engine->load(url);
    }

    KyInfo() << "****** 加载主QML文件 执行时间： " << t.elapsed();
    t.start();
//    // 非首次启动行为为激活AI助手，弹出AI助手的UI界面
    QObject::connect(&app, &QtSingleApplication::messageReceived, windowCtrl, &WindowController::onAppRestarted);
    QObject::connect(windowCtrl, &WindowController::sendFile, msgPane, &CMsgPane::handleSendFile);
    QObject::connect(windowCtrl, &WindowController::historyChanged, msgPane, &CMsgPane::getDialogueList);

    //如果AI助手未启动，收到右键发送的文件，在这里处理
    if (isFile) {
        windowCtrl->setFilePathWithStart(args.first());
    }
    if (silence) {
        auto *ed = QAbstractEventDispatcher::instance();
        if (ed) {
            static int wakeCount = 0;
            static QElapsedTimer diagTimer;
            diagTimer.start();
            QObject::connect(ed, &QAbstractEventDispatcher::awake, []() {
                wakeCount++;
                if (diagTimer.elapsed() >= 30000) {
                    KyInfo() << "[DIAG] silence mode event loop wake-ups in last 30s:"
                             << wakeCount;
                    wakeCount = 0;
                    diagTimer.restart();
                }
            });
        }
    }

    KyInfo() << "****** 其他 执行时间： " << t.elapsed();
    KyInfo() << "**** main.cpp 执行时间： " << allTimer.elapsed();
    const int exitCode = app.exec();
    qWarning() << "[SHUTDOWN] app.exec() returned, exitCode=" << exitCode;
    qWarning() << "[SHUTDOWN] deleting QQmlApplicationEngine...";
    delete engine;
    g_qmlEngine = nullptr;
    qWarning() << "[SHUTDOWN] QQmlApplicationEngine deleted, main() returning";
    return exitCode;
}
