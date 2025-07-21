#include <QStandardItemModel>
#include <QTreeView>

class SearchResultsModel : public QStandardItemModel
{
    Q_OBJECT
public:
    using QStandardItemModel::QStandardItemModel;

public slots:
    void addMatchedFile(const QString &file);
    void addMatched(const QString &file, const QString &text, int line, int start, int end);

private:
    QStandardItem *m_currentItem;
};

class SearchResultsView : public QTreeView
{
    Q_OBJECT
public:
    explicit SearchResultsView(SearchResultsModel *model, QWidget *parent = nullptr);

signals:
    void jumpToResult(const QString &file, int line, int start, int end);

protected:
    void mousePressEvent(QMouseEvent *event) override;
};
