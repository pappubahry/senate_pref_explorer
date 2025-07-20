// Mostly written by o4-mini-high.

#include "custom_parser.h"

Custom_parser::Custom_parser(const std::vector<Custom_token>& tokens)
  : _tokens(tokens)
  , _pos(0)
{
}

std::unique_ptr<Custom_expr> Custom_parser::parse_AST()
{
  // If the very first Custom_token is END_OF_EXPR, return a Custom_true_literal.
  if (_peek().kind == Custom_token_kind::END_OF_EXPR)
  {
    return std::make_unique<Custom_expr>(Custom_op_type::TRUE_LITERAL);
  }

  // Otherwise parse a full expression, then expect EOF.
  auto custom_expr = _parse_or_expr();
  _expect(Custom_token_kind::END_OF_EXPR);
  custom_expr->check_valid();
  return custom_expr;
}

QString Custom_parser::token_kind_to_string(Custom_token_kind k)
{
  switch (k)
  {
  case Custom_token_kind::END_OF_EXPR:
    return "end";
  case Custom_token_kind::IDENTIFIER:
    return "IDENTIFIER";
  case Custom_token_kind::INT_LITERAL:
    return "INT_LITERAL";
  case Custom_token_kind::KW_AND:
    return "and";
  case Custom_token_kind::KW_OR:
    return "or";
  case Custom_token_kind::KW_NOT:
    return "not";
  case Custom_token_kind::KW_IN:
    return "in";
  case Custom_token_kind::EQ:
    return "=";
  case Custom_token_kind::NEQ:
    return "!=";
  case Custom_token_kind::LT:
    return "<";
  case Custom_token_kind::LTE:
    return "<=";
  case Custom_token_kind::GT:
    return ">";
  case Custom_token_kind::GTE:
    return ">=";
  case Custom_token_kind::DOTDOT:
    return "..";
  case Custom_token_kind::LPAREN:
    return "(";
  case Custom_token_kind::RPAREN:
    return ")";
  case Custom_token_kind::COMMA:
    return ",";
  case Custom_token_kind::PLUS:
    return "+";
  case Custom_token_kind::MINUS:
    return "-";
  }
  return "<unknown>";
}

const Custom_token& Custom_parser::_peek() const
{
  if (_pos >= _tokens.size())
  {
    // We should always have at least one EOF Custom_token at the end of the vector.
    throw std::runtime_error("Internal parser error: no EOF Custom_token.");
  }
  return _tokens[_pos];
}

bool Custom_parser::_is(Custom_token_kind k) const
{
  return _peek().kind == k;
}

bool Custom_parser::_match(Custom_token_kind k)
{
  if (_is(k))
  {
    ++_pos;
    return true;
  }
  return false;
}

Custom_token Custom_parser::_expect(Custom_token_kind k)
{
  if (_peek().kind == k)
  {
    return _tokens[_pos++];
  }
  // Simple error message.
  const QString msg = QString("Expected token %1 but found %2")
                        .arg(token_kind_to_string(k), token_kind_to_string(_peek().kind));
  throw std::runtime_error(msg.toStdString());
}

Custom_op_type Custom_parser::_token_to_rel_op(Custom_token_kind tk) const
{
  switch (tk)
  {
  case Custom_token_kind::EQ:
    return Custom_op_type::EQ;
  case Custom_token_kind::NEQ:
    return Custom_op_type::NEQ;
  case Custom_token_kind::LT:
    return Custom_op_type::LT;
  case Custom_token_kind::LTE:
    return Custom_op_type::LTE;
  case Custom_token_kind::GT:
    return Custom_op_type::GT;
  case Custom_token_kind::GTE:
    return Custom_op_type::GTE;
  default:
    throw std::runtime_error("Programmer error: token_to_rel_op called on non‐comparison Custom_token");
  }
}

std::unique_ptr<Custom_expr> Custom_parser::_parse_or_expr()
{
  auto left = _parse_and_expr();
  while (_match(Custom_token_kind::KW_OR))
  {
    auto or_node = std::make_unique<Custom_expr>(Custom_op_type::OR);
    or_node->add_argument(std::move(left));
    auto right = _parse_and_expr();
    or_node->add_argument(std::move(right));
    left = std::move(or_node);
  }
  return left;
}

std::unique_ptr<Custom_expr> Custom_parser::_parse_and_expr()
{
  auto left = _parse_not_expr();
  while (_match(Custom_token_kind::KW_AND))
  {
    auto and_node = std::make_unique<Custom_expr>(Custom_op_type::AND);
    and_node->add_argument(std::move(left));
    auto right = _parse_not_expr();
    and_node->add_argument(std::move(right));
    left = std::move(and_node);
  }
  return left;
}

std::unique_ptr<Custom_expr> Custom_parser::_parse_not_expr()
{
  if (_match(Custom_token_kind::KW_NOT))
  {
    auto not_node = std::make_unique<Custom_expr>(Custom_op_type::NOT);
    auto operand  = _parse_not_expr();
    not_node->add_argument(std::move(operand));
    return not_node;
  }
  return _parse_rel_expr();
}

std::unique_ptr<Custom_expr> Custom_parser::_parse_rel_expr()
{
  // First parse the left side as a Custom_expr_add
  auto left = _parse_add_expr();

  // If next Custom_token is a comparison operator, consume it and parse right‐hand side.
  if (_is(Custom_token_kind::EQ) || _is(Custom_token_kind::NEQ) || _is(Custom_token_kind::LT)
      || _is(Custom_token_kind::LTE) || _is(Custom_token_kind::GT) || _is(Custom_token_kind::GTE))
  {
    Custom_op_type op = _token_to_rel_op(_peek().kind);
    _pos++; // consume that comparison Custom_token
    auto right    = _parse_add_expr();
    auto cmp_node = std::make_unique<Custom_expr>(op);
    cmp_node->add_argument(std::move(left));
    cmp_node->add_argument(std::move(right));
    return cmp_node;
  }

  // Otherwise, check for "in INT_LITERAL .. INT_LITERAL"
  if (_match(Custom_token_kind::KW_IN))
  {
    // low bound must be an INT_LITERAL
    if (!_is(Custom_token_kind::INT_LITERAL))
    {
      throw std::runtime_error("Expected integer literal for low bound of 'in' operator");
    }
    int low = std::atoi(_peek().text.toStdString().c_str());
    _pos++;

    _expect(Custom_token_kind::DOTDOT);

    if (!_is(Custom_token_kind::INT_LITERAL))
    {
      throw std::runtime_error("Expected integer literal for high bound of 'in' operator");
    }
    int high = std::atoi(_peek().text.toStdString().c_str());
    _pos++;

    auto in_range_node = std::make_unique<Custom_expr>(Custom_op_type::IN_RANGE);
    in_range_node->add_int_literal(low);
    in_range_node->add_int_literal(high);
    in_range_node->add_argument(std::move(left));
    return in_range_node;
  }

  // If neither comparison nor in‐range, just return the Custom_expr_add.
  return left;
}

std::unique_ptr<Custom_expr> Custom_parser::_parse_add_expr()
{
  auto left = _parse_primary_expr();
  while (true)
  {
    if (_match(Custom_token_kind::PLUS))
    {
      auto right    = _parse_primary_expr();
      auto add_node = std::make_unique<Custom_expr>(Custom_op_type::ADD);
      add_node->add_argument(std::move(left));
      add_node->add_argument(std::move(right));
      left = std::move(add_node);
      continue;
    }

    if (_match(Custom_token_kind::MINUS))
    {
      auto right    = _parse_primary_expr();
      auto add_node = std::make_unique<Custom_expr>(Custom_op_type::SUB);
      add_node->add_argument(std::move(left));
      add_node->add_argument(std::move(right));
      left = std::move(add_node);
      continue;
    }
    break;
  }
  return left;
}

std::unique_ptr<Custom_expr> Custom_parser::_parse_primary_expr()
{
  // Integer literal?
  if (_match(Custom_token_kind::INT_LITERAL))
  {
    // _tokens[_pos-1] was the INT_LITERAL we just consumed
    const Custom_token& lit           = _tokens[_pos - 1];
    const int value                   = std::atoi(lit.text.toStdString().c_str());
    std::unique_ptr<Custom_expr> expr = std::make_unique<Custom_expr>(Custom_op_type::INT_LITERAL);
    expr->add_int_literal(value);
    return expr;
  }

  // Identifier or function call?
  if (_match(Custom_token_kind::IDENTIFIER))
  {
    QString name = _tokens[_pos - 1].text;

    // function‐call syntax: name '(' expr { ',' expr }* ')'
    if (_match(Custom_token_kind::LPAREN))
    {
      std::vector<std::unique_ptr<Custom_expr>> args;
      // first argument (full‐expression)
      args.push_back(_parse_or_expr());
      // additional arguments
      while (_match(Custom_token_kind::COMMA))
      {
        args.push_back(_parse_or_expr());
      }
      _expect(Custom_token_kind::RPAREN);

      if (name == "min")
      {
        std::unique_ptr<Custom_expr> expr = std::make_unique<Custom_expr>(Custom_op_type::MIN);
        for (int i = 0, n = args.size(); i < n; ++i)
        {
          expr->add_argument(std::move(args.at(i)));
        }
        return expr;
      }
      else if (name == "max")
      {
        std::unique_ptr<Custom_expr> expr = std::make_unique<Custom_expr>(Custom_op_type::MAX);
        for (int i = 0, n = args.size(); i < n; ++i)
        {
          expr->add_argument(std::move(args.at(i)));
        }
        return expr;
      }
      else if (name == "abs")
      {
        if (args.size() != 1)
        {
          throw std::runtime_error("abs() function must have exactly one argument.");
        }
        std::unique_ptr<Custom_expr> expr = std::make_unique<Custom_expr>(Custom_op_type::ABS);
        expr->add_argument(std::move(args.at(0)));
        return expr;
      }
      else if (name == "if")
      {
        if (args.size() != 3)
        {
          throw std::runtime_error("if(condition, value_if_true, value_if_false) function must have exactly three arguments.");
        }
        std::unique_ptr<Custom_expr> expr = std::make_unique<Custom_expr>(Custom_op_type::IF);
        for (int i = 0, n = args.size(); i < n; ++i)
        {
          expr->add_argument(std::move(args.at(i)));
        }
        return expr;
      }
      else if (name == "pi")
      {
        if (args.size() != 1)
        {
          throw std::runtime_error("pi() function must have exactly one argument.");
        }
        std::unique_ptr<Custom_expr> expr = std::make_unique<Custom_expr>(Custom_op_type::PREF_INDEX);
        expr->add_argument(std::move(args.at(0)));
        return expr;
      }
      else if (name == "any")
      {
        if (args.size() != 1)
        {
          throw std::runtime_error("any() function must have exactly one argument.");
        }
        std::unique_ptr<Custom_expr> expr = std::make_unique<Custom_expr>(Custom_op_type::ANY);
        expr->add_argument(std::move(args.at(0)));
        return expr;
      }
      else if (name == "all")
      {
        if (args.size() != 1)
        {
          throw std::runtime_error("all() function must have exactly one argument.");
        }
        std::unique_ptr<Custom_expr> expr = std::make_unique<Custom_expr>(Custom_op_type::ALL);
        expr->add_argument(std::move(args.at(0)));
        return expr;
      }
      else
      {
        throw std::runtime_error(
          QString("Unknown function '%1'").arg(name).toStdString());
      }
    }

    // otherwise it’s just a bare identifier
    std::unique_ptr<Custom_expr> expr = std::make_unique<Custom_expr>(Custom_op_type::IDENTIFIER);
    expr->set_name(name);
    return expr;
  }

  // Parenthesized expression?
  if (_match(Custom_token_kind::LPAREN))
  {
    auto inside = _parse_or_expr();
    _expect(Custom_token_kind::RPAREN);
    return inside;
  }

  // If none match, error
  const QString msg = QString("Expected integer, identifier, or '(' but found %1")
                        .arg(token_kind_to_string(_peek().kind));
  throw std::runtime_error(msg.toStdString());
}
