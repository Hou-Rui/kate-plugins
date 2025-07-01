#include "KateBookmarksPlugin.hpp"

#include <KPluginFactory>
#include <KTextEditor/Application>
#include <KTextEditor/Cursor>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>
#include <QIcon>
#include <QListWidget>
#include <QVBoxLayout>
#include <QVariantMap>


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
    m_mainWindow->showMessage({
        { QStringLiteral("category"), QStringLiteral("Bookmarks") },
        { QStringLiteral("categoryIcon"), QIcon::fromTheme(QStringLiteral("system-run")) },
        { QStringLiteral("type"), QStringLiteral("Log") },
        { QStringLiteral("text"), msg }
    });
}

KateBookmarksView::KateBookmarksView(KateBookmarksPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(plugin), m_plugin(plugin), m_mainWindow(mainWindow)
{
    showMessage("view init!");
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
    m_listWidget = new QListWidget(m_toolView);
    m_toolView->layout()->addWidget(m_listWidget);
}

void KateBookmarksView::connectSignals()
{
    auto editor = KTextEditor::Editor::instance();
    connect(editor, &KTextEditor::Editor::documentCreated, this, [this](auto, auto document) {
        showMessage("Document created!");
        connect(document, &KTextEditor::Document::markChanged, this, [this](auto, auto, auto) {
            showMessage("Mark created!");
            refreshAllBookmarks();
        });
        refreshAllBookmarks();
    });
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &KateBookmarksView::jumpToBookmark);
}

void KateBookmarksView::refreshAllBookmarks()
{
    m_listWidget->clear();
    auto documents = KTextEditor::Editor::instance()->documents();
    for (auto document : documents) {
        refreshBookmarks(document);
    }
}

void KateBookmarksView::refreshBookmarks(KTextEditor::Document *document)
{
    auto marks = document->marks();
    for (auto mark : marks) {
        if (!(mark->type & KTextEditor::Document::Bookmark)) {
            continue;
        }
        auto lineContent = document->line(mark->line).trimmed();
        auto itemContent = QString("%1: %2").arg(mark->line + 1).arg(lineContent);
        auto *item = new QListWidgetItem(itemContent);
        item->setData(DocumentRole, QVariant::fromValue(document));
        item->setData(MarkRole, QVariant::fromValue(mark));
        m_listWidget->addItem(item);
    }
}

void KateBookmarksView::jumpToBookmark(QListWidgetItem *item)
{
    showMessage("Jump to Bookmark!");
    auto document = item->data(DocumentRole).value<KTextEditor::Document *>();
    auto mark = item->data(MarkRole).value<KTextEditor::Mark *>();

    if (auto view = m_mainWindow->openUrl(document->url())) {
        view->setCursorPosition(KTextEditor::Cursor(mark->line, 0));
    }
}

#include "KateBookmarksPlugin.moc"