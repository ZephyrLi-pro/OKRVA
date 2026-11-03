/*
 * Copyright (C) 2026, KylinSoft Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TRYWIDGET_H
#define TRYWIDGET_H

#include <QDialog>
#include <QVariant>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QPalette>
#include <QApplication>
#include <QScrollArea>
#include <QStackedWidget>
#include <QLabel>
#include <QGSettings>
#include <QScrollArea>
#include <QShowEvent>
#include <QCloseEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QTimer>
#include <QPainter>

#include "kysdk/applications/kwidget.h"
#include "kysdk/applications/klineframe.h"

#include "airgestureinfo.h"
#include "switchpage.h"

class TryWidget : public QDialog
{
	Q_OBJECT
public:
	explicit TryWidget(QWidget *parent = nullptr, AirGestureType gestureType = AirGestureType::Wave_Vertical);
	~TryWidget() override = default;

private:
	enum class TrialStage {
		WaitingFirstPalm,
		WaitingFirstGesture,
		WaitingSecondPalm,
		WaitingSecondGesture,
		TransitionPause
	};

	static constexpr int kGestureTransitionDelayMs = 1000;

	void initGSettings();
	QWidget *createVerticalWaveTryWidget();
	QWidget *createHorizontalWaveTryWidget();
	QWidget *createFistTryWidget();
	void updateIconDialog(const QString &palmDirection);
	void updatePromptDialog(bool withIcon, const QString &action);
	void updatePromptGeometry();
	void showPromptDialog();
	void showIconDialog(const QString &palmDirection);
	void beginGestureTransition(const QString &nextPromptAction, TrialStage nextStage);

	void startTrialMode();
	void stopTrialMode();

	void performVerticalGesture(const QString &direction);
	void performHorizontalGesture(const QString &direction);

	QPixmap invertPixmapColors(const QPixmap& pixmap);

private slots:
	void onPalmDetected(const QString &palmDirection);
	void onPalmLost();
	void onTrialGesturePerformed(const QString &gesture);

protected:
	void showEvent(QShowEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

private:

	AirGestureType m_gestureType = AirGestureType::Wave_Vertical;
	TrialStage m_trialStage = TrialStage::WaitingFirstPalm;
	QDialog *m_pPromptDialog = nullptr;
	QVBoxLayout *m_pPromptLayout = nullptr;
	QLabel *m_pPromptIconLabel = nullptr;
	QLabel *m_pPromptLabel = nullptr;
	QDialog *m_pIconDialog = nullptr;
	QLabel *m_pTopIconLabel = nullptr;

	QGSettings *m_pStyleGSettings = nullptr;
	QString m_currentStyle = "";

	QScrollArea *m_pVerticalScrollArea = nullptr;
	SwitchPage *m_pHorizontalPage = nullptr;
};

#endif // TRYWIDGET_H