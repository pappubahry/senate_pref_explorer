// Following https://doc.qt.io/qt-5/qtwidgets-itemviews-frozencolumn-example.html, which
// is released under the GNU Free Documentation license.

#include "freezetablewidget.h"
#include <QHeaderView>
#include <QScrollBar>
#include <QFontMetrics>
#include <QFont>

FreezeTableWidget::FreezeTableWidget(QAbstractItemModel *model,
                                     QStringList variable_widths,
                                     QString regular_width,
                                     bool use_bold,
                                     QWidget *parent)
{
  _parent = parent;
  setModel(model);
  frozenTableView = new QTableView(this);
  
  QFont font_bold;
  font_bold.setBold(use_bold);
  
  int width_1 = QFontMetrics(font_bold).boundingRect(regular_width).width() + 5;
  
  int width_2 = 10;
  for (int i = 0; i < variable_widths.length(); i++)
  {
    width_2 = qMax(width_2, QFontMetrics(font_bold).boundingRect(variable_widths.at(i)).width());
  }
  
  width_2 += 20;
  
  init();
  
  style_table(this, width_1, width_2);
  style_table(frozenTableView, width_1, width_2);
  
  connect(frozenTableView->verticalScrollBar(), &QAbstractSlider::valueChanged,
          verticalScrollBar(), &QAbstractSlider::setValue);
  
  connect(verticalScrollBar(), &QAbstractSlider::valueChanged,
          frozenTableView->verticalScrollBar(), &QAbstractSlider::setValue);

}

FreezeTableWidget::~FreezeTableWidget()
{
  delete frozenTableView;
}

void FreezeTableWidget::init()
{
  frozenTableView->setModel(model());
  frozenTableView->setFocusPolicy(Qt::NoFocus);
  frozenTableView->verticalHeader()->hide();
  frozenTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  
  frozenTableView->setFrameStyle(QFrame::NoFrame);
  
  viewport()->stackUnder(frozenTableView);
  
  for (int col = 2; col < model()->columnCount(); ++col)
  {
    frozenTableView->setColumnHidden(col, true);
  }
  
  // This border is too thick:
  //frozenTableView->horizontalHeader()->setFrameStyle(QFrame::Box);

  frozenTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  frozenTableView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  frozenTableView->show();

  updateFrozenTableGeometry();

  setHorizontalScrollMode(ScrollPerPixel);
  setVerticalScrollMode(ScrollPerPixel);
  frozenTableView->setVerticalScrollMode(ScrollPerPixel);
  
  frozenTableView->horizontalScrollBar()->setEnabled(false);
  
  connect(frozenTableView->horizontalHeader(), SIGNAL(sectionClicked(int)),          _parent, SLOT(clicked_header(int)));
  connect(frozenTableView,                     SIGNAL(clicked(const QModelIndex &)), _parent, SLOT(clicked_table(const QModelIndex &)));
}

void FreezeTableWidget::resizeEvent(QResizeEvent * event)
{
  QTableView::resizeEvent(event);
  updateFrozenTableGeometry();
}

void FreezeTableWidget::updateFrozenTableGeometry()
{
  frozenTableView->setGeometry(verticalHeader()->width() + frameWidth(),
                               frameWidth(), columnWidth(0) + columnWidth(1) - 0,
                               viewport()->height() + horizontalHeader()->height());
}

void FreezeTableWidget::style_table(QTableView *t, int width_1, int width_2)
{
  t->setEditTriggers(QAbstractItemView::NoEditTriggers);
  t->setShowGrid(false);
  t->setAlternatingRowColors(true);
  t->setStyleSheet("QTableView {alternate-background-color: #f0f0f0; background-color: #ffffff}");
  t->setFocusPolicy(Qt::NoFocus);
  t->setSelectionMode(QAbstractItemView::NoSelection);
  t->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  t->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  t->verticalHeader()->setDefaultSectionSize(t->fontMetrics().boundingRect("0").height());
  t->verticalHeader()->hide();
  t->horizontalHeader()->setDefaultSectionSize(width_1);
  t->setColumnWidth(1, width_2);
}
