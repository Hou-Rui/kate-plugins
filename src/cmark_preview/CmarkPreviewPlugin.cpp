#include "CmarkPreviewPlugin.hpp"

#include "CmarkPreviewView.hpp"

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(CmarkPreviewPlugin, "kate_cmark_preview.json")

struct CmarkPreviewPluginPrivate {
    CmarkPreviewPlugin *q = nullptr;
    QList<CmarkPreviewView *> views;
};

CmarkPreviewPlugin::CmarkPreviewPlugin(QObject *parent, const QList<QVariant> &)
    : KTextEditor::Plugin(parent)
    , d(new CmarkPreviewPluginPrivate)
{
    d->q = this;
}

CmarkPreviewPlugin::~CmarkPreviewPlugin()
{
    for (auto view : d->views)
        view->deleteLater();
}

QObject *CmarkPreviewPlugin::createView(KTextEditor::MainWindow *mainWindow)
{
    auto view = new CmarkPreviewView(this, mainWindow);
    connect(view, &CmarkPreviewView::destroyed, [this](QObject *object) {
        d->views.removeAll(static_cast<CmarkPreviewView *>(object));
    });
    d->views.append(view);
    return view;
}

#include "CmarkPreviewPlugin.moc"
