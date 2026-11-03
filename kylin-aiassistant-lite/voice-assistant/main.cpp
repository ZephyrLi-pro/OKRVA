// TODO(设计): 初次显示应位于 AI 助手任务栏常驻图标正上方（屏幕右下角区域）；精确定位可参考
// kylin-note Tray/MiniWindow::adjustWindowPosition 与 MainPage/mainscenario 的屏幕与面板处理。

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include <QTimer>
#include <QUuid>
#include <QtGlobal>
#include <ukui-log4qt.h>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>

#include "mainwindow.h"
#include "dbus/vadbusoperator.h"
#include "dbus/kadbusclient.h"
#include <singleapplication.h>
#include <kysdk/diagnosetest/libkydatacollect.h>

#if defined(Q_OS_UNIX)
#include <unistd.h>
#endif

static bool argvContains(int argc, char **argv, const char *needle)
{
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], needle) == 0) {
            return true;
        }
    }
    return false;
}

int main(int argc, char *argv[])
{
#ifndef QT_DEBUG
    initUkuiLog4qt("kylin-aiassistant");
#endif

    // 须在构造 QtSingleApplication 之前判定：独立 appId 才能与已运行的正式实例并存（Qt Creator 调试用）。
    const bool newInstance = argvContains(argc, argv, "--new-instance");
    QString appId = QStringLiteral("voice-assistant");
    if (newInstance) {
#if defined(Q_OS_UNIX)
        appId += QStringLiteral("-") + QString::number(static_cast<qint64>(::getpid()));
#else
        appId += QStringLiteral("-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
#endif
    }

    //适配分数缩放
#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    kdk::QtSingleApplication app(appId, argc, argv);
    app.setApplicationName(QStringLiteral("voice-assistant"));
    app.setApplicationDisplayName(QStringLiteral("小K"));
    app.setDesktopFileName(QStringLiteral("voice-assistant"));
    app.setWindowIcon(QIcon::fromTheme(
        QStringLiteral("kylin-ai-assistant"),
        QIcon(QStringLiteral(":/image/icon/kylin-ai-assistant-new.png"))));
    qApp->setProperty("useLightTheme", true);
    qApp->setStyle(QStringLiteral("ukui"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Kylin AI voice assistant"));
    parser.addHelpOption();
    // 与助手启动器约定：通过本参数唤起或前置语音通话窗口（尚无正式协议名，暂定为 voice-call）。
    const QCommandLineOption voiceCallOption(
            QStringList() << QStringLiteral("voice-call"),
            QStringLiteral("Show or raise the voice call window (for launcher integration)."));
    parser.addOption(voiceCallOption);
    const QCommandLineOption newInstanceOption(
            QStringList() << QStringLiteral("new-instance"),
            QStringLiteral("Use a separate single-application id (debug from Qt Creator while another instance runs)."));
    parser.addOption(newInstanceOption);
    const QCommandLineOption dbusActivationOption(
            QStringList() << QStringLiteral("dbus-activation"),
            QStringLiteral("Wait for a D-Bus request before starting speech recognition."));
    parser.addOption(dbusActivationOption);
    parser.process(app);

    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), QStringLiteral("qt"), QStringLiteral("_"),
                          QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    QTranslator voiceTranslator;
    static const QStringList translationRoots = {
        QStringLiteral("/usr/share/voice-assistant/translations"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
        QStringLiteral(":/translations"),
    };
    for (const QString &root : translationRoots) {
        if (voiceTranslator.load(QLocale(), QStringLiteral("voice_assistant"), QStringLiteral("_"), root)) {
            app.installTranslator(&voiceTranslator);
            break;
        }
    }

    if (app.isRunning() && !parser.isSet(newInstanceOption)) {
        const QString msg = parser.isSet(voiceCallOption)
                ? QStringLiteral("voice-call")
                : QStringLiteral("activate");
        qWarning() << "voice-assistant: 检测到已有实例在运行，已向其实例发送消息并退出本进程。"
                   << "若从 Qt Creator 启动却看不到窗口，请先结束旧的 voice-assistant，或在运行参数中加入 --new-instance。";
        app.sendMessage(msg, 5000);
        return 0;
    }

    MainWindow w(nullptr, !parser.isSet(dbusActivationOption));
    app.setActivationWindow(&w);
    QObject::connect(&app, &kdk::QtSingleApplication::messageReceived, &w, &MainWindow::onActivationMessage);

    VADBusOperator dbusOperator;
    w.setDbusOperator(&dbusOperator);
    QObject::connect(&dbusOperator, &VADBusOperator::sigShowWindow, &w, &MainWindow::onShowWindow);
    QObject::connect(&dbusOperator, &VADBusOperator::sigShowExternalListening,
                     &w, &MainWindow::onShowExternalListening);
    QObject::connect(&dbusOperator, &VADBusOperator::sigSubmitExternalText,
                     &w, &MainWindow::onSubmitExternalText);
    QObject::connect(&dbusOperator, &VADBusOperator::sigExternalRecognitionFailed,
                     &w, &MainWindow::onExternalRecognitionFailed);
    QObject::connect(&dbusOperator, &VADBusOperator::sigQuit, &w, &MainWindow::shutdownAndQuit);

    KADBusClient kaDbusClient;
    w.setKaDbusClient(&kaDbusClient);

    QDBusConnectionInterface *sessionInterface = QDBusConnection::sessionBus().interface();
    const bool parentRegistered = sessionInterface
            && sessionInterface->isServiceRegistered(QStringLiteral("org.ukui.kylin_aiassistant"));
    if (!parentRegistered && !parser.isSet(dbusActivationOption)) {
        qWarning() << "voice-assistant: parent service org.ukui.kylin_aiassistant is not registered, exiting";
        return 0;
    }
    if (parentRegistered) {
        QDBusServiceWatcher *parentWatcher = new QDBusServiceWatcher(
            "org.ukui.kylin_aiassistant",
            QDBusConnection::sessionBus(),
            QDBusServiceWatcher::WatchForUnregistration,
            &w);
        QObject::connect(parentWatcher, &QDBusServiceWatcher::serviceUnregistered, &w, [&w]() {
            qWarning() << "voice-assistant: parent service org.ukui.kylin_aiassistant unregistered, shutting down";
            w.shutdownAndQuit();
        });
    } else {
        qWarning() << "voice-assistant: running in external-ASR mode without parent service";
    }

    w.show();
    w.raise();
    w.activateWindow();
    QTimer::singleShot(0, &w, &MainWindow::repositionNearScreenCorner);

    // 启动后立即分配 voice sessionId，确保后续消息可立即保存（包括屏幕截图历史）
    QTimer::singleShot(100, &w, &MainWindow::ensureVoiceSessionIdAsync);

    // 数据埋点：应用启动/退出事件
    kdk_dia_upload_program_startup();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        kdk_dia_upload_program_exit();
    });

    return app.exec();
}
