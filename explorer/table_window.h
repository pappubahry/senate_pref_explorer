#ifndef TABLE_WINDOW_H
#define TABLE_WINDOW_H

#include "freeze_table_widget.h"
#include <QCoreApplication>
#include <QStandardItemModel>
#include <QWidget>

// I feel embarrassed about this hack to avoid a clash of
// constructor definitions.
struct Table_tag_standard
{
};
struct Table_tag_custom
{
};
struct Table_tag_divisions
{
};
struct Table_tag_party_abbrevs
{
};

class Table_window : public QWidget
{
  Q_OBJECT
public:
  // Constructors for standard cross tables:
  Table_window(Table_tag_standard,
               QVector<double> base,
               QStringList groups,
               QVector<int> ignore_groups,
               QVector<QVector<double>> table_data,
               QString title,
               QWidget* parent);
  Table_window(Table_tag_standard,
               QVector<int> base,
               QStringList groups,
               QVector<int> ignore_groups,
               QVector<QVector<int>> table_data,
               QString title,
               QWidget* parent);

  // Constructors for custom cross tables:
  Table_window(Table_tag_custom,
               QVector<double> base,
               QStringList row_headers,
               QStringList col_headers,
               QVector<QVector<double>> table_data,
               QString title,
               QWidget* parent);
  Table_window(Table_tag_custom,
               QVector<int> base,
               QStringList row_headers,
               QStringList col_headers,
               QVector<QVector<int>> table_data,
               QString title,
               QWidget* parent);

  // Constructors for divisions cross tables:
  Table_window(Table_tag_divisions,
               QVector<double> base,
               QStringList divisions,
               QStringList groups,
               QVector<QVector<double>> table_data,
               QString title,
               QWidget* parent);
  Table_window(Table_tag_divisions,
               QVector<int> base,
               QStringList divisions,
               QStringList groups,
               QVector<QVector<int>> table_data,
               QString title,
               QWidget* parent);

  // Constructor for party abbreviations table:
  Table_window(Table_tag_party_abbrevs, QStringList short_names, QStringList full_names, QWidget* parent);

  ~Table_window();

signals:

public slots:
  void clicked_header(int i);
  void clicked_table(const QModelIndex& index);

private slots:
  void sort_columns_by_row_order();
  void sort_rows_by_column_order();
  void sort_columns();
  void copy_table();
  void export_table();

private:
  bool _test_identical_axes();
  void _copy_ignore_groups(QVector<int> ignore_groups);
  void _setup_table_data_int(QVector<int>& base, QVector<QVector<int>>& t);
  void _setup_table_data_double(QVector<double>& base, QVector<QVector<double>>& t);
  QString _get_max_width_string(QVector<QVector<int>>& t);
  void _setup_layout(QStringList& variable_widths, const QString& regular_width);
  void _setup_model();
  void _setup_model_divisions();
  void _set_table_cells();
  void _sort_by_row(int i);
  void _sort_by_column(int i);
  QString _get_export_line(int i, QString separator);

  Freeze_table_widget* _table;
  QStandardItemModel* _model;
  QVector<double> _base;
  QVector<int> _ignore_groups;
  QStringList _groups;
  QStringList _divisions;
  QStringList _short_names;
  QStringList _full_names;
  QVector<QVector<double>> _table_data;
  QString _title;
  QString _latest_out_path = QCoreApplication::applicationDirPath();
  bool _is_integer;
  bool _division_cross_table = false;
  bool _standard_cross_table = false;
  bool _custom_cross_table   = false;
  QStringList _custom_row_headers;
  QStringList _custom_col_headers;
  bool _groups_table = false;
  bool _common_axes  = false;
  bool _sort_row_desc;
  bool _sort_col_desc;
  int _sort_row;
  int _sort_col;
  QVector<int> _sort_indices_rows;
  QVector<int> _sort_indices_cols;
};

#endif // TABLE_WINDOW_H
