#include "worker_sql_main_table.h"
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QDebug>

Worker_sql_main_table::Worker_sql_main_table(int thread_num,
                                             QString db_file,
                                             QString q,
                                             bool wide_table,
                                             bool base_count,
                                             int num_groups,
                                             int num_rows,
                                             int num_divisions,
                                             QVector<int> clicked_cells)
{
  _thread_num = thread_num;
  _db_file = db_file;
  _q = q;
  _wide_table = wide_table;
  _base_count = base_count;
  _num_groups = num_groups;
  _num_rows = num_rows;
  _num_divisions = num_divisions;
  _clicked_cells = QVector<int>();
  for (int i = 0; i < clicked_cells.length(); i++)
  {
    _clicked_cells.append(clicked_cells.at(i));
  }
}

Worker_sql_main_table::~Worker_sql_main_table()
{
}

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
      return;
    }
    
    // This hack is for booth exports -- I don't have
    // a previous base count to work from in the main table,
    // so I need to calculate it during the query (sometimes).
    int extra_row = _wide_table && _base_count ? 1 : 0;
    
    QVector<Table_main_item> column_results(_num_rows + extra_row);
    
    for (int i = 0; i < _num_rows + extra_row; i++)
    {
      column_results[i].group_id = i;
      column_results[i].sorted_idx = i;
      for (int j = 0; j < _num_divisions; j++)
      {
        column_results[i].votes.append(0);
      }
    }
    
    int group_id, div_id;
    long div_votes;
    
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
        for (int i = 1; i < _num_groups + 2 + extra_row; i++)
        {
          group_id = i - 1;
          
          // If the group has already been clicked on in the table, then set the
          // cell to zero.
          div_votes = _clicked_cells.indexOf(group_id) >= 0 ? 0 : query.value(i).toLongLong();
          
          column_results[group_id].votes.replace(div_id, div_votes);
        }
      }
    }
    else
    {
      while (query.next())
      {
        div_id = query.value(0).toInt();
        
        group_id = query.value(1).toInt();
        if (group_id == 999) { group_id = _num_groups; }  // Exhaust
        
        div_votes = query.value(2).toLongLong();
        
        column_results[group_id].votes.replace(div_id, div_votes);
      }
    }
    
    _db.close();
    emit finished_query(column_results);
  }
  
  QSqlDatabase::removeDatabase(connection_name);
}
