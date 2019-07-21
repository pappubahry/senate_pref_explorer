#include "map_container.h"
#include "math.h"
#include <QQuickItem>
#include <QQmlProperty>
#include <QToolTip>

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
  
  const double zoom_factor = pow(2., _zoom);
  
  int x = event->pos().x();
  int y = event->pos().y();
  double mouse_tile_x = _tile_x + (x - _size/2)/256.;
  double mouse_tile_y = _tile_y + (y - _size/2)/256.;
  
  double lon = 360. * mouse_tile_x / zoom_factor - 180.;
  double n = M_PI - 2. * M_PI * mouse_tile_y / zoom_factor;
  double lat = rad2deg * atan(sinh(n));
  
  double d_lon = 360. / (256. * zoom_factor);
  double d_lat = 2. * M_PI * cosh(n)/((1 + sinh(n)*sinh(n)) * zoom_factor);
  
  emit mouse_moved(lon, lat);
  emit mouse_moved(lon, lat, d_lon, d_lat);
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

void Map_container::mouseDoubleClickEvent(QMouseEvent *event)
{
  QQuickWidget::mouseDoubleClickEvent(event);
  emit double_clicked();
}

void Map_container::update_coords()
{
  _zoom = QQmlProperty::read(_map, "zoomLevel").toDouble();
  _longitude = QQmlProperty::read(_map, "center.longitude").toDouble();
  _latitude  = QQmlProperty::read(_map, "center.latitude").toDouble();
  _tile_x = pow(2., _zoom) * (_longitude + 180.)/360.;
  _tile_y = pow(2., _zoom - 1.) * (1. - log(tan(_latitude/rad2deg) + 1./cos(_latitude/rad2deg)) / M_PI);
}

int Map_container::get_size()
{
  return _size;
}

void Map_container::show_tooltip(QString text)
{
  QPoint p = QCursor::pos();
  p.setY(p.y() + 10);
  QToolTip::showText(p, text, this);
}
