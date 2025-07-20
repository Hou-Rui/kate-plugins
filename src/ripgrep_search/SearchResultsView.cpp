#include "SearchResultsView.hpp"
#include "RipgrepSearchPlugin.hpp"

#include <QApplication>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <qtreeview.h>

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

void SearchResultDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QString text = index.data(Qt::DisplayRole).toString();
    int highlightStart = index.data(StartColumnRole).toInt();
    int highlightEnd = index.data(EndColumnRole).toInt();

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    painter->save();

    // Draw item background (e.g., selection, hover, etc.)
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

    // Prepare the text layout
    QTextLayout textLayout(text, opt.font);
    textLayout.beginLayout();
    QTextLine line = textLayout.createLine();
    line.setLineWidth(opt.rect.width());
    textLayout.endLayout();

    // Positioning
    QPointF textPos = opt.rect.topLeft();
    textPos.setX(textPos.x() + 4); // small left padding
    textPos.setY(textPos.y() + (opt.rect.height() - line.height()) / 2);

    // Break text into 3 parts: before, highlight, after
    QList<QTextLayout::FormatRange> formats;
    if (highlightStart >= 0 && highlightEnd > highlightStart && highlightEnd <= text.length()) {
        QTextCharFormat normalFormat;
        QTextCharFormat highlightFormat;
        highlightFormat.setBackground(opt.palette.highlight().color());
        highlightFormat.setForeground(opt.palette.highlightedText());

        if (highlightStart > 0) {
            formats.append({0, highlightStart, normalFormat});
        }

        formats.append({highlightStart, highlightEnd - highlightStart, highlightFormat});

        if (highlightEnd < text.length()) {
            int length = text.length() - highlightEnd;
            formats.append({highlightEnd, length, normalFormat});
        }

        textLayout.setFormats(formats);
    }

    // Draw the text
    textLayout.draw(painter, textPos);
    painter->restore();
}

SearchResultsView::SearchResultsView(SearchResultsModel *model, QWidget *parent)
    : QTreeView(parent)
{
    setModel(model);
    setItemDelegate(new SearchResultDelegate(this));
}
