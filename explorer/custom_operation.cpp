#include "custom_operation.h"
#include "custom_expr.h"
#include "main_widget.h"

namespace Custom_identifiers
{
  const QString ROW     = "row";
  const QString COL     = "col";
  const QString N_PREFS = "n_prefs";
  const QString N_MAX   = "n_max";
} // namespace Custom_identifiers

namespace Custom_axis_names
{
  const QString GROUPS     = "groups";
  const QString CANDIDATES = "candidates";
} // namespace Custom_axis_names

namespace Custom_operations
{
  const QString op_name(Custom_op_type op_type)
  {
    switch (op_type)
    {
    case Custom_op_type::TRUE_LITERAL:
      return "True literal";
    case Custom_op_type::INT_LITERAL:
      return "Int literal";
    case Custom_op_type::IDENTIFIER:
      return "Identifier";
    case Custom_op_type::BARE_AGG_IDENTIFIER:
      return "Agg. identifier";
    case Custom_op_type::NUM_CANDS:
      return "Number of candidates";
    case Custom_op_type::INDEX:
      return "Index";
    case Custom_op_type::EQ:
      return "Equals";
    case Custom_op_type::NEQ:
      return "Not-equal";
    case Custom_op_type::LT:
      return "Less than";
    case Custom_op_type::LTE:
      return "Less than or equal to";
    case Custom_op_type::GT:
      return "Greater than";
    case Custom_op_type::GTE:
      return "Greater than or equal to";
    case Custom_op_type::IN_RANGE:
      return "In-range";
    case Custom_op_type::NOT:
      return "Not";
    case Custom_op_type::AND:
      return "And";
    case Custom_op_type::OR:
      return "Or";
    case Custom_op_type::ADD:
      return "Add";
    case Custom_op_type::SUB:
      return "Subtract";
    case Custom_op_type::MIN:
      return "Min";
    case Custom_op_type::MAX:
      return "Max";
    case Custom_op_type::ABS:
      return "Abs";
    case Custom_op_type::IF:
      return "If";
    case Custom_op_type::PREF_INDEX:
      return "Pref index";
    case Custom_op_type::JMP:
      return "Jump";
    case Custom_op_type::JMP_IF_TRUE:
      return "Jump if true";
    case Custom_op_type::JMP_IF_FALSE:
      return "Jump if false";
    case Custom_op_type::BREAK_IF_TRUE:
      return "Break if true";
    case Custom_op_type::BREAK_IF_FALSE:
      return "Break if false";
    case Custom_op_type::NPP_PREF:
      return "NPP preference";
    case Custom_op_type::ANY:
      return "Any";
    case Custom_op_type::ALL:
      return "All";
    }
    return "Unknown custom operation";
  }

  void require_n_params(const Custom_expr* expr, int (Custom_expr::*get_n_method)() const, int n)
  {
    if ((expr->*get_n_method)() != n)
    {
      const QString name = Custom_operations::op_name(expr->get_op_type());
      const QString msg
        = QString("Programmer error: '%1' expression not defined correctly.  Sorry :(").arg(name);
      throw std::runtime_error(msg.toStdString());
    }
  }

  // In BTL with aggregated groups, an aggregated group identifier is stored
  // as -10 minus the group index; this transformation is its own inverse.
  int aggregated_index_to_from_negative(int i)
  {
    return -(10 + i);
  }

  void create_operations(Widget* w,
                         const Custom_expr* parent,
                         const Custom_expr* expr,
                         std::vector<Custom_operation>& operations,
                         int& index_stack_boolean,
                         int& index_stack_integer,
                         bool row_is_aggregated,
                         bool col_is_aggregated,
                         int& i_loop,
                         int depth)
  {
    if (depth == 0 && i_loop != -1)
    {
      throw std::runtime_error(
        "Programmer error: First call to create_operations() should have i_loop = -1");
    }

    if (expr == nullptr)
    {
      return;
    }

    auto parent_is_min_max = [](const Custom_expr* parent)
    {
      if (parent)
      {
        const Custom_op_type op_type = parent->get_op_type();
        if (op_type == Custom_op_type::MIN || op_type == Custom_op_type::MAX)
        {
          return true;
        }
      }
      return false;
    };

    auto recurse = [&](const Custom_expr* this_expr, const Custom_expr* next_expr)
    {
      create_operations(w,
                        this_expr,
                        next_expr,
                        operations,
                        index_stack_boolean,
                        index_stack_integer,
                        row_is_aggregated,
                        col_is_aggregated,
                        i_loop,
                        depth + 1);
    };

    Custom_operation op;
    op.op_type           = expr->get_op_type();
    const int num_groups = w->get_num_groups();
    const bool is_atl    = w->get_abtl() == "atl";

    switch (op.op_type)
    {
    case Custom_op_type::TRUE_LITERAL:
      op.output_index = index_stack_boolean;
      operations.push_back(op);
      index_stack_boolean++;
      return;
    case Custom_op_type::INT_LITERAL:
      require_n_params(expr, &Custom_expr::get_num_int_literals, 1);
      op.output_index = index_stack_integer;
      index_stack_integer++;
      op.int_literal = expr->get_int_literal(0);
      operations.push_back(op);
      return;
    case Custom_op_type::IDENTIFIER:
    {
      const QString identifier_name = expr->get_name();
      op.output_index               = index_stack_integer;
      index_stack_integer++;
      if (identifier_name == Custom_identifiers::N_PREFS)
      {
        // Read num_prefs from the SQL query output at index num_groups + 1
        op.input_indices.push_back(num_groups + 1);
        operations.push_back(op);
        return;
      }
      else if (identifier_name == Custom_identifiers::ROW)
      {
        op.input_indices.push_back(Custom_row_col::ROW);
        if (row_is_aggregated && parent_is_min_max(parent))
        {
          op.op_type = Custom_op_type::BARE_AGG_IDENTIFIER;
        }
        operations.push_back(op);
        return;
      }
      else if (identifier_name == Custom_identifiers::COL)
      {
        op.input_indices.push_back(Custom_row_col::COL);
        if (col_is_aggregated && parent_is_min_max(parent))
        {
          op.op_type = Custom_op_type::BARE_AGG_IDENTIFIER;
        }
        operations.push_back(op);
        return;
      }
      else if (identifier_name == "nc_" + Custom_identifiers::ROW)
      {
        op.input_indices.push_back(Custom_row_col::ROW);
        op.op_type = Custom_op_type::NUM_CANDS;
        operations.push_back(op);
        return;
      }
      else if (identifier_name == "nc_" + Custom_identifiers::COL)
      {
        op.input_indices.push_back(Custom_row_col::COL);
        op.op_type = Custom_op_type::NUM_CANDS;
        operations.push_back(op);
        return;
      }
      else if (identifier_name == "id_" + Custom_identifiers::ROW)
      {
        op.input_indices.push_back(Custom_row_col::ROW);
        op.op_type = Custom_op_type::INDEX;
        operations.push_back(op);
        return;
      }
      else if (identifier_name == "id_" + Custom_identifiers::COL)
      {
        op.input_indices.push_back(Custom_row_col::COL);
        op.op_type = Custom_op_type::INDEX;
        operations.push_back(op);
        return;
      }
      else
      {
        const int group = w->get_group_from_short(identifier_name);
        if (is_atl)
        {
          if (group < 0)
          {
            const QString msg = QString("Unrecognised group: %1").arg(identifier_name);
            throw std::runtime_error(msg.toStdString());
          }
          op.input_indices.push_back(group);
          operations.push_back(op);
          return;
        }

        // BTL
        const int cand = w->get_cand_from_short(identifier_name);
        if (cand < 0)
        {
          if (group >= 0)
          {
            op.input_indices.push_back(aggregated_index_to_from_negative(group));
            if (parent_is_min_max(parent))
            {
              op.op_type = Custom_op_type::BARE_AGG_IDENTIFIER;
            }
          }
          else
          {
            const QString msg = QString("Unrecognised candidate or group: %1").arg(identifier_name);
            throw std::runtime_error(msg.toStdString());
          }
        }
        else
        {
          op.input_indices.push_back(cand);
        }
        operations.push_back(op);
        return;
      }
    }
    case Custom_op_type::NOT:
      require_n_params(expr, &Custom_expr::get_num_arguments, 1);
      op.output_index = index_stack_boolean;
      index_stack_boolean++;
      op.input_indices.push_back(index_stack_boolean);
      recurse(expr, expr->get_argument(0));
      operations.push_back(op);
      return;
    case Custom_op_type::AND:
    case Custom_op_type::OR:
    {
      require_n_params(expr, &Custom_expr::get_num_arguments, 2);
      const int output_index = index_stack_boolean;
      op.output_index        = output_index;
      index_stack_boolean++;
      const int left_slot = index_stack_boolean;
      op.input_indices.push_back(left_slot);
      recurse(expr, expr->get_argument(0));

      // Short-circuit logical operations with a conditional jump
      Custom_operation jmp;
      jmp.output_index = output_index;
      jmp.op_type      = op.op_type == Custom_op_type::AND ? Custom_op_type::JMP_IF_FALSE
                                                           : Custom_op_type::JMP_IF_TRUE;
      jmp.input_indices.push_back(left_slot);
      const int jmp_pos = operations.size();
      operations.push_back(jmp);

      op.input_indices.push_back(index_stack_boolean);
      recurse(expr, expr->get_argument(1));
      operations.push_back(op);

      operations[jmp_pos].jump_to = operations.size();
      return;
    }
    case Custom_op_type::EQ:
    case Custom_op_type::NEQ:
    case Custom_op_type::GT:
    case Custom_op_type::GTE:
    case Custom_op_type::LT:
    case Custom_op_type::LTE:
      require_n_params(expr, &Custom_expr::get_num_arguments, 2);
      op.output_index = index_stack_boolean;
      index_stack_boolean++;
      op.input_indices.push_back(index_stack_integer);
      recurse(expr, expr->get_argument(0));
      op.input_indices.push_back(index_stack_integer);
      recurse(expr, expr->get_argument(1));
      operations.push_back(op);
      return;
    case Custom_op_type::IN_RANGE:
      require_n_params(expr, &Custom_expr::get_num_int_literals, 2);
      op.output_index = index_stack_boolean;
      index_stack_boolean++;

      op.range_lower = expr->get_int_literal(0);
      op.range_upper = expr->get_int_literal(1);
      op.input_indices.push_back(index_stack_integer);
      recurse(expr, expr->get_argument(0));
      operations.push_back(op);
      return;
    case Custom_op_type::ADD:
    case Custom_op_type::SUB:
      require_n_params(expr, &Custom_expr::get_num_arguments, 2);
      op.output_index = index_stack_integer;
      index_stack_integer++;
      op.input_indices.push_back(index_stack_integer);
      recurse(expr, expr->get_argument(0));
      op.input_indices.push_back(index_stack_integer);
      recurse(expr, expr->get_argument(1));
      operations.push_back(op);
      return;
    case Custom_op_type::ABS:
      require_n_params(expr, &Custom_expr::get_num_arguments, 1);
      op.output_index = index_stack_integer;
      index_stack_integer++;

      op.input_indices.push_back(index_stack_integer);
      recurse(expr, expr->get_argument(0));
      operations.push_back(op);
      return;
    case Custom_op_type::MIN:
    case Custom_op_type::MAX:
    {
      const int n_args = expr->get_num_arguments();
      if (n_args == 0)
      {
        const QString name = Custom_operations::op_name(expr->get_op_type());
        const QString msg
          = QString("Programmer error: '%1' expression not defined correctly.  Sorry :(").arg(name);
        throw std::runtime_error(msg.toStdString());
      }
      op.output_index = index_stack_integer;
      index_stack_integer++;

      for (int i = 0; i < n_args; ++i)
      {
        op.input_indices.push_back(index_stack_integer);
        recurse(expr, expr->get_argument(i));
      }
      operations.push_back(op);
      return;
    }
    case Custom_op_type::IF:
      require_n_params(expr, &Custom_expr::get_num_arguments, 3);
      op.output_index = index_stack_integer;
      index_stack_integer++;
      op.input_indices.push_back(index_stack_boolean);
      recurse(expr, expr->get_argument(0));
      op.input_indices.push_back(index_stack_integer);
      recurse(expr, expr->get_argument(1));
      op.input_indices.push_back(index_stack_integer);
      recurse(expr, expr->get_argument(2));
      operations.push_back(op);
      return;
    case Custom_op_type::PREF_INDEX:
      require_n_params(expr, &Custom_expr::get_num_arguments, 1);
      op.output_index = index_stack_integer;
      index_stack_integer++;
      op.input_indices.push_back(index_stack_integer);
      recurse(expr, expr->get_argument(0));
      operations.push_back(op);
      return;
    case Custom_op_type::ANY:
    case Custom_op_type::ALL:
    {
      require_n_params(expr, &Custom_expr::get_num_arguments, 1);
      // Assume that the only type of aggregation possible is groups of BTL candidates.
      // (Could do aggregated numbers in future?)
      if (w->get_abtl() == "atl")
      {
        throw std::runtime_error(
          "Programmer error: ANY or ALL node should only be possible below the line.  Sorry. :(");
      }

      // The ANY operations sequence has the following structure.
      //
      // for (int i = 0; i < aggregate_indices.size(); ++i)
      // {
      //    [operations]
      //    if (stack_boolean[output_index]) { break; }
      // }
      //
      // The ALL is the same but the break happens if !stack_boolean[0].
      // The initial ANY/ALL node will increment the loop counter and jump to
      // after the loop if it is too large.  We also need a
      // JMP_IF_{TRUE/FALSE} for breaking out of the loop.  Finally, an
      // unconditional JMP is used to loop back to the initial ANY/ALL node.

      const int i_op_start   = operations.size();
      const int output_index = index_stack_boolean;
      op.output_index        = output_index;
      index_stack_boolean++;
      const int loop_body_output_index = index_stack_boolean;

      i_loop++;
      op.loop_index       = i_loop;
      op.aggregated_index = expr->get_aggregated_index();
      operations.push_back(op);

      recurse(expr, expr->get_argument(0));

      const int i_op_terminate = operations.size();
      Custom_operation jmp_terminate_loop;
      jmp_terminate_loop.input_indices.push_back(loop_body_output_index);
      jmp_terminate_loop.output_index = output_index;
      jmp_terminate_loop.op_type      = op.op_type == Custom_op_type::ANY
                                          ? Custom_op_type::BREAK_IF_TRUE
                                          : Custom_op_type::BREAK_IF_FALSE;

      operations.push_back(jmp_terminate_loop);

      Custom_operation jmp_restart_loop;
      jmp_restart_loop.op_type = Custom_op_type::JMP;
      jmp_restart_loop.jump_to = i_op_start;

      operations.push_back(jmp_restart_loop);

      const int i_op_after               = operations.size();
      operations[i_op_start].jump_to     = i_op_after;
      operations[i_op_terminate].jump_to = i_op_after;

      i_loop--;
      return;
    }
    default:
      throw std::runtime_error("Programmer error: unhandled operation type in create_operations");
    }
  }

  void create_npp_operation(Widget* w,
                            std::vector<Custom_operation>& operations,
                            int& index_stack_integer,
                            QVector<int>& indices)
  {
    // The indices argument contains the n candidates or groups in the NPP;
    // aggregated groups BTL are negative indices.

    // Widget::get_num_groups() returns either the number of groups (ATL) or
    // number of candidates (BTL):
    const int num_entities = w->get_num_groups();
    Custom_operation op;
    op.op_type      = Custom_op_type::NPP_PREF;
    op.output_index = index_stack_integer;
    index_stack_integer++;
    for (int idx : indices)
    {
      op.input_indices.push_back(idx);
    }
    // Exhaust:
    op.input_indices.push_back(num_entities);
    operations.push_back(op);
  }

  QString operations_table_string(std::vector<Custom_operation>& operations)
  {
    QString debug("");
    debug += QString("  # ");
    debug += QString("Operation").leftJustified(25, ' ');
    debug += QString("Out").rightJustified(5, ' ');
    debug += QString("Int").rightJustified(6, ' ');
    debug += QString("Lo").rightJustified(5, ' ');
    debug += QString("Hi").rightJustified(5, ' ');
    debug += QString("Jmp").rightJustified(5, ' ');
    debug += QString("Loop").rightJustified(5, ' ');
    debug += QString("Agg").rightJustified(7, ' ');
    debug += " | ";
    debug += "Input\n";
    for (int i = 0, n = operations.size(); i < n; ++i)
    {
      Custom_operation op = operations.at(i);
      debug += QString::number(i).rightJustified(3, ' ') + " ";
      debug += Custom_operations::op_name(op.op_type).leftJustified(25, ' ');
      debug += QString::number(op.output_index).rightJustified(5, ' ') + ":";
      debug += QString::number(op.int_literal).rightJustified(5, ' ');
      debug += QString::number(op.range_lower).rightJustified(5, ' ');
      debug += QString::number(op.range_upper).rightJustified(5, ' ');
      debug += QString::number(op.jump_to).rightJustified(5, ' ');
      debug += QString::number(op.loop_index).rightJustified(5, ' ');
      debug += QString::number(op.aggregated_index).rightJustified(7, ' ');
      debug += " | ";
      for (int i : op.input_indices)
      {
        debug += QString::number(i).rightJustified(5, ' ');
      }
      debug += "\n";
    }
    return debug;
  }

  void update_max_loop_index(std::vector<Custom_operation>& operations, int& max_loop_index)
  {
    for (Custom_operation& op : operations)
    {
      max_loop_index = qMax(max_loop_index, op.loop_index);
    }
  }

  void setup_ptr_indices(int& max_stack_index,
                         const int& i,
                         const int& j,
                         std::vector<std::vector<const int*>>& ptr_indices,
                         std::vector<Custom_operation>& ops)
  {
    // By the end of this function, each element of the ptr_indices vector
    // is a vector of pointers to input stack indices used for the corresponding
    // operation in the ops vector.  Usually the pointers are to elements of
    // an operations input_indices, but when an element of the latter is a row
    // or a column, then the pointer is to &i or &j, the looping variables in
    // the caller.
    const int n_ops = ops.size();
    for (int i_op = 0; i_op < n_ops; ++i_op)
    {
      Custom_operation& op = ops.at(i_op);
      max_stack_index      = qMax(max_stack_index, op.output_index);

      const int n = op.input_indices.size();
      ptr_indices.push_back(std::vector<const int*>(n));
      for (int i_idx = 0; i_idx < n; ++i_idx)
      {
        const int input_index = op.input_indices.at(i_idx);
        max_stack_index       = qMax(max_stack_index, input_index);

        if (input_index == Custom_row_col::ROW)
        {
          ptr_indices[i_op][i_idx] = &i;
        }
        else if (input_index == Custom_row_col::COL)
        {
          ptr_indices[i_op][i_idx] = &j;
        }
        else
        {
          ptr_indices[i_op][i_idx] = &op.input_indices.at(i_idx);
        }
      }
    }
  }

  void setup_aggregated_ptr_indices(int& i, int& j, std::vector<Custom_operation>& ops)
  {
    const int n_ops = ops.size();
    for (int i_op = 0; i_op < n_ops; ++i_op)
    {
      const int agg_index = ops.at(i_op).aggregated_index;
      if (agg_index == Custom_row_col::ROW)
      {
        ops[i_op].ptr_aggregated_index = &i;
      }
      else if (agg_index == Custom_row_col::COL)
      {
        ops[i_op].ptr_aggregated_index = &j;
      }
      else
      {
        // Need the aggregated_index to be negative, for consistency with the pointers to &i and &j:
        // in the latter cases, the looping axis variables are looping over negative indices.
        ops.at(i_op).aggregated_index  = aggregated_index_to_from_negative(ops.at(i_op).aggregated_index);
        ops[i_op].ptr_aggregated_index = &ops.at(i_op).aggregated_index;
      }
    }
  }

  void process_vote(int num_groups,
                    std::vector<uint8_t>& stack_boolean,
                    std::vector<int>& stack_integer,
                    std::vector<std::vector<const int*>>& ptr_indices,
                    std::vector<Custom_operation>& ops,
                    int n_ops)
  {
    // Run the operations sequence; the caller will probably read
    // stack_boolean[0] or a specific stack_integer entry afterwards.
    for (int i_op = 0; i_op < n_ops; ++i_op)
    {
      Custom_operation& op                 = ops.at(i_op);
      const int n_indices                  = op.input_indices.size();
      std::vector<const int*>& use_indices = ptr_indices.at(i_op);

      switch (op.op_type)
      {
      case Custom_op_type::TRUE_LITERAL:
        stack_boolean[op.output_index] = true;
        break;
      case Custom_op_type::INT_LITERAL:
        stack_integer[op.output_index] = op.int_literal;
        break;
      case Custom_op_type::IDENTIFIER:
        stack_integer[op.output_index] = stack_integer[*use_indices[0]];
        break;
      case Custom_op_type::INDEX:
        stack_integer[op.output_index] = *use_indices[0] + 1;
        break;
      case Custom_op_type::EQ:
        stack_boolean[op.output_index] = stack_integer[*use_indices[0]] == stack_integer[*use_indices[1]];
        break;
      case Custom_op_type::NEQ:
        stack_boolean[op.output_index] = stack_integer[*use_indices[0]] != stack_integer[*use_indices[1]];
        break;
      case Custom_op_type::LT:
        stack_boolean[op.output_index] = stack_integer[*use_indices[0]] < stack_integer[*use_indices[1]];
        break;
      case Custom_op_type::LTE:
        stack_boolean[op.output_index] = stack_integer[*use_indices[0]] <= stack_integer[*use_indices[1]];
        break;
      case Custom_op_type::GT:
        stack_boolean[op.output_index] = stack_integer[*use_indices[0]] > stack_integer[*use_indices[1]];
        break;
      case Custom_op_type::GTE:
        stack_boolean[op.output_index] = stack_integer[*use_indices[0]] >= stack_integer[*use_indices[1]];
        break;
      case Custom_op_type::IN_RANGE:
        stack_boolean[op.output_index] = (stack_integer[*use_indices[0]] >= op.range_lower) && (stack_integer[*use_indices[0]] <= op.range_upper);
        break;
      case Custom_op_type::NOT:
        stack_boolean[op.output_index] = !stack_boolean[*use_indices[0]];
        break;
      case Custom_op_type::AND:
        stack_boolean[op.output_index] = stack_boolean[*use_indices[0]] && stack_boolean[*use_indices[1]];
        break;
      case Custom_op_type::OR:
        stack_boolean[op.output_index] = stack_boolean[*use_indices[0]] || stack_boolean[*use_indices[1]];
        break;
      case Custom_op_type::ADD:
        stack_integer[op.output_index] = stack_integer[*use_indices[0]] + stack_integer[*use_indices[1]];
        break;
      case Custom_op_type::SUB:
        stack_integer[op.output_index] = stack_integer[*use_indices[0]] - stack_integer[*use_indices[1]];
        break;
      case Custom_op_type::ABS:
        stack_integer[op.output_index] = abs(stack_integer[*use_indices[0]]);
        break;
      case Custom_op_type::IF:
        stack_integer[op.output_index] = stack_boolean[*use_indices[0]]
                                           ? stack_integer[*use_indices[1]]
                                           : stack_integer[*use_indices[2]];
        break;
      case Custom_op_type::MIN:
      case Custom_op_type::NPP_PREF:
      {
        // It looks bizarre to calculate the preference number given to the
        // N-preferred party instead of the index of the party itself, but
        // calculating the preference number allows this to be fit into the
        // framework of the row/col shortcuts in the worker, where it does a
        // single pass over the axis, looking for equality.
        //
        // The NPP peration is assumed to include 'Exhaust', whose
        // preference number is one more than the number of preferences
        // given (or 999 if all preferences are given).  This ensures that
        // there is a unique minimum preference number among those tested.
        int m = stack_integer[*use_indices[0]];
        for (int i_arg = 1; i_arg < n_indices; ++i_arg)
        {
          m = qMin(m, stack_integer[*use_indices[i_arg]]);
        }
        stack_integer[op.output_index] = m;
        break;
      }
      case Custom_op_type::MAX:
      {
        int m = stack_integer[*use_indices[0]];
        for (int i_arg = 1; i_arg < n_indices; ++i_arg)
        {
          m = qMax(m, stack_integer[*use_indices[i_arg]]);
        }
        stack_integer[op.output_index] = m;
        break;
      }
      case Custom_op_type::PREF_INDEX:
      {
        const int pref_number = stack_integer[*use_indices[0]];
        if (pref_number > num_groups || pref_number < 1)
        {
          stack_integer[op.output_index] = 999;
          break;
        }
        // The last Pfor is at index   num_groups - 1;
        // then Exhaust is at index    num_groups;
        // then num_prefs is at index  num_groups + 1;
        // then P1 is at index         num_groups + 2.
        // One added at the end because the index in the SQLite database
        // is zero-indexed, but the user-facing index will be 1-indexed.
        stack_integer[op.output_index] = qMin(stack_integer[num_groups + pref_number + 1] + 1, 999);
        break;
      }
      case Custom_op_type::JMP_IF_FALSE:
        if (!stack_boolean[*use_indices[0]])
        {
          stack_boolean[op.output_index] = false;
          // Subtract one from the jump target because
          // i_op will get incremented by the for loop.
          i_op = op.jump_to - 1;
        }
        break;
      case Custom_op_type::JMP_IF_TRUE:
        if (stack_boolean[*use_indices[0]])
        {
          stack_boolean[op.output_index] = true;
          // Subtract one from the jump target because
          // i_op will get incremented by the for loop.
          i_op = op.jump_to - 1;
        }
        break;
      default:
        break;
      }
    }
  }

  void process_vote_with_aggregation(int num_groups,
                                     std::vector<uint8_t>& stack_boolean,
                                     std::vector<int>& stack_integer,
                                     std::vector<std::vector<const int*>>& ptr_indices,
                                     std::vector<Custom_operation>& ops,
                                     int n_ops,
                                     std::vector<std::vector<int>>& aggregated_indices,
                                     std::vector<int>& stack_loops)
  {
    // Run the operations sequence; the caller will probably read
    // stack_boolean[0] or a specific stack_integer entry afterwards.

    int i_loop = -1;
    int i_op   = 0;

    auto get_index = [&](int i)
    {
      const int idx = *ptr_indices.at(i_op).at(i);
      if (idx >= 0)
      {
        return idx;
      }
      const int aggregated_idx = aggregated_index_to_from_negative(idx);
      return aggregated_indices.at(aggregated_idx).at(stack_loops.at(i_loop));
    };

    auto has_aggregated_indices = [&](int i, int& agg_index)
    {
      // This lambda is used by a MIN/MAX operation when there may be an
      // aggregated identifier in the argument list.  In such a case, the
      // (negative) aggregated index itself has been placed on the integer
      // stack, rather than an evaluated number.
      const int stack_idx = *ptr_indices.at(i_op).at(i);
      const int idx       = stack_integer[stack_idx];
      if (idx >= 0)
      {
        return false;
      }
      agg_index = aggregated_index_to_from_negative(idx);
      return true;
    };

    for (i_op = 0; i_op < n_ops; ++i_op)
    {
      Custom_operation& op = ops.at(i_op);
      const int n_indices  = op.input_indices.size();

      switch (op.op_type)
      {
      case Custom_op_type::TRUE_LITERAL:
        stack_boolean[op.output_index] = true;
        break;
      case Custom_op_type::INT_LITERAL:
        stack_integer[op.output_index] = op.int_literal;
        break;
      case Custom_op_type::IDENTIFIER:
        stack_integer[op.output_index] = stack_integer[get_index(0)];
        break;
      case Custom_op_type::BARE_AGG_IDENTIFIER:
        // A special case, when an aggregated identifier (e.g. 'ALP' when
        // working below-the-line) is a child of a MIN or MAX node.  In
        // this case, the MIN or MAX operation will expand the relevant
        // aggregated indices, and we do not try to evaluate the input
        // index value here.
        stack_integer[op.output_index] = *ptr_indices.at(i_op).at(0);
        break;
      case Custom_op_type::NUM_CANDS:
      {
        const int agg_group            = *ptr_indices.at(i_op).at(0);
        const int group                = aggregated_index_to_from_negative(agg_group);
        stack_integer[op.output_index] = aggregated_indices.at(group).size();
        break;
      }
      case Custom_op_type::INDEX:
        stack_integer[op.output_index] = get_index(0) + 1;
        break;
      case Custom_op_type::EQ:
        stack_boolean[op.output_index] = stack_integer[get_index(0)] == stack_integer[get_index(1)];
        break;
      case Custom_op_type::NEQ:
        stack_boolean[op.output_index] = stack_integer[get_index(0)] != stack_integer[get_index(1)];
        break;
      case Custom_op_type::LT:
        stack_boolean[op.output_index] = stack_integer[get_index(0)] < stack_integer[get_index(1)];
        break;
      case Custom_op_type::LTE:
        stack_boolean[op.output_index] = stack_integer[get_index(0)] <= stack_integer[get_index(1)];
        break;
      case Custom_op_type::GT:
        stack_boolean[op.output_index] = stack_integer[get_index(0)] > stack_integer[get_index(1)];
        break;
      case Custom_op_type::GTE:
        stack_boolean[op.output_index] = stack_integer[get_index(0)] >= stack_integer[get_index(1)];
        break;
      case Custom_op_type::IN_RANGE:
        stack_boolean[op.output_index] = (stack_integer[get_index(0)] >= op.range_lower) && (stack_integer[get_index(0)] <= op.range_upper);
        break;
      case Custom_op_type::NOT:
        stack_boolean[op.output_index] = !stack_boolean[get_index(0)];
        break;
      case Custom_op_type::AND:
        stack_boolean[op.output_index] = stack_boolean[get_index(0)] && stack_boolean[get_index(1)];
        break;
      case Custom_op_type::OR:
        stack_boolean[op.output_index] = stack_boolean[get_index(0)] || stack_boolean[get_index(1)];
        break;
      case Custom_op_type::ADD:
        stack_integer[op.output_index] = stack_integer[get_index(0)] + stack_integer[get_index(1)];
        break;
      case Custom_op_type::SUB:
        stack_integer[op.output_index] = stack_integer[get_index(0)] - stack_integer[get_index(1)];
        break;
      case Custom_op_type::ABS:
        stack_integer[op.output_index] = abs(stack_integer[get_index(0)]);
        break;
      case Custom_op_type::IF:
        stack_integer[op.output_index] = stack_boolean[get_index(0)] ? stack_integer[get_index(1)]
                                                                     : stack_integer[get_index(2)];
        break;
      case Custom_op_type::MIN:
      {
        bool have_first = false;
        int m           = 0;
        int agg_index;
        for (int i_arg = 0; i_arg < n_indices; ++i_arg)
        {
          if (has_aggregated_indices(i_arg, agg_index))
          {
            for (int i_agg_arg = 0, n_agg_indices = aggregated_indices.at(agg_index).size();
                 i_agg_arg < n_agg_indices;
                 ++i_agg_arg)
            {
              const int v = stack_integer[aggregated_indices.at(agg_index).at(i_agg_arg)];
              m           = !have_first ? v : qMin(m, v);
              have_first  = true;
            }
          }
          else
          {
            const int v = stack_integer[get_index(i_arg)];
            m           = !have_first ? v : qMin(m, v);
            have_first  = true;
          }
        }
        stack_integer[op.output_index] = m;
        break;
      }
      case Custom_op_type::MAX:
      {
        bool have_first = false;
        int m           = 0;
        int agg_index;
        for (int i_arg = 0; i_arg < n_indices; ++i_arg)
        {
          if (has_aggregated_indices(i_arg, agg_index))
          {
            for (int i_agg_arg = 0, n_agg_indices = aggregated_indices.at(agg_index).size(); i_agg_arg < n_agg_indices; ++i_agg_arg)
            {
              const int v = stack_integer[aggregated_indices.at(agg_index).at(i_agg_arg)];
              m           = !have_first ? v : qMax(m, v);
              have_first  = true;
            }
          }
          else
          {
            const int v = stack_integer[get_index(i_arg)];
            m           = !have_first ? v : qMax(m, v);
            have_first  = true;
          }
        }
        stack_integer[op.output_index] = m;
        break;
      }
      case Custom_op_type::PREF_INDEX:
      {
        const int pref_number = stack_integer[get_index(0)];
        if (pref_number > num_groups || pref_number < 1)
        {
          stack_integer[op.output_index] = 999;
          break;
        }
        // The last Pfor is at index   num_groups - 1;
        // then Exhaust is at index    num_groups;
        // then num_prefs is at index  num_groups + 1;
        // then P1 is at index         num_groups + 2.
        // One added at the end because the index in the SQLite database
        // is zero-indexed, but the user-facing index will be 1-indexed.
        stack_integer[op.output_index] = qMin(stack_integer[num_groups + pref_number + 1] + 1, 999);
        break;
      }
      case Custom_op_type::JMP:
        i_op = op.jump_to - 1;
        break;
      case Custom_op_type::JMP_IF_TRUE:
        if (stack_boolean[get_index(0)])
        {
          stack_boolean[op.output_index] = true;
          // Subtract one from the jump target because
          // i_op will get incremented by the for loop.
          i_op = op.jump_to - 1;
        }
        break;
      case Custom_op_type::JMP_IF_FALSE:
        if (!stack_boolean[get_index(0)])
        {
          stack_boolean[op.output_index] = false;
          // Subtract one from the jump target because
          // i_op will get incremented by the for loop.
          i_op = op.jump_to - 1;
        }
        break;
      case Custom_op_type::BREAK_IF_TRUE:
        if (stack_boolean[get_index(0)])
        {
          stack_boolean[op.output_index] = true;
          i_op                           = op.jump_to - 1;
          stack_loops[i_loop]            = -1;
          i_loop--;
        }
        break;
      case Custom_op_type::BREAK_IF_FALSE:
        if (!stack_boolean[get_index(0)])
        {
          stack_boolean[op.output_index] = false;
          i_op                           = op.jump_to - 1;
          stack_loops[i_loop]            = -1;
          i_loop--;
        }
        break;

      case Custom_op_type::NPP_PREF:
      {
        bool have_first = false;
        int m           = 0;
        for (int i_arg = 0; i_arg < n_indices; ++i_arg)
        {
          const int stack_idx = *ptr_indices.at(i_op).at(i_arg);
          if (stack_idx >= 0)
          {
            const int v = stack_integer[stack_idx];
            m           = !have_first ? v : qMin(m, v);
            have_first  = true;
          }
          else
          {
            const int agg_index = aggregated_index_to_from_negative(stack_idx);
            for (int i_agg_arg = 0, n_agg_indices = aggregated_indices.at(agg_index).size(); i_agg_arg < n_agg_indices; ++i_agg_arg)
            {
              const int v = stack_integer[aggregated_indices.at(agg_index).at(i_agg_arg)];
              m           = !have_first ? v : qMin(m, v);
              have_first  = true;
            }
          }
        }
        stack_integer[op.output_index] = m;
        break;
      }
      case Custom_op_type::ANY:
      case Custom_op_type::ALL:
      {
        // As operations, ANY and ALL are functionally identical, just the start of a quasi-for loop.
        i_loop = op.loop_index;
        stack_loops[i_loop]++;

        const int agg_index = aggregated_index_to_from_negative(*op.ptr_aggregated_index);

        if (stack_loops.at(i_loop) >= static_cast<int>(aggregated_indices.at(agg_index).size()))
        {
          // End of loop
          i_op                = op.jump_to - 1;
          stack_loops[i_loop] = -1;
          i_loop--;
          // If the loop has completed, then ANY must be false and ALL must be true:
          stack_boolean[op.output_index] = (op.op_type == Custom_op_type::ALL);
        }
        break;
      }
      }
    }
  }

} // namespace Custom_operations
