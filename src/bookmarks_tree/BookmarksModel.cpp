#include "BookmarksModel.hpp"

#include <KTextEditor/Document>

#include <QIcon>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

struct BookmarksModelPrivate {
    void addDocument(KTextEditor::Document *document);

    BookmarksModel *q;
};

BookmarksModel::BookmarksModel(QObject *parent)
    : QStandardItemModel(parent)
    , d(new BookmarksModelPrivate)
{
    d->q = this;
}

BookmarksModel::~BookmarksModel() = default;

void BookmarksModel::refresh(const QList<KTextEditor::Document *> &documents)
{
    clear();
    // clear() drops the header too, so re-establish it on every rebuild.
    setHorizontalHeaderLabels({tr("Bookmarks")});
    horizontalHeaderItem(0)->setTextAlignment(Qt::AlignCenter);

    auto sorted = documents;
    std::sort(sorted.begin(), sorted.end(), [](auto d1, auto d2) {
        return d1->url() < d2->url();
    });
    for (auto document : sorted) {
        d->addDocument(document);
    }
}

void BookmarksModelPrivate::addDocument(KTextEditor::Document *document)
{
    if (!document || !document->url().isValid()) {
        return;
    }

    auto marks = document->marks().values();
    std::sort(marks.begin(), marks.end(), [](auto m1, auto m2) {
        return m1->line < m2->line;
    });

    auto fileItem = new QStandardItem(QIcon::fromTheme(u"document-multiple"_s), document->url().fileName());
    fileItem->setToolTip(document->url().toDisplayString(QUrl::PreferLocalFile));
    fileItem->setEditable(false);

    for (auto mark : marks) {
        if (!(mark->type & KTextEditor::Document::Bookmark)) {
            continue;
        }
        auto lineContent = document->line(mark->line).trimmed();
        auto text = QString(u"%1: %2"_s).arg(mark->line + 1).arg(lineContent);
        auto item = new QStandardItem(QIcon::fromTheme(u"bookmarks"_s), text);
        item->setEditable(false);
        item->setData(QVariant::fromValue(document), BookmarksModel::DocumentRole);
        item->setData(mark->line, BookmarksModel::LineRole);
        fileItem->appendRow(item);
    }

    if (fileItem->rowCount() == 0) {
        delete fileItem;
        return;
    }
    q->invisibleRootItem()->appendRow(fileItem);
}
