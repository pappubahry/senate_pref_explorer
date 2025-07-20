#ifndef MAP_CONTAINER_H
#define MAP_CONTAINER_H

#include <QQuickWidget>

class Map_container : public QQuickWidget
{
  Q_OBJECT

public:
  explicit Map_container(QWidget* parent = nullptr);
  ~Map_container();
  void init_variables();
  void update_coords();

public slots:
  void show_tooltip(QString text);

signals:
  void mouse_moved_2arg(double lon, double lat);
  void mouse_moved_4arg(double lon, double lat, double d_lon, double d_lat);
  void double_clicked();

protected:
  void mouseMoveEvent(QMouseEvent* event);
  void mousePressEvent(QMouseEvent* event);
  void mouseReleaseEvent(QMouseEvent* event);
  void wheelEvent(QWheelEvent* event);
  void mouseDoubleClickEvent(QMouseEvent* event);

private:
  const double _rad2deg = 57.2957795131;
  QObject* _map;
  double _zoom;
  double _longitude;
  double _latitude;
  double _tile_x;
  double _tile_y;
};

#endif // MAP_CONTAINER_H
