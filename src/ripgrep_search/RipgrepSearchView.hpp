#pragma once

#include <KTextEditor/MainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QProcess>

class RipgrepSearchPlugin;

class RipgrepSearchView : public QObject
{
    Q_OBJECT
public:
    explicit RipgrepSearchView(RipgrepSearchPlugin *plugin, KTextEditor::MainWindow *mainWindow);

private:
    enum ItemDataRole {
        FileNameRole = Qt::UserRole,
        LineNumberRole = Qt::UserRole + 1,
    };
    void showMessage(const QString &msg);
    void setupUi();
    void connectSignals();
    void launchSearch();
    void processMatch(const QByteArray &match);
    QString baseDir();

    RipgrepSearchPlugin *m_plugin = nullptr;
    KTextEditor::MainWindow *m_mainWindow = nullptr;
    QWidget *m_content = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_startButton = nullptr;
    QTreeWidget *m_searchResults = nullptr;
    QTreeWidgetItem *m_currentItem = nullptr;
    QProcess *m_rg = nullptr;
};