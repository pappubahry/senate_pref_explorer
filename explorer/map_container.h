#ifndef MAP_CONTAINER_H
#define MAP_CONTAINER_H

#include <QQuickWidget>

class Map_container : public QQuickWidget
{
  Q_OBJECT
  
public:
  explicit Map_container(int size, QWidget *parent = nullptr);
  ~Map_container();
  void init_variables();
  void update_coords();
  
signals:
  void mouse_moved(double lon, double lat);
  
protected:
  void mouseMoveEvent(QMouseEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void wheelEvent(QWheelEvent *event);
  
private:
  const double rad2deg = 57.2957795131;
  QObject *_map;
  int _size;
  double _zoom;
  double _longitude;
  double _latitude;
  double _tile_x;
  double _tile_y;
};

#endif // MAP_CONTAINER_H
