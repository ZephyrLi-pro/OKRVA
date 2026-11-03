#ifndef VOICEWAVEWIDGET_H
#define VOICEWAVEWIDGET_H

#include <QWidget>

class QTimer;

class VoiceWaveWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VoiceWaveWidget(QWidget *parent = nullptr);

    void setActive(bool active);
    bool isActive() const;

    void setDrawable(bool drawable);
    bool isDrawable() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onTimeout();

private:
    QTimer *m_timer = nullptr;
    int m_tick = 0;
    bool m_active = false;
    bool m_drawable = true;
    qreal m_amplitude = 0.0;
    bool m_fadingOut = false;

    static constexpr int kBarCount = 6;
    static constexpr int kBarWidth = 4;
    static constexpr int kBarSpacing = 2;
    static constexpr int kBarRadius = 2;
    static constexpr int kMinBarHeight = 3;
    static constexpr int kMaxBarHeight = 20;
    static constexpr int kAnimIntervalMs = 30;
    static constexpr qreal kAmplitudeStep = 0.06;
    static constexpr qreal kAnimSpeed = 0.12;
};

#endif // VOICEWAVEWIDGET_H
