#include "BookmarksTreeView.hpp"

#include "BookmarksModel.hpp"
#include "BookmarksTreePlugin.hpp"

#include <KTextEditor/Application>
#include <KTextEditor/Cursor>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>

#include <QAction>
#include <QHeaderView>
#include <QIcon>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVariantMap>

using namespace Qt::Literals::StringLiterals;

class BookmarksTreeViewPrivate : public QObject
{
    Q_OBJECT
public slots:
    void clearAllBookmarks();
    void refreshAllBookmarks();

public:
    void setupUi();
    void connectSignals();
    void connectDocument(KTextEditor::Document *document);
    void showMessage(const QString &msg);
    void jumpToBookmark(KTextEditor::Document *document, int line);

    BookmarksTreeView *q;
    BookmarksTreePlugin *plugin = nullptr;
    KTextEditor::MainWindow *mainWindow = nullptr;
    QWidget *toolView = nullptr;
    QTreeView *treeView = nullptr;
    BookmarksModel *model = nullptr;
};

BookmarksTreeView::BookmarksTreeView(BookmarksTreePlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(plugin)
    , d(new BookmarksTreeViewPrivate)
{
    d->q = this;
    d->plugin = plugin;
    d->mainWindow = mainWindow;
    d->setupUi();
    d->connectSignals();
}

BookmarksTreeView::~BookmarksTreeView() = default;

void BookmarksTreeViewPrivate::showMessage(const QString &msg)
{
    // clang-format off
    QVariantMap map {
        { u"category"_s, u"Bookmarks"_s },
        { u"categoryIcon"_s, QIcon::fromTheme(u"bookmarks"_s) },
        { u"type"_s, u"Log"_s },
        { u"text"_s, msg }
    };
    // clang-format on
    QMetaObject::invokeMethod(mainWindow->parent(), "showMessage", Qt::DirectConnection, Q_ARG(QVariantMap, map));
}

void BookmarksTreeViewPrivate::setupUi()
{
    // clang-format off
    toolView = mainWindow->createToolView(plugin, u"BookmarksTreePlugin"_s,
                                          KTextEditor::MainWindow::Left, QIcon::fromTheme(u"bookmarks"_s), tr("Bookmarks"));
    // clang-format on

    model = new BookmarksModel(q);

    treeView = new QTreeView(toolView);
    treeView->setModel(model);
    treeView->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    treeView->header()->setDefaultAlignment(Qt::AlignCenter);
    toolView->layout()->addWidget(treeView);
}

void BookmarksTreeViewPrivate::connectSignals()
{
    // Documents already open when this view is created never trigger
    // documentCreated, so connect them up front, then keep listening for new
    // ones.
    const auto documents = KTextEditor::Editor::instance()->documents();
    for (auto document : documents) {
        connectDocument(document);
    }
    refreshAllBookmarks();

    auto app = KTextEditor::Editor::instance()->application();
    connect(app, &KTextEditor::Application::documentCreated, this, [this](KTextEditor::Document *document) {
        connectDocument(document);
        refreshAllBookmarks();
    });

    connect(treeView, &QTreeView::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto menu = new QMenu(treeView);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        auto actionRefresh = menu->addAction(QIcon::fromTheme(u"view-refresh"_s), tr("Refresh Bookmarks"));
        auto actionClear = menu->addAction(QIcon::fromTheme(u"bookmark-remove"_s), tr("Clear all Bookmarks"));
        connect(actionRefresh, &QAction::triggered, this, &BookmarksTreeViewPrivate::refreshAllBookmarks);
        connect(actionClear, &QAction::triggered, this, &BookmarksTreeViewPrivate::clearAllBookmarks);
        menu->popup(treeView->mapToGlobal(pos));
    });

    connect(treeView, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        auto document = index.data(BookmarksModel::DocumentRole).value<KTextEditor::Document *>();
        if (document) {
            jumpToBookmark(document, index.data(BookmarksModel::LineRole).toInt());
        }
    });
}

void BookmarksTreeViewPrivate::connectDocument(KTextEditor::Document *document)
{
    connect(document, &KTextEditor::Document::markChanged, this, [this](auto, auto, auto) {
        refreshAllBookmarks();
    });
}

void BookmarksTreeViewPrivate::clearAllBookmarks()
{
    // Removing each mark emits markChanged, which drives refreshAllBookmarks().
    const auto documents = KTextEditor::Editor::instance()->documents();
    for (auto document : documents) {
        const auto marks = document->marks().values();
        for (auto mark : marks) {
            document->clearMark(mark->line);
        }
    }
}

void BookmarksTreeViewPrivate::refreshAllBookmarks()
{
    model->refresh(KTextEditor::Editor::instance()->documents());
    treeView->expandAll();
}

void BookmarksTreeViewPrivate::jumpToBookmark(KTextEditor::Document *document, int line)
{
    if (!document) {
        return;
    }
    if (auto view = mainWindow->openUrl(document->url())) {
        view->setCursorPosition(KTextEditor::Cursor(line, 0));
    }
}

#include "BookmarksTreeView.moc"
