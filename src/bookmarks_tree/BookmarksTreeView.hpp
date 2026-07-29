#pragma once
#include <QObject>

namespace KTextEditor
{
class MainWindow;
}
class BookmarksTreePlugin;
class BookmarksTreeViewPrivate;

class BookmarksTreeView : public QObject
{
    Q_OBJECT
public:
    explicit BookmarksTreeView(BookmarksTreePlugin *plugin, KTextEditor::MainWindow *mainWindow);
    ~BookmarksTreeView();

private:
    friend BookmarksTreeViewPrivate;
    const QScopedPointer<BookmarksTreeViewPrivate> d;
};
