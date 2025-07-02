#include "KateBookmarksPlugin.hpp"

#include <KPluginFactory>
#include <KTextEditor/Application>
#include <KTextEditor/Cursor>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>
#include <KXMLGUIFactory>

#include <QAction>
#include <QHeaderView>
#include <QIcon>
#include <QVBoxLayout>
#include <QVariantMap>
#include <ktexteditor/application.h>
#include <ktexteditor/document.h>

K_PLUGIN_CLASS_WITH_JSON(KateBookmarksPlugin, "katebookmarksplugin.json")

KateBookmarksPlugin::KateBookmarksPlugin(QObject *parent, const QList<QVariant> &)
    : KTextEditor::Plugin(parent)
{
}

QObject *KateBookmarksPlugin::createView(KTextEditor::MainWindow *mainWindow)
{
    return new KateBookmarksView(this, mainWindow);
}

void KateBookmarksView::showMessage(const QString &msg)
{
    QVariantMap map{{QStringLiteral("category"), QStringLiteral("Bookmarks")},
                    {QStringLiteral("categoryIcon"), QIcon::fromTheme(QStringLiteral("bookmarks"))},
                    {QStringLiteral("type"), QStringLiteral("Log")},
                    {QStringLiteral("text"), msg}};
    QMetaObject::invokeMethod(m_mainWindow->parent(), "showMessage", Qt::DirectConnection, Q_ARG(QVariantMap, map));
}

KateBookmarksView::KateBookmarksView(KateBookmarksPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(plugin)
    , m_plugin(plugin)
    , m_mainWindow(mainWindow)
{
    setupUi();
    connectSignals();
}

void KateBookmarksView::setupUi()
{
    m_toolView = m_mainWindow->createToolView(m_plugin,
                                              "katebookmarksplugin",
                                              KTextEditor::MainWindow::Left,
                                              QIcon::fromTheme(QStringLiteral("bookmarks")),
                                              tr("Bookmarks"));
    m_toolView->setLayout(new QVBoxLayout());
    m_treeWidget = new QTreeWidget(m_toolView);
    m_treeWidget->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    m_treeWidget->setHeaderLabel(tr("Bookmarks"));
    m_treeWidget->header()->setDefaultAlignment(Qt::AlignCenter);
    m_toolView->layout()->addWidget(m_treeWidget);
}

void KateBookmarksView::connectSignals()
{
    auto app = KTextEditor::Editor::instance()->application();
    connect(app, &KTextEditor::Application::documentCreated, [this](KTextEditor::Document *document) {
        connect(document, &KTextEditor::Document::markChanged, [this](auto, auto, auto) {
            refreshAllBookmarks();
        });
        refreshAllBookmarks();
    });

    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem *item, auto) {
        auto document = item->data(0, DocumentRole).value<KTextEditor::Document *>();
        auto mark = item->data(0, MarkRole).value<KTextEditor::Mark *>();
        if (document && mark) {
            jumpToBookmark(document, mark);
        }
    });
}

KateBookmarksView::~KateBookmarksView()
{
}

void KateBookmarksView::refreshAllBookmarks()
{
    m_treeWidget->clear();
    auto documents = KTextEditor::Editor::instance()->documents();
    for (auto document : documents) {
        refreshBookmarks(document);
    }
}

void KateBookmarksView::refreshBookmarks(KTextEditor::Document *document)
{
    if (!document || !document->url().isValid()) {
        return;
    }
    auto fileItem = new QTreeWidgetItem({document->url().fileName()});
    fileItem->setIcon(0, QIcon::fromTheme(QStringLiteral("document-multiple")));
    auto marks = document->marks();
    for (auto mark : marks) {
        if (!(mark->type & KTextEditor::Document::Bookmark)) {
            continue;
        }
        auto lineContent = document->line(mark->line).trimmed();
        auto itemContent = QString("%1: %2").arg(mark->line + 1).arg(lineContent);
        auto item = new QTreeWidgetItem({itemContent});
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("bookmarks")));
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

void KateBookmarksView::jumpToBookmark(KTextEditor::Document *document, KTextEditor::Mark *mark)
{
    if (!document || !mark) {
        return;
    }
    if (auto view = m_mainWindow->openUrl(document->url())) {
        view->setCursorPosition(KTextEditor::Cursor(mark->line, 0));
    }
}

#include "KateBookmarksPlugin.moc"
