#ifndef TEXTEDITMENUPROVIDER_H
#define TEXTEDITMENUPROVIDER_H

// TextEditMenuProvider.h
#include <QObject>
#include <QTextEdit>
#include <QQuickItem>

class TextEditMenuProvider : public QObject
{
    Q_OBJECT
public:
    explicit TextEditMenuProvider(QObject *parent = nullptr);

    Q_INVOKABLE void showContextMenu(QQuickItem *item, bool canUndo, bool canRedo, bool hasSelection, bool canSelectAll);

private:
    QTextEdit *m_dummyTextEdit; // 用于获取标准菜单
    QMenu *m_contextMenu;
    QString getCommandFromAction(QAction *action);
signals:
    void menuActionTriggered(const QString &actionText);
};
#endif // TEXTEDITMENUPROVIDER_H
