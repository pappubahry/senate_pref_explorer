/* Database table structure:
CREATE TABLE basic_info (id INTEGER PRIMARY KEY, state TEXT, state_full TEXT, year INTEGER, formal_votes INTEGER)
CREATE TABLE seats (id INTEGER PRIMARY KEY, seat TEXT, formal_votes INTEGER)
CREATE TABLE booths (id INTEGER PRIMARY KEY, seat TEXT, booth TEXT, lon REAL, lat REAL, formal_votes INTEGER)
CREATE TABLE groups (id INTEGER PRIMARY KEY, group_letter TEXT, party TEXT, party_ab TEXT, primaries INTEGER)
CREATE TABLE candidates (id INTEGER PRIMARY KEY, group_letter TEXT, group_pos INTEGER, party TEXT, party_ab TEXT, candidate TEXT, primaries INTEGER)
CREATE TABLE atl (id INTEGER PRIMARY KEY, seat_id INTEGER, booth_id INTEGER, num_prefs INTEGER, P1, P2, ..., Pfor0, Pfor1, ...)
CREATE TABLE btl (id INTEGER PRIMARY KEY, seat_id INTEGER, booth_id INTEGER, num_prefs INTEGER, P1, P2, ..., Pfor0, Pfor1, ...)
CREATE TABLE boundaries (id INTEGER PRIARY KEY, boundaries_csv TEXT)
*/

#include "main_widget.h"
#include "custom_expr.h"
#include "custom_lexer.h"
#include "custom_operation.h"
#include "custom_parser.h"
#include "math.h"
#include "table_type_constants.h"
#include "table_window.h"
#include "worker_sql_cross_table.h"
#include "worker_sql_custom_every_expr.h"
#include "worker_sql_custom_table.h"
#include "worker_sql_main_table.h"
#include "worker_sql_npp_table.h"

// Layout etc.:
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFormLayout>
#include <QGridLayout>
#include <QGuiApplication>
#include <QScreen>
#include <QSizePolicy>
#include <QSplitter>
#include <QStyleFactory>

// Form elements:
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpacerItem>
#include <QSpinBox>
#include <QTableView>

// SQL:
#include <QAction>
#include <QMetaType>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QThread>

// For the map:
#include "booth_model.h"
#include "map_container.h"
#include "polygon_model.h"
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QWindow>

#include <QMessageBox>

#define rad2deg 57.2957795131

const QString Widget::TABLE_POPUP = "popup";
const QString Widget::TABLE_MAIN  = "main";

const QString Widget::VALUE_VOTES             = "votes";
const QString Widget::VALUE_PERCENTAGES       = "percentages";
const QString Widget::VALUE_TOTAL_PERCENTAGES = "total_percentages";

const QString Widget::NO_FILTER = "no filter";

const QString Widget::MAP_DIVISIONS           = "divisions";
const QString Widget::MAP_ELECTION_DAY_BOOTHS = "booths_election_day";
const QString Widget::MAP_PREPOLL_BOOTHS      = "booths_prepoll";

using Qt::endl;

Widget::Widget(QWidget* parent)
  : QWidget(parent)
  , _sort_ballot_order(false)
  , _show_btl_headers(true)
  , _latest_path(QDir::currentPath())
  , _latest_out_path(QDir::currentPath())
  , _opened_database(false)
  , _num_groups(0)
  , _num_cands(0)
  , _num_table_rows(0)
  , _doing_calculation(false)
{
  // This is needed to allow a column of table data to be an argument to
  // a signal (which is emitted when a thread finishes processing an SQL
  // query):
  qRegisterMetaType<QVector<QVector<QVector<int>>>>("QVector<QVector<QVector<int>>>");
  qRegisterMetaType<QVector<QVector<int>>>("QVector<QVector<int>>");

  // Required to allow the QML to talk to the model containing the polygons:
  qmlRegisterType<Polygon_model>("Division_boundaries", 1, 0, "Polygon_model");
  qmlRegisterType<Booth_model>("Booths", 1, 0, "Booth_model");

  // The key to having the interface resize nicely when the user resizes the window is:
  // Add the QSplitter to a QGridLayout, then make the latter the layout for the
  // main widget ("this").
  QGridLayout* main_container = new QGridLayout();
  main_container->setContentsMargins(0, 0, 0, 0);

  QSplitter* splitter = new QSplitter(this);
  splitter->setStyleSheet("QSplitter::handle { background: #d0d0d0; }");

  QWidget* container_widget_left   = new QWidget();
  QWidget* container_widget_middle = new QWidget();
  QWidget* container_widget_right  = new QWidget();

  splitter->addWidget(container_widget_left);
  splitter->addWidget(container_widget_middle);
  splitter->addWidget(container_widget_right);

  splitter->setCollapsible(0, false);
  splitter->setCollapsible(1, true);
  splitter->setCollapsible(2, true);

  QVBoxLayout* layout_left = new QVBoxLayout();

  QHBoxLayout* layout_load = new QHBoxLayout();

  QString load_text("Load preferences...");
  _button_load   = new QPushButton(load_text, this);
  int load_width = _get_width_from_text(load_text, _button_load, 10);
  _button_load->setMaximumWidth(load_width);
  _button_load->setMinimumWidth(load_width);

  _label_load = new QLabel(this);
  _label_load->setText("No file loaded");

  layout_load->addWidget(_button_load);
  layout_load->addWidget(_label_load);

  layout_left->addLayout(layout_load);
  layout_left->setAlignment(layout_load, Qt::AlignTop);

  QFormLayout* layout_combos = new QFormLayout;
  layout_combos->setLabelAlignment(Qt::AlignRight);
  layout_combos->setVerticalSpacing(2);

  _combo_abtl = new QComboBox;
  _combo_abtl->addItem("Above the line", "atl");
  _combo_abtl->addItem("Below the line", "btl");

  _combo_table_type = new QComboBox;
  _combo_table_type->addItem("Step forward", Table_types::STEP_FORWARD);
  _combo_table_type->addItem("In first N preferences", Table_types::FIRST_N_PREFS);
  _combo_table_type->addItem("Later preferences", Table_types::LATER_PREFS);
  _combo_table_type->addItem("Preference sources", Table_types::PREF_SOURCES);
  _combo_table_type->addItem("N-party preferred", Table_types::NPP);
  _combo_table_type->addItem("Custom query", Table_types::CUSTOM);

  _combo_value_type = new QComboBox;
  _combo_value_type->addItem("Votes", VALUE_VOTES);
  _combo_value_type->addItem("Percentages", VALUE_PERCENTAGES);
  _combo_value_type->addItem("Total percentages", VALUE_TOTAL_PERCENTAGES);
  _combo_value_type->setCurrentIndex(1);

  _combo_division = new QComboBox;

  const QString first_n_prefs("In first N preferences");
  const int width_combo = _get_width_from_text(first_n_prefs, _combo_table_type);
  _combo_abtl->setMinimumWidth(width_combo);
  _combo_abtl->setMaximumWidth(width_combo);
  _combo_table_type->setMaximumWidth(width_combo);
  _combo_table_type->setMinimumWidth(width_combo);
  _combo_value_type->setMaximumWidth(width_combo);
  _combo_value_type->setMinimumWidth(width_combo);
  _combo_division->setMaximumWidth(width_combo);
  _combo_division->setMinimumWidth(width_combo);

  layout_combos->addRow("Preferences:", _combo_abtl);
  layout_combos->addRow("Table type:", _combo_table_type);
  layout_combos->addRow("Show", _combo_value_type);
  layout_combos->addRow("in", _combo_division);

  layout_left->addLayout(layout_combos);

  // ~~~~ Widgets unique to "first_n_prefs" table type ~~~~
  _container_first_n_prefs_widgets = new QWidget;
  _container_first_n_prefs_widgets->hide();
  QHBoxLayout* layout_first_n_prefs = new QHBoxLayout;

  QLabel* label_first_n_prefs_1 = new QLabel("Consider first");
  QLabel* label_first_n_prefs_2 = new QLabel("preferences");
  _spinbox_first_n_prefs        = new QSpinBox;
  _spinbox_first_n_prefs->setAlignment(Qt::AlignRight);
  _spinbox_first_n_prefs->setKeyboardTracking(false);

  const int spinbox_width = _get_width_from_text("999", _spinbox_first_n_prefs);
  _spinbox_first_n_prefs->setMinimumWidth(spinbox_width);
  _spinbox_first_n_prefs->setMaximumWidth(spinbox_width);

  layout_first_n_prefs->addWidget(label_first_n_prefs_1);
  layout_first_n_prefs->addWidget(_spinbox_first_n_prefs);
  layout_first_n_prefs->addWidget(label_first_n_prefs_2);

  layout_first_n_prefs->insertStretch(-1, 1);

  _container_first_n_prefs_widgets->setLayout(layout_first_n_prefs);
  layout_left->addWidget(_container_first_n_prefs_widgets);

  // ~~~~ Widgets unique to "later_prefs" table type ~~~~
  _container_later_prefs_widgets = new QWidget();
  _container_later_prefs_widgets->hide();
  QGridLayout* layout_later_prefs = new QGridLayout;

  QLabel* label_later_prefs_1 = new QLabel("Fix first");
  QLabel* label_later_prefs_2 = new QLabel("preferences,");
  QLabel* label_later_prefs_3 = new QLabel("and consider up to preference");

  _spinbox_later_prefs_fixed = new QSpinBox;
  _spinbox_later_prefs_fixed->setMinimumWidth(spinbox_width);
  _spinbox_later_prefs_fixed->setMaximumWidth(spinbox_width);
  _spinbox_later_prefs_fixed->setAlignment(Qt::AlignRight);
  _spinbox_later_prefs_fixed->setKeyboardTracking(false);

  _spinbox_later_prefs_up_to = new QSpinBox;
  _spinbox_later_prefs_up_to->setMinimumWidth(spinbox_width);
  _spinbox_later_prefs_up_to->setMaximumWidth(spinbox_width);
  _spinbox_later_prefs_up_to->setAlignment(Qt::AlignRight);
  _spinbox_later_prefs_up_to->setKeyboardTracking(false);

  QSpacerItem* spacer_later_prefs_0 = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  QSpacerItem* spacer_later_prefs_1 = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);

  layout_later_prefs->addWidget(label_later_prefs_1, 0, 0, Qt::AlignRight);
  layout_later_prefs->addWidget(_spinbox_later_prefs_fixed, 0, 1, Qt::AlignLeft);
  layout_later_prefs->addWidget(label_later_prefs_2, 0, 2, Qt::AlignLeft);
  layout_later_prefs->addWidget(label_later_prefs_3, 1, 0, Qt::AlignRight);
  layout_later_prefs->addWidget(_spinbox_later_prefs_up_to, 1, 1, Qt::AlignLeft);
  layout_later_prefs->addItem(spacer_later_prefs_0, 0, 3);
  layout_later_prefs->addItem(spacer_later_prefs_1, 1, 3);

  _container_later_prefs_widgets->setLayout(layout_later_prefs);
  layout_left->addWidget(_container_later_prefs_widgets);

  // ~~~~ Widgets unique to "pref_sources" table type ~~~~
  _container_pref_sources_widgets = new QWidget();
  _container_pref_sources_widgets->hide();
  QHBoxLayout* layout_pref_sources = new QHBoxLayout;

  QLabel* label_pref_sources_1 = new QLabel("Consider preferences");
  QLabel* label_pref_sources_2 = new QLabel("to");

  _spinbox_pref_sources_min = new QSpinBox;
  _spinbox_pref_sources_min->setMinimumWidth(spinbox_width);
  _spinbox_pref_sources_min->setMaximumWidth(spinbox_width);
  _spinbox_pref_sources_min->setAlignment(Qt::AlignRight);
  _spinbox_pref_sources_min->setKeyboardTracking(false);

  _spinbox_pref_sources_max = new QSpinBox;
  _spinbox_pref_sources_max->setMinimumWidth(spinbox_width);
  _spinbox_pref_sources_max->setMaximumWidth(spinbox_width);
  _spinbox_pref_sources_max->setAlignment(Qt::AlignRight);
  _spinbox_pref_sources_max->setKeyboardTracking(false);

  layout_pref_sources->addWidget(label_pref_sources_1);
  layout_pref_sources->addWidget(_spinbox_pref_sources_min);
  layout_pref_sources->addWidget(label_pref_sources_2);
  layout_pref_sources->addWidget(_spinbox_pref_sources_max);

  layout_pref_sources->insertStretch(-1, 1);

  _container_pref_sources_widgets->setLayout(layout_pref_sources);
  layout_left->addWidget(_container_pref_sources_widgets);

  // ~~~~ Widgets unique to "n_party_preferred" table type ~~~~
  _container_n_party_preferred_widgets = new QWidget();
  _container_n_party_preferred_widgets->hide();
  QHBoxLayout* layout_n_party_preferred = new QHBoxLayout;

  QLabel* label_n_party_preferred     = new QLabel("Click on the wanted parties");
  _button_n_party_preferred_calculate = new QPushButton("Calculate", this);
  _button_n_party_preferred_calculate->setEnabled(false);

  layout_n_party_preferred->addWidget(label_n_party_preferred);
  layout_n_party_preferred->addWidget(_button_n_party_preferred_calculate);

  layout_n_party_preferred->insertStretch(-1, 1);

  _container_n_party_preferred_widgets->setLayout(layout_n_party_preferred);

  layout_left->addWidget(_container_n_party_preferred_widgets);

  // ~~~~ Widgets unique to "custom" table type ~~~~
  _container_custom_widgets = new QWidget();
  _container_custom_widgets->hide();

  QVBoxLayout* layout_custom                 = new QVBoxLayout;
  QFormLayout* layout_custom_inputs          = new QFormLayout;
  QHBoxLayout* layout_custom_calc_line       = new QHBoxLayout;
  QHBoxLayout* layout_custom_total_base_line = new QHBoxLayout;

  layout_custom->setContentsMargins(0, 0, 0, 0);
  layout_custom_inputs->setContentsMargins(0, 0, 0, 0);
  layout_custom_calc_line->setContentsMargins(0, 0, 0, 0);
  layout_custom_total_base_line->setContentsMargins(0, 0, 0, 0);

  layout_custom_inputs->setLabelAlignment(Qt::AlignRight);
  layout_custom_inputs->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
#if defined(Q_OS_MAC)
  layout_custom_inputs->setVerticalSpacing(4);
#endif

  QFont fixed_width_font  = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  _lineedit_custom_filter = new QLineEdit();
  _lineedit_custom_rows   = new QLineEdit();
  _lineedit_custom_cols   = new QLineEdit();
  _lineedit_custom_cell   = new QLineEdit();

  _lineedit_custom_filter->setFont(fixed_width_font);
  _lineedit_custom_rows->setFont(fixed_width_font);
  _lineedit_custom_cols->setFont(fixed_width_font);
  _lineedit_custom_cell->setFont(fixed_width_font);

  _button_calculate_custom = new QPushButton("Calculate", this);

  QLabel* label_custom_target = new QLabel("to");

  _combo_custom_table_target = new QComboBox;
  _combo_custom_table_target->addItem("Popup window", TABLE_POPUP);
  _combo_custom_table_target->addItem("Main table and map", TABLE_MAIN);
  _combo_custom_table_target->setCurrentIndex(0);

  QLabel* label_total_base    = new QLabel("Filtered base: ");
  _label_custom_filtered_base = new QLabel("");

  layout_custom_inputs->addRow("Filter", _lineedit_custom_filter);
  layout_custom_inputs->addRow("Rows", _lineedit_custom_rows);
  layout_custom_inputs->addRow("Columns", _lineedit_custom_cols);
  layout_custom_inputs->addRow("Cell", _lineedit_custom_cell);

  layout_custom_calc_line->addWidget(_button_calculate_custom);
  layout_custom_calc_line->addWidget(label_custom_target);
  layout_custom_calc_line->addWidget(_combo_custom_table_target);
  layout_custom_calc_line->setStretch(2, 2);

  layout_custom_total_base_line->addWidget(label_total_base);
  layout_custom_total_base_line->addWidget(_label_custom_filtered_base);
  layout_custom_total_base_line->setStretch(1, 2);

  layout_custom->addLayout(layout_custom_inputs);
  layout_custom->addLayout(layout_custom_calc_line);
  layout_custom->addLayout(layout_custom_total_base_line);

  _container_custom_widgets->setLayout(layout_custom);
  layout_left->addWidget(_container_custom_widgets);

  // ~~~~ End widgets unique to a table type ~~~~

  QHBoxLayout* layout_label_toggles = new QHBoxLayout();

  _label_sort = new Clickable_label();
  _label_sort->setText("<i>Toggle sort</i>");
  _label_sort->setCursor(Qt::PointingHandCursor);
  _label_sort->setSizePolicy(QSizePolicy());

  _label_toggle_names = new Clickable_label();
  _label_toggle_names->setText("<i>Toggle names</i>");
  _label_toggle_names->setCursor(Qt::PointingHandCursor);
  _label_toggle_names->setSizePolicy(QSizePolicy());
  _label_toggle_names->hide();

  _button_calculate_after_spinbox = new QPushButton("Calculate", this);
  _button_calculate_after_spinbox->hide();

  layout_label_toggles->addWidget(_label_sort);
  layout_label_toggles->addWidget(_label_toggle_names);
  layout_label_toggles->addWidget(_button_calculate_after_spinbox);

  layout_label_toggles->insertStretch(-1, 1);
  layout_label_toggles->insertSpacing(1, 15);

  layout_left->addLayout(layout_label_toggles);

  // ~~~~ Main table ~~~~
  _table_main_model = new QStandardItemModel(this);
  _table_main       = new Table_view;
  _table_main->setModel(_table_main_model);
  _table_main->setEditTriggers(QAbstractItemView::NoEditTriggers);
  _table_main->setShowGrid(false);
  _table_main->setAlternatingRowColors(true);
  _table_main->setStyleSheet("QTableView {alternate-background-color: #f0f0f0; background-color: #ffffff; color: black; }");
  _table_main->setFocusPolicy(Qt::NoFocus);
  _table_main->viewport()->setFocusPolicy(Qt::NoFocus);
  _table_main->setSelectionMode(QAbstractItemView::NoSelection);
  _table_main->verticalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
  _table_main->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  _table_main->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  QHeaderView* table_main_header = _table_main->horizontalHeader();
  _reset_npp_sort();

  _one_line_height = _table_main->fontMetrics().boundingRect("0").height();

#if defined(Q_OS_MAC)
  const double two_line_height_factor = 3.;
#else
  const double two_line_height_factor = 2.4;
#endif

  _two_line_height = two_line_height_factor * _one_line_height;

  layout_left->addWidget(_table_main);

  _container_copy_main_table = new QWidget();
  _container_copy_main_table->hide();

  QHBoxLayout* layout_copy_main_table = new QHBoxLayout();
  layout_copy_main_table->setContentsMargins(0, 0, 0, 0);
  _button_copy_main_table = new QPushButton("Copy");
  _button_copy_main_table->setEnabled(false);
  _button_export_main_table = new QPushButton("Export...");
  _button_export_main_table->setEnabled(false);
  QSpacerItem* spacer_copy_main_table = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  layout_copy_main_table->addWidget(_button_copy_main_table);
  layout_copy_main_table->addWidget(_button_export_main_table);
  layout_copy_main_table->addItem(spacer_copy_main_table);

  _container_copy_main_table->setLayout(layout_copy_main_table);

  layout_left->addWidget(_container_copy_main_table);

  QHBoxLayout* layout_left_bottom = new QHBoxLayout();

  QString cross_table("Cross table...");
  _button_cross_table = new QPushButton(cross_table);
  _button_cross_table->setMaximumWidth(_get_width_from_text(cross_table, _button_cross_table, 10));
  _button_cross_table->setEnabled(false);

  layout_left_bottom->addWidget(_button_cross_table);

  QSpacerItem* spacer_left_bottom = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  _label_progress                 = new QLabel("No calculation");
  _label_progress->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  layout_left_bottom->addItem(spacer_left_bottom);
  layout_left_bottom->addWidget(_label_progress);

  layout_left->addLayout(layout_left_bottom);

  // ~~~~~ Middle column: divisions table ~~~~~
  QVBoxLayout* layout_middle = new QVBoxLayout();

  QHBoxLayout* layout_middle_top = new QHBoxLayout();
  _button_abbreviations          = new QPushButton("Abbreviations");
  QSpacerItem* spacer_middle_top = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  _button_help                   = new QPushButton("Help");

  _button_abbreviations->setEnabled(false);
  _button_help->setEnabled(true);

  layout_middle_top->addWidget(_button_abbreviations);
  layout_middle_top->addItem(spacer_middle_top);
  layout_middle_top->addWidget(_button_help);

  layout_middle->addLayout(layout_middle_top);

  _label_division_table_title = new QLabel("<b>No selection</b>");
  _label_division_table_title->setMaximumWidth(200);
  _label_division_table_title->setWordWrap(true);

  _table_divisions_model = new QStandardItemModel(this);
  _table_divisions       = new Table_view;
  _table_divisions->setModel(_table_divisions_model);
  _table_divisions->setEditTriggers(QAbstractItemView::NoEditTriggers);
  _table_divisions->setShowGrid(false);
  _table_divisions->setAlternatingRowColors(true);
  _table_divisions->setStyleSheet("QTableView {alternate-background-color: #f0f0f0; background-color: #ffffff; color: black; }");
  _table_divisions->setFocusPolicy(Qt::NoFocus);
  _table_divisions->setSelectionMode(QAbstractItemView::NoSelection);
  _table_divisions->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  _table_divisions->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  _table_divisions->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  _table_divisions->horizontalHeader()->setDefaultAlignment(Qt::AlignVCenter);
  _table_divisions->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  _table_divisions->verticalHeader()->setDefaultSectionSize(_one_line_height);
  _table_divisions->verticalHeader()->hide();

  QHeaderView* table_divisions_header = _table_divisions->horizontalHeader();

  // *** this doesn't work??? ***
  table_divisions_header->setCursor(Qt::PointingHandCursor);

  QHBoxLayout* layout_division_buttons = new QHBoxLayout();
  _button_divisions_copy               = new QPushButton("Copy");
  const int div_copy_width             = _get_width_from_text("Copy", _button_divisions_copy, 10);
  _button_divisions_copy->setMaximumWidth(div_copy_width);
  _button_divisions_copy->setMinimumWidth(div_copy_width);

  _button_divisions_export   = new QPushButton("Export...");
  const int div_export_width = _get_width_from_text("Export...", _button_divisions_export, 10);
  _button_divisions_export->setMaximumWidth(div_export_width);
  _button_divisions_export->setMinimumWidth(div_export_width);

  _button_divisions_booths_export   = new QPushButton("Export booths...");
  const int div_booths_export_width = _get_width_from_text("Export booths...", _button_divisions_booths_export, 10);
  _button_divisions_booths_export->setMaximumWidth(div_booths_export_width);
  _button_divisions_booths_export->setMinimumWidth(div_booths_export_width);

  QSpacerItem* spacer_division_buttons = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  layout_division_buttons->addWidget(_button_divisions_copy);
  layout_division_buttons->addWidget(_button_divisions_export);
  layout_division_buttons->addWidget(_button_divisions_booths_export);
  layout_division_buttons->addItem(spacer_division_buttons);

  QLabel* label_divisions_cross_tables = new QLabel("Cross tables by:");
  label_divisions_cross_tables->setAlignment(Qt::AlignLeft);

  QHBoxLayout* layout_division_cross_table_buttons = new QHBoxLayout();
  _button_divisions_cross_table                    = new QPushButton("Division");
  _button_booths_cross_table                       = new QPushButton("Booth (export)...");
  QSpacerItem* spacer_division_cross_tables        = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
  layout_division_cross_table_buttons->addWidget(_button_divisions_cross_table);
  layout_division_cross_table_buttons->addWidget(_button_booths_cross_table);
  layout_division_cross_table_buttons->addItem(spacer_division_cross_tables);

  _button_divisions_copy->setEnabled(false);
  _button_divisions_export->setEnabled(false);
  _button_divisions_booths_export->setEnabled(false);
  _button_divisions_cross_table->setEnabled(false);
  _button_booths_cross_table->setEnabled(false);

  layout_middle->addWidget(_label_division_table_title);
  layout_middle->addWidget(_table_divisions);
  layout_middle->addLayout(layout_division_buttons);
  layout_middle->addWidget(label_divisions_cross_tables);
  layout_middle->addLayout(layout_division_cross_table_buttons);

  main_container->addWidget(splitter);
  this->setLayout(main_container);

  _reset_divisions_sort();

  // ~~~~~ Right-hand column: map ~~~~~
  const int booth_threshold = 100;

  _map_divisions_model.setup_list("", "", 2016, QStringList{});
  _map_booths_model.setup_list(_booths, booth_threshold);

  QVBoxLayout* layout_right = new QVBoxLayout();

  QHBoxLayout* layout_map_title_help = new QHBoxLayout();

  _label_map_title = new QLabel("<b>No selection</b>");
  _label_map_title->setWordWrap(true);

  layout_map_title_help->addWidget(_label_map_title, 1);

  _label_map_help = new Clickable_label();
  _label_map_help->setText("<i>API key?</i>");
  _label_map_help->setCursor(Qt::PointingHandCursor);
  layout_map_title_help->addWidget(_label_map_help, 0);

  const QString ini_path = QString("%1/map.ini")
                             .arg(QCoreApplication::applicationDirPath());

  if (!QFile::exists(ini_path))
  {
    QFile ini_file(ini_path);
    if (ini_file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
      QTextStream out(&ini_file);
      out << "[Map]\n";
      out << "UseDefault=true\n";
      out << "; Set UseDefault to false to activate the following option.\n";
      out << "; \n";
      out << "; At the time of writing (June 2025), the Qt default uses the Thunderforest\n";
      out << "; OSM tile server, which displays \"API key required\" watermarks if you don't\n";
      out << "; supply such a key.  If you set up an account with Thunderforest, then you\n";
      out << "; can remove those watermarks by inserting your key into the parameter below.\n";
      out << "; \n";
      out << "; You will probably need to clear the cached tiles, which in Windows are\n";
      out << "; probably several subfolders deep in %LOCALAPPDATA%\\cache\\QtLocation.\n";
      out << "; \n";
      out << "; Non-Thunderforest OSM tile servers can also be set here; in my testing, you\n";
      out << "; need the URL format to be %z/%x/%y rather than {z}/{x}/{y}.  See\n";
      out << "; https://wiki.openstreetmap.org/wiki/Raster_tile_providers for options.\n";
      out << "; \n";
      out << "TileServer=https://tile.thunderforest.com/atlas/%z/%x/%y.png?apikey=<your-api-key>\n";
      out.flush();
      ini_file.close();
    }
  }

  QSettings map_settings(ini_path, QSettings::IniFormat);
  const QString map_tile_server = map_settings.value("Map/TileServer", "").toString();
  QUrl url(map_tile_server);
  const QString tile_host    = url.host(QUrl::FullyDecoded);
  const bool map_use_default = tile_host.isEmpty() || map_settings.value("Map/UseDefault", "true").toString().toLower() != "false";

  QStringList map_copyright_parts;
  if (map_tile_server.contains("thunderforest.com/"))
  {
    map_copyright_parts.append("Maps © <a href=\"https://www.thunderforest.com\">Thunderforest</a>");
  }
  else
  {
    map_copyright_parts.append("Maps " + tile_host);
  }

  map_copyright_parts.append("Map data <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap</a>");

  const QString map_copyright_notice = QString("<span style=\"background-color:rgba(255, 255, 255, 0.9)\">%1</span>")
                                         .arg(map_copyright_parts.join(" | "));

  _qml_map_container = new Map_container(this);
  _qml_map_container->setStyleSheet("QToolTip { color: #000000; }");

  _qml_map_container->rootContext()->setContextProperty("useDefaultServer", map_use_default);
  _qml_map_container->rootContext()->setContextProperty("tileServer", map_tile_server);
  _qml_map_container->rootContext()->setContextProperty("copyrightText", map_copyright_notice);
  _qml_map_container->rootContext()->setContextProperty("divisionsModel", &_map_divisions_model);
  _qml_map_container->rootContext()->setContextProperty("boothsModel", &_map_booths_model);

  _qml_map_container->setSource(QUrl("qrc:///map.qml"));
  _qml_map_container->setMinimumSize(512, 512);
  _qml_map_container->init_variables();

  QObject* qml_root_object = _qml_map_container->rootObject();
  _qml_map                 = qml_root_object->findChild<QObject*>("map");

  QLabel* label_map_division_info = new QLabel();
  _map_divisions_model.set_label(label_map_division_info);

  QHBoxLayout* layout_map_legend = new QHBoxLayout();

  QSpacerItem* spacer_map_legend_right = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);

  _spinbox_map_min = new QDoubleSpinBox;
  _spinbox_map_max = new QDoubleSpinBox;

  const int legend_spinbox_width = _get_width_from_text("99999", _spinbox_map_min);

  _spinbox_map_min->setMinimum(0.);
  _spinbox_map_min->setMaximum(100.);
  _spinbox_map_min->setDecimals(1);
  _spinbox_map_min->setValue(0.);
  _spinbox_map_min->setAlignment(Qt::AlignRight);
  _spinbox_map_min->setKeyboardTracking(false);
  _spinbox_map_min->setMinimumWidth(legend_spinbox_width);
  _spinbox_map_min->setMaximumWidth(legend_spinbox_width);

  _spinbox_map_max->setMinimum(0.);
  _spinbox_map_max->setMaximum(100.);
  _spinbox_map_max->setDecimals(1);
  _spinbox_map_max->setValue(100.);
  _spinbox_map_max->setAlignment(Qt::AlignRight);
  _spinbox_map_max->setKeyboardTracking(false);
  _spinbox_map_max->setMinimumWidth(legend_spinbox_width);
  _spinbox_map_max->setMaximumWidth(legend_spinbox_width);

  QLabel* label_map_legend = new QLabel();
  label_map_legend->setPixmap(QPixmap::fromImage(QImage(":/viridis_scale_20.png")));
  label_map_legend->adjustSize();

  _label_reset_map_scale = new Clickable_label();
  _label_reset_map_scale->setText("<i>Reset</i>");
  _label_reset_map_scale->setCursor(Qt::PointingHandCursor);
  _label_reset_map_scale->setSizePolicy(QSizePolicy());

  layout_map_legend->addWidget(_spinbox_map_min);
  layout_map_legend->addWidget(label_map_legend);
  layout_map_legend->addWidget(_spinbox_map_max);
  layout_map_legend->addWidget(_label_reset_map_scale);
  layout_map_legend->addItem(spacer_map_legend_right);

  QHBoxLayout* layout_map_zooms = new QHBoxLayout();
  QLabel* label_zoom_to         = new QLabel("Zoom to: ");

  Clickable_label* label_zoom_to_state = new Clickable_label();
  label_zoom_to_state->setText("<i>State</i>  ");
  label_zoom_to_state->setCursor(Qt::PointingHandCursor);
  label_zoom_to_state->setSizePolicy(QSizePolicy());

  Clickable_label* label_zoom_to_capital = new Clickable_label();
  label_zoom_to_capital->setText("<i>Capital</i>  ");
  label_zoom_to_capital->setCursor(Qt::PointingHandCursor);
  label_zoom_to_capital->setSizePolicy(QSizePolicy());

  Clickable_label* label_zoom_to_division = new Clickable_label();
  label_zoom_to_division->setText("<i>Division</i>  ");
  label_zoom_to_division->setCursor(Qt::PointingHandCursor);
  label_zoom_to_division->setSizePolicy(QSizePolicy());

  _button_map_copy   = new QPushButton("Copy");
  _button_map_export = new QPushButton("Export...");

  layout_map_zooms->addWidget(label_zoom_to);
  layout_map_zooms->addWidget(label_zoom_to_state);
  layout_map_zooms->addWidget(label_zoom_to_capital);
  layout_map_zooms->addWidget(label_zoom_to_division);
  layout_map_zooms->addWidget(_button_map_copy);
  layout_map_zooms->addWidget(_button_map_export);
  layout_map_zooms->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));

  QHBoxLayout* layout_map_options = new QHBoxLayout();

  _combo_map_type = new QComboBox;
  _combo_map_type->addItem("Divisions", MAP_DIVISIONS);
  _combo_map_type->addItem("Election day booths", MAP_ELECTION_DAY_BOOTHS);
  _combo_map_type->addItem("Prepoll booths", MAP_PREPOLL_BOOTHS);

  _label_map_booth_min_1 = new QLabel("with at least");
  _label_map_booth_min_2 = new QLabel("votes");

  _spinbox_map_booth_threshold = new QSpinBox;
  _spinbox_map_booth_threshold->setMinimum(0);
  _spinbox_map_booth_threshold->setMaximum(10000);
  _spinbox_map_booth_threshold->setValue(booth_threshold);
  _spinbox_map_booth_threshold->setSingleStep(50);
  _spinbox_map_booth_threshold->setAlignment(Qt::AlignRight);
  _spinbox_map_booth_threshold->setKeyboardTracking(false);
  _spinbox_map_booth_threshold->setMinimumWidth(legend_spinbox_width);
  _spinbox_map_booth_threshold->setMaximumWidth(legend_spinbox_width);

  QSizePolicy sp_retain = _spinbox_map_booth_threshold->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  _spinbox_map_booth_threshold->setSizePolicy(sp_retain);

  QSpacerItem* spacer_map_options = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);

  _label_map_booth_min_1->hide();
  _label_map_booth_min_2->hide();
  _spinbox_map_booth_threshold->hide();

  layout_map_options->addWidget(_combo_map_type);
  layout_map_options->addWidget(_label_map_booth_min_1);
  layout_map_options->addWidget(_spinbox_map_booth_threshold);
  layout_map_options->addWidget(_label_map_booth_min_2);
  layout_map_options->addItem(spacer_map_options);

  layout_right->addLayout(layout_map_title_help);
  layout_right->addWidget(label_map_division_info);
  layout_right->addWidget(_qml_map_container, 1);
  layout_right->addLayout(layout_map_legend);
  layout_right->addLayout(layout_map_options);
  layout_right->addLayout(layout_map_zooms);

  // Read the most recent directory an SQLITE file was loaded from.
  const QString last_path_file_name = QString("%1/last_sqlite_dir.txt")
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
        _latest_path = last_path;
      }

      last_path_file.close();
    }
  }

  // Read the most recent custom query
  const QString last_custom_file_name = QString("%1/last_custom_query.txt")
                                          .arg(QCoreApplication::applicationDirPath());

  QFileInfo check_exists_custom(last_custom_file_name);
  if (check_exists_custom.exists())
  {
    QFile last_custom_file(last_custom_file_name);
    if (last_custom_file.open(QIODevice::ReadOnly))
    {
      QTextStream in(&last_custom_file);
      while (!in.atEnd())
      {
        QString line = in.readLine();
        // filter, rows, cols, cell
        if (line.startsWith("Filter:"))
        {
          _lineedit_custom_filter->setText(line.replace("Filter:", "").trimmed());
        }
        else if (line.startsWith("Rows:"))
        {
          _lineedit_custom_rows->setText(line.replace("Rows:", "").trimmed());
        }
        else if (line.startsWith("Cols:"))
        {
          _lineedit_custom_cols->setText(line.replace("Cols:", "").trimmed());
        }
        else if (line.startsWith("Cell:"))
        {
          _lineedit_custom_cell->setText(line.replace("Cell:", "").trimmed());
        }
      }
    }
  }

#if defined(Q_OS_WIN)
  // Workaround for the hideous spinbox design from Qt 6.8+, in which the
  // spinbox arrows are enormous and arranged side-by-side:
  QStyle* vista = QStyleFactory::create("windowsvista");
  _spinbox_first_n_prefs->setStyle(vista);
  _spinbox_later_prefs_fixed->setStyle(vista);
  _spinbox_later_prefs_up_to->setStyle(vista);
  _spinbox_pref_sources_min->setStyle(vista);
  _spinbox_pref_sources_max->setStyle(vista);
  _spinbox_map_booth_threshold->setStyle(vista);
  _spinbox_map_min->setStyle(vista);
  _spinbox_map_max->setStyle(vista);
#endif
  _reset_spinboxes();

  container_widget_left->setLayout(layout_left);
  container_widget_middle->setLayout(layout_middle);
  container_widget_right->setLayout(layout_right);

  connect(_button_load,                        &QPushButton::clicked,                                this, &Widget::_open_database);
  connect(_combo_abtl,                         QOverload<int>::of(&QComboBox::currentIndexChanged),  this, &Widget::_change_abtl);
  connect(_combo_table_type,                   QOverload<int>::of(&QComboBox::currentIndexChanged),  this, &Widget::_change_table_type);
  connect(_combo_value_type,                   QOverload<int>::of(&QComboBox::currentIndexChanged),  this, &Widget::_change_value_type);
  connect(_combo_division,                     QOverload<int>::of(&QComboBox::currentIndexChanged),  this, &Widget::_change_division);
  connect(_spinbox_first_n_prefs,              QOverload<int>::of(&QSpinBox::valueChanged),          this, &Widget::_change_first_n_prefs);
  connect(_spinbox_later_prefs_fixed,          QOverload<int>::of(&QSpinBox::valueChanged),          this, &Widget::_change_later_prefs_fixed);
  connect(_spinbox_later_prefs_up_to,          QOverload<int>::of(&QSpinBox::valueChanged),          this, &Widget::_change_later_prefs_up_to);
  connect(_spinbox_pref_sources_min,           QOverload<int>::of(&QSpinBox::valueChanged),          this, &Widget::_change_pref_sources_min);
  connect(_spinbox_pref_sources_max,           QOverload<int>::of(&QSpinBox::valueChanged),          this, &Widget::_change_pref_sources_max);
  connect(_button_n_party_preferred_calculate, &QPushButton::clicked,                                this, &Widget::_calculate_n_party_preferred);
  connect(_button_calculate_after_spinbox,     &QPushButton::clicked,                                this, &Widget::_add_column_to_main_table);
  connect(_button_calculate_custom,            &QPushButton::clicked,                                this, &Widget::_slot_calculate_custom);
  connect(_button_copy_main_table,             &QPushButton::clicked,                                this, &Widget::_copy_main_table);
  connect(_button_export_main_table,           &QPushButton::clicked,                                this, &Widget::_export_main_table);
  connect(_button_cross_table,                 &QPushButton::clicked,                                this, &Widget::_make_cross_table);
  connect(_button_abbreviations,               &QPushButton::clicked,                                this, &Widget::_show_abbreviations);
  connect(_button_help,                        &QPushButton::clicked,                                this, &Widget::_show_help);
  connect(_button_divisions_copy,              &QPushButton::clicked,                                this, &Widget::_copy_divisions_table);
  connect(_button_divisions_export,            &QPushButton::clicked,                                this, &Widget::_export_divisions_table);
  connect(_button_divisions_booths_export,     &QPushButton::clicked,                                this, &Widget::_export_booths_table);
  connect(_button_divisions_cross_table,       &QPushButton::clicked,                                this, &Widget::_make_divisions_cross_table);
  connect(_button_booths_cross_table,          &QPushButton::clicked,                                this, &Widget::_make_booths_cross_table);
  connect(_label_sort,                         &Clickable_label::clicked,                            this, &Widget::_toggle_sort);
  connect(_label_toggle_names,                 &Clickable_label::clicked,                            this, &Widget::_toggle_names);
  connect(_table_main,                         &QTableView::clicked,                                 this, &Widget::_clicked_main_table);
  connect(table_main_header,                   &QHeaderView::sectionClicked,                         this, &Widget::_slot_click_main_table_header);
  connect(table_divisions_header,              &QHeaderView::sectionClicked,                         this, &Widget::_change_divisions_sort);
  connect(_label_map_help,                     &Clickable_label::clicked,                            this, &Widget::_show_map_help);
  connect(_label_reset_map_scale,              &Clickable_label::clicked,                            this, &Widget::_reset_map_scale);
  connect(label_zoom_to_state,                 &Clickable_label::clicked,                            this, &Widget::_zoom_to_state);
  connect(label_zoom_to_capital,               &Clickable_label::clicked,                            this, &Widget::_zoom_to_capital);
  connect(label_zoom_to_division,              &Clickable_label::clicked,                            this, QOverload<>::of(&Widget::_zoom_to_division));
  connect(_button_map_copy,                    &QPushButton::clicked,                                this, &Widget::_copy_map);
  connect(_button_map_export,                  &QPushButton::clicked,                                this, &Widget::_export_map);
  connect(_spinbox_map_min,                    QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Widget::_update_map_scale_minmax);
  connect(_spinbox_map_max,                    QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Widget::_update_map_scale_minmax);
  connect(_combo_map_type,                     QOverload<int>::of(&QComboBox::currentIndexChanged),  this, &Widget::_change_map_type);
  connect(_spinbox_map_booth_threshold,        QOverload<int>::of(&QSpinBox::valueChanged),          this, &Widget::_change_map_booth_threshold);

  connect(_qml_map_container, &Map_container::mouse_moved_2arg, &_map_divisions_model, &Polygon_model::point_in_polygon);
  connect(_qml_map_container, &Map_container::mouse_moved_4arg, &_map_booths_model,    &Booth_model::check_mouseover);

  // Daisy-chained pair of connections for double-click to zoom to division:
  connect(_qml_map_container,    &Map_container::double_clicked,          &_map_divisions_model, &Polygon_model::received_double_click);
  connect(&_map_divisions_model, &Polygon_model::double_clicked_division, this,                  QOverload<int>::of(&Widget::_zoom_to_division));

  connect(&_map_booths_model, &Booth_model::send_tooltip, _qml_map_container, &Map_container::show_tooltip);

  // Old signal/slot syntax for the QML:
  connect(qml_root_object, SIGNAL(exited_map()), &_map_divisions_model, SLOT(exited_map()));

  connect(_spinbox_map_min, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &_map_divisions_model, &Polygon_model::update_scale_min);
  connect(_spinbox_map_max, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &_map_divisions_model, &Polygon_model::update_scale_max);
  connect(_spinbox_map_min, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &_map_booths_model, &Booth_model::update_scale_min);
  connect(_spinbox_map_max, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &_map_booths_model, &Booth_model::update_scale_max);
}

Widget::~Widget()
{
  // Write the latest SQLITE path to file before closing.
  const QString last_path_file_name = QString("%1/last_sqlite_dir.txt")
                                        .arg(QCoreApplication::applicationDirPath());

  QFile last_path_file(last_path_file_name);

  if (last_path_file.open(QIODevice::WriteOnly))
  {
    QTextStream out(&last_path_file);
    out << _latest_path << endl;
    last_path_file.close();
  }
}

int Widget::_get_width_from_text(const QString& t, QWidget* w, int buffer)
{
  const QSize text_size = w->fontMetrics().size(Qt::TextShowMnemonic, t);

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

void Widget::_reset_spinboxes()
{
  _doing_calculation = true; // Just to prevent any changed() signals from doing anything

  _spinbox_first_n_prefs->setMinimum(2);
  _spinbox_first_n_prefs->setMaximum(6);
  _spinbox_first_n_prefs->setValue(6);

  _spinbox_later_prefs_fixed->setMinimum(1);
  _spinbox_later_prefs_fixed->setMaximum(5);
  _spinbox_later_prefs_fixed->setValue(1);

  _spinbox_later_prefs_up_to->setMinimum(2);
  _spinbox_later_prefs_up_to->setMaximum(6);
  _spinbox_later_prefs_up_to->setValue(6);

  _spinbox_pref_sources_min->setMinimum(2);
  _spinbox_pref_sources_min->setMaximum(6);
  _spinbox_pref_sources_min->setValue(2);

  _spinbox_pref_sources_max->setMinimum(2);
  _spinbox_pref_sources_max->setMaximum(6);
  _spinbox_pref_sources_max->setValue(6);

  _doing_calculation = false;
}

void Widget::_open_database()
{
  const QString file_name = QFileDialog::getOpenFileName(
    this, QString(), _latest_path, QString("*.sqlite"));

  if (file_name.isNull())
  {
    return;
  }

  QFileInfo check_exists(file_name);
  if (check_exists.exists() && check_exists.isFile())
  {
    _latest_path = check_exists.absolutePath();
    _load_database(file_name);
  }
}

void Widget::_load_database(const QString& db_file)
{
  // Get rid of any data that might exist:
  _clear_main_table_data();
  _table_main_model->clear();
  _clicked_cells.clear();
  _clicked_cells_two_axis.clear();
  _clicked_n_parties.clear();
  _clear_divisions_table();
  _division_formal_votes.clear();
  _group_from_short.clear();
  _cand_from_short.clear();
  _group_from_candidate.clear();
  _candidates_per_group.clear();
  _reset_spinboxes();

  bool errors = false;
  QString error_msg;
  QString connection_name("conn");

  {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connection_name);
    db.setDatabaseName(db_file);

    if (!db.open())
    {
      errors    = true;
      error_msg = QString("Error: couldn't open database.");
    }

    if (!errors)
    {
      QStringList tables = db.tables();
      if (tables.indexOf("basic_info") < 0 || tables.indexOf("candidates") < 0 || tables.indexOf("groups") < 0 || tables.indexOf("seats") < 0 ||
          tables.indexOf("booths") < 0 || tables.indexOf("atl") < 0 || tables.indexOf("btl") < 0)
      {
        errors    = true;
        error_msg = QString("Error: database is not formatted correctly.");
      }
    }

    QSqlQuery query(db);

    if (!errors)
    {
      if (!query.exec("SELECT state, state_full, year, formal_votes, atl_votes, btl_votes FROM basic_info"))
      {
        errors    = true;
        error_msg = "Couldn't read basic information from database";
      }
      else
      {
        query.next();
        _state_short        = query.value(0).toString();
        _state_full         = query.value(1).toString();
        _year               = query.value(2).toInt();
        _total_formal_votes = query.value(3).toInt();
        _total_atl_votes    = query.value(4).toInt();
        _total_btl_votes    = query.value(5).toInt();

        _label_load->setText(QString("%1 %2").arg(_state_short).arg(_year));
      }
    }

    if (!errors)
    {
      // *** Error-handling needs to be much better in here ***
      // e.g., even after detecting an error, the code continues to run....

      if (!query.exec("SELECT COUNT(*) FROM groups"))
      {
        errors    = true;
        error_msg = "Couldn't count number of groups";
      }
      else
      {
        query.next();
        _num_groups = query.value(0).toInt();
      }

      if (!query.exec("SELECT COUNT(*) FROM candidates"))
      {
        errors    = true;
        error_msg = "Couldn't count number of candidates";
      }
      else
      {
        query.next();
        _num_cands = query.value(0).toInt();
      }
    }

    if (!errors)
    {
      if (!query.exec("SELECT party, party_ab FROM groups ORDER BY id"))
      {
        errors    = true;
        error_msg = "Couldn't read party names";
      }
      else
      {
        _atl_groups.clear();
        _atl_groups_short.clear();
        int i = 0;

        while (query.next())
        {
          const QString group_short = query.value(1).toString();
          _atl_groups.append(query.value(0).toString());
          _atl_groups_short.append(group_short);
          _group_from_short.insert(group_short, i);
          i++;
        }
      }
    }

    if (!errors)
    {
      if (!query.exec("SELECT party_ab, group_letter, group_pos, candidate FROM candidates ORDER BY id"))
      {
        errors    = true;
        error_msg = "Couldn't read candidate names";
      }
      else
      {
        _btl_names.clear();
        _btl_names_short.clear();
        int i     = 0;
        int group = -1;

        while (query.next())
        {
          QString this_party(query.value(0).toString());
          if (query.value(1).toString() == "UG")
          {
            this_party = "UG";
          }

          QString full_name   = query.value(3).toString();
          const int comma_pos = full_name.indexOf(",");
          if (comma_pos >= 0)
          {
            int n = 1;
            if (comma_pos == full_name.indexOf(", "))
            {
              n = 2;
            }
            full_name.replace(comma_pos, n, ",\n");
          }

          const QVariant group_pos = query.value(2);
          const QString short_name = this_party + "_" + group_pos.toString();
          if (group_pos.toInt() == 1)
          {
            group++;
            _candidates_per_group.push_back(std::vector<int>());
          }
          _btl_names.append(full_name);
          _btl_names_short.append(short_name);
          _cand_from_short.insert(short_name, i);
          _group_from_candidate.append(group);
          _candidates_per_group[group].push_back(i);
          i++;
        }
        // Empty list of candidates for Exhaust
        _candidates_per_group.push_back(std::vector<int>());
      }
    }

    if (!errors)
    {
      if (!query.exec("SELECT seat, formal_votes FROM seats ORDER BY id"))
      {
        errors    = true;
        error_msg = "Couldn't read divisions";
      }
      else
      {
        // The changed() signal from the divisions combobox will be emitted
        // if it's not blocked, leading to a potential crash when the program
        // tries to update the table but the data is missing.
        _combo_division->blockSignals(true);
        _combo_division->clear();

        _divisions.clear();
        _division_bboxes.clear();

        // The bounding box for each division will be set based
        // on its election-day booths; initialise the bounds so
        // that they'll be updated from the first read of a booth
        // coordinate:
        Bounding_box bbox;
        bbox.min_latitude  = 0.;
        bbox.max_latitude  = -80;
        bbox.min_longitude = 180.;
        bbox.max_longitude = 0.;

        while (query.next())
        {
          QString div(query.value(0).toString());
          _divisions.append(div);
          _combo_division->addItem(div);
          _division_formal_votes.append(query.value(1).toInt());
          _division_bboxes.append(bbox);
        }
        _combo_division->addItem(_state_full);
        _combo_division->setCurrentIndex(_divisions.length());

        _division_formal_votes.append(_total_formal_votes);
        _combo_division->blockSignals(false);
      }
    }

    if (!errors)
    {
      if (!query.exec("SELECT id, seat, booth, lon, lat, formal_votes FROM booths ORDER BY id"))
      {
        errors    = true;
        error_msg = "Couldn't read booths";
      }
      else
      {
        _booths.clear();

        int i = 0;

        while (query.next())
        {
          Booth this_booth;
          this_booth.id       = query.value(0).toInt();
          this_booth.division = query.value(1).toString();

          // I was worried that 3000 indexOf's would be slow, but it's fine:
          this_booth.division_id = _divisions.indexOf(this_booth.division);

          this_booth.booth        = query.value(2).toString();
          this_booth.longitude    = query.value(3).toDouble();
          this_booth.latitude     = query.value(4).toDouble();
          this_booth.formal_votes = query.value(5).toInt();

          if (i != this_booth.id)
          {
            errors    = true;
            error_msg = "Booth ID's not as expected";
            break;
          }

          if (this_booth.division_id < 0)
          {
            errors    = true;
            error_msg = QString("Error reading booths; couldn't find %1 in %2")
                          .arg(this_booth.booth, this_booth.division);
            break;
          }

          // Update bounding box based on election-day booths with non-zero lon/lat:
          if (!this_booth.booth.contains("PPVC") && !this_booth.booth.contains("PREPOLL", Qt::CaseInsensitive) &&
              !this_booth.booth.contains("Sydney (") && !this_booth.booth.contains("Adelaide (") && !this_booth.booth.contains("Perth (") &&
              !this_booth.booth.contains("Brisbane City (") && !this_booth.booth.contains("Hobart (") && !this_booth.booth.contains("Melbourne ("))
          {
            if (this_booth.latitude < -1. && this_booth.longitude > 1. && this_booth.formal_votes > 0)
            {
              const int j                       = this_booth.division_id;
              _division_bboxes[j].max_latitude  = qMax(_division_bboxes[j].max_latitude, this_booth.latitude);
              _division_bboxes[j].min_latitude  = qMin(_division_bboxes[j].min_latitude, this_booth.latitude);
              _division_bboxes[j].max_longitude = qMax(_division_bboxes[j].max_longitude, this_booth.longitude);
              _division_bboxes[j].min_longitude = qMin(_division_bboxes[j].min_longitude, this_booth.longitude);
            }
          }

          i++;
          _booths.append(this_booth);
        }
      }
    }

    db.close();
  }

  QSqlDatabase::removeDatabase(connection_name);

  if (errors)
  {
    QMessageBox msg_box;
    msg_box.setText(QString("Error: %1").arg(error_msg));
    msg_box.exec();

    _opened_database    = false;
    _database_file_path = "";
    _label_load->setText("No file selected");

    _button_cross_table->setEnabled(false);
    _button_abbreviations->setEnabled(false);
    _button_divisions_copy->setEnabled(false);
    _button_divisions_export->setEnabled(false);
    _button_divisions_booths_export->setEnabled(false);
    _button_divisions_cross_table->setEnabled(false);
    _button_booths_cross_table->setEnabled(false);
    _button_calculate_custom->setEnabled(false);

    _clear_divisions_table();

    return;
  }
  else
  {
    _opened_database    = true;
    _database_file_path = db_file;
    _set_table_groups();
    const int current_num_groups = get_num_groups();
    _spinbox_first_n_prefs->setMaximum(current_num_groups);
    _spinbox_later_prefs_up_to->setMaximum(current_num_groups);
    _spinbox_pref_sources_max->setMaximum(current_num_groups);
    _spinbox_pref_sources_min->setMaximum(_get_pref_sources_max());
    _spinbox_pref_sources_max->setMinimum(_get_pref_sources_min());

    _button_calculate_custom->setEnabled(true);

    _map_divisions_model.setup_list(_database_file_path, _state_short, _year, _divisions);
    _map_booths_model.setup_list(_booths, _get_map_booth_threshold());

    _setup_main_table();

    _add_column_to_main_table();
  }
}

void Widget::_set_table_groups()
{
  _table_main_groups.clear();
  _table_main_groups_short.clear();

  if (get_abtl() == "atl")
  {
    for (int i = 0; i < _atl_groups.length(); i++)
    {
      _table_main_groups.append(_atl_groups.at(i));
      _table_main_groups_short.append(_atl_groups_short.at(i));
    }
  }
  else
  {
    for (int i = 0; i < _btl_names.length(); i++)
    {
      _table_main_groups.append(_btl_names.at(i));
      _table_main_groups_short.append(_btl_names_short.at(i));
    }
  }
}

void Widget::_setup_main_table()
{
  _clear_main_table_data();

  const QString table_type = _get_table_type();
  if (table_type == Table_types::CUSTOM)
  {
    _label_custom_filtered_base->setText("");
    _table_main->verticalHeader()->hide();
    return;
  }

  const int current_num_groups = get_num_groups();
  _num_table_rows              = current_num_groups + 1;

  QString abtl         = get_abtl();
  const bool doing_atl = (abtl == "atl") ? true : false;

  _table_main_model->setRowCount(_num_table_rows);
  QStringList headers;

  if (table_type == Table_types::STEP_FORWARD)
  {
    const int default_prefs = 6;
    _table_main_model->setColumnCount(default_prefs);
    for (int i = 0; i < default_prefs; i++)
    {
      headers.append(QString("Pref %1").arg(i + 1));
    }
  }
  else if (table_type == Table_types::FIRST_N_PREFS)
  {
    const int num_prefs = _get_n_first_prefs();
    const int num_cols  = qMin(6, num_prefs);

    _table_main_model->setColumnCount(num_cols);

    for (int i = 0; i < num_cols; i++)
    {
      headers.append(QString("By %1").arg(num_prefs));
    }
  }
  else if (table_type == Table_types::LATER_PREFS)
  {
    const int fixed_prefs = _get_later_prefs_n_fixed();
    const int max_prefs   = _get_later_prefs_n_up_to();
    const int num_cols    = qMin(6, max_prefs);

    _table_main_model->setColumnCount(num_cols);
    for (int i = 0; i < fixed_prefs; i++)
    {
      headers.append(QString("Pref %1").arg(i + 1));
    }

    for (int i = fixed_prefs; i < num_cols; i++)
    {
      headers.append(QString("By %1").arg(max_prefs));
    }
  }
  else if (table_type == Table_types::PREF_SOURCES)
  {
    const int min_pref = _get_pref_sources_min();
    const int max_pref = _get_pref_sources_max();

    _table_main_model->setColumnCount(2);

    headers.append(QString("%1-%2").arg(min_pref).arg(max_pref));
    headers.append("Pref 1");
  }
  else if (table_type == Table_types::NPP)
  {
    _table_main_model->setColumnCount(2);

    headers.append("Pref 1");
    QString col_header = doing_atl ? "Group" : "Cand";
    headers.append(col_header);
  }

  _table_main_model->setHorizontalHeaderLabels(headers);
  if (table_type != Table_types::CUSTOM)
  {
    _make_main_table_row_headers(doing_atl || !_show_btl_headers);
  }
  _table_main->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  _table_main->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

  if (table_type == Table_types::NPP)
  {
    _table_main_model->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
  }
}

void Widget::_make_main_table_row_headers(bool is_blank)
{
  const int current_num_groups = get_num_groups();
  QStringList headers;

  if (!is_blank)
  {
    for (int i = 0; i < current_num_groups; i++)
    {
      headers.append(_table_main_groups.at(i));
    }

    if (_num_table_rows > current_num_groups)
    {
      headers.append(_get_table_type() == Table_types::NPP ? "Total" : "Exhaust");
    }

    _table_main_model->setVerticalHeaderLabels(headers);
    _table_main->verticalHeader()->show();
  }
  else
  {
    _table_main->verticalHeader()->hide();
    for (int i = 0; i < _num_table_rows; i++)
    {
      headers.append("");
    }
    _table_main_model->setVerticalHeaderLabels(headers);
  }
}

void Widget::_process_thread_sql_main_table(const QVector<QVector<int>>& col_data)
{
  const int col = _table_main_data.length() - 1;

  for (int i = 0; i < _num_table_rows; i++)
  {
    for (int j = 0; j < col_data.at(i).length(); j++)
    {
      _table_main_booth_data[col][i][j] += col_data.at(i).at(j);
    }
  }

  _completed_threads++;
  const QString table_type = _get_table_type();

  if (_completed_threads == _current_threads)
  {
    if (table_type == Table_types::NPP)
    {
      // Get the totals
      for (int i = 0; i < _num_table_rows - 1; i++)
      {
        for (int j = 0; j < col_data.at(i).length(); j++)
        {
          _table_main_booth_data[col][_num_table_rows - 1][j] += _table_main_booth_data[col][i][j];
        }
      }
    }

    // Sum each division's votes to get the division totals:
    for (int i = 0; i < _table_main_data.at(col).length(); i++)
    {
      int state_votes = 0;

      for (int j = 0; j < _table_main_booth_data.at(col).at(i).length(); j++)
      {
        const int this_votes = _table_main_booth_data.at(col).at(i).at(j);
        _table_main_data[col][i].votes[_booths.at(j).division_id] += this_votes;
        state_votes += this_votes;
      }

      _table_main_data[col][i].votes[_divisions.length()] = state_votes;
    }

    if (!_sort_ballot_order)
    {
      _sort_table_column(col);
    }

    if (table_type == Table_types::NPP)
    {
      // We should only be here for the initial setup of the NPP table,
      // when we get the primary votes.
      _set_main_table_cells(col);
      const int current_num_groups = get_num_groups();
      const int h                  = _table_main->fontMetrics().boundingRect("0").height();

      for (int i = 0; i <= current_num_groups; i++)
      {
        int group_id = _table_main_data.at(0).at(i).group_id;
        _table_main_model->setItem(i, 1, new QStandardItem(_get_short_group(group_id)));
        _table_main_model->item(i, 1)->setTextAlignment(Qt::AlignCenter);

        if (!_show_btl_headers)
        {
          _table_main->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
          _table_main->verticalHeader()->setDefaultSectionSize(h);
        }
      }
    }
    else
    {
      _set_main_table_cells(col);
    }

    _show_calculation_time();
    _unlock_main_interface();
  }
  else
  {
    _label_progress->setText(QString("%1/%2 complete").arg(_completed_threads).arg(_current_threads));
  }
}

void Widget::_write_sql_to_file(const QString& q)
{
  const QString file_name = QString("%1/last_sql_query.txt")
                              .arg(QCoreApplication::applicationDirPath());

  QFile file(file_name);

  if (file.open(QIODevice::WriteOnly))
  {
    QTextStream out(&file);
    out << q << endl;
    file.close();
  }
}

void Widget::_write_custom_operations_to_file()
{
  const QString file_name = QString("%1/last_custom_operations.txt")
                              .arg(QCoreApplication::applicationDirPath());

  QFile file(file_name);

  if (file.open(QIODevice::WriteOnly))
  {
    QTextStream out(&file);

    if (!_custom_filter_sql.isEmpty() && !(_custom_filter_sql == NO_FILTER))
    {
      out << "Filter SQL:\n" + _custom_filter_sql + "\n\n";
    }

    if (_custom_filter_operations.size() > 0)
    {
      out << "Filter operations:\n" + Custom_operations::operations_table_string(_custom_filter_operations) + "\n";
    }

    if (_custom_row_operations.size() > 0)
    {
      out << "Row shortcut operations:\n" + Custom_operations::operations_table_string(_custom_row_operations) + "\n";
    }

    if (_custom_col_operations.size() > 0)
    {
      out << "Column shortcut operations:\n" + Custom_operations::operations_table_string(_custom_col_operations) + "\n";
    }

    if (_custom_cell_operations.size() > 0)
    {
      out << "Cell operations:\n" + Custom_operations::operations_table_string(_custom_cell_operations) + "\n";
    }

    out << "\n";

    file.close();
  }
}

void Widget::_do_sql_query_for_table(const QString& q, bool wide_table)
{
  // If wide_table is true, then the SQL query q should return a table with
  // one row per division, and one column per group.
  //
  // If wide_table is false, then it should return a three-column table (division_ID, group_ID, votes).

  _write_sql_to_file(q);

  // I'm not sure if clicked_cells is ever longer than wanted, but just in case:
  QVector<int> relevant_clicked_cells;

  for (int i = 0; i < _table_main_data.length() - 1; i++)
  {
    relevant_clicked_cells.append(_clicked_cells.at(i));
  }

  const int current_num_groups = get_num_groups();
  const int num_booths         = _booths.length();

  int num_threads     = 1;
  QStringList queries = _queries_threaded(q, num_threads);

  _current_threads   = num_threads;
  _completed_threads = 0;

  _lock_main_interface();
  _label_progress->setText("Calculating...");

  for (int i = 0; i < num_threads; i++)
  {
    QThread* thread               = new QThread;
    Worker_sql_main_table* worker = new Worker_sql_main_table(i,
                                                              _database_file_path,
                                                              queries.at(i),
                                                              wide_table,
                                                              current_num_groups,
                                                              _num_table_rows,
                                                              num_booths,
                                                              relevant_clicked_cells);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &Worker_sql_main_table::do_query);
    connect(worker, &Worker_sql_main_table::finished_query, this, &Widget::_process_thread_sql_main_table);
    connect(worker, &Worker_sql_main_table::finished_query, thread, &QThread::quit);
    connect(worker, &Worker_sql_main_table::finished_query, worker, &Worker_sql_main_table::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    thread->start();
  }
}

QStringList Widget::_queries_threaded(const QString& q, int& num_threads, bool one_thread)
{
  const QString abtl   = get_abtl();
  const int max_record = (abtl == "atl" ? _total_atl_votes : _total_btl_votes) - 1;

  num_threads = max_record > 10000 ? QThread::idealThreadCount() : 1;

  if (one_thread)
  {
    num_threads = 1;
  }

  QStringList queries;
  if (num_threads == 1)
  {
    queries.append(q);
  }
  else
  {
    const bool q_has_where = q.contains("WHERE");

    for (int i = 0; i < num_threads; i++)
    {
      const int id_1 = (i == 0) ? 0 : max_record * i / num_threads + 1;
      const int id_2 = max_record * (i + 1) / num_threads;

      QString where_clause = QString("(id BETWEEN %1 AND %2)").arg(id_1).arg(id_2);

      queries.append(q);

      if (q_has_where)
      {
        queries[i].replace(QString("WHERE"), QString("WHERE %1 AND ").arg(where_clause));
      }
      else
      {
        queries[i].replace(QString("FROM %1").arg(abtl), QString("FROM %1 WHERE %2 ").arg(abtl, where_clause));
      }
    }
  }

  return queries;
}

void Widget::_sort_table_column(int i)
{
  QVector<int> indices;
  for (int j = 0; j < _table_main_data.at(i).length(); j++)
  {
    indices.append(j);
  }

  if (_sort_ballot_order)
  {
    std::sort(_table_main_data[i].begin(), _table_main_data[i].end(), [&](Table_main_item a, Table_main_item b) -> bool
              { return a.group_id < b.group_id; });
  }
  else
  {
    int current_div = _get_current_division();

    std::sort(_table_main_data[i].begin(), _table_main_data[i].end(), [&](Table_main_item a, Table_main_item b) -> bool
              {
      if (a.votes.at(current_div) == b.votes.at(current_div))
      {
        return a.group_id < b.group_id;
      }
      return a.votes.at(current_div) > b.votes.at(current_div); });
  }

  // Get the indices:
  std::sort(indices.begin(), indices.end(), [&](int a, int b) -> bool
            { return _table_main_data.at(i).at(a).group_id < _table_main_data.at(i).at(b).group_id; });

  const int current_num_groups = get_num_groups();
  const bool doing_btl         = (get_abtl() == "btl");

  for (int j = 0; j < _table_main_data.at(i).length(); j++)
  {
    _table_main_data[i][j].sorted_idx = indices[j];
  }

  if (i == 0 && doing_btl)
  {
    // Update the row headers
    for (int j = 0; j < _table_main_data.at(i).length(); j++)
    {
      QString header;
      if (_table_main_data.at(0).at(j).group_id == current_num_groups)
      {
        header = _get_table_type() == Table_types::NPP ? "Total" : "Exhaust";
      }
      else
      {
        header = _table_main_groups.at(_table_main_data.at(0).at(j).group_id);
      }

      _table_main_model->setVerticalHeaderItem(j, new QStandardItem(header));
    }
  }
}

void Widget::_sort_main_table_npp()
{
  const int num_table_cols     = _table_main_data.length();
  const int current_num_groups = get_num_groups();

  if (_sort_npp.i == 1)
  {
    // Sort by group
    for (int j = 0; j < num_table_cols; j++)
    {
      std::sort(_table_main_data[j].begin(), _table_main_data[j].end(), [&](Table_main_item a, Table_main_item b) -> bool
                { return (a.group_id < b.group_id); });

      for (int i = 0; i <= current_num_groups; i++)
      {
        _table_main_data[j][i].sorted_idx = i;
      }
    }
  }
  else
  {
    const int i           = (_sort_npp.i == 0) ? 0 : _sort_npp.i - 1;
    const int current_div = _get_current_division();

    QVector<int> indices;

    for (int j = 0; j <= current_num_groups; j++)
    {
      indices.append(j);
    }

    if (_get_value_type() == VALUE_PERCENTAGES && i > 0)
    {
      std::sort(_table_main_data[i].begin(), _table_main_data[i].end(), [&](Table_main_item a, Table_main_item b) -> bool
                {
        if (a.group_id == b.group_id) { return false; }
        if (_clicked_n_parties.indexOf(a.group_id) >= 0) { return false; }
        if (_clicked_n_parties.indexOf(b.group_id) >= 0) { return true;  }

        if (qAbs(a.percentages.at(current_div) - b.percentages.at(current_div)) < 1.e-10)
        {
          return (a.group_id < b.group_id) != _sort_npp.is_descending;
        }

        return (a.percentages.at(current_div) < b.percentages.at(current_div)) != _sort_npp.is_descending; });
    }
    else
    {
      std::sort(_table_main_data[i].begin(), _table_main_data[i].end(), [&](Table_main_item a, Table_main_item b) -> bool
                {
        if (a.group_id == b.group_id) { return false; }
        if (_clicked_n_parties.indexOf(a.group_id) >= 0) { return false; }
        if (_clicked_n_parties.indexOf(b.group_id) >= 0) { return true;  }

        if (a.votes.at(current_div) == b.votes.at(current_div))
        {
          return (a.group_id < b.group_id) != _sort_npp.is_descending;
        }

        return (a.votes.at(current_div) < b.votes.at(current_div)) != _sort_npp.is_descending; });
    }

    std::sort(indices.begin(), indices.end(), [&](int a, int b) -> bool
              { return _table_main_data.at(i).at(a).group_id < _table_main_data.at(i).at(b).group_id; });

    for (int j = 0; j < num_table_cols; j++)
    {
      if (j != i)
      {
        std::sort(_table_main_data[j].begin(), _table_main_data[j].end(), [&](Table_main_item a, Table_main_item b) -> bool
                  { return indices[a.group_id] < indices[b.group_id]; });
      }

      for (int k = 0; k <= current_num_groups; k++)
      {
        _table_main_data[j][k].sorted_idx = indices[k];
      }
    }
  }

  _set_all_main_table_cells();
  for (int i = 0; i <= current_num_groups; i++)
  {
    _table_main_model->setItem(i, 1, new QStandardItem(_get_short_group(_table_main_data.at(0).at(i).group_id)));
    _table_main_model->item(i, 1)->setTextAlignment(Qt::AlignCenter);
  }

  if (get_abtl() == "btl")
  {
    for (int i = 0; i < _num_table_rows; i++)
    {
      const int group_id        = _table_main_data.at(0).at(i).group_id;
      const QString header_text = (group_id >= current_num_groups) ? "Total" : _table_main_groups.at(_table_main_data.at(0).at(i).group_id);
      _table_main_model->setVerticalHeaderItem(i, new QStandardItem(header_text));
    }
  }

  // Cell highlights/fades
  if (_clicked_n_parties.length() > 0)
  {
    for (int j = 0; j <= current_num_groups; j++)
    {
      if (_clicked_n_parties.indexOf(_table_main_data.at(0).at(j).group_id) >= 0)
      {
        _highlight_cell_n_party_preferred(j, 1);
      }
      else
      {
        _unhighlight_cell(j, 1);
      }
    }
  }

  if (_clicked_cells_two_axis.length() == 1)
  {
    // Following line is to handle exhaust, which might be coded as 999:
    const int clicked_i = qMin(_clicked_cells_two_axis.at(0).i, current_num_groups);

    for (int i = 1; i < _table_main_data.length(); i++)
    {
      for (int j = 0; j <= current_num_groups; j++)
      {
        if (clicked_i == _table_main_data.at(i).at(j).group_id && _clicked_cells_two_axis.at(0).j == i + 1)
        {
          _highlight_cell(j, i + 1);
        }
        else
        {
          _fade_cell(j, i + 1);
        }
      }
    }
  }
}

void Widget::_sort_main_table_rows_custom(int clicked_col)
{
  if (_doing_calculation)
  {
    return;
  }

  const int num_data_cols = _table_main_data.length();
  if (num_data_cols == 0)
  {
    return;
  }
  _custom_sort_indices_rows.clear();
  const int num_rows = _table_main_data.at(0).length();
  for (int i = 0; i < num_rows; ++i)
  {
    _custom_sort_indices_rows.append(i);
  }

  std::function<bool(int, int)> comparator;

  const bool sort_by_categories = (clicked_col < 0) || ((num_data_cols == 1) ? (clicked_col == 0) : (clicked_col == 1));
  if (sort_by_categories)
  {
    const int sort_col = -1;
    if (_sort_custom_rows.i == sort_col)
    {
      _sort_custom_rows.is_descending = !_sort_custom_rows.is_descending;
    }
    else
    {
      // Default to ascending order for categories.
      _sort_custom_rows.is_descending = false;
    }

    _sort_custom_rows.i = sort_col;

    // Underlying table data is already sorted by category; only potential need
    // is to swap the ordering.
    comparator = [this](int a, int b) -> bool
    {
      return (a < b) != _sort_custom_rows.is_descending;
    };
  }
  else
  {
    const int clicked_data_col = (num_data_cols == 1) ? 0 : qMax(0, clicked_col - 1);
    const int data_col         = _custom_sort_indices_cols.at(clicked_data_col);

    if (_sort_custom_rows.i == data_col)
    {
      _sort_custom_rows.is_descending = !_sort_custom_rows.is_descending;
    }
    else
    {
      _sort_custom_rows.is_descending = true;
    }

    _sort_custom_rows.i = data_col;

    const int current_div      = _get_current_division();
    const bool use_percentages = _get_value_type() == VALUE_PERCENTAGES;

    if (use_percentages)
    {
      comparator = [this, data_col, current_div](int a, int b) -> bool
      {
        if (a == b)
        {
          return false;
        }

        const double a_perc = _table_main_data.at(data_col).at(a).percentages.at(current_div);
        const double b_perc = _table_main_data.at(data_col).at(b).percentages.at(current_div);

        if (qAbs(a_perc - b_perc) < 1.e-10)
        {
          return (a < b) != _sort_custom_rows.is_descending;
        }

        return (a_perc < b_perc) != _sort_custom_rows.is_descending;
      };
    }
    else
    {
      comparator = [this, data_col, current_div](int a, int b) -> bool
      {
        if (a == b)
        {
          return false;
        }

        const int a_votes = _table_main_data.at(data_col).at(a).votes.at(current_div);
        const int b_votes = _table_main_data.at(data_col).at(b).votes.at(current_div);

        if (a_votes == b_votes)
        {
          return (a < b) != _sort_custom_rows.is_descending;
        }

        return (a_votes < b_votes) != _sort_custom_rows.is_descending;
      };
    }
  }

  std::sort(_custom_sort_indices_rows.begin(), _custom_sort_indices_rows.end(), comparator);

  _set_all_main_table_cells_custom();
}

void Widget::_sort_main_table_cols_custom(int clicked_row)
{
  const int num_data_cols = _table_main_data.length();
  if (num_data_cols < 2)
  {
    return;
  }
  // use "row" of -1 for sorting by the categories.
  const int row = clicked_row < 0 ? clicked_row : _custom_sort_indices_rows.at(clicked_row);

  if (_sort_custom_cols.i == row)
  {
    _sort_custom_cols.is_descending = !_sort_custom_cols.is_descending;
  }
  else
  {
    _sort_custom_cols.is_descending = true;
  }

  _sort_custom_cols.i = row;

  const int current_div = _get_current_division();

  _custom_sort_indices_cols.clear();
  const int num_sortable_cols = num_data_cols - 1;
  for (int i = 0; i <= num_sortable_cols; ++i)
  {
    _custom_sort_indices_cols.append(i);
  }

  if (row >= 0)
  {
    // Switching between votes, percentages, and total percentages never changes
    // the sorting of a row, since all percentages in a row use the same base
    // denominator.
    auto comparator = [this, row, current_div](int a, int b) -> bool
    {
      if (a == b)
      {
        return false;
      }

      const int a_votes = _table_main_data.at(a).at(row).votes.at(current_div);
      const int b_votes = _table_main_data.at(b).at(row).votes.at(current_div);

      if (a_votes == b_votes)
      {
        return (a < b) != _sort_custom_cols.is_descending;
      }

      return (a_votes < b_votes) != _sort_custom_cols.is_descending;
    };

    std::sort(_custom_sort_indices_cols.begin() + 1, _custom_sort_indices_cols.end(), comparator);
  }

  _set_all_main_table_cells_custom();
}

void Widget::_sort_main_table_custom()
{
  // Call both _sort_main_table_rows_custom and _sort_main_table_cols_custom
  // with the current settings in _sort_custom_rows and _sort_custom_cols.
  // Hack: flipping is_descending on each call, so that it'll get flipped back.
  _custom_sort_indices_rows.clear();
  const int num_rows = _table_main_data.at(0).length();
  for (int i = 0; i < num_rows; ++i)
  {
    _custom_sort_indices_rows.append(i);
  }
  _sort_custom_rows.is_descending = !_sort_custom_rows.is_descending;

  int pseudo_clicked_col = -1;
  if (_sort_custom_rows.i >= 0)
  {
    const int num_table_cols = _table_main_model->columnCount();
    // Category, Base; or
    // Base, Category, Data
    if (num_table_cols == 2)
    {
      pseudo_clicked_col = 1;
    }
    else
    {
      pseudo_clicked_col = _sort_custom_rows.i == 0 ? 0 : _sort_custom_rows.i + 1;
    }
  }

  _sort_main_table_rows_custom(pseudo_clicked_col);

  _custom_sort_indices_cols.clear();
  const int num_sortable_cols = _table_main_data.length() - 1;
  for (int i = 0; i <= num_sortable_cols; ++i)
  {
    _custom_sort_indices_rows.append(i);
  }
  _sort_custom_cols.is_descending = !_sort_custom_cols.is_descending;
  int pseudo_clicked_row          = -1;
  if (_sort_custom_cols.i >= 0)
  {
    for (int i = 0; i < num_rows; ++i)
    {
      if (_custom_sort_indices_rows.at(i) == _sort_custom_cols.i)
      {
        pseudo_clicked_row = i;
        break;
      }
    }
  }
  _sort_main_table_cols_custom(pseudo_clicked_row);
}

void Widget::_set_all_main_table_cells()
{
  if (_get_table_type() == Table_types::CUSTOM)
  {
    _set_all_main_table_cells_custom();
    return;
  }

  for (int i = 0; i < _table_main_data.length(); i++)
  {
    _set_main_table_cells(i);
  }
}

void Widget::_set_all_main_table_cells_custom()
{
  if (_get_table_type() != Table_types::CUSTOM)
  {
    return;
  }
  if (_table_main_data.length() == 0)
  {
    return;
  }
  if (_table_main_data.at(0).length() == 0)
  {
    return;
  }

  const QString value_type = _get_value_type();
  const int current_div    = _get_current_division();

  const int num_rows = _table_main_data.at(0).length();

  const bool have_highlight = _clicked_cells_two_axis.length() == 1;
  const int highlight_i     = have_highlight ? _clicked_cells_two_axis.at(0).i : -1;
  const int highlight_j     = have_highlight ? _clicked_cells_two_axis.at(0).j : -1;

  if (_custom_table_row_headers.length() != num_rows)
  {
    const QString msg = "Programmer error: mismatching rows and row headers.  Sorry :(";
    _show_error_message(msg);
    return;
  }

  const int total_base = _table_main_data_total_base.votes.at(current_div);
  if (value_type == VALUE_VOTES)
  {
    _label_custom_filtered_base->setText(QString::number(total_base));
  }
  else
  {
    const int formal = _division_formal_votes.at(current_div);
    _label_custom_filtered_base->setText(QString::number(100. * total_base / formal, 'f', 2));
  }

  const int num_data_cols       = _table_main_data.length();
  const bool rows_is_none       = (_custom_rows.type == Custom_axis_type::NONE);
  const int rows_is_none_offset = rows_is_none ? 1 : 0;
  const int row_header_col      = rows_is_none ? -1 : num_data_cols == 1 ? 0
                                                                         : 1;

  auto get_table_col = [num_data_cols, rows_is_none_offset](int data_col)
  {
    if (num_data_cols > 1 && data_col == 0)
    {
      return 0;
    }
    return data_col + 1 - rows_is_none_offset;
  };

  // Row headers
  if (!rows_is_none)
  {
    for (int i_row = 0; i_row < num_rows; ++i_row)
    {
      const int i_row_read = _custom_sort_indices_rows.at(i_row);
      _table_main_model->setItem(i_row, row_header_col, new QStandardItem(_custom_table_row_headers.at(i_row_read)));
      _table_main_model->item(i_row, row_header_col)->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    }
  }

  QStringList col_headers;
  col_headers.append(_table_main_model->horizontalHeaderItem(0)->text());
  const int n_unsortable_cols = get_table_col(1);
  if (n_unsortable_cols > 1)
  {
    col_headers.append(_table_main_model->horizontalHeaderItem(1)->text());
  }

  for (int i_data_col = 0; i_data_col < num_data_cols; ++i_data_col)
  {
    const int i_col      = get_table_col(i_data_col);
    const int i_col_read = _custom_sort_indices_cols.at(i_data_col);

    if (i_col > 1 - rows_is_none_offset)
    {
      // Table cols go: Base, [Row-header-title], sortable data
      // Data  cols go: Base, sortable data
      // _custom_table_col_headers contains only the headers for the sortable
      // data, so subtract one for the Base which is not included.
      //_table_main_model->horizontalHeaderItem(i_col)->setText(_custom_table_col_headers.at(i_col_read - 1));
      col_headers.append(_custom_table_col_headers.at(i_col_read - 1));
    }

    for (int i_row = 0; i_row < num_rows; ++i_row)
    {
      const int i_row_read = _custom_sort_indices_rows.at(i_row);
      QString cell_text;
      const int votes = _table_main_data.at(i_col_read).at(i_row_read).votes.at(current_div);
      if (value_type == VALUE_VOTES)
      {
        cell_text = QString::number(votes);
      }
      else
      {
        const int denominator = qMax(
          1,
          value_type == VALUE_TOTAL_PERCENTAGES ? _division_formal_votes.at(current_div)
          : i_data_col == 0                     ? total_base
                                                : _table_main_data.at(0).at(i_row_read).votes.at(current_div));
        const double percentage = 100. * votes / static_cast<double>(denominator);
        cell_text               = QString::number(percentage, 'f', 2);
      }


      auto item = _table_main_model->item(i_row, i_col);
      // For performance reasons when re-rendering a table after sorting, only
      // create a new QStandardItem if one doesn't already exist.
      if (item == nullptr)
      {
        _table_main_model->setItem(i_row, i_col, new QStandardItem(cell_text));
        _table_main_model->item(i_row, i_col)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      }
      else
      {
        item->setText(cell_text);
      }

      if (i_row_read == highlight_i && i_col_read == highlight_j)
      {
        _highlight_cell(i_row, i_col);
      }
      else
      {
        _unhighlight_cell(i_row, i_col);
      }
    }
  }

  QHeaderView* horizontal_header = _table_main->horizontalHeader();
  horizontal_header->setSectionResizeMode(QHeaderView::Fixed);

  _table_main_model->setHorizontalHeaderLabels(col_headers);
  _table_main->resizeColumnsToContents();
  horizontal_header->setSectionResizeMode(QHeaderView::ResizeToContents);

  _set_main_table_row_height();
  // I hadn't intended for the following lines to be necessary, but if you
  // switch to the custom mode before loading a database, then the table isn't
  // initialised correctly.
  _table_main->verticalHeader()->hide();
}

void Widget::_set_main_table_cells(int col)
{
  // n_party_preferred tables have group abbreviations in the second
  // column and in no other cells.
  const QString table_type     = _get_table_type();
  const bool n_party_preferred = (table_type == Table_types::NPP);

  const int current_div    = _get_current_division();
  const QString value_type = _get_value_type();

  const int current_cols = _table_main_model->columnCount();

  if (current_cols == col)
  {
    QString header;
    if (table_type == Table_types::STEP_FORWARD)
    {
      header = QString("Pref %1").arg(col + 1);
    }
    else if (table_type == Table_types::FIRST_N_PREFS)
    {
      header = QString("By %1").arg(_get_n_first_prefs());
    }
    else if (table_type == Table_types::LATER_PREFS)
    {
      const int fixed = _get_later_prefs_n_fixed();
      const int up_to = _get_later_prefs_n_up_to();

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

    _table_main_model->setColumnCount(current_cols + 1);
    _table_main_model->setHorizontalHeaderItem(col, new QStandardItem(header));
  }

  int percentage_denominator = 1;
  if (value_type == VALUE_PERCENTAGES)
  {
    if (col == 0)
    {
      percentage_denominator = _division_formal_votes.at(current_div);
    }
    else
    {
      if (!n_party_preferred)
      {
        if (_clicked_cells.length() >= col)
        {
          const int idx          = _table_main_data.at(col - 1).at(_clicked_cells.at(col - 1)).sorted_idx;
          percentage_denominator = qMax(1, _table_main_data.at(col - 1).at(idx).votes.at(current_div));
        }
        else
        {
          QMessageBox msg_box;
          msg_box.setText("Internal error: Mismatch between table data length and clicked cells.  Oops. :(");
          msg_box.exec();
          return;
        }
      }
      else
      {
        // With NPP, the denominator is different on each row.
      }
    }
  }
  else if (value_type == VALUE_TOTAL_PERCENTAGES)
  {
    percentage_denominator = _division_formal_votes.at(current_div);
  }

  int table_col = col;
  if (n_party_preferred && col > 0)
  {
    table_col++;
  }

  for (int i = 0; i < _table_main_data.at(col).length(); i++)
  {
    const int votes = _table_main_data.at(col)[i].votes.at(current_div);

    QString cell_text;
    if (votes == 0)
    {
      if ((table_type == Table_types::STEP_FORWARD || table_type == Table_types::LATER_PREFS)
           && col == 0 && _table_main_data.at(col).at(i).group_id >= get_num_groups())
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
        const QString gp = _get_short_group(_table_main_data.at(col).at(i).group_id);

        if (value_type == VALUE_VOTES)
        {
          cell_text = QString("%1\n%2").arg(gp).arg(votes);
        }
        else
        {
          const double vote_percentage = 100. * votes / static_cast<double>(percentage_denominator);
          cell_text                    = QString("%1\n%2").arg(gp).arg(vote_percentage, 0, 'f', 2);
        }
      }
      else
      {
        if (value_type == VALUE_VOTES)
        {
          cell_text = QString("%1").arg(votes);
        }
        else
        {
          double vote_percentage;
          if (value_type == VALUE_TOTAL_PERCENTAGES || col == 0)
          {
            vote_percentage = 100. * votes / static_cast<double>(_division_formal_votes.at(current_div));
          }
          else
          {
            percentage_denominator = _table_main_data.at(0).at(i).votes.at(current_div);
            if (percentage_denominator < 1)
            {
              percentage_denominator = 1;
            }
            vote_percentage = 100. * votes / static_cast<double>(percentage_denominator);
          }
          cell_text = QString("%1").arg(vote_percentage, 0, 'f', 2);
        }
      }
    }

    _table_main_model->setItem(i, table_col, new QStandardItem(cell_text));

    if (table_type == Table_types::NPP)
    {
      _table_main_model->item(i, table_col)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    else
    {
      _table_main_model->item(i, table_col)->setTextAlignment(Qt::AlignCenter);
    }

    if (cell_text == "")
    {
      _unhighlight_cell(i, table_col);
    }
    else
    {
      if (n_party_preferred)
      {
        if (_clicked_cells_two_axis.length() > 0 && _clicked_cells_two_axis.at(0).j == col &&
            _clicked_cells_two_axis.at(0).i == _table_main_data.at(col).at(i).group_id)
        {
          _highlight_cell(i, col);
        }
      }
      else
      {
        if (col < _clicked_cells.length() && _clicked_cells.at(col) == _table_main_data.at(col).at(i).group_id)
        {
          _highlight_cell(i, col);
        }
      }
    }
  }

  _set_main_table_row_height();
}

void Widget::_set_main_table_row_height()
{
  const bool btl           = get_abtl() == "btl";
  const QString table_type = _get_table_type();

  if ((table_type == Table_types::NPP && (!btl || (btl && !_show_btl_headers))) || table_type == Table_types::CUSTOM)
  {
    _table_main->verticalHeader()->setDefaultSectionSize(_one_line_height);
  }
  else
  {
    _table_main->verticalHeader()->setDefaultSectionSize(_two_line_height);
  }
}

void Widget::_add_column_to_main_table()
{
  // For table types that aren't n_party_preferred, this function does what it says
  // and adds a column to the main table.
  //
  // For n_party_preferred, this function should only be called to initialise the table,
  // and it adds the first column (primary votes) and the second column (group names).

  _timer.restart();
  _button_calculate_after_spinbox->setEnabled(false);

  QString table_type = _get_table_type();
  if (table_type == Table_types::CUSTOM)
  {
    return;
  }

  const int num_clicked_cells = _clicked_cells.length();

  if (table_type != Table_types::NPP && num_clicked_cells != _table_main_data.length())
  {
    QMessageBox msg_box;
    msg_box.setText("Internal error: Mismatch between table data length and clicked cells.  Oops. :(");
    msg_box.exec();
    return;
  }

  // Initialise a new column of the table data:
  _table_main_data.append(QVector<Table_main_item>());
  _table_main_booth_data.append(QVector<QVector<int>>());
  const int col = _table_main_data.length() - 1;

  for (int i = 0; i < _num_table_rows; i++)
  {
    _table_main_data[col].append(Table_main_item());
    _table_main_data[col][i].group_id   = i;
    _table_main_data[col][i].sorted_idx = i;
    _table_main_data[col][i].votes      = QVector<int>();

    for (int j = 0; j <= _divisions.length(); j++)
    {
      _table_main_data[col][i].votes.append(0);
    }

    _table_main_booth_data[col].append(QVector<int>());
    for (int j = 0; j < _booths.length(); j++)
    {
      _table_main_booth_data[col][i].append(0);
    }
  }

  const int current_num_groups = get_num_groups();

  if (table_type == Table_types::STEP_FORWARD)
  {
    QString query_where("");

    const int this_pref = col + 1;
    if (this_pref > 1)
    {
      query_where = "WHERE";
      QString and_str("");
      for (int i = 1; i < this_pref; i++)
      {
        if (i == 2)
        {
          and_str = " AND ";
        }
        query_where = QString("%1 %2 P%3 = %4").arg(query_where, and_str, QString::number(i), QString::number(_clicked_cells.at(i - 1)));
      }
    }

    const QString query = QString("SELECT booth_id, P%1, COUNT(P%1) FROM %2 %3 GROUP BY booth_id, P%1")
                            .arg(QString::number(this_pref), get_abtl(), query_where);

    _do_sql_query_for_table(query, false);
  }
  else if (table_type == Table_types::FIRST_N_PREFS)
  {
    QString query_where("");

    const int by_pref   = _get_n_first_prefs();
    const int num_terms = col + 1;
    if (num_terms > 1)
    {
      query_where = "WHERE";
      QString and_str("");
      for (int i = 1; i < num_terms; i++)
      {
        if (i == 2)
        {
          and_str = " AND ";
        }
        int gp = _clicked_cells.at(i - 1);

        QString cond;
        if (gp < current_num_groups)
        {
          cond = QString("Pfor%1 <= %2").arg(gp).arg(by_pref);
        }
        else
        {
          cond = QString("num_prefs < %1").arg(by_pref);
        }

        query_where = QString("%1 %2 %3").arg(query_where, and_str, cond);
      }
    }

    QString query = QString("SELECT booth_id");
    for (int i = 0; i < current_num_groups; i++)
    {
      query += QString(", SUM(Pfor%1 <= %2)").arg(i).arg(by_pref);
    }
    query += QString(", SUM(num_prefs < %1)").arg(by_pref);

    query += QString(", COUNT(id) FROM %1 %2 GROUP BY booth_id").arg(get_abtl(), query_where);

    _do_sql_query_for_table(query, true);
  }
  else if (table_type == Table_types::LATER_PREFS)
  {
    const int fixed_prefs = _get_later_prefs_n_fixed();
    const int by_pref     = _get_later_prefs_n_up_to();

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
          if (i == 2)
          {
            and_str = " AND ";
          }
          query_where = QString("%1 %2 P%3 = %4").arg(query_where, and_str, QString::number(i), QString::number(_clicked_cells.at(i - 1)));
        }
      }

      QString query = QString("SELECT booth_id, P%1, COUNT(P%1) FROM %2 %3 GROUP BY booth_id, P%1")
                        .arg(QString::number(this_pref), get_abtl(), query_where);

      _do_sql_query_for_table(query, false);
    }
    else
    {
      // This is first_n_prefs with a different WHERE expression.
      // There are two parts to the WHERE: the step-forward part and the later-prefs part.
      QString query_where("WHERE");
      QString and_str("");

      // Step-forward-style WHERE clause:
      for (int i = 1; i <= fixed_prefs; i++)
      {
        if (i == 2)
        {
          and_str = " AND ";
        }
        query_where = QString("%1 %2 P%3 = %4").arg(query_where, and_str, QString::number(i), QString::number(_clicked_cells.at(i - 1)));
      }

      // first-n-prefs-style WHERE clause (possibly empty):
      for (int i = fixed_prefs; i < col; i++)
      {
        const int gp = _clicked_cells.at(i);

        QString cond;
        if (gp < current_num_groups)
        {
          cond = QString("Pfor%1 <= %2").arg(gp).arg(by_pref);
        }
        else
        {
          cond = QString("num_prefs < %1").arg(by_pref);
        }

        query_where = QString("%1 AND %2").arg(query_where, cond);
      }

      QString query = QString("SELECT booth_id");
      for (int i = 0; i < current_num_groups; i++)
      {
        query += QString(", SUM(Pfor%1 <= %2)").arg(i).arg(by_pref);
      }
      query += QString(", SUM(num_prefs < %1)").arg(by_pref);

      query += QString(", COUNT(id) FROM %1 %2 GROUP BY booth_id").arg(get_abtl(), query_where);

      _do_sql_query_for_table(query, true);
    }
  }
  else if (table_type == Table_types::PREF_SOURCES)
  {
    const int min_pref = _get_pref_sources_min();
    const int max_pref = _get_pref_sources_max();

    if (col == 0)
    {
      QString query = QString("SELECT booth_id");
      for (int i = 0; i < current_num_groups; i++)
      {
        query += QString(", SUM(Pfor%1 BETWEEN %2 AND %3)").arg(i).arg(min_pref).arg(max_pref);
      }
      query += QString(", SUM(num_prefs BETWEEN %1 AND %2)").arg(min_pref - 1).arg(max_pref - 1);

      query += QString(", COUNT(id) FROM %1 GROUP BY booth_id").arg(get_abtl());

      _do_sql_query_for_table(query, true);
    }
    else
    {
      QString query;
      if (_clicked_cells.at(0) == current_num_groups)
      {
        // Exhaust
        query = QString("SELECT booth_id, P1, COUNT(P1) FROM %1 WHERE num_prefs BETWEEN %2 and %3 GROUP BY booth_id, P1")
                  .arg(get_abtl(), QString::number(min_pref - 1), QString::number(max_pref - 1));
      }
      else
      {
        query = QString("SELECT booth_id, P1, COUNT(P1) FROM %1 WHERE Pfor%2 BETWEEN %3 and %4 GROUP BY booth_id, P1")
                  .arg(get_abtl(), QString::number(_clicked_cells.at(0)), QString::number(min_pref), QString::number(max_pref));
      }

      _do_sql_query_for_table(query, false);
    }
  }
  else if (table_type == Table_types::NPP)
  {
    const QString query = QString("SELECT booth_id, P1, COUNT(P1) FROM %1 GROUP BY booth_id, P1").arg(get_abtl());

    _do_sql_query_for_table(query, false);
  }
}

void Widget::_calculate_n_party_preferred()
{
  _timer.restart();
  const int n          = _get_n_preferred();
  const int num_booths = _booths.length();

  // Clear the table data, just in case.
  for (int i = _table_main_data.length() - 1; i > 0; i--)
  {
    _table_main_data.remove(i);
    _table_main_booth_data.remove(i);
  }

  // Initialise the table data:
  for (int i = 0; i <= n; i++)
  {
    _table_main_data.append(QVector<Table_main_item>());
    _table_main_booth_data.append(QVector<QVector<int>>());

    for (int j = 0; j < _num_table_rows; j++)
    {
      _table_main_data[i + 1].append(Table_main_item());
      _table_main_data[i + 1][j].group_id    = _table_main_data.at(0).at(j).group_id;
      _table_main_data[i + 1][j].sorted_idx  = _table_main_data.at(0).at(j).sorted_idx;
      _table_main_data[i + 1][j].votes       = QVector<int>();
      _table_main_data[i + 1][j].percentages = QVector<double>();

      for (int k = 0; k <= _divisions.length(); k++)
      {
        _table_main_data[i + 1][j].votes.append(0);
        _table_main_data[i + 1][j].percentages.append(0);
      }

      _table_main_booth_data[i + 1].append(QVector<int>());
      for (int k = 0; k < _booths.length(); k++)
      {
        _table_main_booth_data[i + 1][j].append(0);
      }
    }
  }

  // The exhaust column for the table itself should not yet exist,
  // so make it now.
  _table_main_model->setColumnCount(n + 3);
  _table_main_model->setHorizontalHeaderItem(n + 2, new QStandardItem("Exh"));
  _table_main_model->horizontalHeaderItem(n + 2)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

  QString q = QString("SELECT booth_id, P1, COUNT(id)");

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
                .arg(q, and_str, QString::number(_clicked_n_parties.at(i)), QString::number(_clicked_n_parties.at(j)));
          and_str = " AND ";
        }
      }
      q = QString("%1) v%2").arg(q).arg(i);
    }
  }
  else
  {
    q = QString("%1, (Pfor%2 < 999) v0").arg(q).arg(_clicked_n_parties.at(0));
  }

  // Exhaust:
  q = QString("%1, (").arg(q);
  QString and_str("");
  for (int i = 0; i < n; i++)
  {
    q       = QString("%1%2 Pfor%3 = 999").arg(q, and_str, QString::number(_clicked_n_parties.at(i)));
    and_str = " AND ";
  }
  q = QString("%1) v%2").arg(q).arg(n);

  q = QString("%1 FROM %2 GROUP BY booth_id, P1").arg(q, get_abtl());

  for (int i = 0; i <= n; i++)
  {
    q = QString("%1, v%2").arg(q).arg(i);
  }

  q       = QString("%1 HAVING ").arg(q);
  and_str = "";

  for (int i = 0; i <= n; i++)
  {
    q       = QString("%1%2 v%3=1").arg(q, and_str, QString::number(i));
    and_str = " OR ";
  }

  int num_threads     = 1;
  QStringList queries = _queries_threaded(q, num_threads);

  _current_threads   = num_threads;
  _completed_threads = 0;

  _write_sql_to_file(q);

  _label_progress->setText("Calculating...");
  _lock_main_interface();

  for (int i = 0; i < num_threads; i++)
  {
    QThread* thread              = new QThread;
    Worker_sql_npp_table* worker = new Worker_sql_npp_table(i,
                                                            _database_file_path,
                                                            queries.at(i),
                                                            get_num_groups(),
                                                            num_booths,
                                                            _clicked_n_parties);

    worker->moveToThread(thread);

    connect(thread, &QThread::started,                     worker, &Worker_sql_npp_table::do_query);
    connect(worker, &Worker_sql_npp_table::finished_query, this,   &Widget::_process_thread_sql_npp_table);
    connect(worker, &Worker_sql_npp_table::finished_query, thread, &QThread::quit);
    connect(worker, &Worker_sql_npp_table::finished_query, worker, &Worker_sql_npp_table::deleteLater);
    connect(thread, &QThread::finished,                    thread, &QThread::deleteLater);

    thread->start();
  }

  _button_n_party_preferred_calculate->setEnabled(false);
}

// This method throws exceptions, so should be run inside a try-catch.
Custom_axis_definition Widget::_read_custom_axis_definition(QLineEdit* lineedit)
{
  // Possibilities:
  // <empty>
  // groups
  // candidates
  // npp(ALP, LP, GRN, ON)
  // 1..6, 999 (comma-separated list of numbers and/or ranges)
  // all(numeric expression)

  Custom_axis_definition axis;
  QString text = lineedit->text().trimmed();

  if (text.isEmpty())
  {
    return axis;
  }

  const int current_num_groups = get_num_groups();
  const QString num_groups_str = QString::number(current_num_groups);
  text.replace(Custom_identifiers::N_MAX, num_groups_str);

  const bool is_atl = get_abtl() == "atl";

  if (text == Custom_axis_names::GROUPS)
  {
    axis.type = Custom_axis_type::GROUPS;
    return axis;
  }

  if (text == Custom_axis_names::CANDIDATES)
  {
    if (is_atl)
    {
      throw std::runtime_error("Cannot set axis to <code>candidates</code> when working above the line.");
    }
    axis.type = Custom_axis_type::CANDIDATES;
    return axis;
  }

  static QRegularExpression npp_pattern("^npp\\s*\\(.*\\)$");
  if (text.contains(npp_pattern))
  {
    static QRegularExpression npp_start_pattern("^npp\\s*\\(");
    text.replace(npp_start_pattern, "");
    text.chop(1);
    // 'parties' might be groups or candidates
    QStringList parties = text.split(",", Qt::SkipEmptyParts);

    const int n = parties.count();

    if (n == 0)
    {
      throw std::runtime_error("Must define at least one argument in <code>npp()</code>.");
    }

    QHash<int, bool> npp_groups;
    QHash<int, bool> npp_candidates;
    QVector<int> npp_candidate_groups;
    for (int i = 0; i < n; ++i)
    {
      const QString entity = parties.at(i).trimmed();
      if (_group_from_short.contains(entity))
      {
        const int group = _group_from_short.value(entity);
        axis.npp_indices.append(is_atl ? group : Custom_operations::aggregated_index_to_from_negative(group));
        if (npp_groups.contains(group))
        {
          throw std::runtime_error("Repeated group in <code>npp</code> definition");
        }
        npp_groups.insert(group, true);
        continue;
      }

      if (is_atl)
      {
        const QString msg = QString("Unrecognised group %1 in <code>npp</code> definition.").arg(entity);
        throw std::runtime_error(msg.toStdString());
      }

      if (_cand_from_short.contains(entity))
      {
        const int cand = _cand_from_short.value(entity);
        if (npp_candidates.contains(cand))
        {
          throw std::runtime_error("Repeated candidate in <code>npp</code> definition");
        }

        const int group = _group_from_candidate.at(cand);
        npp_candidate_groups.append(group);

        npp_candidates.insert(cand, true);
        axis.npp_indices.append(cand);
        continue;
      }

      const QString msg = QString("Unrecognised candidate %1 in <code>npp</code> definition.").arg(entity);
      throw std::runtime_error(msg.toStdString());
    }

    for (int group : npp_candidate_groups)
    {
      if (npp_groups.contains(group))
      {
        throw std::runtime_error("<code>npp</code> definition cannot contain both a candidate and its group.");
      }
    }

    axis.type = Custom_axis_type::NPP;
    return axis;
  }

  static QRegularExpression every_pattern("^every\\s*\\(.*\\)$");
  if (text.contains(every_pattern))
  {
    if (text.contains(Custom_identifiers::ROW) || text.contains(Custom_identifiers::COL))
    {
      throw std::runtime_error("<code>every</code> expression cannot contain <code>row</code> or <code>col</code>.");
    }

    static QRegularExpression every_start_pattern("^every\\s*\\(");
    text.replace(every_start_pattern, "");
    text.chop(1);

    Custom_lexer lex(text);
    std::vector<Custom_token> tokens = lex.tokenize();
    Custom_parser parser(tokens);
    std::unique_ptr<Custom_expr> ast = parser.parse_AST();

    if (!(ast->expr_type() == Custom_expr_type::INTEGER))
    {
      const QString msg = QString("The <code>every(expr)</code> function in an axis definition returns every "
                                  "integer value taken by <code>expr</code>, so <code>expr</code> should evaluate to an integer.  "
                                  "The expression <code>%1</code> instead evaluates to boolean.<pre>%2</pre>")
                            .arg(text, ast->debug_string(0));
      throw std::runtime_error(msg.toStdString());
    }

    axis.type                     = Custom_axis_type::NUMBERS;
    axis.every_numbers_ast        = std::move(ast);
    axis.every_numbers_definition = text;
    return axis;
  }

  // Last possibility: comma-separated numbers and ranges.
  axis.type = Custom_axis_type::NUMBERS;

  QStringList parts = text.split(',', Qt::SkipEmptyParts);
  static QRegularExpression range_pattern("^(\\-?\\d+)\\.\\.(\\-?\\d+)$");
  bool success = true;
  for (QString& part : parts)
  {
    const QString p = part.trimmed();

    bool converted;
    int i = p.toInt(&converted);
    if (converted)
    {
      axis.numbers.append(i);
      continue;
    }

    QRegularExpressionMatch match = range_pattern.match(p);
    if (match.hasMatch())
    {
      const int lower = match.captured(1).toInt();
      const int upper = match.captured(2).toInt();
      //Custom_range r{lower, upper};
      for (int i = lower; i <= upper; ++i)
      {
        if (!axis.numbers.contains(i))
        {
          axis.numbers.append(i);
        }
      }
      continue;
    }

    success = false;
    break;
  }

  if (success)
  {
    if (axis.type == Custom_axis_type::NUMBERS)
    {
      std::sort(axis.numbers.begin(), axis.numbers.end());
    }
    return axis;
  }

  throw std::runtime_error(
    "Unrecognised axis definition format.  An axis definition may be:"
    "<ul>"
    "<li>&lt;empty&gt;"
    "<li><code>groups</code> (case-sensitive)"
    "<li><code>candidates</code> (if below the line)"
    "<li><code>1, 6, 10..12, n_max</code> (comma-separated list of numbers and ranges)"
    "<li><code>all(expr)</code> to use all integer values taken by <code>expr</code>"
    "</ul>");
}

void Widget::_parse_custom_query_line(
  QLineEdit* lineedit, const QString& lineedit_name, bool is_boolean, bool allow_row, bool allow_col, bool shortcut_row_already_forced,
  bool shortcut_col_already_forced, bool row_is_aggregate, bool col_is_aggregate, QString& sql, std::vector<Custom_operation>& row_ops,
  std::vector<Custom_operation>& col_ops, std::vector<Custom_operation>& cell_ops)
{
  const bool is_atl = get_abtl() == "atl";
  QString cell_text = lineedit->text().trimmed();

  QString illegal_identifier = "";
  if (!allow_row && cell_text.contains(Custom_identifiers::ROW))
  {
    illegal_identifier = Custom_identifiers::ROW;
  }
  else if (!allow_col && cell_text.contains(Custom_identifiers::COL))
  {
    illegal_identifier = Custom_identifiers::COL;
  }

  if (!illegal_identifier.isEmpty())
  {
    throw std::runtime_error(
      QString("Cannot use %1 in %2 definition")
        .arg(illegal_identifier, lineedit_name)
        .toStdString());
  }

  if (is_atl && cell_text.contains("nc_"))
  {
    throw std::runtime_error("Prefix <code>nc_</code> is only available below the line.");
  }

  const int current_num_groups = get_num_groups();
  const QString num_groups_str = QString::number(current_num_groups);
  cell_text.replace(Custom_identifiers::N_MAX, num_groups_str);
  cell_text.replace("<>", "!=");
  static QRegularExpression equals_pattern("\\=\\=+");
  cell_text.replace(equals_pattern, "=");

  static QRegularExpression id_row("\\bid_" + Custom_identifiers::ROW + "\\b");
  static QRegularExpression id_col("\\bid_" + Custom_identifiers::COL + "\\b");

  if (is_atl)
  {
    for (int i = 0; i < _num_groups; ++i)
    {
      QRegularExpression id_group("\\bid_" + _atl_groups_short.at(i) + "\\b");
      cell_text.replace(id_group, QString::number(i + 1));
    }

    if (cell_text.contains(id_row) && _custom_rows.type != Custom_axis_type::GROUPS)
    {
      throw std::runtime_error(QString("You can only use <code>id_" + Custom_identifiers::ROW + "</code> when the rows are groups.").toStdString());
    }

    if (cell_text.contains(id_col) && _custom_cols.type != Custom_axis_type::GROUPS)
    {
      throw std::runtime_error(QString("You can only use <code>id_" + Custom_identifiers::COL + "</code> when the columns are groups.").toStdString());
    }
  }
  else
  {
    // BTL
    for (int i = 0; i < _num_cands; ++i)
    {
      QRegularExpression id_cand("\\bid_" + _btl_names_short.at(i) + "\\b");
      cell_text.replace(id_cand, QString::number(i + 1));
    }

    if (cell_text.contains(id_row) && _custom_rows.type != Custom_axis_type::CANDIDATES)
    {
      throw std::runtime_error(QString("You can only use <code>id_" + Custom_identifiers::ROW + "</code> when the rows are candidates.").toStdString());
    }

    if (cell_text.contains(id_col) && _custom_cols.type != Custom_axis_type::CANDIDATES)
    {
      throw std::runtime_error(QString("You can only use <code>id_" + Custom_identifiers::COL + "</code> when the columns are candidates.").toStdString());
    }
  }

  if (!is_atl)
  {
    for (int i = 0; i < _num_groups; ++i)
    {
      QRegularExpression nc_group("\\bnc_" + _atl_groups_short.at(i) + "\\b");
      cell_text.replace(nc_group, QString::number(_candidates_per_group.at(i).size()));
    }

    if (static_cast<int>(_candidates_per_group.size()) > _num_groups + 1)
    {
      // ungrouped candidates are present
      static QRegularExpression nc_ungrouped("\\bnc_UG\\b");
      cell_text.replace(nc_ungrouped, QString::number(_candidates_per_group.at(_num_groups).size()));
    }

    static QRegularExpression nc_row("\\bnc_" + Custom_identifiers::ROW + "\\b");
    if (cell_text.contains(nc_row) && _custom_rows.type != Custom_axis_type::GROUPS)
    {
      throw std::runtime_error(QString("You can only use <code>nc_" + Custom_identifiers::ROW + "</code> when the rows are groups.").toStdString());
    }

    static QRegularExpression nc_col("\\bnc_" + Custom_identifiers::COL + "\\b");
    if (cell_text.contains(nc_col) && _custom_cols.type != Custom_axis_type::GROUPS)
    {
      throw std::runtime_error(QString("You can only use <code>nc_" + Custom_identifiers::COL + "</code> when the columns are groups.").toStdString());
    }
  }

  Custom_lexer lex(cell_text);
  std::vector<Custom_token> tokens = lex.tokenize();
  Custom_parser parser(tokens);
  std::unique_ptr<Custom_expr> ast = parser.parse_AST();

  if (is_atl && ast->has_any_or_all())
  {
    throw std::runtime_error("Can only use <code>any()</code> or <code>all()</code> when working below the line.");
  }

  if (!is_atl)
  {
    ast->check_valid_aggregations(this, false, false, row_is_aggregate, col_is_aggregate);
  }

  if (is_boolean && !(ast->expr_type() == Custom_expr_type::BOOLEAN))
  {
    const QString msg = QString("The expression in the %1 field should evaluate to "
                                "a boolean or be empty (treated as true).  "
                                "The expression <code>%2</code> instead evaluates to an integer.<pre>%3</pre>")
                          .arg(lineedit_name, cell_text, ast->debug_string(0));
    throw std::runtime_error(msg.toStdString());
  }

  // If the main cell expression is of the form
  // row = row_expr and col = col_expr
  // with neither row_expr nor col_expr containing any references to row or
  // col, then we want to process the two parts separately.

  // Traverse the tree counting instances of an identifier (row or col)
  std::function<void(const Custom_expr*, const QString&, int&)> count_identifiers =
    [&count_identifiers](const Custom_expr* expr, const QString& identifier, int& count)
  {
    // Test name against endsWith() to catch nc_{row/col}, id_{row/col}.
    if (expr->get_op_type() == Custom_op_type::IDENTIFIER && expr->get_name().endsWith(identifier))
    {
      count++;
    }

    for (int i = 0, n = expr->get_num_arguments(); i < n; ++i)
    {
      count_identifiers(expr->get_argument(i), identifier, count);
    }
  };

  int count_row = 0;
  int count_col = 0;

  count_identifiers(ast.get(), Custom_identifiers::ROW, count_row);
  count_identifiers(ast.get(), Custom_identifiers::COL, count_col);

  bool shortcut_row = false;
  bool shortcut_col = false;

  int index_stack_boolean = 0;
  // Integer stack contains the num_groups Pfor's, then the Exhaust
  // preference number, then N_prefs, then P1, P2, ..., P(num_groups),
  // then axis numbers.
  const int n_axis_numbers = _custom_axis_numbers.size();
  int index_stack_integer  = 2 * current_num_groups + 2 + n_axis_numbers;

  auto create_shortcut_operations = [&](
    const Custom_expr* operand, const QString& identifier1, const QString& identifier2, std::vector<Custom_operation>& ops, bool& shortcut)
  {
    // We're hoping for
    //   identifier1 = expr
    // or
    //   expr = identifier1
    // in which expr does not contain identifier2.

    if (operand->get_op_type() == Custom_op_type::EQ)
    {
      const Custom_expr* comparand1    = operand->get_argument(0);
      const Custom_expr* comparand2    = operand->get_argument(1);
      const Custom_expr* shortcut_expr = nullptr;
      // Test for, e.g. 'row = expr' or 'expr = row':
      if (comparand1->get_op_type() == Custom_op_type::IDENTIFIER && comparand1->get_name() == identifier1)
      {
        shortcut_expr = comparand2;
      }
      else if (comparand2->get_op_type() == Custom_op_type::IDENTIFIER && comparand2->get_name() == identifier1)
      {
        shortcut_expr = comparand1;
      }

      if (shortcut_expr != nullptr)
      {
        // Make sure that if we've found, e.g. row = expr,
        // then expr does not contain col
        int count_bad_identifiers = 0;
        count_identifiers(shortcut_expr, identifier2, count_bad_identifiers);
        if (count_bad_identifiers == 0)
        {
          shortcut            = true;
          index_stack_boolean = 0;
          index_stack_integer = 2 * current_num_groups + 2 + n_axis_numbers;
          int i_loop          = -1;
          Custom_operations::create_operations(this, nullptr, shortcut_expr, ops, index_stack_boolean, index_stack_integer, row_is_aggregate,
            col_is_aggregate, i_loop, 0);
        }
      }
    }
  };

  const Custom_expr* cell_expr = ast.get();

  if (ast->get_op_type() == Custom_op_type::AND && !shortcut_row_already_forced && !shortcut_col_already_forced)
  {
    if (count_row == 1)
    {
      create_shortcut_operations(ast->get_argument(0), Custom_identifiers::ROW, Custom_identifiers::COL, row_ops, shortcut_row);
      if (shortcut_row)
      {
        cell_expr = ast->get_argument(1);
      }
      else
      {
        create_shortcut_operations(ast->get_argument(1), Custom_identifiers::ROW, Custom_identifiers::COL, row_ops, shortcut_row);
        if (shortcut_row)
        {
          cell_expr = ast->get_argument(0);
        }
      }
    }

    if (count_col == 1)
    {
      create_shortcut_operations(ast->get_argument(0), Custom_identifiers::COL, Custom_identifiers::ROW, col_ops, shortcut_col);
      if (shortcut_col && !shortcut_row)
      {
        cell_expr = ast->get_argument(1);
      }
      if (!shortcut_col)
      {
        create_shortcut_operations(ast->get_argument(1), Custom_identifiers::COL, Custom_identifiers::ROW, col_ops, shortcut_col);
        if (shortcut_col && !shortcut_row)
        {
          cell_expr = ast->get_argument(0);
        }
      }
    }
  }

  if (shortcut_col_already_forced)
  {
    // Check a query of the form 'row = expr'.
    if (count_row == 1)
    {
      create_shortcut_operations(ast.get(), Custom_identifiers::ROW, Custom_identifiers::COL, row_ops, shortcut_row);
      if (shortcut_row)
      {
        cell_expr = nullptr;
      }
    }
  }

  if (shortcut_row_already_forced)
  {
    // Check a query of the form 'col = expr'.
    if (count_col == 1)
    {
      create_shortcut_operations(ast.get(), Custom_identifiers::COL, Custom_identifiers::ROW, col_ops, shortcut_col);
      if (shortcut_col)
      {
        cell_expr = nullptr;
      }
    }
  }

  if (shortcut_row && shortcut_col)
  {
    cell_expr = nullptr;
  }

  index_stack_boolean = 0;
  index_stack_integer = 2 * current_num_groups + 2 + n_axis_numbers;
  int i_loop          = -1;
  Custom_operations::create_operations(this, nullptr, cell_expr, cell_ops, index_stack_boolean, index_stack_integer, row_is_aggregate, col_is_aggregate, i_loop, 0);

  if (ast->can_convert_to_sql(this))
  {
    if (ast->get_op_type() == Custom_op_type::TRUE_LITERAL)
    {
      sql = NO_FILTER;
    }
    else
    {
      sql = ast->to_sql(this);
    }
  }
}

void Widget::_slot_calculate_custom()
{
  _timer.restart();

  // Write query to file so that next time the program opens, the widgets are
  // pre-filled
  const QString last_custom_query_file_name = QString("%1/last_custom_query.txt")
                                                .arg(QCoreApplication::applicationDirPath());

  QFile last_custom_query_file(last_custom_query_file_name);

  auto write_query_to_file = [&](QTextStream& stream, QLineEdit* lineedit, QString prefix)
  {
    const QString text = lineedit->text().trimmed();
    if (!text.isEmpty())
    {
      stream << prefix + text + "\n";
    }
  };

  if (last_custom_query_file.open(QIODevice::WriteOnly))
  {
    QTextStream out(&last_custom_query_file);
    write_query_to_file(out, _lineedit_custom_filter, "Filter: ");
    write_query_to_file(out, _lineedit_custom_rows, "Rows: ");
    write_query_to_file(out, _lineedit_custom_cols, "Cols: ");
    write_query_to_file(out, _lineedit_custom_cell, "Cell: ");
    last_custom_query_file.close();
  }

  _custom_rows = Custom_axis_definition();
  _custom_cols = Custom_axis_definition();
  _custom_filter_sql.clear();
  _custom_filter_operations.clear();
  _custom_row_operations.clear();
  _custom_col_operations.clear();
  _custom_cell_operations.clear();
  _custom_axis_numbers.clear();
  _custom_row_stack_indices.clear();
  _custom_col_stack_indices.clear();
  _num_custom_every_expr_threads           = 0;
  _num_custom_every_expr_threads_completed = 0;
  _clear_divisions_table();
  _reset_table();

  QString current_step = "reading rows definition";
  try
  {
    _custom_rows = _read_custom_axis_definition(_lineedit_custom_rows);
    current_step = "reading columns definition";
    _custom_cols = _read_custom_axis_definition(_lineedit_custom_cols);

    bool have_every_expr = false;

    if (_custom_rows.every_numbers_ast != nullptr)
    {
      have_every_expr = true;
    }
    if (_custom_cols.every_numbers_ast != nullptr)
    {
      have_every_expr = true;
    }

    if (!have_every_expr)
    {
      _calculate_custom_query();
      return;
    }

    // We need to evaluate at least one all(expr) to see which numbers are in
    // a numbers axis.

    // Variables are labelled 'temp' here to distinguish them from their
    // counterparts in the main calculation.  The filter will get redefined
    // with new integer stack indices once the numbers axis is fully defined;
    // this part of the code is only to evaluate what those numbers are.
    QString temp_custom_filter_sql;
    std::vector<Custom_operation> temp_custom_filter_operations;
    std::vector<Custom_operation> dummy_row_ops;
    std::vector<Custom_operation> dummy_col_ops;
    const QString filter_name("Filter");
    _parse_custom_query_line(_lineedit_custom_filter, filter_name, true, false, false, false, false, false, false,
      temp_custom_filter_sql, dummy_row_ops, dummy_col_ops, temp_custom_filter_operations);

    bool use_pure_sql_filter = false;

    QString where_clause = "";
    if (!temp_custom_filter_sql.isEmpty())
    {
      use_pure_sql_filter = true;
      temp_custom_filter_operations.clear();
      if (temp_custom_filter_sql != NO_FILTER)
      {
        where_clause = " WHERE (" + temp_custom_filter_sql + ")";
      }
    }

    const int current_num_groups = get_num_groups();
    int thread_num               = 0;

    auto launch_worker_every_expr = [this, current_num_groups, use_pure_sql_filter, &where_clause, &thread_num]
      (int i_axis, Custom_axis_definition& axis, std::vector<Custom_operation>& filter_operations)
    {
      if (axis.every_numbers_ast == nullptr)
      {
        return;
      }

      std::vector<Custom_operation> axis_operations;
      int index_stack_integer       = 2 * current_num_groups + 2;
      int dummy_index_stack_boolean = 0;
      int i_loop                    = -1;
      Custom_operations::create_operations(this, nullptr, axis.every_numbers_ast.get(), axis_operations, dummy_index_stack_boolean, index_stack_integer,
        false, false, i_loop, 0);

      const bool use_pure_sql = use_pure_sql_filter && axis.every_numbers_ast->can_convert_to_sql(this);

      QString q;
      const QString abtl = get_abtl();
      const bool is_atl  = abtl == "atl";

      if (use_pure_sql)
      {
        const QString expr_sql = axis.every_numbers_ast->to_sql(this);

        q = QString("SELECT DISTINCT (%1) AS v FROM %2 %3 ORDER BY v")
              .arg(expr_sql, abtl, where_clause);
      }
      else
      {
        q = "SELECT ";
        for (int i = 0; i < current_num_groups; ++i)
        {
          q += QString("Pfor%1, ").arg(i);
        }
        // Duplicate num_prefs in the query; one will turn into a "preference
        // number" for Exhaust, the other stays as N_prefs
        q += "num_prefs, num_prefs";
        for (int i = 0; i < current_num_groups; ++i)
        {
          q += QString(", P%1").arg(i + 1);
        }

        q += QString(" FROM %1 %2").arg(abtl, where_clause);
      }

      int num_threads     = 1;
      QStringList queries = _queries_threaded(q, num_threads, use_pure_sql);
      _num_custom_every_expr_threads += num_threads;

      auto slot = use_pure_sql ? &Worker_sql_custom_every_expr::do_query_pure_sql : &Worker_sql_custom_every_expr::do_query_operations;

      std::vector<std::vector<int>> empty_indices;
      std::vector<std::vector<int>>& agg_indices = is_atl ? empty_indices : _candidates_per_group;

      int max_loop_index = -1;
      Custom_operations::update_max_loop_index(filter_operations, max_loop_index);
      Custom_operations::update_max_loop_index(axis_operations, max_loop_index);

      for (int i = 0; i < num_threads; ++i)
      {
        thread_num++;
        QThread* thread                      = new QThread;
        Worker_sql_custom_every_expr* worker = new Worker_sql_custom_every_expr(
          _database_file_path, i_axis, thread_num, queries.at(i), current_num_groups, max_loop_index, agg_indices, filter_operations, axis_operations);

        worker->moveToThread(thread);

        connect(thread, &QThread::started,                             worker, slot);
        connect(worker, &Worker_sql_custom_every_expr::finished_query, this,   &Widget::_process_thread_sql_custom_every_expr);
        connect(worker, &Worker_sql_custom_every_expr::finished_query, thread, &QThread::quit);
        connect(worker, &Worker_sql_custom_every_expr::finished_query, worker, &Worker_sql_custom_every_expr::deleteLater);
        connect(thread, &QThread::finished,                            thread, &QThread::deleteLater);

        thread->start();
      }
    };

    _lock_main_interface();
    launch_worker_every_expr(Custom_row_col::ROW, _custom_rows, temp_custom_filter_operations);
    launch_worker_every_expr(Custom_row_col::COL, _custom_cols, temp_custom_filter_operations);
    _label_progress->setText("Calculating <code>every</code>...");
  }
  catch (const std::exception& ex)
  {
    const QString msg = "Error " + current_step + ".<br><br>" + QString::fromStdString(ex.what());
    _show_error_message(msg);
    _unlock_main_interface();
  }
}

void Widget::_calculate_custom_query()
{
  QString current_step = "sorting numbers";
  try
  {
    const QString abtl = get_abtl();
    const bool is_atl  = (abtl == "atl");

    // By the time we reach here, the entries in any numbers axes have been
    // computed.
    QVector<int> numbers(_custom_rows.numbers);
    for (int& i : _custom_cols.numbers)
    {
      if (!numbers.contains(i))
      {
        numbers.append(i);
      }
    }

    std::sort(numbers.begin(), numbers.end());
    _custom_axis_numbers.assign(numbers.begin(), numbers.end());
    const int n_axis_numbers     = _custom_axis_numbers.size();
    const int current_num_groups = get_num_groups();

    int i_row               = 0;
    int i_col               = 0;
    const int i_offset      = 2 * current_num_groups + 2;
    const int n_row_numbers = _custom_rows.numbers.size();
    const int n_col_numbers = _custom_cols.numbers.size();
    for (int i = 0; i < n_axis_numbers; ++i)
    {
      const int axis_n = _custom_axis_numbers.at(i);
      if (i_row < n_row_numbers && _custom_rows.numbers.at(i_row) == axis_n)
      {
        _custom_row_stack_indices.push_back(i_offset + i);
        i_row++;
      }

      if (i_col < n_col_numbers && _custom_cols.numbers.at(i_col) == axis_n)
      {
        _custom_col_stack_indices.push_back(i_offset + i);
        i_col++;
      }
    }

    auto set_group_stack_indices = [this, current_num_groups, is_atl](Custom_axis_definition& axis, std::vector<int>& stack_indices)
    {
      if ((is_atl && axis.type == Custom_axis_type::GROUPS) || axis.type == Custom_axis_type::CANDIDATES)
      {
        for (int i = 0; i < current_num_groups + 1; ++i)
        {
          stack_indices.push_back(i);
        }
      }
      else if (!is_atl && axis.type == Custom_axis_type::GROUPS)
      {
        // 'Ungrouped' (if relevant) and 'Exh' are both groups for BTL groups:
        const int n_btl_groups = _candidates_per_group.size();
        for (int i = 0; i < n_btl_groups; ++i)
        {
          stack_indices.push_back(Custom_operations::aggregated_index_to_from_negative(i));
        }
      }
      else if (axis.type == Custom_axis_type::NPP)
      {
        const int n = axis.npp_indices.length();
        for (int i = 0; i < n; ++i)
        {
          stack_indices.push_back(axis.npp_indices.at(i));
        }
        stack_indices.push_back(current_num_groups);
      }
    };

    auto set_headers = [this, is_atl](Custom_axis_definition& axis, QStringList& headers)
    {
      headers.clear();
      if (axis.type == Custom_axis_type::GROUPS)
      {
        for (int i = 0; i < _num_groups; ++i)
        {
          headers.append(_atl_groups_short.at(i));
        }

        if (!is_atl && static_cast<int>(_candidates_per_group.size()) > _num_groups + 1)
        {
          // _candidates_per_group contains, potentially, elements for 'ungrouped' and 'exhaust'.
          headers.append("UG");
        }

        headers.append("Exh");
        return;
      }

      if (axis.type == Custom_axis_type::CANDIDATES)
      {
        for (int i = 0; i < _num_cands; ++i)
        {
          headers.append(_btl_names_short.at(i));
        }
        headers.append("Exh");
        return;
      }

      if (axis.type == Custom_axis_type::NPP)
      {
        for (int& i : axis.npp_indices)
        {
          if (!is_atl && i >= 0)
          {
            headers.append(_btl_names_short.at(i));
            continue;
          }

          const int group = i >= 0 ? i : Custom_operations::aggregated_index_to_from_negative(i);
          headers.append(_atl_groups_short.at(group));
        }

        headers.append("Exh");
        return;
      }

      if (axis.type == Custom_axis_type::NUMBERS)
      {
        for (int& i : axis.numbers)
        {
          headers.append(QString::number(i));
        }
        return;
      }

      if (axis.type == Custom_axis_type::NONE)
      {
        headers.append("Vote");
        return;
      }

      throw std::runtime_error("Programmer error: Axis type not handled in set_headers.  Sorry :(");
    };

    set_group_stack_indices(_custom_rows, _custom_row_stack_indices);
    set_group_stack_indices(_custom_cols, _custom_col_stack_indices);
    set_headers(_custom_rows, _custom_table_row_headers);
    set_headers(_custom_cols, _custom_table_col_headers);

    current_step        = "reading filter definition";
    QString filter_text = _lineedit_custom_filter->text();
    if (filter_text.contains(Custom_identifiers::ROW) || filter_text.contains(Custom_identifiers::COL))
    {
      throw std::runtime_error(
        QString("Cannot use %1 or %2 in filter definition")
          .arg(Custom_identifiers::ROW, Custom_identifiers::COL)
          .toStdString());
    }

    QString dummy_sql_cell;
    std::vector<Custom_operation> dummy_row_ops;
    std::vector<Custom_operation> dummy_col_ops;

    _parse_custom_query_line(_lineedit_custom_filter, "Filter", true, false, false, false, false, false, false, _custom_filter_sql,
      dummy_row_ops, dummy_col_ops, _custom_filter_operations);

    current_step                 = "reading cell definition";
    bool npp_forces_shortcut_row = false;
    bool npp_forces_shortcut_col = false;
    const bool contains_row      = _lineedit_custom_cell->text().contains(Custom_identifiers::ROW);
    const bool contains_col      = _lineedit_custom_cell->text().contains(Custom_identifiers::COL);
    if (_custom_rows.type == Custom_axis_type::NPP)
    {
      if (contains_row)
      {
        throw std::runtime_error("Cannot refer to <code>row</code> when the rows are an <code>npp</code>.");
      }
      npp_forces_shortcut_row = true;
      int index_stack_integer = 2 * current_num_groups + 2 + n_axis_numbers;
      Custom_operations::create_npp_operation(this, _custom_row_operations, index_stack_integer, _custom_rows.npp_indices);
    }

    if (_custom_cols.type == Custom_axis_type::NPP)
    {
      if (contains_col)
      {
        throw std::runtime_error("Cannot refer to <code>col</code> when the columns are an <code>npp</code>.");
      }
      npp_forces_shortcut_col = true;
      int index_stack_integer = 2 * current_num_groups + 2 + n_axis_numbers;
      Custom_operations::create_npp_operation(this, _custom_col_operations, index_stack_integer, _custom_cols.npp_indices);
    }

    if (_custom_rows.type == Custom_axis_type::NONE && contains_row)
    {
      throw std::runtime_error("Cannot refer to <code>row</code> when there is no Rows definition.");
    }

    if (_custom_cols.type == Custom_axis_type::NONE && contains_col)
    {
      throw std::runtime_error("Cannot refer to <code>col</code> when there is no Columns definition.");
    }

    // If I allow aggregation of numbers, would need to modify these lines:
    const bool row_is_aggregate = !is_atl && _custom_rows.type == Custom_axis_type::GROUPS;
    const bool col_is_aggregate = !is_atl && _custom_cols.type == Custom_axis_type::GROUPS;

    _parse_custom_query_line(_lineedit_custom_cell, "Cell", true, true, true, npp_forces_shortcut_row, npp_forces_shortcut_col,
      row_is_aggregate, col_is_aggregate, dummy_sql_cell, _custom_row_operations, _custom_col_operations, _custom_cell_operations);

    QString q = "SELECT booth_id, ";
    for (int i = 0; i < current_num_groups; ++i)
    {
      q += QString("Pfor%1, ").arg(i);
    }
    // Duplicate num_prefs in the query; one will turn into a "preference
    // number" for Exhaust, the other stays as n_prefs
    q += "num_prefs, num_prefs";
    for (int i = 0; i < current_num_groups; ++i)
    {
      q += QString(", P%1").arg(i + 1);
    }

    q += " FROM " + abtl;

    QStringList where_clauses;

    if (!_custom_filter_sql.isEmpty())
    {
      _custom_filter_operations.clear();
      if (_custom_filter_sql != NO_FILTER)
      {
        where_clauses.append("(" + _custom_filter_sql + ")");
      }
    }

    const bool popup = _combo_custom_table_target->currentData().toString() == TABLE_POPUP;

    const int this_div             = _get_current_division();
    const bool individual_division = popup && (this_div != _divisions.length());

    if (individual_division)
    {
      where_clauses.append(QString("(seat_id = %1)").arg(this_div));
    }

    if (!where_clauses.isEmpty())
    {
      q += " WHERE " + where_clauses.join(" AND ");
    }

    int num_threads = 1;

    _lock_main_interface();
    _label_progress->setText("Calculating...");

    QStringList queries   = _queries_threaded(q, num_threads, individual_division);
    _current_threads      = num_threads;
    _completed_threads    = 0;
    const int n_main_rows = _custom_row_stack_indices.size();
    const int n_main_cols = _custom_col_stack_indices.size();
    const int n_rows      = qMax(1, n_main_rows);
    const int n_cols      = qMax(1, n_main_cols);

    const int num_booths = _booths.length();

    if (popup)
    {
      _init_cross_table_data(n_rows, n_cols);
      _custom_cross_table_total_base = 0;
      _custom_cross_table_row_bases.clear();
      for (int i = 0; i < n_rows; ++i)
      {
        _custom_cross_table_row_bases.append(0);
      }
    }
    else
    {
      _init_main_table_custom(n_main_rows, n_rows, n_main_cols, n_cols);
    }

    std::vector<std::vector<int>> empty_indices;
    std::vector<std::vector<int>>& agg_indices = is_atl ? empty_indices : _candidates_per_group;

    int max_loop_index = -1;
    Custom_operations::update_max_loop_index(_custom_filter_operations, max_loop_index);
    Custom_operations::update_max_loop_index(_custom_row_operations, max_loop_index);
    Custom_operations::update_max_loop_index(_custom_col_operations, max_loop_index);
    Custom_operations::update_max_loop_index(_custom_cell_operations, max_loop_index);

    for (int i = 0; i < num_threads; i++)
    {
      QThread* thread                 = new QThread;
      Worker_sql_custom_table* worker = new Worker_sql_custom_table(
        i, _database_file_path, queries.at(i), current_num_groups, num_booths, _custom_axis_numbers, _custom_row_stack_indices,
        _custom_col_stack_indices, max_loop_index, agg_indices, _custom_filter_operations, _custom_row_operations, _custom_col_operations, _custom_cell_operations);
      worker->moveToThread(thread);

      if (popup)
      {
        connect(thread, &QThread::started,                        worker, &Worker_sql_custom_table::do_query);
        connect(worker, &Worker_sql_custom_table::finished_query, this,   &Widget::_process_thread_sql_custom_popup_table);
        connect(worker, &Worker_sql_custom_table::finished_query, thread, &QThread::quit);
        connect(worker, &Worker_sql_custom_table::finished_query, worker, &Worker_sql_custom_table::deleteLater);
      }
      else
      {
        connect(thread, &QThread::started,                                 worker, &Worker_sql_custom_table::do_query_by_booth);
        connect(worker, &Worker_sql_custom_table::finished_query_by_booth, this,   &Widget::_process_thread_sql_custom_main_table);
        connect(worker, &Worker_sql_custom_table::finished_query_by_booth, thread, &QThread::quit);
        connect(worker, &Worker_sql_custom_table::finished_query_by_booth, worker, &Worker_sql_custom_table::deleteLater);
      }

      connect(thread, &QThread::finished, thread, &QThread::deleteLater);

      thread->start();
    }

    _write_sql_to_file(q);
    _write_custom_operations_to_file();
  }
  catch (const std::exception& ex)
  {
    const QString msg = "Error: " + QString::fromStdString(ex.what());
    _show_error_message(msg);
  }
}

void Widget::_process_thread_sql_npp_table(const QVector<QVector<QVector<int>>>& table)
{
  const int n                  = table.length() - 1;
  const int current_num_groups = get_num_groups();
  const int num_divisions      = _divisions.length();

  for (int i = 0; i <= n; i++)
  {
    for (int j = 0; j < current_num_groups; j++)
    {
      for (int k = 0; k < table.at(i).at(j).length(); k++)
      {
        _table_main_booth_data[i + 1][j][k] += table.at(i).at(j).at(k);
      }
    }
  }

  _completed_threads++;

  if (_completed_threads == _current_threads)
  {
    const int n_booths = table.at(0).at(0).length();

    // Get the totals:
    for (int k = 0; k < n_booths; k++)
    {
      for (int i = 0; i <= n; i++)
      {
        for (int j = 0; j < current_num_groups; j++)
        {
          _table_main_booth_data[i + 1][current_num_groups][k] += _table_main_booth_data[i + 1][j][k];
        }

        if (i < n)
        {
          _table_main_booth_data[i + 1][current_num_groups][k] += _table_main_booth_data[0][_clicked_n_parties.at(i)][k];
        }
      }
    }

    // Sum each division's votes to get the division totals:
    for (int i = 0; i <= n; i++)
    {
      for (int j = 0; j <= current_num_groups; j++)
      {
        int state_votes    = 0;
        const int sorted_i = _table_main_data.at(i + 1).at(j).sorted_idx;

        for (int k = 0; k < _table_main_booth_data.at(i + 1).at(j).length(); k++)
        {
          int this_votes = _table_main_booth_data.at(i + 1).at(j).at(k);
          _table_main_data[i + 1][sorted_i].votes[_booths.at(k).division_id] += this_votes;
          state_votes += this_votes;
        }

        _table_main_data[i + 1][sorted_i].votes[_divisions.length()] = state_votes;
      }
    }

    // Calculate the percentages (calculated here so that they can be used
    // for sorting)
    for (int i = 0; i <= n; i++)
    {
      for (int j = 0; j <= current_num_groups; j++)
      {
        for (int k = 0; k <= num_divisions; k++)
        {
          const int total                           = _table_main_data.at(0).at(j).votes.at(k);
          const int votes                           = _table_main_data.at(i + 1).at(j).votes.at(k);
          _table_main_data[1 + i][j].percentages[k] = 100. * votes / total;
        }
      }
    }

    for (int i = 0; i <= n; i++)
    {
      _set_main_table_cells(i + 1);
    }

    _show_calculation_time();
    _unlock_main_interface();
  }
  else
  {
    _label_progress->setText(QString("%1/%2 complete").arg(_completed_threads).arg(_current_threads));
  }
}

void Widget::_make_cross_table()
{
  _timer.restart();
  const QString table_type     = _get_table_type();
  const QString value_type     = _get_value_type();
  const int num_clicked        = _clicked_cells.length();
  const int current_num_groups = get_num_groups();

  QString where_clause("");
  QString and_str("");
  const int this_div     = _get_current_division();
  const bool whole_state = (this_div == _divisions.length());
  bool have_where        = false;
  QString q("");
  QStringList queries;
  int num_threads;
  QVector<int> args; // Passed to the worker function

  _cross_table_title = QString("%1: ").arg(_state_full);
  QString title_given("");
  QString space("");

  if (!whole_state)
  {
    where_clause       = QString(" WHERE seat_id = %1").arg(this_div);
    and_str            = " AND";
    have_where         = true;
    _cross_table_title = QString("%1: ").arg(_divisions.at(this_div));
  }

  if (table_type == Table_types::STEP_FORWARD)
  {
    int row_pref = 1;

    if (num_clicked > 1)
    {
      if (!have_where)
      {
        where_clause = " WHERE";
      }

      int max_conditional = num_clicked - 1;
      row_pref            = num_clicked;

      if (num_clicked == current_num_groups)
      {
        max_conditional--;
        row_pref--;
      }

      for (int i = 0; i < max_conditional; i++)
      {
        where_clause = QString("%1%2 P%3 = %4")
                         .arg(where_clause, and_str, QString::number(i + 1), QString::number(_clicked_cells.at(i)));

        and_str = " AND";

        QString title_bit = QString("%1 %2").arg(QString::number(i + 1), _get_short_group(_clicked_cells.at(i)));

        if (value_type == VALUE_PERCENTAGES)
        {
          title_given = QString("%1%2%3").arg(title_given, space, title_bit);
          space       = " ";
        }
        else
        {
          _cross_table_title = QString("%1 %2").arg(_cross_table_title, title_bit);
        }
      }
    }

    const int col_pref = row_pref + 1;

    q = QString("SELECT P%1, P%2, COUNT(id) FROM %3%4 GROUP BY P%1, P%2")
          .arg(QString::number(row_pref), QString::number(col_pref), get_abtl(), where_clause);

    if (value_type == VALUE_PERCENTAGES)
    {
      _cross_table_title = QString("%1%2 Column given %3 %4 Row")
                             .arg(_cross_table_title, QString::number(col_pref), title_given, QString::number(row_pref));
    }
    else
    {
      _cross_table_title = QString("%1 %2 Row %3 Column")
                             .arg(_cross_table_title, QString::number(row_pref), QString::number(col_pref));
    }
  }
  else if (table_type == Table_types::FIRST_N_PREFS)
  {
    const int n = _get_n_first_prefs();
    args.append(n);

    _cross_table_title = QString("%1In first %2 prefs: ").arg(_cross_table_title, QString::number(n));

    if (num_clicked > 1)
    {
      if (!have_where)
      {
        where_clause = " WHERE";
      }

      int max_conditional = num_clicked - 1;
      if (max_conditional == _get_n_first_prefs() - 1)
      {
        max_conditional--;
      }

      for (int i = 0; i < max_conditional; i++)
      {
        const int gp = _clicked_cells.at(i);

        QString cond;
        if (gp < current_num_groups)
        {
          cond = QString("Pfor%1 <= %2").arg(gp, n);
        }
        else
        {
          cond = QString("num_prefs < %1").arg(n);
        }

        where_clause = QString("%1%2 %3").arg(where_clause, and_str, cond);

        and_str = " AND";
        args.append(_clicked_cells.at(i));

        if (value_type == VALUE_PERCENTAGES)
        {
          title_given = QString("%1%2%3")
                          .arg(title_given, space, _get_short_group(_clicked_cells.at(i)));
        }
        else
        {
          _cross_table_title = QString("%1%2%3")
                                 .arg(_cross_table_title, space, _get_short_group(_clicked_cells.at(i)));
        }
        space = ", ";
      }
    }

    q = "SELECT";
    QString comma("");

    for (int i = 1; i <= n; i++)
    {
      q     = QString("%1%2 P%3").arg(q, comma, QString::number(i));
      comma = ",";
    }
    q = QString("%1, num_prefs FROM %2 %3").arg(q, get_abtl(), where_clause);

    if (value_type == VALUE_PERCENTAGES)
    {
      _cross_table_title = QString("%1Column given %2%3Row").arg(_cross_table_title, title_given, space);
    }
    else
    {
      _cross_table_title = QString("%1%2Row, Column").arg(_cross_table_title, space);
    }
  }
  else if (table_type == Table_types::LATER_PREFS)
  {
    const int fixed = _get_later_prefs_n_fixed();
    const int up_to = _get_later_prefs_n_up_to();

    args.append(fixed);
    args.append(up_to);

    if (num_clicked > 1 && up_to > 2)
    {
      if (!have_where)
      {
        where_clause = " WHERE";
      }

      int max_conditional = num_clicked - 1;
      if (max_conditional == up_to - 1)
      {
        max_conditional--;
      }

      for (int i = 0; i < qMin(fixed, max_conditional); i++)
      {
        where_clause = QString("%1%2 P%3 = %4")
                         .arg(where_clause, and_str, QString::number(i + 1), QString::number(_clicked_cells.at(i)));
        and_str = " AND";
        args.append(_clicked_cells.at(i));

        QString title_bit = QString("%1 %2").arg(QString::number(i + 1), _get_short_group(_clicked_cells.at(i)));

        if (value_type == VALUE_PERCENTAGES)
        {
          title_given = QString("%1%2%3").arg(title_given, space, title_bit);
          space       = " ";
        }
        else
        {
          _cross_table_title = QString("%1 %2").arg(_cross_table_title, title_bit);
        }
      }

      for (int i = fixed; i < max_conditional; i++)
      {
        const int gp = _clicked_cells.at(i);

        QString cond;
        if (gp < current_num_groups)
        {
          cond = QString("Pfor%1 <= %2").arg(gp, up_to);
        }
        else
        {
          cond = QString("num_prefs < %1").arg(up_to);
        }

        where_clause = QString("%1%2 %3").arg(where_clause, and_str, cond);

        args.append(_clicked_cells.at(i));

        space = (i == fixed) ? "; " : ", ";

        if (value_type == VALUE_PERCENTAGES)
        {
          title_given = QString("%1%2%3")
                          .arg(title_given, space, _get_short_group(_clicked_cells.at(i)));
        }
        else
        {
          _cross_table_title = QString("%1%2%3")
                                 .arg(_cross_table_title, space, _get_short_group(_clicked_cells.at(i)));
        }
      }
    }

    q = "SELECT";
    QString comma("");

    for (int i = 1; i <= up_to; i++)
    {
      q     = QString("%1%2 P%3").arg(q, comma, QString::number(i));
      comma = ",";
    }
    q = QString("%1, num_prefs FROM %2 %3").arg(q, get_abtl(), where_clause);

    int eff_num_clicked = qMax(1, num_clicked);
    eff_num_clicked     = qMin(up_to - 1, eff_num_clicked);

    if (eff_num_clicked > fixed)
    {
      // Both row and col are "by n".
      if (value_type == VALUE_PERCENTAGES)
      {
        QString sep        = eff_num_clicked == fixed + 1 ? ";" : ",";
        _cross_table_title = QString("%1 Column in first %2 prefs, given %3%4 Row in first %2 prefs")
                               .arg(_cross_table_title, QString::number(up_to), title_given, sep);
      }
      else
      {
        if (eff_num_clicked == fixed + 1)
        {
          _cross_table_title = QString("%1; Row, Column in first %2 prefs").arg(_cross_table_title, QString::number(up_to));
        }
        else
        {
          _cross_table_title = QString("%1, Row, Column in first %2 prefs").arg(_cross_table_title, QString::number(up_to));
        }
      }
    }
    else if (eff_num_clicked == fixed)
    {
      // Row is fixed; Col is "by n".
      if (value_type == VALUE_PERCENTAGES)
      {
        _cross_table_title = QString("%1 Column in first %2 prefs, given %3 %4 Row")
                               .arg(_cross_table_title, QString::number(up_to), title_given, QString::number(fixed));
      }
      else
      {
        _cross_table_title = QString("%1 %2 Row; Column in first %3 prefs")
                               .arg(_cross_table_title, QString::number(fixed), QString::number(up_to));
      }
    }
    else
    {
      // Row and col are fixed.
      const int p1 = qMax(1, eff_num_clicked);

      if (value_type == VALUE_PERCENTAGES)
      {
        _cross_table_title = QString("%1 %2 Column, given %3 %4 Row")
                               .arg(_cross_table_title, QString::number(p1 + 1), title_given, QString::number(p1));
      }
      else
      {
        _cross_table_title = QString("%1 %2 Row %3 Column")
                               .arg(_cross_table_title, QString::number(p1), QString::number(p1 + 1));
      }
    }
  }
  else if (table_type == Table_types::PREF_SOURCES)
  {
    const int pref_min = _get_pref_sources_min();
    const int pref_max = _get_pref_sources_max();

    args.append(pref_min);
    args.append(pref_max);

    q = "SELECT P1";

    for (int i = pref_min; i <= pref_max; i++)
    {
      q = QString("%1, P%2").arg(q).arg(i);
    }

    q = QString("%1, num_prefs FROM %2%3").arg(q, get_abtl(), where_clause);

    QString pref_part;
    if (pref_min == pref_max)
    {
      pref_part = QString("%1").arg(pref_min);
    }
    else
    {
      pref_part = QString("%1-%2").arg(pref_min, pref_max);
    }

    if (value_type == VALUE_PERCENTAGES)
    {
      _cross_table_title = QString("%1 1 Column given %2 Row")
                             .arg(_cross_table_title, pref_part);
    }
    else
    {
      _cross_table_title = QString("%1 1 Column, %2 Row")
                             .arg(_cross_table_title, pref_part);
    }
  }

  _lock_main_interface();
  _label_progress->setText("Calculating...");

  queries            = _queries_threaded(q, num_threads, !whole_state);
  _current_threads   = num_threads;
  _completed_threads = 0;

  const int num_cross_table_rows = get_num_groups() + 1;

  _init_cross_table_data(num_cross_table_rows);

  for (int i = 0; i < num_threads; i++)
  {
    QThread* thread                = new QThread;
    Worker_sql_cross_table* worker = new Worker_sql_cross_table(table_type,
                                                                i,
                                                                _database_file_path,
                                                                queries.at(i),
                                                                num_cross_table_rows,
                                                                args);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &Worker_sql_cross_table::do_query);
    connect(worker, &Worker_sql_cross_table::finished_query, this, &Widget::_process_thread_sql_cross_table);
    connect(worker, &Worker_sql_cross_table::finished_query, thread, &QThread::quit);
    connect(worker, &Worker_sql_cross_table::finished_query, worker, &Worker_sql_cross_table::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    thread->start();
  }

  _write_sql_to_file(q);
}

void Widget::_set_title_from_divisions_table()
{
  // For use by cross tables by division/booth.
  // Also called (woeful design) when copying or exporting
  // a divisions table.

  const QString value_type = _get_value_type();
  const QString table_type = _get_table_type();
  QString standard_title   = _label_division_table_title->text();

  standard_title.replace("&nbsp;", " ");

  const int n = _clicked_cells.length();

  _divisions_cross_table_title = "";

  if (value_type == VALUE_PERCENTAGES)
  {
    if (table_type == Table_types::STEP_FORWARD)
    {
      static QRegularExpression pattern("(.*)<b>([0-9]*) [A-Za-z0-9_]*</b>");
      QRegularExpressionMatch matches = pattern.match(standard_title);

      if (matches.hasMatch() && matches.capturedTexts().length() == 3)
      {
        _divisions_cross_table_title = QString("%1 Column given %2")
                                         .arg(matches.captured(2), matches.captured(1));
      }
    }
    else if (table_type == Table_types::FIRST_N_PREFS)
    {
      static QRegularExpression pattern("(.*)<b>[A-Za-z0-9_]*</b>(.*)");
      QRegularExpressionMatch matches = pattern.match(standard_title);
      if (matches.hasMatch() && matches.capturedTexts().length() == 3)
      {
        _divisions_cross_table_title = QString("%1 Column %2")
                                         .arg(matches.captured(1), matches.captured(2));
      }
    }
    else if (table_type == Table_types::LATER_PREFS)
    {
      if (n <= _get_later_prefs_n_fixed())
      {
        // Like step_forward
        static QRegularExpression pattern("(.*)<b>([0-9]*) [A-Za-z0-9_]*</b>");
        QRegularExpressionMatch matches = pattern.match(standard_title);

        if (matches.hasMatch() && matches.capturedTexts().length() == 3)
        {
          _divisions_cross_table_title = QString("%1 Column given %2")
                                           .arg(matches.captured(2), matches.captured(1));
        }
      }
      else
      {
        // Like first_n_prefs
        static QRegularExpression pattern("(.*)<b>[A-Za-z0-9_]*</b>(.*)");
        QRegularExpressionMatch matches = pattern.match(standard_title);
        if (matches.hasMatch() && matches.capturedTexts().length() == 3)
        {
          _divisions_cross_table_title = QString("%1 Column %2")
                                           .arg(matches.captured(1), matches.captured(2));
        }
      }
    }
    else if (table_type == Table_types::PREF_SOURCES)
    {
      const int pref_min = _get_pref_sources_min();
      const int pref_max = _get_pref_sources_max();
      if (n == 1)
      {
        if (pref_min == pref_max)
        {
          _divisions_cross_table_title = QString("%1 Column").arg(pref_min);
        }
        else
        {
          _divisions_cross_table_title = QString("%1-%2 Column")
            .arg(QString::number(pref_min), QString::number(pref_max));
        }
      }
      else
      {
        static QRegularExpression pattern("<b>1 [A-Za-z0-9_]*</b>(.*)");
        QRegularExpressionMatch matches = pattern.match(standard_title);
        if (matches.hasMatch() && matches.capturedTexts().length() == 2)
        {
          _divisions_cross_table_title = QString("1 Column %1")
                                           .arg(matches.captured(1));
        }
      }
    }

    static QRegularExpression given_pattern("given *$");
    _divisions_cross_table_title.replace(given_pattern, "");
  }
  else
  {
    standard_title.replace("<b>", "");
    standard_title.replace("</b>", "");

    if (table_type == Table_types::STEP_FORWARD)
    {
      static QRegularExpression pattern("(.*)( [A-Za-z0-9_]*)$");
      QRegularExpressionMatch matches = pattern.match(standard_title);
      if (matches.hasMatch() && matches.capturedTexts().length() == 3)
      {
        _divisions_cross_table_title = QString("%1 Column")
                                         .arg(matches.captured(1));
      }
    }
    else if (table_type == Table_types::FIRST_N_PREFS)
    {
      static QRegularExpression pattern("(.*)( [A-Za-z0-9_]*)$");
      QRegularExpressionMatch matches = pattern.match(standard_title);
      if (matches.hasMatch() && matches.capturedTexts().length() == 3)
      {
        _divisions_cross_table_title = QString("%1 Column")
                                         .arg(matches.captured(1));
      }
    }
    else if (table_type == Table_types::LATER_PREFS)
    {
      if (n <= _get_later_prefs_n_fixed())
      {
        static QRegularExpression pattern("(.*)( [A-Za-z0-9_]*)$");
        QRegularExpressionMatch matches = pattern.match(standard_title);
        if (matches.hasMatch() && matches.capturedTexts().length() == 3)
        {
          _divisions_cross_table_title = QString("%1 Column")
                                           .arg(matches.captured(1));
        }
      }
      else
      {
        static QRegularExpression pattern("(.*)( [A-Za-z0-9_]*)( in first [0-9]* prefs)$");
        QRegularExpressionMatch matches = pattern.match(standard_title);

        if (matches.hasMatch() && matches.capturedTexts().length() == 4)
        {
          _divisions_cross_table_title = QString("%1 Column %2")
                                           .arg(matches.captured(1), matches.captured(3));
        }
      }
    }
    else if (table_type == Table_types::PREF_SOURCES)
    {
      if (n == 1)
      {
        static QRegularExpression title_pattern("[A-Za-z0-9_]+$");
        standard_title.replace(title_pattern, "Column");
      }
      else
      {
        static QRegularExpression title_pattern("[A-Za-z0-9_]*,$");
        standard_title.replace(title_pattern, "Column,");
      }
      _divisions_cross_table_title = standard_title;
    }
  }

  static QRegularExpression all_space_pattern("^ +");
  static QRegularExpression trailing_space_pattern(" +$");
  static QRegularExpression multispace_pattern(" +");

  _divisions_cross_table_title.replace(all_space_pattern, "");
  _divisions_cross_table_title.replace(trailing_space_pattern, "");
  _divisions_cross_table_title.replace(multispace_pattern, " ");
}

void Widget::_make_divisions_cross_table()
{
  const bool custom = _get_table_type() == Table_types::CUSTOM;
  const int n       = custom ? _clicked_cells_two_axis.length() : _clicked_cells.length();
  if (n == 0)
  {
    return;
  } // Shouldn't happen

  const QString value_type = _get_value_type();

  const int num_divisions = _table_main_data.at(0).at(0).votes.length();
  const int num_cols      = _table_main_data.at(0).length();
  const int base_col      = n - 2; // For non-custom cross tables only
  const int main_col      = custom ? _clicked_cells_two_axis.at(0).j : n - 1;

  QStringList gps;
  if (custom)
  {
    gps = _custom_table_row_headers;
  }
  else
  {
    for (int i = 0; i < _table_main_groups_short.length(); i++)
    {
      gps.append(_table_main_groups_short.at(i));
    }
    gps.append("Exh");
  }

  QStringList divs;
  for (int i = 0; i < _divisions.length(); i++)
  {
    divs.append(_divisions.at(i));
  }
  divs.append(_state_full);

  if (custom)
  {
    _divisions_cross_table_title = "Custom" + (main_col == 0 ? "" : ": " + _custom_table_col_headers.at(main_col - 1));
  }
  else
  {
    _set_title_from_divisions_table();
  }

  if (value_type == VALUE_VOTES)
  {
    QVector<int> base;

    if (custom)
    {
      for (int i = 0; i < num_divisions; ++i)
      {
        base.append(_table_main_data_total_base.votes.at(i));
      }
    }
    else
    {
      if (base_col < 0)
      {
        for (int i = 0; i < num_divisions; i++)
        {
          base.append(_division_formal_votes.at(i));
        }
      }
      else
      {
        int idx = _table_main_data.at(base_col).at(_clicked_cells.at(n - 2)).sorted_idx;
        for (int i = 0; i < num_divisions; i++)
        {
          base.append(_table_main_data.at(base_col).at(idx).votes.at(i));
        }
      }
    }

    QVector<QVector<int>> table;
    for (int i = 0; i < num_divisions; i++)
    {
      table.append(QVector<int>());

      if (custom)
      {
        for (int j = 0; j < num_cols; ++j)
        {
          table[i].append(_table_main_data.at(main_col).at(j).votes.at(i));
        }
      }
      else
      {
        for (int j = 0; j < num_cols; j++)
        {
          const int idx = _table_main_data.at(main_col).at(j).sorted_idx;
          table[i].append(_table_main_data.at(main_col).at(idx).votes.at(i));
        }
      }
    }

    Table_window* w = new Table_window(Table_tag_divisions{}, base, divs, gps, table, _divisions_cross_table_title, this);
    _init_cross_table_window(w);
  }
  else
  {
    QVector<double> base;
    QVector<int> denominators;

    if (custom)
    {
      for (int i = 0; i < num_divisions; ++i)
      {
        const int div_total = _division_formal_votes.at(i);
        const int div_base  = _table_main_data_total_base.votes.at(i);
        denominators.append(value_type == VALUE_TOTAL_PERCENTAGES ? div_total : div_base);
        base.append(main_col == 0 ? 100. * div_base / div_total : -1.);
      }
    }
    else
    {
      if (base_col < 0)
      {
        for (int i = 0; i < num_divisions; i++)
        {
          base.append(100.);
          denominators.append(_division_formal_votes.at(i));
        }
      }
      else
      {
        for (int i = 0; i < num_divisions; i++)
        {
          const int idx_base         = _table_main_data.at(base_col).at(_clicked_cells.at(n - 2)).sorted_idx;
          const int base_denominator = _division_formal_votes.at(i);
          const int numerator        = _table_main_data.at(base_col).at(idx_base).votes.at(i);

          if (value_type == VALUE_TOTAL_PERCENTAGES)
          {
            denominators.append(_division_formal_votes.at(i));
          }
          else
          {
            denominators.append(numerator);
          }

          base.append(100. * numerator / base_denominator);
        }
      }
    }

    QVector<QVector<double>> table;
    for (int i = 0; i < num_divisions; i++)
    {
      table.append(QVector<double>());

      if (custom)
      {
        const bool total_percentages = (value_type == VALUE_TOTAL_PERCENTAGES);
        for (int j = 0; j < num_cols; ++j)
        {
          const int denominator = qMax(1,
                                       total_percentages ? _division_formal_votes.at(i)
                                       : main_col == 0   ? denominators.at(i)
                                                         : _table_main_data.at(0).at(j).votes.at(i));

          const int numerator = _table_main_data.at(main_col).at(j).votes.at(i);
          table[i].append(100. * numerator / denominator);
        }
      }
      else
      {
        for (int j = 0; j < num_cols; j++)
        {
          const int idx         = _table_main_data.at(main_col).at(j).sorted_idx;
          const int denominator = qMax(1, denominators.at(i));
          const int numerator   = _table_main_data.at(main_col).at(idx).votes.at(i);
          table[i].append(100. * numerator / denominator);
        }
      }
    }

    Table_window* w = new Table_window(Table_tag_divisions{}, base, divs, gps, table, _divisions_cross_table_title, this);
    _init_cross_table_window(w);
  }
}

void Widget::_make_booths_cross_table()
{
  const QString latest_out_path = _get_output_path("csv");

  _booths_output_file = QFileDialog::getSaveFileName(this,
                                                     "Save CSV",
                                                     latest_out_path,
                                                     "*.csv");

  if (_booths_output_file == "")
  {
    return;
  }

  const bool custom = _get_table_type() == Table_types::CUSTOM;
  if (custom && _clicked_cells_two_axis.length() == 0)
  {
    return;
  }

  QFile out_file(_booths_output_file);

  if (out_file.open(QIODevice::WriteOnly))
  {
    _timer.restart();
    QTextStream out(&out_file);
    const int num_output_cols = _table_main_data.at(0).length();
    const QString value_type  = _get_value_type();
    const int num_booths      = _booths.length();

    const int col      = custom ? _clicked_cells_two_axis.at(0).j : _clicked_cells.length() - 1;
    const int base_col = col - 1; // For non-custom cross tables only

    if (custom)
    {
      _divisions_cross_table_title = "Custom" + (col == 0 ? "" : ": " + _custom_table_col_headers.at(col - 1));
    }
    else
    {
      _set_title_from_divisions_table();
    }

    const QString title_line = QString("\"%1\"").arg(_divisions_cross_table_title);

    out << title_line << endl;

    QString header("Division,Booth,Longitude,Latitude,Total");

    if (custom)
    {
      if (col == 0)
      {
        // This is the only case in a custom table when the base
        // is constant for the whole output row.
        header += ",Base";
      }
      for (QString& row_header : _custom_table_row_headers)
      {
        header = QString("%1,%2").arg(header, row_header);
      }
    }
    else
    {
      header += ",Base";
      for (int i = 0; i < num_output_cols - 1; i++)
      {
        header = QString("%1,%2").arg(header, _get_short_group(i));
      }
      header = QString("%1,Exh").arg(header);
    }

    out << header << endl;

    for (int i = 0; i < num_booths; i++)
    {
      QString line = QString("%1,\"%2\",%3,%4,%5")
                       .arg(_booths.at(i).division, _booths.at(i).booth, QString::number(_booths.at(i).longitude),
                            QString::number(_booths.at(i).latitude), QString::number(_booths.at(i).formal_votes));

      int base = 0;

      if (custom)
      {
        base = _table_main_booth_data_total_base.at(i);
      }
      else
      {
        if (base_col < 0)
        {
          base = _booths.at(i).formal_votes;
        }
        else
        {
          base = _table_main_booth_data.at(base_col).at(_clicked_cells.at(base_col)).at(i);
        }
      }

      if (!(custom && col > 0))
      {
        if (value_type == VALUE_VOTES)
        {
          line = QString("%1,%2").arg(line, QString::number(base));
        }
        else
        {
          const double val = _booths.at(i).formal_votes == 0 ? 0. : 100. * base / _booths.at(i).formal_votes;
          line             = QString("%1,%2").arg(line, QString::number(val, 'f', 2));
        }
      }

      if (custom)
      {
        for (int j = 0; j < num_output_cols; ++j)
        {
          const int votes = col == 0 ? _table_main_booth_data_row_bases.at(j).at(i) : _table_main_booth_data.at(col - 1).at(j).at(i);

          if (value_type == VALUE_VOTES)
          {
            line = QString("%1,%2").arg(line, QString::number(votes));
          }
          else
          {
            const int denominator = qMax(1,
                                         value_type == VALUE_TOTAL_PERCENTAGES ? _booths.at(i).formal_votes
                                         : col == 0                            ? base
                                                                               : _table_main_booth_data_row_bases.at(j).at(i));

            const double val = base == 0 ? 0. : 100. * votes / denominator;
            line             = QString("%1,%2").arg(line, QString::number(val, 'f', 2));
          }
        }
      }
      else
      {
        for (int j = 0; j < num_output_cols; j++)
        {
          const int votes = _table_main_booth_data.at(col).at(j).at(i);

          if (value_type == VALUE_VOTES)
          {
            line = QString("%1,%2").arg(line, QString::number(votes));
          }
          else if (value_type == VALUE_PERCENTAGES)
          {
            const double val = base == 0 ? 0. : 100. * votes / base;
            line             = QString("%1,%2").arg(line, QString::number(val, 'f', 2));
          }
          else if (value_type == VALUE_TOTAL_PERCENTAGES)
          {
            const double val = _booths.at(i).formal_votes == 0 ? 0. : 100. * votes / _booths.at(i).formal_votes;
            line             = QString("%1,%2").arg(line, QString::number(val, 'f', 2));
          }
        }
      }

      out << line << endl;
    }

    out_file.close();
  }
  else
  {
    QMessageBox msg_box;
    msg_box.setText("Error: couldn't open file.");
    msg_box.exec();
  }

  _label_progress->setText("Export done");
  _unlock_main_interface();

  const QString file_type("csv");
  _update_output_path(_booths_output_file, file_type);
}

void Widget::_init_cross_table_data(int n_rows, int n_cols)
{
  if (n_cols == -1)
  {
    n_cols = n_rows;
  }

  _cross_table_data.clear();
  for (int i = 0; i < n_rows; i++)
  {
    _cross_table_data.append(QVector<int>());
    for (int j = 0; j < n_cols; j++)
    {
      _cross_table_data[i].append(0);
    }
  }
}

void Widget::_process_thread_sql_cross_table(const QVector<QVector<int>>& table)
{
  const int n = table.length();

  for (int i = 0; i < n; i++)
  {
    for (int j = 0; j < n; j++)
    {
      _cross_table_data[i][j] += table.at(i).at(j);
    }
  }

  _completed_threads++;

  if (_completed_threads == _current_threads)
  {
    if (_table_main_data.at(0).length() != n)
    {
      QMessageBox msg_box;
      msg_box.setText("Programmer error: Mismatch between table data length and cross table size.  Sorry. :(");
      msg_box.exec();
      _unlock_main_interface();
      return;
    }

    const QString value_type = _get_value_type();
    const QString table_type = _get_table_type();
    const int current_div    = _get_current_division();
    int base_col             = _clicked_cells.length() - 1;
    if (base_col < 0)
    {
      base_col = 0;
    }

    if (table_type == Table_types::STEP_FORWARD && base_col == get_num_groups() - 1)
    {
      base_col--;
    }
    if (table_type == Table_types::FIRST_N_PREFS && base_col == _get_n_first_prefs() - 1)
    {
      base_col--;
    }
    if (table_type == Table_types::LATER_PREFS && base_col == _get_later_prefs_n_up_to() - 1)
    {
      base_col--;
    }
    if (table_type == Table_types::PREF_SOURCES)
    {
      base_col = 0;
    }

    QVector<int> ignore_groups;
    for (int i = 0; i < base_col; i++)
    {
      ignore_groups.append(_clicked_cells.at(i));
    }

    const int current_num_groups = get_num_groups();
    QStringList gps;

    for (int i = 0; i < current_num_groups; i++)
    {
      gps.append(_table_main_groups_short.at(i));
    }
    gps.append("Exh");

    if (value_type == VALUE_VOTES)
    {
      QVector<int> base;

      for (int i = 0; i < n; i++)
      {
        base.append(0);
      }

      for (int i = 0; i < n; i++)
      {
        const int gp = _table_main_data[base_col][i].group_id;
        base[gp]     = _table_main_data[base_col][i].votes.at(current_div);
      }

      Table_window* w = new Table_window(Table_tag_standard{}, base, gps, ignore_groups, _cross_table_data, _cross_table_title, this);
      _init_cross_table_window(w);
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

      const int base_denominator = _division_formal_votes.at(current_div);

      for (int i = 0; i < n; i++)
      {
        const int gp    = _table_main_data[base_col][i].group_id;
        const int votes = _table_main_data[base_col][i].votes.at(current_div);
        int main_denominator;

        base[gp] = 100. * votes / base_denominator;

        if (value_type == VALUE_TOTAL_PERCENTAGES)
        {
          main_denominator = _division_formal_votes.at(current_div);
        }
        else
        {
          main_denominator = votes < 1 ? 1 : votes;
        }

        for (int j = 0; j < n; j++)
        {
          table_data[gp][j] = 100. * _cross_table_data.at(gp).at(j) / main_denominator;
        }
      }

      Table_window* w = new Table_window(Table_tag_standard{}, base, gps, ignore_groups, table_data, _cross_table_title, this);
      _init_cross_table_window(w);
    }

    _show_calculation_time();
    _unlock_main_interface();
  }
  else
  {
    _label_progress->setText(QString("%1/%2 complete")
      .arg(QString::number(_completed_threads), QString::number(_current_threads)));
  }
}

void Widget::_process_thread_sql_custom_main_table(
  const QVector<int>& total_base, const QVector<QVector<int>>& bases, const QVector<QVector<QVector<int>>>& table)
{
  // Careful: the table from the worker is indexed [row][col][booth], but the
  // main table is indexed [col][row][booth] because that was the natural
  // organisation for the interactive table types where you add one new column
  // at a time.

  // Booth data remains separated in three parts: total_base, row_bases, data
  // The main table data has total_base separate, but the row_bases become the
  // first column main data.

  const int num_booths = _booths.length();
  const int num_rows   = table.length();
  // num_cols here applies to the incoming table data and to
  // _table_main_booth_data, but not to _table_main_data.
  const int num_cols = _table_main_booth_data.length();

  for (int i_booth = 0; i_booth < num_booths; ++i_booth)
  {
    _table_main_booth_data_total_base[i_booth] += total_base.at(i_booth);

    for (int i_row = 0; i_row < num_rows; ++i_row)
    {
      _table_main_booth_data_row_bases[i_row][i_booth] += bases.at(i_row).at(i_booth);

      for (int i_col = 0; i_col < num_cols; ++i_col)
      {
        _table_main_booth_data[i_col][i_row][i_booth] += table.at(i_row).at(i_col).at(i_booth);
      }
    }
  }

  _completed_threads++;

  if (_completed_threads == _current_threads)
  {
    // Sum each division's votes to get the division totals.
    // First column is the row bases, so the number of columns in
    // _table_main_data will be num_cols + 1.
    // (The number of columns in the table view will be one higher still,
    // thanks to the axis labels, i.e. group abbreviations or numbers.)

    const int num_divisions = _divisions.length();

    for (int i_booth = 0; i_booth < num_booths; ++i_booth)
    {
      const int booth_total_base_votes = _table_main_booth_data_total_base.at(i_booth);
      const int division               = _booths.at(i_booth).division_id;
      _table_main_data_total_base.votes[division] += booth_total_base_votes;
      _table_main_data_total_base.votes[num_divisions] += booth_total_base_votes;

      for (int i_row = 0; i_row < num_rows; ++i_row)
      {
        const int booth_row_base_votes = _table_main_booth_data_row_bases.at(i_row).at(i_booth);
        _table_main_data[0][i_row].votes[division] += booth_row_base_votes;
        _table_main_data[0][i_row].votes[num_divisions] += booth_row_base_votes;

        for (int i_col = 0; i_col < num_cols; ++i_col)
        {
          const int booth_votes = _table_main_booth_data.at(i_col).at(i_row).at(i_booth);
          _table_main_data[i_col + 1][i_row].votes[division] += booth_votes;
          _table_main_data[i_col + 1][i_row].votes[num_divisions] += booth_votes;
        }
      }
    }

    // Calculate percentages for sorting
    for (int i_col = 0; i_col <= num_cols; ++i_col)
    {
      for (int i_row = 0; i_row < num_rows; ++i_row)
      {
        for (int i_div = 0; i_div <= num_divisions; i_div++)
        {
          const int denominator   = i_col == 0 ? _table_main_data_total_base.votes.at(i_div) : _table_main_data.at(0).at(i_row).votes.at(i_div);
          const int numerator     = _table_main_data.at(i_col).at(i_row).votes.at(i_div);
          const double percentage = 100. * numerator / static_cast<double>(qMax(1, denominator));

          _table_main_data[i_col][i_row].percentages[i_div] = percentage;
        }
      }
    }

    _set_all_main_table_cells_custom();

    _show_calculation_time();
    _unlock_main_interface();
  }
}

void Widget::_process_thread_sql_custom_popup_table(int total_base, const QVector<int>& bases, const QVector<QVector<int>>& table)
{
  _custom_cross_table_total_base += total_base;
  const int n_rows = table.length();
  const int n_cols = table.at(0).length();

  for (int i = 0; i < n_rows; i++)
  {
    _custom_cross_table_row_bases[i] += bases.at(i);
    for (int j = 0; j < n_cols; j++)
    {
      _cross_table_data[i][j] += table.at(i).at(j);
    }
  }

  _completed_threads++;

  if (_completed_threads == _current_threads)
  {
    const int current_div     = _get_current_division();
    QString title             = (current_div == _divisions.length() ? _state_full : _divisions.at(current_div)) + "\n";
    const QString filter_text = _lineedit_custom_filter->text();
    if (!filter_text.isEmpty())
    {
      title += "Filter: " + filter_text + "\n";
    }
    if (_custom_rows.type == Custom_axis_type::NPP)
    {
      title += "Rows are N-party preferred\n";
    }
    title += _lineedit_custom_cell->text();
    const QString value_type = _get_value_type();

    int current_num_groups = get_num_groups();
    QStringList gps;

    for (int i = 0; i < current_num_groups; i++)
    {
      gps.append(_table_main_groups_short.at(i));
    }
    gps.append("Exh");

    if (value_type == VALUE_VOTES)
    {
      Table_window* w = new Table_window(
        Table_tag_custom{}, _custom_cross_table_row_bases, _custom_table_row_headers, _custom_table_col_headers, _cross_table_data, title, this);
      _init_cross_table_window(w);
    }
    else
    {
      QVector<QVector<double>> table_data;
      QVector<double> base;

      for (int i = 0; i < n_rows; i++)
      {
        base.append(0.);
        table_data.append(QVector<double>());
        for (int j = 0; j < n_cols; j++)
        {
          table_data[i].append(0.);
        }
      }

      const int base_denominator = value_type == VALUE_TOTAL_PERCENTAGES
                                     ? _division_formal_votes.at(current_div)
                                     : qMax(1, _custom_cross_table_total_base);

      if (value_type == VALUE_PERCENTAGES)
      {
        title += "\nFiltered base: " + QString::number(100. * static_cast<double>(base_denominator) / _division_formal_votes.at(current_div), 'f', 2);
      }

      for (int i = 0; i < n_rows; i++)
      {
        const int votes = _custom_cross_table_row_bases.at(i);

        const int main_denominator = value_type == VALUE_TOTAL_PERCENTAGES
                                       ? _division_formal_votes.at(current_div)
                                       : qMax(1, votes);

        base[i] = 100. * votes / base_denominator;

        for (int j = 0; j < n_cols; j++)
        {
          table_data[i][j] = 100. * _cross_table_data.at(i).at(j) / main_denominator;
        }
      }

      Table_window* w = new Table_window(
        Table_tag_custom{}, base, _custom_table_row_headers, _custom_table_col_headers, table_data, title, this);
      _init_cross_table_window(w);
    }

    _show_calculation_time();
    _unlock_main_interface();
  }
  else
  {
    _label_progress->setText(QString("%1/%2 complete")
      .arg(QString::number(_completed_threads), QString::number(_current_threads)));
  }
}

void Widget::_process_thread_sql_custom_every_expr(int axis, const QVector<int>& numbers)
{
  if (axis == Custom_row_col::ROW)
  {
    _custom_rows.numbers.append(numbers);
  }
  else if (axis == Custom_row_col::COL)
  {
    _custom_cols.numbers.append(numbers);
  }

  _num_custom_every_expr_threads_completed++;

  if (_num_custom_every_expr_threads_completed == _num_custom_every_expr_threads)
  {
    const QSet<int> row_numbers_set = QSet<int>(_custom_rows.numbers.begin(), _custom_rows.numbers.end());
    QList<int> row_numbers          = row_numbers_set.values();
    std::sort(row_numbers.begin(), row_numbers.end());
    _custom_rows.numbers.assign(row_numbers.begin(), row_numbers.end());

    const QSet<int> col_numbers_set = QSet<int>(_custom_cols.numbers.begin(), _custom_cols.numbers.end());
    QList<int> col_numbers          = col_numbers_set.values();
    std::sort(col_numbers.begin(), col_numbers.end());
    _custom_cols.numbers.assign(col_numbers.begin(), col_numbers.end());

    _calculate_custom_query();
  }
}

void Widget::_init_cross_table_window(Table_window* w)
{
  w->setMinimumSize(QSize(200, 200));
  w->resize(500, 500);
  w->setWindowTitle("Cross table");
  w->show();
}

void Widget::_lock_main_interface()
{
  _doing_calculation = true;
  _button_load->setEnabled(false);
  _combo_abtl->setEnabled(false);
  _combo_table_type->setEnabled(false);
  _combo_value_type->setEnabled(false);
  _combo_division->setEnabled(false);
  _spinbox_first_n_prefs->setEnabled(false);
  _spinbox_pref_sources_max->setEnabled(false);
  _spinbox_pref_sources_min->setEnabled(false);
  _spinbox_later_prefs_fixed->setEnabled(false);
  _spinbox_later_prefs_up_to->setEnabled(false);
  _lineedit_custom_filter->setEnabled(false);
  _lineedit_custom_rows->setEnabled(false);
  _lineedit_custom_cols->setEnabled(false);
  _lineedit_custom_cell->setEnabled(false);
  _combo_custom_table_target->setEnabled(false);
  _button_copy_main_table->setEnabled(false);
  _button_export_main_table->setEnabled(false);
  _button_cross_table->setEnabled(false);
  _button_calculate_custom->setEnabled(false);
  _button_abbreviations->setEnabled(false);
  _button_divisions_copy->setEnabled(false);
  _button_divisions_export->setEnabled(false);
  _button_divisions_booths_export->setEnabled(false);
  _button_divisions_cross_table->setEnabled(false);
  _button_booths_cross_table->setEnabled(false);
  _button_map_copy->setEnabled(false);
  _button_map_export->setEnabled(false);
}

void Widget::_unlock_main_interface()
{
  _doing_calculation = false;
  _button_load->setEnabled(true);
  _combo_abtl->setEnabled(true);
  _combo_table_type->setEnabled(true);
  _combo_value_type->setEnabled(true);
  _combo_division->setEnabled(true);
  _spinbox_first_n_prefs->setEnabled(true);
  _spinbox_pref_sources_max->setEnabled(true);
  _spinbox_pref_sources_min->setEnabled(true);
  _spinbox_later_prefs_fixed->setEnabled(true);
  _spinbox_later_prefs_up_to->setEnabled(true);
  _lineedit_custom_filter->setEnabled(true);
  _lineedit_custom_rows->setEnabled(true);
  _lineedit_custom_cols->setEnabled(true);
  _lineedit_custom_cell->setEnabled(true);
  _combo_custom_table_target->setEnabled(true);
  _button_copy_main_table->setEnabled(true);
  _button_export_main_table->setEnabled(true);
  _button_calculate_custom->setEnabled(true);
  _button_map_copy->setEnabled(true);
  _button_map_export->setEnabled(true);

  const QString table_type = _get_table_type();

  if (table_type != Table_types::NPP && table_type != Table_types::CUSTOM)
  {
    _button_cross_table->setEnabled(true);

    if (_clicked_cells.length() > 0)
    {
      _button_divisions_cross_table->setEnabled(true);
      _button_booths_cross_table->setEnabled(true);
    }
  }

  if ((table_type != Table_types::NPP && _clicked_cells.length() > 0) || (table_type == Table_types::NPP && _clicked_cells_two_axis.length() > 0))
  {
    _button_divisions_copy->setEnabled(true);
    _button_divisions_export->setEnabled(true);
  }

  if (table_type == Table_types::NPP && _clicked_cells_two_axis.length() > 0)
  {
    _button_divisions_booths_export->setEnabled(true);
  }

  if (table_type == Table_types::CUSTOM)
  {
    if (_table_main_model->columnCount() == 0)
    {
      _button_copy_main_table->setEnabled(false);
      _button_export_main_table->setEnabled(false);
    }
    _enable_division_export_buttons_custom();
  }

  _button_abbreviations->setEnabled(true);
}

void Widget::_show_calculation_time()
{
  const double time_elapsed = _timer.elapsed() / 1000.;
  _label_progress->setText(QString("Calculation done, %1s").arg(time_elapsed, 0, 'f', 1));
}

void Widget::_change_abtl(int i)
{
  Q_UNUSED(i);

  if (get_abtl() == "atl" || _get_table_type() == Table_types::CUSTOM)
  {
    _label_toggle_names->hide();
  }
  else
  {
    _label_toggle_names->show();
  }

  if (_opened_database)
  {
    _set_table_groups();

    const int current_num_groups = get_num_groups();
    _spinbox_first_n_prefs->setMaximum(current_num_groups);
    _spinbox_later_prefs_up_to->setMaximum(current_num_groups);
    _spinbox_pref_sources_max->setMaximum(current_num_groups);
    _spinbox_pref_sources_min->setMaximum(_get_pref_sources_max());
    _spinbox_pref_sources_max->setMinimum(_get_pref_sources_min());
  }

  _clear_divisions_table();
  _reset_table();
  if (_opened_database)
  {
    _add_column_to_main_table();
  }
}

QString Widget::_get_groups_table()
{
  if (_combo_abtl->currentData() == "atl")
  {
    return "groups";
  }
  else
  {
    return "candidates";
  }
}

void Widget::_reset_table()
{
  _clear_main_table_data();
  _table_main_model->clear();
  _clicked_cells.clear();
  _clicked_n_parties.clear();
  _clicked_cells_two_axis.clear();
  _label_custom_filtered_base->setText("");

  if (_opened_database)
  {
    _setup_main_table();
  }
}

void Widget::_change_table_type(int i)
{
  Q_UNUSED(i);

  QString table_type = _get_table_type();

  _container_first_n_prefs_widgets->hide();
  _container_later_prefs_widgets->hide();
  _container_pref_sources_widgets->hide();
  _container_n_party_preferred_widgets->hide();
  _container_custom_widgets->hide();
  _container_copy_main_table->hide();
  if (table_type == Table_types::FIRST_N_PREFS)
  {
    _container_first_n_prefs_widgets->show();
  }
  if (table_type == Table_types::LATER_PREFS)
  {
    _container_later_prefs_widgets->show();
  }
  if (table_type == Table_types::PREF_SOURCES)
  {
    _container_pref_sources_widgets->show();
  }
  if (table_type == Table_types::CUSTOM)
  {
    _container_custom_widgets->show();
    _container_copy_main_table->show();
  }
  if (table_type == Table_types::NPP)
  {
    _container_n_party_preferred_widgets->show();
    _container_copy_main_table->show();
  }

  _button_calculate_custom->hide();

  if (table_type == Table_types::CUSTOM)
  {
    _button_calculate_after_spinbox->hide();
    _button_calculate_custom->show();
    _button_calculate_custom->setEnabled(_opened_database);
    _button_cross_table->setEnabled(false);
    _button_copy_main_table->setEnabled(false);
    _button_export_main_table->setEnabled(false);
  }
  else if (table_type == Table_types::NPP || table_type == Table_types::STEP_FORWARD)
  {
    _button_calculate_after_spinbox->hide();
  }
  else
  {
    _button_calculate_after_spinbox->show();
    _button_calculate_after_spinbox->setEnabled(false);
  }

  bool show_toggle_sort  = true;
  bool show_toggle_names = get_abtl() == "btl";
  if (table_type == Table_types::NPP)
  {
    _label_sort->setText("<i>Click on column header to sort</i>");
    _label_sort->setCursor(Qt::ArrowCursor);
    _reset_npp_sort();
    _button_cross_table->setEnabled(false);
  }
  else if (table_type == Table_types::CUSTOM)
  {
    show_toggle_sort  = false;
    show_toggle_names = false;
  }
  else
  {
    _label_sort->setText("<i>Toggle sort</i>");
    _label_sort->setCursor(Qt::PointingHandCursor);
  }
  _label_sort->setVisible(show_toggle_sort);
  _label_toggle_names->setVisible(show_toggle_names);

  _clear_divisions_table();
  _reset_table();
  if (_opened_database)
  {
    _add_column_to_main_table();
  }
}

void Widget::_change_value_type(int i)
{
  Q_UNUSED(i);
  _set_all_main_table_cells();

  if (_get_value_type() == VALUE_VOTES)
  {
    _spinbox_map_min->setMaximum(1000000.);
    _spinbox_map_max->setMaximum(1000000.);
    _spinbox_map_min->setDecimals(0);
    _spinbox_map_max->setDecimals(0);
    _map_divisions_model.set_decimals(0);
    _map_booths_model.set_decimals_mouseover(0);
  }
  else
  {
    _spinbox_map_min->setMaximum(100.);
    _spinbox_map_max->setMaximum(100.);
    _spinbox_map_min->setDecimals(1);
    _spinbox_map_max->setDecimals(1);
    _map_divisions_model.set_decimals(1);
    _map_booths_model.set_decimals_mouseover(1);
  }

  if (_table_divisions_data.length() > 0)
  {
    _sort_divisions_table_data();
    _set_divisions_table_title();
    _fill_in_divisions_table();
  }
}

void Widget::_change_division(int i)
{
  // When the selected division changes, the only change
  // is to the table and to the booth map.

  if (_doing_calculation)
  {
    return;
  }

  if (i == _divisions.length())
  {
    i = -1;
  }
  _map_booths_model.update_active_division(i);

  const QString table_type = _get_table_type();

  if (table_type == Table_types::NPP)
  {
    _sort_main_table_npp();
  }
  else if (table_type == Table_types::CUSTOM)
  {
    _sort_main_table_custom();
  }
  else
  {
    _sort_main_table();
  }
}

void Widget::_spinbox_change()
{
  _clear_divisions_table();
  _reset_table();
  _button_cross_table->setEnabled(false);
  if (_opened_database)
  {
    _button_calculate_after_spinbox->setEnabled(true);
  }
}

void Widget::_show_error_message(const QString& msg)
{
  QMessageBox box;
  box.setIcon(QMessageBox::Critical);
  box.setWindowTitle(tr("Error"));
  box.setText(msg);
  box.setTextFormat(Qt::RichText);
  box.setStandardButtons(QMessageBox::Ok);
  box.exec();
}

void Widget::_change_first_n_prefs(int i)
{
  Q_UNUSED(i);
  if (_doing_calculation)
  {
    return;
  }
  _spinbox_change();
}

void Widget::_change_later_prefs_fixed(int i)
{
  Q_UNUSED(i);
  if (_doing_calculation)
  {
    return;
  }
  _spinbox_later_prefs_up_to->setMinimum(_get_later_prefs_n_fixed() + 1);
  _spinbox_change();
}

void Widget::_change_later_prefs_up_to(int i)
{
  Q_UNUSED(i);
  if (_doing_calculation)
  {
    return;
  }
  _spinbox_later_prefs_fixed->setMaximum(_get_later_prefs_n_up_to() - 1);
  _spinbox_change();
}

void Widget::_change_pref_sources_min(int i)
{
  Q_UNUSED(i);
  if (_doing_calculation)
  {
    return;
  }
  _spinbox_pref_sources_max->setMinimum(_get_pref_sources_min());
  _clear_divisions_table();
  _spinbox_change();
}

void Widget::_change_pref_sources_max(int i)
{
  Q_UNUSED(i);
  if (_doing_calculation)
  {
    return;
  }
  _spinbox_pref_sources_min->setMaximum(_get_pref_sources_max());
  _spinbox_change();
}

void Widget::_change_map_type()
{
  const QString map_type = _get_map_type();

  if (map_type == MAP_DIVISIONS)
  {
    _label_map_booth_min_1->hide();
    _label_map_booth_min_2->hide();
    _spinbox_map_booth_threshold->hide();

    _map_booths_model.set_visible(false);
    _map_divisions_model.set_fill_visible(true);
  }
  else
  {
    _label_map_booth_min_1->show();
    _label_map_booth_min_2->show();
    _spinbox_map_booth_threshold->show();

    _map_divisions_model.set_fill_visible(false);
    _map_booths_model.update_prepoll_flag(map_type == MAP_PREPOLL_BOOTHS);
    _map_booths_model.set_idle(false);
    _update_map_booths();
    _map_booths_model.set_visible(true);
  }
}

void Widget::_change_map_booth_threshold(int i)
{
  _map_booths_model.update_min_votes(i);
}

void Widget::_update_map_scale_minmax()
{
  _spinbox_map_min->setMaximum(_spinbox_map_max->value());
  _spinbox_map_max->setMinimum(_spinbox_map_min->value());
}

void Widget::_toggle_names()
{
  if (get_abtl() == "atl" || _doing_calculation)
  {
    return;
  }
  _show_btl_headers = !_show_btl_headers;

  if (_show_btl_headers)
  {
    _table_main->verticalHeader()->show();
  }
  else
  {
    _table_main->verticalHeader()->hide();
  }

  if (_get_table_type() == Table_types::NPP)
  {
    _set_main_table_row_height();
  }
}

void Widget::_toggle_sort()
{
  const QString table_type = _get_table_type();
  if (table_type == Table_types::NPP || table_type == Table_types::CUSTOM || _doing_calculation)
  {
    return;
  }

  _sort_ballot_order = !_sort_ballot_order;
  _sort_main_table();
}

void Widget::_sort_main_table()
{
  const int num_table_cols = _table_main_data.length();
  if (num_table_cols == 0)
  {
    return;
  }

  const int num_clicked_cells = _clicked_cells.length();

  for (int i = 0; i < num_table_cols; i++)
  {
    _sort_table_column(i);
    _set_main_table_cells(i);

    if (i < num_clicked_cells)
    {
      for (int j = 0; j < _num_table_rows; j++)
      {
        if (_table_main_data.at(i).at(j).group_id == _clicked_cells.at(i))
        {
          _highlight_cell(j, i);
        }
        else
        {
          _fade_cell(j, i);
        }
      }
    }
  }
}

void Widget::_sort_divisions_table_data()
{
  const int i     = _sort_divisions.i;
  const bool desc = _sort_divisions.is_descending;

  if (i == 0)
  {
    // Sorting by division
    std::sort(_table_divisions_data.begin(), _table_divisions_data.end(), [&](Table_divisions_item a, Table_divisions_item b) -> bool
              { return (a.division < b.division) != desc; });
  }
  else
  {
    const QString value_type = _get_value_type();
    // The comparators in here all fall back onto the sort-by-division
    // if the votes are equal.  If this fallback isn't present, then it
    // seems std::sort() gets confused, because during different
    // comparisons, there may hold a < b and also b < a.  In such a case,
    // the iterator may continue iterating through memory addresses until
    // it leaves the QVector entirely, causing a segfault.

    if (value_type == VALUE_VOTES)
    {
      std::sort(_table_divisions_data.begin(), _table_divisions_data.end(), [&](Table_divisions_item a, Table_divisions_item b) -> bool
                {
        if (a.division == b.division) { return false; }
        if (a.votes.at(i - 1) == b.votes.at(i - 1))
        {
          return (a.division < b.division) != desc;
        }
        return (a.votes.at(i - 1) < b.votes.at(i - 1)) != desc; });
    }
    else if (value_type == VALUE_PERCENTAGES)
    {
      std::sort(_table_divisions_data.begin(), _table_divisions_data.end(), [&](Table_divisions_item a, Table_divisions_item b) -> bool
                {
        if (a.division == b.division) { return false; }
        if (qAbs(a.percentage.at(i - 1) - b.percentage.at(i - 1)) < 1.e-10)
        {
          return (a.division < b.division) != desc;
        }
        return (a.percentage.at(i - 1) < b.percentage.at(i - 1)) != desc; });
    }
    else if (value_type == VALUE_TOTAL_PERCENTAGES)
    {
      std::sort(_table_divisions_data.begin(), _table_divisions_data.end(), [&](Table_divisions_item a, Table_divisions_item b) -> bool
                {
        if (a.division == b.division) { return false; }
        if (qAbs(a.total_percentage.at(i - 1) - b.total_percentage.at(i - 1)) < 1.e-10)
        {
          return (a.division < b.division) != desc;
        }
        return (a.total_percentage.at(i - 1) < b.total_percentage.at(i - 1)) != desc; });
    }
  }
}

void Widget::_reset_npp_sort()
{
  if (_sort_ballot_order)
  {
    _sort_npp.i             = 1;
    _sort_npp.is_descending = false;
  }
  else
  {
    _sort_npp.i             = 0;
    _sort_npp.is_descending = true;
  }
}

void Widget::_slot_click_main_table_header(int i)
{
  const QString table_type = _get_table_type();
  if (table_type != Table_types::NPP && table_type != Table_types::CUSTOM)
  {
    return;
  }

  if (table_type == Table_types::NPP)
  {
    _change_npp_sort(i);
    return;
  }
  if (table_type == Table_types::CUSTOM)
  {
    _sort_main_table_rows_custom(i);
    return;
  }
}

void Widget::_change_npp_sort(int i)
{
  if (_doing_calculation)
  {
    return;
  }
  if (_get_table_type() != Table_types::NPP)
  {
    return;
  }
  if (_table_main_data.length() == 1 && i > 1)
  {
    return;
  }

  if (i == _sort_npp.i && i != 1)
  {
    _sort_npp.is_descending = !_sort_npp.is_descending;
  }
  else
  {
    _sort_npp.is_descending = (i == 1) ? false : true;
  }

  _sort_npp.i = i;
  _sort_main_table_npp();
}

void Widget::_reset_divisions_sort()
{
  if (_sort_ballot_order || _get_table_type() == Table_types::NPP)
  {
    _sort_divisions.i             = 0;
    _sort_divisions.is_descending = false;
  }
  else
  {
    _sort_divisions.i             = 1;
    _sort_divisions.is_descending = true;
  }
}

void Widget::_change_divisions_sort(int i)
{
  if (i == _sort_divisions.i)
  {
    _sort_divisions.is_descending = !_sort_divisions.is_descending;
  }
  else
  {
    _sort_divisions.i             = i;
    _sort_divisions.is_descending = i == 0 ? false : true;
  }

  _sort_divisions_table_data();
  _fill_in_divisions_table();
}

void Widget::_set_divisions_table_title()
{
  const QString table_type = _get_table_type();
  const QString value_type = _get_value_type();
  const int clicked_n      = _clicked_cells.length();

  // ~~~~~ Title ~~~~~
  QString table_title("");
  QString map_title("");
  QString space("");
  QString open_bold, close_bold;
  if (table_type == Table_types::STEP_FORWARD)
  {
    if (value_type == VALUE_PERCENTAGES)
    {
      for (int i = 0; i < clicked_n; i++)
      {
        if (i == clicked_n - 1)
        {
          open_bold  = "<b>";
          close_bold = "</b>";
        }
        else
        {
          open_bold  = "";
          close_bold = "";
        }
        table_title = QString("%1%2%3%4&nbsp;%5%6")
                        .arg(table_title, space, open_bold, QString::number(i + 1), _get_short_group(_clicked_cells.at(i)), close_bold);

        space = " ";
      }
    }
    else
    {
      table_title = "<b>";
      for (int i = 0; i < clicked_n; i++)
      {
        table_title = QString("%1%2%3&nbsp;%4")
                        .arg(table_title, space, QString::number(i + 1), _get_short_group(_clicked_cells.at(i)));

        space = " ";
      }

      table_title = QString("%1</b>").arg(table_title);
    }
  }
  else if (table_type == Table_types::FIRST_N_PREFS)
  {
    const int n_first = _get_n_first_prefs();
    table_title       = QString("In first %1 prefs: ").arg(n_first);

    if (value_type == VALUE_PERCENTAGES)
    {
      table_title = QString("%1<b>%2</b>")
                      .arg(table_title, _get_short_group(_clicked_cells.at(clicked_n - 1)));

      QString prefix = " given";
      for (int i = 0; i < clicked_n - 1; i++)
      {
        table_title = QString("%1%2 %3")
                        .arg(table_title, prefix, _get_short_group(_clicked_cells.at(i)));

        prefix = ",";
      }
    }
    else
    {
      table_title    = QString("%1<b>").arg(table_title);
      QString prefix = "";
      for (int i = 0; i < clicked_n; i++)
      {
        table_title = QString("%1%2 %3")
                        .arg(table_title, prefix, _get_short_group(_clicked_cells.at(i)));

        prefix = ",";
      }

      table_title = QString("%1</b>").arg(table_title);
    }
  }
  else if (table_type == Table_types::LATER_PREFS)
  {
    const int n_fixed = _get_later_prefs_n_fixed();
    const int n_up_to = _get_later_prefs_n_up_to();

    if (value_type == VALUE_PERCENTAGES)
    {
      if (clicked_n <= n_fixed)
      {
        // This is just step forward

        space = "";

        for (int i = 0; i < clicked_n; i++)
        {
          if (i == clicked_n - 1)
          {
            open_bold  = "<b>";
            close_bold = "</b>";
          }
          else
          {
            open_bold  = "";
            close_bold = "";
          }
          table_title = QString("%1%2%3%4&nbsp;%5%6")
                          .arg(table_title, space, open_bold, QString::number(i + 1), _get_short_group(_clicked_cells.at(i)), close_bold);

          space = " ";
        }
      }
      else
      {
        table_title = QString("<b>%1</b> in first %2 prefs, given")
                        .arg(_get_short_group(_clicked_cells.at(clicked_n - 1)), QString::number(n_up_to));

        QString prefix("");

        for (int i = 0; i < n_fixed; i++)
        {
          table_title = QString("%1 %2&nbsp;%3")
                          .arg(table_title, QString::number(i + 1), _get_short_group(_clicked_cells.at(i)));
        }

        prefix = ";";

        for (int i = n_fixed; i < clicked_n - 1; i++)
        {
          table_title = QString("%1%2 %3")
                          .arg(table_title, prefix, _get_short_group(_clicked_cells.at(i)));

          prefix = ",";
        }

        if (clicked_n > n_fixed + 1)
        {
          table_title = QString("%1 in first %2 prefs")
                          .arg(table_title, QString::number(n_up_to));
        }
      }
    }
    else
    {
      // later_prefs; not percentages

      table_title = "<b>";
      space       = "";

      if (clicked_n <= n_fixed)
      {
        // Step forward

        for (int i = 0; i < clicked_n; i++)
        {
          table_title = QString("%1%2%3&nbsp;%4")
                          .arg(table_title, space, QString::number(i + 1), _get_short_group(_clicked_cells.at(i)));

          space = " ";
        }
      }
      else
      {
        // First the step-forward part

        for (int i = 0; i < n_fixed; i++)
        {
          table_title = QString("%1%2%3&nbsp;%4")
                          .arg(table_title, space, QString::number(i + 1), _get_short_group(_clicked_cells.at(i)));

          space = " ";
        }

        // The the later prefs.
        QString prefix(";");

        for (int i = n_fixed; i < clicked_n; i++)
        {
          table_title = QString("%1%2 %3")
                          .arg(table_title, prefix, _get_short_group(_clicked_cells.at(i)));

          prefix = ",";
        }

        table_title = QString("%1 in first %2 prefs")
                        .arg(table_title, QString::number(n_up_to));
      }

      table_title = QString("%1</b>").arg(table_title);
    }
  }
  else if (table_type == Table_types::PREF_SOURCES)
  {
    const int pref_min = _get_pref_sources_min();
    const int pref_max = _get_pref_sources_max();

    QString pref_part;

    if (pref_min == pref_max)
    {
      pref_part = QString("%1&nbsp;%2").arg(QString::number(pref_min), _get_short_group(_clicked_cells.at(0)));
    }
    else
    {
      pref_part = QString("%1-%2&nbsp;%3")
                    .arg(QString::number(pref_min), QString::number(pref_max), _get_short_group(_clicked_cells.at(0)));
    }

    if (clicked_n == 1)
    {
      table_title = QString("<b>%1</b>").arg(pref_part);
    }
    else
    {
      QString source_part = QString("1&nbsp;%1").arg(_get_short_group(_clicked_cells.at(1)));

      if (value_type == VALUE_PERCENTAGES)
      {
        table_title = QString("<b>%1</b> given %2").arg(source_part, pref_part);
      }
      else
      {
        table_title = QString("<b>%1, %2</b>").arg(source_part, pref_part);
      }
    }
  }
  else if (table_type == Table_types::NPP)
  {
    const int n              = _get_n_preferred();
    const QString pc         = get_abtl() == "atl" ? "P" : "C";
    const QString from_party = _get_short_group(_clicked_cells_two_axis.at(0).i);

    if (_clicked_cells_two_axis.at(0).i == get_num_groups())
    {
      table_title = QString("<b>%1%2P")
                      .arg(QString::number(n), pc);

      map_title = QString("<b>%1 %2%3P")
                    .arg(_table_main_model->horizontalHeaderItem(_clicked_cells_two_axis.at(0).j)->text(), QString::number(n), pc);
    }
    else
    {
      table_title = QString("<b>%1%2P pref flow from %3</b>")
                      .arg(QString::number(n), pc, from_party);

      map_title = QString("<b>%1%2P pref flow from %3 to %4</b>")
                    .arg(QString::number(n), pc, from_party, _table_main_model->horizontalHeaderItem(_clicked_cells_two_axis.at(0).j)->text());
    }
  }
  else if (table_type == Table_types::CUSTOM)
  {
    const int data_row       = _clicked_cells_two_axis.at(0).i;
    const QString row_header = _custom_table_row_headers.at(data_row);
    table_title              = QString("Custom: row = %1").arg(row_header);

    const int data_col = _clicked_cells_two_axis.at(0).j;
    if (data_col == 0)
    {
      if (_table_main_data.length() > 1)
      {
        map_title = QString("Custom: %1 base").arg(row_header);
      }
      else if (_custom_rows.type == Custom_axis_type::NONE)
      {
        map_title = "Custom: Vote";
      }
      else
      {
        map_title = QString("Custom: %1 vote").arg(row_header);
      }
    }
    else
    {
      const QString col_header = _custom_table_col_headers.at(data_col - 1);
      map_title                = QString("Custom: (row, col) = (%1, %2)").arg(row_header, col_header);
    }
  }

  _label_division_table_title->setText(table_title);

  if (table_type != Table_types::NPP && table_type != Table_types::CUSTOM)
  {
    map_title = table_title;
  }

  _label_map_title->setText(map_title);
}

void Widget::_fill_in_divisions_table()
{
  const QString table_type = _get_table_type();
  const QString value_type = _get_value_type();

  const int num_rows = _table_divisions_data.length();
  const int num_cols = table_type == Table_types::NPP      ? _get_n_preferred() + 3
                       : table_type == Table_types::CUSTOM ? _table_main_data.length() + 1
                                                           : 2;

  QFont font_bold;
  QFont font_normal;
  font_bold.setBold(true);
  font_normal.setBold(false);

  bool bold_row = false;

  double min_value = 1.1e10;
  double max_value = 0.;


  QHeaderView* horizontal_header = _table_divisions->horizontalHeader();
  horizontal_header->setSectionResizeMode(QHeaderView::Fixed);

  for (int i_row = 0; i_row < num_rows; i_row++)
  {
    QStandardItem* div_item = _table_divisions_model->item(i_row, 0);
    if (div_item == nullptr)
    {
      _table_divisions_model->setItem(i_row, 0, new QStandardItem());
      div_item = _table_divisions_model->item(i_row, 0);
    }

    if (_table_divisions_data.at(i_row).division == num_rows - 1)
    {
      div_item->setText(_state_full);
      div_item->setFont(font_bold);
      bold_row = true;
    }
    else
    {
      div_item->setText(_divisions.at(_table_divisions_data.at(i_row).division));
      div_item->setFont(font_normal);
      bold_row = false;
    }

    for (int i_col = 1; i_col < num_cols; i_col++)
    {
      QStandardItem* item = _table_divisions_model->item(i_row, i_col);
      if (!item)
      {
        _table_divisions_model->setItem(i_row, i_col, new QStandardItem());
        item = _table_divisions_model->item(i_row, i_col);
      }

      QString cell_text;
      if (value_type == VALUE_VOTES)
      {
        cell_text = QString("%1").arg(_table_divisions_data.at(i_row).votes.at(i_col - 1));
      }
      if (value_type == VALUE_PERCENTAGES)
      {
        cell_text = QString("%1").arg(_table_divisions_data.at(i_row).percentage.at(i_col - 1), 0, 'f', 2);
      }
      if (value_type == VALUE_TOTAL_PERCENTAGES)
      {
        cell_text = QString("%1").arg(_table_divisions_data.at(i_row).total_percentage.at(i_col - 1), 0, 'f', 2);
      }

      item->setText(cell_text);
      item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

      const bool map_npp    = table_type == Table_types::NPP && i_col == _clicked_cells_two_axis.at(0).j;
      const bool map_custom = table_type == Table_types::CUSTOM && _custom_sort_indices_cols.at(i_col - 1) == _clicked_cells_two_axis.at(0).j;

      if (((table_type != Table_types::NPP && table_type != Table_types::CUSTOM) || map_npp || map_custom)
          && _table_divisions_data.at(i_row).division != num_rows - 1)
      {
        double val = cell_text.toDouble();
        _map_divisions_model.set_value(_table_divisions_data.at(i_row).division, val);
        min_value = qMin(min_value, val);
        max_value = qMax(max_value, val);
      }

      if (bold_row)
      {
        item->setFont(font_bold);
      }
      else
      {
        item->setFont(font_normal);
      }
    }
  }

  horizontal_header->setSectionResizeMode(QHeaderView::ResizeToContents);
  _table_divisions->resizeColumnsToContents();

  // Ensure that the spinboxes for min and max of the color
  // scale are different:
  if (value_type == VALUE_VOTES)
  {
    if (max_value - min_value < 0.5)
    {
      max_value += 1.;
    }
  }
  else
  {
    if (qRound(10. * max_value) == qRound(10. * min_value))
    {
      if (max_value > 98.)
      {
        min_value -= 1.;
      }
      else
      {
        max_value += 1.;
      }
    }
  }

  _spinbox_map_min->setMaximum(max_value);
  _spinbox_map_max->setMinimum(min_value);
  _spinbox_map_min->setValue(min_value);
  _spinbox_map_max->setValue(max_value);

  // Following is usually not needed, but _is_ needed if
  // the min/max spinboxes didn't change just now:
  _map_divisions_model.set_colors();

  if (_get_map_type() != MAP_DIVISIONS)
  {
    _update_map_booths();
  }

  _map_scale_min_default = min_value;
  _map_scale_max_default = max_value;
}

void Widget::_update_map_booths()
{
  const QString value_type = _get_value_type();
  const QString table_type = _get_table_type();

  const int num_booths = _booths.length();
  int col, base_col, row, base_row;

  if (table_type == Table_types::NPP)
  {
    base_col = 0;
    if (_clicked_cells_two_axis.length() == 0)
    {
      return;
    }

    col = _clicked_cells_two_axis.at(0).j - 1;
    row = _clicked_cells_two_axis.at(0).i;

    base_row = row;
  }
  else if (table_type == Table_types::CUSTOM)
  {
    if (_clicked_cells_two_axis.length() == 0)
    {
      return;
    }
    col = _clicked_cells_two_axis.at(0).j - 1;
    row = _clicked_cells_two_axis.at(0).i;

    base_col = col < 0 ? -1 : 0;
    base_row = row;
  }
  else
  {
    col = _clicked_cells.length() - 1;
    if (col < 0)
    {
      return;
    }

    base_col = col - 1;
    row      = _clicked_cells.at(col);
    base_row = base_col >= 0 ? _clicked_cells.at(base_col) : 0;
  }

  for (int j = 0; j < num_booths; j++)
  {
    const int votes = (table_type == Table_types::CUSTOM && col < 0)
                        ? _table_main_booth_data_row_bases.at(row).at(j)
                        : _table_main_booth_data.at(col).at(row).at(j);

    int denom;
    double value = 0.;

    if (value_type == VALUE_VOTES)
    {
      value = static_cast<double>(votes);
    }
    else if (value_type == VALUE_PERCENTAGES)
    {
      if (table_type == Table_types::CUSTOM)
      {
        denom = base_col < 0 ? _table_main_booth_data_total_base.at(j) : _table_main_booth_data_row_bases.at(base_row).at(j);
      }
      else
      {
        if (base_col < 0)
        {
          denom = _booths.at(j).formal_votes;
        }
        else
        {
          denom = _table_main_booth_data.at(base_col).at(base_row).at(j);
        }
      }
      value = denom == 0 ? 0. : 100. * votes / denom;
    }
    else if (value_type == VALUE_TOTAL_PERCENTAGES)
    {
      value = _booths.at(j).formal_votes == 0 ? 0. : 100. * votes / _booths.at(j).formal_votes;
    }

    _map_booths_model.set_value(j, value, false);
  }

  _map_booths_model.emit_all_data_changed_text();
  _map_booths_model.set_colors();
}

void Widget::_set_divisions_table()
{
  const QString table_type = _get_table_type();
  const int num_rows       = _table_divisions_data.length();
  int n                    = 0;
  int num_cols             = 2;

  if (table_type == Table_types::NPP)
  {
    n        = _get_n_preferred();
    num_cols = n + 3;
  }

  _set_divisions_table_title();

  _table_divisions_model->setRowCount(num_rows);
  _table_divisions_model->setColumnCount(num_cols);

  QStringList headers;
  headers.append("Division");

  if (table_type == Table_types::NPP)
  {
    headers.append("Base");
    for (int i = 0; i < n; i++)
    {
      headers.append(_table_main_groups_short.at(_clicked_n_parties.at(i)));
    }
    headers.append("Exh");
  }
  else if (table_type == Table_types::CUSTOM)
  {
    const int num_cols            = _table_main_model->columnCount();
    const int rows_is_none_offset = _custom_rows.type == Custom_axis_type::NONE ? 1 : 0;
    headers.append(num_cols > 2 - rows_is_none_offset ? "Base" : "Vote");
    for (int i = 2 - rows_is_none_offset; i < num_cols; ++i)
    {
      headers.append(_table_main_model->horizontalHeaderItem(i)->text());
    }
  }
  else
  {
    headers.append("Vote");
  }

  QHeaderView* horizontal_header = _table_divisions->horizontalHeader();
  horizontal_header->setSectionResizeMode(QHeaderView::Fixed);
  _table_divisions_model->setHorizontalHeaderLabels(headers);

  for (int i = 0; i < _table_divisions_model->columnCount(); i++)
  {
    if (i == 0)
    {
      _table_divisions_model->horizontalHeaderItem(i)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    else
    {
      _table_divisions_model->horizontalHeaderItem(i)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
  }

  _fill_in_divisions_table();

  _table_divisions->resizeColumnsToContents();
  horizontal_header->setSectionResizeMode(QHeaderView::ResizeToContents);
}

void Widget::_init_main_table_custom(int n_main_rows, int n_rows, int n_main_cols, int n_cols)
{
  // Setup main table data and headers
  const int num_divisions = _divisions.length();
  const int num_booths    = _booths.length();

  _clear_main_table_data();

  for (int i_booth = 0; i_booth < num_booths; ++i_booth)
  {
    _table_main_booth_data_total_base.append(0);
  }

  for (int i_div = 0; i_div < num_divisions + 1; ++i_div)
  {
    _table_main_data_total_base.votes.append(0);
  }

  _table_main_data.append(QVector<Table_main_item>());
  for (int i_row = 0; i_row < n_rows; ++i_row)
  {
    _table_main_booth_data_row_bases.append(QVector<int>());
    for (int i_booth = 0; i_booth < num_booths; ++i_booth)
    {
      _table_main_booth_data_row_bases[i_row].append(0);
    }

    // First column of _table_main_data will contain the row bases
    _table_main_data[0].append(Table_main_item());
    for (int i_div = 0; i_div <= num_divisions; ++i_div)
    {
      _table_main_data[0][i_row].votes.append(0);
      _table_main_data[0][i_row].percentages.append(0.);
    }
  }

  for (int i_col = 0; i_col < n_main_cols; ++i_col)
  {
    _table_main_booth_data.append(QVector<QVector<int>>());
    _table_main_data.append(QVector<Table_main_item>());

    for (int i_row = 0; i_row < n_rows; ++i_row)
    {
      _table_main_booth_data[i_col].append(QVector<int>());
      for (int i_booth = 0; i_booth < num_booths; ++i_booth)
      {
        _table_main_booth_data[i_col][i_row].append(0);
      }

      _table_main_data[i_col + 1].append(Table_main_item());
      for (int i_div = 0; i_div <= num_divisions; ++i_div)
      {
        _table_main_data[i_col + 1][i_row].votes.append(0);
        _table_main_data[i_col + 1][i_row].percentages.append(0.);
      }
    }
  }

  const int rows_is_none_offset = _custom_rows.type == Custom_axis_type::NONE ? 1 : 0;
  const int num_table_cols      = 2 + n_main_cols - rows_is_none_offset;
  _table_main_model->clear();
  _table_main_model->setColumnCount(num_table_cols);
  _table_main_model->setRowCount(n_rows);

  const QString base_header = n_main_cols > 0 ? "Base" : "Vote";
  const int base_table_col  = (n_main_cols == 0 && n_main_rows > 0) ? 1 : 0;
  const int rows_title_col  = n_main_cols == 0 ? 0 : 1;

  QStringList col_headers;

  QString rows_title;

  if (n_main_rows > 0)
  {
    switch (_custom_rows.type)
    {
    case Custom_axis_type::GROUPS:
    case Custom_axis_type::NPP:
      rows_title = "Group";
      break;
    case Custom_axis_type::CANDIDATES:
      rows_title = "Cand";
      break;
    case Custom_axis_type::NUMBERS:
      rows_title = "Number";
      break;
    default:
      rows_title = "Blank";
    }
  }

  if (base_table_col == 0)
  {
    col_headers.append(base_header);
  }

  if (n_main_rows > 0)
  {
    col_headers.append(rows_title);
    if (rows_title_col == 0)
    {
      col_headers.append(base_header);
    }
  }

  for (int i = 0; i < n_main_cols; ++i)
  {
    col_headers.append(_custom_table_col_headers.at(i));
  }

  // Switch off auto-resize while the column headers are added,
  // otherwise it can take several seconds to update.
  QHeaderView* horizontal_header = _table_main->horizontalHeader();
  horizontal_header->setSectionResizeMode(QHeaderView::Fixed);

  _table_main_model->setHorizontalHeaderLabels(col_headers);
  _table_main->resizeColumnsToContents();
  horizontal_header->setSectionResizeMode(QHeaderView::ResizeToContents);

  _custom_sort_indices_rows.clear();
  _custom_sort_indices_cols.clear();

  _sort_custom_rows.i = -999;
  _sort_custom_cols.i = -999;

  for (int i = 0; i < n_rows; ++i)
  {
    _custom_sort_indices_rows.append(i);
  }

  // Include an extra col for the row base
  for (int i = 0; i <= n_cols; ++i)
  {
    _custom_sort_indices_cols.append(i);
  }

  _clicked_cells_two_axis.clear();
  _clear_divisions_table();
}

void Widget::_enable_division_export_buttons_custom()
{
  const QString table_type = _get_table_type();
  if (table_type != Table_types::CUSTOM)
  {
    return;
  }
  const bool enable_any       = (_clicked_cells_two_axis.length() > 0);
  const bool enable_cross     = enable_any && (_custom_rows.type != Custom_axis_type::NONE);
  const bool enable_non_cross = enable_any && (_custom_rows.type == Custom_axis_type::NONE || _custom_cols.type != Custom_axis_type::NONE);
  _button_divisions_cross_table->setEnabled(enable_cross);
  _button_booths_cross_table->setEnabled(enable_cross);
  _button_divisions_copy->setEnabled(enable_non_cross);
  _button_divisions_export->setEnabled(enable_non_cross);
  _button_divisions_booths_export->setEnabled(enable_non_cross);
}

void Widget::_clicked_main_table(const QModelIndex& index)
{
  const int i = index.row();
  int j       = index.column();

  if (_doing_calculation)
  {
    return;
  }
  if (_table_main_model->item(i, j) == nullptr)
  {
    return;
  }
  if (_table_main_model->item(i, j)->text() == "")
  {
    return;
  }
  if (_table_main_data.length() == 0)
  {
    return;
  } // Hopefully not needed, but just in case....

  const int num_clicked_cells = _clicked_cells.length();
  const QString table_type    = _get_table_type();

  if (table_type == Table_types::NPP)
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

    const int data_col         = (j == 0) ? 0 : j - 1;
    const int clicked_group_id = _table_main_data.at(data_col).at(i).group_id;

    if (j < 2)
    {
      if (clicked_group_id == get_num_groups())
        return;

      j = 1;

      // First clear all n-party-preferred data.
      for (int s = _table_main_data.length() - 1; s > 0; --s)
      {
        _table_main_data.remove(s);
        _table_main_booth_data.remove(s);
      }

      _table_main_model->setColumnCount(2 + _clicked_n_parties.length());

      for (int s = 2; s < _table_main_model->columnCount(); s++)
      {
        for (int r = 0; r < _num_table_rows; ++r)
        {
          _table_main_model->setItem(r, s, new QStandardItem(""));
          _set_default_cell_style(r, s);
        }
      }

      // Remove cell highlights
      _clicked_cells_two_axis.clear();

      _clear_divisions_table();

      const int index_i = _clicked_n_parties.indexOf(clicked_group_id);
      if (index_i > -1)
      {
        _unhighlight_cell(i, j);
        _clicked_n_parties.remove(index_i);

        // Remove table column
        _table_main_model->takeColumn(index_i + 2);
      }
      else
      {
        _clicked_n_parties.append(clicked_group_id);
        _highlight_cell_n_party_preferred(i, 1);

        // Add a table column
        const int num_cols = _table_main_model->columnCount();
        _table_main_model->setColumnCount(num_cols + 1);
        _table_main_model->setHorizontalHeaderItem(num_cols, new QStandardItem(_table_main_groups_short.at(clicked_group_id)));
        _table_main_model->horizontalHeaderItem(num_cols)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      }

      _button_n_party_preferred_calculate->setEnabled(_clicked_n_parties.length() > 0);
    }
    else
    {
      if (_clicked_cells_two_axis.length() == 1 && _clicked_cells_two_axis.at(0).i == clicked_group_id && _clicked_cells_two_axis.at(0).j == j)
      {
        // Unhighlight
        _clicked_cells_two_axis.clear();

        _set_default_cell_style(i, j);
      }
      else
      {
        if (_clicked_cells_two_axis.length() == 1)
        {
          // Unhighlight the previously clicked cell.
          int prev_i       = _clicked_cells_two_axis.at(0).i;
          const int prev_j = _clicked_cells_two_axis.at(0).j;
          prev_i           = _table_main_data.at(0).at(prev_i).sorted_idx;
          _set_default_cell_style(prev_i, prev_j);
        }

        _set_clicked_cell_n_party(clicked_group_id, j);
        for (int s = 2; s < _table_main_model->columnCount(); ++s)
        {
          for (int r = 0; r < _num_table_rows; ++r)
          {
            _fade_cell(r, s);
          }
        }
        _highlight_cell(i, j);
      }

      // Divisions table

      if (_clicked_cells_two_axis.length() == 0)
      {
        _clear_divisions_table();
        return;
      }
      else
      {
        _table_divisions_data.clear();
        const int num_divisions = _table_main_data.at(data_col).at(i).votes.length();
        const int n             = _get_n_preferred();

        for (int r = 0; r < num_divisions; r++)
        {
          _table_divisions_data.append(Table_divisions_item());
          _table_divisions_data[r].division      = r;
          const int total_percentage_denominator = _division_formal_votes.at(r);
          const int percentage_denominator       = qMax(1, _table_main_data.at(0).at(i).votes.at(r));

          _table_divisions_data[r].votes.append(percentage_denominator);

          const double base = 100. * static_cast<double>(percentage_denominator) / total_percentage_denominator;

          _table_divisions_data[r].percentage.append(base);
          _table_divisions_data[r].total_percentage.append(base);

          for (int s = 0; s <= n; s++)
          {
            const int votes = _table_main_data.at(s + 1).at(i).votes.at(r);
            _table_divisions_data[r].votes.append(votes);
            _table_divisions_data[r].percentage.append(100. * static_cast<double>(votes) / percentage_denominator);
            _table_divisions_data[r].total_percentage.append(100. * static_cast<double>(votes) / total_percentage_denominator);
          }
        }

        _sort_divisions_table_data();
        _set_divisions_table();

        _button_divisions_copy->setEnabled(true);
        _button_divisions_export->setEnabled(true);
        _button_divisions_booths_export->setEnabled(true);
      }
    }
  }
  else if (table_type == Table_types::CUSTOM)
  {
    const int num_data_cols       = _table_main_data.length();
    const bool single_column      = num_data_cols == 1;
    const bool rows_is_none       = (_custom_rows.type == Custom_axis_type::NONE);
    const int rows_is_none_offset = rows_is_none ? 1 : 0;
    const int row_header_col      = rows_is_none ? -1 : single_column ? 0
                                                                      : 1;
    if (j == row_header_col)
    {
      _sort_main_table_cols_custom(i);
      return;
    }

    const int clicked_i = _custom_sort_indices_rows.at(i);
    const int clicked_j = _custom_sort_indices_cols.at(qMax(0, j - 1 + rows_is_none_offset));

    _clear_divisions_table();

    if (_clicked_cells_two_axis.length() == 1)
    {
      const int old_i       = _clicked_cells_two_axis.at(0).i;
      const int old_j       = _clicked_cells_two_axis.at(0).j;
      const int old_table_i = _custom_sort_indices_rows.indexOf(old_i);
      const int old_table_j = (!single_column && old_j == 0) ? 0 : _custom_sort_indices_cols.indexOf(old_j) + 1 - rows_is_none_offset;

      if (old_table_i >= 0 && old_table_j >= 0)
      {
        _unhighlight_cell(old_table_i, old_table_j);
      }

      _clicked_cells_two_axis.clear();

      if (old_i == clicked_i && old_j == clicked_j)
      {
        return;
      }
    }

    _clicked_cells_two_axis.append({clicked_i, clicked_j});
    _highlight_cell(i, j);

    // Divisions table

    if (_clicked_cells_two_axis.length() == 0)
    {
      return;
    }

    _table_divisions_data.clear();
    const int num_divisions = _divisions.length();

    for (int i_div = 0; i_div <= num_divisions; ++i_div)
    {
      _table_divisions_data.append(Table_divisions_item());
      _table_divisions_data[i_div].division  = i_div;
      const int total_percentage_denominator = _division_formal_votes.at(i_div);
      const int percentage_denominator       = qMax(1, _table_main_data.at(0).at(clicked_i).votes.at(i_div));

      _table_divisions_data[i_div].votes.append(percentage_denominator);
      _table_divisions_data[i_div].percentage.append(100. * static_cast<double>(percentage_denominator) / _table_main_data_total_base.votes.at(i_div));
      _table_divisions_data[i_div].total_percentage.append(100. * static_cast<double>(percentage_denominator) / total_percentage_denominator);

      for (int i_col = 1; i_col < num_data_cols; ++i_col)
      {
        const int i_data_col = _custom_sort_indices_cols.at(i_col);
        const int votes      = _table_main_data.at(i_data_col).at(clicked_i).votes.at(i_div);
        _table_divisions_data[i_div].votes.append(votes);
        _table_divisions_data[i_div].percentage.append(100. * static_cast<double>(votes) / percentage_denominator);
        _table_divisions_data[i_div].total_percentage.append(100. * static_cast<double>(votes) / total_percentage_denominator);
      }
    }

    _sort_divisions_table_data();
    _set_divisions_table();
    _enable_division_export_buttons_custom();
  }
  else
  {
    // Table type not n-party-preferred or custom.

    const int clicked_group_id = _table_main_data.at(j).at(i).group_id;
    if (num_clicked_cells > j && _clicked_cells.at(j) == clicked_group_id)
    {
      return;
    }

    const bool exhaust = _table_main_data.at(j).at(i).group_id >= get_num_groups();

    if (j == 0 && exhaust && (table_type == Table_types::STEP_FORWARD || table_type == Table_types::LATER_PREFS))
    {
      return;
    }

    // - Leave content in columns to the left of j unchanged.
    // - Change the highlight of column j to the clicked cell.
    // - Remove content to the right of j.

    for (int s = j; s < _clicked_cells.length(); s++)
    {
      // Unhighlight previously-clicked cells from this column
      // and right.
      _set_default_cell_style(_table_main_data.at(s).at(_clicked_cells.at(s)).sorted_idx, s);
    }

    for (int r = 0; r < _num_table_rows; ++r)
    {
      _fade_cell(r, j);
    }
    _highlight_cell(i, j);

    for (int s = _table_main_data.length() - 1; s > j; s--)
    {
      _table_main_data.remove(s);
      _table_main_booth_data.remove(s);
      for (int r = 0; r < _num_table_rows; ++r)
      {
        _table_main_model->setItem(r, s, new QStandardItem(""));
        _set_default_cell_style(r, s);
      }
    }

    for (int s = num_clicked_cells - 1; s >= j; s--)
    {
      _clicked_cells.remove(s);
    }

    _clicked_cells.append(clicked_group_id);


    if (((table_type == Table_types::STEP_FORWARD)  && (j < get_num_groups() - 1)           && !exhaust)  ||
        ((table_type == Table_types::FIRST_N_PREFS) && (j < _get_n_first_prefs() - 1))                    ||
        ((table_type == Table_types::LATER_PREFS)   && (j < _get_later_prefs_n_up_to() - 1)
                                                    && !((j < _get_later_prefs_n_fixed())   &&  exhaust)) ||
        ((table_type == Table_types::PREF_SOURCES)  && (j == 0)))
    {
      _add_column_to_main_table();
    }

    // Divisions table

    _table_divisions_data.clear();
    const int num_divisions = _table_main_data.at(j).at(i).votes.length();

    for (int r = 0; r < num_divisions; r++)
    {
      _table_divisions_data.append(Table_divisions_item());
      _table_divisions_data[r].division      = r;
      const int votes                        = _table_main_data.at(j).at(i).votes.at(r);
      const int total_percentage_denominator = _division_formal_votes.at(r);
      int percentage_denominator;

      if (j == 0)
      {
        percentage_denominator = total_percentage_denominator;
      }
      else
      {
        const int idx          = _table_main_data.at(j - 1).at(_clicked_cells.at(j - 1)).sorted_idx;
        percentage_denominator = qMax(1, _table_main_data.at(j - 1).at(idx).votes.at(r));
      }

      _table_divisions_data[r].votes.append(votes);
      _table_divisions_data[r].percentage.append(100. * static_cast<double>(votes) / percentage_denominator);
      _table_divisions_data[r].total_percentage.append(100. * static_cast<double>(votes) / total_percentage_denominator);
    }

    _sort_divisions_table_data();
    _set_divisions_table();
  }
}

void Widget::_clear_divisions_table()
{
  _table_divisions_data.clear();
  _table_divisions_model->clear();
  _label_division_table_title->setText("<b>No selection</b>");

  _map_divisions_model.clear_values();
  _map_booths_model.clear_values();
  _label_map_title->setText("<b>No selection</b>");

  _button_divisions_copy->setEnabled(false);
  _button_divisions_export->setEnabled(false);
  _button_divisions_booths_export->setEnabled(false);
  _button_divisions_cross_table->setEnabled(false);
  _button_booths_cross_table->setEnabled(false);
  _reset_divisions_sort();
}

void Widget::_set_clicked_cell_n_party(int i, int j)
{
  if (_clicked_cells_two_axis.length() == 0)
  {
    Two_axis_click_cell ij;
    ij.i = i;
    ij.j = j;
    _clicked_cells_two_axis.append(ij);
  }
  else
  {
    _clicked_cells_two_axis[0].i = i;
    _clicked_cells_two_axis[0].j = j;
  }
}

void Widget::_clear_main_table_data()
{
  _table_main_data.clear();
  _table_main_booth_data.clear();
  _table_main_booth_data_row_bases.clear();
  _table_main_booth_data_total_base.clear();
  _table_main_data_total_base = Table_main_item{};
  _custom_sort_indices_rows.clear();
  _custom_sort_indices_cols.clear();
}

// I have for now commented out the lines that would
// change the text color; these are, remarkably enough,
// a bottleneck in large (below-the-line) tables.

void Widget::_set_default_cell_style(int i, int j)
{
  if (_table_main_model->item(i, j) == nullptr)
  {
    return;
  }
  _table_main_model->item(i, j)->setBackground(QColor(255, 255, 255, 0));
  //table_main_model->item(i, j)->setForeground(get_focused_text_color());
}

void Widget::_fade_cell(int i, int j)
{
  if (_table_main_model->item(i, j) == nullptr)
  {
    return;
  }
  //table_main_model->item(i, j)->setBackground(QColor(255, 255, 255, 0));
  //table_main_model->item(i, j)->setForeground(get_unfocused_text_color());
}

void Widget::_highlight_cell(int i, int j)
{
  if (_table_main_model->item(i, j) == nullptr)
  {
    return;
  }
  _table_main_model->item(i, j)->setBackground(_get_highlight_color());
  //table_main_model->item(i, j)->setForeground(get_focused_text_color());
}

void Widget::_highlight_cell_n_party_preferred(int i, int j)
{
  if (_table_main_model->item(i, j) == nullptr)
  {
    return;
  }
  _table_main_model->item(i, j)->setBackground(_get_n_party_preferred_color());
  //table_main_model->item(i, j)->setForeground(get_focused_text_color());
}

void Widget::_unhighlight_cell(int i, int j)
{
  if (_table_main_model->item(i, j) == nullptr)
  {
    return;
  }
  _table_main_model->item(i, j)->setBackground(QColor(255, 255, 255, 0));
}

QString Widget::_get_export_line(QStandardItemModel* model, int i, const QString& separator)
{
  QString text("");
  QString sep("");

  if (i == -1)
  {
    // Headers
    for (int j = 0; j < model->columnCount(); j++)
    {
      text = QString("%1%2%3").arg(text, sep, model->horizontalHeaderItem(j)->text());
      if (j == 0)
      {
        sep = separator;
      }
    }

    return text;
  }
  else
  {
    for (int j = 0; j < model->columnCount(); j++)
    {
      text = QString("%1%2%3").arg(text, sep, model->item(i, j)->text());
      if (j == 0)
      {
        sep = separator;
      }
    }

    return text;
  }
}

void Widget::_copy_model(QStandardItemModel* model, QString title)
{
  QString text("");
  QString newline("");

  if (title != "")
  {
    text    = title;
    newline = "\n";
  }

  const QString sep("\t");
  for (int i = -1; i < model->rowCount(); i++)
  {
    text = QString("%1%2%3").arg(text, newline, _get_export_line(model, i, sep));
    if (i == -1)
    {
      newline = "\n";
    }
  }

  QApplication::clipboard()->setText(text);
}

QString Widget::_get_output_path(const QString& file_type)
{
  // Read the most recent directory a CSV file was saved to.
  const QString last_path_file_name = QString("%1/last_%2_dir.txt")
                                        .arg(QCoreApplication::applicationDirPath(), file_type);

  QFileInfo check_exists(last_path_file_name);

  QString latest_out_path(QCoreApplication::applicationDirPath());

  if (check_exists.exists())
  {
    QFile last_path_file(last_path_file_name);

    if (last_path_file.open(QIODevice::ReadOnly))
    {
      QTextStream in(&last_path_file);
      const QString last_path = in.readLine();

      if (QDir(last_path).exists())
      {
        latest_out_path = last_path;
      }

      last_path_file.close();
    }
  }

  return latest_out_path;
}

QString Widget::_get_map_type()
{
  return _combo_map_type->currentData().toString();
}

int Widget::_get_map_booth_threshold()
{
  return _spinbox_map_booth_threshold->value();
}

void Widget::_update_output_path(const QString& file_name, const QString& file_type)
{
  const QString last_path_file_name = QString("%1/last_%2_dir.txt")
                                        .arg(QCoreApplication::applicationDirPath(), file_type);

  // Update the most recent CSV path:
  QFileInfo info(file_name);
  _latest_out_path = info.absolutePath();

  QFile last_path_file(last_path_file_name);

  if (last_path_file.open(QIODevice::WriteOnly))
  {
    QTextStream out(&last_path_file);
    out << _latest_out_path << endl;
    last_path_file.close();
  }
}

void Widget::_export_model(QStandardItemModel* model, QString title)
{
  const QString latest_out_path = _get_output_path("csv");

  const QString out_file_name = QFileDialog::getSaveFileName(this,
                                                             "Save CSV",
                                                             latest_out_path,
                                                             "*.csv");

  if (out_file_name == "")
  {
    return;
  }

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
      out << _get_export_line(model, i, ",") << endl;
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

  _update_output_path(out_file_name, "csv");
}

void Widget::_copy_main_table()
{
  _copy_model(_table_main_model);
}

void Widget::_export_main_table()
{
  _export_model(_table_main_model);
}

QString Widget::_get_export_divisions_table_title()
{
  QString title("");
  if (_clicked_cells.length() > 0) // Should be!
  {
    _set_title_from_divisions_table();
    title = _divisions_cross_table_title;
    title.replace("Column", _get_short_group(_clicked_cells.at(_clicked_cells.length() - 1)));
  }
  return title;
}

void Widget::_copy_divisions_table()
{
  _copy_model(_table_divisions_model, _get_export_divisions_table_title());
}

void Widget::_export_divisions_table()
{
  _export_model(_table_divisions_model, _get_export_divisions_table_title());
}

void Widget::_reset_map_scale()
{
  _spinbox_map_min->setValue(_map_scale_min_default);
  _spinbox_map_max->setValue(_map_scale_max_default);
}

void Widget::_zoom_map_to_point(double longitude, double latitude, double zoom)
{
  _qml_map->setProperty("propLongitude", longitude);
  _qml_map->setProperty("propLatitude", latitude);
  _qml_map->setProperty("propZoomLevel", zoom);
  _qml_map_container->update_coords();
}

void Widget::_zoom_to_state()
{
  if (!_opened_database || !_qml_map)
  {
    return;
  }

  _offset_set_map_center = !_offset_set_map_center;
  double lon             = 133.;
  double lat             = -30.;
  double zoom            = 4.;

  if (_state_short == "QLD")
  {
    lon  = 145.;
    lat  = -20.3;
    zoom = 5.08;
  }
  else if (_state_short == "NSW")
  {
    lon  = 147.3;
    lat  = -32.8;
    zoom = 5.8;
  }
  else if (_state_short == "VIC")
  {
    lon  = 145.5;
    lat  = -37.;
    zoom = 6.28;
  }
  else if (_state_short == "TAS")
  {
    lon  = 146.5;
    lat  = -41.9;
    zoom = 6.76;
  }
  else if (_state_short == "SA")
  {
    lon  = 135.;
    lat  = -33.;
    zoom = 5.44;
  }
  else if (_state_short == "WA")
  {
    lon  = 124.;
    lat  = -26.2;
    zoom = 4.72;
  }
  else if (_state_short == "NT")
  {
    lon  = 134.;
    lat  = -19.;
    zoom = 5.32;
  }
  else if (_state_short == "ACT")
  {
    lon  = 149.;
    lat  = -35.55;
    zoom = 9.16;
  }

  if (_offset_set_map_center)
  {
    lon += 1e-6;
    lat += 1e-6;
    zoom += 1e-6;
  }

  _zoom_map_to_point(lon, lat, zoom);
}

void Widget::_zoom_to_capital()
{
  if (!_opened_database || !_qml_map)
  {
    return;
  }

  _offset_set_map_center = !_offset_set_map_center;
  double lon             = 133.;
  double lat             = -30.;
  double zoom            = 4.;

  if (_state_short == "QLD")
  {
    lon  = 152.5;
    lat  = -27.3;
    zoom = 8.2;
  }
  else if (_state_short == "NSW")
  {
    lon  = 150.9;
    lat  = -33.6;
    zoom = 8.32;
  }
  else if (_state_short == "VIC")
  {
    lon  = 145.0;
    lat  = -37.8;
    zoom = 9.28;
  }
  else if (_state_short == "TAS")
  {
    lon  = 147.2;
    lat  = -42.5;
    zoom = 7.96;
  }
  else if (_state_short == "SA")
  {
    lon  = 138.65;
    lat  = -34.8;
    zoom = 9.04;
  }
  else if (_state_short == "WA")
  {
    lon  = 116.4;
    lat  = -32.0;
    zoom = 9.04;
  }
  else if (_state_short == "NT")
  {
    lon  = 131.;
    lat  = -12.5;
    zoom = 9.64;
  }
  else if (_state_short == "ACT")
  {
    lon  = 149.;
    lat  = -35.55;
    zoom = 9.16;
  }

  if (_offset_set_map_center)
  {
    lon += 1e-6;
    lat += 1e-6;
    zoom += 1e-6;
  }

  _zoom_map_to_point(lon, lat, zoom);
}

double Widget::_longitude_to_x(double longitude, double center_longitude, double zoom, int size)
{
  const double tile_x = pow(2., zoom) * (center_longitude + 180.) / 360.;
  const double tx     = pow(2., zoom) * (longitude + 180.) / 360.;
  return 256. * (tx - tile_x) + size / 2.;
}

double Widget::_latitude_to_y(double latitude, double center_latitude, double zoom, int size)
{
  const double tile_y = pow(2., zoom - 1.) * (1. - log(tan(center_latitude / rad2deg) + 1. / cos(center_latitude / rad2deg)) / M_PI);
  const double n      = asinh(tan(latitude / rad2deg));
  const double ty     = pow(2., zoom) * (M_PI - n) / (2. * M_PI);
  return 256. * (ty - tile_y) + size / 2.;
}

double Widget::_x_to_longitude(double x, double center_longitude, double zoom, int size)
{
  const double tile_x = pow(2., zoom) * (center_longitude + 180.) / 360.;
  const double tx     = tile_x + (x - size / 2) / 256.;
  return 360. * tx / pow(2.0, zoom) - 180.;
}

double Widget::_y_to_latitude(double y, double center_latitude, double zoom, int size)
{
  const double tile_y = pow(2., zoom - 1.) * (1. - log(tan(center_latitude / rad2deg) + 1. / cos(center_latitude / rad2deg)) / M_PI);
  const double ty     = tile_y + (y - size / 2) / 256.;
  const double n      = M_PI - 2. * M_PI * ty / pow(2., zoom);
  return rad2deg * atan(sinh(n));
}

void Widget::_zoom_to_division()
{
  if (!_opened_database || !_qml_map)
  {
    return;
  }
  const int div = _get_current_division();
  if (div == _divisions.length())
  {
    return;
  }
  _zoom_to_division(div);
}

void Widget::_zoom_to_division(int div)
{
  _offset_set_map_center = !_offset_set_map_center;

  const int size_x  = _qml_map_container->width();
  const int size_y  = _qml_map_container->height();
  const int size    = qMin(size_x, size_y);
  const double log2 = log(2.);

  // Latitude:
  const double min_lat = _division_bboxes.at(div).min_latitude;
  const double max_lat = _division_bboxes.at(div).max_latitude;

  double lat  = 0.5 * (min_lat + max_lat);
  double zoom = 4.;

  double y1_target = 40.;

  // Adjust center:
  double y1 = _latitude_to_y(max_lat, lat, zoom, size);
  double y2 = _latitude_to_y(min_lat, lat, zoom, size);
  lat       = _y_to_latitude(0.5 * (y1 + y2), lat, zoom, size);

  // Adjust zoom:
  y1 = _latitude_to_y(max_lat, lat, zoom, size);
  zoom -= log((y1 - size / 2.) / (y1_target - size / 2.)) / log2;

  double ideal_zoom_lat = zoom;

  // Longitude:
  const double min_lon = _division_bboxes.at(div).min_longitude;
  const double max_lon = _division_bboxes.at(div).max_longitude;
  double lon           = 0.5 * (min_lon + max_lon);
  zoom                 = 4.;
  double x1_target     = 40.;

  // Adjust center:
  double x1 = _longitude_to_x(min_lon, lon, zoom, size);
  double x2 = _longitude_to_x(max_lon, lon, zoom, size);
  lon       = _x_to_longitude(0.5 * (x1 + x2), lon, zoom, size);

  // Adjust zoom:
  x1 = _longitude_to_x(min_lon, lon, zoom, size);
  zoom -= log((x1 - size / 2.) / (x1_target - size / 2.)) / log2;

  double ideal_zoom_lon = zoom;

  zoom = qMin(ideal_zoom_lon, ideal_zoom_lat);

  // At least on my computer, the zoom increments by +/- 0.12:
  const int n = static_cast<int>((zoom - 1.) / 0.12) + 1;
  zoom        = 1. + 0.12 * n;

  if (_offset_set_map_center)
  {
    lon += 1e-6;
    lat += 1e-6;
    zoom += 1e-6;
  }

  _zoom_map_to_point(lon, lat, zoom);
}

QPixmap Widget::_get_pixmap_for_map()
{
  const QPoint main_window_corner = this->mapToGlobal(QPoint(0, 0));
  const int x0                    = main_window_corner.x();
  const int y0                    = main_window_corner.y();

  const QPoint top_left = _label_map_title->mapToGlobal(QPoint(0, 0));
  const int x           = top_left.x() - x0;
  const int y           = top_left.y() - y0;
  const int width       = _qml_map_container->width();

  const QPoint bottom = _spinbox_map_max->mapToGlobal(QPoint(0, 0));
  const int height    = bottom.y() + _spinbox_map_max->height() - y - y0;

  QScreen* screen = QApplication::primaryScreen();
  if (!screen)
  {
    return QPixmap();
  }

  _label_reset_map_scale->hide();
  _label_map_help->hide();
  QCoreApplication::sendPostedEvents();

  QPixmap map = screen->grabWindow(0, x + x0, y + y0, width, height);

  _label_reset_map_scale->show();
  _label_map_help->show();

  return map;
}

void Widget::_copy_map()
{
  _label_reset_map_scale->hide();
  QTimer::singleShot(250, this, SLOT(_delayed_copy_map()));
}

void Widget::_export_map()
{
  _label_reset_map_scale->hide();
  const QString latest_out_path = _get_output_path("png");

  const QString out_file_name = QFileDialog::getSaveFileName(
    this, "Save PNG", latest_out_path, "*.png");

  if (out_file_name == "")
  {
    return;
  }

  _update_output_path(out_file_name, "png");

  QTimer::singleShot(250, this, [=]() -> void
                     { _delayed_export_map(out_file_name); });
}

void Widget::_delayed_copy_map()
{
  QApplication::clipboard()->setPixmap(_get_pixmap_for_map());
  _label_reset_map_scale->show();
}

void Widget::_delayed_export_map(const QString& file_name)
{
  _get_pixmap_for_map().save(file_name);
  _label_reset_map_scale->show();
}

void Widget::_export_booths_table()
{
  const QString table_type = _get_table_type();
  if (table_type != Table_types::NPP && table_type != Table_types::CUSTOM)
  {
    return;
  }
  if (_clicked_cells_two_axis.length() == 0)
  {
    return;
  }

  QString latest_out_path = _get_output_path("csv");

  _booths_output_file = QFileDialog::getSaveFileName(
    this, "Save CSV", latest_out_path, "*.csv");

  if (_booths_output_file == "")
  {
    return;
  }

  QFile out_file(_booths_output_file);
  const QString value_type = _get_value_type();

  if (out_file.open(QIODevice::WriteOnly))
  {
    _timer.restart();
    QTextStream out(&out_file);

    const int num_booths = _booths.length();
    const int from_row   = _clicked_cells_two_axis.at(0).i;

    QString title = _label_division_table_title->text();
    title.replace("<b>", "");
    title.replace("</b>", "");
    title.replace("&nbsp;", " ");
    out << title << endl;

    QString header("Division,Booth,Longitude,Latitude,Total");

    if (table_type == Table_types::NPP)
    {
      header += ",Base";
      const int n = _table_main_booth_data.length() - 2;
      for (int i = 0; i < n; i++)
      {
        header = QString("%1,%2").arg(header, _table_main_groups_short.at(_clicked_n_parties.at(i)));
      }
      header = QString("%1,Exh").arg(header);
      out << header << endl;

      for (int i = 0; i < num_booths; i++)
      {
        QString line = QString("%1,\"%2\",%3,%4,%5")
                         .arg(_booths.at(i).division, _booths.at(i).booth, QString::number(_booths.at(i).longitude), QString::number(_booths.at(i).latitude),
                            QString::number(_booths.at(i).formal_votes));

        const int base = _table_main_booth_data.at(0).at(from_row).at(i);

        if (value_type == VALUE_VOTES)
        {
          line = QString("%1,%2").arg(line, QString::number(base));
        }
        else
        {
          if (_booths.at(i).formal_votes == 0)
          {
            line = QString("%1,0").arg(line);
          }
          else
          {
            line = QString("%1,%2").arg(line, QString::number(100. * base / _booths.at(i).formal_votes, 'f', 2));
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
            const int numer = _table_main_booth_data.at(j + 1).at(from_row).at(i);
            if (value_type == VALUE_VOTES)
            {
              line = QString("%1,%2").arg(line, QString::number(numer));
            }
            else if (value_type == VALUE_PERCENTAGES)
            {
              line = QString("%1,%2").arg(line, QString::number(100. * numer / base, 'f', 2));
            }
            else if (value_type == VALUE_TOTAL_PERCENTAGES)
            {
              line = QString("%1,%2").arg(line, QString::number(100. * numer / _booths.at(i).formal_votes, 'f', 2));
            }
          }
        }

        out << line << endl;
      }
    }
    else if (table_type == Table_types::CUSTOM)
    {
      header += ",Filtered base";
      if (_custom_cols.type == Custom_axis_type::NONE)
      {
        header += ",Vote";
      }
      else
      {
        header += ",Base";
        for (QString& col_header : _custom_table_col_headers)
        {
          header += "," + col_header;
        }
      }
      out << header << endl;

      for (int i = 0; i < num_booths; ++i)
      {
        QString line = QString("%1,\"%2\",%3,%4,%5")
                         .arg(_booths.at(i).division, _booths.at(i).booth, QString::number(_booths.at(i).longitude), QString::number(_booths.at(i).latitude),
                           QString::number(_booths.at(i).formal_votes));

        const int total    = _booths.at(i).formal_votes;
        const int filtered = _table_main_booth_data_total_base.at(i);
        const int base     = _table_main_booth_data_row_bases.at(from_row).at(i);
        const int num_cols = _table_main_booth_data.length();

        if (value_type == VALUE_VOTES)
        {
          line = QString("%1,%2,%3")
                   .arg(line, QString::number(filtered), QString::number(base));
        }
        else if (value_type == VALUE_PERCENTAGES)
        {
          line = QString("%1,%2,%3")
                   .arg(line, QString::number(100. * filtered / total, 'f', 2), QString::number(100. * base / filtered, 'f', 2));
        }
        else if (value_type == VALUE_TOTAL_PERCENTAGES)
        {
          line = QString("%1,%2,%3")
                   .arg(line, QString::number(100. * filtered / total, 'f', 2), QString::number(100. * base / total, 'f', 2));
        }

        for (int j = 0; j < num_cols; ++j)
        {
          if (base == 0)
          {
            line = QString("%1,0").arg(line);
            continue;
          }
          const int votes = _table_main_booth_data.at(j).at(from_row).at(i);
          if (value_type == VALUE_VOTES)
          {
            line = QString("%1,%2")
                     .arg(line, QString::number(votes));
          }
          else if (value_type == VALUE_PERCENTAGES)
          {
            line = QString("%1,%2")
                     .arg(line, QString::number(100. * votes / base, 'f', 2));
          }
          else if (value_type == VALUE_TOTAL_PERCENTAGES)
          {
            line = QString("%1,%2")
                     .arg(line, QString::number(100. * votes / total, 'f', 2));
          }
        }

        out << line << endl;
      }
    }
    out_file.close();
  }
  else
  {
    QMessageBox msg_box;
    msg_box.setText("Error: couldn't open file.");
    msg_box.exec();
  }

  _label_progress->setText("Export done");
  _unlock_main_interface();

  _update_output_path(_booths_output_file, "csv");
}

void Widget::_show_abbreviations()
{
  Table_window* w = new Table_window(Table_tag_party_abbrevs{}, _table_main_groups_short, _table_main_groups, this);

  w->setMinimumSize(QSize(200, 200));
  w->resize(500, 500);
  w->setWindowTitle("Abbreviations");
  w->show();
}

void Widget::_show_help()
{
  QWidget* help = new QWidget(this, Qt::Window);

  help->setWindowTitle("Help");

  QLabel* label_help = new QLabel();
  const QString custom_doc = QDir(QCoreApplication::applicationDirPath()).filePath("custom_queries.html");
  const QString custom_href = QUrl::fromLocalFile(custom_doc).toString();
  label_help->setText("Senate preference explorer, written by David Barry, 2019.<br>Version 2, 2025-07-22."
                      "<br><br>Documentation for the custom queries is available <a href=\""
                      + custom_href + "\">here</a>."
                      "<br><br>Otherwise, such documentation as there is, as well as links to source code, will be at <a href=\"https://pappubahry.com/pseph/senate_pref/\">"
                      "https://pappubahry.com/pseph/senate_pref/</a>.  You'll need to download specially made SQLite files, which hold the preference "
                      "data in the format expected by this program.  You can then open these by clicking on the 'Load preferences' button."
                      "<br><br>This software was made with Qt Creator, using Qt 6.9: <a href=\"https://www.qt.io/\">https://www.qt.io/</a>; Qt components"
                      " are licensed under (L)GPL.  My own code is public domain.");

  label_help->setWordWrap(true);
  label_help->setOpenExternalLinks(true);
  QGridLayout* help_layout = new QGridLayout();
  help_layout->addWidget(label_help);
  help->setLayout(help_layout);

  help->show();
}

void Widget::_show_map_help()
{
  QWidget* map_help = new QWidget(this, Qt::Window);

  map_help->setWindowTitle("Help");

  QLabel* label_help = new QLabel();
  label_help->setText("At the time of writing (July 2025), the default OpenStreetMap raster tiles are sourced from Thunderforest, which adds an "
                      "\"API Key Required\" watermark.  If you have an API key, or wish to use a different OSM tile server, then you should modify "
                      "the map.ini file in the installation directory accordingly.");

  label_help->setWordWrap(true);
  QGridLayout* help_layout = new QGridLayout();
  help_layout->addWidget(label_help);
  map_help->setLayout(help_layout);

  map_help->show();
}

QString Widget::_get_table_type()
{
  return _combo_table_type->currentData().toString();
}

QString Widget::_get_value_type()
{
  return _combo_value_type->currentData().toString();
}

QString Widget::get_abtl()
{
  return _combo_abtl->currentData().toString();
}

int Widget::get_group_from_short(const QString& group) const
{
  if (_group_from_short.contains(group))
  {
    return _group_from_short.value(group);
  }
  return -1;
}

int Widget::get_cand_from_short(const QString& cand) const
{
  if (_cand_from_short.contains(cand))
  {
    return _cand_from_short.value(cand);
  }
  return -1;
}

int Widget::_get_n_preferred()
{
  return _clicked_n_parties.length();
}

int Widget::_get_n_first_prefs()
{
  return _spinbox_first_n_prefs->value();
}

int Widget::_get_later_prefs_n_fixed()
{
  return _spinbox_later_prefs_fixed->value();
}

int Widget::_get_later_prefs_n_up_to()
{
  return _spinbox_later_prefs_up_to->value();
}

int Widget::_get_pref_sources_min()
{
  return _spinbox_pref_sources_min->value();
}

int Widget::_get_pref_sources_max()
{
  return _spinbox_pref_sources_max->value();
}

int Widget::_get_current_division()
{
  return _combo_division->currentIndex();
}

QString Widget::_get_short_group(int i)
{
  if (i >= get_num_groups())
  {
    return _get_table_type() == Table_types::NPP ? "Total" : "Exh";
  }

  return _table_main_groups_short.at(i);
}

int Widget::get_num_groups()
{
  return (get_abtl() == "atl") ? _num_groups : _num_cands;
}

QColor Widget::_get_highlight_color()
{
  return QColor(224, 176, 176, 255);
}

QColor Widget::_get_n_party_preferred_color()
{
  return QColor(60, 228, 228, 255);
}

QColor Widget::_get_unfocused_text_color()
{
  return QColor(128, 128, 128, 255);
}

QColor Widget::_get_focused_text_color()
{
  return QColor(0, 0, 0, 255);
}
