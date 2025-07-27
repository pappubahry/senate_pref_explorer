// Following https://doc.qt.io/qt-5/qtwidgets-itemviews-frozencolumn-example.html, which
// is released under the GNU Free Documentation license.

#include "freeze_table_widget.h"
#include "table_view.h"
#include "table_window.h"
#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QScrollBar>

Freeze_table_widget::Freeze_table_widget(QAbstractItemModel* model,
                                         QStringList variable_widths,
                                         QString regular_width,
                                         bool use_bold,
                                         Table_window* parent)
{
  _parent = parent;
  setModel(model);
  _frozen_table_view = new Table_view(this);

  QFont font_bold;
  font_bold.setBold(use_bold);

  const int width_1 = QFontMetrics(font_bold).boundingRect(regular_width).width() + 10;

  int width_2 = 10;

  for (int i = 0; i < variable_widths.length(); i++)
  {
    width_2 = qMax(width_2, QFontMetrics(font_bold).boundingRect(variable_widths.at(i)).width());
  }

  width_2 += 20;

  _init();

  _style_table(this, width_1, width_2);
  _style_table(_frozen_table_view, width_1, width_2);

  connect(_frozen_table_view->verticalScrollBar(), &QAbstractSlider::valueChanged, verticalScrollBar(), &QAbstractSlider::setValue);

  connect(verticalScrollBar(), &QAbstractSlider::valueChanged, _frozen_table_view->verticalScrollBar(), &QAbstractSlider::setValue);
}

Freeze_table_widget::~Freeze_table_widget()
{
  delete _frozen_table_view;
}

void Freeze_table_widget::hide_column(int col)
{
  setColumnHidden(col, true);
  _frozen_table_view->setColumnHidden(col, true);
  if (col < 2)
  {
    _update_frozen_table_geometry();
  }
}

void Freeze_table_widget::_init()
{
  _frozen_table_view->setModel(model());
  _frozen_table_view->setFocusPolicy(Qt::NoFocus);
  _frozen_table_view->verticalHeader()->hide();
  _frozen_table_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);

  _frozen_table_view->setFrameStyle(QFrame::NoFrame);

  viewport()->stackUnder(_frozen_table_view);

  for (int col = 2; col < model()->columnCount(); ++col)
  {
    _frozen_table_view->setColumnHidden(col, true);
  }

  // This border is too thick:
  //_frozen_table_view->horizontalHeader()->setFrameStyle(QFrame::Box);

  _frozen_table_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  _frozen_table_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  _frozen_table_view->show();

  _update_frozen_table_geometry();

  setHorizontalScrollMode(ScrollPerPixel);
  setVerticalScrollMode(ScrollPerPixel);
  _frozen_table_view->setVerticalScrollMode(ScrollPerPixel);
  _frozen_table_view->horizontalScrollBar()->setEnabled(false);

  // A hack to make it look like there's a divider between
  // the last frozen column and the first moving column;
  // this wasn't necessary in Qt 5.  [o4-mini-high]
  QFrame* divider = new QFrame(this);
  divider->setFrameShape(QFrame::VLine);
  divider->setFrameShadow(QFrame::Plain);
  divider->setLineWidth(1);
  divider->setMidLineWidth(0);
  divider->setStyleSheet("color: #E0E0E0;");

  _frozen_table_view->viewport()->setCursor(Qt::PointingHandCursor);
  _frozen_table_view->horizontalHeader()->viewport()->setCursor(Qt::PointingHandCursor);
  horizontalHeader()->viewport()->setCursor(Qt::PointingHandCursor);

  auto update_divider = [=]()
  {
    // world‐coordinate x of the seam:
    const int x = _frozen_table_view->horizontalHeader()->sectionPosition(1)
                  + _frozen_table_view->verticalHeader()->width()
                  + _frozen_table_view->columnWidth(1);
    divider->setGeometry(x, 1, 1, _frozen_table_view->horizontalHeader()->height() - 2);
  };
  connect(_frozen_table_view->horizontalHeader(), &QHeaderView::sectionResized, this, update_divider);
  connect(_frozen_table_view->horizontalHeader(), &QHeaderView::geometriesChanged, this, update_divider);
  update_divider();

  connect(_frozen_table_view->horizontalHeader(), &QHeaderView::sectionClicked, _parent, &Table_window::clicked_header);
  connect(_frozen_table_view, &QTableView::clicked, _parent, &Table_window::clicked_table);
}

void Freeze_table_widget::resizeEvent(QResizeEvent* event)
{
  QTableView::resizeEvent(event);
  _update_frozen_table_geometry();
}

void Freeze_table_widget::_update_frozen_table_geometry()
{
  _frozen_table_view->setGeometry(verticalHeader()->width() + frameWidth(),
                                  frameWidth(),
                                  columnWidth(0) + columnWidth(1) - 0,
                                  viewport()->height() + horizontalHeader()->height());
}

void Freeze_table_widget::_style_table(QTableView* t, int width_1, int width_2)
{
  t->setEditTriggers(QAbstractItemView::NoEditTriggers);
  t->setShowGrid(false);
  t->setAlternatingRowColors(true);
  t->setStyleSheet("QTableView {alternate-background-color: #f0f0f0; background-color: #ffffff; color: black;}");
  t->setFocusPolicy(Qt::NoFocus);
  t->setSelectionMode(QAbstractItemView::NoSelection);
  t->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  t->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  t->verticalHeader()->setDefaultSectionSize(t->fontMetrics().boundingRect("0").height());
  t->verticalHeader()->hide();
  t->horizontalHeader()->setDefaultSectionSize(width_1);
  t->setColumnWidth(1, width_2);
}
