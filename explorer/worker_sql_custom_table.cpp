#include "worker_sql_custom_table.h"
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

#include "custom_operation.h"

Worker_sql_custom_table::Worker_sql_custom_table(int thread_num,
                                                 const QString& db_file,
                                                 const QString& q,
                                                 int num_groups,
                                                 int num_booths,
                                                 std::vector<int>& axis_numbers,
                                                 std::vector<int>& row_stack_indices,
                                                 std::vector<int>& col_stack_indices,
                                                 int max_loop_index,
                                                 std::vector<std::vector<int>>& aggregated_indices,
                                                 std::vector<Custom_operation>& filter_operations,
                                                 std::vector<Custom_operation>& row_operations,
                                                 std::vector<Custom_operation>& col_operations,
                                                 std::vector<Custom_operation>& cell_operations)
  : _thread_num(thread_num)
  , _db_file(db_file)
  , _q(q)
  , _num_groups(num_groups)
  , _num_booths(num_booths)
  , _axis_numbers(axis_numbers)
  , _row_stack_indices(row_stack_indices)
  , _col_stack_indices(col_stack_indices)
  , _max_loop_index(max_loop_index)
  , _aggregated_indices(aggregated_indices)
  , _have_aggregated(_aggregated_indices.size() > 0)
  , _filter_operations(filter_operations)
  , _row_operations(row_operations)
  , _col_operations(col_operations)
  , _cell_operations(cell_operations)
{
  // These routines were originally written for two-axis tables, and a dummy
  // row or column is added if necessary.
  //
  // If it works with aggregated indices (which use negative integers), then
  // it is by good fortune rather than design.  But I think it does work:
  // when the {_row/_col}_stack_indices is empty, then the corresponding ROW
  // or COL identifier cannot appear in the cell definition, so the dummy
  // value placed in these vectors should not be read.
  if (_row_stack_indices.empty())
  {
    _row_stack_indices.push_back(-1);
  }

  if (_col_stack_indices.empty())
  {
    _col_stack_indices.push_back(-1);
  }
}

Worker_sql_custom_table::~Worker_sql_custom_table() {}

void Worker_sql_custom_table::do_query()
{
  QString connection_name = QString("db_conn_%1").arg(_thread_num);

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

    QVector<QVector<int>> table_results;
    QVector<int> row_bases;
    int total_base = 0;

    const int num_rows = _row_stack_indices.size();
    const int num_cols = _col_stack_indices.size();

    for (int i = 0; i < num_rows; i++)
    {
      table_results.append(QVector<int>());
      row_bases.append(0);
      for (int j = 0; j < num_cols; j++)
      {
        table_results[i].append(0);
      }
    }

    const int n_filter_operations = _filter_operations.size();
    const int n_row_operations    = _row_operations.size();
    const int n_col_operations    = _col_operations.size();
    const int n_cell_operations   = _cell_operations.size();

    const bool have_row = n_row_operations > 0;
    const bool have_col = n_col_operations > 0;

    int max_stack_index            = 2 * _num_groups + 1 + _axis_numbers.size();
    const int stack_index_int_eval = max_stack_index + 1;

    // i and j will be the looping variables over the axes of the table.
    // They are defined here so that I can use pointers to them for
    // Row and Col indices in the operations vector.
    int i = 0;
    int j = 0;

    std::vector<std::vector<const int*>> ptr_indices_filter;
    std::vector<std::vector<const int*>> ptr_indices_row;
    std::vector<std::vector<const int*>> ptr_indices_col;
    std::vector<std::vector<const int*>> ptr_indices_cell;

    Custom_operations::setup_ptr_indices(max_stack_index, i, j, ptr_indices_filter, _filter_operations);
    Custom_operations::setup_ptr_indices(max_stack_index, i, j, ptr_indices_row,    _row_operations);
    Custom_operations::setup_ptr_indices(max_stack_index, i, j, ptr_indices_col,    _col_operations);
    Custom_operations::setup_ptr_indices(max_stack_index, i, j, ptr_indices_cell,   _cell_operations);

    if (_have_aggregated)
    {
      Custom_operations::setup_aggregated_ptr_indices(i, j, _filter_operations);
      Custom_operations::setup_aggregated_ptr_indices(i, j, _row_operations);
      Custom_operations::setup_aggregated_ptr_indices(i, j, _col_operations);
      Custom_operations::setup_aggregated_ptr_indices(i, j, _cell_operations);
    }

    // uint8_t is much faster than bool;
    // using std::array does not noticeably help performance.
    std::vector<uint8_t> stack_boolean(max_stack_index + 1);
    std::vector<int> stack_integer(max_stack_index + 1);

    // The axis numbers are fixed, so can be set prior to reading
    // any query results.
    int stack_ct = 2 * _num_groups + 2;
    for (int axis_num : _axis_numbers)
    {
      stack_integer[stack_ct] = axis_num;
      stack_ct++;
    }

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

    std::function<int(std::vector<std::vector<const int*>>&, std::vector<Custom_operation>&, int, std::vector<int>&, int)>
      axis_table_index_without_aggregation =
        [this, &stack_boolean, &stack_integer, stack_index_int_eval](std::vector<std::vector<const int*>>& ptr_indices,
                                                                     std::vector<Custom_operation>& ops,
                                                                     int n_ops,
                                                                     std::vector<int>& axis_stack_indices,
                                                                     int n_axis_indices)
    {
      Custom_operations::process_vote(_num_groups, stack_boolean, stack_integer, ptr_indices, ops, n_ops);
      const int target = stack_integer.at(stack_index_int_eval);
      for (int i_loop = 0; i_loop < n_axis_indices; ++i_loop)
      {
        const int i = axis_stack_indices.at(i_loop);
        if (stack_integer.at(i) == target)
        {
          return i_loop;
        }
      }
      // Evaluated integer not one of the axis values:
      return -1;
    };

    std::function<int(std::vector<std::vector<const int*>>&, std::vector<Custom_operation>&, int, std::vector<int>&, int)>
      axis_table_index_with_aggregation =
        [this, &stack_boolean, &stack_integer, stack_index_int_eval, &stack_loops](std::vector<std::vector<const int*>>& ptr_indices,
                                                                                   std::vector<Custom_operation>& ops,
                                                                                   int n_ops,
                                                                                   std::vector<int>& axis_stack_indices,
                                                                                   int n_axis_indices)
    {
      Custom_operations::process_vote_with_aggregation(_num_groups,
                                                       stack_boolean,
                                                       stack_integer,
                                                       ptr_indices,
                                                       ops,
                                                       n_ops,
                                                       _aggregated_indices,
                                                       stack_loops);

      const int target = stack_integer.at(stack_index_int_eval);
      for (int i_loop = 0; i_loop < n_axis_indices; ++i_loop)
      {
        const int i = axis_stack_indices.at(i_loop);
        if (i >= 0)
        {
          if (stack_integer.at(i) == target)
          {
            return i_loop;
          }
          else
          {
            continue;
          }
        }

        const int agg_i = Custom_operations::aggregated_index_to_from_negative(i);
        for (int input_index : _aggregated_indices.at(agg_i))
        {
          if (stack_integer.at(input_index) == target)
          {
            return i_loop;
          }
        }
      }
      // Evaluated integer not one of the axis values:
      return -1;
    };

    auto& process_vote     = _have_aggregated ? process_vote_with_aggregation : process_vote_without_aggregation;
    auto& axis_table_index = _have_aggregated ? axis_table_index_with_aggregation : axis_table_index_without_aggregation;

    while (query.next())
    {
      // SELECT booth_id, Pfor0, Pfor1, ..., Pfor(N-1), num_prefs, num_prefs, P1, P2, ..., PN FROM atl
      // Stack:           Pfor0, Pfor1, ..., Pfor(N-1), Exh,       num_prefs, P1, P2, ..., PN
      for (int iv = 0; iv < 2 * _num_groups + 2; ++iv)
      {
        stack_integer[iv] = query.value(iv + 1).toInt();
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

      total_base++;

      if (have_row && have_col)
      {
        const int i_loop = axis_table_index(ptr_indices_row, _row_operations, n_row_operations, _row_stack_indices, num_rows);
        if (i_loop < 0)
        {
          continue;
        }

        const int j_loop = axis_table_index(ptr_indices_col, _col_operations, n_col_operations, _col_stack_indices, num_cols);
        if (j_loop < 0)
        {
          continue;
        }

        row_bases[i_loop]++;
        table_results[i_loop][j_loop]++;
      }
      else if (have_row && !have_col)
      {
        const int i_loop = axis_table_index(ptr_indices_row, _row_operations, n_row_operations, _row_stack_indices, num_rows);
        if (i_loop < 0)
        {
          continue;
        }

        bool include_in_row_base = false;
        for (int j_loop = 0; j_loop < num_cols; ++j_loop)
        {
          j = _col_stack_indices.at(j_loop);
          Q_UNUSED(j); // eliminate a false-positive static-analyser warning -- j is used in ptr_indices
          process_vote(ptr_indices_cell, _cell_operations, n_cell_operations);
          if (stack_boolean[0])
          {
            table_results[i_loop][j_loop]++;
            include_in_row_base = true;
          }
        }
        if (include_in_row_base)
        {
          row_bases[i_loop]++;
        }
      }
      else if (!have_row && have_col)
      {
        const int j_loop = axis_table_index(ptr_indices_col, _col_operations, n_col_operations, _col_stack_indices, num_cols);
        if (j_loop < 0)
        {
          continue;
        }

        for (int i_loop = 0; i_loop < num_rows; ++i_loop)
        {
          i = _row_stack_indices.at(i_loop);
          Q_UNUSED(i); // eliminate a false-positive static-analyser warning -- i is used in ptr_indices
          process_vote(ptr_indices_cell, _cell_operations, n_cell_operations);
          if (stack_boolean[0])
          {
            row_bases[i_loop]++;
            table_results[i_loop][j_loop]++;
          }
        }
      }
      else
      {
        for (int i_loop = 0; i_loop < num_rows; ++i_loop)
        {
          i = _row_stack_indices.at(i_loop);
          Q_UNUSED(i); // eliminate a false-positive static-analyser warning -- i is used in ptr_indices
          bool include_in_row_base = false;
          for (int j_loop = 0; j_loop < num_cols; ++j_loop)
          {
            j = _col_stack_indices.at(j_loop);
            Q_UNUSED(j); // eliminate a false-positive static-analyser warning -- j is used in ptr_indices
            process_vote(ptr_indices_cell, _cell_operations, n_cell_operations);
            if (stack_boolean[0])
            {
              table_results[i_loop][j_loop]++;
              include_in_row_base = true;
            }
          }
          if (include_in_row_base)
          {
            row_bases[i_loop]++;
          }
        }
      }
    }

    _db.close();
    emit finished_query(total_base, row_bases, table_results);
  }

  QSqlDatabase::removeDatabase(connection_name);
}

void Worker_sql_custom_table::do_query_by_booth()
{
  QString connection_name = QString("db_conn_%1").arg(_thread_num);

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

    QVector<QVector<QVector<int>>> table_results;
    QVector<QVector<int>> row_bases;
    QVector<int> total_base;

    const int num_rows = _row_stack_indices.size();
    const int num_cols = _col_stack_indices.size();

    for (int i = 0; i < num_rows; i++)
    {
      table_results.append(QVector<QVector<int>>());
      for (int j = 0; j < num_cols; j++)
      {
        table_results[i].append(QVector<int>());
        for (int k = 0; k < _num_booths; k++)
        {
          table_results[i][j].append(0);
        }
      }

      row_bases.append(QVector<int>());
      for (int k = 0; k < _num_booths; k++)
      {
        row_bases[i].append(0);
      }
    }

    for (int k = 0; k < _num_booths; k++)
    {
      total_base.append(0);
    }

    const int n_filter_operations = _filter_operations.size();
    const int n_row_operations    = _row_operations.size();
    const int n_col_operations    = _col_operations.size();
    const int n_cell_operations   = _cell_operations.size();

    const bool have_row = n_row_operations > 0;
    const bool have_col = n_col_operations > 0;

    int max_stack_index            = 2 * _num_groups + 1 + _axis_numbers.size();
    const int stack_index_int_eval = max_stack_index + 1;

    // i and j will be the looping variables over the axes of the table.
    // They are defined here so that I can use pointers to them for
    // Row and Col indices in the operations vector.
    int i = 0;
    int j = 0;

    std::vector<std::vector<const int*>> ptr_indices_filter;
    std::vector<std::vector<const int*>> ptr_indices_row;
    std::vector<std::vector<const int*>> ptr_indices_col;
    std::vector<std::vector<const int*>> ptr_indices_cell;

    Custom_operations::setup_ptr_indices(max_stack_index, i, j, ptr_indices_filter, _filter_operations);
    Custom_operations::setup_ptr_indices(max_stack_index, i, j, ptr_indices_row,    _row_operations);
    Custom_operations::setup_ptr_indices(max_stack_index, i, j, ptr_indices_col,    _col_operations);
    Custom_operations::setup_ptr_indices(max_stack_index, i, j, ptr_indices_cell,   _cell_operations);

    if (_have_aggregated)
    {
      Custom_operations::setup_aggregated_ptr_indices(i, j, _filter_operations);
      Custom_operations::setup_aggregated_ptr_indices(i, j, _row_operations);
      Custom_operations::setup_aggregated_ptr_indices(i, j, _col_operations);
      Custom_operations::setup_aggregated_ptr_indices(i, j, _cell_operations);
    }

    // uint8_t is much faster than bool;
    // using std::array does not noticeably help performance.
    std::vector<uint8_t> stack_boolean(max_stack_index + 1);
    std::vector<int> stack_integer(max_stack_index + 1);

    // The axis numbers are fixed, so can be set prior to reading
    // any query results.
    int stack_ct = 2 * _num_groups + 2;
    for (int axis_num : _axis_numbers)
    {
      stack_integer[stack_ct] = axis_num;
      stack_ct++;
    }

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

    std::function<int(std::vector<std::vector<const int*>>&, std::vector<Custom_operation>&, int, std::vector<int>&, int)>
      axis_table_index_without_aggregation =
        [this, &stack_boolean, &stack_integer, stack_index_int_eval](std::vector<std::vector<const int*>>& ptr_indices,
                                                                     std::vector<Custom_operation>& ops,
                                                                     int n_ops,
                                                                     std::vector<int>& axis_stack_indices,
                                                                     int n_axis_indices)
    {
      Custom_operations::process_vote(_num_groups, stack_boolean, stack_integer, ptr_indices, ops, n_ops);
      const int target = stack_integer.at(stack_index_int_eval);
      for (int i_loop = 0; i_loop < n_axis_indices; ++i_loop)
      {
        const int i = axis_stack_indices.at(i_loop);
        if (stack_integer.at(i) == target)
        {
          return i_loop;
        }
      }
      // Evaluated integer not one of the axis values:
      return -1;
    };

    std::function<int(std::vector<std::vector<const int*>>&, std::vector<Custom_operation>&, int, std::vector<int>&, int)>
      axis_table_index_with_aggregation =
        [this, &stack_boolean, &stack_integer, stack_index_int_eval, &stack_loops](std::vector<std::vector<const int*>>& ptr_indices,
                                                                                   std::vector<Custom_operation>& ops,
                                                                                   int n_ops,
                                                                                   std::vector<int>& axis_stack_indices,
                                                                                   int n_axis_indices)
    {
      Custom_operations::process_vote_with_aggregation(_num_groups,
                                                       stack_boolean,
                                                       stack_integer,
                                                       ptr_indices,
                                                       ops,
                                                       n_ops,
                                                       _aggregated_indices,
                                                       stack_loops);

      const int target = stack_integer.at(stack_index_int_eval);
      for (int i_loop = 0; i_loop < n_axis_indices; ++i_loop)
      {
        const int i = axis_stack_indices.at(i_loop);
        if (i >= 0)
        {
          if (stack_integer.at(i) == target)
          {
            return i_loop;
          }
          else
          {
            continue;
          }
        }

        const int agg_i = Custom_operations::aggregated_index_to_from_negative(i);
        for (int input_index : _aggregated_indices.at(agg_i))
        {
          if (stack_integer.at(input_index) == target)
          {
            return i_loop;
          }
        }
      }
      // Evaluated integer not one of the axis values:
      return -1;
    };

    auto& process_vote     = _have_aggregated ? process_vote_with_aggregation : process_vote_without_aggregation;
    auto& axis_table_index = _have_aggregated ? axis_table_index_with_aggregation : axis_table_index_without_aggregation;

    while (query.next())
    {
      // SELECT booth_id, Pfor0, Pfor1, ..., Pfor(N-1), num_prefs, num_prefs, P1, P2, ..., PN FROM atl
      // Stack:           Pfor0, Pfor1, ..., Pfor(N-1), Exh,       num_prefs, P1, P2, ..., PN
      const int booth_id = query.value(0).toInt();

      for (int iv = 0; iv < 2 * _num_groups + 2; ++iv)
      {
        stack_integer[iv] = query.value(iv + 1).toInt();
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

      total_base[booth_id]++;

      if (have_row && have_col)
      {
        const int i_loop = axis_table_index(ptr_indices_row, _row_operations, n_row_operations, _row_stack_indices, num_rows);
        if (i_loop < 0)
        {
          continue;
        }

        const int j_loop = axis_table_index(ptr_indices_col, _col_operations, n_col_operations, _col_stack_indices, num_cols);
        if (j_loop < 0)
        {
          continue;
        }

        row_bases[i_loop][booth_id]++;
        table_results[i_loop][j_loop][booth_id]++;
      }
      else if (have_row && !have_col)
      {
        const int i_loop = axis_table_index(ptr_indices_row, _row_operations, n_row_operations, _row_stack_indices, num_rows);
        if (i_loop < 0)
        {
          continue;
        }

        bool include_in_row_base = false;
        for (int j_loop = 0; j_loop < num_cols; ++j_loop)
        {
          j = _col_stack_indices.at(j_loop);
          Q_UNUSED(j); // eliminate a false-positive static-analyser warning -- j is used in ptr_indices
          process_vote(ptr_indices_cell, _cell_operations, n_cell_operations);
          if (stack_boolean[0])
          {
            table_results[i_loop][j_loop][booth_id]++;
            include_in_row_base = true;
          }
        }
        if (include_in_row_base)
        {
          row_bases[i_loop][booth_id]++;
        }
      }
      else if (!have_row && have_col)
      {
        const int j_loop = axis_table_index(ptr_indices_col, _col_operations, n_col_operations, _col_stack_indices, num_cols);
        if (j_loop < 0)
        {
          continue;
        }

        for (int i_loop = 0; i_loop < num_rows; ++i_loop)
        {
          i = _row_stack_indices.at(i_loop);
          Q_UNUSED(i); // eliminate a false-positive static-analyser warning -- i is used in ptr_indices
          process_vote(ptr_indices_cell, _cell_operations, n_cell_operations);
          if (stack_boolean[0])
          {
            row_bases[i_loop][booth_id]++;
            table_results[i_loop][j_loop][booth_id]++;
          }
        }
      }
      else
      {
        for (int i_loop = 0; i_loop < num_rows; ++i_loop)
        {
          i = _row_stack_indices.at(i_loop);
          Q_UNUSED(i); // eliminate a false-positive static-analyser warning -- i is used in ptr_indices
          bool include_in_row_base = false;
          for (int j_loop = 0; j_loop < num_cols; ++j_loop)
          {
            j = _col_stack_indices.at(j_loop);
            Q_UNUSED(j); // eliminate a false-positive static-analyser warning -- j is used in ptr_indices
            process_vote(ptr_indices_cell, _cell_operations, n_cell_operations);
            if (stack_boolean[0])
            {
              table_results[i_loop][j_loop][booth_id]++;
              include_in_row_base = true;
            }
          }
          if (include_in_row_base)
          {
            row_bases[i_loop][booth_id]++;
          }
        }
      }
    }

    _db.close();
    emit finished_query_by_booth(total_base, row_bases, table_results);
  }

  QSqlDatabase::removeDatabase(connection_name);
}
