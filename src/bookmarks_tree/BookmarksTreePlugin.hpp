#pragma once
#include <KTextEditor/Plugin>

namespace KTextEditor
{
class MainWindow;
}
class BookmarksTreePluginPrivate;

class BookmarksTreePlugin : public KTextEditor::Plugin
{
    Q_OBJECT
public:
    explicit BookmarksTreePlugin(QObject *parent = nullptr, const QList<QVariant> & = QList<QVariant>());
    ~BookmarksTreePlugin() override;
    QObject *createView(KTextEditor::MainWindow *mainWindow) override;

private:
    const QScopedPointer<BookmarksTreePluginPrivate> d;
};
