// Mostly written by o4-mini-high.

#ifndef CUSTOM_LEXER_H
#define CUSTOM_LEXER_H

#include "custom_token.h"

class Custom_lexer
{
public:
  Custom_lexer(const QString& input)
    : _source(input)
    , _pos(0)
    , _length(input.size())
  {
  }

  std::vector<Custom_token> tokenize();

private:
  const QString _source;
  size_t _pos;
  size_t _length;

  QChar _peek() const;
  QChar _peek_next() const;
  void _advance();
  void _skip_whitespace();
  bool _next_is_digit() const;

  Custom_token _lex_identifier_or_keyword();
  Custom_token _lex_int_literal();
  Custom_token _lex_equals();
  Custom_token _lex_less_than();
  Custom_token _lex_greater_than();
  Custom_token _lex_not_equal();
  Custom_token _lex_dotdot();
};

#endif // CUSTOM_LEXER_H
