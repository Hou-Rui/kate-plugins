#include "BookmarksTreePlugin.hpp"

#include <KPluginFactory>
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
#include <QVBoxLayout>
#include <QVariantMap>

#include <algorithm>

#define THEME_ICON(name) QIcon::fromTheme(QStringLiteral(name))

K_PLUGIN_CLASS_WITH_JSON(BookmarksTreePlugin, "bookmarks_tree.json")

BookmarksTreePlugin::BookmarksTreePlugin(QObject *parent, const QList<QVariant> &)
    : KTextEditor::Plugin(parent)
{
}

QObject *BookmarksTreePlugin::createView(KTextEditor::MainWindow *mainWindow)
{
    return new BookmarksTreeView(this, mainWindow);
}

void BookmarksTreeView::showMessage(const QString &msg)
{
    QVariantMap map{{QStringLiteral("category"), QStringLiteral("Bookmarks")},
                    {QStringLiteral("categoryIcon"), THEME_ICON("bookmarks")},
                    {QStringLiteral("type"), QStringLiteral("Log")},
                    {QStringLiteral("text"), msg}};
    QMetaObject::invokeMethod(m_mainWindow->parent(), "showMessage", Qt::DirectConnection, Q_ARG(QVariantMap, map));
}

BookmarksTreeView::BookmarksTreeView(BookmarksTreePlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(plugin)
    , m_plugin(plugin)
    , m_mainWindow(mainWindow)
{
    setupUi();
    connectSignals();
}

void BookmarksTreeView::setupUi()
{
    m_toolView = m_mainWindow->createToolView(m_plugin, "BookmarksTreePlugin", KTextEditor::MainWindow::Left, THEME_ICON("bookmarks"), tr("Bookmarks"));
    m_treeWidget = new QTreeWidget(m_toolView);
    m_treeWidget->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeWidget->setHeaderLabel(tr("Bookmarks"));
    m_treeWidget->header()->setDefaultAlignment(Qt::AlignCenter);
    m_toolView->layout()->addWidget(m_treeWidget);
}

void BookmarksTreeView::connectSignals()
{
    auto app = KTextEditor::Editor::instance()->application();
    connect(app, &KTextEditor::Application::documentCreated, [this](KTextEditor::Document *document) {
        connect(document, &KTextEditor::Document::markChanged, [this](auto, auto, auto) {
            refreshAllBookmarks();
        });
        refreshAllBookmarks();
    });

    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested, [this](const QPoint &pos) {
        auto menu = new QMenu(m_treeWidget);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        auto actionRefresh = menu->addAction(THEME_ICON("view-refresh"), tr("Refresh Bookmarks"));
        auto actionClear = menu->addAction(THEME_ICON("bookmark-remove"), tr("Clear all Bookmarks"));
        connect(actionRefresh, &QAction::triggered, this, &BookmarksTreeView::refreshAllBookmarks);
        connect(actionClear, &QAction::triggered, this, &BookmarksTreeView::clearAllBookmarks);
        menu->popup(m_treeWidget->mapToGlobal(pos));
    });

    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem *item, auto) {
        auto document = item->data(0, DocumentRole).value<KTextEditor::Document *>();
        auto mark = item->data(0, MarkRole).value<KTextEditor::Mark *>();
        if (document && mark) {
            jumpToBookmark(document, mark);
        }
    });
}

void BookmarksTreeView::clearAllBookmarks()
{
    m_treeWidget->clear();
    auto documents = KTextEditor::Editor::instance()->documents();
    for (auto document : documents) {
        clearBookmarks(document);
    }
}

void BookmarksTreeView::clearBookmarks(KTextEditor::Document *document)
{
    auto marks = document->marks().values();
    for (auto mark : marks) {
        document->clearMark(mark->line);
    }
}

void BookmarksTreeView::refreshAllBookmarks()
{
    m_treeWidget->clear();
    auto documents = KTextEditor::Editor::instance()->documents();
    std::sort(documents.begin(), documents.end(), [](auto d1, auto d2) {
        return d1->url() < d2->url();
    });
    for (auto document : documents) {
        refreshBookmarks(document);
    }
}

void BookmarksTreeView::refreshBookmarks(KTextEditor::Document *document)
{
    if (!document || !document->url().isValid()) {
        return;
    }
    auto fileItem = new QTreeWidgetItem({document->url().fileName()});
    fileItem->setIcon(0, THEME_ICON("document-multiple"));
    auto marks = document->marks().values();
    std::sort(marks.begin(), marks.end(), [](auto m1, auto m2) {
        return m1->line < m2->line;
    });
    for (auto mark : marks) {
        if (!(mark->type & KTextEditor::Document::Bookmark)) {
            continue;
        }
        auto lineContent = document->line(mark->line).trimmed();
        auto itemContent = QString("%1: %2").arg(mark->line + 1).arg(lineContent);
        auto item = new QTreeWidgetItem({itemContent});
        item->setIcon(0, THEME_ICON("bookmarks"));
        item->setData(0, DocumentRole, QVariant::fromValue(document));
        item->setData(0, MarkRole, QVariant::fromValue(mark));
        fileItem->addChild(item);
    }

    if (fileItem->childCount() == 0) {
        delete fileItem;
        return;
    }
    m_treeWidget->addTopLevelItem(fileItem);
    fileItem->setExpanded(true);
}

void BookmarksTreeView::jumpToBookmark(KTextEditor::Document *document, KTextEditor::Mark *mark)
{
    if (!document || !mark) {
        return;
    }
    if (auto view = m_mainWindow->openUrl(document->url())) {
        view->setCursorPosition(KTextEditor::Cursor(mark->line, 0));
    }
}

#include "BookmarksTreePlugin.moc"
