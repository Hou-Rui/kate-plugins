#include "BookmarksTreePlugin.hpp"

#include "BookmarksTreeView.hpp"

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(BookmarksTreePlugin, "bookmarks_tree.json")

struct BookmarksTreePluginPrivate {
    BookmarksTreePlugin *q;
    QList<BookmarksTreeView *> views;
};

BookmarksTreePlugin::BookmarksTreePlugin(QObject *parent, const QList<QVariant> &)
    : KTextEditor::Plugin(parent)
    , d(new BookmarksTreePluginPrivate)
{
    d->q = this;
}

BookmarksTreePlugin::~BookmarksTreePlugin()
{
    for (auto view : d->views) {
        view->deleteLater();
    }
}

QObject *BookmarksTreePlugin::createView(KTextEditor::MainWindow *mainWindow)
{
    auto view = new BookmarksTreeView(this, mainWindow);
    connect(view, &BookmarksTreeView::destroyed, [this](QObject *view) {
        d->views.removeAll(static_cast<BookmarksTreeView *>(view));
    });
    d->views.append(view);
    return view;
}

#include "BookmarksTreePlugin.moc"
