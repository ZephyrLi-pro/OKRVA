#include "minipanel.h"
#include "screenshareoverlay.h"
#include "ui_minipanel.h"

#include <QApplication>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QIcon>
#include <QCursor>
#include <QPixmap>
#include <QScreen>
#include <ukui-log4qt.h>

namespace {
const QString kMiniBtnStyleWhite =
    QStringLiteral(
        " QPushButton {"
        "  border-radius: 16px;"
        "  background-color: #FFFFFF;"
        "  border: 1px solid rgba(0, 0, 0, 0);"
        " }"
        " QPushButton:hover {"
        "  background-color: #F2F2F2;"
        " }"
        " QPushButton:pressed {"
        "  background-color: #EBEBEB;"
        " }"
        " QPushButton:checked {"
        "  background-color: #EBF4FF;"
        " }"
        " QPushButton:checked:hover {"
        "  background-color: #E1EFFE;"
        " }"
        " QPushButton:checked:pressed {"
        "  background-color: #D7E9FE;"
        " }");

const QString kMiniCutoffStyle =
    QStringLiteral(
        " QPushButton {"
        "  border-radius: 16px;"
        "  background-color: #FFFFFF;"
        "  border: 1px solid rgba(0, 0, 0, 0);"
        " }"
        " QPushButton:hover {"
        "  background-color: #F53F3F;"
        " }"
        " QPushButton:pressed {"
        "  background-color: #C43232;"
        " }");

const QString kMiniRestoreStyle =
    QStringLiteral(
        " QPushButton {"
        "  border-radius: 16px;"
        "  background-color: #FFFFFF;"
        "  border: 1px solid rgba(0, 0, 0, 0);"
        " }"
        " QPushButton:hover {"
        "  background-color: #F2F2F2;"
        " }"
        " QPushButton:pressed {"
        "  background-color: #EBEBEB;"
        " }");

const QString kNormalBtnStyle =
    QStringLiteral(
        " QPushButton {"
        "  border-radius: 24px;"
        "  background-color: #F7F7F7;"
        "  border: 1px solid rgba(0, 0, 0, 0);"
        " }"
        " QPushButton:hover {"
        "  background-color: #F2F2F2;"
        " }"
        " QPushButton:pressed {"
        "  background-color: #EBEBEB;"
        " }"
        " QPushButton:checked {"
        "  background-color: #EBF4FF;"
        " }"
        " QPushButton:checked:hover {"
        "  background-color: #E1EFFE;"
        " }"
        " QPushButton:checked:pressed {"
        "  background-color: #D7E9FE;"
        " }");

const QString kNormalCutoffStyle =
    QStringLiteral(
        " QPushButton {"
        "  border-radius: 24px;"
        "  background-color: #F53F3F;"
        "  border: 1px solid rgba(0, 0, 0, 0);"
        " }"
        " QPushButton:hover {"
        "  background-color: #E93C3C;"
        " }"
        " QPushButton:pressed {"
        "  background-color: #C43232;"
        " }");
}

MiniPanel::MiniPanel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MiniPanel)
{
    ui->setupUi(this);
    ui->label_logo->setPixmap(
        QPixmap(QStringLiteral(":/image/icon/kylin-ai-assistant-new.png")).scaled(
            24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->pushButton_subtitle->setCheckable(true);
    ui->pushButton_screenshare->setCheckable(true);
    ui->pushButton_mute->setCheckable(true);
    connect(ui->pushButton_subtitle, &QPushButton::toggled, this, &MiniPanel::subtitleToggled);
    connect(ui->pushButton_screenshare, &QPushButton::toggled, this, [this](bool checked) {
        updateScreenShareButtonState(checked);
    });
    connect(ui->pushButton_subtitle, &QPushButton::toggled, this, [this](bool checked) {
        updateSubtitleButtonState(checked);
    });
    connect(ui->pushButton_mute, &QPushButton::toggled, this, [this](bool checked) {
        updateMuteButtonState(checked);
    });
    updateSubtitleButtonState(ui->pushButton_subtitle->isChecked());
    ui->pushButton_mute->setChecked(true);
    updateMuteButtonState(ui->pushButton_mute->isChecked());
    m_logoRightSpacer = new QWidget(this);
    m_logoRightSpacer->setFixedSize(4, 1);
    m_logoRightSpacer->hide();
    ui->horizontalLayout->insertWidget(1, m_logoRightSpacer);
    m_normalLeftMargin = ui->horizontalLayout->contentsMargins().left();
    m_normalRightMargin = ui->horizontalLayout->contentsMargins().right();
    m_normalSpacing = ui->horizontalLayout->spacing();
    connect(qApp, &QGuiApplication::primaryScreenChanged, this, &MiniPanel::onPrimaryMonitorChangedForScreenShare);

    ui->pushButton_cutoff->installEventFilter(this);
}

MiniPanel::~MiniPanel()
{
    ui->pushButton_cutoff->removeEventFilter(this);
    destroyScreenShareOverlays();
    delete ui;
}

bool MiniPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->pushButton_cutoff && m_miniMode) {
        const QEvent::Type type = event->type();
        if (type == QEvent::Enter) {
            updateMiniCutoffButtonIcon();
        } else if (type == QEvent::Leave) {
            // 按住拖拽时 Leave 会先于 Release 触发，此时不要切换图标，避免残影
            if (!(QApplication::mouseButtons() & Qt::LeftButton)) {
                updateMiniCutoffButtonIcon();
            }
        } else if (type == QEvent::MouseButtonRelease) {
            updateMiniCutoffButtonIcon();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MiniPanel::setSubtitleChecked(bool checked)
{
    ui->pushButton_subtitle->blockSignals(true);
    ui->pushButton_subtitle->setChecked(checked);
    updateSubtitleButtonState(checked);
    ui->pushButton_subtitle->blockSignals(false);
}

void MiniPanel::setMuteChecked(bool checked)
{
    ui->pushButton_mute->blockSignals(true);
    ui->pushButton_mute->setChecked(checked);
    updateMuteButtonState(checked);
    ui->pushButton_mute->blockSignals(false);
}

bool MiniPanel::muteChecked() const
{
    return ui->pushButton_mute->isChecked();
}

void MiniPanel::setScreenShareActive(bool active)
{
    if (active) {
        active = createScreenShareOverlays();
    } else {
        destroyScreenShareOverlays();
    }

    m_screenShareActive = active;
    ui->pushButton_screenshare->blockSignals(true);
    ui->pushButton_screenshare->setChecked(active);
    updateScreenShareButtonState(active);
    ui->pushButton_screenshare->blockSignals(false);
}

void MiniPanel::destroyScreenShareOverlays()
{
    QObject::disconnect(m_primaryScreenGeometryConnection);

    if (m_screenShareOverlay) {
        delete m_screenShareOverlay;
        m_screenShareOverlay = nullptr;
    }
}

bool MiniPanel::createScreenShareOverlays()
{
    destroyScreenShareOverlays();

    QScreen *primary = QGuiApplication::primaryScreen();
    if (!primary) {
        KyInfo() << "屏幕共享：当前无法获取主屏幕（primaryScreen 为空），取消显示覆盖层";
        return false;
    }

    ScreenShareOverlay *overlay = new ScreenShareOverlay(primary);
    m_primaryScreenGeometryConnection = QObject::connect(
        primary,
        &QScreen::geometryChanged,
        overlay,
        [overlay](const QRect &) { overlay->reposition(); });
    m_screenShareOverlay = overlay;
    overlay->raise();
    overlay->show();
    return true;
}

void MiniPanel::onPrimaryMonitorChangedForScreenShare(QScreen *newPrimary)
{
    Q_UNUSED(newPrimary)
    if (!m_screenShareActive) {
        return;
    }
    if (!createScreenShareOverlays()) {
        setScreenShareActive(false);
    }
}

void MiniPanel::on_pushButton_screenshare_clicked(bool checked)
{
    ui->pushButton_screenshare->blockSignals(true);
    ui->pushButton_screenshare->setChecked(m_screenShareActive);
    updateScreenShareButtonState(m_screenShareActive);
    ui->pushButton_screenshare->blockSignals(false);
    emit screenShareToggled(checked);
}

void MiniPanel::on_pushButton_mute_clicked(bool checked)
{
    emit muteToggled(!checked);
}

void MiniPanel::on_pushButton_cutoff_clicked()
{
    emit cutoffClicked();
}

void MiniPanel::on_pushButton_answer_clicked()
{
    emit answerClicked();
}

void MiniPanel::on_pushButton_restore_clicked()
{
    emit restoreClicked();
}

void MiniPanel::setCallbackMode(bool enabled)
{
    if (m_miniMode) {
        return;
    }

    if (enabled) {
        ui->pushButton_screenshare->hide();
        ui->pushButton_subtitle->hide();
        ui->pushButton_mute->hide();
        ui->pushButton_answer->show();

        QMargins margins = ui->horizontalLayout->contentsMargins();
        margins.setLeft(88);
        margins.setRight(88);
        ui->horizontalLayout->setContentsMargins(margins);
        ui->horizontalLayout->setSpacing(16);
    } else {
        ui->pushButton_answer->hide();
        ui->pushButton_screenshare->show();
        ui->pushButton_subtitle->show();
        ui->pushButton_mute->show();

        QMargins margins = ui->horizontalLayout->contentsMargins();
        margins.setLeft(m_normalLeftMargin);
        margins.setRight(m_normalRightMargin);
        ui->horizontalLayout->setContentsMargins(margins);
        ui->horizontalLayout->setSpacing(m_normalSpacing);
    }
}

void MiniPanel::setMiniMode(bool enabled)
{
    if (m_miniMode == enabled) {
        return;
    }
    m_miniMode = enabled;

    if (enabled) {
        ui->pushButton_subtitle->hide();
        ui->pushButton_answer->hide();
        ui->label_logo->show();
        m_logoRightSpacer->show();
        ui->pushButton_screenshare->show();
        ui->pushButton_mute->show();
        ui->pushButton_cutoff->show();
        ui->frame_divider->show();
        ui->pushButton_restore->show();

        ui->label_logo->setMinimumSize(24, 24);
        ui->label_logo->setMaximumSize(24, 24);
        ui->pushButton_screenshare->setMinimumSize(32, 32);
        ui->pushButton_screenshare->setMaximumSize(32, 32);
        ui->pushButton_mute->setMinimumSize(32, 32);
        ui->pushButton_mute->setMaximumSize(32, 32);
        ui->pushButton_cutoff->setMinimumSize(32, 32);
        ui->pushButton_cutoff->setMaximumSize(32, 32);

        applyMiniModeStyles();

        ui->pushButton_cutoff->setIcon(QIcon(QStringLiteral(":/image/icon/call-stop-symbolic-red.svg")));

        QMargins margins = ui->horizontalLayout->contentsMargins();
        margins.setLeft(8);
        margins.setRight(4);
        ui->horizontalLayout->setContentsMargins(margins);
        ui->horizontalLayout->setSpacing(2);
        ui->horizontalLayout->invalidate();
    } else {
        ui->label_logo->hide();
        m_logoRightSpacer->hide();
        ui->frame_divider->hide();
        ui->pushButton_restore->hide();

        ui->pushButton_screenshare->setMinimumSize(48, 48);
        ui->pushButton_screenshare->setMaximumSize(48, 48);
        ui->pushButton_mute->setMinimumSize(48, 48);
        ui->pushButton_mute->setMaximumSize(48, 48);
        ui->pushButton_cutoff->setMinimumSize(48, 48);
        ui->pushButton_cutoff->setMaximumSize(48, 48);

        applyNormalModeStyles();

        ui->pushButton_cutoff->setIcon(QIcon(QStringLiteral(":/image/icon/call-stop-symbolic.svg")));

        QMargins margins = ui->horizontalLayout->contentsMargins();
        margins.setLeft(m_normalLeftMargin);
        margins.setRight(m_normalRightMargin);
        ui->horizontalLayout->setContentsMargins(margins);
        ui->horizontalLayout->setSpacing(m_normalSpacing);
        ui->horizontalLayout->invalidate();

        ui->pushButton_screenshare->show();
        ui->pushButton_subtitle->show();
        ui->pushButton_mute->show();
    }
}

void MiniPanel::applyMiniModeStyles()
{
    ui->pushButton_screenshare->setStyleSheet(kMiniBtnStyleWhite);
    ui->pushButton_mute->setStyleSheet(kMiniBtnStyleWhite);
    ui->pushButton_cutoff->setStyleSheet(kMiniCutoffStyle);
    ui->pushButton_restore->setStyleSheet(kMiniRestoreStyle);
}

void MiniPanel::applyNormalModeStyles()
{
    ui->pushButton_screenshare->setStyleSheet(kNormalBtnStyle);
    ui->pushButton_mute->setStyleSheet(kNormalBtnStyle);
    ui->pushButton_cutoff->setStyleSheet(kNormalCutoffStyle);
}

void MiniPanel::updateMiniCutoffButtonIcon()
{
    if (!m_miniMode) {
        return;
    }

    const bool mouseInside = ui->pushButton_cutoff->rect().contains(
        ui->pushButton_cutoff->mapFromGlobal(QCursor::pos()));
    ui->pushButton_cutoff->setIcon(QIcon(mouseInside
        ? QStringLiteral(":/image/icon/call-stop-symbolic.svg")
        : QStringLiteral(":/image/icon/call-stop-symbolic-red.svg")));
}

void MiniPanel::updateSubtitleButtonState(bool checked)
{
    ui->pushButton_subtitle->setIcon(QIcon(checked
        ? QStringLiteral(":/image/icon/zimu-blue.svg")
        : QStringLiteral(":/image/icon/zimu.svg")));
    ui->pushButton_subtitle->setToolTip(checked
        ? tr("Hide subtitles")
        : tr("Show subtitles"));
}

void MiniPanel::updateScreenShareButtonState(bool active)
{
    ui->pushButton_screenshare->setIcon(QIcon(active
        ? QStringLiteral(":/image/icon/computer-symbolic-blue.svg")
        : QStringLiteral(":/image/icon/computer-symbolic.svg")));
}

void MiniPanel::updateMuteButtonState(bool checked)
{
    ui->pushButton_mute->setIcon(QIcon(checked
        ? QStringLiteral(":/image/icon/ukui-microphone-on-symbolic-blue.svg")
        : QStringLiteral(":/image/icon/ukui-microphone-on-symbolic.svg")));
    ui->pushButton_mute->setToolTip(checked
        ? tr("Turn off microphone")
        : tr("Turn on microphone"));
}

void MiniPanel::on_pushButton_subtitle_clicked(bool checked)
{
    Q_UNUSED(checked)
}

void MiniPanel::on_pushButton_subtitle_clicked()
{
}
