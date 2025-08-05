#include "RipgrepSearchPlugin.hpp"
#include "RipgrepCommand.hpp"
#include "SearchResultsView.hpp"

#include <KActionCollection>
#include <KFileItem>
#include <KXMLGUIFactory>
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
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>
#include <QVariantMap>

K_PLUGIN_CLASS_WITH_JSON(RipgrepSearchPlugin, "ripgrep_search.json")

RipgrepSearchView::RipgrepSearchView(RipgrepSearchPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(plugin)
    , m_plugin(plugin)
    , m_mainWindow(mainWindow)
    , m_rg(new RipgrepCommand(this))
{
    setupActions();
    setupUi();
    connectSignals();
}

RipgrepSearchView::~RipgrepSearchView()
{
    m_mainWindow->guiFactory()->removeClient(this);
}

static inline QToolBar *createToolBar(QWidget *parent)
{
    auto toolBar = new QToolBar(parent);
    auto style = parent ? parent->style() : QApplication::style();
    int margin = style->pixelMetric(QStyle::PM_DockWidgetTitleMargin);
    int iconSize = style->pixelMetric(QStyle::PM_ButtonIconSize);
    toolBar->layout()->setContentsMargins(margin, margin, margin, margin);
    toolBar->setIconSize(QSize(iconSize, iconSize));
    return toolBar;
}

void RipgrepSearchView::resetStatusMessage()
{
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Ready to search."));
    }
}

void RipgrepSearchView::setupActions()
{
    KXMLGUIClient::setComponentName("ripgrep_search", tr("RIPGrep Search"));
    m_mainWindow->guiFactory()->addClient(this);
    m_searchSelectionAction = actionCollection()->addAction(QStringLiteral("ripgrep_search_in_files"));
    auto defaultShortcut = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::Key_F);
    KActionCollection::setDefaultShortcut(m_searchSelectionAction, defaultShortcut);
    m_searchSelectionAction->setText(tr("Find in Files using RIPGrep"));
    m_searchSelectionAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
    m_searchSelectionAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
}

void RipgrepSearchView::setupUi()
{
    // clang-format off
    m_toolView = m_mainWindow->createToolView(m_plugin, "RipgrepSearchPlugin",
                                              KTextEditor::MainWindow::Left,
                                              QIcon::fromTheme("search"), tr("Ripgrep Search"));
    // clang-format on

    auto headerBar = createToolBar(m_toolView);
    auto searchLabel = new QLabel(tr("<b>Search</b>"));
    searchLabel->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
    headerBar->addWidget(searchLabel);
    m_refreshAction = new QAction(QIcon::fromTheme("view-refresh"), tr("Refresh"));
    headerBar->addAction(m_refreshAction);
    m_clearAction = new QAction(QIcon::fromTheme("edit-clear-all"), tr("Clear results"));
    headerBar->addAction(m_clearAction);

    auto searchBar = createToolBar(m_toolView);
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("Search"));
    searchBar->addWidget(m_searchEdit);
    m_wholeWordAction = new QAction(QIcon::fromTheme("ime-punctuation-fullwidth"), tr("Match whole words"));
    m_wholeWordAction->setCheckable(true);
    searchBar->addAction(m_wholeWordAction);
    m_caseSensitiveAction = new QAction(QIcon::fromTheme("format-text-superscript"), tr("Case sensitive"));
    m_caseSensitiveAction->setCheckable(true);
    searchBar->addAction(m_caseSensitiveAction);
    m_useRegexAction = new QAction(QIcon::fromTheme("code-context"), tr("Use regular expression"));
    m_useRegexAction->setCheckable(true);
    searchBar->addAction(m_useRegexAction);

    m_resultsModel = new SearchResultsModel(this);
    m_resultsView = new SearchResultsView(m_resultsModel, m_toolView);
    m_resultsView->setHeaderHidden(true);
    m_resultsView->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    m_statusBar = new QStatusBar(m_toolView);
    resetStatusMessage();
}

void RipgrepSearchView::connectSignals()
{
    connect(m_clearAction, &QAction::triggered, [this] {
        m_searchEdit->clear();
        m_resultsModel->clear();
        resetStatusMessage();
    });

    connect(m_refreshAction, &QAction::triggered, this, &RipgrepSearchView::startSearch);
    connect(m_searchEdit, &QLineEdit::editingFinished, this, &RipgrepSearchView::startSearch);
    connect(m_wholeWordAction, &QAction::triggered, m_rg, &RipgrepCommand::setWholeWord);
    connect(m_caseSensitiveAction, &QAction::triggered, m_rg, &RipgrepCommand::setCaseSensitive);
    connect(m_useRegexAction, &QAction::triggered, m_rg, &RipgrepCommand::setUseRegex);
    connect(m_searchSelectionAction, &QAction::triggered, [this] {
        if (!m_toolView->isVisible())
            m_mainWindow->showToolView(m_toolView);
        if (auto view = m_mainWindow->activeView(); view && view->selection()) {
            auto selectionText = view->selectionText().trimmed();
            m_searchEdit->setText(selectionText);
            startSearch();
        }
    });

    connect(m_rg, &RipgrepCommand::searchOptionsChanged, this, &RipgrepSearchView::startSearch);
    connect(m_rg, &RipgrepCommand::matchFoundInFile, m_resultsModel, &SearchResultsModel::addMatchedFile);
    connect(m_rg, &RipgrepCommand::matchFound, m_resultsModel, &SearchResultsModel::addMatched);
    connect(m_rg, &RipgrepCommand::searchFinished, [this](int found, int nanos) {
        auto seconds = QString::number(nanos / 1000000000.0, 'f', 6);
        auto results = found == 1 ? tr("result") : tr("results");
        m_statusBar->showMessage(tr("Found %1 %2 in %3 seconds.").arg(found).arg(results).arg(seconds));
    });

    connect(m_resultsView, &SearchResultsView::jumpToFile, [this](const QString &file) {
        m_mainWindow->openUrl(QUrl::fromLocalFile(file));
    });
    connect(m_resultsView, &SearchResultsView::jumpToResult, [this](const QString &file, int line, int start, int end) {
        if (auto view = m_mainWindow->openUrl(QUrl::fromLocalFile(file))) {
            line--;
            view->setCursorPosition(KTextEditor::Cursor(line, start));
            view->setSelection(KTextEditor::Range(line, start, line, end));
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

    m_statusBar->showMessage(tr("Searching..."));
    m_resultsModel->clear();
    if (auto baseDir = projectBaseDir(); !baseDir.isEmpty()) {
        m_rg->searchInDir(term, baseDir);
    } else if (auto files = openedFiles(); !files.isEmpty()) {
        m_rg->searchInFiles(term, files);
    } else {
        qInfo() << "No opened documents, not performing searching.";
    }
}

#include "RipgrepSearchPlugin.moc"
