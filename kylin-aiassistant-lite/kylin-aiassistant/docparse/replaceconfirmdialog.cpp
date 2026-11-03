#include "replaceconfirmdialog.h"
#include "documentlistmodel.h"

#include <QMimeType>
#include <QMimeDatabase>
#include <QFontMetrics>
#include <QPixmap>
#include <QFileInfo>

ReplaceConfirmDialog::ReplaceConfirmDialog(QString fileName, QString uuid, QWidget *parent):
    QDialog(parent)
{
    m_fileName = fileName;
    m_uuid = uuid;
    initLayout();
    initConnect();
    checkTheme();
}

ReplaceConfirmDialog::ReplaceConfirmDialog(QString fileName, QString uuid, bool isSingle, QWidget *parent):
    QDialog(parent)
{
    m_fileName = fileName;
    m_uuid = uuid;
    initLayout();
    initConnect();
    checkTheme();

    m_selectAll->setVisible(!isSingle);
}

ReplaceConfirmDialog::~ReplaceConfirmDialog()
{

}

bool ReplaceConfirmDialog::getIsApplyToAll()
{
    return m_selectAll->isChecked();
}

void ReplaceConfirmDialog::initLayout()
{
    this->setFixedSize(376, 196);
    this->setWindowTitle(QString(" "));
    this->setWindowFlags(Qt::Dialog);
    this->setAttribute(Qt::WA_AlwaysShowToolTips);

    m_messageLabel = new QLabel(this);
    m_messageLabel->setAccessibleName("kylin-aiassistant_QLabel_m_messageLabel");
    m_messageLabel->setFixedWidth(280);
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setText(QString(tr("This location already contains a file named \"%1\". Are you sure you want to replace it?")).arg(m_fileName));

    QHBoxLayout *contentHLayout = new QHBoxLayout();
    contentHLayout->setContentsMargins(8, 0, 0, 0);
    contentHLayout->setSpacing(8);
    contentHLayout->addWidget(m_iconLabel);
    contentHLayout->addWidget(m_messageLabel);
    contentHLayout->addStretch();

    m_cancelBtn = new QPushButton(tr("Cancel"));
    m_cancelBtn->setAccessibleName("kylin-aiassistant_QPushButton_m_cancelBtn");
    m_cancelBtn->setFixedSize(80, 36);

    m_acceptBtn = new QPushButton(tr("Replace"));
    m_acceptBtn->setAccessibleName("kylin-aiassistant_QPushButton_m_acceptBtn");
    m_acceptBtn->setFixedSize(80, 36);

    m_selectAll = new QCheckBox(tr("Apply to All"), this);
    m_selectAll->setAccessibleName("kylin-aiassistant_QPushButton_m_selectAll");
    m_selectAll->setFixedSize(80, 20);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addWidget(m_selectAll);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelBtn);
    buttonLayout->setSpacing(8);
    buttonLayout->addWidget(m_acceptBtn);

    QVBoxLayout *contentLayout = new QVBoxLayout;
    contentLayout->setSpacing(0);
    contentLayout->setContentsMargins(24, 16, 24, 24);
    contentLayout->addLayout(contentHLayout);
    contentLayout->addStretch();
    contentLayout->addLayout(buttonLayout);

    QWidget *contentWidget = new QWidget;
    contentWidget->setAccessibleName("kylin-aiassistant_QWidget_contentWidget");
    contentWidget->setContentsMargins(0, 0, 0, 0);
    contentWidget->setLayout(contentLayout);

    QVBoxLayout *vLayout = new QVBoxLayout;
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);
    vLayout->addWidget(contentWidget);
    this->setLayout(vLayout);

    return;
}

void ReplaceConfirmDialog::initConnect()
{
    connect(m_cancelBtn, &QPushButton::clicked, [this](){reject();});
    connect(m_acceptBtn, &QPushButton::clicked, [this](){accept();});
    connect(Globals::getinstance(), &Globals::fontsizeChanged, this, &ReplaceConfirmDialog::adjustFileName);
    return;
}

void ReplaceConfirmDialog::changeTheme()
{
    QPalette p = this->palette();
    p.setColor(QPalette::Window, Qt::white);
    this->setPalette(p);
    return;
}

void ReplaceConfirmDialog::checkTheme()
{
    changeTheme();
    adjustFileName();
    if(QGSettings::isSchemaInstalled("org.ukui.style")) {
        themeData = new QGSettings("org.ukui.style");
        connect(themeData, &QGSettings::changed, this, [=](const QString &key) {
            if(key == "styleName") {
                changeTheme();
            }
        });
    }

    return;
}

void ReplaceConfirmDialog::accept()
{
    return QDialog::accept();
}

void ReplaceConfirmDialog::keyPressEvent(QKeyEvent *event)
{
    // Key_Enter是小键盘的确认键，Key_Return是大键盘的回车键
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit accept();
    }
    return;
}

void ReplaceConfirmDialog::adjustFileName()
{
    if (m_messageLabel == nullptr || m_fileName == "") {
        return;
    }

    QFontMetrics fm(m_messageLabel->font());
    QRect labelContentsRect = m_messageLabel->contentsRect(); // 文本有效渲染区域（排除内边距）

    // 文本总高度上限
    const int maxTotalHeight = 4 * fm.lineSpacing();
    // 文本有效宽度
    const int effectiveWidth = labelContentsRect.width();
    if (effectiveWidth <= 0 || maxTotalHeight <= 0) {
        return;
    }

    // 文案模板
    const QString textTemplate = tr("This location already contains a file named \"%1\". Are you sure you want to replace it?");
    QString displayFileName = m_fileName;
    QString finalText = textTemplate.arg(displayFileName);
    const QString ellipsisStr = "..."; // 省略号

    while (true) {
        // 计算当前文案的渲染高度（按有效宽度换行后）
        QRect textRect = fm.boundingRect(
            labelContentsRect,
            Qt::TextWordWrap | m_messageLabel->alignment(),
            finalText
        );

        // 文案高度<4行，退出循环
        if (textRect.height() < maxTotalHeight) {
            break;
        }

        // 文件名无法再缩短，退出避免死循环
        if (displayFileName.length() <= 3) { // 如 "..."
            break;
        }

        // 移除右侧一个字符，加省略号
        if (displayFileName.endsWith(ellipsisStr)) {
            displayFileName.chop(ellipsisStr.length());
            displayFileName.chop(1);
            displayFileName += ellipsisStr;
        } else {
            displayFileName.chop(1);
            displayFileName += ellipsisStr;
        }

        finalText = textTemplate.arg(displayFileName);
    }

    m_messageLabel->setWordWrap(true);
    m_messageLabel->setText(finalText);

    // 判断行数并设置弹窗高度
    QRect finalTextRect = fm.boundingRect(labelContentsRect, Qt::TextWordWrap, finalText);
    const int singleLineHeight = fm.lineSpacing();
    const int finalLineCount = finalTextRect.height() / singleLineHeight; // 实际行数
    const int dialogHeightTwoLines = 196;   // ≤2行
    const int dialogHeightThreeLines = 208; // ≥3行
    int targetDialogHeight = (finalLineCount <= 2) ? dialogHeightTwoLines : dialogHeightThreeLines;
    this->setFixedHeight(targetDialogHeight);

    qDebug() << "最终行数：" << finalTextRect.height() / fm.lineSpacing()
             << "，文件名：" << displayFileName
             << "，文案高度：" << finalTextRect.height()
             << "，4行高度：" << maxTotalHeight;

    return;
}
