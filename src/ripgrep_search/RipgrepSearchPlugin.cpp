#include "RipgrepSearchPlugin.hpp"
#include "RipgrepSearchView.hpp"

#include <KActionCollection>
#include <KConfigGroup>
#include <KPluginFactory>
#include <KXMLGUIFactory>

K_PLUGIN_CLASS_WITH_JSON(RipgrepSearchPlugin, "kate_ripgrep_search.json")

struct RipgrepSearchPluginPrivate {
    RipgrepSearchPlugin *q;
    QList<RipgrepSearchView *> views;
};

RipgrepSearchPlugin::RipgrepSearchPlugin(QObject *parent)
    : KTextEditor::Plugin(parent)
    , d(new RipgrepSearchPluginPrivate)
{
    d->q = this;
}

RipgrepSearchPlugin::~RipgrepSearchPlugin()
{
    for (auto view : d->views) {
        view->deleteLater();
    }
}

QObject *RipgrepSearchPlugin::createView(KTextEditor::MainWindow *mainWindow)
{
    auto view = new RipgrepSearchView(this, mainWindow);
    connect(view, &RipgrepSearchView::destroyed, [this](QObject *view) {
        d->views.removeAll(static_cast<RipgrepSearchView *>(view));
    });
    d->views.append(view);
    return view;
}

#include "RipgrepSearchPlugin.moc"
