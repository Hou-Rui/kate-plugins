#pragma once
#include <KTextEditor/Plugin>

namespace KTextEditor
{
class MainWindow;
}
class RipgrepSearchPluginPrivate;

class RipgrepSearchPlugin : public KTextEditor::Plugin
{
    Q_OBJECT
public:
    explicit RipgrepSearchPlugin(QObject *parent);
    ~RipgrepSearchPlugin() override;
    QObject *createView(KTextEditor::MainWindow *mainWindow) override;

private:
    const QScopedPointer<RipgrepSearchPluginPrivate> d;
};
