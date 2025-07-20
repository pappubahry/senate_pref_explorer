#ifndef TABLE_VIEW_H
#define TABLE_VIEW_H

#include <QStyledItemDelegate>
#include <QTableView>

// In Qt 6.9, table cells can have their background appear dark blue when
// scrollwheeling or mousedown-mousemove-ing.  I am not sure of the precise
// cause, but of about 10 suggested fixes from o4-mini-high, the following
// delegate was the only one that worked.

class No_hover_delegate : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void initStyleOption(QStyleOptionViewItem* opt, const QModelIndex& idx) const override
  {
    QStyledItemDelegate::initStyleOption(opt, idx);
    opt->state &= ~QStyle::State_MouseOver;
  }
};

class Table_view : public QTableView
{
public:
  explicit Table_view(QWidget* parent = nullptr)
    : QTableView(parent)
  {
    setItemDelegate(new No_hover_delegate(this));
  }
};

#endif // TABLE_VIEW_H
