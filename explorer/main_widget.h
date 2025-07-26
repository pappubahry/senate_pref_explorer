#ifndef MAIN_WIDGET_H
#define MAIN_WIDGET_H

#include "booth_model.h"
#include "clickable_label.h"
#include "custom_operation.h"
#include "map_container.h"
#include "polygon_model.h"
#include "table_view.h"
#include "table_window.h"
#include <QComboBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QQuickWidget>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QStandardItemModel>
#include <QTableView>
#include <QThread>
#include <QWidget>

struct Table_main_item
{
  // Data for one cell of the main table
  int group_id;
  int sorted_idx;
  QVector<double> percentages;
  QVector<int> votes;
};

struct Table_divisions_item
{
  // Data for one row of the divisions table
  // (vectors are length 1 for most table types, but
  // are of length n+1 for n-party-preferred)
  int division;
  QVector<int> votes;
  QVector<double> percentage;
  QVector<double> total_percentage;
};

struct Two_axis_click_cell
{
  int i;
  int j;
};

struct Arbitrary_axis_sort
{
  int i;
  bool is_descending;
};

struct Bounding_box
{
  double min_longitude;
  double max_longitude;
  double min_latitude;
  double max_latitude;
};

class Widget : public QWidget
{
  Q_OBJECT

public:
  Widget(QWidget* parent = nullptr);
  ~Widget();

  static const QString TABLE_POPUP;
  static const QString TABLE_MAIN;

  static const QString VALUE_VOTES;
  static const QString VALUE_PERCENTAGES;
  static const QString VALUE_TOTAL_PERCENTAGES;

  static const QString NO_FILTER;

  static const QString MAP_DIVISIONS;
  static const QString MAP_ELECTION_DAY_BOOTHS;
  static const QString MAP_PREPOLL_BOOTHS;

  int get_num_groups();
  QString get_abtl();
  int get_group_from_short(const QString& group) const;
  int get_cand_from_short(const QString& cand) const;

private slots:
  void _process_thread_sql_main_table(const QVector<QVector<int>>&);
  void _process_thread_sql_npp_table(const QVector<QVector<QVector<int>>>&);
  void _process_thread_sql_cross_table(const QVector<QVector<int>>&);
  void _process_thread_sql_custom_main_table(const QVector<int>&, const QVector<QVector<int>>&, const QVector<QVector<QVector<int>>>&);
  void _process_thread_sql_custom_popup_table(int, const QVector<int>&, const QVector<QVector<int>>&);
  void _process_thread_sql_custom_every_expr(int, const QVector<int>&);
  void _open_database();
  void _clicked_main_table(const QModelIndex& index);
  void _change_abtl(int i);
  void _change_table_type(int i);
  void _change_value_type(int i);
  void _change_division(int i);
  void _change_first_n_prefs(int i);
  void _change_later_prefs_fixed(int i);
  void _change_later_prefs_up_to(int i);
  void _change_pref_sources_min(int i);
  void _change_pref_sources_max(int i);
  void _change_map_type();
  void _change_map_booth_threshold(int i);
  void _update_map_scale_minmax();
  void _add_column_to_main_table();
  void _calculate_n_party_preferred();
  void _slot_calculate_custom();
  void _make_cross_table();
  void _make_divisions_cross_table();
  void _make_booths_cross_table();
  void _export_booths_table();
  void _show_abbreviations();
  void _show_help();
  void _show_map_help();
  void _toggle_sort();
  void _toggle_names();
  void _slot_click_main_table_header(int i);
  void _change_npp_sort(int i);
  void _change_divisions_sort(int i);
  void _copy_main_table();
  void _export_main_table();
  void _copy_divisions_table();
  void _export_divisions_table();
  void _reset_map_scale();
  void _zoom_to_state();
  void _zoom_to_capital();
  void _zoom_to_division();
  void _zoom_to_division(int div);
  void _copy_map();
  void _export_map();
  void _delayed_copy_map();
  void _delayed_export_map(const QString& file_name);

private:
  void _reset_spinboxes();
  void _load_database(const QString& db_file);
  void _set_table_groups();
  void _setup_main_table();
  void _reset_table();
  void _set_main_table_cells(int col);
  void _set_all_main_table_cells();
  void _set_all_main_table_cells_custom();
  void _set_main_table_row_height();
  void _make_main_table_row_headers(bool is_blank);
  void _do_sql_query_for_table(const QString& q, bool wide_table = false);
  void _set_divisions_table();
  void _init_main_table_custom(int n_main_rows, int n_rows, int n_main_cols, int n_cols);
  void _enable_division_export_buttons_custom();
  void _sort_table_column(int i);
  void _sort_main_table();
  void _sort_main_table_npp();
  void _sort_main_table_rows_custom(int clicked_col);
  void _sort_main_table_cols_custom(int clicked_row);
  void _sort_main_table_custom();
  void _set_clicked_cell_n_party(int i, int j);
  void _clear_main_table_data();
  void _reset_npp_sort();
  void _reset_divisions_sort();
  void _clear_divisions_table();
  void _set_divisions_table_title();
  void _fill_in_divisions_table();
  void _update_map_booths();
  void _sort_divisions_table_data();
  void _lock_main_interface();
  void _unlock_main_interface();
  void _show_calculation_time();
  void _init_cross_table_data(int n_rows, int n_cols = -1);
  void _init_cross_table_window(Table_window* w);
  void _write_sql_to_file(const QString& q);
  void _write_custom_operations_to_file();
  void _copy_model(QStandardItemModel* model, QString title = "");
  void _export_model(QStandardItemModel* model, QString title = "");
  void _update_output_path(const QString& file_name, const QString& file_type);
  void _set_title_from_divisions_table();
  void _spinbox_change();
  void _show_error_message(const QString& msg);
  QString _get_export_divisions_table_title();
  QString _get_export_line(QStandardItemModel* model, int i, const QString& separator);
  std::uint64_t _available_physical_memory();
  QStringList _queries_threaded(const QString& q, int& num_threads, bool one_thread = false);
  QStringList _queries_threaded_with_max(const QString& q, int& num_threads, int max_threads = -1);
  QString _get_table_type();
  QString _get_value_type();
  QString _get_groups_table();
  QString _get_short_group(int i);
  QString _get_output_path(const QString& file_type);
  QString _get_map_type();
  Custom_axis_definition _read_custom_axis_definition(QLineEdit* lineedit);
  void _parse_custom_query_line(QLineEdit* lineedit, const QString& lineedit_name, bool is_boolean, bool allow_row, bool allow_col,
    bool shortcut_row_already_forced, bool shortcut_col_already_forced, bool row_is_aggregate, bool col_is_aggregate, QString& sql,
    std::vector<Custom_operation>& row_ops, std::vector<Custom_operation>& col_ops, std::vector<Custom_operation>& cell_ops);
  void _calculate_custom_query();
  int _get_map_booth_threshold();
  int _get_width_from_text(const QString& t, QWidget* w, int buffer = 30);
  int _get_n_preferred();
  int _get_n_first_prefs();
  int _get_later_prefs_n_up_to();
  int _get_later_prefs_n_fixed();
  int _get_pref_sources_min();
  int _get_pref_sources_max();
  int _get_current_division();
  QColor _get_highlight_color();
  QColor _get_n_party_preferred_color();
  QColor _get_unfocused_text_color();
  QColor _get_focused_text_color();
  void _set_default_cell_style(int i, int j);
  void _fade_cell(int i, int j);
  void _highlight_cell(int i, int j);
  void _highlight_cell_n_party_preferred(int i, int j);
  void _unhighlight_cell(int i, int j);
  QPixmap _get_pixmap_for_map();
  double _longitude_to_x(double longitude, double center_longitude, double zoom, int size);
  double _latitude_to_y(double latitude, double center_latitude, double zoom, int size);
  double _x_to_longitude(double x, double center_longitude, double zoom, int size);
  double _y_to_latitude(double y, double center_latitude, double zoom, int size);
  void _zoom_map_to_point(double longitude, double latitude, double zoom);

  QPushButton* _button_load;
  QLabel* _label_load;
  QComboBox* _combo_abtl;
  QComboBox* _combo_table_type;
  QComboBox* _combo_value_type;
  QComboBox* _combo_division;
  QWidget* _container_first_n_prefs_widgets;
  QSpinBox* _spinbox_first_n_prefs;
  QWidget* _container_later_prefs_widgets;
  QSpinBox* _spinbox_later_prefs_fixed;
  QSpinBox* _spinbox_later_prefs_up_to;
  QWidget* _container_pref_sources_widgets;
  QSpinBox* _spinbox_pref_sources_min;
  QSpinBox* _spinbox_pref_sources_max;
  QWidget* _container_n_party_preferred_widgets;
  QPushButton* _button_n_party_preferred_calculate;
  QWidget* _container_custom_widgets;
  QLineEdit* _lineedit_custom_filter;
  QLineEdit* _lineedit_custom_rows;
  QLineEdit* _lineedit_custom_cols;
  QLineEdit* _lineedit_custom_cell;
  QPushButton* _button_calculate_custom;
  QComboBox* _combo_custom_table_target;
  QLabel* _label_custom_filtered_base;
  Clickable_label* _label_sort;
  Clickable_label* _label_toggle_names;
  QPushButton* _button_calculate_after_spinbox;
  QWidget* _container_copy_main_table;
  QPushButton* _button_copy_main_table;
  QPushButton* _button_export_main_table;
  QPushButton* _button_cross_table;
  QLabel* _label_progress;
  QPushButton* _button_abbreviations;
  QPushButton* _button_help;
  QLabel* _label_division_table_title;
  QLabel* _label_map_title;
  Clickable_label* _label_map_help;
  QPushButton* _button_divisions_copy;
  QPushButton* _button_divisions_export;
  QPushButton* _button_divisions_booths_export;
  QPushButton* _button_divisions_cross_table;
  QPushButton* _button_booths_cross_table;
  QComboBox* _combo_map_type;
  QLabel* _label_map_booth_min_1;
  QLabel* _label_map_booth_min_2;
  QSpinBox* _spinbox_map_booth_threshold;
  Table_view* _table_main;
  Table_view* _table_divisions;
  QStandardItemModel* _table_main_model;
  QStandardItemModel* _table_divisions_model;
  QVector<QVector<Table_main_item>> _table_main_data;
  Table_main_item _table_main_data_total_base;
  QVector<Table_divisions_item> _table_divisions_data;
  QVector<QVector<QVector<int>>> _table_main_booth_data;
  QVector<QVector<int>> _table_main_booth_data_row_bases;
  QVector<int> _table_main_booth_data_total_base;
  QVector<QVector<int>> _cross_table_data;
  Custom_axis_definition _custom_rows;
  Custom_axis_definition _custom_cols;
  QString _custom_filter_sql;
  std::vector<Custom_operation> _custom_filter_operations;
  std::vector<Custom_operation> _custom_row_operations;
  std::vector<Custom_operation> _custom_col_operations;
  std::vector<Custom_operation> _custom_cell_operations;
  std::vector<int> _custom_axis_numbers;
  std::vector<int> _custom_row_stack_indices;
  std::vector<int> _custom_col_stack_indices;
  QVector<int> _custom_cross_table_row_bases;
  int _custom_cross_table_total_base;
  QStringList _custom_table_row_headers;
  QStringList _custom_table_col_headers;
  int _num_custom_every_expr_threads;
  int _num_custom_every_expr_threads_completed;
  QVector<int> _custom_main_table_col_widths;
  QElapsedTimer _timer;
  Polygon_model _map_divisions_model;
  Booth_model _map_booths_model;
  Map_container* _qml_map_container;
  QObject* _qml_map;
  QDoubleSpinBox* _spinbox_map_min;
  QDoubleSpinBox* _spinbox_map_max;
  Clickable_label* _label_reset_map_scale;
  QPushButton* _button_map_copy;
  QPushButton* _button_map_export;
  bool _sort_ballot_order;
  Arbitrary_axis_sort _sort_npp;
  Arbitrary_axis_sort _sort_divisions;
  Arbitrary_axis_sort _sort_custom_rows;
  Arbitrary_axis_sort _sort_custom_cols;
  QVector<int> _custom_sort_indices_rows;
  QVector<int> _custom_sort_indices_cols;
  bool _show_btl_headers;
  QString _latest_path;
  QString _latest_out_path;
  QString _database_file_path;
  QString _booths_output_file;
  bool _opened_database;
  QVector<int> _clicked_cells;
  QVector<Two_axis_click_cell> _clicked_cells_two_axis;
  QVector<int> _clicked_n_parties;
  QHash<QString, int> _group_from_short;
  QHash<QString, int> _cand_from_short;
  QVector<int> _group_from_candidate;
  std::vector<std::vector<int>> _candidates_per_group;
  int _num_groups;
  int _num_cands;
  int _num_table_rows;
  QStringList _divisions;
  QVector<Bounding_box> _division_bboxes;
  int _year;
  QString _state_short;
  QString _state_full;
  QStringList _atl_groups;
  QStringList _atl_groups_short;
  QStringList _btl_names;
  QStringList _btl_names_short;
  QStringList _table_main_groups;
  QStringList _table_main_groups_short;
  int _total_formal_votes;
  int _total_atl_votes;
  int _total_btl_votes;
  int _current_threads;
  int _completed_threads;
  bool _doing_calculation;
  int _one_line_height;
  int _two_line_height;
  double _map_scale_min_default = 0.;
  double _map_scale_max_default = 100.;
  QString _cross_table_title;
  QString _divisions_cross_table_title;
  QVector<int> _division_formal_votes;
  QVector<Booth> _booths;
  bool _offset_set_map_center = false;
};

#endif // MAIN_WIDGET_H
