#include "aiballmanager.h"

#include "aiballcontroller.h"

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QUrl>

AIBallManager::AIBallManager(QObject *parent)
    : QObject(parent)
    , m_controller(new AIBallController(this))
    , m_engine(new QQmlApplicationEngine(this))
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);
    QQuickWindow::setDefaultAlphaBuffer(true);

    connect(m_controller, &AIBallController::textSubmitted,
            this, &AIBallManager::textSubmitted);

    m_engine->rootContext()->setContextProperty(QStringLiteral("aiBallController"), m_controller);
    m_engine->load(QUrl(QStringLiteral("qrc:/voice-kyling/qml/main.qml")));
}

void AIBallManager::setAIBallVisible(bool visible)
{
    if (visible) {
        m_controller->showBall();
        return;
    }

    m_controller->hideBall();
}
