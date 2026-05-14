#pragma once
#include <QObject>
#include <KXMLGUIClient>

namespace KTextEditor
{
class MainWindow;
}
class RipgrepSearchPlugin;
class RipgrepSearchViewPrivate;

class RipgrepSearchView : public QObject, public KXMLGUIClient
{
    Q_OBJECT
public:
    explicit RipgrepSearchView(RipgrepSearchPlugin *plugin, KTextEditor::MainWindow *mainWindow);
    ~RipgrepSearchView();

private:
    friend RipgrepSearchViewPrivate;
    const QScopedPointer<RipgrepSearchViewPrivate> d;
};