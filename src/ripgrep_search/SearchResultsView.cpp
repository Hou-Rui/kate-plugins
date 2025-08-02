#include "SearchResultsView.hpp"
#include "RipgrepSearchPlugin.hpp"

#include <KFileItem>

#include <QApplication>
#include <QFileInfo>
#include <QPainter>
#include <QPalette>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QTreeView>
#include <qnamespace.h>

enum ItemDataRole {
    FileNameRole = Qt::UserRole,
    LineNumberRole,
    StartColumnRole,
    EndColumnRole,
};

class SearchResultDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

static inline bool isMatchedLine(const QModelIndex &index)
{
    auto role = index.data(LineNumberRole);
    return role.isValid() && role.canConvert<int>();
}

static std::pair<int, QString> trimLeft(const QString &str)
{
    auto result = str.trimmed();
    for (int i = 0; i < str.length(); i++) {
        if (!str[i].isSpace())
            return {i, result};
    }
    return {str.length(), result};
}

static QList<QTextLayout::FormatRange> highlightFormats(const QPalette &palette, int length, int start, int end)
{
    QList<QTextLayout::FormatRange> formats;
    if (start >= 0 && end > start && end <= length) {
        QTextCharFormat normal;
        QTextCharFormat highlight;
        highlight.setBackground(palette.highlight().color());
        highlight.setForeground(palette.highlightedText());
        if (start > 0)
            formats.append({0, start, normal});
        formats.append({start, end - start, highlight});
        if (end < length)
            formats.append({end, length - end, normal});
    }
    return formats;
}

void SearchResultDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    painter->save();

    auto style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

    auto icon = index.data(Qt::DecorationRole).value<QIcon>();
    auto iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);
    if (!icon.isNull())
        icon.paint(painter, iconRect, Qt::AlignCenter);

    const auto &[trimmed, text] = trimLeft(index.data(Qt::DisplayRole).toString());
    int start = index.data(StartColumnRole).toInt() - trimmed;
    int end = index.data(EndColumnRole).toInt() - trimmed;

    QTextLayout layout(text, opt.font);
    if (isMatchedLine(index)) {
        const auto &formats = highlightFormats(opt.palette, text.length(), start, end);
        layout.setFormats(formats);
    }
    layout.beginLayout();
    auto line = layout.createLine();
    layout.endLayout();

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
    int x = iconRect.right() + style->pixelMetric(QStyle::PM_LineEditIconMargin);
    int y = opt.rect.top() + (opt.rect.height() - line.height()) / 2;
    layout.draw(painter, QPointF(x, y));

    painter->restore();
}

SearchResultsView::SearchResultsView(SearchResultsModel *model, QWidget *parent)
    : QTreeView(parent)
{
    setModel(model);
    setItemDelegate(new SearchResultDelegate(this));
    setWordWrap(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setUniformRowHeights(true);
    setEditTriggers(NoEditTriggers);

    connect(model, &SearchResultsModel::rowsInserted, [this](const QModelIndex &parent, auto, auto) {
        expand(parent);
    });
}

void SearchResultsView::mousePressEvent(QMouseEvent *event)
{
    QTreeView::mousePressEvent(event);
    if (event->button() != Qt::LeftButton)
        return;

    auto index = indexAt(event->pos());
    if (!index.isValid())
        return;

    if (index.parent() == rootIndex())
        expand(index);

    auto file = index.data(FileNameRole).toString();
    if (isMatchedLine(index)) {
        auto line = index.data(LineNumberRole).toInt();
        auto start = index.data(StartColumnRole).toInt();
        auto end = index.data(EndColumnRole).toInt();
        emit jumpToResult(file, line, start, end);
    } else {
        emit jumpToFile(file);
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
    m_currentItem = new QStandardItem(icon, text);
    m_currentItem->setData(file, Qt::ToolTipRole);
    m_currentItem->setData(file, FileNameRole);
    invisibleRootItem()->appendRow(m_currentItem);
}

void SearchResultsModel::addMatched(const QString &file, const QString &text, int line, int start, int end)
{
    auto resultItem = new QStandardItem(text);
    auto tooltip = tr("%1, line %2, column %3 to %4").arg(file).arg(line).arg(start + 1).arg(end + 1);
    resultItem->setData(tooltip, Qt::ToolTipRole);
    resultItem->setData(file, FileNameRole);
    resultItem->setData(line, LineNumberRole);
    resultItem->setData(start, StartColumnRole);
    resultItem->setData(end, EndColumnRole);
    m_currentItem->appendRow(resultItem);
}

#include "SearchResultsView.moc"
