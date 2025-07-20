#include "worker_sql_custom_every_expr.h"
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

#include "custom_operation.h"

Worker_sql_custom_every_expr::Worker_sql_custom_every_expr(const QString& db_file,
                                                           int axis,
                                                           int thread_num,
                                                           const QString& q,
                                                           int num_groups,
                                                           int max_loop_index,
                                                           std::vector<std::vector<int>>& aggregated_indices,
                                                           std::vector<Custom_operation>& filter_operations,
                                                           std::vector<Custom_operation>& cell_operations)
  : _db_file(db_file)
  , _axis(axis)
  , _thread_num(thread_num)
  , _q(q)
  , _num_groups(num_groups)
  , _max_loop_index(max_loop_index)
  , _aggregated_indices(aggregated_indices)
  , _have_aggregated(_aggregated_indices.size() > 0)
  , _filter_operations(filter_operations)
  , _cell_operations(cell_operations)
{
}

Worker_sql_custom_every_expr::~Worker_sql_custom_every_expr() {}

void Worker_sql_custom_every_expr::do_query_operations()
{
  // The integer stack will not contain any axis numbers -- the all(expr)
  // is being evaluated here to determine what these numbers need to be.
  // Therefore the integer stack only is initialised to 2*n + 2 entries
  // (Pfor's, Exh, num_prefs, P's) and the output integer value is in
  // index 2*n + 2.

  QString connection_name = QString("db_conn_all_%1").arg(_thread_num);

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

    QHash<int, uint8_t> unique_values;

    const int n_cell_operations   = _cell_operations.size();
    const int n_filter_operations = _filter_operations.size();

    int max_stack_index                 = 2 * _num_groups + 1;
    const int final_integer_stack_index = 2 * _num_groups + 2;

    // Not used, but required because the Custom_operations routines
    // handle looping over rows and columns.
    int i = 0;
    int j = 0;

    std::vector<std::vector<const int*>> ptr_indices_filter;
    std::vector<std::vector<const int*>> ptr_indices_cell;

    Custom_operations::setup_ptr_indices(max_stack_index, i, j, ptr_indices_filter, _filter_operations);
    Custom_operations::setup_ptr_indices(max_stack_index, i, j, ptr_indices_cell, _cell_operations);

    if (_have_aggregated)
    {
      Custom_operations::setup_aggregated_ptr_indices(i, j, _filter_operations);
      Custom_operations::setup_aggregated_ptr_indices(i, j, _cell_operations);
    }

    // uint8_t is much faster than bool;
    // using std::array does not noticeably help performance.
    std::vector<uint8_t> stack_boolean(max_stack_index + 1);
    std::vector<int> stack_integer(max_stack_index + 1);

    // Important to initialise to -1, sorry.
    std::vector<int> stack_loops(_max_loop_index + 1, -1);

    std::function<void(std::vector<std::vector<const int*>>&, std::vector<Custom_operation>&, int)> process_vote_without_aggregation =
      [&, this](std::vector<std::vector<const int*>>& ptr_indices, std::vector<Custom_operation>& ops, int n_ops)
    { Custom_operations::process_vote(_num_groups, stack_boolean, stack_integer, ptr_indices, ops, n_ops); };

    std::function<void(std::vector<std::vector<const int*>>&, std::vector<Custom_operation>&, int)> process_vote_with_aggregation =
      [&, this](std::vector<std::vector<const int*>>& ptr_indices, std::vector<Custom_operation>& ops, int n_ops)
    {
      Custom_operations::process_vote_with_aggregation(_num_groups,
                                                       stack_boolean,
                                                       stack_integer,
                                                       ptr_indices,
                                                       ops,
                                                       n_ops,
                                                       _aggregated_indices,
                                                       stack_loops);
    };

    auto& process_vote = _have_aggregated ? process_vote_with_aggregation : process_vote_without_aggregation;

    while (query.next())
    {
      // SELECT Pfor0, Pfor1, ..., Pfor(N-1), num_prefs, num_prefs, P1, P2, ..., PN FROM atl
      // Stack: Pfor0, Pfor1, ..., Pfor(N-1), Exh, num_prefs, P1, P2, ..., PN
      for (int iv = 0; iv < 2 * _num_groups + 2; ++iv)
      {
        stack_integer[iv] = query.value(iv).toInt();
      }

      // "Preference number" for exhaust:
      if (stack_integer[_num_groups] == _num_groups)
      {
        stack_integer[_num_groups] = 999;
      }
      else
      {
        stack_integer[_num_groups]++;
      }

      // Initialise to true in case the filter is empty
      stack_boolean[0] = true;

      process_vote(ptr_indices_filter, _filter_operations, n_filter_operations);
      if (!stack_boolean[0])
      {
        continue;
      }

      process_vote(ptr_indices_cell, _cell_operations, n_cell_operations);
      const int value = stack_integer.at(final_integer_stack_index);
      if (!unique_values.contains(value))
      {
        unique_values.insert(value, 1);
      }
    }

    QVector<int> values(unique_values.keys());
    std::sort(values.begin(), values.end());

    _db.close();
    emit finished_query(_axis, values);
  }

  QSqlDatabase::removeDatabase(connection_name);
}

void Worker_sql_custom_every_expr::do_query_pure_sql()
{
  QString connection_name = QString("db_conn_all_pure_sql_%1").arg(_axis);

  {
    // *** Should add error handling ***
    QSqlDatabase _db = QSqlDatabase::addDatabase("QSQLITE", connection_name);
    _db.setDatabaseName(_db_file);
    _db.open();

    QSqlQuery query(_db);

    if (!query.exec(_q))
    {
      emit error(QString("Error: failed to execute query:\n%1").arg(_q));
      qDebug() << "Didn't execute SQL" << _q;
      return;
    }

    QVector<int> values;
    while (query.next())
    {
      // SELECT DISTINCT (all_expr) as v FROM atl WHERE (filter_expr) ORDER BY v
      values.append(query.value(0).toInt());
    }

    _db.close();
    emit finished_query(_axis, values);
  }

  QSqlDatabase::removeDatabase(connection_name);
}
