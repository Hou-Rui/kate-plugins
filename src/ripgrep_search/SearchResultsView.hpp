#pragma once
#include <QTreeView>

class SearchResultsModel;
class SearchResultsViewPrivate;

class SearchResultsView : public QTreeView
{
    Q_OBJECT
public:
    explicit SearchResultsView(SearchResultsModel *model, QWidget *parent = nullptr);
    ~SearchResultsView();

    bool showCheckboxes() const;
    void setShowCheckboxes(bool show);

signals:
    void jumpToFile(const QString &file);
    // byteStart/byteEnd are absolute UTF-8 byte offsets; the receiver maps them
    // to a Kate cursor (ripgrep's line numbers cannot be trusted across lone '\r').
    void jumpToResult(const QString &file, qint64 byteStart, qint64 byteEnd);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    const QScopedPointer<SearchResultsViewPrivate> d;
};
