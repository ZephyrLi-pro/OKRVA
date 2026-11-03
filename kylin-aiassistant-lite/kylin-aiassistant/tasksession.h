#ifndef TASKSESSION_H
#define TASKSESSION_H

#include <QString>
#include "gatewaywebsocketmanager.h"

struct TaskSession {
    QString sessionUuid;
    int historyId;
    GatewayWebSocketManager* wsManager;
    bool isReplying;
    bool isCancelled;

    TaskSession()
        : historyId(0)
        , wsManager(nullptr)
        , isReplying(false)
        , isCancelled(false)
    {}
};

#endif
