/* Database table structure:
CREATE TABLE basic_info (id INTEGER PRIMARY KEY, state TEXT, state_full TEXT, year INTEGER, formal_votes INTEGER)
CREATE TABLE seats (id INTEGER PRIMARY KEY, seat TEXT, formal_votes INTEGER)
CREATE TABLE booths (id INTEGER PRIMARY KEY, seat TEXT, booth TEXT, lon REAL, lat REAL, formal_votes INTEGER)
CREATE TABLE groups (id INTEGER PRIMARY KEY, group_letter TEXT, party TEXT, party_ab TEXT, primaries INTEGER)
CREATE TABLE candidates (id INTEGER PRIMARY KEY, group_letter TEXT, group_pos INTEGER, party TEXT, party_ab TEXT, candidate TEXT, primaries INTEGER)
CREATE TABLE atl (id INTEGER PRIMARY KEY, seat_id INTEGER, booth_id INTEGER, num_prefs INTEGER, P1, P2, ..., Pfor0, Pfor1, ...)
CREATE TABLE btl (id INTEGER PRIMARY KEY, seat_id INTEGER, booth_id INTEGER, num_prefs INTEGER, P1, P2, ..., Pfor0, Pfor1, ...)
*/


#include "main_widget.h"
#include "worker_sql_main_table.h"
#include "worker_sql_npp_table.h"
#include "worker_sql_cross_table.h"
#include "table_window.h"

// Layout etc.:
#include <QSplitter>
#include <QGridLayout>
#include <QFormLayout>
#include <QSizePolicy>
#include <QDir>
#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QScreen>

// Form elements:
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QSpacerItem>
#include <QTableView>
#include <QHeaderView>
#include <QFileDialog>
#include <QFileInfo>
#include <QCoreApplication>
#include <QFont>

// SQL:
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QThread>
#include <QAction>
#include <QMetaType>

// For the map:
#include <QWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickWidget>
#include <QQuickItem>
#include <QTimer>
//#include <QSslSocket>
//#include <QGeoPath>
#include "polygon_model.h"
#include "map_container.h"

#include <QMessageBox>


Widget::Widget(QWidget *parent)
  : QWidget(parent),
    sort_ballot_order(false),
    show_btl_headers(true),
    latest_path(QDir::currentPath()),
    latest_out_path(QDir::currentPath()),
    opened_database(false),
    num_groups(0),
    num_cands(0),
    num_table_rows(0),
    ideal_threads(QThread::idealThreadCount()),
    doing_calculation(false)
{
  // This is needed to allow a column of table data to be an argument to 
  // a signal (which is emitted when a thread finishes processing an SQL
  // query):
  qRegisterMetaType<QVector<Table_main_item>>("QVector<Table_main_item>");
  qRegisterMetaType<QVector<QVector<Table_main_item>>>("QVector<QVector<Table_main_item>>");
  qRegisterMetaType<QVector<QVector<long>>>("QVector<QVector<long>>");
  
  // Required to allow the QML to talk to the model containing the polygons:
  qmlRegisterType<Polygon_model>("Division_boundaries", 1, 0, "Polygon_model");
  
  
  // The key to having the interface resize nicely when the user resizes the window is:
  // Add the QSplitter to a QGridLayout, then make the latter the layout for the 
  // main widget ("this").
  QGridLayout *main_container = new QGridLayout();
  main_container->setContentsMargins(0, 0, 0, 0);
  
  QSplitter *splitter = new QSplitter(this);
  splitter->setStyleSheet("QSplitter::handle { background: #d0d0d0; }");
  
  QWidget *container_widget_left   = new QWidget();
  QWidget *container_widget_middle = new QWidget();
  QWidget *container_widget_right  = new QWidget();
  
  
  splitter->addWidget(container_widget_left);
  splitter->addWidget(container_widget_middle);
  splitter->addWidget(container_widget_right);
  
  splitter->setCollapsible(0, false);
  splitter->setCollapsible(1, true);
  splitter->setCollapsible(2, true);
  
  QVBoxLayout *layout_left = new QVBoxLayout();

  QHBoxLayout *layout_load = new QHBoxLayout();
  
  QString load_text("Load preferences...");
  button_load = new QPushButton(load_text, this);
  int load_width = get_width_from_text(load_text, button_load, 10);
  button_load->setMaximumWidth(load_width);
  button_load->setMinimumWidth(load_width);
  
  label_load = new QLabel(this);
  label_load->setText("No file loaded");
  
  layout_load->addWidget(button_load);
  layout_load->addWidget(label_load);
  
  layout_left->addLayout(layout_load);
  layout_left->setAlignment(layout_load, Qt::AlignTop);
  
  QFormLayout *layout_combos = new QFormLayout;
  layout_combos->setLabelAlignment(Qt::AlignRight);
  
  combo_abtl = new QComboBox;
  combo_abtl->addItem("Above the line", "atl");
  combo_abtl->addItem("Below the line", "btl");
  
  combo_table_type = new QComboBox;
  combo_table_type->addItem("Step forward", "step_forward");
  combo_table_type->addItem("In first N preferences", "first_n_prefs");
  combo_table_type->addItem("Later preferences", "later_prefs");
  combo_table_type->addItem("Preference sources", "pref_sources");
  combo_table_type->addItem("N-party preferred", "n_party_preferred");
  
  combo_value_type = new QComboBox;
  combo_value_type->addItem("Votes", "votes");
  combo_value_type->addItem("Percentages", "percentages");
  combo_value_type->addItem("Total percentages", "total_percentages");
  combo_value_type->setCurrentIndex(1);
  
  combo_division = new QComboBox;
  
  int width_combo = get_width_from_text("In first N preferences", combo_table_type);
  combo_abtl->setMinimumWidth(width_combo);
  combo_abtl->setMaximumWidth(width_combo);
  combo_table_type->setMaximumWidth(width_combo);
  combo_table_type->setMinimumWidth(width_combo);
  combo_value_type->setMaximumWidth(width_combo);
  combo_value_type->setMinimumWidth(width_combo);
  combo_division->setMaximumWidth(width_combo);
  combo_division->setMinimumWidth(width_combo);
  
  
  layout_combos->addRow("Preferences:", combo_abtl);
  layout_combos->addRow("Table type:", combo_table_type);
  layout_combos->addRow("Show", combo_value_type);
  layout_combos->addRow("in", combo_division);
  
  layout_left->addLayout(layout_combos);
  
  
  
  // ~~~~ Widgets unique to "first_n_prefs" table type ~~~~
  container_first_n_prefs_widgets = new QWidget;
  container_first_n_prefs_widgets->hide();
  QHBoxLayout *layout_first_n_prefs = new QHBoxLayout;
  
  QLabel *label_first_n_prefs_1 = new QLabel("Consider first");
  QLabel *label_first_n_prefs_2 = new QLabel("preferences");
  spinbox_first_n_prefs = new QSpinBox;
  spinbox_first_n_prefs->setAlignment(Qt::AlignRight);
  spinbox_first_n_prefs->setKeyboardTracking(false);
  
  int spinbox_width = get_width_from_text("999", spinbox_first_n_prefs);
  spinbox_first_n_prefs->setMinimumWidth(spinbox_width);
  spinbox_first_n_prefs->setMaximumWidth(spinbox_width);
  
  layout_first_n_prefs->addWidget(label_first_n_prefs_1);
  layout_first_n_prefs->addWidget(spinbox_first_n_prefs);
  layout_first_n_prefs->addWidget(label_first_n_prefs_2);
  
  layout_first_n_prefs->insertStretch(-1, 1);
  
  container_first_n_prefs_widgets->setLayout(layout_first_n_prefs);
  layout_left->addWidget(container_first_n_prefs_widgets);
  
  
  // ~~~~ Widgets unique to "later_prefs" table type ~~~~
  container_later_prefs_widgets = new QWidget();
  container_later_prefs_widgets->hide();
  QGridLayout *layout_later_prefs = new QGridLayout;
  
  QLabel *label_later_prefs_1 = new QLabel("Fix first");
  QLabel *label_later_prefs_2 = new QLabel("preferences,");
  QLabel *label_later_prefs_3 = new QLabel("and consider up to preference");
  
  spinbox_later_prefs_fixed = new QSpinBox;
  spinbox_later_prefs_fixed->setMinimumWidth(spinbox_width);
  spinbox_later_prefs_fixed->setMaximumWidth(spinbox_width);
  spinbox_later_prefs_fixed->setAlignment(Qt::AlignRight);
  spinbox_later_prefs_fixed->setKeyboardTracking(false);
  
  spinbox_later_prefs_up_to = new QSpinBox;
  spinbox_later_prefs_up_to->setMinimumWidth(spinbox_width);
  spinbox_later_prefs_up_to->setMaximumWidth(spinbox_width);
  spinbox_later_prefs_up_to->setAlignment(Qt::AlignRight);
  spinbox_later_prefs_up_to->setKeyboardTracking(false);
  
  QSpacerItem *spacer_later_prefs_0 = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  QSpacerItem *spacer_later_prefs_1 = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  
  layout_later_prefs->addWidget(label_later_prefs_1, 0, 0, Qt::AlignRight);
  layout_later_prefs->addWidget(spinbox_later_prefs_fixed, 0, 1, Qt::AlignLeft);
  layout_later_prefs->addWidget(label_later_prefs_2, 0, 2, Qt::AlignLeft);
  layout_later_prefs->addWidget(label_later_prefs_3, 1, 0, Qt::AlignRight);
  layout_later_prefs->addWidget(spinbox_later_prefs_up_to, 1, 1, Qt::AlignLeft);
  layout_later_prefs->addItem(spacer_later_prefs_0, 0, 3);
  layout_later_prefs->addItem(spacer_later_prefs_1, 1, 3);
  
  container_later_prefs_widgets->setLayout(layout_later_prefs);
  layout_left->addWidget(container_later_prefs_widgets);
  
  
  // ~~~~ Widgets unique to "pref_sources" table type ~~~~
  container_pref_sources_widgets = new QWidget();
  container_pref_sources_widgets->hide();
  QHBoxLayout *layout_pref_sources = new QHBoxLayout;
  
  QLabel *label_pref_sources_1 = new QLabel("Consider preferences");
  QLabel *label_pref_sources_2 = new QLabel("to");
  
  spinbox_pref_sources_min = new QSpinBox;
  spinbox_pref_sources_min->setMinimumWidth(spinbox_width);
  spinbox_pref_sources_min->setMaximumWidth(spinbox_width);
  spinbox_pref_sources_min->setAlignment(Qt::AlignRight);
  spinbox_pref_sources_min->setKeyboardTracking(false);
  
  spinbox_pref_sources_max = new QSpinBox;
  spinbox_pref_sources_max->setMinimumWidth(spinbox_width);
  spinbox_pref_sources_max->setMaximumWidth(spinbox_width);
  spinbox_pref_sources_max->setAlignment(Qt::AlignRight);
  spinbox_pref_sources_max->setKeyboardTracking(false);
  
  layout_pref_sources->addWidget(label_pref_sources_1);
  layout_pref_sources->addWidget(spinbox_pref_sources_min);
  layout_pref_sources->addWidget(label_pref_sources_2);
  layout_pref_sources->addWidget(spinbox_pref_sources_max);
  
  layout_pref_sources->insertStretch(-1, 1);
  
  container_pref_sources_widgets->setLayout(layout_pref_sources);
  layout_left->addWidget(container_pref_sources_widgets);
  
  
  // ~~~~ Widgets unique to "n_party_preferred" table type ~~~~
  container_n_party_preferred_widgets = new QWidget();
  container_n_party_preferred_widgets->hide();
  QHBoxLayout *layout_n_party_preferred = new QHBoxLayout;
  
  QLabel *label_n_party_preferred = new QLabel("Click on the wanted parties");
  button_n_party_preferred_calculate = new QPushButton("Calculate", this);
  button_n_party_preferred_calculate->setEnabled(false);
  
  layout_n_party_preferred->addWidget(label_n_party_preferred);
  layout_n_party_preferred->addWidget(button_n_party_preferred_calculate);
  
  layout_n_party_preferred->insertStretch(-1, 1);
  
  container_n_party_preferred_widgets->setLayout(layout_n_party_preferred);
  
  layout_left->addWidget(container_n_party_preferred_widgets);
  
  
  QHBoxLayout *layout_label_toggles = new QHBoxLayout();
  
  label_sort = new ClickableLabel();
  label_sort->setText("<i>Toggle sort</i>");
  label_sort->setCursor(Qt::PointingHandCursor);
  label_sort->setSizePolicy(QSizePolicy());
  
  
  label_toggle_names = new ClickableLabel();
  label_toggle_names->setText("<i>Toggle names</i>");
  label_toggle_names->setCursor(Qt::PointingHandCursor);
  label_toggle_names->setSizePolicy(QSizePolicy());
  label_toggle_names->hide();
  
  button_calculate_after_spinbox = new QPushButton("Calculate", this);
  button_calculate_after_spinbox->hide();
  
  
  layout_label_toggles->addWidget(label_sort);
  layout_label_toggles->addWidget(label_toggle_names);
  layout_label_toggles->addWidget(button_calculate_after_spinbox);
  
  layout_label_toggles->insertStretch(-1, 1);
  layout_label_toggles->insertSpacing(1, 15);
  
  layout_left->addLayout(layout_label_toggles);
  
  
  // ~~~~ Main table ~~~~
  table_main_model = new QStandardItemModel(this);
  table_main = new QTableView;
  table_main->setModel(table_main_model);
  table_main->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_main->setShowGrid(false);
  table_main->setAlternatingRowColors(true);
  table_main->setStyleSheet("QTableView {alternate-background-color: #f0f0f0; background-color: #ffffff}");
  table_main->setFocusPolicy(Qt::NoFocus);
  table_main->setSelectionMode(QAbstractItemView::NoSelection);
  table_main->verticalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
  
  QHeaderView *table_main_header = table_main->horizontalHeader();
  reset_npp_sort();
  
  one_line_height = table_main->fontMetrics().boundingRect("0").height();
  // Factor of 3 in the following is a fudge (2 is fine in Ubuntu):
  two_line_height = 3*one_line_height;
  
  layout_left->addWidget(table_main);
  
  
  container_copy_main_table = new QWidget();
  container_copy_main_table->hide();
  
  QHBoxLayout *layout_copy_main_table = new QHBoxLayout();
  button_copy_main_table = new QPushButton("Copy");
  button_copy_main_table->setEnabled(false);
  button_export_main_table = new QPushButton("Export...");
  button_export_main_table->setEnabled(false);
  QSpacerItem *spacer_copy_main_table = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  layout_copy_main_table->addWidget(button_copy_main_table);
  layout_copy_main_table->addWidget(button_export_main_table);
  layout_copy_main_table->addItem(spacer_copy_main_table);
  
  container_copy_main_table->setLayout(layout_copy_main_table);
  
  layout_left->addWidget(container_copy_main_table);
  
  
  QHBoxLayout *layout_left_bottom = new QHBoxLayout();
  
  QString cross_table("Cross table...");
  button_cross_table = new QPushButton(cross_table);
  button_cross_table->setMaximumWidth(get_width_from_text(cross_table, button_cross_table, 10));
  button_cross_table->setEnabled(false);
  
  layout_left_bottom->addWidget(button_cross_table);
  
  QSpacerItem *spacer_left_bottom = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  label_progress = new QLabel("No calculation");
  label_progress->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  
  layout_left_bottom->addItem(spacer_left_bottom);
  layout_left_bottom->addWidget(label_progress);
  
  layout_left->addLayout(layout_left_bottom);
  
  
  // ~~~~~ Middle column: divisions table ~~~~~
  QVBoxLayout *layout_middle = new QVBoxLayout();
  
  QHBoxLayout *layout_middle_top = new QHBoxLayout();
  button_abbreviations = new QPushButton("Abbreviations");
  QSpacerItem *spacer_middle_top = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  button_help = new QPushButton("Help");
  
  button_abbreviations->setEnabled(false);
  button_help->setEnabled(true);
  
  layout_middle_top->addWidget(button_abbreviations);
  layout_middle_top->addItem(spacer_middle_top);
  layout_middle_top->addWidget(button_help);
  
  layout_middle->addLayout(layout_middle_top);
  
  label_division_table_title = new QLabel("<b>No selection</b>");
  label_division_table_title->setMaximumWidth(200);
  label_division_table_title->setWordWrap(true);
  
  table_divisions_model = new QStandardItemModel(this);
  table_divisions = new QTableView;
  table_divisions->setModel(table_divisions_model);
  table_divisions->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_divisions->setShowGrid(false);
  table_divisions->setAlternatingRowColors(true);
  table_divisions->setStyleSheet("QTableView {alternate-background-color: #f0f0f0; background-color: #ffffff}");
  table_divisions->setFocusPolicy(Qt::NoFocus);
  table_divisions->setSelectionMode(QAbstractItemView::NoSelection);
  
  table_divisions->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  table_divisions->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  table_divisions->verticalHeader()->setDefaultSectionSize(one_line_height);
  table_divisions->verticalHeader()->hide();
  
  QHeaderView *table_divisions_header = table_divisions->horizontalHeader();
  
  // *** this doesn't work??? ***
  table_divisions_header->setCursor(Qt::PointingHandCursor);
  
  QHBoxLayout *layout_division_buttons = new QHBoxLayout();
  button_divisions_copy = new QPushButton("Copy");
  int div_copy_width = get_width_from_text("Copy", button_divisions_copy, 10);
  button_divisions_copy->setMaximumWidth(div_copy_width);
  button_divisions_copy->setMinimumWidth(div_copy_width);
  
  button_divisions_export = new QPushButton("Export...");
  int div_export_width = get_width_from_text("Export...", button_divisions_export, 10);
  button_divisions_export->setMaximumWidth(div_export_width);
  button_divisions_export->setMinimumWidth(div_export_width);
  
  button_divisions_booths_export = new QPushButton("Export booths...");
  int div_booths_export_width = get_width_from_text("Export booths...", button_divisions_booths_export, 10);
  button_divisions_booths_export->setMaximumWidth(div_booths_export_width);
  button_divisions_booths_export->setMinimumWidth(div_booths_export_width);
  
  
  QSpacerItem *spacer_division_buttons = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  layout_division_buttons->addWidget(button_divisions_copy);
  layout_division_buttons->addWidget(button_divisions_export);
  layout_division_buttons->addWidget(button_divisions_booths_export);
  layout_division_buttons->addItem(spacer_division_buttons);
  
  
  QLabel *label_divisions_cross_tables = new QLabel("Cross tables by:");
  label_divisions_cross_tables->setAlignment(Qt::AlignLeft);
  
  
  QHBoxLayout *layout_division_cross_table_buttons = new QHBoxLayout();
  button_divisions_cross_table = new QPushButton("Division");
  button_booths_cross_table = new QPushButton("Booth (export)...");
  QSpacerItem *spacer_division_cross_tables = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  layout_division_cross_table_buttons->addWidget(button_divisions_cross_table);
  layout_division_cross_table_buttons->addWidget(button_booths_cross_table);
  layout_division_cross_table_buttons->addItem(spacer_division_cross_tables);
  
  button_divisions_copy->setEnabled(false);
  button_divisions_export->setEnabled(false);
  button_divisions_booths_export->setEnabled(false);
  button_divisions_cross_table->setEnabled(false);
  button_booths_cross_table->setEnabled(false);
  
  
  layout_middle->addWidget(label_division_table_title);
  layout_middle->addWidget(table_divisions);
  layout_middle->addLayout(layout_division_buttons);
  layout_middle->addWidget(label_divisions_cross_tables);
  layout_middle->addLayout(layout_division_cross_table_buttons);
  
  
  main_container->addWidget(splitter);
  this->setLayout(main_container);
  
  
  reset_divisions_sort();
  
  // ~~~~~ Right-hand column: map ~~~~~
  int map_size = 512;
  
  map_divisions_model.setup_list("", 2016, QStringList{});
  
  QVBoxLayout *layout_right = new QVBoxLayout();
  
  label_map_title = new QLabel("<b>No selection</b>");
  label_map_title->setMaximumWidth(map_size);
  label_map_title->setWordWrap(true);
  
  qml_map_container = new Map_container(map_size);
  qml_map_container->setMaximumWidth(map_size);
  qml_map_container->rootContext()->setContextProperty("divisions_model", &map_divisions_model);
  qml_map_container->rootContext()->setContextProperty("map_size", map_size);
  
  qml_map_container->setSource(QUrl("qrc:///map.qml"));
  qml_map_container->init_variables();
  
  QObject *qml_root_object = qml_map_container->rootObject();
  qml_map = qml_root_object->findChild<QObject*>("map");
  
  QLabel *label_map_division_info = new QLabel("Note: the map can be sluggish.");
  map_divisions_model.set_label(label_map_division_info);
  
  QHBoxLayout *layout_map_legend = new QHBoxLayout();
  
  QSpacerItem *spacer_map_legend_right = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  
  spinbox_map_min = new QDoubleSpinBox;
  spinbox_map_max = new QDoubleSpinBox;
  
  int legend_spinbox_width = get_width_from_text("99999", spinbox_map_min);
  
  spinbox_map_min->setMinimum(0.);
  spinbox_map_min->setMaximum(100.);
  spinbox_map_min->setDecimals(1);
  spinbox_map_min->setValue(0.);
  spinbox_map_min->setAlignment(Qt::AlignRight);
  spinbox_map_min->setKeyboardTracking(false);
  spinbox_map_min->setMinimumWidth(legend_spinbox_width);
  spinbox_map_min->setMaximumWidth(legend_spinbox_width);
  
  spinbox_map_max->setMinimum(0.);
  spinbox_map_max->setMaximum(100.);
  spinbox_map_max->setDecimals(1);
  spinbox_map_max->setValue(100.);
  spinbox_map_max->setAlignment(Qt::AlignRight);
  spinbox_map_max->setKeyboardTracking(false);
  spinbox_map_max->setMinimumWidth(legend_spinbox_width);
  spinbox_map_max->setMaximumWidth(legend_spinbox_width);
  
  QLabel *label_map_legend = new QLabel();
  label_map_legend->setPixmap(QPixmap::fromImage(QImage(":/viridis_scale_20.png")));
  label_map_legend->adjustSize();
  
  label_reset_map_scale = new ClickableLabel();
  label_reset_map_scale->setText("<i>Reset</i>");
  label_reset_map_scale->setCursor(Qt::PointingHandCursor);
  label_reset_map_scale->setSizePolicy(QSizePolicy());
  
  layout_map_legend->addWidget(spinbox_map_min);
  layout_map_legend->addWidget(label_map_legend);
  layout_map_legend->addWidget(spinbox_map_max);
  layout_map_legend->addWidget(label_reset_map_scale);
  layout_map_legend->addItem(spacer_map_legend_right);
  
  
  QHBoxLayout *layout_map_zooms = new QHBoxLayout();
  QLabel *label_zoom_to = new QLabel("Zoom to: ");
  
  ClickableLabel *label_zoom_to_state = new ClickableLabel();
  label_zoom_to_state->setText("<i>State</i>  ");
  label_zoom_to_state->setCursor(Qt::PointingHandCursor);
  label_zoom_to_state->setSizePolicy(QSizePolicy());
  
  ClickableLabel *label_zoom_to_capital = new ClickableLabel();
  label_zoom_to_capital->setText("<i>Capital</i>  ");
  label_zoom_to_capital->setCursor(Qt::PointingHandCursor);
  label_zoom_to_capital->setSizePolicy(QSizePolicy());
  
  layout_map_zooms->addWidget(label_zoom_to);
  layout_map_zooms->addWidget(label_zoom_to_state);
  layout_map_zooms->addWidget(label_zoom_to_capital);
  layout_map_zooms->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
  
  
  QHBoxLayout *layout_map_copy_export = new QHBoxLayout();
  button_map_copy = new QPushButton("Copy");
  button_map_export = new QPushButton("Export...");
  QSpacerItem *spacer_map_copy_export = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  
  layout_map_copy_export->addWidget(button_map_copy);
  layout_map_copy_export->addWidget(button_map_export);
  layout_map_copy_export->addItem(spacer_map_copy_export);
  
  layout_right->addWidget(label_map_title);
  layout_right->addWidget(label_map_division_info);
  layout_right->addWidget(qml_map_container);
  layout_right->addLayout(layout_map_legend);
  layout_right->addLayout(layout_map_zooms);
  layout_right->addLayout(layout_map_copy_export);
  
  layout_right->insertStretch(-1, 1);
  
  
  // Read the most recent directory an SQLITE file was loaded from.
  QString last_path_file_name = QString("%1/last_sqlite_dir.txt")
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
        latest_path = last_path;
      }
      
      last_path_file.close();
    }
  }
  
  reset_spinboxes();
  
  container_widget_left->setLayout(layout_left);
  container_widget_middle->setLayout(layout_middle);
  container_widget_right->setLayout(layout_right);
  
  connect(button_load,                        SIGNAL(clicked()),                    this, SLOT(open_database()));
  connect(combo_abtl,                         SIGNAL(currentIndexChanged(int)),     this, SLOT(change_abtl(int)));
  connect(combo_table_type,                   SIGNAL(currentIndexChanged(int)),     this, SLOT(change_table_type(int)));
  connect(combo_value_type,                   SIGNAL(currentIndexChanged(int)),     this, SLOT(change_value_type(int)));
  connect(combo_division,                     SIGNAL(currentIndexChanged(int)),     this, SLOT(change_division(int)));
  connect(spinbox_first_n_prefs,              SIGNAL(valueChanged(int)),            this, SLOT(change_first_n_prefs(int)));
  connect(spinbox_later_prefs_fixed,          SIGNAL(valueChanged(int)),            this, SLOT(change_later_prefs_fixed(int)));
  connect(spinbox_later_prefs_up_to,          SIGNAL(valueChanged(int)),            this, SLOT(change_later_prefs_up_to(int)));
  connect(spinbox_pref_sources_min,           SIGNAL(valueChanged(int)),            this, SLOT(change_pref_sources_min(int)));
  connect(spinbox_pref_sources_max,           SIGNAL(valueChanged(int)),            this, SLOT(change_pref_sources_max(int)));
  connect(button_n_party_preferred_calculate, SIGNAL(clicked()),                    this, SLOT(calculate_n_party_preferred()));
  connect(button_calculate_after_spinbox,     SIGNAL(clicked()),                    this, SLOT(add_column_to_main_table()));
  connect(button_copy_main_table,             SIGNAL(clicked()),                    this, SLOT(copy_main_table()));
  connect(button_export_main_table,           SIGNAL(clicked()),                    this, SLOT(export_main_table()));
  connect(button_cross_table,                 SIGNAL(clicked()),                    this, SLOT(make_cross_table()));
  connect(button_abbreviations,               SIGNAL(clicked()),                    this, SLOT(show_abbreviations()));
  connect(button_help,                        SIGNAL(clicked()),                    this, SLOT(show_help()));
  connect(button_divisions_copy,              SIGNAL(clicked()),                    this, SLOT(copy_divisions_table()));
  connect(button_divisions_export,            SIGNAL(clicked()),                    this, SLOT(export_divisions_table()));
  connect(button_divisions_cross_table,       SIGNAL(clicked()),                    this, SLOT(make_divisions_cross_table()));
  connect(button_booths_cross_table,          SIGNAL(clicked()),                    this, SLOT(make_booths_cross_table()));
  connect(button_divisions_booths_export,     SIGNAL(clicked()),                    this, SLOT(export_booths_table()));
  connect(label_sort,                         SIGNAL(clicked()),                    this, SLOT(toggle_sort()));
  connect(label_toggle_names,                 SIGNAL(clicked()),                    this, SLOT(toggle_names()));
  connect(table_main,                         SIGNAL(clicked(const QModelIndex &)), this, SLOT(clicked_main_table(const QModelIndex &)));
  connect(table_main_header,                  SIGNAL(sectionClicked(int)),          this, SLOT(change_npp_sort(int)));
  connect(table_divisions_header,             SIGNAL(sectionClicked(int)),          this, SLOT(change_divisions_sort(int)));
  connect(label_reset_map_scale,              SIGNAL(clicked()),                    this, SLOT(reset_map_scale()));
  connect(label_zoom_to_state,                SIGNAL(clicked()),                    this, SLOT(zoom_to_state()));
  connect(label_zoom_to_capital,              SIGNAL(clicked()),                    this, SLOT(zoom_to_capital()));
  connect(button_map_copy,                    SIGNAL(clicked()),                    this, SLOT(copy_map()));
  connect(button_map_export,                  SIGNAL(clicked()),                    this, SLOT(export_map()));
  connect(spinbox_map_min,                    SIGNAL(valueChanged(double)),         this, SLOT(update_map_scale_minmax()));
  connect(spinbox_map_max,                    SIGNAL(valueChanged(double)),         this, SLOT(update_map_scale_minmax()));
  
  connect(qml_map_container, SIGNAL(mouse_moved(double, double)), &map_divisions_model, SLOT(point_in_polygon(double, double)));
  connect(qml_root_object,   SIGNAL(exited_map()),                &map_divisions_model, SLOT(exited_map()));
  connect(spinbox_map_min,   SIGNAL(valueChanged(double)),        &map_divisions_model, SLOT(update_scale_min(double)));
  connect(spinbox_map_max,   SIGNAL(valueChanged(double)),        &map_divisions_model, SLOT(update_scale_max(double)));
}

Widget::~Widget()
{
  // Write the latest SQLITE path to file before closing.
  QString last_path_file_name = QString("%1/last_sqlite_dir.txt")
      .arg(QCoreApplication::applicationDirPath());
  
  QFile last_path_file(last_path_file_name);
  
  if (last_path_file.open(QIODevice::WriteOnly))
  {
    QTextStream out(&last_path_file);
    out << latest_path << endl;
    last_path_file.close();
  }
}

ClickableLabel::ClickableLabel(QWidget* parent)
    : QLabel(parent)
{
}

ClickableLabel::~ClickableLabel() {}

void ClickableLabel::mouseReleaseEvent(QMouseEvent* event) {
  Q_UNUSED(event);
  emit clicked();
}


int Widget::get_width_from_text(QString t, QWidget *w, int buffer)
{
  QSize text_size = w->fontMetrics().size(Qt::TextShowMnemonic, t);
  
  QStyleOption opt;
  opt.initFrom(w);
  opt.rect.setSize(text_size);
  
  QString widget_type(w->metaObject()->className());
  QStyle::ContentsType style_type;
  
  if (widget_type == "QComboBox")
  {
    style_type = QStyle::CT_ComboBox;
  }
  else
  {
    style_type = QStyle::CT_PushButton;
  }
  
  return w->style()->sizeFromContents(style_type, &opt, text_size, w).width() + buffer;
}

void Widget::reset_spinboxes()
{
  doing_calculation = true; // Just to prevent any changed() signals from doing anything
  
  spinbox_first_n_prefs->setMinimum(2);
  spinbox_first_n_prefs->setMaximum(6);
  spinbox_first_n_prefs->setValue(6);
  
  spinbox_later_prefs_fixed->setMinimum(1);
  spinbox_later_prefs_fixed->setMaximum(5);
  spinbox_later_prefs_fixed->setValue(1);
  
  spinbox_later_prefs_up_to->setMinimum(2);
  spinbox_later_prefs_up_to->setMaximum(6);
  spinbox_later_prefs_up_to->setValue(6);

  spinbox_pref_sources_min->setMinimum(2);
  spinbox_pref_sources_min->setMaximum(6);
  spinbox_pref_sources_min->setValue(2);

  spinbox_pref_sources_max->setMinimum(2);
  spinbox_pref_sources_max->setMaximum(6);
  spinbox_pref_sources_max->setValue(6);
  
  doing_calculation = false;
}

void Widget::open_database()
{
  QString file_name = QFileDialog::getOpenFileName(this,
                                                   QString(),
                                                   latest_path,
                                                   QString("*.sqlite"));
  
  if (file_name.isNull()) { return; }
  
  QFileInfo check_exists(file_name);
  if (check_exists.exists() && check_exists.isFile())
  {
    latest_path = check_exists.absolutePath();
    load_database(file_name);
  }
}


void Widget::load_database(QString db_file)
{
  // Get rid of any data that might exist:
  table_main_data.clear();
  table_main_model->clear();
  clicked_cells.clear();
  clicked_cells_n_party.clear();
  clicked_n_parties.clear();
  clear_divisions_table();
  division_formal_votes.clear();
  reset_spinboxes();
  
  bool errors = false;
  QString error_msg;
  QString connection_name("conn");
  
  {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connection_name);
    db.setDatabaseName(db_file);
    
    if (!db.open())
    {
      errors = true;
      error_msg = QString("Error: couldn't open database.");
    }
    
    if (!errors)
    {
      QStringList tables = db.tables();
      if (tables.indexOf("basic_info") < 0 ||
          tables.indexOf("candidates") < 0 ||
          tables.indexOf("groups")     < 0 ||
          tables.indexOf("seats")      < 0 ||
          tables.indexOf("booths")     < 0 ||
          tables.indexOf("atl")        < 0 ||
          tables.indexOf("btl")        < 0)
      {
        errors = true;
        error_msg = QString("Error: database is not formatted correctly.");
      }
    }
    
    QSqlQuery query(db);
    
    if (!errors)
    {
      if (!query.exec("SELECT state, state_full, year, formal_votes, atl_votes, btl_votes FROM basic_info"))
      {
        errors = true;
        error_msg = "Couldn't read basic information from database";
      }
      else
      {
        query.next();
        //label_load->setText(query.value(0).toString() + " " + query.value(2).toString());
        state_short        = query.value(0).toString();
        state_full         = query.value(1).toString();
        year               = query.value(2).toInt();
        total_formal_votes = query.value(3).toLongLong();
        total_atl_votes    = query.value(4).toLongLong();
        total_btl_votes    = query.value(5).toLongLong();
        
        label_load->setText(QString("%1 %2").arg(state_short).arg(year));
      }
    }
    
    if (!errors)
    {
      // *** Error-handling needs to be much better in here ***
      // e.g., even after detecting an error, the code continues to run....
      
      if (!query.exec("SELECT COUNT(*) FROM groups"))
      {
        errors = true;
        error_msg = "Couldn't count number of groups";
      }
      else 
      {
        query.next();
        num_groups = query.value(0).toInt();
      }
      
      if (!query.exec("SELECT COUNT(*) FROM candidates"))
      {
        errors = true;
        error_msg = "Couldn't count number of candidates";
      }
      else 
      {
        query.next();
        num_cands = query.value(0).toInt();
      }
    }
    
    if (!errors)
    {
      if (!query.exec("SELECT party, party_ab FROM groups ORDER BY id"))
      {
        errors = true;
        error_msg = "Couldn't read party names";
      }
      else
      {
        atl_groups.clear();
        atl_groups_short.clear();
        
        while (query.next())
        {
          atl_groups.append(query.value(0).toString());
          atl_groups_short.append(query.value(1).toString());
        }
      }
    }
    
    if (!errors)
    {
      if (!query.exec("SELECT party_ab, group_letter, group_pos, candidate FROM candidates ORDER BY id"))
      {
        errors = true;
        error_msg = "Couldn't read candidate names";
      }
      else
      {
        btl_names.clear();
        btl_names_short.clear();
        
        while (query.next())
        {
          QString this_party(query.value(0).toString());
          if (query.value(1).toString() == "UG")
          {
            this_party = "UG";
          } 
          
          QString full_name = query.value(3).toString();
          int comma_pos = full_name.indexOf(",");
          if (comma_pos >= 0)
          {
            int n = 1;
            if (comma_pos == full_name.indexOf(", "))
            {
              n = 2;
            }
            full_name.replace(comma_pos, n, ",\n");
          }
          
          btl_names.append(full_name);
          btl_names_short.append(this_party + "_" + query.value(2).toString());
        }
      }
    }
    
    if (!errors)
    {
      if (!query.exec("SELECT seat, formal_votes FROM seats ORDER BY id"))
      {
        errors = true;
        error_msg = "Couldn't read divisions";
      }
      else
      {
        // The changed() signal from the divisions combobox will be emitted 
        // if it's not blocked, leading to a potential crash when the program
        // tries to update the table but the data is missing.
        combo_division->blockSignals(true);
        combo_division->clear();
        
        divisions.clear();
        
        while(query.next())
        {
          QString div(query.value(0).toString());
          divisions.append(div);
          combo_division->addItem(div);
          division_formal_votes.append(query.value(1).toLongLong());
        }
        combo_division->addItem(state_full);
        combo_division->setCurrentIndex(divisions.length());
        
        division_formal_votes.append(total_formal_votes);
        combo_division->blockSignals(false);
      }
    }
    
    if (!errors)
    {
      if (!query.exec("SELECT id, seat, booth, lon, lat, formal_votes FROM booths ORDER BY id"))
      {
        errors = true;
        error_msg = "Couldn't read booths";
      }
      else
      {
        booths.clear();
        int i = 0;
        while (query.next())
        {
          Booth this_booth;
          this_booth.id = query.value(0).toInt();
          this_booth.division = query.value(1).toString();
          this_booth.booth = query.value(2).toString();
          this_booth.longitude = query.value(3).toDouble();
          this_booth.latitude = query.value(4).toDouble();
          this_booth.formal_votes = query.value(5).toLongLong();
          
          if (i != this_booth.id)
          {
            errors = true;
            error_msg = "Booth ID's not as expected";
            break;
          }
          
          i++;
          booths.append(this_booth);
        }
      }
    }
    
    db.close();
  }
  
  QSqlDatabase::removeDatabase(connection_name);
  
  if (errors)
  {
    QMessageBox msgBox;
    msgBox.setText(QString("Error: %1").arg(error_msg));
    msgBox.exec();
    
    opened_database = false;
    database_file_path = "";
    label_load->setText("No file selected");
    
    button_cross_table->setEnabled(false);
    button_abbreviations->setEnabled(false);
    button_divisions_copy->setEnabled(false);
    button_divisions_export->setEnabled(false);
    button_divisions_booths_export->setEnabled(false);
    button_divisions_cross_table->setEnabled(false);
    button_booths_cross_table->setEnabled(false);
    
    // *** clear map ***
    
    return;
  }
  else
  {
    opened_database = true;
    database_file_path = db_file;
    set_table_groups();
    int current_num_groups = get_num_groups();
    spinbox_first_n_prefs->setMaximum(current_num_groups);
    spinbox_later_prefs_up_to->setMaximum(current_num_groups);
    spinbox_pref_sources_max->setMaximum(current_num_groups);
    spinbox_pref_sources_min->setMaximum(get_pref_sources_max());
    spinbox_pref_sources_max->setMinimum(get_pref_sources_min());
    
    map_divisions_model.setup_list(state_short, year, divisions);
    
    setup_main_table();
    
    add_column_to_main_table();
  }
}


void Widget::set_table_groups()
{
  table_main_groups.clear();
  table_main_groups_short.clear();
  
  if (get_abtl() == "atl")
  {
    for (int i = 0; i < atl_groups.length(); i++)
    {
      table_main_groups.append(atl_groups.at(i));
      table_main_groups_short.append(atl_groups_short.at(i));
    }
  }
  else
  {
    for (int i = 0; i < btl_names.length(); i++)
    {
      table_main_groups.append(btl_names.at(i));
      table_main_groups_short.append(btl_names_short.at(i));
    }
  }
}

void Widget::setup_main_table()
{
  table_main_data.clear();
  
  int current_num_groups = get_num_groups();
  num_table_rows = current_num_groups + 1;
  
  QString abtl = get_abtl();
  bool doing_atl = (abtl == "atl") ? true : false;
  
  QString table_type = get_table_type();
  
  // In N-party-preferred, exhaust is a column, not a row:
  if (table_type == "n_party_preferred") { num_table_rows--; }
  
  table_main_model->setRowCount(num_table_rows);
  QStringList headers;
  
  if (table_type == "step_forward")
  {
    int default_prefs = 6;
    table_main_model->setColumnCount(default_prefs);
    for (int i = 0; i < default_prefs; i++)
    {
      headers.append(QString("Pref %1").arg(i+1));
    }
  }
  else if (table_type == "first_n_prefs")
  {
    int num_prefs = get_n_first_prefs();
    int num_cols = qMin(6, num_prefs);
    
    table_main_model->setColumnCount(num_cols);
    
    for (int i = 0; i < num_cols; i++)
    {
      headers.append(QString("By %1").arg(num_prefs));
    }
  }
  else if (table_type == "later_prefs")
  {
    int fixed_prefs = get_later_prefs_n_fixed();
    int max_prefs = get_later_prefs_n_up_to();
    int num_cols = qMin(6, max_prefs);
    
    table_main_model->setColumnCount(num_cols);
    for (int i = 0; i < fixed_prefs; i++)
    {
      headers.append(QString("Pref %1").arg(i+1));
    }
    
    for (int i = fixed_prefs; i < num_cols; i++)
    {
      headers.append(QString("By %1").arg(max_prefs));
    }
  }
  else if (table_type == "pref_sources")
  {
    int min_pref = get_pref_sources_min();
    int max_pref = get_pref_sources_max();
    
    table_main_model->setColumnCount(2);
    
    headers.append(QString("%1-%2").arg(min_pref).arg(max_pref));
    headers.append("Pref 1");
  }
  else if (table_type == "n_party_preferred")
  {
    table_main_model->setColumnCount(2);
    
    headers.append("Pref 1");
    QString col_header = doing_atl ? "Group" : "Cand";
    headers.append(col_header);
  }
  
  table_main_model->setHorizontalHeaderLabels(headers);
  make_main_table_row_headers(doing_atl || !show_btl_headers);
  table_main->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  table_main->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  
  if (table_type == "n_party_preferred")
  {
    table_main_model->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignRight);
  }
}


void Widget::make_main_table_row_headers(bool is_blank)
{
  int current_num_groups = get_num_groups();
  QStringList headers;
 
  if (!is_blank)
  {
    for (int i = 0; i < current_num_groups; i++)
    {
      headers.append(table_main_groups.at(i));
      //table_main->verticalHeaderItem(i)->setTextAlignment(Qt::AlignVCenter);
    }
    
    if (num_table_rows > current_num_groups)
    {
      headers.append("Exhaust");
      //table_main->verticalHeaderItem(current_num_groups)->setTextAlignment(Qt::AlignVCenter);
    }
    
    table_main_model->setVerticalHeaderLabels(headers);
    table_main->verticalHeader()->show();
  }
  else
  {
    table_main->verticalHeader()->hide();
    for (int i = 0; i < num_table_rows; i++)
    {
      headers.append("");
      //table_main->verticalHeaderItem(i)->setTextAlignment(Qt::AlignVCenter);
    }
    table_main_model->setVerticalHeaderLabels(headers);
  }
}

void Widget::process_thread_sql_main_table(const QVector<Table_main_item> &col_data)
{
  if (booth_calculation)
  {
    // Rows and columns will be transposed at output; for now, they are
    // treated as though this is a column of the main table.
    int num_rows = col_data.length();
    int num_cols = col_data.at(0).votes.length();
    
    if (temp_booths_table_data.length() == 0)
    {
      temp_booths_table_data = QVector<Table_main_item>();
      
      for (int i = 0; i < num_rows; i++)
      {
        temp_booths_table_data.append(Table_main_item());
        temp_booths_table_data[i].group_id = i;
        temp_booths_table_data[i].sorted_idx = i;
        temp_booths_table_data[i].votes = QVector<long>();
        
        for (int j = 0; j < num_cols; j++)
        {
          temp_booths_table_data[i].votes.append(0);
        }
      }
    }
    
    for (int i = 0; i < num_rows; i++)
    {
      for (int j = 0; j < num_cols; j++)
      {
        temp_booths_table_data[i].votes[j] += col_data.at(i).votes.at(j);
      }
    }
    
    completed_threads++;
    
    if (completed_threads == current_threads)
    {
      QFile out_file(booths_output_file);
      
      int num_booths = booths.length();
      if (num_booths != temp_booths_table_data.at(0).votes.length())
      {
        QMessageBox msgBox;
        msgBox.setText("Internal error: wrong number of booths in output.");
        msgBox.exec();
        unlock_main_interface();
        return;
      }
      
      int num_input_cols = temp_booths_table_data.length();
      if (num_input_cols != table_main_groups_short.length() + 1 &&
          num_input_cols != table_main_groups_short.length() + 2)
      {
        QMessageBox msgBox;
        msgBox.setText("Internal error: wrong number of groups in output.");
        msgBox.exec();
        unlock_main_interface();
        return;
      }
      
      bool sum_base = (num_input_cols == table_main_groups_short.length() + 1);
      int num_output_cols = table_main_groups_short.length() + 1;
      
      if (out_file.open(QIODevice::WriteOnly))
      {
        QTextStream out(&out_file);
        
        QString value_type = get_value_type();
        QString table_type = get_table_type();
        
        set_title_from_divisions_table();
        QString title_line = QString("\"%1\"").arg(divisions_cross_table_title);
        
        out << title_line << endl;
        
        QString header("Division,Booth,Longitude,Latitude,Total,Base");
        
        for (int i = 0; i < table_main_groups_short.length(); i++)
        {
          header = QString("%1,%2").arg(header).arg(get_short_group(i));
        }
        header = QString("%1,Exh").arg(header);
        
        out << header << endl;
        
        for (int i = 0; i < num_booths; i++)
        {
          QString line = QString("%1,\"%2\",%3,%4,%5")
              .arg(booths.at(i).division).arg(booths.at(i).booth)
              .arg(booths.at(i).longitude).arg(booths.at(i).latitude)
              .arg(booths.at(i).formal_votes);
          
          long base = 0;
          
          if (clicked_cells.length() == 1)
          {
            base = booths.at(i).formal_votes;
          }
          else
          {
            if (sum_base)
            {
              for (int j = 0; j < num_output_cols; j++)
              {
                base += temp_booths_table_data.at(j).votes.at(i);
              }
            }
            else
            {
              base = temp_booths_table_data.at(num_output_cols).votes.at(i);
            }
          }
          
          if (value_type == "votes")
          {
            line = QString("%1,%2").arg(line).arg(base);
          }
          else
          {
            double val = booths.at(i).formal_votes == 0 ? 0. : 100. * base / booths.at(i).formal_votes;
            line = QString("%1,%2").arg(line).arg(val, 0, 'f', 2);
          }
          
          for (int j = 0; j < num_output_cols; j++)
          {
            if (value_type == "votes")
            {
              line = QString("%1,%2").arg(line).arg(temp_booths_table_data.at(j).votes.at(i));
            }
            else if (value_type == "percentages")
            {
              double val = base == 0 ? 0. : 100. * temp_booths_table_data.at(j).votes.at(i) / base;
              line = QString("%1,%2").arg(line).arg(val, 0, 'f', 2);
            }
            else if (value_type == "total_percentages")
            {
              double val = booths.at(i).formal_votes == 0 ? 0. : 100. * temp_booths_table_data.at(j).votes.at(i) / booths.at(i).formal_votes;
              line = QString("%1,%2").arg(line).arg(val, 0, 'f', 2);
            }
          }
          
          out << line << endl;
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
      
      label_progress->setText("Calculation done");
      unlock_main_interface();
    }
  }
  else
  {
    // Not a booth calculation.
    int col = table_main_data.length() - 1;
    
    for (int i = 0; i < num_table_rows; i++)
    {
      for (int k = 0; k < col_data.at(i).votes.length(); k++)
      {
        table_main_data[col][i].votes[k] += col_data.at(i).votes.at(k);
      }
    }
    
    completed_threads++;
    
    if (completed_threads == current_threads)
    {
      // Sum the division votes to get the state totals:
      for (int i = 0; i < table_main_data.at(col).length(); i++)
      {
        long state_votes = 0;
        for (int k = 0; k < table_main_data.at(col).at(i).votes.length(); k++)
        {
          state_votes += table_main_data.at(col).at(i).votes.at(k);
        }
        
        table_main_data[col][i].votes.replace(divisions.length(), state_votes);
      }
      
      if (!sort_ballot_order)
      {
        sort_table_column(col);
      }
      
      if (get_table_type() == "n_party_preferred")
      {
        // We should only be here for the initial setup of the NPP table,
        // when we get the primary votes.
        set_main_table_cells(col, true);
        
        int current_num_groups = get_num_groups();
        int h = table_main->fontMetrics().boundingRect("0").height();
        
        for (int i = 0; i < current_num_groups; i++)
        {
          int group_id = table_main_data.at(0).at(i).group_id;
          table_main_model->setItem(i, 1, new QStandardItem(table_main_groups_short.at(group_id)));
          table_main_model->item(i, 1)->setTextAlignment(Qt::AlignCenter);
          
          if (!show_btl_headers)
          {
            table_main->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
            table_main->verticalHeader()->setDefaultSectionSize(h);
          }
        }
      }
      else
      {
        set_main_table_cells(col);
      }
      
      label_progress->setText("Calculation done");
      unlock_main_interface();
    }
    else
    {
      label_progress->setText(QString("%1/%2 complete").arg(completed_threads).arg(current_threads));
    }
  }
}

void Widget::write_sql_to_file(QString q)
{
  QString file_name = QString("%1/last_sql_query.txt")
      .arg(QCoreApplication::applicationDirPath());
  
  QFile file(file_name);
  
  if (file.open(QIODevice::WriteOnly))
  {
    QTextStream out(&file);
    out << q << endl;
    file.close();
  }
}

void Widget::do_sql_query_for_table(QString q, bool wide_table, bool by_booth)
{
  // If wide_table is true, then the SQL query q should return a table with
  // one row per division, and one column per group.
  //
  // If wide_table is false, then it should return a three-column table (division_ID, group_ID, votes).
  
  write_sql_to_file(q);
  
  // I'm not sure if clicked_cells is ever longer than wanted, but just in case:
  QVector<int> relevant_clicked_cells;
  int rel = by_booth ? 1 : 0;
  
  for (int i = 0; i < table_main_data.length() - 1 - rel; i++)
  {
    relevant_clicked_cells.append(clicked_cells.at(i));
  }
  
  int current_num_groups = get_num_groups();
  
  int num_geo_groups = by_booth ? booths.length() : divisions.length();
  
  int num_threads;
  QStringList queries = queries_threaded(q, num_threads);
  
  current_threads = num_threads;
  completed_threads = 0;
  booth_calculation = by_booth;
  
  if (by_booth)
  {
    temp_booths_table_data.clear();
  }
  
  lock_main_interface();
  label_progress->setText("Calculating...");
  
  for (int i = 0; i < num_threads; i++)
  {
    QThread *thread = new QThread;
    Worker_sql_main_table *worker = new Worker_sql_main_table(i,
                                                              database_file_path,
                                                              queries.at(i),
                                                              wide_table,
                                                              wide_table && by_booth,
                                                              current_num_groups,
                                                              num_table_rows,
                                                              num_geo_groups,
                                                              relevant_clicked_cells);
    worker->moveToThread(thread);
    
    connect(thread, SIGNAL(started()),                                        worker, SLOT(do_query()));
    connect(worker, SIGNAL(finished_query(const QVector<Table_main_item> &)), this,   SLOT(process_thread_sql_main_table(const QVector<Table_main_item> &)));
    connect(worker, SIGNAL(finished_query(const QVector<Table_main_item> &)), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished_query(const QVector<Table_main_item> &)), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()),                                       thread, SLOT(deleteLater()));
    thread->start();
  }
}

QStringList Widget::queries_threaded(QString q, int &num_threads, bool one_thread)
{
  QString abtl = get_abtl();
  
  long max_record = abtl == "atl" ? total_atl_votes : total_btl_votes;
  max_record--;
  
  // *** Does this need changing?  Should multithreading not be used sometimes? ***
  num_threads = max_record > 10000 ? ideal_threads : 1;
  
  if (one_thread) { num_threads = 1; }
  
  QStringList queries;
  if (num_threads == 1)
  {
    queries.append(q);
  }
  else
  {
    bool q_has_where = q.contains("WHERE");
    
    for (int i = 0; i < num_threads; i++)
    {
      long id_1 = (i == 0) ? 0 : max_record * i / num_threads + 1;
      long id_2 = max_record * (i + 1) / num_threads;
      
      QString where_clause = QString("(id BETWEEN %1 AND %2)").arg(id_1).arg(id_2);
      
      queries.append(q);
      
      if (q_has_where)
      {
        queries[i].replace(QString("WHERE"), QString("WHERE %1 AND ").arg(where_clause));
      }
      else
      {
        queries[i].replace(QString("FROM %1").arg(abtl), QString("FROM %1 WHERE %2 ").arg(abtl).arg(where_clause));
      }
    }
  }
  
  return queries;
}

void Widget::sort_table_column(int i)
{
  QVector<int> indices;
  for (int j = 0; j < table_main_data.at(i).length(); j++)
  {
    indices.append(j);
  }
  
  if (sort_ballot_order)
  {
    std::sort(table_main_data[i].begin(), table_main_data[i].end(),
              [&](Table_main_item a, Table_main_item b)->bool {
      return a.group_id < b.group_id;
    });
  }
  else
  {
    int current_div = get_current_division();
    
    std::sort(table_main_data[i].begin(), table_main_data[i].end(),
              [&](Table_main_item a, Table_main_item b)->bool {
      if (a.votes.at(current_div) == b.votes.at(current_div))
      {
        return a.group_id < b.group_id;
      }
      return a.votes.at(current_div) > b.votes.at(current_div);
    });
  }
  
  // Get the indices:
  std::sort(indices.begin(), indices.end(),
            [&](int a, int b)->bool {
    return table_main_data.at(i).at(a).group_id < table_main_data.at(i).at(b).group_id;
  });
  
  int current_num_groups = get_num_groups();
  bool doing_btl = (get_abtl() == "btl");
  
  for (int j = 0; j < table_main_data.at(i).length(); j++)
  {
    table_main_data[i][j].sorted_idx = indices[j];
  }
  
  if (i == 0 && doing_btl)
  {
    // Update the row headers
    for (int j = 0; j < table_main_data.at(i).length(); j++)
    {
      QString header;
      if (table_main_data.at(0).at(j).group_id == current_num_groups)
      {
        header = "Exhaust";
      }
      else
      {
        header = table_main_groups.at(table_main_data.at(0).at(j).group_id);
      }
      
      table_main_model->setVerticalHeaderItem(j, new QStandardItem(header));
    }
  }
}


void Widget::sort_main_table_npp()
{
  int num_table_cols = table_main_data.length();
  int current_num_groups = get_num_groups();
  
  if (sort_npp.i == 1)
  {
    // Sort by group
    for (int j = 0; j < num_table_cols; j++) {
      std::sort(table_main_data[j].begin(), table_main_data[j].end(),
                [&](Table_main_item a, Table_main_item b)->bool {
        return (a.group_id < b.group_id);
      });
      
      for (int i = 0; i < current_num_groups; i++)
      {
        table_main_data[j][i].sorted_idx = i;
      }
    }
  }
  else
  {
    int i = (sort_npp.i == 0) ? 0 : sort_npp.i - 1; 
    int current_div = get_current_division();
    
    QVector<int> indices;
    
    for (int j = 0; j < current_num_groups; j++)
    {
      indices.append(j);
    }
    
    if (get_value_type() == "percentages" && i > 0)
    {
      std::sort(table_main_data[i].begin(), table_main_data[i].end(),
                [&](Table_main_item a, Table_main_item b)->bool {
        if (clicked_n_parties.indexOf(a.group_id) >= 0) { return false; }
        if (clicked_n_parties.indexOf(b.group_id) >= 0) { return true;  }
        
        if (qAbs(a.percentages.at(current_div) - b.percentages.at(current_div)) < 1.e-10)
        {
          return (a.group_id < b.group_id) != sort_npp.is_descending;
        }
        
        return (a.percentages.at(current_div) < b.percentages.at(current_div)) != sort_npp.is_descending;
      });
    }
    else
    {
      std::sort(table_main_data[i].begin(), table_main_data[i].end(),
                [&](Table_main_item a, Table_main_item b)->bool {
        if (clicked_n_parties.indexOf(a.group_id) >= 0) { return false; }
        if (clicked_n_parties.indexOf(b.group_id) >= 0) { return true;  }
        
        if (a.votes.at(current_div) == b.votes.at(current_div))
        {
          return (a.group_id < b.group_id) != sort_npp.is_descending;
        }
        
        return (a.votes.at(current_div) < b.votes.at(current_div)) != sort_npp.is_descending;
      });
    }
    
    std::sort(indices.begin(), indices.end(),
              [&](int a, int b)->bool {
      return table_main_data.at(i).at(a).group_id < table_main_data.at(i).at(b).group_id;
    });
    
    for (int j = 0; j < num_table_cols; j++)
    {
      if (j != i)
      {
        std::sort(table_main_data[j].begin(), table_main_data[j].end(),
                  [&](Table_main_item a, Table_main_item b)->bool {
          return indices[a.group_id] < indices[b.group_id];
        });
      }
      
      for (int k = 0; k < current_num_groups; k++)
      {
        table_main_data[j][k].sorted_idx = indices[k];
      }
    }
  }
  
  set_all_main_table_cells();
  for (int i = 0; i < current_num_groups; i++)
  {
    table_main_model->setItem(i, 1, new QStandardItem(table_main_groups_short.at(table_main_data.at(0).at(i).group_id)));
    table_main_model->item(i, 1)->setTextAlignment(Qt::AlignCenter);
  }
  
  if (get_abtl() == "btl")
  {
    for (int i = 0; i < num_table_rows; i++)
    {
      table_main_model->setVerticalHeaderItem(i, new QStandardItem(table_main_groups.at(table_main_data.at(0).at(i).group_id)));
    }
  }
  
  // Cell highlights/fades
  if (clicked_n_parties.length() > 0)
  {
    for (int j = 0; j < current_num_groups; j++)
    {
      if (clicked_n_parties.indexOf(table_main_data.at(0).at(j).group_id) >= 0)
      {
        highlight_cell_n_party_preferred(j, 1);
      }
      else
      {
        unhighlight_cell(j, 1);
      }
    }
  }
  
  if (clicked_cells_n_party.length() == 1)
  {
    // Following line is to handle exhaust, which might be coded as 999:
    int clicked_i = qMin(clicked_cells_n_party.at(0).i, current_num_groups);
    
    for (int i = 1; i < table_main_data.length(); i++)
    {
      for (int j = 0; j < current_num_groups; j++)
      {
        if (                    clicked_i == table_main_data.at(i).at(j).group_id &&
            clicked_cells_n_party.at(0).j == i + 1)
        {
          highlight_cell(j, i + 1);
        }
        else
        {
          fade_cell(j, i + 1);
        }
      }
    }
  }
}


void Widget::set_all_main_table_cells()
{
  bool n_party = get_table_type() == "n_party_preferred";
  for (int i = 0; i < table_main_data.length(); i++)
  {
    set_main_table_cells(i, n_party);
  }
}


void Widget::set_main_table_cells(int col, bool n_party_preferred)
{
  // n_party_preferred tables have group abbreviations in the second
  // column and in no other cells.
  
  int current_div = get_current_division();
  QString table_type = get_table_type();
  QString value_type = get_value_type();
  
  int current_cols = table_main_model->columnCount();
  
  if (current_cols == col)
  {
    QString header;
    if (table_type == "step_forward")
    {
      header = QString("Pref %1").arg(col + 1);
    }
    else if (table_type == "first_n_prefs")
    {
      header = QString("By %1").arg(get_n_first_prefs());
    }
    else if (table_type == "later_prefs")
    {
      int fixed = get_later_prefs_n_fixed();
      int up_to = get_later_prefs_n_up_to();
      
      if (col + 1 <= fixed)
      {
        header = QString("Pref %1").arg(col + 1);
      }
      else
      {
        header = QString("By %1").arg(up_to);
      }
    }
    // There should be no other cases: pref_sources is two-column only,
    // and NPP columns are added by an NPP routine.
    
    table_main_model->setColumnCount(current_cols + 1);
    table_main_model->setHorizontalHeaderItem(col, new QStandardItem(header));
  }
  
  long percentage_denominator = 1;
  if (value_type == "percentages")
  {
    if (col == 0)
    {
      percentage_denominator = division_formal_votes.at(current_div);
    }
    else
    {
      if (!n_party_preferred)
      {
        if (clicked_cells.length() >= col)
        {
          int idx = table_main_data.at(col - 1).at(clicked_cells.at(col - 1)).sorted_idx;
          percentage_denominator = table_main_data.at(col - 1).at(idx).votes.at(current_div);
          if (percentage_denominator < 1) { percentage_denominator = 1; }
        }
        else
        {
          QMessageBox msgBox;
          msgBox.setText("Internal error: Mismatch between table data length and clicked cells.  Oops. :(");
          msgBox.exec();
          return;
        }
      }
      else
      {
        // With NPP, the denominator is different on each row.
      }
    }
  }
  else if (value_type == "total_percentages")
  {
    percentage_denominator = division_formal_votes.at(current_div);
  }
  
  int table_col = col;
  if (n_party_preferred && col > 0) { table_col++; }
  
  for (int i = 0; i < table_main_data.at(col).length(); i++)
  {
    long votes = table_main_data.at(col)[i].votes.at(current_div);
    
    QString cell_text;
    if (votes == 0)
    {
      if ((table_type == "step_forward" || table_type == "later_prefs") &&
          col == 0 &&
          table_main_data.at(col).at(i).group_id >= get_num_groups())
      {
        cell_text = "Exh\n0";
      }
      else
      {
        cell_text = "";
      }
    }
    else
    {
      if (!n_party_preferred)
      {
        QString gp = get_short_group(table_main_data.at(col).at(i).group_id);
        
        if (value_type == "votes")
        {
          cell_text = QString("%1\n%2").arg(gp).arg(votes);
        }
        else
        {
          double vote_percentage = 100. * votes/static_cast<double>(percentage_denominator);
          cell_text = QString("%1\n%2").arg(gp).arg(vote_percentage, 0, 'f', 2);
        }
      }
      else
      {
        if (value_type == "votes")
        {
          cell_text = QString("%1").arg(votes);
        }
        else
        {
          double vote_percentage;
          if (value_type == "total_percentages" || col == 0)
          {
            vote_percentage = 100. * votes/static_cast<double>(division_formal_votes.at(current_div));
          }
          else
          {
            percentage_denominator = table_main_data.at(0).at(i).votes.at(current_div);
            if (percentage_denominator < 1) { percentage_denominator = 1; }
            vote_percentage = 100. * votes/static_cast<double>(percentage_denominator);
          }
          cell_text = QString("%1").arg(vote_percentage, 0, 'f', 2);
        }
      }
    }
    
    table_main_model->setItem(i, table_col, new QStandardItem(cell_text));
    
    if (table_type == "n_party_preferred")
    {
      table_main_model->item(i, table_col)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    else
    {
      table_main_model->item(i, table_col)->setTextAlignment(Qt::AlignCenter);
    }
    
    if (cell_text == "")
    {
      unhighlight_cell(i, table_col);
    }
    else
    {
      if (!n_party_preferred                                           &&
          col < clicked_cells.length()                                 &&
          clicked_cells.at(col) == table_main_data.at(col).at(i).group_id)
      {
        highlight_cell(i, col);
      }
    }
  }
  
  set_main_table_row_height();
}


void Widget::set_main_table_row_height()
{
  bool btl = get_abtl() == "btl";
  //int buffer = (btl && show_btl_headers) ? 3 : 0;
  
  if (get_table_type() == "n_party_preferred" &&
      (!btl || (btl && !show_btl_headers)))
  {
    table_main->verticalHeader()->setDefaultSectionSize(one_line_height);
  }
  else
  {
    table_main->verticalHeader()->setDefaultSectionSize(two_line_height);
  }
  
}

void Widget::add_column_to_main_table(bool by_booth)
{
  // For table types that aren't n_party_preferred, this function does what it says
  // and adds a column to the main table.
  //
  // For n_party_preferred, this function should only be called to initialise the table,
  // and it adds the first column (primary votes) and the second column (group names).
  
  button_calculate_after_spinbox->setEnabled(false);
  
  QString table_type = get_table_type();
  int num_clicked_cells = clicked_cells.length();
  
  QString geo_group = by_booth ? "booth_id" : "seat_id";
  
  if (!by_booth && table_type != "n_party_preferred" && num_clicked_cells != table_main_data.length())
  {
    QMessageBox msgBox;
    msgBox.setText("Internal error: Mismatch between table data length and clicked cells.  Oops. :(");
    msgBox.exec();
    return;
  }
  
  
  int col;
  
  if (by_booth)
  {
    // For the booth cross table, we're simulating clicking the column again.
    col = table_main_data.length() - 2;
    
    if ((table_type == "step_forward"  && num_clicked_cells == table_main_groups_short.length()) ||
        (table_type == "first_n_prefs" && num_clicked_cells == get_n_first_prefs())              ||
        (table_type == "later_prefs"   && num_clicked_cells == get_later_prefs_n_up_to())        ||
        (table_type == "pref_sources"  && num_clicked_cells == 2))
    {
      // Right-edge of table; the cross table is defined from one column earlier.
      col++;
    }
  }  
  else
  {
    // Initialise a new column of the table data:
    table_main_data.append(QVector<Table_main_item>());
    col = table_main_data.length() - 1;
    
    for (int i = 0; i < num_table_rows; i++)
    {
      table_main_data[col].append(Table_main_item());
      table_main_data[col][i].group_id = i;
      table_main_data[col][i].sorted_idx = i;
      table_main_data[col][i].votes = QVector<long>();
      
      for (int j = 0; j <= divisions.length(); j++)
      {
        table_main_data[col][i].votes.append(0);
      }
    }
  }
  
  
  if (table_type == "step_forward")
  {
    QString query_where("");
    
    int this_pref = col + 1;
    if (this_pref > 1)
    {
      query_where = "WHERE";
      QString and_str("");
      for (int i = 1; i < this_pref; i++)
      {
        if (i == 2) { and_str = " AND "; }
        query_where = QString("%1 %2 P%3 = %4").arg(query_where).arg(and_str).arg(i).arg(clicked_cells.at(i - 1));
      }
    }
    
    QString query = QString("SELECT %1, P%2, COUNT(P%2) FROM %3 %4 GROUP BY %1, P%2")
        .arg(geo_group).arg(this_pref).arg(get_abtl()).arg(query_where);
    
    do_sql_query_for_table(query, false, by_booth);
  }
  else if (table_type == "first_n_prefs")
  {
    QString query_where("");
    
    int current_num_groups = get_num_groups();
    int by_pref = get_n_first_prefs();
    int num_terms = col + 1;
    if (num_terms > 1)
    {
      query_where = "WHERE";
      QString and_str("");
      for (int i = 1; i < num_terms; i++)
      {
        if (i == 2) { and_str = " AND "; }
        int gp = clicked_cells.at(i - 1);
        
        QString cond;
        if (gp < current_num_groups)
        {
          cond = QString("Pfor%1 <= %2").arg(gp).arg(by_pref);
        }
        else
        {
          cond = QString("num_prefs < %1").arg(by_pref);
        }
        
        query_where = QString("%1 %2 %3").arg(query_where).arg(and_str).arg(cond);
      }
    }
    
    QString query = QString("SELECT %1").arg(geo_group);
    for (int i = 0; i < current_num_groups; i++)
    {
      query += QString(", SUM(Pfor%1 <= %2)").arg(i).arg(by_pref);
    }
    query += QString(", SUM(num_prefs < %1)").arg(by_pref);
    
    query += QString(", COUNT(id) FROM %1 %2 GROUP BY %3").arg(get_abtl()).arg(query_where).arg(geo_group);
    
    do_sql_query_for_table(query, true, by_booth);
  }
  else if (table_type == "later_prefs")
  {
    int fixed_prefs = get_later_prefs_n_fixed();
    int by_pref = get_later_prefs_n_up_to();
    
    if (col < fixed_prefs)
    {
      // This is step_forward.
      QString query_where("");
      
      int this_pref = col + 1;
      if (this_pref > 1)
      {
        query_where = " WHERE";
        QString and_str("");
        for (int i = 1; i < this_pref; i++)
        {
          if (i == 2) { and_str = " AND "; }
          query_where = QString("%1 %2 P%3 = %4").arg(query_where).arg(and_str).arg(i).arg(clicked_cells.at(i - 1));
        }
      }
      
      QString query = QString("SELECT %1, P%2, COUNT(P%2) FROM %3 %4 GROUP BY %1, P%2")
          .arg(geo_group).arg(this_pref).arg(get_abtl()).arg(query_where);
      
      do_sql_query_for_table(query, false, by_booth);
    }
    else
    {
      // This is first_n_prefs with a different WHERE expression.
      // There are two parts to the WHERE: the step-forward part and the later-prefs part.
      QString query_where("WHERE");
      QString and_str("");
      int current_num_groups = get_num_groups();
      
      // Step-forward-style WHERE clause:
      for (int i = 1; i <= fixed_prefs; i++)
      {
        if (i == 2) { and_str = " AND "; }
        query_where = QString("%1 %2 P%3 = %4").arg(query_where).arg(and_str).arg(i).arg(clicked_cells.at(i - 1));
      }
      
      // first-n-prefs-style WHERE clause (possibly empty):
      for (int i = fixed_prefs; i < col; i++)
      {
        //query_where = QString("%1 AND Pfor%2 <= %3").arg(query_where).arg(clicked_cells.at(i)).arg(by_pref);
        
        int gp = clicked_cells.at(i);
        
        QString cond;
        if (gp < current_num_groups)
        {
          cond = QString("Pfor%1 <= %2").arg(gp).arg(by_pref);
        }
        else
        {
          cond = QString("num_prefs < %1").arg(by_pref);
        }
        
        query_where = QString("%1 AND %2").arg(query_where).arg(cond);
      }
      
      QString query = QString("SELECT %1").arg(geo_group);
      for (int i = 0; i < current_num_groups; i++)
      {
        query += QString(", SUM(Pfor%1 <= %2)").arg(i).arg(by_pref);
      }
      query += QString(", SUM(num_prefs < %1)").arg(by_pref);
      
      query += QString(", COUNT(id) FROM %1 %2 GROUP BY %3").arg(get_abtl()).arg(query_where).arg(geo_group);
      
      do_sql_query_for_table(query, true, by_booth);
    }
  }
  else if (table_type == "pref_sources")
  {
    int min_pref = get_pref_sources_min();
    int max_pref = get_pref_sources_max();
    
    if (col == 0)
    {
      QString query = QString("SELECT %1").arg(geo_group);
      int current_num_groups = get_num_groups();
      for (int i = 0; i < current_num_groups; i++)
      {
        query += QString(", SUM(Pfor%1 BETWEEN %2 AND %3)").arg(i).arg(min_pref).arg(max_pref);
      }
      query += QString(", SUM(num_prefs BETWEEN %1 AND %2)").arg(min_pref - 1).arg(max_pref - 1);
      
      query += QString(", COUNT(id) FROM %1 GROUP BY %2").arg(get_abtl()).arg(geo_group);
      
      do_sql_query_for_table(query, true, by_booth);
    }
    else
    {
      QString query;
      if (clicked_cells.at(0) == get_num_groups())
      {
        // Exhaust
        query = QString("SELECT %1, P1, COUNT(P1) FROM %2 WHERE num_prefs BETWEEN %3 and %4 GROUP BY %1, P1")
                  .arg(geo_group).arg(get_abtl()).arg(min_pref - 1).arg(max_pref - 1);
      }
      else
      {
        query = QString("SELECT %1, P1, COUNT(P1) FROM %2 WHERE Pfor%3 BETWEEN %4 and %5 GROUP BY %1, P1")
                  .arg(geo_group).arg(get_abtl()).arg(clicked_cells.at(0)).arg(min_pref).arg(max_pref);
      }      
      
      do_sql_query_for_table(query, false, by_booth);
    }
  }
  else if (table_type == "n_party_preferred")
  {
    QString query = QString("SELECT %1, P1, COUNT(P1) FROM %2 GROUP BY %1, P1").arg(geo_group).arg(get_abtl());
    
    do_sql_query_for_table(query, false, by_booth);
  }
}


void Widget::calculate_n_party_preferred(bool by_booth)
{
  int n = get_n_preferred();
  int num_divs;
  QString geo_group;
  booth_calculation = by_booth;
  
  if (by_booth)
  {
    geo_group = "booth_id";
    num_divs = booths.length();
    temp_booths_npp_data.clear();
  }
  else
  {
    geo_group = "seat_id";
    num_divs = divisions.length();
    
    // Clear the table data, just in case.
    for (int i = table_main_data.length() - 1; i > 0; i--)
    {
      table_main_data.remove(i);
    }
    
    // Initialise the table data:
    for (int i = 0; i <= n; i++)
    {
      table_main_data.append(QVector<Table_main_item>());
      
      for (int j = 0; j < num_table_rows; j++)
      {
        table_main_data[i + 1].append(Table_main_item());
        table_main_data[i + 1][j].group_id = table_main_data.at(0).at(j).group_id;
        table_main_data[i + 1][j].sorted_idx = table_main_data.at(0).at(j).sorted_idx;
        table_main_data[i + 1][j].votes = QVector<long>();
        table_main_data[i + 1][j].percentages = QVector<double>();
        
        for (int k = 0; k <= divisions.length(); k++)
        {
          table_main_data[i + 1][j].votes.append(0);
          table_main_data[i + 1][j].percentages.append(0);
        }
      }
    }
    
    // The exhaust column for the table itself should not yet exist,
    // so make it now.
    table_main_model->setColumnCount(n + 3);
    table_main_model->setHorizontalHeaderItem(n + 2, new QStandardItem("Exh"));
    
    table_main_model->horizontalHeaderItem(n+2)->setTextAlignment(Qt::AlignRight);
  }
  
  
  
  QString q = QString("SELECT %1, P1, COUNT(id)").arg(geo_group);
  
  if (n > 1)
  {
    for (int i = 0; i < n; i++)
    {
      q = QString("%1, (").arg(q);
      QString and_str("");
      for (int j = 0; j < n; j++)
      {
        if (i != j)
        {
          q = QString("%1%2 Pfor%3 < Pfor%4")
              .arg(q).arg(and_str).arg(clicked_n_parties.at(i)).arg(clicked_n_parties.at(j));
          and_str = " AND ";
        }
      }
      q = QString("%1) v%2").arg(q).arg(i);
    }
  }
  else
  {
    q = QString("%1, (Pfor%2 < 999) v0").arg(q).arg(clicked_n_parties.at(0));
  }
  
  
  // Exhaust:
  q = QString("%1, (").arg(q);
  QString and_str("");
  for (int i = 0; i < n; i++)
  {
    q = QString("%1%2 Pfor%3 = 999").arg(q).arg(and_str).arg(clicked_n_parties.at(i));
    and_str = " AND ";
  }
  q = QString("%1) v%2").arg(q).arg(n);
  
  
  q = QString("%1 FROM %2 GROUP BY %3, P1").arg(q).arg(get_abtl()).arg(geo_group);
  
  for (int i = 0; i <= n; i++)
  {
    q = QString("%1, v%2").arg(q).arg(i);
  }
  
  q = QString("%1 HAVING ").arg(q);
  and_str = "";
  
  for (int i = 0; i <= n; i++)
  {
    q = QString("%1%2 v%3=1").arg(q).arg(and_str).arg(i);
    and_str = " OR ";
  }
  
  int num_threads;
  QStringList queries = queries_threaded(q, num_threads);
  
  current_threads = num_threads;
  completed_threads = 0;
  
  write_sql_to_file(q);
  
  label_progress->setText("Calculating...");
  lock_main_interface();
  
  for (int i = 0; i < num_threads; i++)
  {
    QThread *thread = new QThread;
    Worker_sql_npp_table *worker = new Worker_sql_npp_table(i,
                                                            database_file_path,
                                                            queries.at(i),
                                                            get_num_groups(),
                                                            num_divs,
                                                            clicked_n_parties);
    
    worker->moveToThread(thread);
    
    connect(thread, SIGNAL(started()),                                                 worker, SLOT(do_query()));
    connect(worker, SIGNAL(finished_query(const QVector<QVector<Table_main_item>> &)), this,   SLOT(process_thread_sql_npp_table(const QVector<QVector<Table_main_item>> &)));
    connect(worker, SIGNAL(finished_query(const QVector<QVector<Table_main_item>> &)), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished_query(const QVector<QVector<Table_main_item>> &)), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()),                                                thread, SLOT(deleteLater()));
    thread->start();
  }
  
  button_n_party_preferred_calculate->setEnabled(false);
}


void Widget::process_thread_sql_npp_table(const QVector<QVector<Table_main_item>> &table)
{
  int n = table.length() - 1;
  int current_num_groups = get_num_groups();
  int num_divs = table.at(0).at(0).votes.length();
  
  if (booth_calculation)
  {
    if (temp_booths_npp_data.length() == 0)
    {
      for (int i = 0; i <= n; i++)
      {
        temp_booths_npp_data.append(QVector<Table_main_item>());
        
        for (int j = 0; j < current_num_groups; j++)
        {
          temp_booths_npp_data[i].append(Table_main_item());
          temp_booths_npp_data[i][j].group_id = j;
          
          for (int k = 0; k < num_divs; k++)
          {
            temp_booths_npp_data[i][j].votes.append(0);
          }
        }
      }
    }
    
    
    for (int i = 0; i <= n; i++)
    {
      for (int j = 0; j < current_num_groups; j++)
      {
        for (int k = 0; k < num_divs; k++)
        {
          temp_booths_npp_data[i][j].votes[k] += table.at(i).at(j).votes.at(k);
        }
      }
    }
    
    completed_threads++;
    
    if (completed_threads == current_threads)
    {
      QFile out_file(booths_output_file);
      
      QString value_type = get_value_type();
      
      int num_booths = booths.length();
      if (num_booths != temp_booths_npp_data.at(0).at(0).votes.length())
      {
        QMessageBox msgBox;
        msgBox.setText("Internal error: wrong number of booths in output.");
        msgBox.exec();
        unlock_main_interface();
        return;
      }
      
      // I actually have enough data here to dump every group's NPP from every
      // booth to CSV, but for now I'll just work with the selected group.
      int group_id = clicked_cells_n_party.at(0).i;
      
      int num_input_cols = temp_booths_npp_data.length();
      if (num_input_cols != n + 1)
      {
        QMessageBox msgBox;
        msgBox.setText("Internal error: wrong number of groups in output.");
        msgBox.exec();
        unlock_main_interface();
        return;
      }
      
      
      if (out_file.open(QIODevice::WriteOnly))
      {
        QTextStream out(&out_file);
        
        QString title = label_division_table_title->text();
        title.replace("<b>", "");
        title.replace("</b>", "");
        
        out << title << endl;
        
        QString header("Division,Booth,Longitude,Latitude,Total,Base");
        
        for (int i = 0; i < n; i++)
        {
          header = QString("%1,%2").arg(header).arg(table_main_groups_short.at(clicked_n_parties.at(i)));
        }
        header = QString("%1,Exh").arg(header);
        
        out << header << endl;
        
        for (int i = 0; i < num_booths; i++)
        {
          QString line = QString("%1,\"%2\",%3,%4,%5")
              .arg(booths.at(i).division).arg(booths.at(i).booth)
              .arg(booths.at(i).longitude).arg(booths.at(i).latitude)
              .arg(booths.at(i).formal_votes);
          
          long base = 0;
          for (int j = 0; j <= n; j++)
          {
            base += temp_booths_npp_data.at(j).at(group_id).votes.at(i);
          }
          
          if (value_type == "votes")
          {
            line = QString("%1,%2").arg(line).arg(base);
          }
          else
          {
            if (booths.at(i).formal_votes == 0)
            {
              line = QString("%1,0").arg(line);
            }
            else
            {
              line = QString("%1,%2").arg(line).arg(100. * base / booths.at(i).formal_votes, 0, 'f', 2);
            }
          }
          
          for (int j = 0; j <= n; j++)
          {
            if (base == 0)
            {
              line = QString("%1,0").arg(line);
            }
            else
            {
              if (value_type == "votes")
              {
                line = QString("%1,%2").arg(line).arg(temp_booths_npp_data.at(j).at(group_id).votes.at(i));
              }
              else if (value_type == "percentages")
              {
                line = QString("%1,%2").arg(line).arg(100. * temp_booths_npp_data.at(j).at(group_id).votes.at(i) / base, 0, 'f', 2);
              }
              else if (value_type == "total_percentages")
              {
                line = QString("%1,%2").arg(line).arg(100. * temp_booths_npp_data.at(j).at(group_id).votes.at(i) / booths.at(i).formal_votes, 0, 'f', 2);
              }
              
            }
          }
          
          out << line << endl;
        }
        
        out_file.close();
      }
      
      label_progress->setText("Calculation done");
      unlock_main_interface();
    }
    else
    {
      label_progress->setText(QString("%1/%2 complete").arg(completed_threads).arg(current_threads));
    }
  }
  else
  {
    int num_divisions = divisions.length();
    
    for (int i = 0; i <= n; i++)
    {
      for (int j = 0; j < current_num_groups; j++)
      {
        int sorted_i = table_main_data.at(1 + i).at(j).sorted_idx;
        
        for (int k = 0; k < num_divisions; k++)
        {
          table_main_data[1+i][sorted_i].votes[k] += table.at(i).at(j).votes.at(k);
        }
      }
    }
    
    completed_threads++;
    
    if (completed_threads == current_threads)
    {
      // Calculate the percentages
      for (int i = 0; i <= n; i++)
      {
        for (int j = 0; j < current_num_groups; j++)
        {
          for (int k = 0; k < num_divisions; k++)
          {
            long primaries = table_main_data.at(0).at(j).votes.at(k);
            long votes = table_main_data.at(i+1).at(j).votes.at(k);
            table_main_data[1+i][j].percentages[k] = 100. * votes / primaries;
          }
        }
      }
      
      for (int i = 0; i <= n; i++)
      {
        for (int j = 0; j < table_main_data.at(i+1).length(); j++)
        {
          long state_votes = 0;
          for (int k = 0; k < table_main_data.at(i+1).at(j).votes.length(); k++)
          {
            state_votes += table_main_data.at(i+1).at(j).votes.at(k);
          }
          
          table_main_data[i+1][j].votes.replace(divisions.length(), state_votes);
          
          long primaries = table_main_data.at(0).at(j).votes.at(divisions.length());
          table_main_data[i+1][j].percentages.replace(divisions.length(), 100. * state_votes / primaries);
        }
      }
      
      for (int i = 0; i <= n; i++)
      {
        set_main_table_cells(i+1, true);
      }
      
      label_progress->setText("Calculation done");
      unlock_main_interface();
    }
    else
    {
      label_progress->setText(QString("%1/%2 complete").arg(completed_threads).arg(current_threads));
    }
  }
}

void Widget::make_cross_table()
{
  QString table_type = get_table_type();
  QString value_type = get_value_type();
  int num_clicked = clicked_cells.length();
  int current_num_groups = get_num_groups();
  
  QString where_clause("");
  QString and_str("");
  int this_div = get_current_division();
  bool whole_state = (this_div == divisions.length());
  bool have_where = false;
  QString q("");
  QStringList queries;
  int num_threads;
  QVector<int> args; // Passed to the worker function
  
  cross_table_title = QString("%1: ").arg(state_full);
  QString title_given("");
  QString space("");
  
  if (!whole_state)
  {
    where_clause = QString(" WHERE seat_id = %1").arg(this_div);
    and_str = " AND";
    have_where = true;
    cross_table_title = QString("%1: ").arg(divisions.at(this_div));
  }
  
  if (table_type == "step_forward")
  {
    int row_pref = 1;
    
    if (num_clicked > 1)
    {
      if (!have_where)
      {
        where_clause = " WHERE";
        have_where = true;
      }
      
      int max_conditional = num_clicked - 1;
      row_pref = num_clicked;
      
      if (num_clicked == current_num_groups)
      {
        max_conditional--;
        row_pref--;
      }
      
      for (int i = 0; i < max_conditional; i++)
      {
        where_clause = QString("%1%2 P%3 = %4")
            .arg(where_clause).arg(and_str).arg(i+1).arg(clicked_cells.at(i));
        
        and_str = " AND";
        
        QString title_bit = QString("%1 %2").arg(i+1).arg(get_short_group(clicked_cells.at(i)));
        
        if (value_type == "percentages")
        {
          title_given = QString("%1%2%3").arg(title_given).arg(space).arg(title_bit);
          space = " ";
        }
        else
        {
          cross_table_title = QString("%1 %2").arg(cross_table_title).arg(title_bit);
        }
      }
    }
    
    int col_pref = row_pref + 1;
    
    q = QString("SELECT P%1, P%2, COUNT(id) FROM %3%4 GROUP BY P%1, P%2")
        .arg(row_pref).arg(col_pref).arg(get_abtl()).arg(where_clause);
    
    if (value_type == "percentages")
    {
      cross_table_title = QString("%1%2 Column given %3 %4 Row")
          .arg(cross_table_title).arg(col_pref).arg(title_given).arg(row_pref);
    }
    else
    {
      cross_table_title = QString("%1 %2 Row %3 Column")
          .arg(cross_table_title).arg(row_pref).arg(col_pref);
    }
  }
  else if (table_type == "first_n_prefs")
  {
    int n = get_n_first_prefs();
    args.append(n);
    
    cross_table_title = QString("%1In first %2 prefs: ").arg(cross_table_title).arg(n);
    
    if (num_clicked > 1)
    {
      if (!have_where)
      {
        where_clause = " WHERE";
        have_where = true;
      }
      
      int max_conditional = num_clicked - 1;
      if (max_conditional == get_n_first_prefs() - 1)
      {
        max_conditional--;
      }
      
      for (int i = 0; i < max_conditional; i++)
      {
        int gp = clicked_cells.at(i);
        
        QString cond;
        if (gp < current_num_groups)
        {
          cond = QString("Pfor%1 <= %2").arg(gp).arg(n);
        }
        else
        {
          cond = QString("num_prefs < %1").arg(n);
        }
        
        where_clause = QString("%1%2 %3").arg(where_clause).arg(and_str).arg(cond);
        
        
        and_str = " AND";
        args.append(clicked_cells.at(i));
        
        if (value_type == "percentages")
        {
          title_given = QString("%1%2%3")
              .arg(title_given).arg(space).arg(get_short_group(clicked_cells.at(i)));
        }
        else
        {
          cross_table_title = QString("%1%2%3")
              .arg(cross_table_title).arg(space).arg(get_short_group(clicked_cells.at(i)));
        }
        space = ", ";
      }
    }
    
    q = "SELECT";
    QString comma("");
    
    for (int i = 1; i <= n; i++)
    {
      q = QString("%1%2 P%3").arg(q).arg(comma).arg(i);
      comma = ",";
    }
    q = QString("%1, num_prefs FROM %2 %3").arg(q).arg(get_abtl()).arg(where_clause);
    
    if (value_type == "percentages")
    {
      cross_table_title = QString("%1Column given %2%3Row").arg(cross_table_title).arg(title_given).arg(space);
    }
    else
    {
      cross_table_title = QString("%1%2Row, Column").arg(cross_table_title).arg(space);
    }
  }
  else if (table_type == "later_prefs")
  {
    int fixed = get_later_prefs_n_fixed();
    int up_to = get_later_prefs_n_up_to();
    
    args.append(fixed);
    args.append(up_to);
    
    if (num_clicked > 1 && up_to > 2)
    {
      if (!have_where)
      {
        where_clause = " WHERE";
        have_where = true;
      }
      
      int max_conditional = num_clicked - 1;
      if (max_conditional == up_to - 1)
      {
        max_conditional--;
      }
      
      for (int i = 0; i < qMin(fixed, max_conditional); i++)
      {
        where_clause = QString("%1%2 P%3 = %4")
            .arg(where_clause).arg(and_str).arg(i + 1).arg(clicked_cells.at(i));
        and_str = " AND";
        args.append(clicked_cells.at(i));
        
        
        QString title_bit = QString("%1 %2").arg(i+1).arg(get_short_group(clicked_cells.at(i)));
        
        if (value_type == "percentages")
        {
          title_given = QString("%1%2%3").arg(title_given).arg(space).arg(title_bit);
          space = " ";
        }
        else
        {
          cross_table_title = QString("%1 %2").arg(cross_table_title).arg(title_bit);
        }
      }
      
      for (int i = fixed; i < max_conditional; i++)
      {
        int gp = clicked_cells.at(i);
        
        QString cond;
        if (gp < current_num_groups)
        {
          cond = QString("Pfor%1 <= %2").arg(gp).arg(up_to);
        }
        else
        {
          cond = QString("num_prefs < %1").arg(up_to);
        }
        
        where_clause = QString("%1%2 %3").arg(where_clause).arg(and_str).arg(cond);
        
        args.append(clicked_cells.at(i));
        
        space = (i == fixed) ? "; " : ", ";
        
        if (value_type == "percentages")
        {
          title_given = QString("%1%2%3")
              .arg(title_given).arg(space).arg(get_short_group(clicked_cells.at(i)));
        }
        else
        {
          cross_table_title = QString("%1%2%3")
              .arg(cross_table_title).arg(space).arg(get_short_group(clicked_cells.at(i)));
        }
      }
    }
    
    q = "SELECT";
    QString comma("");
    
    for (int i = 1; i <= up_to; i++)
    {
      q = QString("%1%2 P%3").arg(q).arg(comma).arg(i);
      comma = ",";
    }
    q = QString("%1, num_prefs FROM %2 %3").arg(q).arg(get_abtl()).arg(where_clause);
    
    int eff_num_clicked = qMax(1, num_clicked);
    eff_num_clicked = qMin(up_to - 1, eff_num_clicked);
    
    if (eff_num_clicked > fixed)
    {
      // Both row and col are "by n".
      if (value_type == "percentages")
      {
        QString sep = eff_num_clicked == fixed + 1 ? ";" : ",";
        cross_table_title = QString("%1 Column in first %2 prefs, given %3%4 Row in first %2 prefs")
            .arg(cross_table_title).arg(up_to).arg(title_given).arg(sep);
      }
      else
      {
        if (eff_num_clicked == fixed + 1)
        {
          cross_table_title = QString("%1; Row, Column in first %2 prefs").arg(cross_table_title).arg(up_to);
        }
        else
        {
          cross_table_title = QString("%1, Row, Column in first %2 prefs").arg(cross_table_title).arg(up_to);
        }
      }
    }
    else if (eff_num_clicked == fixed)
    {
      // Row is fixed; Col is "by n".
      if (value_type == "percentages")
      {
        cross_table_title = QString("%1 Column in first %2 prefs, given %3 %4 Row")
            .arg(cross_table_title).arg(up_to).arg(title_given).arg(fixed);
      }
      else
      {
        cross_table_title = QString("%1 %2 Row; Column in first %3 prefs")
            .arg(cross_table_title).arg(fixed).arg(up_to);
      }
    }
    else
    {
      // Row and col are fixed.
      int p1 = qMax(1, eff_num_clicked);
      
      if (value_type == "percentages")
      {
        cross_table_title = QString("%1 %2 Column, given %3 %4 Row")
            .arg(cross_table_title).arg(p1 + 1).arg(title_given).arg(p1);
      }
      else
      {
        cross_table_title = QString("%1 %2 Row %3 Column")
            .arg(cross_table_title).arg(p1).arg(p1 + 1);
      }
    }
  }
  else if (table_type == "pref_sources")
  {
    int pref_min = get_pref_sources_min();
    int pref_max = get_pref_sources_max();
    
    args.append(pref_min);
    args.append(pref_max);
    
    q = "SELECT P1";
    
    for (int i = pref_min; i <= pref_max; i++)
    {
      q = QString("%1, P%2").arg(q).arg(i);
    }
    
    q = QString("%1, num_prefs FROM %2%3").arg(q).arg(get_abtl()).arg(where_clause);
    
    QString pref_part;
    if (pref_min == pref_max)
    {
      pref_part = QString("%1").arg(pref_min);
    }
    else
    {
      pref_part = QString("%1-%2").arg(pref_min).arg(pref_max);
    }
    
    if (value_type == "percentages")
    {
      cross_table_title = QString("%1 1 Column given %2 Row")
          .arg(cross_table_title).arg(pref_part);
    }
    else
    {
      cross_table_title = QString("%1 1 Column, %2 Row")
          .arg(cross_table_title).arg(pref_part);
    }
  }
  
  lock_main_interface();
  label_progress->setText("Calculating...");
  
  queries = queries_threaded(q, num_threads, !whole_state);
  current_threads = num_threads;
  completed_threads = 0;
  
  int num_cross_table_rows = get_num_groups() + 1;
  
  init_cross_table_data(num_cross_table_rows);
  
  for (int i = 0; i < num_threads; i++)
  {
    QThread *thread = new QThread;
    Worker_sql_cross_table *worker = new Worker_sql_cross_table(table_type,
                                                                i,
                                                                database_file_path,
                                                                queries.at(i),
                                                                num_cross_table_rows,
                                                                args);
    worker->moveToThread(thread);
    
    connect(thread, SIGNAL(started()),                                      worker, SLOT(do_query()));
    connect(worker, SIGNAL(finished_query(const QVector<QVector<long>> &)), this,   SLOT(process_thread_sql_cross_table(const QVector<QVector<long>> &)));
    connect(worker, SIGNAL(finished_query(const QVector<QVector<long>> &)), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished_query(const QVector<QVector<long>> &)), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()),                                     thread, SLOT(deleteLater()));
    thread->start();
  }
  
  write_sql_to_file(q);
}

void Widget::set_title_from_divisions_table()
{
  // For use by cross tables by division/booth.
  // Also called (woeful design) when copying or exporting
  // a divisions table.
  
  QString value_type = get_value_type();
  QString table_type = get_table_type();
  QString standard_title = label_division_table_title->text();
  int n = clicked_cells.length();
  
  divisions_cross_table_title = "";
  
  if (value_type == "percentages")
  {
    if (table_type == "step_forward")
    {
      QRegularExpression pattern("(.*)<b>([0-9]*) [A-Za-z0-9_]*</b>");
      QRegularExpressionMatch matches = pattern.match(standard_title);
      
      if (matches.hasMatch() && matches.capturedTexts().length() == 3)
      {
        divisions_cross_table_title = QString("%1 Column given %2")
            .arg(matches.captured(2)).arg(matches.captured(1));
      }
    }
    else if (table_type == "first_n_prefs")
    {
      QRegularExpression pattern("(.*)<b>[A-Za-z0-9_]*</b>(.*)");
      QRegularExpressionMatch matches = pattern.match(standard_title);
      if (matches.hasMatch() && matches.capturedTexts().length() == 3)
      {
        divisions_cross_table_title = QString("%1 Column %2")
            .arg(matches.captured(1)).arg(matches.captured(2));
      }
    }
    else if (table_type == "later_prefs")
    {
      if (n <= get_later_prefs_n_fixed())
      {
        // Like step_forward
        QRegularExpression pattern("(.*)<b>([0-9]*) [A-Za-z0-9_]*</b>");
        QRegularExpressionMatch matches = pattern.match(standard_title);
        
        if (matches.hasMatch() && matches.capturedTexts().length() == 3)
        {
          divisions_cross_table_title = QString("%1 Column given %2")
              .arg(matches.captured(2)).arg(matches.captured(1));
        }
      }
      else
      {
        // Like first_n_prefs
        QRegularExpression pattern("(.*)<b>[A-Za-z0-9_]*</b>(.*)");
        QRegularExpressionMatch matches = pattern.match(standard_title);
        if (matches.hasMatch() && matches.capturedTexts().length() == 3)
        {
          divisions_cross_table_title = QString("%1 Column %2")
              .arg(matches.captured(1)).arg(matches.captured(2));
        }
      }
    }
    else if (table_type == "pref_sources")
    {
      int pref_min = get_pref_sources_min();
      int pref_max = get_pref_sources_max();
      if (n == 1)
      {
        if (pref_min == pref_max)
        {
          divisions_cross_table_title = QString("%1 Column").arg(pref_min);
        }
        else
        {
          divisions_cross_table_title = QString("%1-%2 Column").arg(pref_min).arg(pref_max);
        }
      }
      else
      {
        QRegularExpression pattern("<b>1 [A-Za-z0-9_]*</b>(.*)");
        QRegularExpressionMatch matches = pattern.match(standard_title);
        if (matches.hasMatch() && matches.capturedTexts().length() == 2)
        {
          divisions_cross_table_title = QString("1 Column %1")
              .arg(matches.captured(1));
        }
      }
    }
    
    divisions_cross_table_title.replace(QRegularExpression("given *$"), "");
  }
  else
  {
    standard_title.replace("<b>", "");
    standard_title.replace("</b>", "");
    
    if (table_type == "step_forward")
    {
      QRegularExpression pattern("(.*)( [A-Za-z0-9_]*)$");
      QRegularExpressionMatch matches = pattern.match(standard_title);
      if (matches.hasMatch() && matches.capturedTexts().length() == 3)
      {
        divisions_cross_table_title = QString("%1 Column")
            .arg(matches.captured(1));
      }
    }
    else if (table_type == "first_n_prefs")
    {
      QRegularExpression pattern("(.*)( [A-Za-z0-9_]*)$");
      QRegularExpressionMatch matches = pattern.match(standard_title);
      if (matches.hasMatch() && matches.capturedTexts().length() == 3)
      {
        divisions_cross_table_title = QString("%1 Column")
            .arg(matches.captured(1));
      }
    }
    else if (table_type == "later_prefs")
    {
      if (n <= get_later_prefs_n_fixed())
      {
        QRegularExpression pattern("(.*)( [A-Za-z0-9_]*)$");
        QRegularExpressionMatch matches = pattern.match(standard_title);
        if (matches.hasMatch() && matches.capturedTexts().length() == 3)
        {
          divisions_cross_table_title = QString("%1 Column")
              .arg(matches.captured(1));
        }
      }
      else
      {
        QRegularExpression pattern("(.*)( [A-Za-z0-9_]*)( in first [0-9]* prefs)$");
        QRegularExpressionMatch matches = pattern.match(standard_title);
        
        if (matches.hasMatch() && matches.capturedTexts().length() == 4)
        {
          divisions_cross_table_title = QString("%1 Column %2")
              .arg(matches.captured(1)).arg(matches.captured(3));
        }
      }
      
    }
    else if (table_type == "pref_sources")
    {
      if (n == 1)
      {
        standard_title.replace(QRegularExpression("[A-Za-z0-9_]+$"), "Column");
      }
      else
      {
        standard_title.replace(QRegularExpression("[A-Za-z0-9_]*,"), "Column,");
      }
      divisions_cross_table_title = standard_title;
    }
  }
  
  divisions_cross_table_title.replace(QRegularExpression("^ +"), "");
  divisions_cross_table_title.replace(QRegularExpression(" +$"), "");
  divisions_cross_table_title.replace(QRegularExpression(" +"), " ");
}

void Widget::make_divisions_cross_table()
{
  int n = clicked_cells.length();
  if (n == 0) { return; } // Shouldn't happen, divisions column should be blank.
  
  QString value_type = get_value_type();
  QString table_type = get_table_type();
  
  int num_divisions = table_main_data.at(0).at(0).votes.length();
  int num_cols = table_main_data.at(0).length();
  int base_col = n - 2;
  int main_col = n - 1;
  
  QStringList gps;
  for (int i = 0; i < table_main_groups_short.length(); i++)
  {
    gps.append(table_main_groups_short.at(i));
  }
  gps.append("Exh");
  
  QStringList divs;
  for (int i = 0; i < divisions.length(); i++)
  {
    divs.append(divisions.at(i));
  }
  divs.append(state_full);
  
  set_title_from_divisions_table();
  
  if (value_type == "votes")
  {
    QVector<long> base;
    
    if (base_col < 0)
    {
      for (int i = 0; i < num_divisions; i++)
      {
        base.append(division_formal_votes.at(i));
      }
    }
    else
    {
      int idx = table_main_data.at(base_col).at(clicked_cells.at(n - 2)).sorted_idx;
      for (int i = 0; i < num_divisions; i++)
      {
        base.append(table_main_data.at(base_col).at(idx).votes.at(i));
      }
    }
    
    QVector<QVector<long>> table;
    for (int i = 0; i < num_divisions; i++)
    {
      table.append(QVector<long>());
      
      for (int j = 0; j < num_cols; j++)
      {
        int idx = table_main_data.at(main_col).at(j).sorted_idx;
        table[i].append(table_main_data.at(main_col).at(idx).votes.at(i));
      }
    }
    
    Table_window *w = new Table_window(base, divs, gps, table, divisions_cross_table_title, this);
    init_cross_table_window(w);
  }
  else
  {
    QVector<double> base;
    long denominator;
    QVector<long> bases;
    
    if (base_col < 0)
    {
      for (int i = 0; i < num_divisions; i++)
      {
        base.append(100.);
        bases.append(division_formal_votes.at(i));
      }
    }
    else
    {
      long base_denominator;
      int idx_base  = table_main_data.at(base_col).at(clicked_cells.at(n - 2)).sorted_idx;
      
      for (int i = 0; i < num_divisions; i++)
      {
        base_denominator = division_formal_votes.at(i);
        long numerator = table_main_data.at(base_col).at(idx_base).votes.at(i);
        
        if (value_type == "total_percentages")
        {
          bases.append(division_formal_votes.at(i));
        }
        else
        {
          bases.append(numerator);
        }
        
        base.append(100. * numerator / base_denominator);
      }
    }
    
    QVector<QVector<double>> table;
    for (int i = 0; i < num_divisions; i++)
    {
      table.append(QVector<double>());
      
      for (int j = 0; j < num_cols; j++)
      {
        int idx = table_main_data.at(main_col).at(j).sorted_idx;
        denominator = bases.at(i);
        long numerator = table_main_data.at(main_col).at(idx).votes.at(i);
        table[i].append(100. * numerator / denominator);
      }
    }
    
    Table_window *w = new Table_window(base, divs, gps, table, divisions_cross_table_title, this);
    init_cross_table_window(w);
  }
}

void Widget::make_booths_cross_table()
{
  QString latest_out_path = get_output_path("csv");
  
  booths_output_file = QFileDialog::getSaveFileName(this,
                                                    "Save CSV",
                                                    latest_out_path,
                                                    "*.csv");
  
  
  if (booths_output_file == "") { return; }
  
  add_column_to_main_table(true);
  update_output_path(booths_output_file, "csv");
}

void Widget::init_cross_table_data(int n)
{
  cross_table_data.clear();
  for (int i = 0; i < n; i++)
  {
    cross_table_data.append(QVector<long>());
    for (int j = 0; j < n; j++)
    {
      cross_table_data[i].append(0);
    }
  }
}


void Widget::process_thread_sql_cross_table(const QVector<QVector<long>> &table)
{
  int n = table.length();
  
  for (int i = 0; i < n; i++)
  {
    for (int j = 0; j < n; j++)
    {
      cross_table_data[i][j] += table.at(i).at(j);
    }
  }
  
  completed_threads++;
  
  if (completed_threads == current_threads)
  {
    if (table_main_data.at(0).length() != n)
    {
      QMessageBox msgBox;
      msgBox.setText("Internal error: Mismatch between table data length and cross table size.  Oops. :(");
      msgBox.exec();
      unlock_main_interface();
      return;
    }
    
    QString value_type = get_value_type();
    QString table_type = get_table_type();
    int current_div = get_current_division();
    int base_col = clicked_cells.length() - 1;
    if (base_col < 0) { base_col = 0; }    
    
    if (table_type == "step_forward"  && base_col == get_num_groups() - 1)          { base_col--;   }
    if (table_type == "first_n_prefs" && base_col == get_n_first_prefs() - 1)       { base_col--;   }
    if (table_type == "later_prefs"   && base_col == get_later_prefs_n_up_to() - 1) { base_col--;   }
    if (table_type == "pref_sources")                                               { base_col = 0; }
    
    QVector<int> ignore_groups;
    for (int i = 0; i < base_col; i++)
    {
      ignore_groups.append(clicked_cells.at(i));
    }
    
    int current_num_groups = get_num_groups();
    QStringList gps;
    
    for (int i = 0; i < current_num_groups; i++)
    {
      gps.append(table_main_groups_short.at(i));
    }
    gps.append("Exh");
    
    
    if (value_type == "votes")
    {
      QVector<long> base;
      
      for (int i = 0; i < n; i++)
      {
        base.append(0);
      }
      
      for (int i = 0; i < n; i++)
      {
        int gp   = table_main_data[base_col][i].group_id;
        base[gp] = table_main_data[base_col][i].votes.at(current_div);
      }
      
      Table_window *w = new Table_window(base, gps, ignore_groups, cross_table_data, cross_table_title, this);
      init_cross_table_window(w);
    }
    else
    {
      QVector<double> base;
      QVector<QVector<double>> table_data;
      
      for (int i = 0; i < n; i++)
      {
        base.append(0.);
        table_data.append(QVector<double>());
        for (int j = 0; j < n; j++)
        {
          table_data[i].append(0.);
        }
      }
      
      long base_denominator;
      
      base_denominator = division_formal_votes.at(current_div);
      
      for (int i = 0; i < n; i++)
      {
        int gp     = table_main_data[base_col][i].group_id;
        long votes = table_main_data[base_col][i].votes.at(current_div);
        long main_denominator;
        
        base[gp] = 100. * votes / base_denominator;
        
        if (value_type == "total_percentages")
        {
          main_denominator = division_formal_votes.at(current_div);
        }
        else
        {
          main_denominator = votes < 1 ? 1 : votes;
        }
        
        for (int j = 0; j < n; j++)
        {
          table_data[gp][j] = 100. * cross_table_data.at(gp).at(j) / main_denominator;
        }
      }
      
      Table_window *w = new Table_window(base, gps, ignore_groups, table_data, cross_table_title, this);
      init_cross_table_window(w);
    }
    
    label_progress->setText("Calculation done");
    unlock_main_interface();
  }
  else
  {
    label_progress->setText(QString("%1/%2 complete").arg(completed_threads).arg(current_threads));
  }
}

void Widget::init_cross_table_window(Table_window *w)
{
  w->setMinimumSize(QSize(200, 200));
  w->resize(500, 500);
  w->setWindowTitle("Cross table");
  w->show();
}


void Widget::lock_main_interface()
{
  doing_calculation = true;
  button_load->setEnabled(false);
  combo_abtl->setEnabled(false);
  combo_table_type->setEnabled(false);
  combo_value_type->setEnabled(false);
  combo_division->setEnabled(false);
  spinbox_first_n_prefs->setEnabled(false);
  spinbox_pref_sources_max->setEnabled(false);
  spinbox_pref_sources_min->setEnabled(false);
  spinbox_later_prefs_fixed->setEnabled(false);
  spinbox_later_prefs_up_to->setEnabled(false);
  button_copy_main_table->setEnabled(false);
  button_export_main_table->setEnabled(false);
  button_cross_table->setEnabled(false);
  button_abbreviations->setEnabled(false);
  button_divisions_copy->setEnabled(false);
  button_divisions_export->setEnabled(false);
  button_divisions_booths_export->setEnabled(false);
  button_divisions_cross_table->setEnabled(false);
  button_booths_cross_table->setEnabled(false);
  button_map_copy->setEnabled(false);
  button_map_export->setEnabled(false);
}

void Widget::unlock_main_interface()
{
  doing_calculation = false;
  button_load->setEnabled(true);
  combo_abtl->setEnabled(true);
  combo_table_type->setEnabled(true);
  combo_value_type->setEnabled(true);
  combo_division->setEnabled(true);
  spinbox_first_n_prefs->setEnabled(true);
  spinbox_pref_sources_max->setEnabled(true);
  spinbox_pref_sources_min->setEnabled(true);
  spinbox_later_prefs_fixed->setEnabled(true);
  spinbox_later_prefs_up_to->setEnabled(true);
  button_copy_main_table->setEnabled(true);
  button_export_main_table->setEnabled(true);
  button_map_copy->setEnabled(true);
  button_map_export->setEnabled(true);
  
  QString table_type = get_table_type();
  
  if (table_type != "n_party_preferred")
  {
    button_cross_table->setEnabled(true);
    
    if (clicked_cells.length() > 0)
    {
      button_divisions_cross_table->setEnabled(true);
      button_booths_cross_table->setEnabled(true);
    }
  }
  
  if ((table_type != "n_party_preferred" && clicked_cells.length() > 0)   ||
      (table_type == "n_party_preferred" && clicked_cells_n_party.length() > 0))
  {
    button_divisions_copy->setEnabled(true);
    button_divisions_export->setEnabled(true);
  }
  
  if (table_type == "n_party_preferred" && clicked_cells_n_party.length() > 0)
  {
    button_divisions_booths_export->setEnabled(true);
  }
  
  button_abbreviations->setEnabled(true);
}

void Widget::change_abtl(int i)
{
  Q_UNUSED(i);
  
  if (get_abtl() == "atl")
  {
    label_toggle_names->hide();
  }
  else
  {
    label_toggle_names->show();
  }
  
  if (opened_database)
  {
    set_table_groups();
    
    int current_num_groups = get_num_groups();
    spinbox_first_n_prefs->setMaximum(current_num_groups);
    spinbox_later_prefs_up_to->setMaximum(current_num_groups);
    spinbox_pref_sources_max->setMaximum(current_num_groups);
    spinbox_pref_sources_min->setMaximum(get_pref_sources_max());
    spinbox_pref_sources_max->setMinimum(get_pref_sources_min());
  }
  
  clear_divisions_table();
  reset_table();
  if (opened_database)
  {
    add_column_to_main_table();
  }
}

QString Widget::get_groups_table()
{
  if (combo_abtl->currentData() == "atl")
  {
    return "groups";
  }
  else
  {
    return "candidates";
  }
}

void Widget::reset_table()
{
  table_main_data.clear();
  table_main_model->clear();
  clicked_cells.clear();
  clicked_n_parties.clear();
  clicked_cells_n_party.clear();
  
  if (opened_database)
  {
    setup_main_table();
    // *** delete I think ***
    //add_column_to_main_table();
  }
}

void Widget::change_table_type(int i)
{
  Q_UNUSED(i);
  
  QString table_type = get_table_type();
  
  container_first_n_prefs_widgets->hide();
  container_later_prefs_widgets->hide();
  container_pref_sources_widgets->hide();
  container_n_party_preferred_widgets->hide();
  container_copy_main_table->hide();
  if (table_type == "first_n_prefs")     { container_first_n_prefs_widgets->show();     }
  if (table_type == "later_prefs")       { container_later_prefs_widgets->show();       }
  if (table_type == "pref_sources")      { container_pref_sources_widgets->show();      }
  if (table_type == "n_party_preferred") { container_n_party_preferred_widgets->show(); }
  if (table_type == "n_party_preferred") { container_copy_main_table->show(); }
  
  if (table_type == "n_party_preferred" || table_type == "step_forward")
  {
    button_calculate_after_spinbox->hide();
  }
  else
  {
    button_calculate_after_spinbox->show();
    button_calculate_after_spinbox->setEnabled(false);
  }
  
  if (table_type == "n_party_preferred")
  {
    label_sort->setText("<i>Click on column header to sort</i>");
    label_sort->setCursor(Qt::ArrowCursor);
    reset_npp_sort();
    button_cross_table->setEnabled(false);
  }
  else
  {
    label_sort->setText("<i>Toggle sort</i>");
    label_sort->setCursor(Qt::PointingHandCursor);
  }
  
  clear_divisions_table();
  reset_table();
  if (opened_database)
  {
    add_column_to_main_table();
  }
}

void Widget::change_value_type(int i)
{
  Q_UNUSED(i);
  set_all_main_table_cells();
  
  if (get_value_type() == "votes")
  {
    spinbox_map_min->setMaximum(1000000.);
    spinbox_map_max->setMaximum(1000000.);
    spinbox_map_min->setDecimals(0);
    spinbox_map_max->setDecimals(0);
    map_divisions_model.set_decimals(0);
  }
  else
  {
    spinbox_map_min->setMaximum(100.);
    spinbox_map_max->setMaximum(100.);
    spinbox_map_min->setDecimals(1);
    spinbox_map_max->setDecimals(1);
    map_divisions_model.set_decimals(1);
  }
  
  if (table_divisions_data.length() > 0)
  {
    sort_divisions_table_data();
    set_divisions_table_title();
    fill_in_divisions_table();
  }
}

void Widget::change_division(int i)
{
  // When the selected division changes, the only change
  // is to the table.  (*** Unless maybe in future I have booth-level
  // maps ***)
  
  Q_UNUSED(i);
  
  if (doing_calculation) { return; }
  
  if (get_table_type() == "n_party_preferred")
  {
    sort_main_table_npp();
  }
  else
  {
    sort_main_table();
  }
}

void Widget::spinbox_change()
{
  clear_divisions_table();
  reset_table();
  button_cross_table->setEnabled(false);
  if (opened_database)
  {
    button_calculate_after_spinbox->setEnabled(true);
  }
}

void Widget::change_first_n_prefs(int i)
{
  Q_UNUSED(i);
  if (doing_calculation) { return; }
  spinbox_change();
}

void Widget::change_later_prefs_fixed(int i)
{
  Q_UNUSED(i);
  if (doing_calculation) { return; }
  spinbox_later_prefs_up_to->setMinimum(get_later_prefs_n_fixed() + 1);
  spinbox_change();
}

void Widget::change_later_prefs_up_to(int i)
{
  Q_UNUSED(i);
  if (doing_calculation) { return; }
  spinbox_later_prefs_fixed->setMaximum(get_later_prefs_n_up_to() - 1);
  spinbox_change();
}

void Widget::change_pref_sources_min(int i)
{
  Q_UNUSED(i);
  if (doing_calculation) { return; }
  spinbox_pref_sources_max->setMinimum(get_pref_sources_min());
  clear_divisions_table();
  spinbox_change();
}

void Widget::change_pref_sources_max(int i)
{
  Q_UNUSED(i);
  if (doing_calculation) { return; }
  spinbox_pref_sources_min->setMaximum(get_pref_sources_max());
  spinbox_change();
}

void Widget::update_map_scale_minmax()
{
  spinbox_map_min->setMaximum(spinbox_map_max->value());
  spinbox_map_max->setMinimum(spinbox_map_min->value());
}

void Widget::toggle_names()
{
  if (get_abtl() == "atl" || doing_calculation) { return; }
  show_btl_headers = !show_btl_headers;
  
  if (show_btl_headers)
  {
    table_main->verticalHeader()->show();
  }
  else
  {
    table_main->verticalHeader()->hide();
  }
  
  if (get_table_type() == "n_party_preferred")
  {
    set_main_table_row_height();
  }
}


void Widget::toggle_sort()
{
  if (get_table_type() == "n_party_preferred" || doing_calculation) { return; }
  
  sort_ballot_order = !sort_ballot_order;
  sort_main_table();
}


void Widget::sort_main_table()
{
  int num_table_cols = table_main_data.length();
  if (num_table_cols == 0) { return; }
  
  int num_clicked_cells = clicked_cells.length();
  
  for (int i = 0; i < num_table_cols; i++)
  {
    sort_table_column(i);
    set_main_table_cells(i);
    
    if (i < num_clicked_cells)
    {
      for (int j = 0; j < num_table_rows; j++)
      {
        if (table_main_data.at(i).at(j).group_id == clicked_cells.at(i))
        {
          highlight_cell(j, i);
        }
        else
        {
          fade_cell(j, i);
        }
      }
    }
  }
}

void Widget::sort_divisions_table_data()
{
  int i = sort_divisions.i;
  bool desc = sort_divisions.is_descending;
  
  if (i == 0)
  {
    // Sorting by division
    std::sort(table_divisions_data.begin(), table_divisions_data.end(),
              [&](Table_divisions_item a, Table_divisions_item b)->bool {
      return (a.division < b.division) != desc;
    });
  }
  else
  {
    QString value_type = get_value_type();
    // The comparators in here all fall back onto the sort-by-division
    // if the votes are equal.  If this fallback isn't present, then it 
    // seems std::sort() gets confused, because during different
    // comparisons, there may hold a < b and also b < a.  In such a case,
    // the iterator may continue iterating through memory addresses until
    // it leaves the QVector entirely, causing a segfault.
    
    if (value_type == "votes")
    {
      std::sort(table_divisions_data.begin(), table_divisions_data.end(),
                [&](Table_divisions_item a, Table_divisions_item b)->bool {
        if (a.votes.at(i - 1) == b.votes.at(i - 1))
        {
          return (a.division < b.division) != desc;
        }
        return (a.votes.at(i - 1) < b.votes.at(i - 1)) != desc;
      });
    }
    else if (value_type == "percentages")
    {
      std::sort(table_divisions_data.begin(), table_divisions_data.end(),
                [&](Table_divisions_item a, Table_divisions_item b)->bool {
        if (qAbs(a.percentage.at(i - 1) - b.percentage.at(i - 1)) < 1.e-10)
        {
          return (a.division < b.division) != desc;
        }
        return (a.percentage.at(i - 1) < b.percentage.at(i - 1)) != desc;
      });
    }
    else if (value_type == "total_percentages")
    {
      std::sort(table_divisions_data.begin(), table_divisions_data.end(),
                [&](Table_divisions_item a, Table_divisions_item b)->bool {
        if (qAbs(a.total_percentage.at(i - 1) - b.total_percentage.at(i - 1)) < 1.e-10)
        {
          return (a.division < b.division) != desc;
        }
        return (a.total_percentage.at(i - 1) < b.total_percentage.at(i - 1)) != desc;
      });
    }
  }
}

void Widget::reset_npp_sort()
{
  if (sort_ballot_order)
  {
    sort_npp.i = 1;
    sort_npp.is_descending = false;
  }
  else
  {
    sort_npp.i = 0;
    sort_npp.is_descending = true;
  }
}

void Widget::change_npp_sort(int i)
{
  if (doing_calculation)                       { return; }
  if (get_table_type() != "n_party_preferred") { return; }
  if (table_main_data.length() == 1 && i > 1)  { return; }
  
  if (i == sort_npp.i && i != 1)
  {
    sort_npp.is_descending = !sort_npp.is_descending;
  }
  else
  {
    sort_npp.is_descending = (i == 1) ? false : true;
  }
  
  sort_npp.i = i;
  sort_main_table_npp();
}

void Widget::reset_divisions_sort()
{
  if (sort_ballot_order || get_table_type() == "n_party_preferred")
  {
    sort_divisions.i = 0;
    sort_divisions.is_descending = false;
  }
  else
  {
    sort_divisions.i = 1;
    sort_divisions.is_descending = true;
  }
}

void Widget::change_divisions_sort(int i)
{
  if (i == sort_divisions.i)
  {
    sort_divisions.is_descending = !sort_divisions.is_descending;
  }
  else
  {
    sort_divisions.i = i;
    sort_divisions.is_descending = i == 0 ? false : true;
  }
  
  sort_divisions_table_data();
  fill_in_divisions_table();
}


void Widget::set_divisions_table_title()
{
  QString table_type = get_table_type();
  QString value_type = get_value_type();
  int clicked_n = clicked_cells.length();
  
  // ~~~~~ Title ~~~~~
  QString table_title("");
  QString map_title("");
  QString space("");
  QString open_bold, close_bold;
  if (table_type == "step_forward")
  {
    if (value_type == "percentages")
    {
      for (int i = 0; i < clicked_n; i++)
      {
        if (i == clicked_n - 1)
        {
          open_bold = "<b>";
          close_bold = "</b>";
        }
        else
        {
          open_bold = "";
          close_bold = "";
        }
        table_title = QString("%1%2%3%4&nbsp;%5%6")
            .arg(table_title)
            .arg(space)
            .arg(open_bold)
            .arg(i+1)
            .arg(get_short_group(clicked_cells.at(i)))
            .arg(close_bold);
        
        space = " ";
      }
    }
    else
    {
      table_title = "<b>";
      for (int i = 0; i < clicked_n; i++)
      {
        table_title = QString("%1%2%3&nbsp;%4")
            .arg(table_title)
            .arg(space)
            .arg(i+1)
            .arg(get_short_group(clicked_cells.at(i)));
        
        space = " ";
      }
      
      table_title = QString("%1</b>").arg(table_title);
    }
  }
  else if (table_type == "first_n_prefs")
  {
    int n_first = get_n_first_prefs();
    table_title = QString("In first %1 prefs: ").arg(n_first);
    
    if (value_type == "percentages")
    {
      table_title = QString("%1<b>%2</b>")
          .arg(table_title)
          .arg(get_short_group(clicked_cells.at(clicked_n - 1)));
      
      QString prefix = " given";
      for (int i = 0; i < clicked_n - 1; i++)
      {
        table_title = QString("%1%2 %3")
            .arg(table_title)
            .arg(prefix)
            .arg(get_short_group(clicked_cells.at(i)));
        
        prefix = ",";
      }
    }
    else
    {
      table_title = QString("%1<b>").arg(table_title);
      QString prefix = "";
      for (int i = 0; i < clicked_n; i++)
      {
        table_title = QString("%1%2 %3")
            .arg(table_title)
            .arg(prefix)
            .arg(get_short_group(clicked_cells.at(i)));
        
        prefix = ",";
      }
      
      table_title = QString("%1</b>").arg(table_title);
    }
  }
  else if (table_type == "later_prefs")
  {
    int n_fixed = get_later_prefs_n_fixed();
    int n_up_to = get_later_prefs_n_up_to();
    
    if (value_type == "percentages")
    {
      if (clicked_n <= n_fixed)
      {
        // This is just step forward
        
        space = "";
        
        for (int i = 0; i < clicked_n; i++)
        {
          if (i == clicked_n - 1)
          {
            open_bold = "<b>";
            close_bold = "</b>";
          }
          else
          {
            open_bold = "";
            close_bold = "";
          }
          table_title = QString("%1%2%3%4&nbsp;%5%6")
              .arg(table_title)
              .arg(space)
              .arg(open_bold)
              .arg(i+1)
              .arg(get_short_group(clicked_cells.at(i)))
              .arg(close_bold);
          
          space = " ";
        }
      }
      else
      {
        table_title = QString("<b>%1</b> in first %2 prefs, given")
            .arg(get_short_group(clicked_cells.at(clicked_n - 1)))
            .arg(n_up_to);
        
        QString prefix("");
        
        for (int i = 0; i < n_fixed; i++)
        {
          table_title = QString("%1 %2&nbsp;%3")
              .arg(table_title)
              .arg(i+1)
              .arg(get_short_group(clicked_cells.at(i)));
        }
        
        prefix = ";";
        
        for (int i = n_fixed; i < clicked_n - 1; i++)
        {
          table_title = QString("%1%2 %3")
              .arg(table_title)
              .arg(prefix)
              .arg(get_short_group(clicked_cells.at(i)));
          
          prefix = ",";
        }
        
        if (clicked_n > n_fixed + 1)
        {
          table_title = QString("%1 in first %2 prefs")
              .arg(table_title)
              .arg(n_up_to);
        }
      }
    }
    else
    {
      // later_prefs; not percentages
      
      table_title = "<b>";
      space = "";
      
      if (clicked_n <= n_fixed)
      {
        // Step forward
        
        for (int i = 0; i < clicked_n; i++)
        {
          table_title = QString("%1%2%3&nbsp;%4")
              .arg(table_title)
              .arg(space)
              .arg(i+1)
              .arg(get_short_group(clicked_cells.at(i)));
          
          space = " ";
        }
      }
      else
      {
        // First the step-forward part
        
        for (int i = 0; i < n_fixed; i++)
        {
          table_title = QString("%1%2%3&nbsp;%4")
              .arg(table_title)
              .arg(space)
              .arg(i+1)
              .arg(get_short_group(clicked_cells.at(i)));
          
          space = " ";
        }
        
        // The the later prefs.
        QString prefix(";");
        
        for (int i = n_fixed; i < clicked_n; i++)
        {
          table_title = QString("%1%2 %3")
              .arg(table_title)
              .arg(prefix)
              .arg(get_short_group(clicked_cells.at(i)));
          
          prefix = ",";
        }
        
        table_title = QString("%1 in first %2 prefs")
            .arg(table_title)
            .arg(n_up_to);
      }
      
      table_title = QString("%1</b>").arg(table_title);
    }
  }
  else if (table_type == "pref_sources")
  {
    int pref_min = get_pref_sources_min();
    int pref_max = get_pref_sources_max();
    
    QString pref_part;
    
    if (pref_min == pref_max)
    {
      pref_part = QString("%1&nbsp;%2").arg(pref_min).arg(get_short_group(clicked_cells.at(0)));
    }
    else
    {
      pref_part = QString("%1-%2&nbsp;%3")
              .arg(pref_min)
              .arg(pref_max)
              .arg(get_short_group(clicked_cells.at(0)));
    }
    
    
    if (clicked_n == 1)
    {
      table_title = QString("<b>%1</b>").arg(pref_part);
    }
    else
    {
      QString source_part = QString("1&nbsp;%1").arg(get_short_group(clicked_cells.at(1)));
      
      if (value_type == "percentages")
      {
        table_title = QString("<b>%1</b> given %2").arg(source_part).arg(pref_part);
      }
      else
      {
        table_title = QString("<b>%1, %2</b>").arg(source_part).arg(pref_part);
      }
    }
  }
  else if (table_type == "n_party_preferred")
  {
    int n = get_n_preferred();
    QString pc = get_abtl() == "atl" ? "P" : "C";
    
    table_title = QString("<b>%1%2P pref flow from %3</b>")
        .arg(n)
        .arg(pc)
        .arg(get_short_group(clicked_cells_n_party.at(0).i));
    
    map_title = QString("<b>%1%2P pref flow from %3 to %4</b>")
        .arg(n)
        .arg(pc)
        .arg(get_short_group(clicked_cells_n_party.at(0).i))
        .arg(table_main_model->horizontalHeaderItem(clicked_cells_n_party.at(0).j)->text());
  }
  
  label_division_table_title->setText(table_title);
  
  if (table_type != "n_party_preferred")
  {
    map_title = table_title;
  }
  
  label_map_title->setText(map_title);
}

void Widget::fill_in_divisions_table()
{
  QString table_type = get_table_type();
  QString value_type = get_value_type();
  
  int num_rows = table_divisions_data.length();
  int n = 0;
  int num_cols = 2;
  
  if (table_type == "n_party_preferred")
  {
    n = get_n_preferred();
    num_cols = n + 3;
  }
  
  QFont font_bold;
  QFont font_normal;
  font_bold.setBold(true);
  font_normal.setBold(false);
  
  bool bold_row = false;
  
  double min_value = 1.1e10;
  double max_value = 0.;
  
  for (int j = 0; j < num_rows; j++)
  {
    if (table_divisions_data.at(j).division == num_rows - 1)
    {
      table_divisions_model->setItem(j, 0, new QStandardItem(state_full));
      table_divisions_model->item(j, 0)->setFont(font_bold);
      bold_row = true;
    }
    else
    {
      table_divisions_model->setItem(j, 0, new QStandardItem(divisions.at(table_divisions_data.at(j).division)));
      table_divisions_model->item(j, 0)->setFont(font_normal);
      bold_row = false;
    }
    
    for (int i = 1; i < num_cols; i++)
    {
      QString cell_text;
      if (value_type == "votes")             { cell_text = QString("%1").arg(table_divisions_data.at(j).votes.at(i - 1)); }
      if (value_type == "percentages")       { cell_text = QString("%1").arg(table_divisions_data.at(j).percentage.at(i - 1), 0, 'f', 2); }
      if (value_type == "total_percentages") { cell_text = QString("%1").arg(table_divisions_data.at(j).total_percentage.at(i - 1), 0, 'f', 2); }
      
      table_divisions_model->setItem(j, i, new QStandardItem(cell_text));
      table_divisions_model->item(j, i)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      
      if ((table_type != "n_party_preferred" ||
           (table_type == "n_party_preferred" && i == clicked_cells_n_party.at(0).j)) &&
          table_divisions_data.at(j).division != num_rows - 1)
      {
        double val = cell_text.toDouble();
        map_divisions_model.set_value(table_divisions_data.at(j).division, val);
        min_value = qMin(min_value, val);
        max_value = qMax(max_value, val);
      }
      
      if (bold_row)
      {
        table_divisions_model->item(j, i)->setFont(font_bold);
      }
      else
      {
        table_divisions_model->item(j, i)->setFont(font_normal);
      }
    }
  }
  
  spinbox_map_min->setMaximum(max_value);
  spinbox_map_max->setMinimum(min_value);
  spinbox_map_min->setValue(min_value);
  spinbox_map_max->setValue(max_value);
  
  // Following is usually not needed, but is needed if
  // the min/max spinboxes didn't change just now:
  map_divisions_model.set_colors();
  
  map_scale_min_default = min_value;
  map_scale_max_default = max_value;
}

void Widget::set_divisions_table()
{
  QString table_type = get_table_type();
  int num_rows = table_divisions_data.length();
  int n = 0;
  int num_cols = 2;
  
  if (table_type == "n_party_preferred")
  {
    n = get_n_preferred();
    num_cols = n + 3;
  }
  
  set_divisions_table_title();
  
  table_divisions_model->setRowCount(num_rows);
  table_divisions_model->setColumnCount(num_cols);
  
  QStringList headers;
  headers.append("Division");
  
  if (table_type == "n_party_preferred")
  {
    headers.append("Base");
    for (int i = 0; i < n; i++)
    {
      headers.append(table_main_groups_short.at(clicked_n_parties.at(i)));
    }
    headers.append("Exh");
  }
  else
  {
    headers.append("Vote");
  }
  
  table_divisions_model->setHorizontalHeaderLabels(headers);
  
  for (int i = 0; i < table_divisions_model->columnCount(); i++)
  {
    if (i == 0)
    {
      table_divisions_model->horizontalHeaderItem(i)->setTextAlignment(Qt::AlignLeft);
    }
    else
    {
      table_divisions_model->horizontalHeaderItem(i)->setTextAlignment(Qt::AlignRight);
    }
  }
  
  fill_in_divisions_table();
}

void Widget::set_divisions_map()
{
  // *** delete I think ***
}

void Widget::clicked_main_table(const QModelIndex &index)
{
  int i = index.row();
  int j = index.column();
  
  if (doing_calculation)                          { return; }
  if (table_main_model->item(i, j) == nullptr)    { return; }
  if (table_main_model->item(i, j)->text() == "") { return; }
  if (table_main_data.length() == 0)              { return; } // Hopefully not needed, but just in case....
  
  int num_clicked_cells = clicked_cells.length();
  QString table_type = get_table_type();
  
  if (table_type == "n_party_preferred")
  {
    // If clicking on a cell in the first two columns,
    // add the group to the list of clicked_n_parties.
    //
    // If clicking on a cell containing an n-party-preferred
    // number, highlight the cell and fade all others; the map
    // and divisions table will be updated with this number.
    //
    // Unlike elsewhere (***?***), if clicking again on the
    // highlighted cell, then unhighlight and unfade the table.
    
    int data_col = (j == 0) ? 0 : j - 1;
    int clicked_group_id = table_main_data.at(data_col).at(i).group_id;
    
    if (j < 2)
    {
      j = 1;
      
      // First clear all n-party-preferred data.
      for (int s = table_main_data.length() - 1; s > 0; --s)
      {
        table_main_data.remove(s);
      }
      
      table_main_model->setColumnCount(2 + clicked_n_parties.length());
      
      for (int s = 2; s < table_main_model->columnCount(); s++)
      {
        for (int r = 0; r < num_table_rows; ++r)
        {
          table_main_model->setItem(r, s, new QStandardItem(""));
          set_default_cell_style(r, s);
        }
      }
      
      // Remove cell highlights
      clicked_cells_n_party.clear();
      
      clear_divisions_table();
      
      int index_i = clicked_n_parties.indexOf(clicked_group_id);
      if (index_i > -1)
      {
        unhighlight_cell(i, j);
        clicked_n_parties.remove(index_i);
        
        // Remove table column
        table_main_model->takeColumn(index_i + 2);
      }
      else
      {
        clicked_n_parties.append(clicked_group_id);
        highlight_cell_n_party_preferred(i, 1);
        
        // Add a table column
        int num_cols = table_main_model->columnCount();
        table_main_model->setColumnCount(num_cols + 1);
        table_main_model->setHorizontalHeaderItem(num_cols, new QStandardItem(table_main_groups_short.at(clicked_group_id)));
        table_main_model->horizontalHeaderItem(num_cols)->setTextAlignment(Qt::AlignRight);
      }
      
      button_n_party_preferred_calculate->setEnabled(clicked_n_parties.length() > 0);
    }
    else
    {
      if (clicked_cells_n_party.length() == 1                &&
          clicked_cells_n_party.at(0).i  == clicked_group_id && 
          clicked_cells_n_party.at(0).j  == j)
      {
        // Unhighlight
        clicked_cells_n_party.clear();
        
        set_default_cell_style(i, j);
        
        // The following is relevant if I've earlier faded cells.
        // Performance on fading is woeful though, so I'm leaving
        // the text colors as they are.
        /*
        for (int s = 2; s < table_main_model->columnCount(); ++s)
        {
          for (int r = 0; r < num_table_rows; ++r)
          {
            set_default_cell_style(r, s);
          }
        }
        */
      }
      else
      {
        if (clicked_cells_n_party.length() == 1)
        {
          // Unhighlight the previously clicked  cell.
          int prev_i = clicked_cells_n_party.at(0).i;
          int prev_j = clicked_cells_n_party.at(0).j;
          prev_i = table_main_data.at(0).at(prev_i).sorted_idx;
          set_default_cell_style(prev_i, prev_j);
        }
        
        set_clicked_cell_n_party(clicked_group_id, j);
        for (int s = 2; s < table_main_model->columnCount(); ++s)
        {
          for (int r = 0; r < num_table_rows; ++r)
          {
            fade_cell(r, s);
          }
        }
        highlight_cell(i, j);
      }
      
      // Divisions table
      
      if (clicked_cells_n_party.length() == 0)
      {
        clear_divisions_table();
        return;
      }
      else
      {
        table_divisions_data.clear();
        int num_divisions = table_main_data.at(data_col).at(i).votes.length();
        int n = get_n_preferred();
        
        for (int r = 0; r < num_divisions; r++)
        {
          table_divisions_data.append(Table_divisions_item());
          table_divisions_data[r].division = r;
          long total_percentage_denominator = division_formal_votes.at(r);
          long percentage_denominator = table_main_data.at(0).at(i).votes.at(r);
          if (percentage_denominator == 0) { percentage_denominator = 1; }
          
          table_divisions_data[r].votes.append(percentage_denominator);
          
          double base = 100. * static_cast<double>(percentage_denominator) / total_percentage_denominator;
          
          table_divisions_data[r].percentage.append(base);
          table_divisions_data[r].total_percentage.append(base);
          
          for (int s = 0; s <= n; s++)
          {
            long votes = table_main_data.at(s+1).at(i).votes.at(r);
            table_divisions_data[r].votes.append(votes);
            table_divisions_data[r].percentage.append(100. * static_cast<double>(votes) / percentage_denominator);
            table_divisions_data[r].total_percentage.append(100. * static_cast<double>(votes) / total_percentage_denominator);
          }
        }
        
        sort_divisions_table_data();
        set_divisions_table();
        
        button_divisions_copy->setEnabled(true);
        button_divisions_export->setEnabled(true);
        button_divisions_booths_export->setEnabled(true);
      }
    }
  }
  else
  {
    // Table type not n-party-preferred.
    
    int clicked_group_id = table_main_data.at(j).at(i).group_id;
    if (num_clicked_cells > j && clicked_cells.at(j) == clicked_group_id) { return; }
    
    bool exhaust = table_main_data.at(j).at(i).group_id >= get_num_groups();
    
    if (j == 0 && exhaust &&
        (table_type == "step_forward" || table_type == "later_prefs"))
    {
      return;
    }
    
    // - Leave content in columns to the left of j unchanged.
    // - Change the highlight of column j to the clicked cell.
    // - Remove content to the right of j.
    
    for (int s = j; s < clicked_cells.length(); s++)
    {
      // Unhighlight previously-clicked cells from this column
      // and right.
      set_default_cell_style(table_main_data.at(s).at(clicked_cells.at(s)).sorted_idx, s);
    }
    
    for (int r = 0; r < num_table_rows; ++r)
    {
      fade_cell(r, j);
    }
    highlight_cell(i, j);
    
    for (int s = table_main_data.length() - 1; s > j; s--)
    {
      table_main_data.remove(s);
      for (int r = 0; r < num_table_rows; ++r)
      {
        table_main_model->setItem(r, s, new QStandardItem(""));
        set_default_cell_style(r, s);
      }
    }
    
    for (int s = num_clicked_cells - 1; s >= j; s--)
    {
      clicked_cells.remove(s);
    }
    
    clicked_cells.append(clicked_group_id);
    
    
    if (((table_type == "step_forward")  && (j < get_num_groups() - 1)          && !exhaust)  ||
        ((table_type == "first_n_prefs") && (j < get_n_first_prefs() - 1))                    ||
        ((table_type == "later_prefs")   && (j < get_later_prefs_n_up_to() - 1)
                                         && !((j < get_later_prefs_n_fixed())   &&  exhaust)) ||
        ((table_type == "pref_sources")  && (j == 0)))
    {
      add_column_to_main_table();
    }
    
    // Divisions table
    
    table_divisions_data.clear();
    int num_divisions = table_main_data.at(j).at(i).votes.length();
    
    for (int r = 0; r < num_divisions; r++)
    {
      table_divisions_data.append(Table_divisions_item());
      table_divisions_data[r].division = r;
      long votes = table_main_data.at(j).at(i).votes.at(r);
      long total_percentage_denominator = division_formal_votes.at(r);
      long percentage_denominator;
      
      if (j == 0)
      {
        percentage_denominator = total_percentage_denominator;
      }
      else
      {
        int idx = table_main_data.at(j - 1).at(clicked_cells.at(j - 1)).sorted_idx;
        percentage_denominator = table_main_data.at(j - 1).at(idx).votes.at(r);
        if (percentage_denominator == 0) { percentage_denominator = 1; }
      }
      
      table_divisions_data[r].votes.append(votes);
      table_divisions_data[r].percentage.append(100. * static_cast<double>(votes) / percentage_denominator);
      table_divisions_data[r].total_percentage.append(100. * static_cast<double>(votes) / total_percentage_denominator);
    }
    
    sort_divisions_table_data();
    set_divisions_table();
    
    // *** Map ***
  }
}

void Widget::clear_divisions_table()
{
  table_divisions_data.clear();
  table_divisions_model->clear();
  label_division_table_title->setText("<b>No selection</b>");
  
  map_divisions_model.clear_values();
  label_map_title->setText("<b>No selection</b>");
  
  button_divisions_copy->setEnabled(false);
  button_divisions_export->setEnabled(false);
  button_divisions_booths_export->setEnabled(false);
  button_divisions_cross_table->setEnabled(false);
  button_booths_cross_table->setEnabled(false);
  reset_divisions_sort();
}

void Widget::set_clicked_cell_n_party(int i, int j)
{
  if (clicked_cells_n_party.length() == 0)
  {
    N_party_click_cell ij;
    ij.i = i;
    ij.j = j;
    clicked_cells_n_party.append(ij);
  }
  else
  {
    clicked_cells_n_party[0].i = i;
    clicked_cells_n_party[0].j = j;
  }
}

// I have for now commented out the lines that would
// change the text color; these are, remarkably enough,
// a bottleneck in large (below-the-line) tables.

void Widget::set_default_cell_style(int i, int j)
{
  if (table_main_model->item(i, j) == nullptr) { return; }
  table_main_model->item(i, j)->setBackground(QColor(255, 255, 255, 0));
  //table_main_model->item(i, j)->setForeground(get_focused_text_color());
}

void Widget::fade_cell(int i, int j)
{
  if (table_main_model->item(i, j) == nullptr) { return; }
  //table_main_model->item(i, j)->setBackground(QColor(255, 255, 255, 0));
  //table_main_model->item(i, j)->setForeground(get_unfocused_text_color());
}

void Widget::highlight_cell(int i, int j)
{
  if (table_main_model->item(i, j) == nullptr) { return; }
  table_main_model->item(i, j)->setBackground(get_highlight_color());
  //table_main_model->item(i, j)->setForeground(get_focused_text_color());
}

void Widget::highlight_cell_n_party_preferred(int i, int j)
{
  if (table_main_model->item(i, j) == nullptr) { return; }
  table_main_model->item(i, j)->setBackground(get_n_party_preferred_color());
  //table_main_model->item(i, j)->setForeground(get_focused_text_color());
}

void Widget::unhighlight_cell(int i, int j)
{
  if (table_main_model->item(i, j) == nullptr) { return; }
  table_main_model->item(i, j)->setBackground(QColor(255, 255, 255, 0));
}

QString Widget::get_export_line(QStandardItemModel *model, int i, QString separator)
{
  QString text("");
  QString sep("");
  
  if (i == -1)
  {
    // Headers
    for (int j = 0; j < model->columnCount(); j++)
    {
      text = QString("%1%2%3").arg(text).arg(sep).arg(model->horizontalHeaderItem(j)->text());
      if (j == 0) { sep = separator; }
    }
    
    return text;
  }
  else
  {
    for (int j = 0; j < model->columnCount(); j++)
    {
      text = QString("%1%2%3").arg(text).arg(sep).arg(model->item(i, j)->text());
      if (j == 0) { sep = separator; }
    }
    
    return text;
  }
}

void Widget::copy_model(QStandardItemModel *model, QString title)
{
  QString text("");
  QString newline("");
  
  if (title != "")
  {
    text = title;
    newline = "\n";
  }
  
  for (int i = -1; i < model->rowCount(); i++)
  {
    text = QString("%1%2%3").arg(text).arg(newline).arg(get_export_line(model, i, "\t"));
    if (i == -1) { newline = "\n"; }
  }
  
  QApplication::clipboard()->setText(text);
}

QString Widget::get_output_path(QString file_type)
{
  // Read the most recent directory a CSV file was saved to.
  QString last_path_file_name = QString("%1/last_%2_dir.txt")
      .arg(QCoreApplication::applicationDirPath())
      .arg(file_type);
  
  QFileInfo check_exists(last_path_file_name);
  
  QString latest_out_path(QCoreApplication::applicationDirPath());
  
  if (check_exists.exists())
  {
    QFile last_path_file(last_path_file_name);
    
    if (last_path_file.open(QIODevice::ReadOnly))
    {
      QTextStream in(&last_path_file);
      QString last_path = in.readLine();
      
      if (QDir(last_path).exists())
      {
        latest_out_path = last_path;
      }
      
      last_path_file.close();
    }
  }
  
  return latest_out_path;
}

void Widget::update_output_path(QString file_name, QString file_type)
{
  QString last_path_file_name = QString("%1/last_%2_dir.txt")
      .arg(QCoreApplication::applicationDirPath())
      .arg(file_type);
  
  // Update the most recent CSV path:
  QFileInfo info(file_name);
  latest_out_path = info.absolutePath();
  
  QFile last_path_file(last_path_file_name);
  
  if (last_path_file.open(QIODevice::WriteOnly))
  {
    QTextStream out(&last_path_file);
    out << latest_out_path << endl;
    last_path_file.close();
  }
}

void Widget::export_model(QStandardItemModel *model, QString title)
{
  QString latest_out_path = get_output_path("csv");
  
  QString out_file_name = QFileDialog::getSaveFileName(this,
                                                      "Save CSV",
                                                      latest_out_path,
                                                      "*.csv");
  
  if (out_file_name == "") { return; }
  
  QFile out_file(out_file_name);
  
  if (out_file.open(QIODevice::WriteOnly))
  {
    QTextStream out(&out_file);
    
    if (title != "")
    {
      out << title << endl;
    }
    
    for (int i = -1; i < model->rowCount(); i++)
    {
      out << get_export_line(model, i, ",") << endl;
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
  
  update_output_path(out_file_name, "csv");
}

void Widget::copy_main_table()
{
  copy_model(table_main_model);
}

void Widget::export_main_table()
{
  export_model(table_main_model);
}

QString Widget::get_export_divisions_table_title()
{
  QString title("");
  if (clicked_cells.length() > 0) // Should be!
  {
    set_title_from_divisions_table();
    title = divisions_cross_table_title;
    title.replace("Column", get_short_group(clicked_cells.at(clicked_cells.length() - 1)));
  }
  return title;
}

void Widget::copy_divisions_table()
{
  copy_model(table_divisions_model, get_export_divisions_table_title());
}

void Widget::export_divisions_table()
{
  export_model(table_divisions_model, get_export_divisions_table_title());
}

void Widget::reset_map_scale()
{
  spinbox_map_min->setValue(map_scale_min_default);
  spinbox_map_max->setValue(map_scale_max_default);
}

void Widget::zoom_to_state()
{
  if (opened_database && qml_map)
  {
    offset_set_map_center = !offset_set_map_center;
    double lon = 133.;
    double lat = -30.;
    double zoom = 4.;
    
    if (state_short == "QLD")
    {
      lon = 145.;
      lat = -20.3;
      zoom = 5.08;
    }
    else if (state_short == "NSW")
    {
      lon = 147.3;
      lat = -32.8;
      zoom = 5.8;
    }
    else if (state_short == "VIC")
    {
      lon = 145.5;
      lat = -37.;
      zoom = 6.28;
    }
    else if (state_short == "TAS")
    {
      lon = 146.5;
      lat = -41.9;
      zoom = 6.76;
    }
    else if (state_short == "SA")
    {
      lon = 135.;
      lat = -33.;
      zoom = 5.44;
    }
    else if (state_short == "WA")
    {
      lon = 124.;
      lat = -26.2;
      zoom = 4.72;
    }
    else if (state_short == "NT")
    {
      lon = 134.;
      lat = -19.;
      zoom = 5.32;
    }
    else if (state_short == "ACT")
    {
      lon = 149.;
      lat = -35.55;
      zoom = 9.16;
    }
    
    if (offset_set_map_center)
    {
      lon += 1e-6;
      lat += 1e-6;
      zoom += 1e-6;
    }
    
    qml_map->setProperty("longitude", lon);
    qml_map->setProperty("latitude", lat);
    qml_map->setProperty("zoom_level", zoom);
    qml_map_container->update_coords();
  }
}

void Widget::zoom_to_capital()
{
  if (opened_database && qml_map)
  {
    offset_set_map_center = !offset_set_map_center;
    double lon = 133.;
    double lat = -30.;
    double zoom = 4.;
    
    if (state_short == "QLD")
    {
      lon = 152.5;
      lat = -27.3;
      zoom = 8.2;
    }
    else if (state_short == "NSW")
    {
      lon = 150.9;
      lat = -33.6;
      zoom = 8.32;
    }
    else if (state_short == "VIC")
    {
      lon = 145.0;
      lat = -37.8;
      zoom = 9.28;
    }
    else if (state_short == "TAS")
    {
      // (147.2, -42.5), 7.96
      lon = 147.2;
      lat = -42.5;
      zoom = 7.96;
    }
    else if (state_short == "SA")
    {
      lon = 138.65;
      lat = -34.8;
      zoom = 9.04;
    }
    else if (state_short == "WA")
    {
      lon = 116.4;
      lat = -32.0;
      zoom = 9.04;
    }
    else if (state_short == "NT")
    {
      lon = 131.;
      lat = -12.5;
      zoom = 9.64;
    }
    else if (state_short == "ACT")
    {
      lon = 149.;
      lat = -35.55;
      zoom = 9.16;
    }
    
    if (offset_set_map_center)
    {
      lon += 1e-6;
      lat += 1e-6;
      zoom += 1e-6;
    }
    
    qml_map->setProperty("longitude", lon);
    qml_map->setProperty("latitude", lat);
    qml_map->setProperty("zoom_level", zoom);
    qml_map_container->update_coords();
  }
}

QPixmap Widget::get_pixmap_for_map()
{
  QPoint main_window_corner = this->mapToGlobal(QPoint(0, 0));
  int x0 = main_window_corner.x();
  int y0 = main_window_corner.y();
  
  QPoint top_left = label_map_title->mapToGlobal(QPoint(0, 0));
  int x = top_left.x() - x0;
  int y = top_left.y() - y0;
  int width = qml_map_container->width();
  
  QPoint bottom = spinbox_map_max->mapToGlobal(QPoint(0, 0));
  int height = bottom.y() + spinbox_map_max->height() - y - y0;
  
  QScreen *screen = QApplication::primaryScreen();
  if (!screen) { return QPixmap(); }
  
  label_reset_map_scale->hide();
  QCoreApplication::sendPostedEvents();
  
  QPixmap map = screen->grabWindow(this->winId(), x, y, width, height);
  
  label_reset_map_scale->show();
  
  return map;
}

void Widget::copy_map()
{
  label_reset_map_scale->hide();
  QTimer::singleShot(250, this, SLOT(delayed_copy_map()));
}

void Widget::export_map()
{
  label_reset_map_scale->hide();
  QString latest_out_path = get_output_path("png");
  
  QString out_file_name = QFileDialog::getSaveFileName(this,
                                                      "Save PNG",
                                                      latest_out_path,
                                                      "*.png");
  
  if (out_file_name == "") { return; }
  
  update_output_path(out_file_name, "png");
  
  QTimer::singleShot(250, this, [=]()->void { delayed_export_map(out_file_name); });
}

void Widget::delayed_copy_map()
{
  QApplication::clipboard()->setPixmap(get_pixmap_for_map());
  label_reset_map_scale->show();
}

void Widget::delayed_export_map(QString file_name)
{
  get_pixmap_for_map().save(file_name);
  label_reset_map_scale->show();
}

void Widget::export_booths_table()
{
  if (get_table_type() != "n_party_preferred") { return; }
  
  QString latest_out_path = get_output_path("csv");
  
  booths_output_file = QFileDialog::getSaveFileName(this,
                                                    "Save CSV",
                                                    latest_out_path,
                                                    "*.csv");
  
  
  if (booths_output_file == "") { return; }
  
  calculate_n_party_preferred(true);
  update_output_path(booths_output_file, "csv");
}

void Widget::show_abbreviations()
{
  Table_window *w = new Table_window(table_main_groups_short, table_main_groups, this);
  
  w->setMinimumSize(QSize(200, 200));
  w->resize(500, 500);
  w->setWindowTitle("Abbreviations");
  w->show();
}

void Widget::show_help()
{
  QWidget *help = new QWidget(this, Qt::Window);
  
  help->setMinimumSize(QSize(400, 500));
  help->setWindowTitle("Help");
  
  QLabel *label_help = new QLabel();
  label_help->setText("Senate preference explorer, written by David Barry, 2019.<br>Version 1.1, 2019-07-07."
                      "<br><br>Such documentation as there is, as well as links to source code, will be at <a href=\"https://pappubahry.com/pseph/senate_pref/\">"
                      "https://pappubahry.com/pseph/senate_pref/</a>.  You'll need to download specially made SQLITE files, which hold the preference "
                      "data in the format expected by this program.  You can then open these by clicking on the 'Load preferences' button."
                      "<br><br>This software was made with Qt Creator, using Qt 5.12: <a href=\"https://www.qt.io/\">https://www.qt.io/</a>; Qt components"
                      " are licensed under (L)GPL.  My own code is public domain."
                      );
  
  label_help->setWordWrap(true);
  label_help->setOpenExternalLinks(true);
  QGridLayout *help_layout = new QGridLayout();
  help_layout->addWidget(label_help);
  help->setLayout(help_layout);
  
  help->show();
}

QString Widget::get_table_type()
{
  return combo_table_type->currentData().toString();
}

QString Widget::get_value_type()
{
  return combo_value_type->currentData().toString();
}

QString Widget::get_abtl()
{
  return combo_abtl->currentData().toString();
}

int Widget::get_n_preferred()
{
  return clicked_n_parties.length();
}

int Widget::get_n_first_prefs()
{
  return spinbox_first_n_prefs->value();
}

int Widget::get_later_prefs_n_fixed()
{
  return spinbox_later_prefs_fixed->value();
}

int Widget::get_later_prefs_n_up_to()
{
  return spinbox_later_prefs_up_to->value();
}

int Widget::get_pref_sources_min()
{
  return spinbox_pref_sources_min->value();
}

int Widget::get_pref_sources_max()
{
  return spinbox_pref_sources_max->value();
}

int Widget::get_current_division()
{
  return combo_division->currentIndex();
}

QString Widget::get_short_group(int i)
{
  if (i >= get_num_groups())
  {
    return "Exh";
  }
  
  return table_main_groups_short.at(i);
}

int Widget::get_num_groups()
{
  return (get_abtl() == "atl") ? num_groups : num_cands;
}

QColor Widget::get_highlight_color()
{
  return QColor(224, 176, 176, 255);
}

QColor Widget::get_n_party_preferred_color()
{
  return QColor(60, 228, 228, 255);
}

QColor Widget::get_unfocused_text_color()
{
  return QColor(128, 128, 128, 255);
}

QColor Widget::get_focused_text_color()
{
  return QColor(0, 0, 0, 255);
}
