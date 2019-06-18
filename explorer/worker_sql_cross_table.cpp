#include "worker_sql_cross_table.h"
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QDebug>

Worker_sql_cross_table::Worker_sql_cross_table(QString table_type,
                                               int thread_num,
                                               QString db_file,
                                               QString q,
                                               int num_rows,
                                               QVector<int> args)
{
  _table_type = table_type;
  _thread_num = thread_num;
  _db_file = db_file;
  _q = q;
  _num_rows = num_rows;
  
  for (int i = 0; i < args.length(); i++)
  {
    _args.append(args.at(i));
  }
}

Worker_sql_cross_table::~Worker_sql_cross_table()
{
}

void Worker_sql_cross_table::do_query()
{
  QString connection_name = QString("db_conn_%1").arg(_thread_num);
  
  {
    // *** Should add error handling ***
    QSqlDatabase _db = QSqlDatabase::addDatabase("QSQLITE", connection_name);
    _db.setDatabaseName(_db_file);
    _db.open();
    
    QSqlQuery query(_db);
    
    QVector<QVector<long>> table_results;
    
    for (int i = 0; i < _num_rows; i++)
    {
      table_results.append(QVector<long>());
      for (int j = 0; j < _num_rows; j++)
      {
        table_results[i].append(0);
      }
    }
    
    if (_table_type == "step_forward")
    {
      if (!query.exec(_q))
      {
        emit error(QString("Error: failed to execute query:\n%1").arg(_q));
        return;
      }
      
      while (query.next())
      {
        // SELECT P1, P2, COUNT(id) FROM atl [WHERE...] GROUP BY P1, P2
        int i = qMin(query.value(0).toInt(), _num_rows - 1);
        int j = qMin(query.value(1).toInt(), _num_rows - 1);
        
        long long votes = query.value(2).toLongLong();
        
        table_results[i][j] = votes;
      }
    }
    else if (_table_type == "first_n_prefs")
    {
      query.setForwardOnly(true);
      
      if (!query.exec(_q))
      {
        emit error(QString("Error: failed to execute query:\n%1").arg(_q));
        return;
      }
      
      int n = _args.at(0);
      QVector<int> ignore_groups;
      for (int i = 1; i < _args.length(); i++)
      {
        ignore_groups.append(_args.at(i));
      }
      
      while (query.next())
      {
        // SELECT P1, P2, ..., Pn, num_prefs FROM atl [WHERE...]
        
        int num_prefs = query.value(n).toInt();
        int max_search = qMin(num_prefs, n);
        
        for (int i = 0; i < max_search; i++)
        {
          int p_i = query.value(i).toInt();
          if (ignore_groups.indexOf(p_i) < 0)
          {
            for (int j = i + 1; j < max_search; j++)
            {
              int p_j = query.value(j).toInt();
              
              if (ignore_groups.indexOf(p_j) < 0)
              {
                table_results[p_i][p_j] += 1;
                table_results[p_j][p_i] += 1;
              }
            }
            
            if (num_prefs < n)
            {
              table_results[_num_rows - 1][p_i] += 1;
              table_results[p_i][_num_rows - 1] += 1;
            }
          }
        }
      }
    }
    else if (_table_type == "later_prefs")
    {
      query.setForwardOnly(true);
      
      if (!query.exec(_q))
      {
        emit error(QString("Error: failed to execute query:\n%1").arg(_q));
        return;
      }
      
      int fixed = _args.at(0);
      int up_to = _args.at(1);
      
      QVector<int> ignore_groups;
      for (int i = 2; i < _args.length(); i++)
      {
        ignore_groups.append(_args.at(i));
      }
      
      int row_fixed_pref = qMax(1, ignore_groups.length() + 1);
      int col_fixed_pref = row_fixed_pref + 1;
      
      bool fixed_row = row_fixed_pref <= fixed;
      bool fixed_col = col_fixed_pref <= fixed;
      
      while (query.next())
      {
        // SELECT P1, P2, ..., Pn, num_prefs FROM atl [WHERE...]
        
        int p_i, p_j;
        
        if (fixed_row)
        {
          p_i = query.value(row_fixed_pref - 1).toInt();
          
          if (p_i < _num_rows - 1 && ignore_groups.indexOf(p_i) < 0)
          {
            if (fixed_col)
            {
              p_j = query.value(col_fixed_pref - 1).toInt();
              
              if (p_j < _num_rows - 1 && ignore_groups.indexOf(p_j) < 0)
              {
                table_results[p_i][p_j] += 1;
              }
              
              if (p_j >= _num_rows)
              {
                table_results[p_i][_num_rows - 1] += 1;
              }
            }
            else
            {
              int num_prefs = query.value(up_to).toInt();
              int max_search = qMin(num_prefs, up_to);
              
              for (int i = row_fixed_pref; i < max_search; i++)
              {
                p_j = query.value(i).toInt();
                if (ignore_groups.indexOf(p_j) < 0)
                {
                  table_results[p_i][p_j] += 1;
                }
              }
              
              if (num_prefs < up_to)
              {
                table_results[p_i][_num_rows - 1] += 1;
              }
            }
          }
        }
        else
        {
          // !fixed_row, i.e., the row isn't a fixed preference, it's a "by N".
          int num_prefs = query.value(up_to).toInt();
          int max_search = qMin(num_prefs, up_to);
          
          for (int i = 0; i < max_search; i++)
          {
            int p_i = query.value(i).toInt();
            if (ignore_groups.indexOf(p_i) < 0)
            {
              for (int j = i + 1; j < max_search; j++)
              {
                int p_j = query.value(j).toInt();
                
                if (ignore_groups.indexOf(p_j) < 0)
                {
                  table_results[p_i][p_j] += 1;
                  table_results[p_j][p_i] += 1;
                }
              }
              
              if (num_prefs < up_to)
              {
                table_results[_num_rows - 1][p_i] += 1;
                table_results[p_i][_num_rows - 1] += 1;
              }
            }
          }
        }
      }
    }
    else if (_table_type == "pref_sources")
    {
      query.setForwardOnly(true);
      
      if (!query.exec(_q))
      {
        emit error(QString("Error: failed to execute query:\n%1").arg(_q));
        return;
      }
      
      int pref_min = _args.at(0);
      int pref_max = _args.at(1);
      
      int num_later_prefs = pref_max - pref_min + 1;
      int n = 1 + num_later_prefs;
      
      while (query.next())
      {
        // SELECT P1, P4, P5, P6, num_prefs FROM atl [WHERE...]
        
        int p_j = query.value(0).toInt();
        int num_prefs = query.value(n).toInt();
        int max_search = qMin(num_prefs, pref_max) - pref_min + 1;
        
        for (int i = 0; i < max_search; i++)
        {
          int p_i = query.value(i + 1).toInt();
          table_results[p_i][p_j] += 1;
        }
        
        if ((num_prefs >= pref_min - 1) && (num_prefs < pref_max))
        {
          table_results[_num_rows - 1][p_j] += 1;
        }
      }
    }
    
    _db.close();
    emit finished_query(table_results);
  }
  
  QSqlDatabase::removeDatabase(connection_name);
}
