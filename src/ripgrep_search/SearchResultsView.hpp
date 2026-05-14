#pragma once
#include <QTreeView>

class SearchResultsModel;

class SearchResultsView : public QTreeView
{
    Q_OBJECT
public:
    explicit SearchResultsView(SearchResultsModel *model, QWidget *parent = nullptr);

signals:
    void jumpToFile(const QString &file);
    void jumpToResult(const QString &file, int line, int start, int end);

protected:
    void mousePressEvent(QMouseEvent *event) override;
};
