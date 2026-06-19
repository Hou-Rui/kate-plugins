#pragma once
#include <QTreeView>

class QAction;
class SearchResultsModel;

class SearchResultsView : public QTreeView
{
    Q_OBJECT
public:
    explicit SearchResultsView(SearchResultsModel *model, QWidget *parent = nullptr);

    bool showCheckboxes() const;
    void setShowCheckboxes(bool show);

signals:
    void jumpToFile(const QString &file);
    void jumpToResult(const QString &file, int line, int start, int end);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void jumpToCurrentResult();
    void expandCurrentFile();
    void collapseCurrentFile();

private:
    QRect checkBoxRect(const QModelIndex &index) const;
    void createActions();
    void jumpTo(const QModelIndex &index);
    QModelIndex fileIndexFor(const QModelIndex &index) const;

    bool m_showCheckboxes = false;

    QAction *m_selectAllAction = nullptr;
    QAction *m_deselectAllAction = nullptr;
    QAction *m_invertSelectionAction = nullptr;
    QAction *m_jumpToResultAction = nullptr;
    QAction *m_expandFileAction = nullptr;
    QAction *m_collapseFileAction = nullptr;
    QAction *m_expandAllAction = nullptr;
    QAction *m_collapseAllAction = nullptr;
};
