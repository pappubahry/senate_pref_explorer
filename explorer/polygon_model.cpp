#include "polygon_model.h"

#include <QImage>
#include <QMessageBox>
#include <QThread>
#include "worker_setup_polygon.h"

Polygon_model::Polygon_model(QObject *parent) : QAbstractListModel(parent)
{
}

int Polygon_model::rowCount(const QModelIndex &parent) const
{
  if (parent.isValid()) { return 0; }
  return _polygons.length();
}

QVariant Polygon_model::data(const QModelIndex &index, int role) const
{
  if (!index.isValid() || _polygons.length() == 0)
  {
    return QVariant();
  }
  
  const Polygon_item item = _polygons.at(index.row());
  
  if      (role == Division_id_role)   { return QVariant(item.division_id);   }
  else if (role == Division_name_role) { return QVariant(item.division_name); }
  else if (role == Value_role)         { return QVariant(item.value);         }
  else if (role == Red_role)           { return QVariant(item.red);           }
  else if (role == Green_role)         { return QVariant(item.green);         }
  else if (role == Blue_role)          { return QVariant(item.blue);          }
  else if (role == Opacity_role)       { return QVariant(item.opacity);       }
  else if (role == Coordinates_role)
  {
    // https://stackoverflow.com/a/51428928
    QVariantList coords;
    for (const QGeoCoordinate &coord: item.coordinates)
    {
      coords.append(QVariant::fromValue(coord));
    }
    return coords;
  }
  
  return QVariant();
}

void Polygon_model::set_value(int division, double value)
{
  if (division >= _polygon_ids.length() || map_not_ready) { return; }
  
  for (int i = 0; i < _polygon_ids.at(division).length(); i++)
  {
    int j = _polygon_ids.at(division).at(i);
    _polygons[j].value = value;
    
    QModelIndex index = this->index(j);
    
    emit dataChanged(index, index, QVector<int>() << Value_role);
  }
}

void Polygon_model::clear_values()
{
  if (map_not_ready) { return; }
  for (int i = 0; i < _polygon_ids.length(); i++)
  {
    set_value(i, 0.);
  }
  set_colors();
}

void Polygon_model::set_color_scale_bounds(double min, double max)
{
  color_scale_min = min;
  color_scale_max = max;
}

void Polygon_model::update_scale_min(double x)
{
  color_scale_min = x;
  set_colors();
}

void Polygon_model::update_scale_max(double x)
{
  color_scale_max = x;
  set_colors();
}

void Polygon_model::set_colors()
{
  if (map_not_ready) { return; }
  
  for (int i = 0; i < _polygons.length(); i++)
  {
    double denom = color_scale_max - color_scale_min;
    int j = denom > 1e-5 ? qRound(255 * (_polygons.at(i).value - color_scale_min) / denom) : 0;
    
    j = qMax(j, 0);
    j = qMin(j, 255);
    
    _polygons[i].red   = viridis_scale.at(j).at(0);
    _polygons[i].green = viridis_scale.at(j).at(1);
    _polygons[i].blue  = viridis_scale.at(j).at(2);
    
    QModelIndex index = this->index(i);
    emit dataChanged(index, index, QVector<int>() << Red_role << Green_role << Blue_role);
  }
}

void Polygon_model::set_label(QLabel *label)
{
  _label_division_info = label;
}

void Polygon_model::set_decimals(int n)
{
  _decimals = n;
}

void Polygon_model::set_fill_visible(bool visible)
{
  fill_polygons = visible;
  double opacity = visible ? default_opacity : 0.;
  for (int i = 0; i < _polygons.length(); i++)
  {
    QModelIndex index = this->index(i);
    setData(index, opacity, Opacity_role);
  }
}

void Polygon_model::point_in_polygon(double x, double y)
{
  if (map_not_ready) { return; }
  QVector<int> candidates;
  
  // Only do the full loop over coordinates on polygons in whose
  // bounding box the mouse (lon, lat) currently is.
  for (int i = 0; i < _polygon_bboxes.length(); i++)
  {
    if (x > _polygon_bboxes.at(i).at(0).longitude() &&
        x < _polygon_bboxes.at(i).at(1).longitude() &&
        y > _polygon_bboxes.at(i).at(0).latitude()  &&
        y < _polygon_bboxes.at(i).at(1).latitude())
    {
      candidates.append(i);
    }
  }
  
  int poly = -1;
  bool inside_poly = false;
  
  // Raycaster algorithm.  CAUTION!  I treat (0, 0) as definitely
  // outside the polygon, which is fine for Australia, but not
  // everywhere.
  
  for (int i = 0; i < candidates.length(); i++)
  {
    int idx = candidates.at(i);
    int intersections = 0;
    for (int j = 0; j < _polygons.at(idx).coordinates.length() - 1; j++)
    {
      double x1 = _polygons.at(idx).coordinates.at(j).longitude();
      double x2 = _polygons.at(idx).coordinates.at(j+1).longitude();
      double y1 = _polygons.at(idx).coordinates.at(j).latitude();
      double y2 = _polygons.at(idx).coordinates.at(j+1).latitude();
      
      double numer = y1*x - x1*y;
      double denom = y*(x2 - x1) - x*(y2 - y1);
      
      if (((numer > 0 && denom > 0) || (numer < 0 && denom < 0)) &&
          (qAbs(numer) < qAbs(denom)))
      {
        double s = numer / denom;
        double t = (x1 + s*(x2 - x1)) / x;
        
        if (t > 0 && t < 1)
        {
          intersections++;
        }
      }
    }
    
    if (intersections % 2 == 1)
    {
      inside_poly = true;
      poly = idx;
      break;
    }
  }
  
  if (inside_poly)
  {
    int this_div = _polygons.at(poly).division_id;
    
    if (this_div != current_mouseover_division)
    {
      QModelIndex index;
      if (current_mouseover_division >= 0 && fill_polygons)
      {
        index = this->index(_polygon_ids.at(current_mouseover_division).at(0));
        setData(index, default_opacity, Opacity_role);
      }
      
      current_mouseover_division = this_div;
      
      _label_division_info->setText(QString("%1: %2")
                                    .arg(_polygons.at(poly).division_name)
                                    .arg(_polygons.at(poly).value, 0, 'f', _decimals));
      
      index = this->index(poly);
      if (fill_polygons)
      {
        setData(index, 1., Opacity_role);
      }
    }
  }
  else
  {
    if (current_mouseover_division >= 0)
    {
      _label_division_info->setText("");
      QModelIndex index = this->index(_polygon_ids.at(current_mouseover_division).at(0));
      
      if (fill_polygons)
      {
        setData(index, default_opacity, Opacity_role);
      }
      
      current_mouseover_division = -1;
    }
  }
}

void Polygon_model::exited_map()
{
  point_in_polygon(-500., -500.);
}

bool Polygon_model::setData(const QModelIndex &index, const QVariant &value, int role)
{
  if (map_not_ready || _polygons.length() <= index.row()) { return false; }
  
  // Only the opacity should be changed by a call to setData()
  if (role == Opacity_role)
  {
    int division = _polygons.at(index.row()).division_id;
    for (int i = 0; i < _polygon_ids.at(division).length(); i++)
    {
      QModelIndex idx = this->index(_polygon_ids.at(division).at(i));
      
      if (qAbs(_polygons[_polygon_ids.at(division).at(i)].opacity - value.toDouble()) > 1.e-5)
      {
        _polygons[_polygon_ids.at(division).at(i)].opacity = value.toDouble();
        emit dataChanged(idx, idx, QVector<int>() << Opacity_role);
      }
    }
    
    return true;
  }
  
  return false;
}

Qt::ItemFlags Polygon_model::flags(const QModelIndex &index) const
{
  if (!index.isValid()) { return Qt::NoItemFlags; }
  return Qt::ItemIsEditable;
}

QHash<int, QByteArray> Polygon_model::roleNames() const
{
  QHash<int, QByteArray> names;
  names[Division_id_role] = "division_id";
  names[Division_name_role] = "division_name";
  names[Coordinates_role] = "coordinates";
  names[Value_role] = "value";
  names[Red_role] = "red";
  names[Green_role] = "green";
  names[Blue_role] = "blue";
  names[Opacity_role] = "opacity";
  return names;
}

void Polygon_model::setup_list(QString state, int year, QStringList divisions)
{
  map_not_ready = true;
  
  beginResetModel();
  _polygons.clear();
  _polygon_ids.clear();
  _polygon_bboxes.clear();
  _divisions.clear();
  
  if (state != "")
  {
    for (int i = 0; i < divisions.length(); i++)
    {
      _divisions.append(divisions.at(i));
    }
    
    QThread *thread = new QThread;
    Worker_setup_polygon *worker = new Worker_setup_polygon(state,
                                                            year,
                                                            divisions,
                                                            _polygons);
    
    
    worker->moveToThread(thread);
    
    connect(thread, SIGNAL(started()),              worker, SLOT(start_setup()));
    connect(worker, SIGNAL(finished_coordinates()), this,   SLOT(finalise_setup()));
    connect(worker, SIGNAL(finished_coordinates()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished_coordinates()), worker, SLOT(deleteLater()));
    connect(worker, SIGNAL(error(QString)),         this,   SLOT(setup_error(QString)));
    connect(thread, SIGNAL(finished()),             thread, SLOT(deleteLater()));
    thread->start();
  }
}

void Polygon_model::finalise_setup()
{
  QStringList divisions_upper;
  for (int i = 0; i < _divisions.length(); i++)
  {
    divisions_upper.append(_divisions.at(i).toUpper());
  }
  
  double opacity = fill_polygons ? default_opacity : 0.;
  
  for (int i = 0; i < _polygons.length(); i++)
  {
    _polygons[i].value = 0.;
    _polygons[i].opacity = opacity;
    _polygons[i].red = 0.;
    _polygons[i].blue = 0.;
    _polygons[i].green = 0.;
    
    double min_lon =  500.;
    double max_lon = -500.;
    double min_lat =  500.;
    double max_lat = -500.;
    
    for (int j = 0; j < _polygons.at(i).coordinates.length(); j++)
    {
      min_lon = qMin(min_lon, _polygons.at(i).coordinates.at(j).longitude());
      max_lon = qMax(max_lon, _polygons.at(i).coordinates.at(j).longitude());
      min_lat = qMin(min_lat, _polygons.at(i).coordinates.at(j).latitude());
      max_lat = qMax(max_lat, _polygons.at(i).coordinates.at(j).latitude());
    }
    
    _polygon_bboxes.append(QVector<QGeoCoordinate>());
    _polygon_bboxes[i].append(QGeoCoordinate(min_lat, min_lon));
    _polygon_bboxes[i].append(QGeoCoordinate(max_lat, max_lon));
    
    if (_polygons.at(i).division_id < 0)
    {
      int j = divisions_upper.indexOf(_polygons.at(i).division_name.toUpper());
      if (j >= 0)
      {
        _polygons[i].division_id = j;
        _polygons[i].division_name = _divisions.at(j);
      }
      else
      {
        QMessageBox m;
        m.setText(QString("Error: Couldn't find %1.  The application will probably crash"
                               " if you move your mouse over it!")
                  .arg(_polygons.at(i).division_name));
        m.exec();
      }
    }
  }
  
  for (int i = 0; i < _divisions.length(); i++)
  {
    _polygon_ids.append(QVector<int>(0));
    for (int j = 0; j < _polygons.length(); j++)
    {
      if (_polygons.at(j).division_id == i)
      {
        _polygon_ids[i].append(j);
      }
    }
  }
  
  endResetModel();
  
  map_not_ready = false;
}

void Polygon_model::setup_error(QString error)
{
  QMessageBox m;
  m.setText(error);
  m.exec();
  endResetModel();
  map_not_ready = true;
}

void Polygon_model::received_double_click()
{
  if (current_mouseover_division >= 0)
  {
    emit double_clicked_division(current_mouseover_division);
  }
}
