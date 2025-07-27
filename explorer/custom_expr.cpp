#include "custom_expr.h"
#include "main_widget.h"

bool Custom_expr::check_valid()
{
  auto require_numeric_arguments = [&](int start = 0, int stop = -1)
  {
    const int n = stop > 0 ? stop : _arguments.size();
    for (int i = start; i < n; ++i)
    {
      if (_arguments.at(i)->expr_type() != Custom_expr_type::INTEGER)
      {
        const QString msg = QString("%1 can only work on integers, not booleans.\n\n")
          .arg(Custom_operations::op_name(_op_type)) + debug_string(0);
        throw std::runtime_error(msg.toStdString());
      }
    }
  };

  auto require_boolean_arguments = [&](int start = 0, int stop = -1)
  {
    const int n = stop > 0 ? stop : _arguments.size();
    for (int i = start; i < n; ++i)
    {
      if (_arguments.at(i)->expr_type() != Custom_expr_type::BOOLEAN)
      {
        const QString msg = QString("%1 can only work on booleans, not integers.\n\n")
          .arg(Custom_operations::op_name(_op_type)) + debug_string(0);
        throw std::runtime_error(msg.toStdString());
      }
    }
  };

  auto require_n_arguments = [&](size_t n)
  {
    if (_arguments.size() != n)
    {
      const QString plural = n == 1 ? "" : "s";
      const QString msg    = QString("%1 must have exactly %2 argument%3.\n\n")
        .arg(Custom_operations::op_name(_op_type), QString::number(n), plural) + debug_string(0);
      throw std::runtime_error(msg.toStdString());
    }
  };

  switch (_op_type)
  {
  case Custom_op_type::TRUE_LITERAL:
  case Custom_op_type::IDENTIFIER:
    return true;
  case Custom_op_type::INT_LITERAL:
    if (_int_literals.size() != 1)
    {
      throw std::runtime_error("Programmer error: int literal not uniquely defined.");
    }
    return true;
  case Custom_op_type::IN_RANGE:
    if (_int_literals.size() != 2)
    {
      throw std::runtime_error("Programmer error: in-range operator not properly defined.");
    }
    break;
  case Custom_op_type::EQ:
  case Custom_op_type::NEQ:
  case Custom_op_type::GT:
  case Custom_op_type::GTE:
  case Custom_op_type::LT:
  case Custom_op_type::LTE:
  case Custom_op_type::ADD:
  case Custom_op_type::SUB:
    require_n_arguments(2);
    require_numeric_arguments();
    break;
  case Custom_op_type::ABS:
    require_n_arguments(1);
    require_numeric_arguments();
    break;
  case Custom_op_type::MIN:
  case Custom_op_type::MAX:
    require_numeric_arguments();
    break;
  case Custom_op_type::NOT:
    require_n_arguments(1);
    require_boolean_arguments();
    break;
  case Custom_op_type::AND:
  case Custom_op_type::OR:
    require_n_arguments(2);
    require_boolean_arguments();
    break;
  case Custom_op_type::IF:
    require_n_arguments(3);
    require_boolean_arguments(0, 1);
    require_numeric_arguments(1, 3);
    break;
  case Custom_op_type::PREF_INDEX:
    require_n_arguments(1);
    require_numeric_arguments();
    break;
  case Custom_op_type::ANY:
  case Custom_op_type::ALL:
    require_n_arguments(1);
    require_boolean_arguments();
    break;
  default:
    throw std::runtime_error("Programmer error: AST check_valid() failed.  Sorry. :(");
  }

  for (int i = 0, n = _arguments.size(); i < n; ++i)
  {
    _arguments.at(i)->check_valid();
  }

  return true;
}

bool Custom_expr::check_valid_aggregations(
  Widget* w, bool in_any_all, bool in_min_max, bool row_is_aggregate, bool col_is_aggregate)
{
  // Currently assumes that the only aggregations are BTL candidates into
  // groups, and this should only be called if working BTL.

  auto get_agg_index = [w, row_is_aggregate, col_is_aggregate](const QString& name, int& index)
  {
    if (row_is_aggregate && name == Custom_identifiers::ROW)
    {
      index = Custom_row_col::ROW;
      return true;
    }

    if (col_is_aggregate && name == Custom_identifiers::COL)
    {
      index = Custom_row_col::COL;
      return true;
    }

    const int group = w->get_group_from_short(name);
    if (group >= 0)
    {
      index = group;
      return true;
    }

    return false;
  };

  std::function<void(const Custom_expr*, int&, int&)> scan_any_all_node;

  scan_any_all_node = [&](const Custom_expr* expr, int& agg_index, int& agg_index_count)
  {
    const int n_args = expr->get_num_arguments();
    for (int i = 0; i < n_args; ++i)
    {
      const Custom_expr* child     = expr->get_argument(i);
      const Custom_op_type op_type = child->get_op_type();
      if (op_type == Custom_op_type::ANY || op_type == Custom_op_type::ALL
          || op_type == Custom_op_type::MIN || op_type == Custom_op_type::MAX)
      {
        // A nested ANY or ALL node should contain its own aggregated identifier.
        // MIN and MAX nodes may also contain their own aggregated identifiers.
        continue;
      }

      if (op_type == Custom_op_type::IDENTIFIER)
      {
        const QString name = child->get_name();
        int possible_agg_index;
        if (get_agg_index(name, possible_agg_index))
        {
          agg_index = possible_agg_index;
          agg_index_count++;
        }

        if (agg_index_count > 1)
        {
          throw std::runtime_error("<code>any</code> or <code>all</code> node contains more than "
                                   "one aggregated identifier.");
        }
      }

      scan_any_all_node(child, agg_index, agg_index_count);
    }
  };

  if (_op_type == Custom_op_type::IDENTIFIER && !in_any_all && !in_min_max
      && !_name.startsWith("nc_"))
  {
    int dummy;
    if (get_agg_index(_name, dummy))
    {
      throw std::runtime_error("An aggregated identifier can only appear inside an "
                               "<code>any()</code> or <code>all()</code> "
                               "function, or as an argument of a <code>min()</code> or "
                               "<code>max()</code> function.  In the latter "
                               "two cases, the identifier cannot be part of a larger expression.");
    }
  }

  // ANY and ALL nodes can have the aggregated identifier anywhere descended
  // from them; for MIN and MAX nodes, an aggregated identifier must be a
  // child node.
  bool any_all = in_any_all;
  bool min_max = false;

  if (_op_type == Custom_op_type::ANY || _op_type == Custom_op_type::ALL)
  {
    any_all = true;
    int agg_index;
    int agg_index_count = 0;
    scan_any_all_node(this, agg_index, agg_index_count);
    if (agg_index_count == 0)
    {
      throw std::runtime_error(
        "No aggregated identifier found in <code>any()</code> or <code>all()</code>");
    }
    _aggregated_index = agg_index;
  }

  if (_op_type == Custom_op_type::MIN || _op_type == Custom_op_type::MAX)
  {
    min_max = true;
  }

  for (int i = 0, n = _arguments.size(); i < n; ++i)
  {
    _arguments.at(i)->check_valid_aggregations(w,
                                               any_all,
                                               min_max,
                                               row_is_aggregate,
                                               col_is_aggregate);
  }
  return true;
}

QString Custom_expr::debug_string(int indent)
{
  QString s = "\n" + QString().leftJustified(indent, ' ');
  s += Custom_operations::op_name(_op_type);
  switch (_op_type)
  {
  case Custom_op_type::TRUE_LITERAL:
    break;
  case Custom_op_type::INT_LITERAL:
    if (_int_literals.size() != 1)
    {
      throw std::runtime_error("Programmer error: int literal not uniquely defined.");
    }
    s += " " + QString::number(_int_literals.at(0));
    break;
  case Custom_op_type::IDENTIFIER:
    s += " " + _name;
    break;
  case Custom_op_type::IN_RANGE:
    if (_int_literals.size() != 2)
    {
      throw std::runtime_error("Programmer error: in-range not properly defined.");
    }
    s += " " + QString::number(_int_literals.at(0)) + ".." + QString::number(_int_literals.at(1));
    Q_FALLTHROUGH();
  case Custom_op_type::EQ:
  case Custom_op_type::NEQ:
  case Custom_op_type::GT:
  case Custom_op_type::GTE:
  case Custom_op_type::LT:
  case Custom_op_type::LTE:
  case Custom_op_type::NOT:
  case Custom_op_type::AND:
  case Custom_op_type::OR:
  case Custom_op_type::ADD:
  case Custom_op_type::SUB:
  case Custom_op_type::ABS:
  case Custom_op_type::MIN:
  case Custom_op_type::MAX:
  case Custom_op_type::IF:
  case Custom_op_type::PREF_INDEX:
  case Custom_op_type::ANY:
  case Custom_op_type::ALL:
    for (std::unique_ptr<Custom_expr>& arg : _arguments)
    {
      s += arg->debug_string(indent + 1);
    }
  default:
    break;
  }

  return s;
}

Custom_expr_type Custom_expr::expr_type()
{
  switch (_op_type)
  {
  case Custom_op_type::TRUE_LITERAL:
  case Custom_op_type::EQ:
  case Custom_op_type::NEQ:
  case Custom_op_type::GT:
  case Custom_op_type::GTE:
  case Custom_op_type::LT:
  case Custom_op_type::LTE:
  case Custom_op_type::IN_RANGE:
  case Custom_op_type::NOT:
  case Custom_op_type::AND:
  case Custom_op_type::OR:
  case Custom_op_type::ANY:
  case Custom_op_type::ALL:
    return Custom_expr_type::BOOLEAN;
  case Custom_op_type::INT_LITERAL:
  case Custom_op_type::IDENTIFIER:
  case Custom_op_type::ADD:
  case Custom_op_type::SUB:
  case Custom_op_type::ABS:
  case Custom_op_type::MIN:
  case Custom_op_type::MAX:
  case Custom_op_type::IF:
  case Custom_op_type::PREF_INDEX:
    return Custom_expr_type::INTEGER;
  default:
    throw std::runtime_error("Programmer error: Unexpected operation in expr_type()");
  }
}

void Custom_expr::add_argument(std::unique_ptr<Custom_expr> arg)
{
  _arguments.push_back(std::move(arg));
}

Custom_expr* Custom_expr::get_argument(int rank) const
{
  return _arguments.at(rank).get();
}

bool Custom_expr::has_any_or_all() const
{
  if (_op_type == Custom_op_type::ANY || _op_type == Custom_op_type::ALL)
  {
    return true;
  }

  const int n = _arguments.size();
  for (int i = 0; i < n; ++i)
  {
    if (_arguments.at(i)->has_any_or_all())
    {
      return true;
    }
  }
  return false;
}

bool Custom_expr::can_convert_to_sql(Widget* w) const
{
  bool can_convert = true;
  if (_op_type == Custom_op_type::PREF_INDEX || _op_type == Custom_op_type::ANY
      || _op_type == Custom_op_type::ALL || _op_type == Custom_op_type::JMP
      || _op_type == Custom_op_type::JMP_IF_TRUE || _op_type == Custom_op_type::JMP_IF_FALSE
      || _op_type == Custom_op_type::BREAK_IF_TRUE || _op_type == Custom_op_type::BREAK_IF_FALSE)
  {
    // Several of these op types shouldn't even be possible in a Custom_expr, only in a Custom_operation.
    return false;
  }
  if (_op_type == Custom_op_type::IDENTIFIER)
  {
    if (_name == Custom_identifiers::N_PREFS)
    {
      return true;
    }

    if (_name == Custom_identifiers::ROW || _name == Custom_identifiers::COL)
    {
      return false;
    }

    if (w->get_abtl() == "atl")
    {
      const int group = w->get_group_from_short(_name);
      return group >= 0;
    }

    // BTL; Can't handle aggregated groups in SQL.
    const int cand = w->get_cand_from_short(_name);
    return cand >= 0;
  }
  const int n = _arguments.size();
  for (int i = 0; i < n; ++i)
  {
    can_convert &= _arguments.at(i)->can_convert_to_sql(w);
  }

  return can_convert;
}

QString Custom_expr::to_sql(Widget* w) const
{
  switch (_op_type)
  {
  case Custom_op_type::TRUE_LITERAL:
    return "1";
  case Custom_op_type::INT_LITERAL:
    return QString::number(_int_literals.at(0));
  case Custom_op_type::IDENTIFIER:
  {
    if (_name == Custom_identifiers::N_PREFS)
    {
      return "num_prefs";
    }
    const bool is_atl = w->get_abtl() == "atl";
    if (is_atl)
    {
      const int group = w->get_group_from_short(_name);
      return "Pfor" + QString::number(group);
    }
    // Assumption is that this method doesn't get called if there's an aggregated group.
    const int cand = w->get_cand_from_short(_name);
    return "Pfor" + QString::number(cand);
  }
  case Custom_op_type::EQ:
    return "(" + _arguments.at(0)->to_sql(w) + " = " + _arguments.at(1)->to_sql(w) + ")";
  case Custom_op_type::NEQ:
    return "(" + _arguments.at(0)->to_sql(w) + " <> " + _arguments.at(1)->to_sql(w) + ")";
  case Custom_op_type::GT:
    return "(" + _arguments.at(0)->to_sql(w) + " > " + _arguments.at(1)->to_sql(w) + ")";
  case Custom_op_type::GTE:
    return "(" + _arguments.at(0)->to_sql(w) + " >= " + _arguments.at(1)->to_sql(w) + ")";
  case Custom_op_type::LT:
    return "(" + _arguments.at(0)->to_sql(w) + " < " + _arguments.at(1)->to_sql(w) + ")";
  case Custom_op_type::LTE:
    return "(" + _arguments.at(0)->to_sql(w) + " <= " + _arguments.at(1)->to_sql(w) + ")";
  case Custom_op_type::IN_RANGE:
    return "(" + _arguments.at(0)->to_sql(w) + " BETWEEN " + QString::number(_int_literals.at(0)) + " AND " + QString::number(_int_literals.at(1));
  case Custom_op_type::NOT:
    return "(NOT " + _arguments.at(0)->to_sql(w) + ")";
  case Custom_op_type::AND:
    return "(" + _arguments.at(0)->to_sql(w) + " AND " + _arguments.at(1)->to_sql(w) + ")";
  case Custom_op_type::OR:
    return "(" + _arguments.at(0)->to_sql(w) + " OR " + _arguments.at(1)->to_sql(w) + ")";
  case Custom_op_type::ADD:
    return "(" + _arguments.at(0)->to_sql(w) + " + " + _arguments.at(1)->to_sql(w) + ")";
  case Custom_op_type::SUB:
    return "(" + _arguments.at(0)->to_sql(w) + " - " + _arguments.at(1)->to_sql(w) + ")";
  case Custom_op_type::ABS:
    return "(ABS(" + _arguments.at(0)->to_sql(w) + "))";
  case Custom_op_type::MIN:
  case Custom_op_type::MAX:
  {
    const QString name = _op_type == Custom_op_type::MIN ? "MIN" : "MAX";
    QString sql        = "(" + name + "(" + _arguments.at(0)->to_sql(w) + ")";
    const int n        = _arguments.size();
    for (int i = 0; i < n; ++i)
    {
      sql += ", (" + _arguments.at(i)->to_sql(w) + ")";
    }
    sql += ")";
    return sql;
  }
  case Custom_op_type::IF:
    return "(IIF(" + _arguments.at(0)->to_sql(w) + ", " + _arguments.at(1)->to_sql(w) + ", "
           + _arguments.at(2)->to_sql(w) + "))";
  case Custom_op_type::PREF_INDEX:
    throw std::runtime_error(
      "Programmer error: Shouldn't be directly making SQL from a pi() function.  Sorry :(");
  case Custom_op_type::ANY:
    throw std::runtime_error(
      "Programmer error: Shouldn't be directly making SQL from an any() function.  Sorry :(");
  case Custom_op_type::ALL:
    throw std::runtime_error(
      "Programmer error: Shouldn't be directly making SQL from an all() function.  Sorry :(");
  default:
    throw std::runtime_error(
      "Programmer error: Unexpected operation type while making SQL.  Sorry :(");
  }

  return QString();
}
