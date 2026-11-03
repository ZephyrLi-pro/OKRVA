#include "texteditmenuprovider.h"
#include <QMenu>
#include <QQuickWindow>
#include <QDebug>
#include <QAction>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QTimer>
#include <QKeySequence>

TextEditMenuProvider::TextEditMenuProvider(QObject *parent)
    : QObject(parent)
{
    m_dummyTextEdit = new QTextEdit();
    m_contextMenu = m_dummyTextEdit->createStandardContextMenu();
}

QString TextEditMenuProvider::getCommandFromAction(QAction *action)
{
    QKeySequence shortcut = action->shortcut();

    if (shortcut == QKeySequence::Undo) return "Undo";
    if (shortcut == QKeySequence::Redo) return "Redo";
    if (shortcut == QKeySequence::Cut) return "Cut";
    if (shortcut == QKeySequence::Copy) return "Copy";
    if (shortcut == QKeySequence::Paste) return "Paste";
    if (shortcut == QKeySequence::Delete) return "Delete";
    if (shortcut == QKeySequence::SelectAll) return "SelectAll";

    // 如果没有快捷键，尝试通过 objectName 或 data 来识别
    QString objName = action->objectName();
    if (objName.contains("undo")) return "Undo";
    if (objName.contains("redo")) return "Redo";
    if (objName.contains("cut")) return "Cut";
    if (objName.contains("copy")) return "Copy";
    if (objName.contains("paste")) return "Paste";
    if (objName.contains("delete")) return "Delete";
    if (objName.contains("select")) return "SelectAll";

    return "Unknown";
}

void TextEditMenuProvider::showContextMenu(QQuickItem *item, bool canUndo, bool canRedo, bool hasSelection, bool canSelectAll)
{
    if (!item || !item->window()) return;
    // 更新菜单项状态
    m_contextMenu = m_dummyTextEdit->createStandardContextMenu();

    // 连接菜单动作到信号
    int index = 0;
    foreach (QAction *action, m_contextMenu->actions()) {
        qDebug() << "Action[" << index << "] text:" << action->text()
                 << ", shortcut:" << action->shortcut().toString()
                 << ", objectName:" << action->objectName()
                 << ", isSeparator:" << action->isSeparator();
        index++;

        if (action->isSeparator()) continue;

        QString cmd = getCommandFromAction(action);
        qDebug() << "Action cmd:" << cmd;

        if (cmd == "Undo") {
            action->setEnabled(canUndo);
            if (action->shortcut().isEmpty())
                action->setShortcut(QKeySequence::Undo);
        }
        else if (cmd == "Redo") {
            action->setEnabled(canRedo);
            if (action->shortcut().isEmpty())
                action->setShortcut(QKeySequence::Redo);
        }
        else if (cmd == "Cut") {
            action->setEnabled(hasSelection);
            if (action->shortcut().isEmpty())
                action->setShortcut(QKeySequence::Cut);
        }
        else if (cmd == "Copy") {
            action->setEnabled(hasSelection);
            if (action->shortcut().isEmpty())
                action->setShortcut(QKeySequence::Copy);
        }
        else if (cmd == "Delete") {
            action->setEnabled(hasSelection);
            if (action->shortcut().isEmpty())
                action->setShortcut(QKeySequence::Delete);
        }
        else if (cmd == "Paste") {
            if (action->shortcut().isEmpty())
                action->setShortcut(QKeySequence::Paste);
        }
        else if (cmd == "SelectAll") {
            action->setEnabled(canSelectAll);
            if (action->shortcut().isEmpty())
                action->setShortcut(QKeySequence::SelectAll);
        }

        connect(action, &QAction::triggered, this, [this, cmd](){
            qDebug() << "Action triggered, cmd:" << cmd;
            emit menuActionTriggered(cmd);
        });
    }
    m_contextMenu->exec(QCursor::pos());
    return;
}
