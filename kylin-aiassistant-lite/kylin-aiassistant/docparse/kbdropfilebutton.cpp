#include "kbdropfilebutton.h"

#include <QPointer>
#include <ukui-log4qt.h>

KBDropFileButton::KBDropFileButton(QWidget *parent) : QPushButton(parent)
{
    setAcceptDrops(true); // 启用拖拽接收
    initLayout();
    initConnect();
}

KBDropFileButton::~KBDropFileButton()
{

}

void KBDropFileButton::initLayout()
{
    QFont minFont;
    minFont.setPointSize(Globals::fontSize - 2);

    m_pIcon = new QLabel(this);
    m_pIcon->setAccessibleName("kylin-aiassistant_QLabel_m_pIcon");

    // 设置QLabel的渲染属性
    m_pIcon->setAttribute(Qt::WA_Hover, true);
    m_pIcon->setStyleSheet("QLabel { image-rendering: smooth; }");

    QPixmap originalPixmap(":/icon/icon/add-file.svg");
    QPixmap scaledPixmap = originalPixmap.scaled(32, 32,
                                                 Qt::IgnoreAspectRatio,
                                                 Qt::SmoothTransformation);

    m_pIcon->setPixmap(scaledPixmap);
    m_pIcon->setAlignment(Qt::AlignHCenter);
    m_pIcon->setStyleSheet("background:transparent;");

    m_pLabel = new QLabel(this);
    m_pLabel->setAccessibleName("kylin-aiassistant_QLabel_m_pLabel");
    m_pLabel->setFixedWidth(296);
    m_pLabel->setAlignment(Qt::AlignHCenter);
    m_pLabel->setText(tr("Click or drag the file here to upload"));
    m_pLabel->setFont(minFont);
    m_pLabel->setStyleSheet("background:transparent;");

    m_pTipLabel = new QLabel();
    m_pTipLabel->setAccessibleName("kylin-aiassistant_QLabel_m_pTipLabel");
    m_pTipLabel->setFixedWidth(296);
    m_pTipLabel->setAlignment(Qt::AlignHCenter);
    m_pTipLabel->setText(tr("Format support (plain text): doc, docx, pdf;\n"
                            "Number of files: no more than 1;\n"
                            "File size: Each individual file should not exceed 5 MB"));
    m_pTipLabel->setDisabled(true);
    m_pTipLabel->setFont(minFont);
    m_pTipLabel->setStyleSheet("background:transparent;");

    QVBoxLayout *uploadBtnVLayout = new QVBoxLayout();
    uploadBtnVLayout->setContentsMargins(0, 0, 0, 0);
    uploadBtnVLayout->setSpacing(0);
    uploadBtnVLayout->setAlignment(Qt::AlignHCenter);
    uploadBtnVLayout->addSpacing(28);
    uploadBtnVLayout->addWidget(m_pIcon);
    uploadBtnVLayout->addSpacing(4);
    uploadBtnVLayout->addWidget(m_pLabel);
    uploadBtnVLayout->addSpacing(8);
    uploadBtnVLayout->addWidget(m_pTipLabel);
    uploadBtnVLayout->addStretch();

    this->setLayout(uploadBtnVLayout);
    this->setObjectName("dropButton");
    this->setStyleSheet("#dropButton {"
                        "background-color: rgba(0, 0, 0, 0.03);"
                        "border: 1px dashed rgba(0, 0, 0, 0.1);"
                        "border-radius: 6px;"
                        "opacity:1;"
                        "flex-grow:1;"
                        "align-self: stretch;"
                        "box-sizing: border-box;"
                        "z-index:2;"
                        "}"
                        "#dropButton:hover {"
                        "background-color: rgba(0, 0, 0, 0.05);"
                        "}"
                        "#dropButton:pressed {"
                        "background-color: rgba(0, 0, 0, 0.08);"
                        "}");
    return;
}

void KBDropFileButton::initConnect()
{
    connect(this, &QPushButton::clicked, this, &KBDropFileButton::onSelectFile);
    QPointer<KBDropFileButton> safeThis(this);
    connect(Globals::getinstance(), &Globals::fontsizeChanged, this, [safeThis](float fontSize) {
        if (!safeThis) return;
        QFont font;
        font.setPointSize(fontSize - 2);
        safeThis->m_pLabel->setFont(font);
        safeThis->m_pTipLabel->setFont(font);
    });

    return;
}

// 拖拽进入
void KBDropFileButton::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) { // 是否为文件
        event->acceptProposedAction();
    } else {
        event->ignore();
    }

    return;
}

// 拖拽释放
void KBDropFileButton::dropEvent(QDropEvent* event)
{
    event->ignore();

    // 是否为文件
    if (!event->mimeData()->hasUrls()) {
        return;
    }

    QStringList filePaths;
    const QList<QUrl> urls = event->mimeData()->urls();
    KyInfo() << "------dropEvent------" << urls;
    if (urls.isEmpty()) {
        return;
    }

    // 遍历所有拖拽的文件URL
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            filePaths.append(url.toLocalFile());
        } else {
            KyInfo() << "Invalid file path!";
        }
    }

    if (!filePaths.isEmpty()) {
        KyInfo() << "Dropped file:" << filePaths;
        emit sigFileDropped(filePaths);
        event->acceptProposedAction();
    }

    return;
}

void KBDropFileButton::onSelectFile()
{
    emit sigSelectFile();
    return;
}
