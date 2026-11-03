#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include <QObject>
#include <QClipboard>
#include <QApplication>
#include <QDebug>
#include "global/kaglobal.h"

class Clipboard : public QObject
{
    Q_OBJECT
public:
    explicit Clipboard(QObject *parent = nullptr);
    QString getClipText();
    bool isScribeWordsDisabled();
    void clearClipText();
    bool isScribeWordsVisible();
    //void setText(const QString &text);

private:
    bool scribeWordsDisabled = false;
    QClipboard *selectClip = nullptr;

signals:
    void selectWords();
public slots:
    void onScribeWordsDisabled();
    void onScribeWordsIsabled();
    void setText(const QString &text);
};

#endif // CLIPBOARD_H
