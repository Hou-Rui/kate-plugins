#pragma once
#include <QStandardItemModel>

class SearchResultsModelPrivate;

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

public slots:
    void addMatchedFile(const QString &file);
    void addMatched(const QString &file, const QString &text, int line, int start, int end);

private:
    const QScopedPointer<SearchResultsModelPrivate> d;
};