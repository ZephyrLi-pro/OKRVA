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

#ifndef SETTINGSPLUGIN_H
#define SETTINGSPLUGIN_H

#include "ukcc/interface/subplugininterface.h"

#include "airgestureui.h"

class SettingsPlugin : public SubPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SubPluginInterface_iid)
    Q_INTERFACES(SubPluginInterface)

public:
    explicit SettingsPlugin() = default;
    ~SettingsPlugin() = default;

    QWidget *pluginUi() override;
    const QString name() const override;
    bool isEnable() const override;
    QString translationPath() const override;

private:
    bool hasVideoCaptureCapability(int fd) const;
    bool isCameraAvailable() const;
    void installTranslation();
};

#endif // SETTINGSPLUGIN_H
