/*
 * Copyright (C) 2026, KylinSoft Co., Ltd.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QMainWindow>
#include <QString>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() = default;

    void setIcon(const QString &palmDirection);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void initWindow();
    void initUI();

    QLabel *m_iconLabel = nullptr;
};

#endif // MAINWINDOW_H
