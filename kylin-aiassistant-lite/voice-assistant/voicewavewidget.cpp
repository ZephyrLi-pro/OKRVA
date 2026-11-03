#include "voicewavewidget.h"

#include <QPainter>
#include <QTimer>
#include <QtMath>

VoiceWaveWidget::VoiceWaveWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(44, 24);
    setMaximumSize(44, 24);

    m_timer = new QTimer(this);
    m_timer->setInterval(kAnimIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &VoiceWaveWidget::onTimeout);
}

void VoiceWaveWidget::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    if (m_active) {
        m_fadingOut = false;
        m_tick = 0;
        m_timer->start();
    } else {
        m_fadingOut = true;
    }
}

bool VoiceWaveWidget::isActive() const
{
    return m_active;
}

void VoiceWaveWidget::onTimeout()
{
    ++m_tick;

    if (m_fadingOut) {
        m_amplitude -= kAmplitudeStep;
        if (m_amplitude <= 0.0) {
            m_amplitude = 0.0;
            m_fadingOut = false;
            m_timer->stop();
        }
    } else if (m_active) {
        m_amplitude += kAmplitudeStep;
        if (m_amplitude > 1.0) {
            m_amplitude = 1.0;
        }
    }

    update();
}

void VoiceWaveWidget::setDrawable(bool drawable)
{
    m_drawable = drawable;
    update();
}

bool VoiceWaveWidget::isDrawable() const
{
    return m_drawable;
}

void VoiceWaveWidget::paintEvent(QPaintEvent *)
{
    if (!m_drawable) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);

    const int totalBarsWidth = kBarCount * kBarWidth + (kBarCount - 1) * kBarSpacing;
    const int startX = (width() - totalBarsWidth) / 2;
    const int centerY = height() / 2;

    for (int i = 0; i < kBarCount; ++i) {
        const qreal phase = i * (M_PI / 3.0);
        const qreal t = m_tick * kAnimSpeed;
        const qreal val = 0.5 + 0.5 * qSin(t + phase);
        const int barHeight = kMinBarHeight
                + static_cast<int>((kMaxBarHeight - kMinBarHeight) * val * m_amplitude);

        const int x = startX + i * (kBarWidth + kBarSpacing);
        const int y = centerY - barHeight / 2;

        p.drawRoundedRect(x, y, kBarWidth, barHeight, kBarRadius, kBarRadius);
    }
}
