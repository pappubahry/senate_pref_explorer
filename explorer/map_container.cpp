#include "map_container.h"
#include "math.h"
#include <QQuickItem>
#include <QQmlProperty>
#include <QDebug>

Map_container::Map_container(int size, QWidget *parent) : QQuickWidget(parent)
{
  _size = size;
}

Map_container::~Map_container() {}

void Map_container::init_variables()
{
  _map = rootObject()->findChild<QObject*>("map");
  
  // *** Ideally have better error-handling here ***
  if (_map == nullptr) { return; }
  
  update_coords();
}

void Map_container::mouseMoveEvent(QMouseEvent *event)
{
  QQuickWidget::mouseMoveEvent(event);
  int x = event->pos().x();
  int y = event->pos().y();
  double mouse_tile_x = _tile_x + (x - _size/2)/256.;
  double mouse_tile_y = _tile_y + (y - _size/2)/256.;
  
  double lon = mouse_tile_x / pow(2.0, _zoom) * 360. - 180.;
  double n = M_PI - 2.*M_PI*mouse_tile_y/pow(2., _zoom);
  double lat = rad2deg * atan(0.5 * (exp(n) - exp(-n)));
  
  emit mouse_moved(lon, lat);
}

void Map_container::mousePressEvent(QMouseEvent *event)
{
  QQuickWidget::mousePressEvent(event);
}

void Map_container::mouseReleaseEvent(QMouseEvent *event)
{
  QQuickWidget::mouseReleaseEvent(event);
  update_coords();
}

void Map_container::wheelEvent(QWheelEvent *event)
{
  QQuickWidget::wheelEvent(event);
  update_coords();
}

void Map_container::update_coords()
{
  _zoom = QQmlProperty::read(_map, "zoomLevel").toDouble();
  _longitude = QQmlProperty::read(_map, "center.longitude").toDouble();
  _latitude  = QQmlProperty::read(_map, "center.latitude").toDouble();
  _tile_x = pow(2., _zoom) * (_longitude + 180.)/360.;
  _tile_y = pow(2., _zoom - 1.) * (1. - log(tan(_latitude/rad2deg) + 1./cos(_latitude/rad2deg)) / M_PI);
}
