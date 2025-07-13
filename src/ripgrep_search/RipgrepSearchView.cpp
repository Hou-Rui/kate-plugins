#include "RipgrepSearchView.hpp"
#include "RipgrepCommand.hpp"
#include "RipgrepSearchPlugin.hpp"

#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>
#include <QAction>
#include <QByteArray>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QProcess>
#include <QSizePolicy>
#include <QToolBar>
#include <QVBoxLayout>
#include <QVariantMap>

#define L(literal) QStringLiteral(literal)
#define THEME_ICON(name) QIcon::fromTheme(QStringLiteral(name))

void RipgrepSearchView::showMessage(const QString &msg)
{
    QVariantMap map {
        {L("category"), L("Ripgrep Search")},
        {L("categoryIcon"), THEME_ICON("search")},
        {L("type"), L("Log")},
        {L("text"), msg}
    };
    QMetaObject::invokeMethod(m_mainWindow->parent(), "showMessage", Qt::DirectConnection, Q_ARG(QVariantMap, map));
}

RipgrepSearchView::RipgrepSearchView(RipgrepSearchPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(mainWindow)
    , m_plugin(plugin)
    , m_mainWindow(mainWindow)
    , m_rg(new RipgrepCommand(this))
{
    setupUi();
    connectSignals();
}

void RipgrepSearchView::setupUi()
{
    auto toolView = m_mainWindow->createToolView(m_plugin, "RipgrepSearchPlugin", KTextEditor::MainWindow::Left, THEME_ICON("search"), tr("Ripgrep Search"));
    auto toolBar = new QToolBar(toolView);

    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("Search"));
    toolBar->addWidget(m_searchEdit);

    m_startAction = new QAction(THEME_ICON("search"), "Search");
    toolBar->addAction(m_startAction);

    m_searchResults = new QTreeWidget(toolView);
    m_searchResults->setHeaderHidden(true);
    m_searchResults->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
}

void RipgrepSearchView::connectSignals()
{
    connect(m_searchEdit, &QLineEdit::editingFinished, [this] {
        emit m_startAction->triggered();
    });

    connect(m_startAction, &QAction::triggered, [this] {
        m_searchResults->clear();
        auto editor = KTextEditor::Editor::instance();
        if (auto cwd = baseDir(); !cwd.isEmpty()) {
            m_rg->setWorkingDirectory(cwd);
            m_rg->search(m_searchEdit->text());
        }
    });

    connect(m_rg, &RipgrepCommand::matchFoundInFile, [this](const QString &fileName) {
        m_currentItem = new QTreeWidgetItem();
        m_searchResults->addTopLevelItem(m_currentItem);
        m_currentItem->setIcon(0, THEME_ICON("document-multiple"));
        m_currentItem->setText(0, QFileInfo(fileName).fileName());
        m_currentItem->setData(0, FileNameRole, fileName);
        m_currentItem->setData(0, LineNumberRole, 1);
    });
    
    connect(m_rg, &RipgrepCommand::matchFound, [this](RipgrepCommand::Result result) {
        auto resultItem = new QTreeWidgetItem();
        m_currentItem->addChild(resultItem);
        resultItem->setText(0, QString("%1: %2").arg(result.lineNumber).arg(result.line));
        resultItem->setData(0, FileNameRole, result.fileName);
        resultItem->setData(0, LineNumberRole, result.lineNumber);
    });

    connect(m_searchResults, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem *item, auto) {
        auto fileName = item->data(0, FileNameRole).toString();
        auto lineNumber = item->data(0, LineNumberRole).toInt();
        auto editor = KTextEditor::Editor::instance();
        if (auto view = m_mainWindow->openUrl(QUrl::fromLocalFile(fileName))) {
            view->setCursorPosition(KTextEditor::Cursor(lineNumber - 1, 0));
        }
    });
}

QString RipgrepSearchView::baseDir()
{
    if (auto projectPlugin = m_mainWindow->pluginView(L("kateprojectplugin"))) {
        return projectPlugin->property("projectBaseDir").toString();
    }
    qWarning() << "No project..?";
    return "";
}
