#pragma once

#include <QTreeWidget>
#include <QObject>

#include <KTextEditor/Document>
#include <KTextEditor/Plugin>
#include <KTextEditor/View>

class BookmarksTreePlugin : public KTextEditor::Plugin
{
    Q_OBJECT
public:
    explicit BookmarksTreePlugin(QObject *parent = nullptr, const QList<QVariant> & = QList<QVariant>());
    QObject *createView(KTextEditor::MainWindow *mainWindow) override;
};

class BookmarksTreeView : public QObject
{
    Q_OBJECT
public:
    explicit BookmarksTreeView(BookmarksTreePlugin *plugin, KTextEditor::MainWindow *mainWindow);
    
public Q_SLOTS:
    void clearAllBookmarks();
    void clearBookmarks(KTextEditor::Document *document);
    void refreshAllBookmarks();
    void refreshBookmarks(KTextEditor::Document *document);
    void jumpToBookmark(KTextEditor::Document *document, KTextEditor::Mark *mark);

private:
    enum ItemDataRole {
        DocumentRole = Qt::UserRole,
        MarkRole = Qt::UserRole + 1,
    };
    void showMessage(const QString &msg);
    void setupUi();
    void connectSignals();

    BookmarksTreePlugin *m_plugin = nullptr;
    KTextEditor::MainWindow *m_mainWindow = nullptr;
    QWidget *m_toolView = nullptr;
    QTreeWidget *m_treeWidget = nullptr;
};