#include "polygon_model.h"
#include "viridis.h"
#include "worker_setup_polygon.h"

#include <QColor>
#include <QMessageBox>
#include <QThread>

void Polygon_model::set_value(int division, double value)
{
  if (division >= _polygon_ids.length() || _map_not_ready)
  {
    return;
  }

  for (int i = 0; i < _polygon_ids.at(division).length(); i++)
  {
    int j = _polygon_ids.at(division).at(i);
    _polygons[j]->set_value(value);
  }
}

void Polygon_model::clear_values()
{
  if (_map_not_ready)
  {
    return;
  }
  for (int i = 0; i < _polygon_ids.length(); i++)
  {
    set_value(i, 0.);
  }
  set_colors();
}

void Polygon_model::set_color_scale_bounds(double min, double max)
{
  _color_scale_min = min;
  _color_scale_max = max;
}

void Polygon_model::update_scale_min(double x)
{
  _color_scale_min = x;
  set_colors();
}

void Polygon_model::update_scale_max(double x)
{
  _color_scale_max = x;
  set_colors();
}

void Polygon_model::set_colors()
{
  if (_map_not_ready)
  {
    return;
  }

  for (int i = 0; i < _polygons.length(); i++)
  {
    const double denom = _color_scale_max - _color_scale_min;
    int j        = denom > 1e-5 ? qRound(255 * (_polygons.at(i)->get_value() - _color_scale_min) / denom) : 0;

    j = qMax(j, 0);
    j = qMin(j, 255);

    const double alpha  = _polygons.at(i)->color().alphaF();
    const double red    = Viridis::colors.at(j).at(0);
    const double green  = Viridis::colors.at(j).at(1);
    const double blue   = Viridis::colors.at(j).at(2);

    const QColor color = QColor::fromRgbF(red, green, blue, alpha);

    _polygons[i]->set_color(color);
  }
}

void Polygon_model::set_label(QLabel* label)
{
  _label_division_info = label;
}

void Polygon_model::set_decimals(int n)
{
  _decimals = n;
}

void Polygon_model::set_fill_visible(bool visible)
{
  _fill_polygons = visible;
  const double opacity = visible ? _default_opacity : 0.;
  for (int i = 0; i < _polygons.length(); i++)
  {
    _polygons.at(i)->set_opacity(opacity);
  }
}

void Polygon_model::point_in_polygon(double x, double y)
{
  if (_map_not_ready)
  {
    return;
  }
  QVector<int> candidates;

  // Only do the full loop over coordinates on polygons in whose
  // bounding box the mouse (lon, lat) currently is.
  for (int i = 0; i < _polygon_bboxes.length(); i++)
  {
    if (x > _polygon_bboxes.at(i).at(0).longitude() && x < _polygon_bboxes.at(i).at(1).longitude() && y > _polygon_bboxes.at(i).at(0).latitude()
        && y < _polygon_bboxes.at(i).at(1).latitude())
    {
      candidates.append(i);
    }
  }

  int poly         = -1;
  bool inside_poly = false;

  // Raycaster algorithm.  CAUTION!  I treat (0, 0) as definitely
  // outside the polygon, which is fine for Australia, but not
  // everywhere.

  for (int i = 0; i < candidates.length(); i++)
  {
    int idx           = candidates.at(i);
    int intersections = 0;
    const int n_verticles = _polygons.at(idx)->path().length();
    for (int j = 0; j < n_verticles - 1; j++)
    {
      const QGeoCoordinate coord1 = _polygons.at(idx)->path().at(j).value<QGeoCoordinate>();
      const QGeoCoordinate coord2 = _polygons.at(idx)->path().at(j + 1).value<QGeoCoordinate>();
      double x1 = coord1.longitude();
      double x2 = coord2.longitude();
      double y1 = coord1.latitude();
      double y2 = coord2.latitude();

      double numer = y1 * x - x1 * y;
      double denom = y * (x2 - x1) - x * (y2 - y1);

      if (((numer > 0 && denom > 0) || (numer < 0 && denom < 0)) && (qAbs(numer) < qAbs(denom)))
      {
        double s = numer / denom;
        double t = (x1 + s * (x2 - x1)) / x;

        if (t > 0 && t < 1)
        {
          intersections++;
        }
      }
    }

    if (intersections % 2 == 1)
    {
      inside_poly = true;
      poly        = idx;
      break;
    }
  }

  if (inside_poly)
  {
    const int this_div = _polygons.at(poly)->get_division_id();

    if (this_div != _current_mouseover_division)
    {
      if (_current_mouseover_division >= 0 && _fill_polygons)
      {
        _set_division_opacity(_current_mouseover_division, _default_opacity);
      }

      _current_mouseover_division = this_div;

      _label_division_info->setText(QString("%1: %2")
                                      .arg(_polygons.at(poly)->get_division_name(), QString::number(_polygons.at(poly)->get_value(), 'f', _decimals)));

      if (_fill_polygons)
      {
        _set_division_opacity(_current_mouseover_division, 1.);
      }
    }
  }
  else
  {
    if (_current_mouseover_division >= 0)
    {
      _label_division_info->setText("");

      if (_fill_polygons)
      {
        _set_division_opacity(_current_mouseover_division, _default_opacity);
      }

      _current_mouseover_division = -1;
    }
  }
}

void Polygon_model::exited_map()
{
  point_in_polygon(-500., -500.);
}

void Polygon_model::setup_list(QString db_file, QString state, int year, QStringList divisions)
{
  _map_not_ready = true;

  _polygons.clear();
  _polygon_ids.clear();
  _polygon_bboxes.clear();
  _divisions.clear();

  if (state == "")
  {
    return;
  }

  for (int i = 0; i < divisions.length(); i++)
  {
    _divisions.append(divisions.at(i));
  }

  QThread* thread              = new QThread;
  Worker_setup_polygon* worker = new Worker_setup_polygon(db_file, state, year, divisions, _polygons);

  worker->moveToThread(thread);
  connect(thread, &QThread::started,                           worker, &Worker_setup_polygon::start_setup);
  connect(worker, &Worker_setup_polygon::finished_coordinates, this,   &Polygon_model::finalise_setup);
  connect(worker, &Worker_setup_polygon::finished_coordinates, thread, &QThread::quit);
  connect(worker, &Worker_setup_polygon::finished_coordinates, worker, &Worker_setup_polygon::deleteLater);
  connect(worker, &Worker_setup_polygon::error,                this,   &Polygon_model::setup_error);
  connect(thread, &QThread::finished,                          thread, &QThread::deleteLater);
  thread->start();
}

void Polygon_model::finalise_setup(const QStringList& names, const QList<QList<QGeoCoordinate>>& coords)
{
  _polygons.clear();
  const int n_polygons = names.length();
  if (coords.length() != n_polygons) { return; }

  QStringList divisions_upper;
  for (int i = 0; i < _divisions.length(); i++)
  {
    divisions_upper.append(_divisions.at(i).toUpper());
  }

  const double opacity = _fill_polygons ? _default_opacity : 0.;

  for (int i = 0; i < n_polygons; ++i)
  {
    const QString name_upper = names.at(i).toUpper();
    const int div_id = divisions_upper.indexOf(name_upper);

    if (div_id < 0)
    {
      QMessageBox m;
      m.setText(QString("Error: Couldn't find %1.  Aborting polygon overlay.")
        .arg(_polygons.at(i)->get_division_name()));
      m.exec();
      return;
    }

    Polygon_item* item = new Polygon_item(this);
    item->set_division_name(_divisions.at(div_id));
    item->set_division_id(div_id);
    item->set_coordinates(coords.at(i));
    item->set_value(0.);
    const QColor black = QColor::fromRgbF(0., 0., 0., opacity);
    item->set_color(black);
    _polygons.append(item);

    double min_lon = 500.;
    double max_lon = -500.;
    double min_lat = 500.;
    double max_lat = -500.;

    for (int j = 0, n = coords.at(i).length(); j < n; ++j)
    {
      const QGeoCoordinate coord = coords.at(i).at(j);
      min_lon = qMin(min_lon, coord.longitude());
      max_lon = qMax(max_lon, coord.longitude());
      min_lat = qMin(min_lat, coord.latitude());
      max_lat = qMax(max_lat, coord.latitude());
    }

    _polygon_bboxes.append(QVector<QGeoCoordinate>());
    _polygon_bboxes[i].append(QGeoCoordinate(min_lat, min_lon));
    _polygon_bboxes[i].append(QGeoCoordinate(max_lat, max_lon));
  }

  for (int i = 0; i < _divisions.length(); i++)
  {
    _polygon_ids.append(QVector<int>(0));
    for (int j = 0; j < _polygons.length(); j++)
    {
      if (_polygons.at(j)->get_division_id() == i)
      {
        _polygon_ids[i].append(j);
      }
    }
  }

  emit polygonsChanged();
  _map_not_ready = false;
}

void Polygon_model::setup_error(QString error)
{
  QMessageBox m;
  m.setText(error);
  m.exec();
  _map_not_ready = true;
}

void Polygon_model::received_double_click()
{
  if (_current_mouseover_division >= 0)
  {
    emit double_clicked_division(_current_mouseover_division);
  }
}

void Polygon_model::_set_division_opacity(int div, double opacity)
{
  for (const int& i_poly : _polygon_ids.at(div))
  {
    _polygons.at(i_poly)->set_opacity(opacity);
  }
}
