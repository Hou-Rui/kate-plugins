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
#include <QComboBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QSizePolicy>
#include <QStatusBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>
#include <QVariantMap>

class RipgrepSearchViewPrivate : public QObject
{
    Q_OBJECT
public slots:
    void setupActions();
    void setupUi();
    void setupRipgrepProcess();
    void startSearch();
    void searchSelection();
    void resetStatusMessage();
    void clearResults();

public:
    QString projectBaseDir();
    QStringList openedFiles();
    QAction *addAction(const QString &name, const QString &iconName, const QString &text);
    QAction *addCheckableAction(const QString &name, const QString &iconName, const QString &text);
    QComboBox *createEditableComboBox(const QString &placeholderText);

    RipgrepSearchView *q;
    RipgrepSearchPlugin *plugin = nullptr;
    KTextEditor::MainWindow *mainWindow = nullptr;
    QWidget *toolView = nullptr;
    QComboBox *searchBox = nullptr;
    QAction *searchSelectionAction = nullptr;
    QAction *refreshAction = nullptr;
    QAction *clearAction = nullptr;
    QAction *wholeWordAction = nullptr;
    QAction *caseSensitiveAction = nullptr;
    QAction *useRegexAction = nullptr;
    QAction *showAdvancedAction = nullptr;
    QComboBox *includeFileBox = nullptr;
    QComboBox *excludeFileBox = nullptr;
    SearchResultsModel *resultsModel = nullptr;
    SearchResultsView *resultsView = nullptr;
    QStatusBar *statusBar = nullptr;
    RipgrepCommand *rg = nullptr;
};

RipgrepSearchView::RipgrepSearchView(RipgrepSearchPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(plugin)
    , d(new RipgrepSearchViewPrivate)
{
    d->moveToThread(thread());
    d->q = this;
    d->plugin = plugin;
    d->mainWindow = mainWindow;
    d->rg = new RipgrepCommand(this);

    d->setupActions();
    d->setupUi();
    d->setupRipgrepProcess();
}

RipgrepSearchView::~RipgrepSearchView()
{
    d->mainWindow->guiFactory()->removeClient(this);
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

void RipgrepSearchViewPrivate::resetStatusMessage()
{
    if (statusBar) {
        statusBar->showMessage(tr("Ready to search."));
    }
}

QAction *RipgrepSearchViewPrivate::addAction(const QString &name, const QString &iconName, const QString &text)
{
    auto action = q->actionCollection()->addAction(name);
    action->setIcon(QIcon::fromTheme(iconName));
    action->setText(text);
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    return action;
}

QAction *RipgrepSearchViewPrivate::addCheckableAction(const QString &name, const QString &iconName, const QString &text)
{
    auto action = addAction(name, iconName, text);
    action->setCheckable(true);
    return action;
}

void RipgrepSearchViewPrivate::setupActions()
{
    q->KXMLGUIClient::setComponentName("kate_ripgrep_search", tr("RIPGrep Search"));
    q->setXMLFile("actions.rc");

    searchSelectionAction = addAction("ripgrep_search_in_files", "edit-find", tr("Find in Files using RIPGrep"));
    KActionCollection::setDefaultShortcut(searchSelectionAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::Key_F));
    connect(searchSelectionAction, &QAction::triggered, this, &RipgrepSearchViewPrivate::searchSelection);

    refreshAction = addAction("ripgrep_refresh", "view-refresh", tr("Refresh"));
    connect(refreshAction, &QAction::triggered, this, &RipgrepSearchViewPrivate::startSearch);

    clearAction = addAction("ripgrep_clear", "edit-clear-all", tr("Clear results"));
    connect(clearAction, &QAction::triggered, this, &RipgrepSearchViewPrivate::clearResults);

    wholeWordAction = addCheckableAction("ripgrep_whole_word", "ime-punctuation-fullwidth", tr("Match whole words"));
    connect(wholeWordAction, &QAction::triggered, rg, &RipgrepCommand::setWholeWord);

    caseSensitiveAction = addCheckableAction("ripgrep_case_sensitive", "format-text-superscript", tr("Case sensitive"));
    connect(caseSensitiveAction, &QAction::triggered, rg, &RipgrepCommand::setCaseSensitive);

    useRegexAction = addCheckableAction("ripgrep_use_regex", "code-context", tr("Use regular expression"));
    connect(useRegexAction, &QAction::triggered, rg, &RipgrepCommand::setUseRegex);

    showAdvancedAction = addCheckableAction("ripgrep_show_advanced", "overflow-menu", tr("Show advanced options"));

    mainWindow->guiFactory()->addClient(q);
}

QComboBox *RipgrepSearchViewPrivate::createEditableComboBox(const QString &placeholderText)
{
    auto comboBox = new QComboBox();
    comboBox->setEditable(true);
    comboBox->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    comboBox->lineEdit()->setPlaceholderText(placeholderText);
    connect(comboBox->lineEdit(), &QLineEdit::returnPressed, this, &RipgrepSearchViewPrivate::startSearch);
    return comboBox;
}

void RipgrepSearchViewPrivate::setupUi()
{
    // clang-format off
    toolView = mainWindow->createToolView(plugin, "RipgrepSearchPlugin",
                                              KTextEditor::MainWindow::Left,
                                              QIcon::fromTheme("search"), tr("Ripgrep Search"));
    // clang-format on

    auto headerBar = createToolBar(toolView);
    auto searchLabel = new QLabel(tr("<b>Search</b>"));
    searchLabel->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
    headerBar->addWidget(searchLabel);
    headerBar->addAction(refreshAction);
    headerBar->addAction(clearAction);
    headerBar->addAction(showAdvancedAction);

    auto searchBar = createToolBar(toolView);
    searchBox = createEditableComboBox(tr("Search (⇵ for history)"));
    searchBar->addWidget(searchBox);
    searchBar->addAction(wholeWordAction);
    searchBar->addAction(caseSensitiveAction);
    searchBar->addAction(useRegexAction);

    auto includeBar = createToolBar(toolView);
    includeBar->setVisible(false);
    connect(showAdvancedAction, &QAction::triggered, includeBar, &QToolBar::setVisible);
    auto filterContainer = new QWidget();
    includeBar->addWidget(filterContainer);
    auto filterForm = new QFormLayout(filterContainer);
    filterForm->setContentsMargins(0, 0, 0, 0);
    includeFileBox = createEditableComboBox(tr("Files to include, separated by commas"));
    excludeFileBox = createEditableComboBox(tr("Files to exclude, separated by commas"));
    filterForm->addRow(tr("Include:"), includeFileBox);
    filterForm->addRow(tr("Exclude:"), excludeFileBox);

    resultsModel = new SearchResultsModel(this);
    resultsView = new SearchResultsView(resultsModel, toolView);
    resultsView->setHeaderHidden(true);
    resultsView->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    connect(resultsView, &SearchResultsView::jumpToFile, [this](const QString &file) {
        mainWindow->openUrl(QUrl::fromLocalFile(file));
    });
    connect(resultsView, &SearchResultsView::jumpToResult, [this](const QString &file, int line, int start, int end) {
        if (auto view = mainWindow->openUrl(QUrl::fromLocalFile(file))) {
            line--;
            view->setCursorPosition(KTextEditor::Cursor(line, start));
            view->setSelection(KTextEditor::Range(line, start, line, end));
        }
    });

    statusBar = new QStatusBar(toolView);
    resetStatusMessage();
}

void RipgrepSearchViewPrivate::setupRipgrepProcess()
{
    connect(rg, &RipgrepCommand::searchOptionsChanged, this, &RipgrepSearchViewPrivate::startSearch);
    connect(rg, &RipgrepCommand::matchFoundInFile, resultsModel, &SearchResultsModel::addMatchedFile);
    connect(rg, &RipgrepCommand::matchFound, resultsModel, &SearchResultsModel::addMatched);
    connect(rg, &RipgrepCommand::searchFinished, [this](int found, qint64 nanos) {
        auto seconds = QString::number(nanos / 1000000000.0, 'f', 6);
        auto results = found == 1 ? tr("result") : tr("results");
        statusBar->showMessage(tr("Found %1 %2 in %3 seconds.").arg(found).arg(results).arg(seconds));
    });
}

QString RipgrepSearchViewPrivate::projectBaseDir()
{
    if (auto projectPlugin = mainWindow->pluginView("kateprojectplugin")) {
        return projectPlugin->property("projectBaseDir").toString();
    }
    return QString();
}

QStringList RipgrepSearchViewPrivate::openedFiles()
{
    QStringList result;
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

inline static QStringList commaSeparated(const QString &line)
{
    auto result = line.split(",", Qt::SkipEmptyParts);
    for (auto &&name : result)
        name = name.trimmed();
    return result;
}

void RipgrepSearchViewPrivate::startSearch()
{
    auto term = searchBox->currentText();
    if (term.isEmpty())
        return;

    rg->setIncludeFiles(commaSeparated(includeFileBox->currentText()));
    rg->setExcludeFiles(commaSeparated(excludeFileBox->currentText()));

    QCoreApplication::removePostedEvents(resultsModel, QEvent::MetaCall);

    statusBar->showMessage(tr("Searching..."));
    resultsModel->clear();
    if (auto baseDir = projectBaseDir(); !baseDir.isEmpty()) {
        rg->searchInDir(term, baseDir);
    } else if (auto files = openedFiles(); !files.isEmpty()) {
        rg->searchInFiles(term, files);
    } else {
        qInfo() << "No opened documents, not performing searching.";
    }
}

void RipgrepSearchViewPrivate::searchSelection()
{
    if (!toolView->isVisible())
        mainWindow->showToolView(toolView);

    if (auto view = mainWindow->activeView(); view && view->selection()) {
        auto selectionText = view->selectionText().trimmed();
        searchBox->addItem(selectionText);
        searchBox->setCurrentIndex(searchBox->count() - 1);
        startSearch();
    }
}

void RipgrepSearchViewPrivate::clearResults()
{
    searchBox->clear();
    includeFileBox->clear();
    excludeFileBox->clear();
    resultsModel->clear();
    resetStatusMessage();
}

#include "RipgrepSearchView.moc"
