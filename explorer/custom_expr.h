#ifndef CUSTOM_EXPR_H
#define CUSTOM_EXPR_H

#include "custom_operation.h"
#include <QObject>

enum class Custom_expr_type
{
  BOOLEAN,
  INTEGER
};

class Custom_expr
{
public:
  Custom_expr(Custom_op_type fn)
    : _op_type(fn)
  {
  }
  ~Custom_expr() = default;
  Custom_expr_type expr_type();
  bool check_valid();
  bool check_valid_aggregations(
    Widget* w, bool in_any_all, bool in_min_max, bool row_is_aggregate, bool col_is_aggregate);

  void add_int_literal(int i) { _int_literals.push_back(i); }
  int get_int_literal(int rank) const { return _int_literals.at(rank); }
  int get_num_int_literals() const { return _int_literals.size(); }

  void set_name(QString& name) { _name = name; };
  QString get_name() const { return _name; }

  void set_aggregated_index(int i) { _aggregated_index = i; }
  int get_aggregated_index() const { return _aggregated_index; }

  void add_argument(std::unique_ptr<Custom_expr> arg);
  Custom_expr* get_argument(int rank) const;
  int get_num_arguments() const { return _arguments.size(); }

  bool has_any_or_all() const;
  Custom_op_type get_op_type() const { return _op_type; }

  bool can_convert_to_sql(Widget* w) const;
  QString to_sql(Widget* w) const;

  QString debug_string(int indent);
  std::string debug_std_string(int indent) { return debug_string(indent).toStdString(); }

private:
  Custom_op_type _op_type;
  std::vector<int> _int_literals;
  std::vector<std::unique_ptr<Custom_expr>> _arguments;
  QString _name;
  int _aggregated_index;
};

#endif // CUSTOM_EXPR_H
