#ifndef MAIN_WIDGET_H
#define MAIN_WIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QTableView>
#include <QStandardItemModel>
#include <QSqlDatabase>
#include <QThread>
#include <QQuickWidget>
#include <table_window.h>
#include "map_container.h"
#include "polygon_model.h"
#include "booth_model.h"


struct Table_main_item
{
  // Data for one cell of the main table
  int group_id;
  int sorted_idx;
  QVector<double> percentages;
  QVector<long> votes;
};

struct Table_divisions_item
{
  // Data for one row of the divisions table
  // (vectors are length 1 for most table types, but
  // are of length n+1 for n-party-preferred)
  int division;
  QVector<long> votes;
  QVector<double> percentage;
  QVector<double> total_percentage;
};

struct N_party_click_cell
{
  int i;
  int j;
};

struct Arbitrary_col_sort
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

// https://wiki.qt.io/Clickable_QLabel
class ClickableLabel : public QLabel { 
    Q_OBJECT 

public:
    explicit ClickableLabel(QWidget* parent = Q_NULLPTR);
    ~ClickableLabel();

signals:
    void clicked();

protected:
    void mouseReleaseEvent(QMouseEvent* event);
};

class Widget : public QWidget
{
  Q_OBJECT
  
public:
  Widget(QWidget *parent = nullptr);
  ~Widget();

private slots:
  void process_thread_sql_main_table(const QVector<QVector<long>> &);
  void process_thread_sql_npp_table(const QVector<QVector<QVector<long>>> &);
  void process_thread_sql_cross_table(const QVector<QVector<long>> &);
  void open_database();
  void clicked_main_table(const QModelIndex &index);
  void change_abtl(int i);
  void change_table_type(int i);
  void change_value_type(int i);
  void change_division(int i);
  void change_first_n_prefs(int i);
  void change_later_prefs_fixed(int i);
  void change_later_prefs_up_to(int i);
  void change_pref_sources_min(int i);
  void change_pref_sources_max(int i);
  void change_map_type();
  //void change_map_booth_type();
  void change_map_booth_threshold(int i);
  void update_map_scale_minmax();
  void calculate_n_party_preferred();
  void add_column_to_main_table();
  void make_cross_table();
  void make_divisions_cross_table();
  void make_booths_cross_table();
  void export_booths_table();
  void show_abbreviations();
  void show_help();
  void toggle_sort();
  void toggle_names();
  void change_npp_sort(int i);
  void change_divisions_sort(int i);
  void copy_main_table();
  void export_main_table();
  void copy_divisions_table();
  void export_divisions_table();
  void reset_map_scale();
  void zoom_to_state();
  void zoom_to_capital();
  void zoom_to_division();
  void zoom_to_division(int div);
  void copy_map();
  void export_map();
  void delayed_copy_map();
  void delayed_export_map(QString file_name);
  
private:
  const double rad2deg = 57.2957795131;
  QPushButton *button_load;
  QLabel *label_load;
  QComboBox *combo_abtl;
  QComboBox *combo_table_type;
  QComboBox *combo_value_type;
  QComboBox *combo_division;
  QWidget *container_first_n_prefs_widgets;
  QSpinBox *spinbox_first_n_prefs;
  QWidget *container_later_prefs_widgets;
  QSpinBox *spinbox_later_prefs_fixed;
  QSpinBox *spinbox_later_prefs_up_to;
  QWidget *container_pref_sources_widgets;
  QSpinBox *spinbox_pref_sources_min;
  QSpinBox *spinbox_pref_sources_max;
  QWidget *container_n_party_preferred_widgets;
  QPushButton *button_n_party_preferred_calculate;
  ClickableLabel *label_sort;
  ClickableLabel *label_toggle_names;
  QPushButton *button_calculate_after_spinbox;
  QWidget *container_copy_main_table;
  QPushButton *button_copy_main_table;
  QPushButton *button_export_main_table;
  QPushButton *button_cross_table;
  QLabel *label_progress;
  QPushButton *button_abbreviations;
  QPushButton *button_help;
  QLabel *label_division_table_title;
  QLabel *label_map_title;
  QPushButton *button_divisions_copy;
  QPushButton *button_divisions_export;
  QPushButton *button_divisions_booths_export;
  QPushButton *button_divisions_cross_table;
  QPushButton *button_booths_cross_table;
  QComboBox *combo_map_type;
  QComboBox *combo_map_booth_type;
  QLabel *label_map_booth_min_1;
  QLabel *label_map_booth_min_2;
  QSpinBox *spinbox_map_booth_threshold;
  QTableView *table_main;
  QTableView *table_divisions;
  QStandardItemModel *table_main_model;
  QStandardItemModel *table_divisions_model;
  QVector<QVector<Table_main_item>> table_main_data;
  QVector<Table_divisions_item> table_divisions_data;
  QVector<QVector<QVector<long>>> table_main_booth_data;
  QVector<QVector<long>> cross_table_data;
  QVector<QVector<Table_main_item>> temp_booths_npp_data;
  Polygon_model map_divisions_model;
  Booth_model map_booths_model;
  Map_container *qml_map_container;
  QObject *qml_map;
  QDoubleSpinBox *spinbox_map_min;
  QDoubleSpinBox *spinbox_map_max;
  ClickableLabel *label_reset_map_scale;
  QPushButton *button_map_copy;
  QPushButton *button_map_export;
  bool sort_ballot_order;
  Arbitrary_col_sort sort_npp;
  Arbitrary_col_sort sort_divisions;
  bool show_btl_headers;
  QString latest_path;
  QString latest_out_path;
  QString database_file_path;
  QString booths_output_file;
  bool opened_database;
  QVector<int> clicked_cells;
  QVector<N_party_click_cell> clicked_cells_n_party;
  QVector<int> clicked_n_parties;
  int num_groups;
  int num_cands;
  int num_table_rows;
  QStringList divisions;
  QVector<Bounding_box> division_bboxes;
  int year;
  QString state_short;
  QString state_full;
  QStringList atl_groups;
  QStringList atl_groups_short;
  QStringList btl_names;
  QStringList btl_names_short;
  QStringList table_main_groups;
  QStringList table_main_groups_short;
  long total_formal_votes;
  long total_atl_votes;
  long total_btl_votes;
  int ideal_threads;
  int current_threads;
  int completed_threads;
  bool doing_calculation;
  int one_line_height;
  int two_line_height;
  double map_scale_min_default = 0.;
  double map_scale_max_default = 100.;
  QString cross_table_title;
  QString divisions_cross_table_title;
  QVector<long> division_formal_votes;
  QVector<Booth> booths;
  bool offset_set_map_center = false;
  void reset_spinboxes();
  void load_database(QString db_file);
  void set_table_groups();
  void setup_main_table();
  void reset_table();
  void set_main_table_cells(int col, bool n_party_preferred = false);
  void set_all_main_table_cells();
  void set_main_table_row_height();
  void make_main_table_row_headers(bool is_blank);
  void do_sql_query_for_table(QString q, bool wide_table = false);
  void set_divisions_table();
  void set_divisions_map();
  void sort_table_column(int i);
  void sort_main_table();
  void sort_main_table_npp();
  void set_clicked_cell_n_party(int i, int j);
  void reset_npp_sort();
  void reset_divisions_sort();
  void clear_divisions_table();
  void set_divisions_table_title();
  void fill_in_divisions_table();
  void update_map_booths();
  void sort_divisions_table_data();
  void lock_main_interface();
  void unlock_main_interface();
  void init_cross_table_data(int n);
  void init_cross_table_window(Table_window *w);
  void write_sql_to_file(QString q);
  void copy_model(QStandardItemModel *model, QString title="");
  void export_model(QStandardItemModel *model, QString title="");
  void update_output_path(QString file_name, QString file_type);
  void set_title_from_divisions_table();
  void spinbox_change();
  QString get_export_divisions_table_title();
  QString get_export_line(QStandardItemModel *model, int i, QString separator);
  QStringList queries_threaded(QString q, int &num_threads, bool one_thread = false);
  QString get_table_type();
  QString get_value_type();
  QString get_abtl();
  QString get_groups_table();
  QString get_short_group(int i);
  QString get_output_path(QString file_type);
  QString get_map_type();
  QString get_map_booth_type();
  int get_map_booth_threshold();
  int get_width_from_text(QString t, QWidget *w, int buffer = 30);
  int get_num_groups();
  int get_n_preferred();
  int get_n_first_prefs();
  int get_later_prefs_n_up_to();
  int get_later_prefs_n_fixed();
  int get_pref_sources_min();
  int get_pref_sources_max();
  int get_current_division();
  QColor get_highlight_color();
  QColor get_n_party_preferred_color();
  QColor get_unfocused_text_color();
  QColor get_focused_text_color();
  void set_default_cell_style(int i, int j);
  void fade_cell(int i, int j);
  void highlight_cell(int i, int j);
  void highlight_cell_n_party_preferred(int i, int j);
  void unhighlight_cell(int i, int j);
  QPixmap get_pixmap_for_map();
  double longitude_to_x(double longitude, double center_longitude, double zoom, int size);
  double latitude_to_y(double latitude, double center_latitude, double zoom, int size);
  double x_to_longitude(double x, double center_longitude, double zoom, int size);
  double y_to_latitude(double y, double center_latitude, double zoom, int size);
};

#endif // MAIN_WIDGET_H
