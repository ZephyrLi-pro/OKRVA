#ifndef WAYLANDWINDOWACTIVATION_H
#define WAYLANDWINDOWACTIVATION_H

#include <QDebug>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

// Qt 5's raise()/activateWindow() cannot reliably activate a native wlroots
// window after a D-Bus voice wake (there is no input activation token).
// Use the optional compositor client, asynchronously, for explicit user input.
inline void requestWaylandWindowActivation(QObject *owner, const QString &platform,
                                          const QString &appId)
{
    if (!owner || !platform.startsWith(QStringLiteral("wayland"))) return;
    const QString program = QStandardPaths::findExecutable(QStringLiteral("wlrctl"));
    if (program.isEmpty()) return;
    QTimer::singleShot(0, owner, [owner, program, appId]() {
        auto *process = new QProcess(owner);
        auto *deadline = new QTimer(process);
        deadline->setSingleShot(true);
        QObject::connect(deadline, &QTimer::timeout, process, &QProcess::kill);
        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         process, [process, deadline](int code, QProcess::ExitStatus status) {
            deadline->stop();
            if (code != 0 || status != QProcess::NormalExit)
                qWarning() << "voice-assistant compositor activation failed";
            process->deleteLater();
        });
        QObject::connect(process, &QProcess::errorOccurred, process,
                         [process](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) process->deleteLater();
        });
        process->start(program, {QStringLiteral("toplevel"), QStringLiteral("focus"),
                                QStringLiteral("app_id:") + appId});
        deadline->start(2000);
    });
}

#endif
