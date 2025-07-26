#ifndef BOOTH_MODEL_H
#define BOOTH_MODEL_H

// In previous versions, I used a subclass of QAbstractListModel.  After the
// polygons handling was changed (see polygon_model.h), I updated the booths
// model to the same structure, but here the difference is much more marginal,
// maybe a 20-millisecond improvement in the first rendering of the text.
//
// Based on QML profiling, all the individual properties of the text are bound
// to the QML much faster via the structure below, but binding the model itself
// takes about 50 milliseconds on my computer, versus just 9 ms for the old
// QAbstractListModel, offsetting most of the gains.

#include <QObject>
#include <QColor>
#include <QVariant>
#include <QGeoCoordinate>

struct Booth
{
  int id;
  QString division;
  int division_id;
  QString booth;
  double longitude;
  double latitude;
  int formal_votes;
};

class Booth_item : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QGeoCoordinate coordinates READ coordinates NOTIFY coordinatesChanged)
  Q_PROPERTY(QColor color READ color NOTIFY colorChanged)
  Q_PROPERTY(QString text READ text NOTIFY textChanged)
  Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)

public:
  explicit Booth_item(QObject* parent = nullptr)
    : QObject(parent)
  {}

  QGeoCoordinate coordinates() const { return _coordinates; }
  QColor               color() const { return _color;       }
  QString               text() const { return _text;        }
  bool               visible() const { return _visible;     }

  void set_coordinates(QGeoCoordinate coord)
  {
    _coordinates = coord;
    emit coordinatesChanged();
  }

  void set_color(const QColor& c)
  {
    _color = c;
    emit colorChanged();
  }

  void set_text(const QString& t)
  {
    _text = t;
    emit textChanged();
  }

  void set_visible(bool visible)
  {
    _visible = visible;
    emit visibleChanged();
  }

  void set_value(double v) { _value = v; }
  double get_value() const { return _value; }

  void set_booth_id(int id) { _booth_id = id; }
  int get_booth_id() const { return _booth_id; }

  void set_division_id(int id) { _division_id = id; }
  int get_division_id() const { return _division_id; }

  void set_division_name(const QString& name) { _division_name = name; }
  QString get_division_name() const { return _division_name; }

  void set_booth_name(QString& name) { _booth_name = name; }
  QString get_booth_name() const { return _booth_name; }

  void set_formal_votes(int votes) { _formal_votes = votes; }
  int get_formal_votes() const { return _formal_votes; }

  void set_prepoll(bool p) { _prepoll = p; }
  bool is_prepoll() const { return _prepoll; }

  void set_in_active_division(bool b) { _in_active_division = b; }
  bool is_in_active_division() const { return _in_active_division; }

  void set_above_threshold(bool b) { _above_threshold = b; }
  bool is_above_threshold() const { return _above_threshold; }


signals:
  void coordinatesChanged();
  void colorChanged();
  void textChanged();
  void visibleChanged();

private:
  // Shown to the QML:
  QGeoCoordinate _coordinates;
  QColor _color;
  QString _text;
  bool _visible;

  // Not shown to the QML:
  int _booth_id;
  int _division_id;
  QString _division_name;
  QString _booth_name;
  double _value;
  int _formal_votes;
  bool _prepoll;
  bool _in_active_division;
  bool _above_threshold;
};

class Booth_model : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QVariantList booths READ booths NOTIFY boothsChanged)
public:
  explicit Booth_model(QObject* parent = nullptr)
    : QObject(parent)
  {
  }

  QVariantList booths() const
  {
    QVariantList list;
    list.reserve(_booths.size());
    for (auto *src : _booths)
    {
      list.append(QVariant::fromValue<QObject*>(src));
    }
    return list;
  }

  void setup_list(QVector<Booth>& booths, int booth_threshold);
  void clear_values();
  void set_value(int booth_id, double value);
  void set_colors();
  void set_decimals(int n);
  void set_decimals_mouseover(int n);
  void set_visible(bool b);
  bool is_booth_visible(int i);
  void recalculate_visible(int i);
  void update_prepoll_flag(bool show_prepoll);
  void set_idle(bool b);

public slots:
  void update_scale_min(double x);
  void update_scale_max(double x);
  void update_min_votes(int n);
  void update_active_division(int division);
  void check_mouseover(double lon, double lat, double d_lon, double d_lat);

signals:
  void boothsChanged();
  void send_tooltip(QString text);

private:
  QVector<Booth_item*> _booths;
  bool _map_not_ready     = true;
  bool _idle              = true;
  bool _text_type         = false;
  bool _prepoll           = false;
  int _votes_threshold    = 0;
  double _color_scale_min = 0.;
  double _color_scale_max = 100.;
  QVector<int> _booth_ids;
  int _decimals           = 0;
  int _decimals_mouseover = 1;
};

#endif // BOOTH_MODEL_H
