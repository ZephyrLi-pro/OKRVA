#ifndef MYPUSHBUTTON_H
#define MYPUSHBUTTON_H

#include <QPushButton>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include "clipboard.h"

class MyPushButton : public QPushButton {
    Q_OBJECT

public:
    MyPushButton(QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

signals:
    void leftButtonClicked();
    void rightButtonClicked();
    void sigSelectWidgetClose();
    void sigHomePageShow();
    void sigSettingShow();
    void sigDisableScribe();

public:
    QMenu *settingRightClickMenu = nullptr;
    QAction *homePageAction = nullptr;
    QAction *disableAction = nullptr;
    QAction *settingAction = nullptr;
    QAction *closeAction = nullptr;
    QAction *selectedAction = nullptr;

private:
    Clipboard *selectClip = nullptr;

private slots:

};
#endif // MYPUSHBUTTON_H
