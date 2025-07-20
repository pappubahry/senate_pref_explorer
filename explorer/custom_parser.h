// Mostly written by o4-mini-high.

#ifndef CUSTOM_PARSER_H
#define CUSTOM_PARSER_H

#include "custom_expr.h"
#include "custom_token.h"

class Custom_parser
{
public:
  Custom_parser(const std::vector<Custom_token>& tokens);
  std::unique_ptr<Custom_expr> parse_AST();
  static QString token_kind_to_string(Custom_token_kind k);

private:
  const Custom_token& _peek() const;
  bool _is(Custom_token_kind k) const;
  bool _match(Custom_token_kind k);
  Custom_token _expect(Custom_token_kind k);

  Custom_op_type _token_to_rel_op(Custom_token_kind tk) const;

  std::unique_ptr<Custom_expr> _parse_or_expr();
  std::unique_ptr<Custom_expr> _parse_and_expr();
  std::unique_ptr<Custom_expr> _parse_not_expr();
  std::unique_ptr<Custom_expr> _parse_rel_expr();
  std::unique_ptr<Custom_expr> _parse_add_expr();
  std::unique_ptr<Custom_expr> _parse_primary_expr();

  const std::vector<Custom_token>& _tokens;
  size_t _pos;
};

#endif // CUSTOM_PARSER_H
