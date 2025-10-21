#include "RipgrepSearchView.hpp"
#include "RipgrepCommand.hpp"
#include "RipgrepSearchPlugin.hpp"
#include "SearchResultsView.hpp"

#include <KActionCollection>
#include <KFileItem>
#include <KPluginFactory>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
#include <KTextEditor/Range>
#include <KTextEditor/View>
#include <KXMLGUIFactory>

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QFileInfo>
#include <QFormLayout>
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

RipgrepSearchView::RipgrepSearchView(RipgrepSearchPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(plugin)
    , m_plugin(plugin)
    , m_mainWindow(mainWindow)
    , m_rg(new RipgrepCommand(this))
{
    setupActions();
    setupUi();
    setupRipgrepProcess();
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

static inline QToolBar *createSeparator(QWidget *parent)
{
    auto toolBar = new QToolBar(parent);
    toolBar->layout()->setContentsMargins(0, 0, 0, 0);
    auto separator = new QFrame();
    separator->setFrameStyle(QFrame::VLine);
    toolBar->addWidget(separator);
    return toolBar;
}

void RipgrepSearchView::resetStatusMessage()
{
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Ready to search."));
    }
}

QAction *RipgrepSearchView::addAction(const QString &name, const QString &iconName, const QString &text)
{
    auto action = actionCollection()->addAction(name);
    action->setIcon(QIcon::fromTheme(iconName));
    action->setText(text);
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    return action;
}

QAction *RipgrepSearchView::addCheckableAction(const QString &name, const QString &iconName, const QString &text)
{
    auto action = addAction(name, iconName, text);
    action->setCheckable(true);
    return action;
}

void RipgrepSearchView::setupActions()
{
    KXMLGUIClient::setComponentName("kate_ripgrep_search", tr("RIPGrep Search"));
    setXMLFile("actions.rc");

    m_searchSelectionAction = addAction("ripgrep_search_in_files", "edit-find", tr("Find in Files using RIPGrep"));
    KActionCollection::setDefaultShortcut(m_searchSelectionAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::Key_F));
    connect(m_searchSelectionAction, &QAction::triggered, this, &RipgrepSearchView::searchSelection);

    m_refreshAction = addAction("ripgrep_refresh", "view-refresh", tr("Refresh"));
    connect(m_refreshAction, &QAction::triggered, this, &RipgrepSearchView::startSearch);

    m_clearAction = addAction("ripgrep_clear", "edit-clear-all", tr("Clear results"));
    connect(m_clearAction, &QAction::triggered, this, &RipgrepSearchView::clearResults);

    m_wholeWordAction = addCheckableAction("ripgrep_whole_word", "ime-punctuation-fullwidth", tr("Match whole words"));
    connect(m_wholeWordAction, &QAction::triggered, m_rg, &RipgrepCommand::setWholeWord);

    m_caseSensitiveAction = addCheckableAction("ripgrep_case_sensitive", "format-text-superscript", tr("Case sensitive"));
    connect(m_caseSensitiveAction, &QAction::triggered, m_rg, &RipgrepCommand::setCaseSensitive);

    m_useRegexAction = addCheckableAction("ripgrep_use_regex", "code-context", tr("Use regular expression"));
    connect(m_useRegexAction, &QAction::triggered, m_rg, &RipgrepCommand::setUseRegex);

    m_showAdvancedAction = addCheckableAction("ripgrep_show_advanced", "overflow-menu", tr("Show advanced options"));
    connect(m_showAdvancedAction, &QAction::triggered, this, &RipgrepSearchView::showAdvancedChanged);

    m_mainWindow->guiFactory()->addClient(this);
}

inline static QComboBox *createEditableComboBox()
{
    auto comboBox = new QComboBox();
    comboBox->setEditable(true);
    comboBox->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    return comboBox;
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
    headerBar->addAction(m_refreshAction);
    headerBar->addAction(m_clearAction);
    headerBar->addAction(m_showAdvancedAction);

    auto searchBar = createToolBar(m_toolView);
    m_searchBox = createEditableComboBox();
    connect(m_searchBox->lineEdit(), &QLineEdit::returnPressed, this, &RipgrepSearchView::startSearch);
    searchBar->addWidget(m_searchBox);
    searchBar->addAction(m_wholeWordAction);
    searchBar->addAction(m_caseSensitiveAction);
    searchBar->addAction(m_useRegexAction);

    auto includeBar = createToolBar(m_toolView);
    includeBar->setVisible(false);
    connect(this, &RipgrepSearchView::showAdvancedChanged, includeBar, &QToolBar::setVisible);
    auto filterContainer = new QWidget();
    includeBar->addWidget(filterContainer);
    auto filterForm = new QFormLayout(filterContainer);
    filterForm->setContentsMargins(0, 0, 0, 0);
    filterForm->addRow(tr("Include:"), m_includeFileBox = createEditableComboBox());
    filterForm->addRow(tr("Exclude:"), m_excludeFileBox = createEditableComboBox());

    m_resultsModel = new SearchResultsModel(this);
    m_resultsView = new SearchResultsView(m_resultsModel, m_toolView);
    m_resultsView->setHeaderHidden(true);
    m_resultsView->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
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

    m_statusBar = new QStatusBar(m_toolView);
    resetStatusMessage();
}

void RipgrepSearchView::setupRipgrepProcess()
{
    connect(m_rg, &RipgrepCommand::searchOptionsChanged, this, &RipgrepSearchView::startSearch);
    connect(m_rg, &RipgrepCommand::matchFoundInFile, m_resultsModel, &SearchResultsModel::addMatchedFile);
    connect(m_rg, &RipgrepCommand::matchFound, m_resultsModel, &SearchResultsModel::addMatched);
    connect(m_rg, &RipgrepCommand::searchFinished, [this](int found, int nanos) {
        auto seconds = QString::number(nanos / 1000000000.0, 'f', 6);
        auto results = found == 1 ? tr("result") : tr("results");
        m_statusBar->showMessage(tr("Found %1 %2 in %3 seconds.").arg(found).arg(results).arg(seconds));
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
    auto term = m_searchBox->currentText();
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

void RipgrepSearchView::searchSelection()
{
    if (!m_toolView->isVisible())
        m_mainWindow->showToolView(m_toolView);

    if (auto view = m_mainWindow->activeView(); view && view->selection()) {
        auto selectionText = view->selectionText().trimmed();
        m_searchBox->addItem(selectionText);
        m_searchBox->setCurrentIndex(m_searchBox->count() - 1);
        startSearch();
    }
}

void RipgrepSearchView::clearResults()
{
    m_searchBox->clear();
    m_resultsModel->clear();
    resetStatusMessage();
}
