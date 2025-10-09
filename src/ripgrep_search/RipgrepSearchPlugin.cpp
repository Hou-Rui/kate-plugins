#include "RipgrepSearchPlugin.hpp"
#include "RipgrepSearchView.hpp"

#include <KActionCollection>
#include <KConfigGroup>
#include <KPluginFactory>
#include <KXMLGUIFactory>

K_PLUGIN_CLASS_WITH_JSON(RipgrepSearchPlugin, "ripgrep_search.json")

RipgrepSearchPlugin::RipgrepSearchPlugin(QObject *parent)
    : KTextEditor::Plugin(parent)
{
}

RipgrepSearchPlugin::~RipgrepSearchPlugin()
{
    for (auto view : m_views) {
        view->deleteLater();
    }
}

QObject *RipgrepSearchPlugin::createView(KTextEditor::MainWindow *mainWindow)
{
    auto view = new RipgrepSearchView(this, mainWindow);
    connect(view, &RipgrepSearchView::destroyed, [this](QObject *view) {
        m_views.removeAll(static_cast<RipgrepSearchView *>(view));
    });
    m_views.append(view);
    return view;
}

#include "RipgrepSearchPlugin.moc"
