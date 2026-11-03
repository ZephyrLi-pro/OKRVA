#ifndef SELECTION_H
#define SELECTION_H

#include <QWidget>
#include <QPushButton>
#include <QLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include "mypushbutton.h"
#include "selectmorecommand.h"
#include <QDebug>
#include <QQuickWidget>
#include <QQuickItem>
#include <QMenu>
#include <QAction>
#include <QVariantList>
#include <QSettings>
#include <QDir>
#include <QAbstractListModel>
#include <QObject>
#include <QDebug>
#include <QToolTip>
#include <QEvent>

#include "commandtemplatelist.h"
#include "selectmorecommand.h"
#include "./global/kaglobal.h"
#include <kysdk/applications/kwidget.h>
#include <ukuistylehelper/ukuistylehelper.h>

#include "windowhelper.h"

#define MORE_COMMAND_MARGIN 6

using namespace kdk;

class SelectWidget : public KWidget
{
    Q_OBJECT
public:
    explicit SelectWidget(QWidget *parent = nullptr);
    void initQmlSelectWidget();
    void initSelectMoreCommandWidget();
    void initSelectPopupIconWindow();
    void initAnimation();
    void initMenu();
    void showSelectWidgetWithAnimation();
    bool isIncludeMenuPage(int x, int y);
    QString getButtonText();
    int getCurrentId();

    bool isIncludeMoreCommandWidget(int x, int y );
    void hideSelectWidgetWithAnimation();
public slots:
    void showScribeWordsAtCursorPosition();
    void sendButtonText(const QString &currentButtonText, const int currentId);
    void slResizeRect(const int width, const int height);
    void slShowToolTip(bool isShow);
    void slSettingButtonClicked();
    void slShowContextMenu();
    void slUpdateSelectEnableStatus(bool isEnabled);
    void closeWidgetAndinstructPage(int x, int y);
    void onHideAnimationFinished();
    void slEnableSelectFocus();
    void slDisableSelectFocus();
protected:
    bool eventFilter(QObject *o, QEvent *e);
    void moveEvent(QMoveEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
private:
    void insertText(const QString &text);
signals:
    void signalSendClicpText();
    void sigbuttontext();
    void sigHomePageShow();
    void sigSettingShow();
    void sigDisableScribe();
    void sigDisableMorePage();
    void sigUpdateModelAndSelectSize();
    void sigUpdateSelectEnableStatus(bool isEnabled);

private:
    QHBoxLayout *m_hLayoutMain;
    QQuickWidget *m_quickWidget;
    QMenu *settingRightClickMenu = nullptr;
    QAction *sendAction = nullptr;
    QAction *homePageAction = nullptr;
    QAction *disableAction = nullptr;
    QAction *settingAction = nullptr;
    QAction *closeAction = nullptr;
    QAction *selectedAction = nullptr;
    QPropertyAnimation *m_animation = nullptr;
    QString buttonText;
    int promptId;
    int m_moreCommandStartPos =0;
    bool m_clickFlag = false;
    //更多指令
    SelectMoreCommandWidget *m_selectMoreCommandWidget = nullptr;

#ifdef UKUI_WINDOWHELPER_SUPPORT
    UkuiWindowHelper *m_ukuiWindowHelper = nullptr;
#endif

private:
    QTimer *m_hideTimer;
    QQmlEngine *m_qmlEngine;
    QQuickWindow *m_iconWindow;

};



#endif // SELECTION_H

