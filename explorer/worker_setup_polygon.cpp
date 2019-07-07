#include "worker_setup_polygon.h"
#include "polygon_model.h"
#include <QCoreApplication>
#include <QTextStream>
#include <QMessageBox>
#include <QFileInfo>
#include <QFile>
#include <QDebug>

Worker_setup_polygon::Worker_setup_polygon(QString state,
                                           int year,
                                           QStringList divisions,
                                           QVector<Polygon_item> &polygons)
  : _polygons(polygons)
{
  _state = state;
  _year = year;
  for (int i = 0; i < divisions.length(); i++)
  {
    _divisions.append(divisions.at(i));
  }
}

Worker_setup_polygon::~Worker_setup_polygon()
{
}

void Worker_setup_polygon::start_setup()
{
  QString polygons_file_name;
  
  if (_state == "QLD")
  {
    if (_year == 2016) { polygons_file_name = "qld_2010"; }
    if (_year == 2019) { polygons_file_name = "qld_2018"; }
  }
  else if (_state == "NSW")
  {
    if (_year == 2016) { polygons_file_name = "nsw_2016"; }
    if (_year == 2019) { polygons_file_name = "nsw_2016"; }
  }
  else if (_state == "VIC")
  {
    if (_year == 2016) { polygons_file_name = "vic_2011"; }
    if (_year == 2019) { polygons_file_name = "vic_2018"; }
  }
  else if (_state == "TAS")
  {
    if (_year == 2016) { polygons_file_name = "tas_2009"; }
    if (_year == 2019) { polygons_file_name = "tas_2017"; }
  }
  else if (_state == "SA")
  {
    if (_year == 2016) { polygons_file_name = "sa_2011"; }
    if (_year == 2019) { polygons_file_name = "sa_2018"; }
  }
  else if (_state == "WA")
  {
    if (_year == 2016) { polygons_file_name = "wa_2016"; }
    if (_year == 2019) { polygons_file_name = "wa_2016"; }
  }
  else if (_state == "ACT")
  {
    if (_year == 2016) { polygons_file_name = "act_2016"; }
    if (_year == 2019) { polygons_file_name = "act_2018"; }
  }
  else if (_state == "NT")
  {
    if (_year == 2016) { polygons_file_name = "nt_2008"; }
    if (_year == 2019) { polygons_file_name = "nt_2017"; }
  }
  else
  {
    emit finished_coordinates();
    return;
  }
  
  polygons_file_name = QString("%1/boundaries/polygon_model_%2.csv")
      .arg(QCoreApplication::applicationDirPath())
      .arg(polygons_file_name);
  
  QFileInfo check_exists(polygons_file_name);
  
  if (check_exists.exists())
  {
    QFile polygons_file(polygons_file_name);
    
    if (polygons_file.open(QIODevice::ReadOnly))
    {
      QTextStream in(&polygons_file);
      
      Polygon_item item;
      
      while (!in.atEnd())
      {
        QString line = in.readLine();
        QStringList cells = line.split(",");
        
        if (cells.at(0) == "start")
        {
          item.division_name = cells.at(1);
          item.division_id = _divisions.indexOf(item.division_name);
          item.coordinates.clear();
        }
        else if (cells.at(0) == "end")
        {
          _polygons.append(item);
        }
        else
        {
          item.coordinates.append(QGeoCoordinate(cells.at(0).toDouble(), cells.at(1).toDouble()));
        }
      }
    }
  }
  else
  {
    emit error(QString("Error: couldn't find boundaries file %1").arg(polygons_file_name));
  }
  
  emit finished_coordinates();
}
