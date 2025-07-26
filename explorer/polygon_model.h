#ifndef POLYGON_MODEL_H
#define POLYGON_MODEL_H

// In previous versions, Polygon_item was a simple struct, and it was a
// QAbstractListModel that was sent to the QML.  In Qt6, this setup had much
// degraded performance when loading the polygon layer (~3 seconds during which
// the interface is frozen for a large state), seemingly caused by some
// combination of
//    - slow QML conversions of QGeoCoordinates to QVariants;
//    - very very inefficient handling of long coordinate paths after calling
//      endResetModel().
//
// o4-mini-high instead suggested the structure below, with Polygon_item being
// a subclass of QObject with a QVariantList of coordinates exposed to QML via
// a Q_PROPERTY.  This means that the conversion to QVariant is done in the
// C++, and the full list is read by the QML at once, without whatever
// inefficiencies endResetModel() was triggering.

#include <QObject>
#include <QVariantList>
#include <QColor>
#include <QGeoCoordinate>
#include <QLabel>

class Polygon_item : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QVariantList path  READ path  NOTIFY pathChanged)
  Q_PROPERTY(QColor       color READ color NOTIFY colorChanged)

public:
  explicit Polygon_item(QObject* parent = nullptr)
    : QObject(parent)
  {}

  QVariantList path()  const { return _path; }
  QColor       color() const { return _color; }

  void set_coordinates(const QList<QGeoCoordinate>& coords)
  {
    QVariantList list;
    list.reserve(coords.size());
    for (const QGeoCoordinate& c : coords)
    {
      list.append(QVariant::fromValue(c));
    }
    _path = std::move(list);
    emit pathChanged();
  }

  void set_color(const QColor& c)
  {
    if (c == _color) { return; }
    _color = c;
    emit colorChanged();
  }

  void set_opacity(double a)
  {
    if (_color.alphaF() != a)
    {
      _color.setAlphaF(a);
      emit colorChanged();
    }
  }

  void set_value(double v) { _value = v; }
  double get_value() const { return _value; }


  void set_division_id(int id) { _division_id = id; }
  int get_division_id() const { return _division_id; }

  void set_division_name(const QString& name) { _division_name = name; }
  QString get_division_name() const { return _division_name; }

signals:
  void pathChanged();
  void colorChanged();

private:
  QVariantList _path = QVariantList{};
  QColor _color = Qt::blue;
  QString _division_name;
  int _division_id;
  double _value;
};

class Polygon_model : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QVariantList polygons READ polygons NOTIFY polygonsChanged)
public:
  explicit Polygon_model(QObject* parent = nullptr)
    : QObject(parent)
  {
  }

  QVariantList polygons() const
  {
    QVariantList list;
    list.reserve(_polygons.size());
    for (auto *src : _polygons)
    {
      list.append(QVariant::fromValue<QObject*>(src));
    }
    return list;
  }

  void setup_list(QString db_file, QString state, int year, QStringList divisions);
  void set_value(int division, double value);
  void clear_values();
  void set_color_scale_bounds(double min, double max);
  void set_colors();
  void set_label(QLabel* label);
  void set_decimals(int n);
  void set_fill_visible(bool visible);

signals:
  void double_clicked_division(int division);
  void polygonsChanged();

public slots:
  void finalise_setup(const QStringList& names, const QList<QList<QGeoCoordinate>>& coords);
  void update_scale_min(double x);
  void update_scale_max(double x);
  void point_in_polygon(double x, double y);
  void exited_map();
  void setup_error(QString);
  void received_double_click();

private:
  void _set_division_opacity(int div, double opacity);

  bool _map_not_ready     = true;
  double _color_scale_min = 0.;
  double _color_scale_max = 100.;
  QVector<Polygon_item*> _polygons;
  QVector<QVector<int>> _polygon_ids;
  QVector<QVector<QGeoCoordinate>> _polygon_bboxes;
  QStringList _divisions;
  int _current_mouseover_division = -1;
  QLabel* _label_division_info;
  int _decimals           = 1;
  double _default_opacity = 0.7;
  bool _fill_polygons     = true;
};

#endif // POLYGON_MODEL_H
