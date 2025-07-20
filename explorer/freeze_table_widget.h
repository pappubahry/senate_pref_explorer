// Following https://doc.qt.io/qt-5/qtwidgets-itemviews-frozencolumn-example.html, which
// is released under the GNU Free Documentation license.

#ifndef FREEZE_TABLE_WIDGET_H
#define FREEZE_TABLE_WIDGET_H

#include "table_view.h"
#include <QWidget>

class Table_window;

class Freeze_table_widget : public Table_view
{
  Q_OBJECT

public:
  Freeze_table_widget(QAbstractItemModel* model,
                      QStringList variable_widths,
                      QString regular_width,
                      bool use_bold,
                      Table_window* parent);
  ~Freeze_table_widget() override;

  void hide_column(int col);

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  Table_window* _parent;
  Table_view* _frozen_table_view;
  void _init();
  void _update_frozen_table_geometry();
  void _style_table(QTableView* t, int width_1, int width_2);
};

#endif // FREEZE_TABLE_WIDGET_H
