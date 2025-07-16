#pragma once

#include <KTextEditor/MainWindow>

#include <QAction>
#include <QLineEdit>
#include <QProcess>
#include <QTreeWidget>

class RipgrepSearchPlugin;
class RipgrepCommand;

class RipgrepSearchView : public QObject
{
    Q_OBJECT
public:
    explicit RipgrepSearchView(RipgrepSearchPlugin *plugin, KTextEditor::MainWindow *mainWindow);

private slots:
    void setupUi();
    void connectSignals();
    void startSearch();

private:
    enum ItemDataRole {
        FileNameRole = Qt::UserRole,
        LineNumberRole,
        ColumnRole,
    };

    QString projectBaseDir();
    QList<QString> openedFiles();

    RipgrepSearchPlugin *m_plugin = nullptr;
    KTextEditor::MainWindow *m_mainWindow = nullptr;
    QWidget *m_content = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QAction *m_refreshAction = nullptr;
    QAction *m_clearAction = nullptr;
    QAction *m_wholeWordAction = nullptr;
    QAction *m_caseSensitiveAction = nullptr;
    QAction *m_useRegexAction = nullptr;
    QTreeWidget *m_searchResults = nullptr;
    QTreeWidgetItem *m_currentItem = nullptr;
    RipgrepCommand *m_rg = nullptr;
};