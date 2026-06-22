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

class SearchResultsViewPrivate : public QObject
{
    Q_OBJECT
public slots:
    void jumpToCurrentResult();
    void jumpToCurrentFile();
    void expandCurrentFile();
    void collapseCurrentFile();

public:
    QRect checkBoxRect(const QModelIndex &index) const;
    void createActions();
    void jumpTo(const QModelIndex &index);
    QModelIndex fileIndexFor(const QModelIndex &index) const;

    SearchResultsView *q;
    bool showCheckboxes = false;

    QAction *selectAllAction = nullptr;
    QAction *deselectAllAction = nullptr;
    QAction *invertSelectionAction = nullptr;
    QAction *jumpToResultAction = nullptr;
    QAction *jumpToFileAction = nullptr;
    QAction *expandFileAction = nullptr;
    QAction *collapseFileAction = nullptr;
    QAction *expandAllAction = nullptr;
    QAction *collapseAllAction = nullptr;
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
    , d(new SearchResultsViewPrivate)
{
    d->moveToThread(thread());
    d->q = this;

    setModel(model);
    setItemDelegate(new SearchResultDelegate(this));
    setWordWrap(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setUniformRowHeights(true);
    setEditTriggers(NoEditTriggers);

    connect(model, &SearchResultsModel::rowsInserted, [this](const QModelIndex &parent, auto, auto) {
        expand(parent);
    });

    d->createActions();
}

SearchResultsView::~SearchResultsView() = default;

void SearchResultsViewPrivate::createActions()
{
    auto selectionModel = qobject_cast<SearchResultsModel *>(q->model());

    selectAllAction = new QAction(QIcon::fromTheme("edit-select-all"), tr("Select All"), q);
    selectAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));
    connect(selectAllAction, &QAction::triggered, selectionModel, &SearchResultsModel::selectAll);

    deselectAllAction = new QAction(QIcon::fromTheme("edit-select-none"), tr("De-select All"), q);
    deselectAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    connect(deselectAllAction, &QAction::triggered, selectionModel, &SearchResultsModel::deselectAll);

    invertSelectionAction = new QAction(tr("Invert Selection"), q);
    invertSelectionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(invertSelectionAction, &QAction::triggered, selectionModel, &SearchResultsModel::invertSelection);

    jumpToResultAction = new QAction(QIcon::fromTheme("go-jump"), tr("Jump to Result"), q);
    jumpToResultAction->setShortcut(QKeySequence(Qt::Key_Return));
    connect(jumpToResultAction, &QAction::triggered, this, &SearchResultsViewPrivate::jumpToCurrentResult);

    jumpToFileAction = new QAction(QIcon::fromTheme("go-jump"), tr("Jump to File"), q);
    connect(jumpToFileAction, &QAction::triggered, this, &SearchResultsViewPrivate::jumpToCurrentFile);

    expandFileAction = new QAction(tr("Expand Results in File"), q);
    expandFileAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right));
    connect(expandFileAction, &QAction::triggered, this, &SearchResultsViewPrivate::expandCurrentFile);

    collapseFileAction = new QAction(tr("Collapse Results in File"), q);
    collapseFileAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left));
    connect(collapseFileAction, &QAction::triggered, this, &SearchResultsViewPrivate::collapseCurrentFile);

    expandAllAction = new QAction(tr("Expand All"), q);
    expandAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    connect(expandAllAction, &QAction::triggered, q, &QTreeView::expandAll);

    collapseAllAction = new QAction(tr("Collapse All"), q);
    collapseAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    connect(collapseAllAction, &QAction::triggered, q, &QTreeView::collapseAll);

    // Register the actions on the view so their shortcuts fire while it has
    // focus, even when the context menu is not open.
    const auto actions = {selectAllAction,
                          deselectAllAction,
                          invertSelectionAction,
                          jumpToResultAction,
                          jumpToFileAction,
                          expandFileAction,
                          collapseFileAction,
                          expandAllAction,
                          collapseAllAction};
    for (auto action : actions) {
        action->setShortcutContext(Qt::WidgetShortcut);
        q->addAction(action);
    }
}

bool SearchResultsView::showCheckboxes() const
{
    return d->showCheckboxes;
}

void SearchResultsView::setShowCheckboxes(bool show)
{
    if (d->showCheckboxes == show)
        return;
    d->showCheckboxes = show;
    viewport()->update();
}

QRect SearchResultsViewPrivate::checkBoxRect(const QModelIndex &index) const
{
    if (!showCheckboxes || !(index.flags() & Qt::ItemIsUserCheckable))
        return QRect();
    QStyleOptionViewItem opt;
    opt.initFrom(q);
    opt.rect = q->visualRect(index);
    opt.features |= QStyleOptionViewItem::HasCheckIndicator;
    opt.decorationSize = q->iconSize();
    return q->style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt, q);
}

void SearchResultsView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        auto index = indexAt(event->pos());
        if (index.isValid() && d->checkBoxRect(index).contains(event->pos())) {
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

    d->jumpTo(index);
}

void SearchResultsViewPrivate::jumpTo(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    auto file = index.data(SearchResultsModel::FileNameRole).toString();
    if (isMatchedLine(index)) {
        auto byteStart = index.data(SearchResultsModel::ByteStartRole).toLongLong();
        auto byteEnd = index.data(SearchResultsModel::ByteEndRole).toLongLong();
        emit q->jumpToResult(file, byteStart, byteEnd);
    } else {
        emit q->jumpToFile(file);
    }
}

QModelIndex SearchResultsViewPrivate::fileIndexFor(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();
    return isMatchedLine(index) ? index.parent() : index;
}

void SearchResultsViewPrivate::jumpToCurrentResult()
{
    jumpTo(q->currentIndex());
}

void SearchResultsViewPrivate::jumpToCurrentFile()
{
    // Jumping to a file index (never a matched line) emits jumpToFile.
    jumpTo(fileIndexFor(q->currentIndex()));
}

void SearchResultsViewPrivate::expandCurrentFile()
{
    if (auto file = fileIndexFor(q->currentIndex()); file.isValid())
        q->expand(file);
}

void SearchResultsViewPrivate::collapseCurrentFile()
{
    if (auto file = fileIndexFor(q->currentIndex()); file.isValid())
        q->collapse(file);
}

void SearchResultsView::contextMenuEvent(QContextMenuEvent *event)
{
    // Make the right-clicked row current so the per-item actions target it.
    if (auto index = indexAt(event->pos()); index.isValid())
        setCurrentIndex(index);

    auto current = currentIndex();
    bool hasResults = model() && model()->rowCount() > 0;
    d->selectAllAction->setEnabled(hasResults);
    d->deselectAllAction->setEnabled(hasResults);
    d->invertSelectionAction->setEnabled(hasResults);
    bool onMatchedLine = isMatchedLine(current);
    d->jumpToResultAction->setEnabled(onMatchedLine);
    d->jumpToFileAction->setEnabled(current.isValid());
    d->expandFileAction->setEnabled(current.isValid());
    d->collapseFileAction->setEnabled(current.isValid());
    d->expandAllAction->setEnabled(hasResults);
    d->collapseAllAction->setEnabled(hasResults);

    QMenu menu(this);
    // The selection actions only make sense while the checkboxes are visible
    // (i.e. the replace options are shown), so omit them otherwise.
    if (d->showCheckboxes) {
        menu.addAction(d->selectAllAction);
        menu.addAction(d->deselectAllAction);
        menu.addAction(d->invertSelectionAction);
        menu.addSeparator();
    }
    // On a result line, offer "Jump to Result"; on a file item, "Jump to File".
    menu.addAction(onMatchedLine ? d->jumpToResultAction : d->jumpToFileAction);
    menu.addSeparator();
    menu.addAction(d->expandFileAction);
    menu.addAction(d->collapseFileAction);
    menu.addSeparator();
    menu.addAction(d->expandAllAction);
    menu.addAction(d->collapseAllAction);
    menu.exec(event->globalPos());
}

#include "SearchResultsView.moc"
