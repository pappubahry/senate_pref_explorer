#include "worker_sql_npp_table.h"
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

Worker_sql_npp_table::Worker_sql_npp_table(
  int thread_num, const QString& db_file, const QString& q, int num_groups, int num_booths, QVector<int>& clicked_n_parties)
  : _thread_num(thread_num)
  , _db_file(db_file)
  , _q(q)
  , _num_groups(num_groups)
  , _clicked_n_parties(clicked_n_parties)
  , _num_booths(num_booths)
{
}

Worker_sql_npp_table::~Worker_sql_npp_table() {}

void Worker_sql_npp_table::do_query()
{
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
    query.setForwardOnly(true);

    if (!query.exec(_q))
    {
      emit error(QString("Error: failed to execute query:\n%1").arg(_q));
      _db.close();
      QSqlDatabase::removeDatabase(connection_name);
      return;
    }

    const int n = _clicked_n_parties.length();

    QVector<QVector<QVector<int>>> table_results;

    for (int i = 0; i <= n; i++)
    {
      table_results.append(QVector<QVector<int>>());
      for (int j = 0; j < _num_groups; j++)
      {
        table_results[i].append(QVector<int>());
        for (int k = 0; k < _num_booths; k++)
        {
          table_results[i][j].append(0);
        }
      }
    }

    int booth_id, group_id, n_party_id;
    int votes;

    while (query.next())
    {
      booth_id   = query.value(0).toInt();
      group_id   = query.value(1).toInt();
      n_party_id = -1;

      for (int i = 0; i <= n; i++)
      {
        if (query.value(i + 3).toInt() == 1)
        {
          n_party_id = i;
          break;
        }
      }

      if (n_party_id < 0)
      {
        emit error(QString("Internal error: Wrong SQL for n-party-preferred. :(\n%1").arg(_q));
        return;
      }

      votes = query.value(2).toInt();

      if (_clicked_n_parties.indexOf(group_id) > -1)
      {
        votes = 0;
      }

      table_results[n_party_id][group_id][booth_id] = votes;
    }

    _db.close();
    emit finished_query(table_results);
  }

  QSqlDatabase::removeDatabase(connection_name);
}
