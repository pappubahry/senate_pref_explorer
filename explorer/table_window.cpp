#include "table_window.h"
#include <math.h>
#include <QApplication>
#include <QMessageBox>
#include <QClipboard>
#include <QFileDialog>
#include <QLayout>
#include <QBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTableView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QScrollBar>
#include <QSpacerItem>
#include <freezetablewidget.h>
#include <main_widget.h>

// Constructor for standard cross table (double)
Table_window::Table_window(QVector<double> base,
                           QStringList groups,
                           QVector<int> ignore_groups,
                           QVector<QVector<double>> table_data,
                           QString title,
                           QWidget *parent)
  : QWidget(parent, Qt::Window)
{
  _is_integer = false;
  _standard_cross_table = true;
  
  _title = title;
  copy_ignore_groups(ignore_groups);
  
  QString regular_width = groups.at(0).contains("_") ? "AAAA_99" : "999.99";
  
  setup_table_data_double(base, table_data, groups);
  setup_model();
  setup_layout(this, _groups, regular_width);
}

// Constructor for standard cross table (long)
Table_window::Table_window(QVector<long> base,
                           QStringList groups,
                           QVector<int> ignore_groups, 
                           QVector<QVector<long>> table_data,
                           QString title,
                           QWidget *parent)
  : QWidget(parent, Qt::Window)
{
  _is_integer = true;
  _standard_cross_table = true;
  
  _title = title;
  copy_ignore_groups(ignore_groups);
  
  QString regular_width = get_max_width_string(table_data);
  
  setup_table_data_long(base, table_data, groups);
  setup_model();
  setup_layout(this, _groups, regular_width);
}

// Constructor for divisions (long)
Table_window::Table_window(QVector<long> base,
                           QStringList divisions,
                           QStringList groups,
                           QVector<QVector<long>> table_data,
                           QString title,
                           QWidget *parent)
  : QWidget(parent, Qt::Window)
{
  _is_integer = true;
  _division_cross_table = true;
  
  _title = title;
  
  QString regular_width = get_max_width_string(table_data);
  
  for (int i = 0; i < divisions.length(); i++)
  {
    _divisions.append(divisions.at(i));
  }
  
  setup_table_data_long(base, table_data, groups);
  setup_model_divisions();
  setup_layout(this, _divisions, regular_width);
}

// Constructor for divisions (double)
Table_window::Table_window(QVector<double> base,
                           QStringList divisions,
                           QStringList groups,
                           QVector<QVector<double>> table_data,
                           QString title,
                           QWidget *parent)
  : QWidget(parent, Qt::Window)
{
  _is_integer = false;
  _division_cross_table = true;
  
  _title = title;
  
  for (int i = 0; i < divisions.length(); i++)
  {
    _divisions.append(divisions.at(i));
  }
  
  QString regular_width = groups.at(0).contains("_") ? "AAAA_99" : "999.99";
  
  setup_table_data_double(base, table_data, groups);
  setup_model_divisions();
  setup_layout(this, _divisions, regular_width);
}

// Constructor for two-column group abbreviations table
Table_window::Table_window(QStringList short_names, QStringList full_names, QWidget *parent)
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
  
  _sort_col = 0;
  _sort_col_desc = false;
  _sort_row = -1;
  _sort_row_desc = false;
  
  setup_layout(this, _full_names, "AAAA_99");
  
  sort_by_column(0);
  
  _model->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft);
  _model->horizontalHeaderItem(1)->setTextAlignment(Qt::AlignLeft);
}

void Table_window::copy_ignore_groups(QVector<int> ignore_groups)
{
  for (int i = 0; i < ignore_groups.length(); i++)
  {
    _ignore_groups.append(ignore_groups.at(i));
  }
}

void Table_window::setup_table_data_long(QVector<long> &base,
                                         QVector<QVector<long>> &t,
                                         QStringList &groups)
{
  int num_rows = t.length();
  int num_cols = t.at(0).length();
  
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
  
  for (int i = 0; i < num_cols; i++)
  {
    _groups.append(groups.at(i));
  }
}


void Table_window::setup_table_data_double(QVector<double> &base,
                                           QVector<QVector<double>> &t,
                                           QStringList &groups)
{
  int num_rows = t.length();
  int num_cols = t.at(0).length();
  
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
  
  for (int i = 0; i < num_cols; i++)
  {
    _groups.append(groups.at(i));
  }
}

QString Table_window::get_max_width_string(QVector<QVector<long>> &t)
{
  long max_val = 1;
  for (int i = 0; i < t.length(); i++)
  {
    for (int j = 0; j < t.at(i).length(); j++)
    {
      max_val = qMax(max_val, qAbs(t.at(i).at(j)));
    }
  }
  
  int ord = static_cast<int>(floor(log10(max_val)));
  QString regular_width;
  regular_width.fill('9', ord + 3);
  
  return regular_width;
}

Table_window::~Table_window()
{
}


void Table_window::setup_layout(Table_window *w, QStringList variable_widths, QString regular_width)
{
  QVBoxLayout *layout = new QVBoxLayout();
  QHBoxLayout *layout_buttons = new QHBoxLayout();
  
  QString export_label("Export CSV...");
  
  QPushButton *button_copy = new QPushButton("Copy");
  QPushButton *button_export = new QPushButton(export_label);
  QSpacerItem *spacer_buttons = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  
  int button_width = button_copy->fontMetrics().boundingRect(export_label).width() + 15;
  
  button_copy->setMaximumWidth(button_width);
  button_export->setMaximumWidth(button_width);
  
  layout_buttons->addWidget(button_copy);
  layout_buttons->addWidget(button_export);
  layout_buttons->addItem(spacer_buttons);
  
  layout->addLayout(layout_buttons);
  
  if (_standard_cross_table)
  {
    ClickableLabel *label_sort_rows_to_column = new ClickableLabel();
    label_sort_rows_to_column->setText("<i>Sort rows to column order</i>");
    label_sort_rows_to_column->setCursor(Qt::PointingHandCursor);
    
    ClickableLabel *label_sort_columns_to_row = new ClickableLabel();
    label_sort_columns_to_row->setText("<i>Sort columns to row order</i>");
    label_sort_columns_to_row->setCursor(Qt::PointingHandCursor);
    
    layout->addWidget(label_sort_rows_to_column);
    layout->addWidget(label_sort_columns_to_row);
    
    connect(label_sort_columns_to_row, &ClickableLabel::clicked, this, &Table_window::sort_columns_by_row_order);
    connect(label_sort_rows_to_column, &ClickableLabel::clicked, this, &Table_window::sort_rows_by_column_order);
  }
  
  _title.replace("  ", " ");
  QLabel *label_title = new QLabel(_title);
  label_title->setMaximumWidth(300);
  label_title->setWordWrap(true);
  layout->addWidget(label_title);
  
  if (_standard_cross_table || _division_cross_table)
  {
    variable_widths << "Group";
    table = new FreezeTableWidget(_model, variable_widths, regular_width, _division_cross_table, w);
    layout->addWidget(table);
    
    QHeaderView *table_header = table->horizontalHeader();
    
    connect(table,        &FreezeTableWidget::clicked,  this, &Table_window::clicked_table);
    connect(table_header, &QHeaderView::sectionClicked, this, &Table_window::clicked_header);
  }
  else
  {
    QTableView *groups_table = new QTableView();
    groups_table->setModel(_model);
    
    int width_1 = groups_table->fontMetrics().boundingRect(regular_width).width() + 5;
    
    int width_2 = 10;
    for (int i = 0; i < variable_widths.length(); i++)
    {
      width_2 = qMax(width_2, groups_table->fontMetrics().boundingRect(variable_widths.at(i)).width());
    }
    
    width_2 += 5;
    
    groups_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    groups_table->setShowGrid(false);
    groups_table->setAlternatingRowColors(true);
    groups_table->setStyleSheet("QTableView {alternate-background-color: #f0f0f0; background-color: #ffffff}");
    groups_table->setFocusPolicy(Qt::NoFocus);
    groups_table->setSelectionMode(QAbstractItemView::NoSelection);
    groups_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    groups_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    groups_table->verticalHeader()->setDefaultSectionSize(groups_table->fontMetrics().boundingRect("0").height());
    groups_table->verticalHeader()->hide();
    groups_table->horizontalHeader()->setDefaultSectionSize(width_1);
    groups_table->setColumnWidth(1, width_2);
    
    layout->addWidget(groups_table);
    
    QHeaderView *table_header = groups_table->horizontalHeader();
    
    connect(table_header, &QHeaderView::sectionClicked, this, &Table_window::clicked_header);
  }
  
  w->setLayout(layout);
  
  connect(button_copy,   &QPushButton::clicked, this, &Table_window::copy_table);
  connect(button_export, &QPushButton::clicked, this, &Table_window::export_table);
}

void Table_window::setup_model()
{
  int num_rows = _table_data.length();
  int num_cols = _table_data.length() + 2;
  
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
  
  _sort_row = -1;
  _sort_row_desc = true;
  _sort_col = 0;
  _sort_col_desc = true;
  
  sort_by_column(0);
  sort_columns_by_row_order();
}

void Table_window::setup_model_divisions()
{
  int num_rows = _table_data.length();
  int num_cols = _table_data.at(0).length() + 2;
  
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
  
  _sort_row = -1;
  _sort_row_desc = true;
  _sort_col = 0;
  _sort_col_desc = true;
  
  // *** should be unnnecessary: ***
  //sort_by_column(0);
  
  set_table_cells();
}

void Table_window::set_table_cells()
{
  if (_standard_cross_table || _division_cross_table)
  {
    // First column base percentage
    // Second column group/division name (string)
    // Remainder of table is num_rows x num_cols
    
    int num_rows = _sort_indices_rows.length();
    int num_cols = _sort_indices_cols.length();
    int dec_places = _is_integer ? 0 : 2;
    
    QFont font_bold;
    QFont font_normal;
    font_bold.setBold(true);
    
    bool bold_row = false;
    
    for (int i = 0; i < num_rows; i++)
    {
      int idx_i = _sort_indices_rows.at(i);
      _model->horizontalHeaderItem(0)->setText("Base");
      _model->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      _model->item(i, 0)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      _model->item(i, 0)->setText(QString("%1").arg(_base.at(idx_i), 0, 'f', dec_places));
      
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
          
          _model->horizontalHeaderItem(1)->setTextAlignment(Qt::AlignCenter);
        }
        
        _model->item(i, 1)->setText(QString("%1").arg(_groups.at(idx_i)));
        _model->item(i, 1)->setTextAlignment(Qt::AlignCenter);
      }
      else
      {
        // Divisions cross table
        if (i == 0)
        {
          _model->horizontalHeaderItem(1)->setText("Division");
          _model->horizontalHeaderItem(1)->setTextAlignment(Qt::AlignLeft);
        }
        
        _model->item(i, 1)->setText(QString("%1").arg(_divisions.at(idx_i)));
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
        int idx_j = _sort_indices_cols.at(j);
        
        if (i == 0)
        {
          _model->horizontalHeaderItem(j+2)->setText(_groups.at(idx_j));
          _model->horizontalHeaderItem(j+2)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
        
        if (_standard_cross_table && ((idx_i == idx_j)                     ||
                                      (_base.at(idx_i) < 1.e-10)           ||
                                      (_ignore_groups.indexOf(idx_i) >= 0) ||
                                      (_ignore_groups.indexOf(idx_j) >= 0)))
        {
          _model->item(i, j+2)->setText("");
        }
        else
        {
          _model->item(i, j+2)->setText(QString("%1").arg(_table_data.at(idx_i).at(idx_j), 0, 'f', dec_places));
        }
        _model->item(i, j+2)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        
        if (bold_row)
        {
          _model->item(i, j+2)->setFont(font_bold);
        }
        else
        {
          _model->item(i, j+2)->setFont(font_normal);
        }
      }
    }
  }
  else if (_groups_table)
  {
    int num_rows = _sort_indices_rows.length();
    
    for (int i = 0; i < num_rows; i++)
    {
      int idx = _sort_indices_rows.at(i);
      _model->item(i, 0)->setText(_short_names.at(idx));
      _model->item(i, 1)->setText(_full_names.at(idx));
    }
  }
}


void Table_window::clicked_table(const QModelIndex &index)
{
  if ((_standard_cross_table || _division_cross_table) && (index.column() < 2))
  {
    int i = index.row();
    _sort_row_desc = (_sort_row == i) ? !_sort_row_desc : true;
    _sort_row = i;
    sort_by_row(i);
  }
}

void Table_window::clicked_header(int i)
{
  _sort_col_desc = (_sort_col == i) ? !_sort_col_desc : !_groups_table;
  _sort_col = i;
  sort_by_column(i);
}

void Table_window::sort_by_row(int i)
{
  if (_groups_table) { return; }
  int idx_i = _sort_indices_rows.at(i);
  
  std::sort(_sort_indices_cols.begin(), _sort_indices_cols.end(),
            [&](int a, int b)->bool {
    
    
    
    
    
    
    if (qAbs(_table_data.at(idx_i).at(a) - _table_data.at(idx_i).at(b)) < 1.e-10)
    {
      
      
      
      return (a < b) != _sort_row_desc;
    }
    else
    {
      
      
      
      return (_table_data.at(idx_i).at(a) < _table_data.at(idx_i).at(b)) != _sort_row_desc;
    }
  });
  
  set_table_cells();
}

void Table_window::sort_by_column(int i)
{
  if (_groups_table) {
    std::sort(_sort_indices_rows.begin(), _sort_indices_rows.end(),
              [&](int a, int b)->bool {
      if (i == 0) { return (_short_names.at(a).compare(_short_names.at(b)) < 0) != _sort_col_desc; }
      else        { return (_full_names.at(a).compare(_full_names.at(b)) < 0)   != _sort_col_desc; }
    });
  }
  else
  {
    if (i == 0)
    {
      std::sort(_sort_indices_rows.begin(), _sort_indices_rows.end(),
                [&](int a, int b)->bool {
        
        // *** TEMPORARY TESTING, MUST DELETE ***
        QFile sort_file("sort_detailss.csv");
        
        sort_file.open(QIODevice::WriteOnly | QIODevice::Append);
        
        QTextStream out(&sort_file);
        out << QString("%1,%2,%3,%4")
               .arg(a)
               .arg(b)
               .arg(_base.at(a))
               .arg(_base.at(b));
        
        
        
        
        
        
        
        if (qAbs(_base.at(a) - _base.at(b)) < 1.e-10)
        {
          // *** DELETE ***
          out << ",Returning a < b" << endl;
          sort_file.close();
          
          return (a < b) != _sort_col_desc;
        }
        else
        {
          // *** DELETE ***
          out << ",Returning data a < data b" << endl;
          sort_file.close();
          
          
          return (_base.at(a) < _base.at(b)) != _sort_col_desc;
        }
      });
    }
    else if (i == 1)
    {
      // No reverse sort for the alphabetical orderings.
      std::sort(_sort_indices_rows.begin(), _sort_indices_rows.end(),
                [&](int a, int b)->bool {
        return a < b;
      });
    }
    else
    {
      int idx_i = _sort_indices_cols.at(i - 2);
      
      std::sort(_sort_indices_rows.begin(), _sort_indices_rows.end(),
                [&](int a, int b)->bool {
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
  
  set_table_cells();
}

void Table_window::sort_columns_by_row_order()
{
  if (!_standard_cross_table) { return; }
  
  int n = _sort_indices_cols.length();
  
  for (int i = 0; i < n; i++)
  {
    _sort_indices_cols[i] = _sort_indices_rows.at(i);
  }
  
  set_table_cells();
}

void Table_window::sort_rows_by_column_order()
{
  if (!_standard_cross_table) { return; }
  
  int n = _sort_indices_rows.length();
  
  for (int i = 0; i < n; i++)
  {
    _sort_indices_rows[i] = _sort_indices_cols.at(i);
  }
  
  set_table_cells();
}

QString Table_window::get_export_line(int i, QString separator)
{
  QString text("");
  QString sep("");
  
  QString quotes = _groups_table ? "\"" : "";
  
  if (i == -1)
  {
    // Headers
    for (int j = 0; j < _model->columnCount(); j++)
    {
      text = QString("%1%2%3%4%3")
          .arg(text).arg(sep).arg(quotes).arg(_model->horizontalHeaderItem(j)->text());
      if (j == 0) { sep = separator; }
    }
    
    return text;
  }
  else
  {
    for (int j = 0; j < _model->columnCount(); j++)
    {
      text = QString("%1%2%3%4%3")
          .arg(text).arg(sep).arg(quotes).arg(_model->item(i, j)->text());
      if (j == 0) { sep = separator; }
    }
    
    return text;
  }
}

void Table_window::copy_table()
{
  QString text = QString("\"%1\"\n").arg(_title);
  QString newline("");
  
  for (int i = -1; i < _model->rowCount(); i++)
  {
    text = QString("%1%2%3").arg(text).arg(newline).arg(get_export_line(i, "\t"));
    if (i == -1) { newline = "\n"; }
  }
  
  QApplication::clipboard()->setText(text);
}

void Table_window::export_table()
{
  // Read the most recent directory a CSV file was saved to.
  QString last_path_file_name = QString("%1/last_csv_dir.txt")
      .arg(QCoreApplication::applicationDirPath());
  
  QFileInfo check_exists(last_path_file_name);
  
  if (check_exists.exists())
  {
    QFile last_path_file(last_path_file_name);
    
    if (last_path_file.open(QIODevice::ReadOnly))
    {
      QTextStream in(&last_path_file);
      QString last_path = in.readLine();
      
      if (QDir(last_path).exists())
      {
        _latest_out_path = last_path;
      }
      
      last_path_file.close();
    }
  }
  
  QString out_file_name = QFileDialog::getSaveFileName(this,
                                                      "Save CSV",
                                                      _latest_out_path,
                                                      "*.csv");
  
  if (out_file_name == "") { return; }
  
  QFile out_file(out_file_name);
  
  if (out_file.open(QIODevice::WriteOnly))
  {
    QTextStream out(&out_file);
    
    out << QString("\"%1\"\n").arg(_title);
    
    for (int i = -1; i < _model->rowCount(); i++)
    {
      out << get_export_line(i, ",") << endl;
    }
    
    out_file.close();
  }
  else
  {
    QMessageBox msgBox;
    msgBox.setText("Error: couldn't open file.");
    msgBox.exec();
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
