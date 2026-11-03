#ifndef CUSTOMMESSAGEBOX_H
#define CUSTOMMESSAGEBOX_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QGSettings>
#include <kpushbutton.h>
using namespace kdk;
class CustomPushButton;
class CustomMessageBox : public QDialog {
    Q_OBJECT

public:
    enum MessageBoxButton {
        OkButton = 1,
        CancelButton = 2,
        CloseButton =3
    };
    enum Icon {
        NoIcon = 0,
        Information = 1,
        Warning = 2,
        Critical = 3,
        Question = 4
    };
    explicit CustomMessageBox(QWidget *parent = nullptr);

    ///
    /// \brief 弹窗提示信息
    /// \param 提示信息
    ///
    void setMessage(const QString &message);

    ///
    /// \brief 删除弹窗默认按钮设置
    /// \param 按钮类型
    ///
    void setButtons(int buttons);

    ///
    /// \brief 弹窗次要提示信息
    /// \param 次要提示信息
    ///
    void setInformativeText(const QString &text);

    ///
    /// \brief 弹窗按钮添加
    /// \param text按钮文本，role按钮类型
    ///
    void addButton(const QString &text, MessageBoxButton role);

    ///
    /// \brief 弹窗图标设置
    /// \param 图标类型
    ///
    void setIcon(Icon role);

    ///
    /// \brief 弹窗按钮设置默认按钮
    /// \param 默认按钮类型
    ///
    void setDefaulteButton(MessageBoxButton role);

    ///
    /// \brief 获取弹窗点击的按钮
    /// \return 返回弹窗点击的按钮
    ///
    int clickedButton() const;

    int exec() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void initLayout();
    void initConnect();
    void checkTheme();
    int getLableHeight(QLabel *label);
    void updateFixedSize();
    void centerToParent();
    void handleButtonText();

private Q_SLOTS:
    void onOkClicked();
    void onCancelClicked();
    void changeTheme();

private:
    QLabel *messageLabel;
    QLabel *iconLabel;
    QLabel *informativeLabel;
    CustomPushButton *okButton;
    CustomPushButton *cancelButton;
    QPushButton *closeButton;
    int m_clickedButton;

    QGSettings *themeData = nullptr; // 监测主题

};

class CustomPushButton : public QPushButton
{
    Q_OBJECT
public:
    explicit CustomPushButton(const QString &text, QWidget *parent = nullptr);
    explicit CustomPushButton(QWidget *parent = nullptr);
    void setText(const QString &text);

private:
    void checkTheme();
    // 更新显示的文本（使用省略模式）
    void updateElidedText();
    QString m_fullText = ""; // 存储完整的原始文本
};
#endif // CUSTOMMESSAGEBOX_H
