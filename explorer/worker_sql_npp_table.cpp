#include "worker_sql_npp_table.h"
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QDebug>

Worker_sql_npp_table::Worker_sql_npp_table(int thread_num,
                                           QString db_file,
                                           QString q,
                                           int num_groups,
                                           int num_divisions,
                                           QVector<int> clicked_n_parties)
{
  _thread_num = thread_num;
  _db_file = db_file;
  _q = q;
  _num_groups = num_groups;
  _num_divisions = num_divisions;
  _clicked_n_parties = QVector<int>();
  
  for (int i = 0; i < clicked_n_parties.length(); i++)
  {
    _clicked_n_parties.append(clicked_n_parties.at(i));
  }
}

Worker_sql_npp_table::~Worker_sql_npp_table()
{
}

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
    
    if (!query.exec(_q))
    {
      emit error(QString("Error: failed to execute query:\n%1").arg(_q));
      return;
    }
    
    int n = _clicked_n_parties.length();
    
    QVector<QVector<Table_main_item>> table_results;
    
    for (int i = 0; i <= n; i++)
    {
      table_results.append(QVector<Table_main_item>(_num_groups));
      for (int j = 0; j < _num_groups; j++)
      {
        table_results[i][j].group_id = i;
        table_results[i][j].sorted_idx = i;
        for (int k = 0; k < _num_divisions; k++)
        {
          table_results[i][j].votes.append(0);
        }
      }
    }
    
    
    int div_id, group_id, n_party_id;
    long div_votes;
    
    while (query.next())
    {
      div_id = query.value(0).toInt();
      group_id = query.value(1).toInt();
      n_party_id = -1;
      
      for (int i = 0; i <= n; i++)
      {
        if (query.value(i+3).toInt() == 1)
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
      
      div_votes = query.value(2).toLongLong();
      
      if (_clicked_n_parties.indexOf(group_id) > -1)
      {
        div_votes = 0;
      }
      
      table_results[n_party_id][group_id].votes.replace(div_id, div_votes);
    }
    
    _db.close();
    emit finished_query(table_results);
  }
  
  QSqlDatabase::removeDatabase(connection_name);
}
