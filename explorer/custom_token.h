#ifndef CUSTOM_TOKEN_H
#define CUSTOM_TOKEN_H

#include <QObject>

enum class Custom_token_kind
{
  END_OF_EXPR,

  IDENTIFIER,
  INT_LITERAL,

  KW_AND,
  KW_OR,
  KW_NOT,
  KW_IN,

  EQ,  // =
  NEQ, // !=
  LT,  // <
  LTE, // <=
  GT,  // >
  GTE, // >=

  DOTDOT, // ..
  LPAREN, // (
  RPAREN, // )
  COMMA,  // ,

  PLUS, // +
  MINUS // -
};

struct Custom_token
{
  Custom_token_kind kind;
  QString text;
  Custom_token(Custom_token_kind k)
    : kind(k)
    , text("")
  {
  }
  Custom_token(Custom_token_kind k, const QString& t)
    : kind(k)
    , text(t)
  {
  }
};

#endif // CUSTOM_TOKEN_H
