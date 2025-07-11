#include "RipgrepSearchView.hpp"
#include "RipgrepSearchPlugin.hpp"

#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>
#include <QByteArray>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QProcess>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QVariantMap>
#include <QToolBar>
#include <QAction>

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
    , m_rg(new QProcess(this))
{
    setupUi();
    connectSignals();
}

void RipgrepSearchView::setupUi()
{
    auto toolView = m_mainWindow->createToolView(m_plugin, "RipgrepSearchPlugin",KTextEditor::MainWindow::Left,
                                                 THEME_ICON("search"), tr("Ripgrep Search"));
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

static inline QString jsonPath(const QJsonDocument &json)
{
    return json.object()
        .value("data").toObject()
        .value("path").toObject()
        .value("text").toString();
}

static inline QString jsonMatchedLines(const QJsonDocument &json)
{
    return json.object()
        .value("data").toObject()
        .value("lines").toObject()
        .value("text").toString()
        .trimmed();
}

static inline int jsonLineNumber(const QJsonDocument &json)
{
    return json.object()
        .value("data").toObject()
        .value("line_number").toInt();
}

void RipgrepSearchView::connectSignals()
{
    connect(m_searchEdit, &QLineEdit::editingFinished, [this] {
        emit m_startAction->triggered();
    });
    connect(m_startAction, &QAction::triggered, [this] {
        m_searchResults->clear();
        launchSearch();
    });

    connect(m_rg, &QProcess::readyReadStandardOutput, [this] {
        auto lines = m_rg->readAllStandardOutput().split('\n');
        for (const auto &line : lines) {
            processMatch(line.trimmed());
        }
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

void RipgrepSearchView::launchSearch()
{
    auto editor = KTextEditor::Editor::instance();
    auto cwd = baseDir();
    if (cwd.isEmpty()) {
        return;
    }

    if (m_rg->state() != QProcess::NotRunning) {
        m_rg->kill();
        m_rg->waitForFinished();
    }

    QStringList args;
    args << "-u" << "--json" << "-e" << m_searchEdit->text() << cwd;
    m_rg->start("/usr/bin/rg", args);
}

void RipgrepSearchView::processMatch(const QByteArray &match)
{
    if (match.isEmpty())
        return;

    QJsonParseError err;
    auto json = QJsonDocument::fromJson(match, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << err.errorString();
        return;
    }

    auto type = json.object().value("type").toString();
    if (type == "begin") {
        m_currentItem = new QTreeWidgetItem();
        m_searchResults->addTopLevelItem(m_currentItem);
        QString fileName = jsonPath(json);
        m_currentItem->setIcon(0, THEME_ICON("document-multiple"));
        m_currentItem->setText(0, QFileInfo(fileName).fileName());
        m_currentItem->setData(0, FileNameRole, fileName);
        m_currentItem->setData(0, LineNumberRole, 1);
    } else if (type == "match") {
        auto resultItem = new QTreeWidgetItem();
        m_currentItem->addChild(resultItem);
        QString fileName = jsonPath(json);
        QString matched = jsonMatchedLines(json);
        int lineNumber = jsonLineNumber(json);
        resultItem->setText(0, QString("%1: %2").arg(lineNumber).arg(matched));
        resultItem->setData(0, FileNameRole, fileName);
        resultItem->setData(0, LineNumberRole, lineNumber);
    }
}
