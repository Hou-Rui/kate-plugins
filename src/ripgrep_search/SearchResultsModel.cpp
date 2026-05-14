#include "SearchResultsModel.hpp"

#include <KFileItem>
#include <QFileInfo>
#include <QStandardItemModel>

struct SearchResultsModelPrivate {
    SearchResultsModel *q;
    QStandardItem *currentItem = nullptr;
};

SearchResultsModel::SearchResultsModel(QObject *parent)
    : QStandardItemModel(parent)
    , d(new SearchResultsModelPrivate)
{
    d->q = this;
}

SearchResultsModel::~SearchResultsModel() = default;

void SearchResultsModel::clear()
{
    d->currentItem = nullptr;
    QStandardItemModel::clear();
}

static inline QIcon iconForFile(const QString &filePath)
{
    KFileItem item(QUrl::fromLocalFile(filePath), QString(), KFileItem::Unknown);
    return QIcon::fromTheme(item.iconName());
}

void SearchResultsModel::addMatchedFile(const QString &file)
{
    auto icon = iconForFile(file);
    auto text = QFileInfo(file).fileName();
    d->currentItem = new QStandardItem(icon, text);
    d->currentItem->setData(file, Qt::ToolTipRole);
    d->currentItem->setData(file, FileNameRole);
    invisibleRootItem()->appendRow(d->currentItem);
}

void SearchResultsModel::addMatched(const QString &file, const QString &text, int line, int start, int end)
{
    if (d->currentItem == nullptr)
        return;
    auto resultItem = new QStandardItem(text);
    // clang-format off
    auto tooltip = tr("%1<hr/>%2<br/>line %3, column %4 to %5")
        .arg(text.trimmed().toHtmlEscaped())
        .arg(file.toHtmlEscaped())
        .arg(line).arg(start + 1).arg(end + 1);
    // clang-format on
    resultItem->setData(tooltip, Qt::ToolTipRole);
    resultItem->setData(file, FileNameRole);
    resultItem->setData(line, LineNumberRole);
    resultItem->setData(start, StartColumnRole);
    resultItem->setData(end, EndColumnRole);
    d->currentItem->appendRow(resultItem);
}