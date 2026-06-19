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
    void jumpToResult(const QString &file, int line, int start, int end);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    const QScopedPointer<SearchResultsViewPrivate> d;
};
