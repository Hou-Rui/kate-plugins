#include "SearchResultsView.hpp"
#include "SearchResultsModel.hpp"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QPainter>
#include <QPalette>
#include <QStandardItemModel>
#include <QStyleOptionViewItem>
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

    // Only reserve and draw the check indicator while the replace options are
    // visible; otherwise the results read as a plain list.
    auto view = qobject_cast<const SearchResultsView *>(opt.widget);
    if (view && !view->showCheckboxes())
        opt.features &= ~QStyleOptionViewItem::HasCheckIndicator;

    auto style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

    if (opt.features & QStyleOptionViewItem::HasCheckIndicator) {
        auto checkRect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt, opt.widget);
        QStyleOptionViewItem checkOpt = opt;
        checkOpt.rect = checkRect;
        checkOpt.state = checkOpt.state & ~QStyle::State_HasFocus;
        switch (opt.checkState) {
        case Qt::Checked:
            checkOpt.state |= QStyle::State_On;
            break;
        case Qt::PartiallyChecked:
            checkOpt.state |= QStyle::State_NoChange;
            break;
        case Qt::Unchecked:
        default:
            checkOpt.state |= QStyle::State_Off;
            break;
        }
        style->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &checkOpt, painter, opt.widget);
    }

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

    createActions();
}

void SearchResultsView::createActions()
{
    auto selectionModel = qobject_cast<SearchResultsModel *>(model());

    m_selectAllAction = new QAction(QIcon::fromTheme("edit-select-all"), tr("Select All"), this);
    m_selectAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));
    connect(m_selectAllAction, &QAction::triggered, selectionModel, &SearchResultsModel::selectAll);

    m_deselectAllAction = new QAction(QIcon::fromTheme("edit-select-none"), tr("De-select All"), this);
    m_deselectAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    connect(m_deselectAllAction, &QAction::triggered, selectionModel, &SearchResultsModel::deselectAll);

    m_invertSelectionAction = new QAction(tr("Invert Selection"), this);
    m_invertSelectionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(m_invertSelectionAction, &QAction::triggered, selectionModel, &SearchResultsModel::invertSelection);

    m_jumpToResultAction = new QAction(QIcon::fromTheme("go-jump"), tr("Jump to Result"), this);
    m_jumpToResultAction->setShortcut(QKeySequence(Qt::Key_Return));
    connect(m_jumpToResultAction, &QAction::triggered, this, &SearchResultsView::jumpToCurrentResult);

    m_jumpToFileAction = new QAction(QIcon::fromTheme("go-jump"), tr("Jump to File"), this);
    connect(m_jumpToFileAction, &QAction::triggered, this, &SearchResultsView::jumpToCurrentFile);

    m_expandFileAction = new QAction(tr("Expand Results in File"), this);
    m_expandFileAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right));
    connect(m_expandFileAction, &QAction::triggered, this, &SearchResultsView::expandCurrentFile);

    m_collapseFileAction = new QAction(tr("Collapse Results in File"), this);
    m_collapseFileAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left));
    connect(m_collapseFileAction, &QAction::triggered, this, &SearchResultsView::collapseCurrentFile);

    m_expandAllAction = new QAction(tr("Expand All"), this);
    m_expandAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    connect(m_expandAllAction, &QAction::triggered, this, &QTreeView::expandAll);

    m_collapseAllAction = new QAction(tr("Collapse All"), this);
    m_collapseAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    connect(m_collapseAllAction, &QAction::triggered, this, &QTreeView::collapseAll);

    // Register the actions on the view so their shortcuts fire while it has
    // focus, even when the context menu is not open.
    const auto actions = {m_selectAllAction,    m_deselectAllAction, m_invertSelectionAction,
                          m_jumpToResultAction, m_jumpToFileAction,  m_expandFileAction,
                          m_collapseFileAction, m_expandAllAction,   m_collapseAllAction};
    for (auto action : actions) {
        action->setShortcutContext(Qt::WidgetShortcut);
        addAction(action);
    }
}

bool SearchResultsView::showCheckboxes() const
{
    return m_showCheckboxes;
}

void SearchResultsView::setShowCheckboxes(bool show)
{
    if (m_showCheckboxes == show)
        return;
    m_showCheckboxes = show;
    viewport()->update();
}

QRect SearchResultsView::checkBoxRect(const QModelIndex &index) const
{
    if (!m_showCheckboxes || !(index.flags() & Qt::ItemIsUserCheckable))
        return QRect();
    QStyleOptionViewItem opt;
    opt.initFrom(this);
    opt.rect = visualRect(index);
    opt.features |= QStyleOptionViewItem::HasCheckIndicator;
    opt.decorationSize = iconSize();
    return style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt, this);
}

void SearchResultsView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        auto index = indexAt(event->pos());
        if (index.isValid() && checkBoxRect(index).contains(event->pos())) {
            // Toggle the result's inclusion without jumping to it.
            auto state = index.data(Qt::CheckStateRole).toInt();
            auto next = state == Qt::Checked ? Qt::Unchecked : Qt::Checked;
            model()->setData(index, next, Qt::CheckStateRole);
            return;
        }
    }

    QTreeView::mousePressEvent(event);
    if (event->button() != Qt::LeftButton)
        return;

    auto index = indexAt(event->pos());
    if (!index.isValid())
        return;

    if (index.parent() == rootIndex())
        expand(index);

    jumpTo(index);
}

void SearchResultsView::jumpTo(const QModelIndex &index)
{
    if (!index.isValid())
        return;

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

QModelIndex SearchResultsView::fileIndexFor(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();
    return isMatchedLine(index) ? index.parent() : index;
}

void SearchResultsView::jumpToCurrentResult()
{
    jumpTo(currentIndex());
}

void SearchResultsView::jumpToCurrentFile()
{
    // Jumping to a file index (never a matched line) emits jumpToFile.
    jumpTo(fileIndexFor(currentIndex()));
}

void SearchResultsView::expandCurrentFile()
{
    if (auto file = fileIndexFor(currentIndex()); file.isValid())
        expand(file);
}

void SearchResultsView::collapseCurrentFile()
{
    if (auto file = fileIndexFor(currentIndex()); file.isValid())
        collapse(file);
}

void SearchResultsView::contextMenuEvent(QContextMenuEvent *event)
{
    // Make the right-clicked row current so the per-item actions target it.
    if (auto index = indexAt(event->pos()); index.isValid())
        setCurrentIndex(index);

    auto current = currentIndex();
    bool hasResults = model() && model()->rowCount() > 0;
    m_selectAllAction->setEnabled(hasResults);
    m_deselectAllAction->setEnabled(hasResults);
    m_invertSelectionAction->setEnabled(hasResults);
    bool onMatchedLine = isMatchedLine(current);
    m_jumpToResultAction->setEnabled(onMatchedLine);
    m_jumpToFileAction->setEnabled(current.isValid());
    m_expandFileAction->setEnabled(current.isValid());
    m_collapseFileAction->setEnabled(current.isValid());
    m_expandAllAction->setEnabled(hasResults);
    m_collapseAllAction->setEnabled(hasResults);

    QMenu menu(this);
    // The selection actions only make sense while the checkboxes are visible
    // (i.e. the replace options are shown), so omit them otherwise.
    if (m_showCheckboxes) {
        menu.addAction(m_selectAllAction);
        menu.addAction(m_deselectAllAction);
        menu.addAction(m_invertSelectionAction);
        menu.addSeparator();
    }
    // On a result line, offer "Jump to Result"; on a file item, "Jump to File".
    menu.addAction(onMatchedLine ? m_jumpToResultAction : m_jumpToFileAction);
    menu.addSeparator();
    menu.addAction(m_expandFileAction);
    menu.addAction(m_collapseFileAction);
    menu.addSeparator();
    menu.addAction(m_expandAllAction);
    menu.addAction(m_collapseAllAction);
    menu.exec(event->globalPos());
}

#include "SearchResultsView.moc"
