#include "RipgrepSearchPlugin.hpp"
#include "RipgrepSearchView.hpp"

#include <KPluginFactory>
#include <KTextEditor/MainWindow>
#include <KTextEditor/Plugin>

#include <QList>

K_PLUGIN_CLASS_WITH_JSON(RipgrepSearchPlugin, "ripgrep_search.json")

RipgrepSearchPlugin::RipgrepSearchPlugin(QObject *parent, const QList<QVariant> &)
    : KTextEditor::Plugin(parent)
{
}

QObject *RipgrepSearchPlugin::createView(KTextEditor::MainWindow *mainWindow)
{
    return new RipgrepSearchView(this, mainWindow);
}

#include "RipgrepSearchPlugin.moc"