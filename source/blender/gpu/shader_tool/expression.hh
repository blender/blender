/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 *
 * Simple integer logic expression using a Pratt-parser.
 */

#pragma once

#include "pratt_parser.hh"
#include "token.hh"
#include "token_stream.hh"

#include <stdexcept>

namespace blender::gpu::shader::parser {

/**
 * Same as FullLexer but considers angle brackets as multitokens, allowing identification of
 * operators using them.
 */
struct ExpressionLexer {
  static void lexical_analysis(LexerBase &lex, std::string_view input)
  {
    lex.process(input, LexerBase::default_char_class_table.data());
    lex.merge_complex_literals();
    lex.identify_keywords();
  }
};

/**
 * Simple expression parsing and evaluation.
 * Will evaluate starting the given token until the end of the token stream.
 * As this is supposed to be use for preprocessor directives, unknown identifiers (words) will
 * evaluate to 0.
 */
class ExpressionParser : public Parser<ExpressionLexer, NullParser> {
 public:
  int64_t eval() const
  {
    assert(size_ > 0);
    EvalContext ctx((*this)[0]);

    int64_t v = ctx.expr(EvalContext::BindingPower(0));
    if (ctx.peek() != Invalid) {
      throw std::runtime_error("Trailing input");
    }
    return v;
  }

 private:
  struct EvalContext : public PrattParser<EvalContext, int64_t, Token> {
    using Base = PrattParser<EvalContext, int64_t, Token>;
    using ErrorType = Base::ErrorType;
    using BindingPower = Base::BindingPower;

   private:
    Token tok;

   public:
    EvalContext(Token tok) : tok(tok) {}

    int64_t identifier(const Token & /*t*/)
    {
      /* Undefined identifier (not macro substituted). Evaluate to 0. */
      return 0;
    }

    int64_t number_literal(const Token &t)
    {
      return int64_t(std::stoull(std::string(t.str()), nullptr, 0));
    }

    int64_t string_literal(const Token & /*t*/)
    {
      throw std::runtime_error("Invalid expression: strings not supported");
    }

    int64_t prefix(const Token &t, BindingPower p)
    {
      switch (t.type()) {
        case Plus:
          return +expr(p);
        case Minus:
          return -expr(p);
        case Not: {
          int64_t v = expr(p);
          /* Note that '!' token is of MultiTok class and can contain many unary '!'. */
          return (t.str().size() & 1) ? !v : !!v;
        }
        case BitwiseNot:
          return ~expr(p);
        default:
          throw std::runtime_error("Invalid prefix operator");
      }
    }

    int64_t parenthesis(const Token & /*t*/, BindingPower p)
    {
      /* Parse the whole parenthesis expression. */
      int64_t v = expr(p);
      /* Consume the closing parenthesis. */
      if (consume().type() != ParClose) {
        throw std::runtime_error("Expected ')'");
      }
      return v;
    }

    int64_t member(int64_t /*left*/, const Token & /*t*/)
    {
      throw std::runtime_error("Invalid operator: member access not supported");
    }

    int64_t function_call(int64_t /*left*/, const Token & /*t*/)
    {
      throw std::runtime_error("Invalid expression: function call not supported");
    }

    int64_t subscript(int64_t /*left*/, const Token & /*t*/)
    {
      throw std::runtime_error("Invalid operator: subscript not supported");
    }

    int64_t suffix(int64_t /*left*/, const Token & /*t*/)
    {
      throw std::runtime_error("Invalid operator: suffix not supported");
    }

    int64_t comma(int64_t /*left*/, const Token & /*t*/, BindingPower /*p*/)
    {
      throw std::runtime_error("Invalid operator: comma not supported");
    }

    int64_t binary(int64_t left, const Token &t, BindingPower p)
    {
      switch (t.type()) {
        case Multiply:
          return left * expr(p);
        case Divide:
          if (int64_t right = expr(p); right == 0) {
            throw std::runtime_error("Division by zero");
          }
          else {
            return left / right;
          }
        case Modulo:
          if (int64_t right = expr(p); right == 0) {
            throw std::runtime_error("Modulo by zero");
          }
          else {
            return left % right;
          }
        case Plus:
          return left + expr(p);
        case Minus:
          return left - expr(p);
        case LShift:
          return left << expr(p);
        case RShift:
          return left >> expr(p);
        case LThan:
          return left < expr(p);
        case LEqual:
          return left <= expr(p);
        case GThan:
          return left > expr(p);
        case GEqual:
          return left >= expr(p);
        case Equal:
          return left == expr(p);
        case NotEqual:
          return left != expr(p);
        case And:
          return left & expr(p);
        case Xor:
          return left ^ expr(p);
        case Or:
          return left | expr(p);
        case LogicalAnd: {
          /* Avoid short circuit. */
          int64_t right = expr(p);
          return left && right;
        }
        case LogicalOr: {
          /* Avoid short circuit. */
          int64_t right = expr(p);
          return left || right;
        }
        default:
          throw std::runtime_error("Invalid operator");
      }
    }

    int64_t ternary(int64_t left, const Token & /*t*/, BindingPower p_first, BindingPower p_second)
    {
      int64_t tval = expr(p_first);
      if (consume().type() != Colon) {
        error(ErrorType::ExpectedColon, Colon);
      }
      int64_t fval = expr(p_second);
      return left ? tval : fval;
    }

    int64_t error(ErrorType err, lexit::TokenType type)
    {
      switch (err) {
        case ErrorType::ExpectedParenthesis:
          throw std::runtime_error("Expected ')'");
        case ErrorType::ExpectedColon:
          throw std::runtime_error("Expected ':'");
        case ErrorType::InvalidOperator:
          throw std::runtime_error("Invalid operator " + to_str(type));
        case ErrorType::InvalidExpression:
          throw std::runtime_error("Invalid expression");
      }
      throw std::runtime_error("Parse error");
    }

    TokenType peek() const
    {
      return tok.type();
    }

    Token consume()
    {
      Token t = tok;
      tok = tok.next();
      return t;
    }

    TokenType to_type(const Token &t) const
    {
      return t.type();
    }
  };
};

}  // namespace blender::gpu::shader::parser
