// Mostly written by o4-mini-high.

#include "custom_lexer.h"

// Main entry‐point: produce a vector<Custom_token> (always ending with END_OF_EXPR).
std::vector<Custom_token> Custom_lexer::tokenize()
{
  std::vector<Custom_token> tokens;
  while (true)
  {
    _skip_whitespace();
    if (_pos >= _length)
    {
      tokens.push_back(Custom_token(Custom_token_kind::END_OF_EXPR));
      break;
    }

    QChar c = _peek();
    if (c.isLetter() || c == '_')
    {
      tokens.push_back(_lex_identifier_or_keyword());
    }
    else if (c.isDigit() || (c == '-' && _next_is_digit()))
    {
      // We allow negative integer literals if ‘-’ is directly followed by a digit.
      tokens.push_back(_lex_int_literal());
    }
    else
    {
      switch (c.unicode())
      {
      case '=':
        tokens.push_back(_lex_equals());
        break;
      case '<':
        tokens.push_back(_lex_less_than());
        break;
      case '>':
        tokens.push_back(_lex_greater_than());
        break;
      case '!':
        tokens.push_back(_lex_not_equal());
        break;
      case '.':
        tokens.push_back(_lex_dotdot());
        break;
      case '(':
        _advance();
        tokens.push_back(Custom_token(Custom_token_kind::LPAREN));
        break;
      case ')':
        _advance();
        tokens.push_back(Custom_token(Custom_token_kind::RPAREN));
        break;
      case ',':
        _advance();
        tokens.push_back(Custom_token(Custom_token_kind::COMMA));
        break;
      case '+':
        _advance();
        tokens.push_back(Custom_token(Custom_token_kind::PLUS));
        break;
      case '-':
        // If we get here, it means ‘-’ was not followed by a digit (so it's the minus operator).
        _advance();
        tokens.push_back(Custom_token(Custom_token_kind::MINUS));
        break;
      default:
      {
        const std::string msg = std::string("Unexpected character in Custom_lexer: '")
                              + std::string(1, c.unicode()) + "'";
        throw std::runtime_error(msg);
      }
      }
    }
  }
  return tokens;
}

// Peek at current char (does not advance). If _pos >= _length, return '\0'.
QChar Custom_lexer::_peek() const
{
  return (_pos < _length ? _source[_pos] : '\0');
}

// Peek at next char (_pos+1). If beyond end, return '\0'.
QChar Custom_lexer::_peek_next() const
{
  return (_pos + 1 < _length ? _source[_pos + 1] : '\0');
}

// Advance _pos by one (unless already at end).
void Custom_lexer::_advance()
{
  if (_pos < _length)
  {
    ++_pos;
  }
}

// If the current char is whitespace, skip past all consecutive whitespace.
void Custom_lexer::_skip_whitespace()
{
  while (_pos < _length && _source[_pos].isSpace())
  {
    ++_pos;
  }
}

// Return true if current char is ‘-’ and the next char is a digit (0–9).
bool Custom_lexer::_next_is_digit() const
{
  return (_pos + 1 < _length && _source[_pos + 1].isDigit());
}

// Lex an identifier or a keyword ("and", "or", "not", "in").
// An identifier is [A-Za-z_][A-Za-z0-9_]*.
Custom_token Custom_lexer::_lex_identifier_or_keyword()
{
  size_t start = _pos;
  // First character is already known to be isalpha or '_'.
  while (_pos < _length && (_source[_pos].isLetterOrNumber() || _source[_pos] == '_'))
  {
    ++_pos;
  }
  QString word = _source.mid(start, _pos - start);

  // Check if it’s one of our keywords:
  if (word == "and")
    return Custom_token(Custom_token_kind::KW_AND);
  if (word == "or")
    return Custom_token(Custom_token_kind::KW_OR);
  if (word == "not")
    return Custom_token(Custom_token_kind::KW_NOT);
  if (word == "in")
    return Custom_token(Custom_token_kind::KW_IN);

  // Otherwise, it’s a plain Custom_identifier:
  return Custom_token(Custom_token_kind::IDENTIFIER, word);
}

// Lex an integer literal. We support an optional leading '-' if next_is_digit()==true.
Custom_token Custom_lexer::_lex_int_literal()
{
  size_t start = _pos;

  // If there is a leading '-', consume it as part of the number.
  if (_source[_pos] == '-')
  {
    ++_pos;
  }

  // Now, consume all digits [0-9]+
  while (_pos < _length && _source[_pos].isDigit())
  {
    ++_pos;
  }
  QString numText = _source.mid(start, _pos - start);
  return Custom_token(Custom_token_kind::INT_LITERAL, numText);
}

// Lex "=" → Custom_token_kind::EQ
Custom_token Custom_lexer::_lex_equals()
{
  // We know source[pos] == '='
  _advance(); // consume '='
  return Custom_token(Custom_token_kind::EQ);
}

// Lex either "<=" or "<"
Custom_token Custom_lexer::_lex_less_than()
{
  // source[pos] == '<'
  if (_peek_next() == '=')
  {
    _advance(); // consume '<'
    _advance(); // consume '='
    return Custom_token(Custom_token_kind::LTE);
  }
  else
  {
    _advance(); // consume '<'
    return Custom_token(Custom_token_kind::LT);
  }
}

// Lex either ">=" or ">"
Custom_token Custom_lexer::_lex_greater_than()
{
  // source[pos] == '>'
  if (_peek_next() == '=')
  {
    _advance(); // consume '>'
    _advance(); // consume '='
    return Custom_token(Custom_token_kind::GTE);
  }
  else
  {
    _advance(); // consume '>'
    return Custom_token(Custom_token_kind::GT);
  }
}

// Lex either "!=" or throw an error (because every '!' must be followed by '=')
Custom_token Custom_lexer::_lex_not_equal()
{
  // source[pos] == '!'
  if (_peek_next() == '=')
  {
    _advance(); // consume '!'
    _advance(); // consume '='
    return Custom_token(Custom_token_kind::NEQ);
  }
  else
  {
    throw std::runtime_error("Unexpected '!' without '=' (got '!' by itself)");
  }
}

// Lex either ".." or throw an error (single '.' is not valid here).
Custom_token Custom_lexer::_lex_dotdot()
{
  // source[pos] == '.'
  if (_peek_next() == '.')
  {
    _advance(); // consume '.'
    _advance(); // consume second '.'
    return Custom_token(Custom_token_kind::DOTDOT);
  }
  else
  {
    throw std::runtime_error("Single '.' is not a valid token; expected '..'");
  }
}
