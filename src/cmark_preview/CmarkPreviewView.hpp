#pragma once

#include <QObject>

#include <QScopedPointer>

namespace KTextEditor
{
class MainWindow;
}

class CmarkPreviewPlugin;
class CmarkPreviewViewPrivate;

class CmarkPreviewView : public QObject
{
    Q_OBJECT
public:
    explicit CmarkPreviewView(CmarkPreviewPlugin *plugin, KTextEditor::MainWindow *mainWindow);
    ~CmarkPreviewView() override;

private:
    friend class CmarkPreviewViewPrivate;
    const QScopedPointer<CmarkPreviewViewPrivate> d;
};
