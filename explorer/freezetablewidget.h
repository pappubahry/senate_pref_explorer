#ifndef FREEZETABLEWIDGET_H
#define FREEZETABLEWIDGET_H

#include <QWidget>
#include <QTableView>

class FreezeTableWidget : public QTableView {
  Q_OBJECT

public:
  FreezeTableWidget(QAbstractItemModel *model,
                    QStringList variable_widths,
                    QString regular_width,
                    bool use_bold,
                    QWidget *parent);
  ~FreezeTableWidget() override;

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  QWidget *_parent;
  QTableView *frozenTableView;
  void init();
  void updateFrozenTableGeometry();
  void style_table(QTableView *t, int width_1, int width_2);
};

#endif // FREEZETABLEWIDGET_H
