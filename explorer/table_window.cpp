#include "table_window.h"
#include "clickable_label.h"
#include "freeze_table_widget.h"
#include "table_view.h"
#include <math.h>
#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSpacerItem>
#include <QStandardItemModel>
#include <QTableView>

using Qt::endl;

// Constructor for standard cross table (double)
Table_window::Table_window(Table_tag_standard,
                           QVector<double> base,
                           QStringList groups,
                           QVector<int> ignore_groups,
                           QVector<QVector<double>> table_data,
                           QString title,
                           QWidget* parent)
  : QWidget(parent, Qt::Window)
  , _groups(groups)
{
  _is_integer           = false;
  _standard_cross_table = true;
  _common_axes          = true;

  _title = title;
  _copy_ignore_groups(ignore_groups);

  const QString regular_width = groups.at(0).contains("_") ? "AAAA_99" : "999.99";

  _setup_table_data_double(base, table_data);
  _setup_model();
  _setup_layout(_groups, regular_width);
}

// Constructor for standard cross table (int)
Table_window::Table_window(Table_tag_standard,
                           QVector<int> base,
                           QStringList groups,
                           QVector<int> ignore_groups,
                           QVector<QVector<int>> table_data,
                           QString title,
                           QWidget* parent)
  : QWidget(parent, Qt::Window)
  , _groups(groups)
{
  _is_integer           = true;
  _standard_cross_table = true;
  _common_axes          = true;

  _title = title;
  _copy_ignore_groups(ignore_groups);

  const QString regular_width = _get_max_width_string(table_data);

  _setup_table_data_int(base, table_data);
  _setup_model();
  _setup_layout(_groups, regular_width);
}

// Constructor for custom cross table (double)
Table_window::Table_window(Table_tag_custom,
                           QVector<double> base,
                           QStringList row_headers,
                           QStringList col_headers,
                           QVector<QVector<double>> table_data,
                           QString title,
                           QWidget* parent)
  : QWidget(parent, Qt::Window)
{
  _is_integer         = false;
  _custom_cross_table = true;
  _custom_row_headers = row_headers;
  _custom_col_headers = col_headers;
  _title              = title;
  _common_axes        = _test_identical_axes();

  const QString regular_width = col_headers.at(0).contains("_") ? "AAAA_99" : "999.99";

  _setup_table_data_double(base, table_data);
  _setup_model();
  _setup_layout(row_headers, regular_width);
}

// Constructor for custom cross table (int)
Table_window::Table_window(Table_tag_custom,
                           QVector<int> base,
                           QStringList row_headers,
                           QStringList col_headers,
                           QVector<QVector<int>> table_data,
                           QString title,
                           QWidget* parent)
  : QWidget(parent, Qt::Window)
{
  _is_integer         = true;
  _custom_cross_table = true;
  _custom_row_headers = row_headers;
  _custom_col_headers = col_headers;
  _title              = title;
  _common_axes        = _test_identical_axes();

  const QString regular_width = _get_max_width_string(table_data);

  _setup_table_data_int(base, table_data);
  _setup_model();
  _setup_layout(row_headers, regular_width);
}

// Constructor for divisions (int)
Table_window::Table_window(
  Table_tag_divisions, QVector<int> base, QStringList divisions, QStringList groups, QVector<QVector<int>> table_data, QString title, QWidget* parent)
  : QWidget(parent, Qt::Window)
  , _groups(groups)
{
  _is_integer           = true;
  _division_cross_table = true;

  _title = title;

  const QString regular_width = _get_max_width_string(table_data);

  for (int i = 0; i < divisions.length(); i++)
  {
    _divisions.append(divisions.at(i));
  }

  _setup_table_data_int(base, table_data);
  _setup_model_divisions();
  _setup_layout(_divisions, regular_width);
}

// Constructor for divisions (double)
Table_window::Table_window(Table_tag_divisions,
                           QVector<double> base,
                           QStringList divisions,
                           QStringList groups,
                           QVector<QVector<double>> table_data,
                           QString title,
                           QWidget* parent)
  : QWidget(parent, Qt::Window)
  , _groups(groups)
{
  _is_integer           = false;
  _division_cross_table = true;

  _title = title;

  for (int i = 0; i < divisions.length(); i++)
  {
    _divisions.append(divisions.at(i));
  }

  const QString regular_width = groups.at(0).contains("_") ? "AAAA_99" : "999.99";

  _setup_table_data_double(base, table_data);
  _setup_model_divisions();
  _setup_layout(_divisions, regular_width);
}

// Constructor for two-column group abbreviations table
Table_window::Table_window(Table_tag_party_abbrevs, QStringList short_names, QStringList full_names, QWidget* parent)
  : QWidget(parent, Qt::Window)
{
  _groups_table = true;

  int num_rows = short_names.length();
  int num_cols = 2;

  _model = new QStandardItemModel();
  _model->setRowCount(num_rows);
  _model->setColumnCount(num_cols);

  _sort_indices_rows = QVector<int>();

  for (int i = 0; i < num_rows; i++)
  {
    _short_names.append(short_names.at(i));
    _full_names.append(full_names.at(i));
    _full_names[i].replace("\n", " ");
    _model->setItem(i, 0, new QStandardItem());
    _model->setItem(i, 1, new QStandardItem());

    _sort_indices_rows.append(i);
  }

  QStringList headers;
  headers.append("Short");
  headers.append("Full");
  _model->setHorizontalHeaderLabels(headers);

  // And the sort indices
  _sort_indices_cols = QVector<int>();
  _sort_indices_cols.append(0);
  _sort_indices_cols.append(1);

  _sort_col      = 0;
  _sort_col_desc = false;
  _sort_row      = -1;
  _sort_row_desc = false;

  const QString regular_width("AAAA_99");
  _setup_layout(_full_names, regular_width);

  _sort_by_column(0);

  _model->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft);
  _model->horizontalHeaderItem(1)->setTextAlignment(Qt::AlignLeft);
}

void Table_window::_copy_ignore_groups(QVector<int> ignore_groups)
{
  for (int i = 0; i < ignore_groups.length(); i++)
  {
    _ignore_groups.append(ignore_groups.at(i));
  }
}

void Table_window::_setup_table_data_int(QVector<int>& base, QVector<QVector<int>>& t)
{
  const int num_rows = t.length();
  const int num_cols = t.at(0).length();

  _model = new QStandardItemModel();
  _model->setRowCount(num_rows);
  _model->setColumnCount(num_cols + 2);

  for (int i = 0; i < num_rows; i++)
  {
    _base.append(static_cast<double>(base.at(i)));

    _table_data.append(QVector<double>());

    for (int j = 0; j < num_cols; j++)
    {
      _table_data[i].append(static_cast<double>(t.at(i).at(j)));
    }
  }
}

void Table_window::_setup_table_data_double(QVector<double>& base, QVector<QVector<double>>& t)
{
  const int num_rows = t.length();
  const int num_cols = t.at(0).length();

  _model = new QStandardItemModel();
  _model->setRowCount(num_rows);
  _model->setColumnCount(num_cols + 2);

  for (int i = 0; i < num_rows; i++)
  {
    _base.append(base.at(i));

    _table_data.append(QVector<double>());

    for (int j = 0; j < num_cols; j++)
    {
      _table_data[i].append(t.at(i).at(j));
    }
  }
}

QString Table_window::_get_max_width_string(QVector<QVector<int>>& t)
{
  int max_val = 1;
  for (int i = 0; i < t.length(); i++)
  {
    for (int j = 0; j < t.at(i).length(); j++)
    {
      max_val = qMax(max_val, qAbs(t.at(i).at(j)));
    }
  }

  const int ord = static_cast<int>(floor(log10(max_val)));
  QString regular_width;
  regular_width.fill('9', qMax(ord + 3, 6));

  return regular_width;
}

Table_window::~Table_window() {}

void Table_window::_setup_layout(QStringList& variable_widths, const QString& regular_width)
{
  QVBoxLayout* layout         = new QVBoxLayout();
  QHBoxLayout* layout_buttons = new QHBoxLayout();

  QString export_label("Export CSV...");

  QPushButton* button_copy    = new QPushButton("Copy");
  QPushButton* button_export  = new QPushButton(export_label);
  QSpacerItem* spacer_buttons = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);

  const int button_width = button_copy->fontMetrics().boundingRect(export_label).width() + 15;

  button_copy->setMaximumWidth(button_width);
  button_export->setMaximumWidth(button_width);

  layout_buttons->addWidget(button_copy);
  layout_buttons->addWidget(button_export);
  layout_buttons->addItem(spacer_buttons);

  layout->addLayout(layout_buttons);

  if (_common_axes)
  {
    Clickable_label* label_sort_rows_to_column = new Clickable_label();
    label_sort_rows_to_column->setText("<i>Sort rows to column order</i>");
    label_sort_rows_to_column->setCursor(Qt::PointingHandCursor);

    Clickable_label* label_sort_columns_to_row = new Clickable_label();
    label_sort_columns_to_row->setText("<i>Sort columns to row order</i>");
    label_sort_columns_to_row->setCursor(Qt::PointingHandCursor);

    layout->addWidget(label_sort_rows_to_column);
    layout->addWidget(label_sort_columns_to_row);

    connect(label_sort_columns_to_row, &Clickable_label::clicked, this, &Table_window::sort_columns_by_row_order);
    connect(label_sort_rows_to_column, &Clickable_label::clicked, this, &Table_window::sort_rows_by_column_order);
  }
  else if (!_groups_table)
  {
    Clickable_label* label_sort_columns = new Clickable_label();
    label_sort_columns->setText("<i>Sort columns</i>");
    label_sort_columns->setCursor(Qt::PointingHandCursor);
    layout->addWidget(label_sort_columns);
    connect(label_sort_columns, &Clickable_label::clicked, this, &Table_window::sort_columns);
  }

  _title.replace("  ", " ");
  QLabel* label_title = new QLabel(_title);
  label_title->setMaximumWidth(300);
  label_title->setWordWrap(true);
  layout->addWidget(label_title);

  if (!_groups_table)
  {
    variable_widths << "Group";
    _table = new Freeze_table_widget(_model, variable_widths, regular_width, _division_cross_table, this);
    if (_base.at(0) < 0.)
    {
      _table->hide_column(0);
    }

    layout->addWidget(_table);

    QHeaderView* table_header = _table->horizontalHeader();

    connect(_table, &Freeze_table_widget::clicked, this, &Table_window::clicked_table);
    connect(table_header, &QHeaderView::sectionClicked, this, &Table_window::clicked_header);
  }
  else
  {
    Table_view* groups_table = new Table_view();
    groups_table->setModel(_model);

    const int width_1 = groups_table->fontMetrics().boundingRect(regular_width).width() + 5;

    int width_2 = 10;
    for (int i = 0; i < variable_widths.length(); i++)
    {
      width_2 = qMax(width_2, groups_table->fontMetrics().boundingRect(variable_widths.at(i)).width());
    }

    width_2 += 5;

    groups_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    groups_table->setShowGrid(false);
    groups_table->setAlternatingRowColors(true);
    groups_table->setStyleSheet("QTableView {alternate-background-color: #f0f0f0; background-color: #ffffff; color: black; }");
    groups_table->setFocusPolicy(Qt::NoFocus);
    groups_table->setSelectionMode(QAbstractItemView::NoSelection);
    groups_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    groups_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    groups_table->verticalHeader()->setDefaultSectionSize(groups_table->fontMetrics().boundingRect("0").height());
    groups_table->verticalHeader()->hide();
    groups_table->horizontalHeader()->setDefaultSectionSize(width_1);
    groups_table->setColumnWidth(1, width_2);

    layout->addWidget(groups_table);

    QHeaderView* table_header = groups_table->horizontalHeader();

    connect(table_header, &QHeaderView::sectionClicked, this, &Table_window::clicked_header);
  }

  setLayout(layout);

  connect(button_copy, &QPushButton::clicked, this, &Table_window::copy_table);
  connect(button_export, &QPushButton::clicked, this, &Table_window::export_table);
}

void Table_window::_setup_model()
{
  const int num_rows = _table_data.length();
  const int num_cols = _table_data.at(0).length() + 2;

  for (int i = 0; i < num_rows; i++)
  {
    for (int j = 0; j < num_cols; j++)
    {
      _model->setItem(i, j, new QStandardItem());
    }
  }

  QStringList headers;
  for (int i = 0; i < num_cols; i++)
  {
    headers.append("");
  }
  _model->setHorizontalHeaderLabels(headers);

  // And the sort indices:

  _sort_indices_rows = QVector<int>();

  for (int i = 0; i < num_rows; i++)
  {
    _sort_indices_rows.append(i);
  }

  // We don't sort the first two columns
  for (int i = 0; i < num_cols - 2; i++)
  {
    _sort_indices_cols.append(i);
  }

  _sort_row      = -1;
  _sort_row_desc = true;
  _sort_col      = 0;
  _sort_col_desc = true;

  _sort_by_column(0);
  sort_columns_by_row_order();
}

void Table_window::_setup_model_divisions()
{
  const int num_rows = _table_data.length();
  const int num_cols = _table_data.at(0).length() + 2;

  for (int i = 0; i < num_rows; i++)
  {
    for (int j = 0; j < num_cols; j++)
    {
      _model->setItem(i, j, new QStandardItem());
    }
  }

  QStringList headers;
  for (int i = 0; i < num_cols; i++)
  {
    headers.append("");
  }
  _model->setHorizontalHeaderLabels(headers);

  // And the sort indices:

  _sort_indices_rows = QVector<int>();

  for (int i = 0; i < num_rows; i++)
  {
    _sort_indices_rows.append(i);
  }

  // We don't sort the first two columns
  for (int i = 0; i < num_cols - 2; i++)
  {
    _sort_indices_cols.append(i);
  }

  _sort_row      = -1;
  _sort_row_desc = true;
  _sort_col      = 0;
  _sort_col_desc = true;

  _set_table_cells();
}

void Table_window::_set_table_cells()
{
  if (!_groups_table)
  {
    // First column base percentage
    // Second column group/division/number name (string)
    // Remainder of table is num_rows x num_cols

    const int num_rows   = _sort_indices_rows.length();
    const int num_cols   = _sort_indices_cols.length();
    const int dec_places = _is_integer ? 0 : 2;

    QFont font_bold;
    QFont font_normal;
    font_bold.setBold(true);

    bool bold_row = false;

    for (int i = 0; i < num_rows; i++)
    {
      const int idx_i = _sort_indices_rows.at(i);
      _model->horizontalHeaderItem(0)->setText("Base");
      _model->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      _model->item(i, 0)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      _model->item(i, 0)->setText(QString::number(_base.at(idx_i), 'f', dec_places));

      if (_standard_cross_table)
      {
        if (i == 0)
        {
          if (_groups.at(i).indexOf("_") >= 0)
          {
            _model->horizontalHeaderItem(1)->setText("Cand");
          }
          else
          {
            _model->horizontalHeaderItem(1)->setText("Group");
          }

          _model->horizontalHeaderItem(1)->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        }

        _model->item(i, 1)->setText(_groups.at(idx_i));
        _model->item(i, 1)->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
      }
      else if (_custom_cross_table)
      {
        if (i == 0)
        {
          bool is_number;
          const QString first_header = _custom_row_headers.at(0);
          first_header.toInt(&is_number);
          const QString header = is_number ? "Number" : first_header.indexOf("_") >= 0 ? "Cand" : "Group";
          _model->horizontalHeaderItem(1)->setText(header);
          _model->horizontalHeaderItem(1)->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        }
        _model->item(i, 1)->setText(_custom_row_headers.at(idx_i));
        _model->item(i, 1)->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
      }
      else
      {
        // Divisions cross table
        if (i == 0)
        {
          _model->horizontalHeaderItem(1)->setText("Division");
          _model->horizontalHeaderItem(1)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        }

        _model->item(i, 1)->setText(_divisions.at(idx_i));
        _model->item(i, 1)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        if (idx_i == num_rows - 1)
        {
          bold_row = true;
          _model->item(i, 1)->setFont(font_bold);
          _model->item(i, 0)->setFont(font_bold);
        }
        else
        {
          bold_row = false;
          _model->item(i, 1)->setFont(font_normal);
          _model->item(i, 0)->setFont(font_normal);
        }
      }

      for (int j = 0; j < num_cols; j++)
      {
        const int idx_j = _sort_indices_cols.at(j);

        if (i == 0)
        {
          const QString header = (_custom_cross_table ? _custom_col_headers : _groups).at(idx_j);
          _model->horizontalHeaderItem(j + 2)->setText(header);
          _model->horizontalHeaderItem(j + 2)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }

        if (_standard_cross_table
            && ((idx_i == idx_j) || (_base.at(idx_i) < 1.e-10) || (_ignore_groups.indexOf(idx_i) >= 0) || (_ignore_groups.indexOf(idx_j) >= 0)))
        {
          _model->item(i, j + 2)->setText("");
        }
        else
        {
          _model->item(i, j + 2)->setText(QString::number(_table_data.at(idx_i).at(idx_j), 'f', dec_places));
        }
        _model->item(i, j + 2)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        _model->item(i, j + 2)->setFont(bold_row ? font_bold : font_normal);
      }
    }
  }
  else if (_groups_table)
  {
    const int num_rows = _sort_indices_rows.length();

    for (int i = 0; i < num_rows; i++)
    {
      const int idx = _sort_indices_rows.at(i);
      _model->item(i, 0)->setText(_short_names.at(idx));
      _model->item(i, 1)->setText(_full_names.at(idx));
    }
  }
}

void Table_window::clicked_table(const QModelIndex& index)
{
  if (!_groups_table && (index.column() < 2))
  {
    const int i    = index.row();
    _sort_row_desc = (_sort_row == i) ? !_sort_row_desc : true;
    _sort_row      = i;
    _sort_by_row(i);
  }
}

void Table_window::clicked_header(int i)
{
  _sort_col_desc = (_sort_col == i) ? !_sort_col_desc : !_groups_table;
  _sort_col      = i;
  _sort_by_column(i);
}

void Table_window::_sort_by_row(int i)
{
  if (_groups_table)
  {
    return;
  }
  const int idx_i = _sort_indices_rows.at(i);

  std::sort(_sort_indices_cols.begin(),
            _sort_indices_cols.end(),
            [&](int a, int b) -> bool
            {
              if (a == b)
              {
                return false;
              }
              if (qAbs(_table_data.at(idx_i).at(a) - _table_data.at(idx_i).at(b)) < 1.e-10)
              {
                return (a < b) != _sort_row_desc;
              }
              else
              {
                return (_table_data.at(idx_i).at(a) < _table_data.at(idx_i).at(b)) != _sort_row_desc;
              }
            });

  _set_table_cells();
}

void Table_window::_sort_by_column(int i)
{
  if (_groups_table)
  {
    std::sort(_sort_indices_rows.begin(),
              _sort_indices_rows.end(),
              [&](int a, int b) -> bool
              {
                if (a == b)
                {
                  return false;
                }
                if (i == 0)
                {
                  return (_short_names.at(a).compare(_short_names.at(b)) < 0) != _sort_col_desc;
                }
                else
                {
                  return (_full_names.at(a).compare(_full_names.at(b)) < 0) != _sort_col_desc;
                }
              });
  }
  else
  {
    if (i == 0)
    {
      std::sort(_sort_indices_rows.begin(),
                _sort_indices_rows.end(),
                [&](int a, int b) -> bool
                {
                  if (a == b)
                  {
                    return false;
                  }
                  if (qAbs(_base.at(a) - _base.at(b)) < 1.e-10)
                  {
                    return (a < b) != _sort_col_desc;
                  }
                  else
                  {
                    return (_base.at(a) < _base.at(b)) != _sort_col_desc;
                  }
                });
    }
    else if (i == 1)
    {
      // No reverse sort for the alphabetical orderings.
      std::sort(_sort_indices_rows.begin(), _sort_indices_rows.end(), [&](int a, int b) -> bool { return a < b; });
    }
    else
    {
      int idx_i = _sort_indices_cols.at(i - 2);

      std::sort(_sort_indices_rows.begin(),
                _sort_indices_rows.end(),
                [&](int a, int b) -> bool
                {
                  if (a == b)
                  {
                    return false;
                  }
                  if (qAbs(_table_data.at(a).at(idx_i) - _table_data.at(b).at(idx_i)) < 1.e-10)
                  {
                    return (a < b) != _sort_col_desc;
                  }
                  else
                  {
                    return (_table_data.at(a).at(idx_i) < _table_data.at(b).at(idx_i)) != _sort_col_desc;
                  }
                });
    }
  }

  _set_table_cells();
}

void Table_window::sort_columns_by_row_order()
{
  if (!_common_axes)
  {
    return;
  }

  const int n = _sort_indices_cols.length();

  for (int i = 0; i < n; i++)
  {
    _sort_indices_cols[i] = _sort_indices_rows.at(i);
  }

  _set_table_cells();
}

void Table_window::sort_rows_by_column_order()
{
  if (!_common_axes)
  {
    return;
  }

  const int n = _sort_indices_rows.length();

  for (int i = 0; i < n; i++)
  {
    _sort_indices_rows[i] = _sort_indices_cols.at(i);
  }

  _set_table_cells();
}

void Table_window::sort_columns()
{
  const int n = _sort_indices_cols.length();
  // Check if already sorted
  bool already_sorted = true;
  for (int i = 0; i < n; ++i)
  {
    if (_sort_indices_cols.at(i) != i)
    {
      already_sorted = false;
      break;
    }
  }

  for (int i = 0; i < n; ++i)
  {
    _sort_indices_cols[i] = already_sorted ? n - i - 1 : i;
  }

  _set_table_cells();
}

QString Table_window::_get_export_line(int i, QString separator)
{
  QString text("");
  QString sep("");

  QString quotes = _groups_table ? "\"" : "";

  if (i == -1)
  {
    // Headers
    for (int j = 0; j < _model->columnCount(); j++)
    {
      text = QString("%1%2%3%4%3").arg(text, sep, quotes, _model->horizontalHeaderItem(j)->text());
      if (j == 0)
      {
        sep = separator;
      }
    }

    return text;
  }
  else
  {
    for (int j = 0; j < _model->columnCount(); j++)
    {
      text = QString("%1%2%3%4%3").arg(text, sep, quotes, _model->item(i, j)->text());
      if (j == 0)
      {
        sep = separator;
      }
    }

    return text;
  }
}

void Table_window::copy_table()
{
  QStringList title_lines = _title.split("\n");
  QString text;
  for (QString& line : title_lines)
  {
    text += QString("\"%1\"\n").arg(line);
  }

  QString newline("");

  for (int i = -1; i < _model->rowCount(); i++)
  {
    text = QString("%1%2%3").arg(text, newline, _get_export_line(i, "\t"));
    if (i == -1)
    {
      newline = "\n";
    }
  }

  QApplication::clipboard()->setText(text);
}

void Table_window::export_table()
{
  // Read the most recent directory a CSV file was saved to.
  const QString last_path_file_name = QString("%1/last_csv_dir.txt").arg(QCoreApplication::applicationDirPath());

  QFileInfo check_exists(last_path_file_name);

  if (check_exists.exists())
  {
    QFile last_path_file(last_path_file_name);

    if (last_path_file.open(QIODevice::ReadOnly))
    {
      QTextStream in(&last_path_file);
      const QString last_path = in.readLine();

      if (QDir(last_path).exists())
      {
        _latest_out_path = last_path;
      }

      last_path_file.close();
    }
  }

  const QString out_file_name = QFileDialog::getSaveFileName(this, "Save CSV", _latest_out_path, "*.csv");

  if (out_file_name == "")
  {
    return;
  }

  QFile out_file(out_file_name);

  if (out_file.open(QIODevice::WriteOnly))
  {
    QTextStream out(&out_file);

    QStringList title_lines = _title.split("\n");
    QString text;
    for (QString& line : title_lines)
    {
      text += QString("\"%1\"\n").arg(line);
    }
    out << text;

    for (int i = -1; i < _model->rowCount(); i++)
    {
      out << _get_export_line(i, ",") << endl;
    }

    out_file.close();
  }
  else
  {
    QMessageBox msg_box;
    msg_box.setText("Error: couldn't open file.");
    msg_box.exec();
    return;
  }

  // Update the most recent CSV path:
  QFileInfo info(out_file_name);
  _latest_out_path = info.absolutePath();

  QFile last_path_file(last_path_file_name);

  if (last_path_file.open(QIODevice::WriteOnly))
  {
    QTextStream out(&last_path_file);
    out << _latest_out_path << endl;
    last_path_file.close();
  }
}

bool Table_window::_test_identical_axes()
{
  const int n_rows = _custom_row_headers.size();
  if (n_rows != _custom_col_headers.size())
  {
    return false;
  }

  for (int i = 0; i < n_rows; ++i)
  {
    if (_custom_row_headers.at(i) != _custom_col_headers.at(i))
    {
      return false;
    }
  }
  return true;
}
