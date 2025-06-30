#pragma once

#include <KTextEditor/Plugin>
#include <QObject>
#include <QListWidget>

class KateBookmarksPlugin : public KTextEditor::Plugin
{
    Q_OBJECT

public:
    explicit KateBookmarksPlugin(QObject *parent = nullptr, const QList<QVariant> & = QList<QVariant>());
    ~KateBookmarksPlugin() override;

public Q_SLOTS:
    void refreshBookmarks();

private:
    void createToolView();

    QWidget *m_toolView;
    QListWidget *m_listWidget;
};