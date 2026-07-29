#pragma once
#include <QStandardItemModel>

namespace KTextEditor
{
class Document;
}
class BookmarksModelPrivate;

class BookmarksModel : public QStandardItemModel
{
    Q_OBJECT
public:
    enum ItemDataRole {
        DocumentRole = Qt::UserRole,
        // Zero-based line of the bookmark, used to build the Kate cursor at
        // navigation time (see BookmarksTreeView jump handling).
        LineRole,
    };

    explicit BookmarksModel(QObject *parent = nullptr);
    ~BookmarksModel();

public slots:
    // Rebuild the whole two-level tree (document → bookmark lines) from the
    // given documents. Documents without bookmarks are skipped.
    void refresh(const QList<KTextEditor::Document *> &documents);

private:
    const QScopedPointer<BookmarksModelPrivate> d;
};
