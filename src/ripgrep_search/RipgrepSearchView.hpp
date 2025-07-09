#pragma once

#include <KTextEditor/MainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>

class RipgrepSearchPlugin;

class RipgrepSearchView : public QObject
{
    Q_OBJECT
public:
    explicit RipgrepSearchView(RipgrepSearchPlugin *plugin, KTextEditor::MainWindow *mainWindow);

public Q_SLOTS:

private:
    enum ItemDataRole {
        FileNameRole = Qt::UserRole,
        LineNumberRole = Qt::UserRole + 1,
    };
    void showMessage(const QString &msg);
    void setupUi();
    void connectSignals();
    void launchSearch();
    QString workingDir();

    RipgrepSearchPlugin *m_plugin = nullptr;
    KTextEditor::MainWindow *m_mainWindow = nullptr;
    QWidget *m_toolView = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_startButton = nullptr;
    QTreeWidget *m_searchResults = nullptr;
    QTreeWidgetItem *m_currentItem = nullptr;
};