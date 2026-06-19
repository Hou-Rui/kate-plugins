#include "RipgrepSearchView.hpp"
#include "RipgrepCommand.hpp"
#include "RipgrepSearchPlugin.hpp"
#include "SearchResultsModel.hpp"
#include "SearchResultsView.hpp"

#include <KActionCollection>
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
#include <QFileSystemWatcher>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QProcess>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextStream>
#include <QTimer>
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
    void replaceAll();
    void updateReplaceState();
    void scheduleResearch();
    void watchResultFile(const QString &file);

public:
    void clearWatches();

    QString projectBaseDir();
    QStringList openedFiles();
    KTextEditor::Document *documentForFile(const QString &file, bool *wasOpen);
    QAction *addAction(const QString &name, const QString &iconName, const QString &text);
    QAction *addCheckableAction(const QString &name, const QString &iconName, const QString &text);
    QComboBox *createEditableComboBox(const QString &placeholderText);
    QWidget *createPlaceholder();
    bool ripgrepAvailable();

    RipgrepSearchView *q;
    RipgrepSearchPlugin *plugin = nullptr;
    bool rgAvailable = true;
    KTextEditor::MainWindow *mainWindow = nullptr;
    QWidget *toolView = nullptr;
    QComboBox *searchBox = nullptr;
    QAction *searchSelectionAction = nullptr;
    QAction *refreshAction = nullptr;
    QAction *clearAction = nullptr;
    QAction *wholeWordAction = nullptr;
    QAction *caseSensitiveAction = nullptr;
    QAction *useRegexAction = nullptr;
    QAction *showReplaceAction = nullptr;
    QAction *showAdvancedAction = nullptr;
    QComboBox *replaceBox = nullptr;
    QPushButton *replaceAllButton = nullptr;
    QComboBox *includeFileBox = nullptr;
    QComboBox *excludeFileBox = nullptr;
    SearchResultsModel *resultsModel = nullptr;
    SearchResultsView *resultsView = nullptr;
    QStatusBar *statusBar = nullptr;
    RipgrepCommand *rg = nullptr;
    QFileSystemWatcher *fileWatcher = nullptr;
    QTimer *researchTimer = nullptr;
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

    showReplaceAction = addCheckableAction("ripgrep_show_replace", "edit-find-replace", tr("Show replace options"));

    showAdvancedAction = addCheckableAction("ripgrep_show_advanced", "overflow-menu", tr("Show advanced options"));

    // Without rg there is nothing the actions (and their shortcuts) can do, so
    // disable them all; the tool view shows a placeholder explaining why.
    rgAvailable = ripgrepAvailable();
    if (!rgAvailable) {
        for (auto action : q->actionCollection()->actions())
            action->setEnabled(false);
    }

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

    // The search UI and the "rg not found" notice live on two pages of a stack;
    // only one is ever shown depending on whether ripgrep is on PATH.
    auto contentStack = new QStackedWidget(toolView);
    auto searchPage = new QWidget();
    auto pageLayout = new QVBoxLayout(searchPage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto headerBar = createToolBar(searchPage);
    auto searchLabel = new QLabel(tr("<b>Search</b>"));
    searchLabel->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
    headerBar->addWidget(searchLabel);
    headerBar->addAction(refreshAction);
    headerBar->addAction(clearAction);
    headerBar->addAction(showReplaceAction);
    headerBar->addAction(showAdvancedAction);
    pageLayout->addWidget(headerBar);

    auto searchBar = createToolBar(searchPage);
    searchBox = createEditableComboBox(tr("Search (⇵ for history)"));
    searchBar->addWidget(searchBox);
    searchBar->addAction(wholeWordAction);
    searchBar->addAction(caseSensitiveAction);
    searchBar->addAction(useRegexAction);
    pageLayout->addWidget(searchBar);

    auto replaceBar = createToolBar(searchPage);
    replaceBar->setVisible(false);
    pageLayout->addWidget(replaceBar);
    connect(showReplaceAction, &QAction::triggered, replaceBar, &QToolBar::setVisible);
    replaceBox = createEditableComboBox(tr("Replace with"));
    replaceBar->addWidget(replaceBox);
    replaceAllButton = new QPushButton(QIcon::fromTheme("edit-find-replace"), tr("Replace All"));
    replaceAllButton->setEnabled(false);
    connect(replaceAllButton, &QPushButton::clicked, this, &RipgrepSearchViewPrivate::replaceAll);
    replaceBar->addWidget(replaceAllButton);

    auto includeBar = createToolBar(searchPage);
    includeBar->setVisible(false);
    pageLayout->addWidget(includeBar);
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
    connect(resultsModel, &QAbstractItemModel::rowsInserted, this, &RipgrepSearchViewPrivate::updateReplaceState);
    connect(resultsModel, &QAbstractItemModel::rowsRemoved, this, &RipgrepSearchViewPrivate::updateReplaceState);
    connect(resultsModel, &QAbstractItemModel::modelReset, this, &RipgrepSearchViewPrivate::updateReplaceState);
    resultsView = new SearchResultsView(resultsModel, searchPage);
    pageLayout->addWidget(resultsView);
    resultsView->setHeaderHidden(true);
    resultsView->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    resultsView->setShowCheckboxes(showReplaceAction->isChecked());
    connect(showReplaceAction, &QAction::toggled, resultsView, &SearchResultsView::setShowCheckboxes);
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

    statusBar = new QStatusBar(searchPage);
    pageLayout->addWidget(statusBar);
    resetStatusMessage();

    contentStack->addWidget(searchPage);
    contentStack->addWidget(createPlaceholder());
    contentStack->setCurrentIndex(rgAvailable ? 0 : 1);
}

bool RipgrepSearchViewPrivate::ripgrepAvailable()
{
    return !QStandardPaths::findExecutable(QStringLiteral("rg")).isEmpty();
}

QWidget *RipgrepSearchViewPrivate::createPlaceholder()
{
    auto placeholder = new QWidget();
    auto layout = new QVBoxLayout(placeholder);
    layout->setAlignment(Qt::AlignCenter);

    auto iconLabel = new QLabel();
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(QIcon::fromTheme("dialog-warning").pixmap(64, 64));

    // clang-format off
    auto textLabel = new QLabel(tr("<b>ripgrep is not available</b><br/><br/>"
                                   "The <tt>rg</tt> command could not be found on your PATH.<br/>"
                                   "Please install ripgrep to use this plugin."));
    // clang-format on
    textLabel->setAlignment(Qt::AlignCenter);
    textLabel->setWordWrap(true);
    textLabel->setTextFormat(Qt::RichText);

    layout->addWidget(iconLabel);
    layout->addWidget(textLabel);
    return placeholder;
}

void RipgrepSearchViewPrivate::setupRipgrepProcess()
{
    connect(rg, &RipgrepCommand::searchOptionsChanged, this, &RipgrepSearchViewPrivate::startSearch);
    connect(rg, &RipgrepCommand::matchFoundInFile, resultsModel, &SearchResultsModel::addMatchedFile);
    connect(rg, &RipgrepCommand::matchFoundInFile, this, &RipgrepSearchViewPrivate::watchResultFile);
    connect(rg, &RipgrepCommand::matchFound, resultsModel, &SearchResultsModel::addMatched);
    connect(rg, &RipgrepCommand::searchFinished, [this](int found, qint64 nanos) {
        auto seconds = QString::number(nanos / 1000000000.0, 'f', 6);
        auto results = found == 1 ? tr("result") : tr("results");
        statusBar->showMessage(tr("Found %1 %2 in %3 seconds.").arg(found).arg(results).arg(seconds));
    });

    // ripgrep only ever sees what is on disk, so the results drift out of sync
    // the moment a matched file changes — whether Kate saves an edited document
    // or some external tool rewrites it. Watch every file that produced a result
    // and re-run the search when it changes on disk.
    fileWatcher = new QFileSystemWatcher(this);
    connect(fileWatcher, &QFileSystemWatcher::fileChanged, this, &RipgrepSearchViewPrivate::scheduleResearch);

    // Coalesce bursts of notifications (atomic saves delete-and-recreate the
    // file, firing several changes) into a single re-search.
    researchTimer = new QTimer(this);
    researchTimer->setSingleShot(true);
    researchTimer->setInterval(300);
    connect(researchTimer, &QTimer::timeout, this, &RipgrepSearchViewPrivate::startSearch);
}

void RipgrepSearchViewPrivate::watchResultFile(const QString &file)
{
    if (fileWatcher && !fileWatcher->files().contains(file))
        fileWatcher->addPath(file);
}

void RipgrepSearchViewPrivate::clearWatches()
{
    if (fileWatcher && !fileWatcher->files().isEmpty())
        fileWatcher->removePaths(fileWatcher->files());
}

void RipgrepSearchViewPrivate::scheduleResearch()
{
    if (rgAvailable && !searchBox->currentText().isEmpty())
        researchTimer->start();
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

KTextEditor::Document *RipgrepSearchViewPrivate::documentForFile(const QString &file, bool *wasOpen)
{
    auto editor = KTextEditor::Editor::instance();
    auto url = QUrl::fromLocalFile(file);
    for (auto doc : editor->documents()) {
        if (doc->url() == url) {
            if (wasOpen)
                *wasOpen = true;
            return doc;
        }
    }
    if (wasOpen)
        *wasOpen = false;
    auto doc = editor->createDocument(nullptr);
    if (!doc->openUrl(url)) {
        doc->deleteLater();
        return nullptr;
    }
    return doc;
}

void RipgrepSearchViewPrivate::updateReplaceState()
{
    if (replaceAllButton)
        replaceAllButton->setEnabled(resultsModel && resultsModel->invisibleRootItem()->rowCount() > 0);
}

void RipgrepSearchViewPrivate::replaceAll()
{
    auto replacement = replaceBox->currentText();
    auto targets = resultsModel->checkedResults();
    if (targets.isEmpty()) {
        statusBar->showMessage(tr("No results selected to replace."));
        return;
    }

    // Group the selected matches per file so each document is touched once.
    QMap<QString, QVector<ReplacementTarget>> byFile;
    for (const auto &target : targets)
        byFile[target.file].append(target);

    int replaced = 0;
    for (auto it = byFile.begin(); it != byFile.end(); ++it) {
        // Apply matches bottom-up so earlier edits never shift later positions.
        auto &matches = it.value();
        std::sort(matches.begin(), matches.end(), [](const auto &a, const auto &b) {
            return a.line != b.line ? a.line > b.line : a.start > b.start;
        });

        bool wasOpen = false;
        auto doc = documentForFile(it.key(), &wasOpen);
        if (!doc)
            continue;

        for (const auto &match : matches) {
            int line = match.line - 1;
            doc->replaceText(KTextEditor::Range(line, match.start, line, match.end), replacement);
            replaced++;
        }
        doc->save();
        if (!wasOpen)
            doc->deleteLater();
    }

    statusBar->showMessage(tr("Replaced %1 occurrences.").arg(replaced));
    startSearch();
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

    // A pending debounced re-search is now subsumed by this run; the watch list
    // is rebuilt as the fresh results stream back in via watchResultFile().
    if (researchTimer)
        researchTimer->stop();
    clearWatches();

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
    replaceBox->clear();
    includeFileBox->clear();
    excludeFileBox->clear();
    if (researchTimer)
        researchTimer->stop();
    clearWatches();
    resultsModel->clear();
    resetStatusMessage();
}

#include "RipgrepSearchView.moc"
