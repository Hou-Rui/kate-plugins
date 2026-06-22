#pragma once
#include <QStandardItemModel>
#include <QVector>

class SearchResultsModelPrivate;

struct ReplacementTarget {
    QString file;
    qint64 byteStart;
    qint64 byteEnd;
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
        // Absolute UTF-8 byte offsets of the match in the file, used to derive
        // the Kate cursor at navigation time (see RipgrepCommand::matchFound).
        ByteStartRole,
        ByteEndRole,
    };

    explicit SearchResultsModel(QObject *parent = nullptr);
    ~SearchResultsModel();
    void clear();

    QVector<ReplacementTarget> checkedResults() const;

public slots:
    void addMatchedFile(const QString &file);
    void addMatched(const QString &file, const QString &text, int line, int start, int end, qint64 byteStart, qint64 byteEnd);

    void selectAll();
    void deselectAll();
    void invertSelection();

private:
    const QScopedPointer<SearchResultsModelPrivate> d;
};
