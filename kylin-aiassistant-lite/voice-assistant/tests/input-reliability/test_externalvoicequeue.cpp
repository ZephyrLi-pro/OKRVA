#include <QCoreApplication>
#include <QDebug>
#include "externalvoicequeue.h"

static void require(bool ok, const char *message) {
    if (!ok) qFatal("%s", message);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    ExternalVoiceQueue queue;
    ExternalVoiceInput input;
    quint64 first = 0, second = 0;
    require(queue.append(input) == ExternalVoiceQueue::Empty, "empty rejected");
    input.text = QStringLiteral("first question");
    require(queue.append(input, &first) == ExternalVoiceQueue::Accepted, "first accepted");
    input.text = QStringLiteral("second question");
    require(queue.append(input, &second) == ExternalVoiceQueue::Accepted, "second accepted");
    require(queue.resolve(second, QStringLiteral("second context"), false), "resolve second first");
    require(!queue.frontReady(), "slow first context cannot reorder the queue");
    require(queue.resolve(first, QStringLiteral("first context snapshot"), true), "resolve first");
    require(queue.frontReady(), "front ready");
    const ExternalVoiceInput one = queue.take();
    require(one.text == QStringLiteral("first question"), "FIFO preserved");
    require(one.context == QStringLiteral("first context snapshot") && one.contextTruncated, "context snapshot preserved");
    require(queue.take().text == QStringLiteral("second question"), "second follows first");
    input.text = QString(ExternalVoiceQueue::maxTextChars() + 1, QLatin1Char('x'));
    require(queue.append(input) == ExternalVoiceQueue::TooLong, "oversize rejected");
    input.text = QStringLiteral("bounded queue question");
    for (int i = 0; i < ExternalVoiceQueue::capacity(); ++i)
        require(queue.append(input) == ExternalVoiceQueue::Accepted, "fill queue");
    require(queue.append(input) == ExternalVoiceQueue::Full, "full rejected without replacing old input");
    require(queue.size() == ExternalVoiceQueue::capacity(), "size remains bounded");
    queue.clear();
    require(queue.isEmpty(), "clear on shutdown or error");
    require(!queue.resolve(first, QStringLiteral("late callback"), false), "late context cannot revive canceled input");
    qInfo() << "PASS: FIFO, out-of-order context, bounds, cancellation, stale callback";
}
