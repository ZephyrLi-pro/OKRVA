/*
 * Copyright (C) 2020, KylinSoft Co., Ltd.
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
#include "configmanager.h"
#include <QFile>
#include <QDir>
#include <QDebug>
#include <ukui-log4qt.h>
#include <QObject>
#include <QStandardPaths>

// 静态成员初始化
ConfigManager* ConfigManager::m_instance = nullptr;

namespace {
const int kWindowModeValue = 3;
}

ConfigManager* ConfigManager::getInstance()
{
    if (!m_instance) {
        QString configFilePath = QDir::homePath().append("/.config/kylin-aiassistant/settings.ini");
        m_instance = new ConfigManager(configFilePath);
    }
    return m_instance;
}

ConfigManager::ConfigManager(const QString &configFilePath, QObject *parent)
    : QObject(parent)
{
    // 创建 QSettings 对象，指定 INI 格式
    m_settings = new QSettings(configFilePath, QSettings::IniFormat);

    // 检查文件是否存在
    if (!QFile::exists(configFilePath)) {
        KyInfo() << "配置文件不存在，创建新的配置文件...";
        createDefaultConfig();
    }

    // 5.0升级兼容：如果is-first-install-50不存在，说明是从4.0升级，需要显示5.0新手引导
    if (!m_settings->contains("is-first-install-50")) {
        m_settings->setValue("is-first-install-50", true);
        m_settings->sync();
        KyInfo() << "5.0升级兼容：配置文件中无is-first-install-50，已设置默认值true";
    }

    // 用于兼容2503初始化无判断is-sidebar-close，is-sidebar-close是否存在，不存在则设置默认值false
    if (!m_settings->contains("is-sidebar-close")) {
        m_settings->setValue("is-sidebar-close", true);
        m_settings->sync(); // 立即写入配置文件
        KyInfo() << "配置文件中无is-sidebar-close，已设置默认值true";
    }

    // 用于兼容升级初始化无判断is-show-history-max-tip是否存在，不存在则设置默认值true
    if (!m_settings->contains("is-show-history-max-tip")) {
        m_settings->setValue("is-show-history-max-tip", true);
        m_settings->sync(); // 立即写入配置文件
        KyInfo() << "配置文件中无is-show-history-max-tip，已设置默认值true";
    }
    if (!m_settings->contains("gateway-host")) {
        m_settings->setValue("gateway-host", "127.0.0.1:42617");
        m_settings->sync();
        KyInfo() << "配置文件中无gateway-host，已设置默认值";
    }
    if (!m_settings->contains("voice-callback-enabled")) {
        m_settings->setValue("voice-callback-enabled", true);
        m_settings->sync();
        KyInfo() << "配置文件中无voice-callback-enabled，已设置默认值true";
    }
    if (!m_settings->contains("scribe-auto-invoke")) {
        m_settings->setValue("scribe-auto-invoke", true);
        m_settings->sync();
        KyInfo() << "配置文件中无scribe-auto-invoke，已设置默认值true（自动唤起）";
    }

    // 初始化新手引导完成状态
    updateGuideCompletedState();
    ensureWindowModeForIncompleteGuide();
    // 初始化5.0新手引导完成状态
    updateGuide50CompletedState();

    // 从配置文件读取值
    //loadConfig();
}

ConfigManager::~ConfigManager()
{
    delete m_settings;
}

QVariant ConfigManager::getConfigValue(const QString &key, const QVariant &defaultValue) const
{
    if (key == "win-mode" && !m_isGuideCompleted) {
        return kWindowModeValue;
    }
    return m_settings->value(key, defaultValue);  // 获取指定键的值
}

void ConfigManager::updateConfig(const QString &key, const QVariant &value)
{
    QVariant actualValue = value;
    if (key == "win-mode" && !m_isGuideCompleted && value.toInt() != kWindowModeValue) {
        actualValue = kWindowModeValue;
        KyInfo() << "新手引导未完成，忽略非窗口模式配置，强制写入 win-mode=3";
    }

    m_settings->setValue(key, actualValue);  // 设置新的配置值
    m_settings->sync();  // 强制写入
    KyInfo() << "配置已更新：" << key << "=" << actualValue;
    // 如果更新的是is-first-install配置，更新状态并发出信号
    if (key == "is-first-install") {
        updateGuideCompletedState();
        ensureWindowModeForIncompleteGuide();
    }
    if (key == "is-first-install-50") {
        updateGuide50CompletedState();
    }
}

bool ConfigManager::guideCompleted() const
{
    return m_isGuideCompleted;
}

bool ConfigManager::guide50Completed() const
{
    return m_isGuide50Completed;
}

void ConfigManager::updateGuideCompletedState()
{
    QVariant isFirstInstall = getConfigValue("is-first-install", true);
    // is-first-install为false表示已完成引导
    bool newValue = (isFirstInstall.toString() == "false" || isFirstInstall.toBool() == false);
    
    if (m_isGuideCompleted != newValue) {
        m_isGuideCompleted = newValue;
        KyInfo() << "新手引导完成状态变化，新值：" << m_isGuideCompleted;
        emit isGuideCompletedChanged();
        KyInfo() << "已发出 isGuideCompletedChanged 信号";
    }
}

void ConfigManager::updateGuide50CompletedState()
{
    QVariant isFirstInstall50 = m_settings->value("is-first-install-50", true);
    bool newValue = (isFirstInstall50.toString() == "false" || isFirstInstall50.toBool() == false);

    if (m_isGuide50Completed != newValue) {
        m_isGuide50Completed = newValue;
        KyInfo() << "5.0新手引导完成状态变化，新值：" << m_isGuide50Completed;
        emit isGuide50CompletedChanged();
        KyInfo() << "已发出 isGuide50CompletedChanged 信号";
    }
}

void ConfigManager::ensureWindowModeForIncompleteGuide()
{
    if (m_isGuideCompleted) {
        return;
    }

    const int currentWinMode = m_settings->value("win-mode", kWindowModeValue).toInt();
    if (currentWinMode == kWindowModeValue) {
        return;
    }

    m_settings->setValue("win-mode", kWindowModeValue);
    m_settings->sync();
    KyInfo() << "新手引导未完成，已将 settings.ini 中的 win-mode 强制修正为窗口模式";
}

void ConfigManager::updateNonCriticalConfig(const QString &key, const QVariant &value) {
    KyInfo() << "updateNonCriticalConfig";
    m_settings->setValue(key, value);  // 设置新的配置值
    KyInfo() << "配置已更新：" << key << "=" << value;
}

void ConfigManager::createDefaultConfig()
{
    m_settings->setValue("is-stroke-word", true);
    m_settings->setValue("scribe-auto-invoke", true);
    m_settings->setValue("history-id", 0);
    m_settings->setValue("meeting-id", 0);
    m_settings->setValue("voice-id", 0);
    m_settings->setValue("win-mode", 3);
    m_settings->setValue("is-side-mode-abreast", false);
    m_settings->setValue("is-window-mode-topping", false);
    m_settings->setValue("is-first-install", true);
    m_settings->setValue("is-first-install-50", true);
    m_settings->setValue("is-privacy-detection", false);
    m_settings->setValue("is-hide-tray",true);
    m_settings->setValue("is-need-tips",true);
    m_settings->setValue("cur-nlp-model", "");
    m_settings->setValue("is-sidebar-close", false);
    m_settings->setValue("question-mode-enabled", true);
    m_settings->setValue("is-first-template", true);
    m_settings->setValue("is-show-history-max-tip", true);
    m_settings->setValue("is-lowversion-shown", false);
    m_settings->setValue("gateway-host", "127.0.0.1:42617");
    m_settings->setValue("voice-callback-enabled", true);
    m_settings->sync();  // 强制写入
}
