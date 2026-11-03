#ifndef CUSTOMWINDOWBUTTON_H
#define CUSTOMWINDOWBUTTON_H

#include <QEvent>
#include <QObject>
#include <QPushButton>
#include "kysdk/applications/themeController.h"
#include "kysdk/applications/parmscontroller.h"

using namespace kdk;

class CustomWindowButton : public QPushButton, public ThemeController
{
    Q_OBJECT

public:
    explicit CustomWindowButton(QWidget *parent = 0);
    ~CustomWindowButton();
    void setCustomIcon(QIcon customIcon);
private:
    QColor m_pixColor;
    QIcon icon;
protected:
    bool eventFilter(QObject *watched, QEvent *event);
    void changeTheme() override;
};

#endif // CUSTOMWINDOWBUTTON_H
