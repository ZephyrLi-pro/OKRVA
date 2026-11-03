/*
 * Copyright (C) 2026, KylinSoft Co., Ltd.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mainwindow.h"
#include "airgestureicons.h"

#include <QApplication>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QVBoxLayout>

namespace {
constexpr int WINDOW_SIZE = 48;
constexpr int ICON_SIZE = 28;
constexpr int WINDOW_Y = 42;
constexpr int WINDOW_RADIUS = 12;

const QMap<QString, QString> directionToIcon = {
    {"up", "hand-palm-up-24-symbolic"},
    {"down", "hand-back-down-24-symbolic"},
    {"left", "hand-palm-right-24-symbolic"},
    {"right", "hand-back-left-24-symbolic"}
};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    initWindow();
    initUI();
}

void MainWindow::setIcon(const QString &palmDirection)
{
    m_iconLabel->setPixmap(
        airGestureIconPixmap(directionToIcon.value(palmDirection), ICON_SIZE));
}

void MainWindow::initWindow()
{
    // These standard Qt window hints require no desktop-specific window SDK.
    setWindowFlags(Qt::Tool
                   | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(WINDOW_SIZE, WINDOW_SIZE);

    const QRect screen = QApplication::primaryScreen()->availableGeometry();
    move(screen.x() + (screen.width() - WINDOW_SIZE) / 2,
         screen.y() + WINDOW_Y);
}

void MainWindow::initUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(10, 10, 10, 10);

    m_iconLabel = new QLabel(centralWidget);
    m_iconLabel->setFixedSize(ICON_SIZE, ICON_SIZE);
    layout->addWidget(m_iconLabel);
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(42, 42, 42, 220));
    painter.drawRoundedRect(rect(), WINDOW_RADIUS, WINDOW_RADIUS);
}
