#include "worker_setup_polygon.h"
#include "polygon_model.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTextStream>

Worker_setup_polygon::Worker_setup_polygon(
  QString& db_file, QString& state, int year, QStringList& divisions, QVector<Polygon_item*>& polygons)
  : _db_file(db_file)
  , _state(state)
  , _year(year)
  , _divisions(divisions)
  , _polygons(polygons)
{
}

Worker_setup_polygon::~Worker_setup_polygon() {}

void Worker_setup_polygon::start_setup()
{
  QTextStream in;
  QString boundaries_csv;
  QFile polygons_file;
  bool boundaries_in_db         = false;
  const QString connection_name = "db_conn_polygons";

  QList<QList<QGeoCoordinate>> coords;
  QStringList names;

  if (!_db_file.isEmpty())
  {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connection_name);
    db.setDatabaseName(_db_file);
    db.open();

    QSqlQuery query(db);

    QString q = "SELECT 1 FROM sqlite_master WHERE type='table' AND name='boundaries'";
    if (!query.exec(q))
    {
      emit error("Error: failed to execute query:\n" + q);
      return;
    }

    if (query.next())
    {
      // The boundaries table is in the file.

      q = "SELECT boundaries_csv FROM boundaries";

      if (!query.exec(q))
      {
        emit error("Error: failed to execute query:\n" + q);
        return;
      }

      if (query.next())
      {
        boundaries_csv = query.value(0).toString();
        in.setString(&boundaries_csv);
        boundaries_in_db = true;
      }
      else
      {
        emit error("Error in database: Missing boundaries.");
        emit finished_coordinates(names, coords);
        return;
      }
    }

    db.close();
  }

  QSqlDatabase::removeDatabase(connection_name);

  if (!boundaries_in_db)
  {
    // This section is legacy code from when the boundaries were stored
    // in files that came with the program, and remains here so that old
    // sqlite files (which don't contain the division boundaries) continue
    // to work.
    if (_year > 2019)
    {
      emit finished_coordinates(names, coords);
      return;
    }

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
      emit finished_coordinates(names, coords);
      return;
    }

    polygons_file_name = QString("%1/boundaries/polygon_model_%2.csv").arg(QCoreApplication::applicationDirPath(), polygons_file_name);

    QFileInfo polygon_file_info(polygons_file_name);

    if (polygon_file_info.exists())
    {
      polygons_file.setFileName(polygons_file_name);

      if (polygons_file.open(QIODevice::ReadOnly))
      {
        in.setDevice(&polygons_file);
      }
    }
    else
    {
      emit finished_coordinates(names, coords);
      return;
    }
  }

  int polygon_count = 0;
  while (!in.atEnd())
  {
    const QString line      = in.readLine();
    const QStringList cells = line.split(",");

    if (cells.at(0) == "start")
    {
      const QString name = cells.at(1);
      names.append(name);
      coords.append(QList<QGeoCoordinate>());
    }
    else if (cells.at(0) == "end")
    {
      polygon_count++;
    }
    else
    {
      coords[polygon_count].append(QGeoCoordinate(cells.at(0).toDouble(), cells.at(1).toDouble()));
    }
  }

  emit finished_coordinates(names, coords);
}
