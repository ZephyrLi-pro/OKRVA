#ifndef AIASSISTANTMENUPLUGIN_H
#define AIASSISTANTMENUPLUGIN_H

#include "peony-menu-plugin-kylin-aiassistant_global.h"
#include <menu-plugin-iface.h>
#include <QTranslator>
#include <QLocale>
#include <QApplication>
#include <QProcess>
#include <QAction>
#include <QFileInfo>
#include <QString>
#include <QDebug>

#include <file-info.h>
#include <gio/gio.h>

namespace Peony {

class PEONYMENUPLUGINKYLINAIASSISTANT_EXPORT AiassistantMenuPlugin: public QObject, public MenuPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID MenuPluginInterface_iid FILE "common.json")
    Q_INTERFACES(Peony::MenuPluginInterface)
public:
    explicit AiassistantMenuPlugin(QObject *parent = nullptr);

    PluginInterface::PluginType pluginType() override {return PluginInterface::MenuPlugin;}
    const QString name() override {return tr("Peony-Qt Qi Ling Extension");}
    const QString description() override {return tr("Qi Ling Menu Extension.");}
    const QIcon icon() override {return QIcon::fromTheme("kylin-ai-assistant");}
    void setEnable(bool enable) override {m_enable = enable;}
    bool isEnable() override {return m_enable;}
    QString testPlugin() override {return "test compress";}
    QList<QAction *> menuActions(Types types, const QString &uri, const QStringList &selectionUris) override;

private:
    bool m_enable;
};

}
#endif // AIASSISTANTMENUPLUGIN_H
