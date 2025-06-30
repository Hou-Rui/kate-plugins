#pragma once

#include <QListWidget>
#include <QObject>

#include <KTextEditor/Document>
#include <KTextEditor/Plugin>
#include <KTextEditor/View>
#include <KXMLGUIClient>

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
    void refreshAllBookmarks();
    void refreshBookmarks(KTextEditor::Document *document);
    void jumpToBookmark(QListWidgetItem *item);

private:
    void showMessage(const QString &msg);

    KTextEditor::MainWindow *m_mainWindow = nullptr;
    QWidget *m_toolView = nullptr;
    QListWidget *m_listWidget = nullptr;
};