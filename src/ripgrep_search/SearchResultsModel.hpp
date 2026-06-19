#pragma once
#include <QStandardItemModel>
#include <QVector>

class SearchResultsModelPrivate;

struct ReplacementTarget {
    QString file;
    int line;
    int start;
    int end;
};

class SearchResultsModel : public QStandardItemModel
{
    Q_OBJECT
public:
    enum ItemDataRole {
        FileNameRole = Qt::UserRole,
        LineNumberRole,
        StartColumnRole,
        EndColumnRole,
    };

    explicit SearchResultsModel(QObject *parent = nullptr);
    ~SearchResultsModel();
    void clear();

    QVector<ReplacementTarget> checkedResults() const;

public slots:
    void addMatchedFile(const QString &file);
    void addMatched(const QString &file, const QString &text, int line, int start, int end);

    void selectAll();
    void deselectAll();
    void invertSelection();

private:
    const QScopedPointer<SearchResultsModelPrivate> d;
};
