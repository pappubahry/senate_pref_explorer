#include "worker_sql_main_table.h"
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

Worker_sql_main_table::Worker_sql_main_table(
  int thread_num, const QString& db_file, const QString& q, bool wide_table, int num_groups, int num_rows, int num_booths, QVector<int>& clicked_cells)
  : _thread_num(thread_num)
  , _db_file(db_file)
  , _q(q)
  , _wide_table(wide_table)
  , _num_groups(num_groups)
  , _num_rows(num_rows)
  , _num_booths(num_booths)
  , _clicked_cells(clicked_cells)
{
}

Worker_sql_main_table::~Worker_sql_main_table() {}

void Worker_sql_main_table::do_query()
{
  // If wide_table is true, then the SQL query q should return a table with
  // one row per division, and one column per group.
  //
  // If wide_table is false, then it should return a three-column table (division_ID, group_ID, votes).

  // Need to open the database from each thread separately.

  QString connection_name = QString("db_conn_%1").arg(_thread_num);

  // The following is inside its own scope so that the database can be
  // removed properly: https://doc.qt.io/qt-5/qsqldatabase.html#removeDatabase
  {
    // *** Should add error handling ***
    QSqlDatabase _db = QSqlDatabase::addDatabase("QSQLITE", connection_name);
    _db.setDatabaseName(_db_file);
    _db.open();

    QSqlQuery query(_db);

    if (!query.exec(_q))
    {
      emit error(QString("Error: failed to execute query:\n%1").arg(_q));
      _db.close();
      QSqlDatabase::removeDatabase(connection_name);
      return;
    }

    QVector<QVector<int>> column_results(_num_rows);

    for (int i = 0; i < _num_rows; i++)
    {
      for (int j = 0; j < _num_booths; j++)
      {
        column_results[i].append(0);
      }
    }

    int group_id, div_id;
    int div_votes;

    // *** Ideally have better error-handling here, sometimes have a problem
    // with the query.value() ***

    if (_wide_table)
    {
      while (query.next())
      {
        if (query.record().count() != _num_groups + 3)
        {
          emit error(QString("Internal error: wrong number of columns in query:\n%1").arg(_q));
          return;
        }

        div_id = query.value(0).toInt();
        for (int i = 1; i < _num_groups + 2; i++)
        {
          group_id = i - 1;

          // If the group has already been clicked on in the table, then set the
          // cell to zero.
          div_votes                        = _clicked_cells.indexOf(group_id) >= 0 ? 0 : query.value(i).toInt();
          column_results[group_id][div_id] = div_votes;
        }
      }
    }
    else
    {
      while (query.next())
      {
        div_id = query.value(0).toInt();

        group_id = query.value(1).toInt();
        if (group_id == 999)
        {
          group_id = _num_groups;
        } // Exhaust

        div_votes                        = query.value(2).toInt();
        column_results[group_id][div_id] = div_votes;
      }
    }

    _db.close();
    emit finished_query(column_results);
  }

  QSqlDatabase::removeDatabase(connection_name);
}
