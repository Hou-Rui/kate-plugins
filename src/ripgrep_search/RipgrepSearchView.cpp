#include "RipgrepSearchView.hpp"
#include "RipgrepCommand.hpp"
#include "RipgrepSearchPlugin.hpp"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
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
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>
#include <QVariantMap>

#define L(literal) QStringLiteral(literal)
#define THEME_ICON(name) QIcon::fromTheme(QStringLiteral(name))

RipgrepSearchView::RipgrepSearchView(RipgrepSearchPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(mainWindow)
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
    auto toolView = m_mainWindow->createToolView(m_plugin, "RipgrepSearchPlugin", KTextEditor::MainWindow::Left, THEME_ICON("search"), tr("Ripgrep Search"));

    auto headerBar = createToolBar(toolView);
    auto searchLabel = new QLabel(tr("<b>Search</b>"));
    searchLabel->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
    searchLabel->setIndent(4);
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

    m_searchResults = new QTreeWidget(toolView);
    m_searchResults->setHeaderHidden(true);
    m_searchResults->setAnimated(true);
    m_searchResults->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
}

static QString renderSearchResult(const RipgrepCommand::Result &result)
{
    QString output;
    QTextStream builder(&output);
    int mark = 0;
    for (auto [start, end] : result.submatches) {
        auto palette = QApplication::palette();
        auto highlight = palette.highlight().color().name();
        auto color = palette.highlightedText().color().name();
        builder << result.line.sliced(mark, start - mark).toHtmlEscaped() << "<span style=\"background-color: " << highlight << "; color: " << color << "\">"
                << result.line.sliced(start, end - start).toHtmlEscaped() << "</span>";
        mark = end;
    }
    builder << result.line.sliced(mark).toHtmlEscaped();
    return output;
}

void RipgrepSearchView::connectSignals()
{
    connect(m_clearAction, &QAction::triggered, [this] {
        m_searchEdit->clear();
        m_searchResults->clear();
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
        m_currentItem->setIcon(0, THEME_ICON("document-multiple"));
        m_currentItem->setText(0, QFileInfo(fileName).fileName());
        m_currentItem->setData(0, FileNameRole, fileName);
        m_currentItem->setData(0, LineNumberRole, 1);
        m_currentItem->setData(0, ColumnRole, 0);
    });

    connect(m_rg, &RipgrepCommand::matchFound, [this](RipgrepCommand::Result result) {
        auto resultItem = new QTreeWidgetItem();
        m_currentItem->addChild(resultItem);
        resultItem->setData(0, FileNameRole, result.fileName);
        resultItem->setData(0, LineNumberRole, result.lineNumber);
        int column = result.submatches.isEmpty() ? 0 : result.submatches.front().start;
        resultItem->setData(0, ColumnRole, column);
        auto resultWidget = new QLabel(renderSearchResult(result));
        m_searchResults->setItemWidget(resultItem, 0, resultWidget);
    });

    connect(m_searchResults, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem *item, auto) {
        auto fileName = item->data(0, FileNameRole).toString();
        auto lineNumber = item->data(0, LineNumberRole).toInt();
        auto column = item->data(0, ColumnRole).toInt();
        auto editor = KTextEditor::Editor::instance();
        if (auto view = m_mainWindow->openUrl(QUrl::fromLocalFile(fileName))) {
            view->setCursorPosition(KTextEditor::Cursor(lineNumber - 1, column));
        }
    });
}

QString RipgrepSearchView::projectBaseDir()
{
    if (auto projectPlugin = m_mainWindow->pluginView(L("kateprojectplugin"))) {
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
