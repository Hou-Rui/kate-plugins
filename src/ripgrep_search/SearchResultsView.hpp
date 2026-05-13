#include <QStandardItemModel>
#include <QTreeView>

class SearchResultsModelPrivate;

class SearchResultsModel : public QStandardItemModel
{
    Q_OBJECT
public:
    explicit SearchResultsModel(QObject *parent = nullptr);
    ~SearchResultsModel();
    void clear();

public slots:
    void addMatchedFile(const QString &file);
    void addMatched(const QString &file, const QString &text, int line, int start, int end);

private:
    const QScopedPointer<SearchResultsModelPrivate> d;
};

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
