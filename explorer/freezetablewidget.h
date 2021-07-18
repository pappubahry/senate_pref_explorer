// Following https://doc.qt.io/qt-5/qtwidgets-itemviews-frozencolumn-example.html, which
// is released under the GNU Free Documentation license.

#ifndef FREEZETABLEWIDGET_H
#define FREEZETABLEWIDGET_H

#include <QWidget>
#include <QTableView>

class Table_window;

class FreezeTableWidget : public QTableView {
  Q_OBJECT

public:
  FreezeTableWidget(QAbstractItemModel *model,
                    QStringList variable_widths,
                    QString regular_width,
                    bool use_bold,
                    Table_window *parent);
  ~FreezeTableWidget() override;

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  Table_window *_parent;
  QTableView *frozenTableView;
  void init();
  void updateFrozenTableGeometry();
  void style_table(QTableView *t, int width_1, int width_2);
};

#endif // FREEZETABLEWIDGET_H
