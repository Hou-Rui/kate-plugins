#include "RipgrepSearchPlugin.hpp"
#include "RipgrepCommand.hpp"
#include "SearchResultsView.hpp"

#include <KFileItem>
#include <KPluginFactory>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
#include <KTextEditor/Range>
#include <KTextEditor/View>

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QSizePolicy>
#include <QStyledItemDelegate>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>
#include <QVariantMap>

#define L(literal) QStringLiteral(literal)
#define THEME_ICON(name) QIcon::fromTheme(QStringLiteral(name))

K_PLUGIN_CLASS_WITH_JSON(RipgrepSearchPlugin, "ripgrep_search.json")

RipgrepSearchView::RipgrepSearchView(RipgrepSearchPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(plugin)
    , m_plugin(plugin)
    , m_mainWindow(mainWindow)
    , m_rg(new RipgrepCommand(this))
{
    setupUi();
    connectSignals();
}

static inline QToolBar *createToolBar(QWidget *parent)
{
    auto toolBar = new QToolBar(parent);
    toolBar->layout()->setContentsMargins(2, 2, 2, 2);
    auto iconSize = parent->style()->pixelMetric(QStyle::PM_ButtonIconSize, nullptr);
    toolBar->setIconSize(QSize(iconSize, iconSize));
    return toolBar;
}

void RipgrepSearchView::setupUi()
{
    // clang-format off
    auto toolView = m_mainWindow->createToolView(m_plugin, "RipgrepSearchPlugin",
                                                 KTextEditor::MainWindow::Left,
                                                 THEME_ICON("search"), tr("Ripgrep Search"));
    // clang-format on

    auto headerBar = createToolBar(toolView);
    auto searchLabel = new QLabel(tr("<b>Search</b>"));
    searchLabel->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
    headerBar->addWidget(searchLabel);
    m_refreshAction = new QAction(THEME_ICON("view-refresh"), tr("Refresh"));
    headerBar->addAction(m_refreshAction);
    m_clearAction = new QAction(THEME_ICON("edit-clear-all"), tr("Clear results"));
    headerBar->addAction(m_clearAction);

    auto searchBar = createToolBar(toolView);
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("Search"));
    searchBar->addWidget(m_searchEdit);
    m_wholeWordAction = new QAction(THEME_ICON("ime-punctuation-fullwidth"), tr("Match whole words"));
    m_wholeWordAction->setCheckable(true);
    searchBar->addAction(m_wholeWordAction);
    m_caseSensitiveAction = new QAction(THEME_ICON("format-text-subscript"), tr("Case sensitive"));
    m_caseSensitiveAction->setCheckable(true);
    searchBar->addAction(m_caseSensitiveAction);
    m_useRegexAction = new QAction(THEME_ICON("code-context"), tr("Use regular expression"));
    m_useRegexAction->setCheckable(true);
    searchBar->addAction(m_useRegexAction);

    auto resultsView = new SearchResultsView(toolView);
    resultsView->setHeaderHidden(true);
    resultsView->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    m_resultsModel = resultsView->model();
}

static inline QIcon iconForFile(const QString &filePath)
{
    KFileItem item(QUrl::fromLocalFile(filePath), QString(), KFileItem::Unknown);
    return QIcon::fromTheme(item.iconName());
}

void RipgrepSearchView::connectSignals()
{
    connect(m_clearAction, &QAction::triggered, [this] {
        m_searchEdit->clear();
        m_resultsModel->clear();
    });

    connect(m_refreshAction, &QAction::triggered, this, &RipgrepSearchView::startSearch);
    connect(m_searchEdit, &QLineEdit::editingFinished, this, &RipgrepSearchView::startSearch);
    connect(m_rg, &RipgrepCommand::searchOptionsChanged, this, &RipgrepSearchView::startSearch);
    connect(m_wholeWordAction, &QAction::triggered, m_rg, &RipgrepCommand::setWholeWord);
    connect(m_caseSensitiveAction, &QAction::triggered, m_rg, &RipgrepCommand::setCaseSensitive);
    connect(m_useRegexAction, &QAction::triggered, m_rg, &RipgrepCommand::setUseRegex);

    connect(m_rg, &RipgrepCommand::matchFoundInFile, [this](const QString &fileName) {
        m_currentItem = new QTreeWidgetItem();
        m_searchResults->addTopLevelItem(m_currentItem);
        m_currentItem->setIcon(0, iconForFile(fileName));
        m_currentItem->setText(0, QFileInfo(fileName).fileName());
        m_currentItem->setData(0, FileNameRole, fileName);
    });

    connect(m_rg, &RipgrepCommand::matchFound, [this](RipgrepCommand::Result result) {
        auto resultItem = new QTreeWidgetItem();
        m_currentItem->addChild(resultItem);
        resultItem->setData(0, FileNameRole, result.fileName);
        resultItem->setData(0, LineNumberRole, result.lineNumber);
        if (!result.submatches.isEmpty()) {
            const auto &[start, end] = result.submatches.front();
            resultItem->setData(0, StartColumnRole, start);
            resultItem->setData(0, EndColumnRole, end);
        }
        auto resultWidget = new QLabel(renderSearchResult(result));
        m_searchResults->setItemWidget(resultItem, 0, resultWidget);
    });

    connect(m_searchResults, &QTreeWidget::itemClicked, [this](QTreeWidgetItem *item, auto) {
        auto fileName = item->data(0, FileNameRole).toString();
        if (auto view = m_mainWindow->openUrl(QUrl::fromLocalFile(fileName))) {
            if (auto lineRole = item->data(0, LineNumberRole); lineRole.isValid()) {
                auto line = lineRole.toInt() - 1;
                auto start = item->data(0, StartColumnRole).toInt();
                auto end = item->data(0, EndColumnRole).toInt();
                view->setCursorPosition(KTextEditor::Cursor(line, start));
                view->setSelection(KTextEditor::Range(line, start, line, end));
            }
        }
    });
}

QString RipgrepSearchView::projectBaseDir()
{
    if (auto projectPlugin = m_mainWindow->pluginView("kateprojectplugin")) {
        return projectPlugin->property("projectBaseDir").toString();
    }
    return QString();
}

QList<QString> RipgrepSearchView::openedFiles()
{
    QList<QString> result;
    auto editor = KTextEditor::Editor::instance();
    for (auto doc : editor->documents()) {
        if (doc->url().isLocalFile()) {
            auto fileName = doc->url().toLocalFile();
            if (QFileInfo::exists(fileName))
                result.append(fileName);
        }
    }
    return result;
}

void RipgrepSearchView::startSearch()
{
    auto term = m_searchEdit->text();
    if (term.isEmpty())
        return;

    m_searchResults->clear();
    if (auto baseDir = projectBaseDir(); !baseDir.isEmpty()) {
        m_rg->searchInDir(term, baseDir);
    } else if (auto files = openedFiles(); !files.isEmpty()) {
        m_rg->searchInFiles(term, files);
    } else {
        qInfo() << "No opened documents, not performing searching.";
    }
}

#include "RipgrepSearchPlugin.moc"