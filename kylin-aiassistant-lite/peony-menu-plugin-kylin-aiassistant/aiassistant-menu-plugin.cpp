#include "aiassistant-menu-plugin.h"

using namespace Peony;

AiassistantMenuPlugin::AiassistantMenuPlugin(QObject *parent) : QObject (parent)
{
    QTranslator *t = new QTranslator(this);
    qDebug()<<"system().name:"<<QLocale::system().name();
    qDebug()<<"\n\n\n\n\n\n\ntranslate:"<<t->load("/usr/share/kylin-aiassistant/translations/peony-menu-plugin-kylin-aiassistant_"+QLocale::system().name());
    QApplication::installTranslator(t);
}

QList<QAction *> AiassistantMenuPlugin::menuActions(MenuPluginInterface::Types types, const QString &uri, const QStringList &selectionUris)
{
    QList<QAction*> actions;
    if (types == MenuPluginInterface::DirectoryView || types == MenuPluginInterface::DesktopWindow)
    {
        if (selectionUris.isEmpty() || selectionUris.size() > 1) {
            return actions;
        }

        auto info = FileInfo::fromUri(selectionUris.first());
        if (info->isDir()){
            return actions;
        }

        if(selectionUris.first().endsWith(".desktop")){
           return actions;
        }
        //special type mountable, return
        qDebug()<<"info isVirtual:"<<info->isVirtual()<<info->mimeType();
        if (selectionUris.first().startsWith("computer:///") ||
            selectionUris.first().startsWith("trash:///") ||
            selectionUris.first().startsWith("recent:///") ||
            selectionUris.first().startsWith("filesafe:///") ||
            info->isVirtual())
              return actions;

        QFileInfo file(selectionUris.first());
        QIcon icon(QIcon::fromTheme("kylin-ai-assistant"));
        QAction *sendToAI = new QAction(icon, tr("Send to Qi Ling"), nullptr);
        actions<<sendToAI;
        connect(sendToAI, &QAction::triggered, [=](){
            qDebug()<<"send file:"<<uri<<selectionUris;
            QProcess p;
            p.setProgram("kylin-aiassistant");
            p.setArguments(QStringList()<<"-f"<<selectionUris.first());
            p.startDetached(p.program(), p.arguments());
            p.waitForFinished(-1);
        });

    }


    return actions;
}
