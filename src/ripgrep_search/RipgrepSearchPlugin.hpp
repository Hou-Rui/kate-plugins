#pragma once

#include <KTextEditor/MainWindow>
#include <KTextEditor/Plugin>
#include <KXMLGUIClient>

#include <QAction>
#include <QComboBox>
#include <QStatusBar>
#include <QTreeView>

class RipgrepCommand;
class RipgrepSearchPlugin;
class SearchResultsModel;
class SearchResultsView;

class RipgrepSearchView : public QObject, public KXMLGUIClient
{
    Q_OBJECT
public:
    explicit RipgrepSearchView(RipgrepSearchPlugin *plugin, KTextEditor::MainWindow *mainWindow);
    ~RipgrepSearchView();

private slots:
    void setupActions();
    void setupUi();
    void connectSignals();
    void startSearch();
    void resetStatusMessage();

private:
    QString projectBaseDir();
    QList<QString> openedFiles();

    RipgrepSearchPlugin *m_plugin = nullptr;
    KTextEditor::MainWindow *m_mainWindow = nullptr;
    QWidget *m_toolView = nullptr;
    QComboBox *m_searchBox = nullptr;
    QAction *m_searchSelectionAction = nullptr;
    QAction *m_refreshAction = nullptr;
    QAction *m_clearAction = nullptr;
    QAction *m_wholeWordAction = nullptr;
    QAction *m_caseSensitiveAction = nullptr;
    QAction *m_useRegexAction = nullptr;
    SearchResultsModel *m_resultsModel = nullptr;
    SearchResultsView *m_resultsView = nullptr;
    QStatusBar *m_statusBar = nullptr;
    RipgrepCommand *m_rg = nullptr;
};

class RipgrepSearchPlugin : public KTextEditor::Plugin
{
    Q_OBJECT
public:
    explicit RipgrepSearchPlugin(QObject *parent = nullptr, const QList<QVariant> & = QList<QVariant>())
        : KTextEditor::Plugin(parent)
    {
    }
    QObject *createView(KTextEditor::MainWindow *mainWindow) override
    {
        return new RipgrepSearchView(this, mainWindow);
    }
};
