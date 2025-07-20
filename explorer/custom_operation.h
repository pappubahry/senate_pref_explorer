#ifndef CUSTOM_OPERATION_H
#define CUSTOM_OPERATION_H

#include <vector>
#include <QObject>
#include <QString>

class Widget;
class Custom_expr;

// Special values used to denote that an identifier is 'row' or 'col',
// and should therefore be read according to the current looping row
// or column variable.
namespace Custom_row_col
{
  constexpr int ROW = -1;
  constexpr int COL = -2;
} // namespace Custom_row_col

// Annoying pattern of declaring the extern const and then defining it outside
// the namespace is to avoid a warning about non-"plain old data" static
// consts.
namespace Custom_identifiers
{
  extern const QString ROW;
  extern const QString COL;
  extern const QString N_PREFS;
  extern const QString N_MAX;
} // namespace Custom_identifiers

namespace Custom_axis_names
{
  extern const QString GROUPS;
  extern const QString CANDIDATES;
} // namespace Custom_axis_names

enum class Custom_op_type
{
  TRUE_LITERAL,
  INT_LITERAL,
  IDENTIFIER,
  BARE_AGG_IDENTIFIER,
  NUM_CANDS,
  INDEX,
  EQ,
  NEQ,
  LT,
  LTE,
  GT,
  GTE,
  IN_RANGE,
  NOT,
  AND,
  OR,
  ADD,
  SUB,
  MIN,
  MAX,
  ABS,
  IF,
  PREF_INDEX,
  JMP,
  JMP_IF_TRUE,
  JMP_IF_FALSE,
  BREAK_IF_TRUE,
  BREAK_IF_FALSE,
  NPP_PREF,
  ANY,
  ALL
};

struct Custom_operation
{
  Custom_op_type op_type;
  std::vector<int> input_indices = std::vector<int>(0);
  int jump_to                    = -1;
  int int_literal                = -1;
  int range_lower                = -1;
  int range_upper                = -1;
  int output_index               = -1;
  int loop_index                 = -1;
  int aggregated_index           = -9999;
  int* ptr_aggregated_index      = nullptr;
};

enum class Custom_axis_type
{
  GROUPS,
  CANDIDATES,
  NUMBERS,
  NPP,
  NONE
};

struct Custom_axis_definition
{
  Custom_axis_type type = Custom_axis_type::NONE;
  QVector<int> numbers;
  QString every_numbers_definition;
  std::unique_ptr<Custom_expr> every_numbers_ast;
  // npp_indices are negative if groups BTL, non-negative otherwise
  QVector<int> npp_indices;
};

namespace Custom_operations
{
  const QString op_name(Custom_op_type op_type);
  void require_n_params(const Custom_expr* expr, int (Custom_expr::*get_n_method)() const, int n);
  int aggregated_index_to_from_negative(int i);

  void create_operations(Widget* w, const Custom_expr* parent, const Custom_expr* expr, std::vector<Custom_operation>& operations, int& index_stack_boolean, int& index_stack_integer, bool row_is_aggregated, bool col_is_aggregated, int& i_loop, int depth = 0);

  void create_npp_operation(Widget* w, std::vector<Custom_operation>& operations, int& index_stack_integer, QVector<int>& groups);

  QString operations_table_string(std::vector<Custom_operation>& operations);

  void update_max_loop_index(std::vector<Custom_operation>& operations, int& max_loop_index);

  void setup_ptr_indices(int& max_stack_index, const int& i, const int& j, std::vector<std::vector<const int*>>& ptr_indices, std::vector<Custom_operation>& ops);

  void setup_aggregated_ptr_indices(int& i, int& j, std::vector<Custom_operation>& ops);

  void process_vote(int num_groups, std::vector<uint8_t>& stack_boolean, std::vector<int>& stack_integer, std::vector<std::vector<const int*>>& ptr_indices, std::vector<Custom_operation>& ops, int n_ops);

  void process_vote_with_aggregation(int num_groups, std::vector<uint8_t>& stack_boolean, std::vector<int>& stack_integer, std::vector<std::vector<const int*>>& ptr_indices, std::vector<Custom_operation>& ops, int n_ops, std::vector<std::vector<int>>& aggregated_indices, std::vector<int>& stack_loops);
} // namespace Custom_operations

#endif // CUSTOM_OPERATION_H
