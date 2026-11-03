#ifndef SELECTMORECOMMAND_H
#define SELECTMORECOMMAND_H
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

#include "commandtemplatelist.h"
#include "./global/kaglobal.h"
#include <windowmanager/windowmanager.h>


class SelectMoreCommandWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SelectMoreCommandWidget (QWidget *parent =nullptr);
    void initSelectMoreCommandWidget();
    int getPromptId();
protected:
    bool eventFilter(QObject *o, QEvent *e);
    void keyPressEvent(QKeyEvent *event) override;
public slots:
    void setWidgetWidth(const int width);
    void setWidgetHeight(const int height);
signals:
    void sigCurrentCommandText(const QString &text,const int currentId);
    void sigStartSetHeight();
    void sigCloseSelectWidget();
    void sigDeactiveWidget(bool flag);
public slots:
    void sendButtonText(const QString &currentButtonText,const int currentId);
private:
    QHBoxLayout *m_hLayoutMain;
    QQuickWidget *m_quickWidget;
    QWidget *m_parentWidget = nullptr;
};

#endif //SELECTMORECOMMAND_H
