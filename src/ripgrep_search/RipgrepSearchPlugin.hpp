#pragma once

#include <QList>

#include <KTextEditor/MainWindow>
#include <KTextEditor/Plugin>
#include <KXMLGUIClient>

class RipgrepSearchView;

class RipgrepSearchPlugin : public KTextEditor::Plugin
{
    Q_OBJECT
public:
    explicit RipgrepSearchPlugin(QObject *parent);
    ~RipgrepSearchPlugin() override;
    QObject *createView(KTextEditor::MainWindow *mainWindow) override;

private:
    QList<RipgrepSearchView *> m_views;
};
