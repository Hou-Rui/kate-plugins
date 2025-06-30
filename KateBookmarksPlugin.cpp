#include "katebookmarksplugin.h"
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>
#include <QListWidget>
#include <QVBoxLayout>

KateBookmarksPlugin::KateBookmarksPlugin(QObject *parent, const QList<QVariant> &)
    : KTextEditor::Plugin((KTextEditor::Application *)parent), m_toolView(nullptr), m_listWidget(nullptr)
{
    createToolView();
    refreshBookmarks();

    // Connect signals for updates
    connect(KTextEditor::Editor::instance(), &KTextEditor::Editor::editorStarted,
            this, &KateBookmarksPlugin::refreshBookmarks);
}

void KateBookmarksPlugin::createToolView()
{
    m_toolView = mainWindow()->createToolView(this, QStringLiteral("kate_plugin_kate_bookmarks_view"), KTextEditor::MainWindow::Bottom, i18n("Project Bookmarks"));

    m_listWidget = new QListWidget;
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(m_listWidget);
    m_toolView->setWidget(new QWidget);
    m_toolView->widget()->setLayout(layout);

    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &KateBookmarksPlugin::jumpToBookmark);
}

void KateBookmarksPlugin::refreshBookmarks()
{
    m_listWidget->clear();

    auto documents = documentManager()->documents();
    for (auto document : documents) {
        if (auto view = mainWindow()->openUrl(document->url())) {
            auto bookmarks = view->document()->bookmarks();
            for (int bookmark : bookmarks) {
                auto lineContent = view->document()->line(bookmark);
                auto *item = new QListWidgetItem(QString("%1:%2 %3").arg(document->url().toLocalFile(), QString::number(bookmark + 1), lineContent));
                item->setData(Qt::UserRole, QVariant::fromValue(document));
                item->setData(Qt::UserRole + 1, bookmark);
                m_listWidget->addItem(item);
            }
        }
    }
}

void KateBookmarksPlugin::jumpToBookmark(QListWidgetItem *item)
{
    auto document = item->data(Qt::UserRole).value<KTextEditor::Document *>();
    auto bookmark = item->data(Qt::UserRole + 1).toInt();
    
    if (auto view = mainWindow()->openUrl(document->url())) {
        view->setCursorPosition(KTextEditor::Cursor(bookmark, 0));
    }
}

KateBookmarksPlugin::~KateBookmarksPlugin() {}

K_PLUGIN_CLASS_WITH_JSON(KateBookmarksPlugin, "katebookmarksplugin.json")

#include "katebookmarksplugin.moc"