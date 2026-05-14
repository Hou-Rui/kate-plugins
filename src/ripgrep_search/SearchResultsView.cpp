#include "SearchResultsView.hpp"
#include "SearchResultsModel.hpp"

#include <QApplication>
#include <QFileInfo>
#include <QPainter>
#include <QPalette>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QTreeView>

class SearchResultDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

static inline bool isMatchedLine(const QModelIndex &index)
{
    auto role = index.data(SearchResultsModel::LineNumberRole);
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
    int start = index.data(SearchResultsModel::StartColumnRole).toInt() - trimmed;
    int end = index.data(SearchResultsModel::EndColumnRole).toInt() - trimmed;

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

    auto file = index.data(SearchResultsModel::FileNameRole).toString();
    if (isMatchedLine(index)) {
        auto line = index.data(SearchResultsModel::LineNumberRole).toInt();
        auto start = index.data(SearchResultsModel::StartColumnRole).toInt();
        auto end = index.data(SearchResultsModel::EndColumnRole).toInt();
        emit jumpToResult(file, line, start, end);
    } else {
        emit jumpToFile(file);
    }
}

#include "SearchResultsView.moc"
