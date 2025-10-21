#pragma once

#include <QAction>
#include <QComboBox>
#include <QStatusBar>
#include <QTreeView>

#include <KTextEditor/MainWindow>
#include <KTextEditor/Plugin>
#include <KXMLGUIClient>

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
    void setupRipgrepProcess();
    void startSearch();
    void searchSelection();
    void resetStatusMessage();
    void clearResults();

private:
    QString projectBaseDir();
    QList<QString> openedFiles();
    QAction *addAction(const QString &name, const QString &iconName, const QString &text);
    QAction *addCheckableAction(const QString &name, const QString &iconName, const QString &text);

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
    QAction *m_showAdvancedAction = nullptr;
    QComboBox *m_includeFileBox = nullptr;
    QComboBox *m_excludeFileBox = nullptr;
    SearchResultsModel *m_resultsModel = nullptr;
    SearchResultsView *m_resultsView = nullptr;
    QStatusBar *m_statusBar = nullptr;
    RipgrepCommand *m_rg = nullptr;
};