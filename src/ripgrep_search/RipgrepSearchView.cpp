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
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QSharedPointer>
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
#include <QtConcurrentRun>

#include <algorithm>
#include <functional>

using LineStarts = QList<qint64>;
using LineStartsPtr = QSharedPointer<const LineStarts>;

struct LineStartBuildResult {
    QString file;
    qint64 size = -1;
    QDateTime lastModified;
    LineStarts starts;
    bool valid = false;
};

struct LineStartCacheEntry {
    qint64 size = -1;
    QDateTime lastModified;
    LineStartsPtr starts;
};

struct PendingReplacement {
    QString replacement;
    QMap<QString, QVector<ReplacementTarget>> targetsByFile;
    QHash<QString, LineStartsPtr> lineStartsByFile;
    int pendingIndexes = 0;
    int failedIndexes = 0;
};

using PendingReplacementPtr = QSharedPointer<PendingReplacement>;

// Build Kate-compatible line starts without holding a second full copy of the
// file in memory. This runs in a worker thread; the one-byte state preserves
// CRLF handling when a pair straddles two chunks.
static LineStartBuildResult buildLineStarts(const QString &file)
{
    LineStartBuildResult result;
    result.file = file;
    result.starts.append(0);

    const QFileInfo before(file);
    result.size = before.size();
    result.lastModified = before.lastModified();

    QFile f(file);
    if (!before.exists() || !f.open(QIODevice::ReadOnly))
        return result;

    constexpr qint64 chunkSize = 1024 * 1024;
    qint64 offset = 0;
    bool previousWasCr = false;
    while (!f.atEnd()) {
        const QByteArray bytes = f.read(chunkSize);
        if (bytes.isEmpty() && f.error() != QFileDevice::NoError)
            return result;

        for (qsizetype i = 0; i < bytes.size(); ++i) {
            const char c = bytes.at(i);
            const qint64 position = offset + i;
            if (previousWasCr) {
                if (c == '\n') {
                    result.starts.append(position + 1);
                    previousWasCr = false;
                    continue;
                }
                result.starts.append(position);
                previousWasCr = false;
            }

            if (c == '\r')
                previousWasCr = true;
            else if (c == '\n')
                result.starts.append(position + 1);
        }
        offset += bytes.size();
    }
    if (previousWasCr)
        result.starts.append(offset);

    const QFileInfo after(file);
    result.valid = after.exists() && after.size() == result.size && after.lastModified() == result.lastModified;
    return result;
}

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
    void invalidateLineStartCache();
    void requestLineStarts(const QString &file, std::function<void(LineStartsPtr)> callback);
    void finishReplaceAll(const PendingReplacementPtr &replacement);

    QString projectBaseDir();
    QStringList openedFiles();
    KTextEditor::Range mapToKate(const LineStarts &starts, qint64 byteStart, qint64 byteEnd, KTextEditor::Document *doc);
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
    // Per-file cache of the byte offsets at which each Kate line begins. Cache
    // misses are built asynchronously and shared by concurrent consumers.
    QHash<QString, LineStartCacheEntry> lineStartCache;
    QHash<QString, QFutureWatcher<LineStartBuildResult> *> lineStartJobs;
    QHash<QString, QList<std::function<void(LineStartsPtr)>>> lineStartWaiters;
    quint64 searchGeneration = 0;
    quint64 navigationSerial = 0;
    PendingReplacementPtr pendingReplacement;
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
        ++navigationSerial;
        mainWindow->openUrl(QUrl::fromLocalFile(file));
    });
    connect(resultsView, &SearchResultsView::jumpToResult, [this](const QString &file, qint64 byteStart, qint64 byteEnd) {
        const quint64 serial = ++navigationSerial;
        if (auto view = mainWindow->openUrl(QUrl::fromLocalFile(file))) {
            const QPointer<KTextEditor::View> guardedView(view);
            const quint64 generation = searchGeneration;
            requestLineStarts(file, [this, guardedView, byteStart, byteEnd, generation, serial](const LineStartsPtr &starts) {
                if (!guardedView || generation != searchGeneration || serial != navigationSerial)
                    return;
                if (!starts) {
                    statusBar->showMessage(tr("Could not locate the result because the file changed while it was being indexed."));
                    return;
                }
                auto range = mapToKate(*starts, byteStart, byteEnd, guardedView->document());
                guardedView->setCursorPosition(range.start());
                guardedView->setSelection(range);
            });
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

void RipgrepSearchViewPrivate::invalidateLineStartCache()
{
    ++searchGeneration;
    ++navigationSerial;
    lineStartCache.clear();
    pendingReplacement.clear();
    updateReplaceState();
}

void RipgrepSearchViewPrivate::requestLineStarts(const QString &file, std::function<void(LineStartsPtr)> callback)
{
    const QFileInfo current(file);
    if (auto it = lineStartCache.find(file); it != lineStartCache.end()) {
        if (current.exists() && current.size() == it->size && current.lastModified() == it->lastModified) {
            callback(it->starts);
            return;
        }
        lineStartCache.erase(it);
    }

    lineStartWaiters[file].append(std::move(callback));
    if (lineStartJobs.contains(file))
        return;

    auto watcher = new QFutureWatcher<LineStartBuildResult>(this);
    lineStartJobs.insert(file, watcher);
    connect(watcher, &QFutureWatcher<LineStartBuildResult>::finished, this, [this, watcher, file] {
        const auto result = watcher->result();
        lineStartJobs.remove(file);
        const auto waiters = lineStartWaiters.take(file);
        watcher->deleteLater();

        LineStartsPtr starts;
        const QFileInfo current(result.file);
        if (result.valid && current.exists() && current.size() == result.size && current.lastModified() == result.lastModified) {
            starts.reset(new LineStarts(result.starts));
            lineStartCache.insert(file, {result.size, result.lastModified, starts});
        }

        for (const auto &waiter : waiters)
            waiter(starts);
    });
    watcher->setFuture(QtConcurrent::run(buildLineStarts, file));
}

// Map a match's absolute byte range to the Kate range to select. The match is
// kept on a single line: should it straddle a lone '\r' (one ripgrep line, but
// several Kate lines) the selection is clamped to the end of its start line.
KTextEditor::Range RipgrepSearchViewPrivate::mapToKate(const LineStarts &starts, qint64 byteStart, qint64 byteEnd, KTextEditor::Document *doc)
{
    auto lineOf = [&starts](qint64 offset) {
        auto it = std::upper_bound(starts.cbegin(), starts.cend(), offset);
        return std::max<int>(0, int(it - starts.cbegin()) - 1);
    };
    auto columnOf = [doc](int line, qint64 lineStart, qint64 offset) -> int {
        if (line < 0 || line >= doc->lines())
            return 0;
        const QByteArray lineUtf8 = doc->line(line).toUtf8();
        const qint64 byteInLine = qBound<qint64>(0, offset - lineStart, lineUtf8.size());
        return int(QString::fromUtf8(lineUtf8.constData(), int(byteInLine)).size());
    };

    const int startLine = lineOf(byteStart);
    const int startColumn = columnOf(startLine, starts.at(startLine), byteStart);
    const int endColumn = lineOf(byteEnd) == startLine ? columnOf(startLine, starts.at(startLine), byteEnd) : doc->line(startLine).size();
    return KTextEditor::Range(startLine, startColumn, startLine, endColumn);
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
        replaceAllButton->setEnabled(!pendingReplacement && resultsModel && resultsModel->invisibleRootItem()->rowCount() > 0);
}

void RipgrepSearchViewPrivate::replaceAll()
{
    auto targets = resultsModel->checkedResults();
    if (targets.isEmpty()) {
        statusBar->showMessage(tr("No results selected to replace."));
        return;
    }

    auto replacement = PendingReplacementPtr::create();
    replacement->replacement = replaceBox->currentText();
    // Group the selected matches per file so each document is touched once and
    // each line index is prepared only once.
    for (const auto &target : targets)
        replacement->targetsByFile[target.file].append(target);

    replacement->pendingIndexes = replacement->targetsByFile.size();
    pendingReplacement = replacement;
    updateReplaceState();
    statusBar->showMessage(tr("Preparing replacements..."));

    for (auto it = replacement->targetsByFile.cbegin(); it != replacement->targetsByFile.cend(); ++it) {
        const QString file = it.key();
        requestLineStarts(file, [this, replacement, file](const LineStartsPtr &starts) {
            if (pendingReplacement != replacement)
                return;
            if (starts)
                replacement->lineStartsByFile.insert(file, starts);
            else
                ++replacement->failedIndexes;
            if (--replacement->pendingIndexes == 0)
                finishReplaceAll(replacement);
        });
    }
}

void RipgrepSearchViewPrivate::finishReplaceAll(const PendingReplacementPtr &replacement)
{
    if (pendingReplacement != replacement)
        return;
    int replaced = 0;
    for (auto it = replacement->targetsByFile.cbegin(); it != replacement->targetsByFile.cend(); ++it) {
        const auto starts = replacement->lineStartsByFile.value(it.key());
        if (!starts)
            continue;

        bool wasOpen = false;
        auto doc = documentForFile(it.key(), &wasOpen);
        if (!doc)
            continue;

        // Resolve every match to a Kate range while the document is still
        // pristine (mapToKate reads the unedited line text for its columns).
        QVector<KTextEditor::Range> ranges;
        ranges.reserve(it.value().size());
        for (const auto &match : it.value())
            ranges.append(mapToKate(*starts, match.byteStart, match.byteEnd, doc));

        // Apply matches bottom-up so earlier edits never shift later positions.
        std::sort(ranges.begin(), ranges.end(), [](const auto &a, const auto &b) {
            return a.start() > b.start();
        });

        for (const auto &range : ranges) {
            doc->replaceText(range, replacement->replacement);
            replaced++;
        }
        doc->save();
        if (!wasOpen)
            doc->deleteLater();
    }

    pendingReplacement.clear();
    if (replacement->failedIndexes > 0)
        statusBar->showMessage(tr("Replaced %1 occurrences; skipped %2 files that changed while being indexed.").arg(replaced).arg(replacement->failedIndexes));
    else
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
    // Results are about to be rebuilt against the current on-disk contents, so
    // any cached line-start maps (a file may have changed) are now stale.
    invalidateLineStartCache();

    statusBar->showMessage(tr("Searching..."));
    resultsModel->clear();
    if (auto baseDir = projectBaseDir(); !baseDir.isEmpty()) {
        rg->searchInDir(term, baseDir);
    } else if (auto files = openedFiles(); !files.isEmpty()) {
        rg->searchInFiles(term, files);
    } else {
        statusBar->showMessage(tr("No project or local files to search."));
        qInfo() << "No project or local files to search.";
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
    invalidateLineStartCache();
    resultsModel->clear();
    resetStatusMessage();
}

#include "RipgrepSearchView.moc"
