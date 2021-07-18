#ifndef WORKER_SETUP_POLYGON_H
#define WORKER_SETUP_POLYGON_H

#include <QObject>
#include "polygon_model.h"

class Worker_setup_polygon : public QObject
{
  Q_OBJECT
public:
  Worker_setup_polygon(QString db_file,
                       QString state,
                       int year,
                       QStringList divisions,
                       QVector<Polygon_item> &polygons);
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
  QVector<Polygon_item> &_polygons;
  void setup_act_2016(QStringList divisions);
  void setup_act_2018(QStringList divisions);
  void setup_nsw_2016(QStringList divisions);
  void setup_nt_2008(QStringList divisions);
  void setup_nt_2017(QStringList divisions);
  void setup_qld_2010(QStringList divisions);
  void setup_qld_2018(QStringList divisions);
  void setup_sa_2011(QStringList divisions);
  void setup_sa_2018(QStringList divisions);
  void setup_tas_2009(QStringList divisions);
  void setup_tas_2017(QStringList divisions);
  void setup_vic_2011(QStringList divisions);
  void setup_vic_2018(QStringList divisions);
  void setup_wa_2016(QStringList divisions);
};

#endif // WORKER_SETUP_POLYGON_H
