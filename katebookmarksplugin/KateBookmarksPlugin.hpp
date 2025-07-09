#pragma once

#include <QTreeWidget>
#include <QObject>

#include <KTextEditor/Document>
#include <KTextEditor/Plugin>
#include <KTextEditor/View>
#include <KToolBar>

class KateBookmarksPlugin : public KTextEditor::Plugin
{
    Q_OBJECT
public:
    explicit KateBookmarksPlugin(QObject *parent = nullptr, const QList<QVariant> & = QList<QVariant>());
    QObject *createView(KTextEditor::MainWindow *mainWindow) override;
};

class KateBookmarksView : public QObject
{
    Q_OBJECT
public:
    explicit KateBookmarksView(KateBookmarksPlugin *plugin, KTextEditor::MainWindow *mainWindow);

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

    KateBookmarksPlugin *m_plugin = nullptr;
    KTextEditor::MainWindow *m_mainWindow = nullptr;
    QWidget *m_toolView = nullptr;
    KToolBar *m_toolBar = nullptr;
    QTreeWidget *m_treeWidget = nullptr;
};