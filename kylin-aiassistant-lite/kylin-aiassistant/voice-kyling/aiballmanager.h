#ifndef AIBALLMANAGER_H
#define AIBALLMANAGER_H

#include <QObject>

class AIBallController;
class QQmlApplicationEngine;

class AIBallManager : public QObject
{
    Q_OBJECT

public:
    explicit AIBallManager(QObject *parent = nullptr);
    ~AIBallManager() override = default;

public slots:
    void setAIBallVisible(bool visible);

signals:
    void textSubmitted(const QString &text);

private:
    AIBallController *m_controller = nullptr;
    QQmlApplicationEngine *m_engine = nullptr;
};

#endif // AIBALLMANAGER_H
