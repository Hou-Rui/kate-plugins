#pragma once

#include <KTextEditor/Plugin>

#include <QScopedPointer>
#include <QVariant>

namespace KTextEditor
{
class MainWindow;
}

class CmarkPreviewPluginPrivate;

class CmarkPreviewPlugin : public KTextEditor::Plugin
{
    Q_OBJECT
public:
    explicit CmarkPreviewPlugin(QObject *parent = nullptr, const QList<QVariant> & = QList<QVariant>());
    ~CmarkPreviewPlugin() override;
    QObject *createView(KTextEditor::MainWindow *mainWindow) override;

private:
    const QScopedPointer<CmarkPreviewPluginPrivate> d;
};
