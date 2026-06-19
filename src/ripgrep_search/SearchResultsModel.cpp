#include "SearchResultsModel.hpp"

#include <KFileItem>
#include <QFileInfo>
#include <QStandardItemModel>

struct SearchResultsModelPrivate {
    void onItemChanged(QStandardItem *item);
    void updateParentState(QStandardItem *parent);

    SearchResultsModel *q;
    QStandardItem *currentItem = nullptr;
    bool updatingChecks = false;
};

SearchResultsModel::SearchResultsModel(QObject *parent)
    : QStandardItemModel(parent)
    , d(new SearchResultsModelPrivate)
{
    d->q = this;
    connect(this, &QStandardItemModel::itemChanged, this, [this](QStandardItem *item) {
        d->onItemChanged(item);
    });
}

SearchResultsModel::~SearchResultsModel() = default;

void SearchResultsModel::clear()
{
    d->currentItem = nullptr;
    QStandardItemModel::clear();
}

void SearchResultsModelPrivate::onItemChanged(QStandardItem *item)
{
    if (updatingChecks)
        return;
    if (!(item->flags() & Qt::ItemIsUserCheckable))
        return;

    updatingChecks = true;
    if (item->hasChildren()) {
        // File item toggled: propagate its state to every result line below it.
        auto state = item->checkState();
        if (state != Qt::PartiallyChecked) {
            for (int i = 0; i < item->rowCount(); ++i)
                item->child(i)->setCheckState(state);
        }
    } else if (auto parent = item->parent()) {
        // Result line toggled: refresh the parent file's tri-state.
        updateParentState(parent);
    }
    updatingChecks = false;
}

void SearchResultsModelPrivate::updateParentState(QStandardItem *parent)
{
    int checked = 0;
    int total = parent->rowCount();
    for (int i = 0; i < total; ++i) {
        if (parent->child(i)->checkState() == Qt::Checked)
            checked++;
    }
    auto state = checked == 0 ? Qt::Unchecked : (checked == total ? Qt::Checked : Qt::PartiallyChecked);
    parent->setCheckState(state);
}

QVector<ReplacementTarget> SearchResultsModel::checkedResults() const
{
    QVector<ReplacementTarget> result;
    auto root = invisibleRootItem();
    for (int i = 0; i < root->rowCount(); ++i) {
        auto fileItem = root->child(i);
        for (int j = 0; j < fileItem->rowCount(); ++j) {
            auto item = fileItem->child(j);
            if (item->checkState() == Qt::Unchecked)
                continue;
            result.append({
                item->data(FileNameRole).toString(),
                item->data(LineNumberRole).toInt(),
                item->data(StartColumnRole).toInt(),
                item->data(EndColumnRole).toInt(),
            });
        }
    }
    return result;
}

void SearchResultsModel::selectAll()
{
    // Toggling a file item cascades to its result lines via onItemChanged().
    auto root = invisibleRootItem();
    for (int i = 0; i < root->rowCount(); ++i)
        root->child(i)->setCheckState(Qt::Checked);
}

void SearchResultsModel::deselectAll()
{
    auto root = invisibleRootItem();
    for (int i = 0; i < root->rowCount(); ++i)
        root->child(i)->setCheckState(Qt::Unchecked);
}

void SearchResultsModel::invertSelection()
{
    // Flip each result line; the parent file tri-state is refreshed per change.
    auto root = invisibleRootItem();
    for (int i = 0; i < root->rowCount(); ++i) {
        auto fileItem = root->child(i);
        for (int j = 0; j < fileItem->rowCount(); ++j) {
            auto item = fileItem->child(j);
            auto next = item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked;
            item->setCheckState(next);
        }
    }
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
    d->currentItem->setCheckable(true);
    d->currentItem->setAutoTristate(true);
    d->currentItem->setCheckState(Qt::Checked);
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
    resultItem->setCheckable(true);
    resultItem->setCheckState(Qt::Checked);
    d->currentItem->appendRow(resultItem);
}
