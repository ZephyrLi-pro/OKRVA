#ifndef CLOSEMODEWIDGET_H
#define CLOSEMODEWIDGET_H

#include <QObject>
#include <QWidget>
#include <QDialog>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGSettings>
#include <QRadioButton>
#include <QPushButton>
#include <QCheckBox>
class CloseModeWidget : public QDialog
{
    Q_OBJECT
public:
    enum ModeType {
        HideToTray,
        CloseMode,
    };
    explicit CloseModeWidget(QWidget *parent = nullptr);
    void initLayout();
    void updateMsg();
signals:
    void sigHideCurrentApp();
    void sigCurrentCloseMode(bool isHideToTray);
private:
    QVBoxLayout *mainLayout = nullptr;
    QHBoxLayout *titleHLayout = nullptr;
    QWidget *titleWidget = nullptr;
    QRadioButton *hideWindowsRadio = nullptr;
    QRadioButton *exitRadio = nullptr;
    QCheckBox *noRemindCheckBox = nullptr;
    QPushButton *cancelButton = nullptr;
    QPushButton *okButton = nullptr;
    QPushButton *closeButton = nullptr;
};

#endif // CLOSEMODEWIDGET_H
