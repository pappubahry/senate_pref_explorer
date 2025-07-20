#ifndef WORKER_SETUP_POLYGON_H
#define WORKER_SETUP_POLYGON_H

#include "polygon_model.h"
#include <QObject>

class Worker_setup_polygon : public QObject
{
  Q_OBJECT
public:
  Worker_setup_polygon(QString& db_file, QString& state, int year, QStringList& divisions, QVector<Polygon_item>& polygons);
  ~Worker_setup_polygon();

signals:
  void finished_coordinates();
  void error(QString err);

public slots:
  void start_setup();

private:
  QString _db_file;
  QString _state;
  int _year;
  QStringList _divisions;
  QVector<Polygon_item>& _polygons;
};

#endif // WORKER_SETUP_POLYGON_H
