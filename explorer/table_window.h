#ifndef TABLE_WINDOW_H
#define TABLE_WINDOW_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QCoreApplication>
#include <freezetablewidget.h>


class Table_window : public QWidget
{
  Q_OBJECT
public:
  // Constructors for standard cross tables:
  Table_window(QVector<double> base, QStringList groups, QVector<int> ignore_groups, QVector<QVector<double>> table_data, QString title, QWidget *parent);
  Table_window(QVector<long> base, QStringList groups, QVector<int> ignore_groups, QVector<QVector<long>> table_data, QString title, QWidget *parent);
  
  // Constructors for divisions cross tables:
  Table_window(QVector<double> base, QStringList divisions, QStringList groups, QVector<QVector<double>> table_data, QString title, QWidget *parent);
  Table_window(QVector<long> base, QStringList divisions, QStringList groups, QVector<QVector<long>> table_data, QString title, QWidget *parent);
  
  // Constructor for party abbreviations table:
  Table_window(QStringList short_names, QStringList full_names, QWidget *parent);
  
  ~Table_window();
  
signals:
  
public slots:
  void clicked_header(int i);
  void clicked_table(const QModelIndex &index);
  
private slots:
  void sort_columns_by_row_order();
  void sort_rows_by_column_order();
  void copy_table();
  void export_table();
  
private:
  FreezeTableWidget *table;
  QStandardItemModel *_model;
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
  bool _groups_table = false;
  bool _sort_row_desc;
  bool _sort_col_desc;
  int _sort_row;
  int _sort_col;
  QVector<int> _sort_indices_rows;
  QVector<int> _sort_indices_cols;
  void copy_ignore_groups(QVector<int> ignore_groups);
  void setup_table_data_long(QVector<long> &base, QVector<QVector<long>> &t, QStringList &groups);
  void setup_table_data_double(QVector<double> &base, QVector<QVector<double>> &t, QStringList &groups);
  QString get_max_width_string(QVector<QVector<long>> &t);
  void setup_layout(Table_window *w, QStringList variable_widths, QString regular_width);
  void setup_model();
  void setup_model_divisions();
  void set_table_cells();
  void sort_by_row(int i);
  void sort_by_column(int i);
  QString get_export_line(int i, QString separator);
};

#endif // TABLE_WINDOW_H
