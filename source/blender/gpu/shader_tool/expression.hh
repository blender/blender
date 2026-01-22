/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 *
 * Simple integer logic expression using a Pratt-parser.
 */

#pragma once

#include "token.hh"
#include "token_stream.hh"

#include <stdexcept>

namespace blender::gpu::shader::parser {

/**
 * Simple expression parsing and evaluation.
 * Will evaluate starting the given token until the end of the token stream.
 * As this is supposed to be use for preprocessor directives, unknown identifiers (words) will
 * evaluate to 0.
 */
class ExpressionParser : ParserBase {
 private:
  Token tok;

 public:
  explicit ExpressionParser(const ExpressionLexer &lex) : ParserBase(lex) {}

  int64_t eval()
  {
    tok = (*this)[0];

    int64_t v = expr(0);
    if (peek() != Invalid) {
      throw std::runtime_error("Trailing input");
    }
    return v;
  }

 private:
  int64_t expr(int right_binding_power)
  {
    /* Parse unary operator, evaluate parenthesis, evaluate constant. */
    int64_t left = nud(consume());
    /* While left binding power is greater than the right, continue consuming binary operations. */
    while (left_binding_power(peek().type()) > right_binding_power) {
      left = led(left, consume());
    }
    return left;
  }

  /* How a token evaluates without left context (e.g. unary operator).
   * Also known as Null-Denotation or NUD. */
  int64_t nud(const Token &t)
  {
    /* Unary operators must have the highest precedence. */
    static constexpr int unary_binding_power = 1000;
    /* Let parenthesis evaluate everything until a closing parenthesis. */
    static constexpr int parenthesis_binding_power = 0;

    switch (t.type()) {
      case Word:
        /* Undefined identifier (not macro substituted). Evaluate to 0. */
        return 0;
      case Number:
        return std::stol(t.str());
      case Plus:
        return +expr(unary_binding_power);
      case Minus:
        return -expr(unary_binding_power);
      case Not: {
        int v = expr(unary_binding_power);
        /* Note that '!' token is of MultiTok class and can contain many unary '!'. */
        return (t.str_view().size() & 1) ? !v : !!v;
      }
      case BitwiseNot:
        return ~expr(unary_binding_power);
      case ParOpen: {
        /* Parse the whole parenthesis expression. */
        int64_t v = expr(parenthesis_binding_power);
        /* Consume the closing parenthesis. */
        if (consume() != ParClose) {
          throw std::runtime_error("Expected ')'");
        }
        return v;
      }
      default:
        throw std::runtime_error("Invalid expression");
    }
  }

  /* How a token evaluates from left-to-right, on two operands.
   * Also known as Left-Denotation or LED. */
  int64_t led(int64_t left, const Token &t)
  {
    switch (t.type()) {
      case Multiply:
        return left * expr(left_binding_power(Multiply));
      case Divide: {
        int64_t right = expr(left_binding_power(Divide));
        if (right == 0) {
          throw std::runtime_error("Division by zero");
        }
        return left / right;
      }
      case Modulo: {
        int64_t right = expr(left_binding_power(Modulo));
        if (right == 0) {
          throw std::runtime_error("Modulo by zero");
        }
        return left % right;
      }
      case Plus:
        return left + expr(left_binding_power(Plus));
      case Minus:
        return left - expr(left_binding_power(Minus));
#if 0 /* Not implemented yet. */
      case LShift:
        return left << expression(binding_power(LShift));
      case RShift:
        return left >> expression(binding_power(RShift));
#endif
      case LThan:
        return left < expr(left_binding_power(LThan));
      case LEqual:
        return left <= expr(left_binding_power(LEqual));
      case GThan:
        return left > expr(left_binding_power(GThan));
      case GEqual:
        return left >= expr(left_binding_power(GEqual));
      case Equal:
        return left == expr(left_binding_power(Equal));
      case NotEqual:
        return left != expr(left_binding_power(NotEqual));
      case And:
        return left & expr(left_binding_power(And));
      case Xor:
        return left ^ expr(left_binding_power(Xor));
      case Or:
        return left | expr(left_binding_power(Or));
      case LogicalAnd: {
        /* Avoid short circuit. */
        int right = expr(left_binding_power(LogicalAnd));
        return left && right;
      }
      case LogicalOr: {
        /* Avoid short circuit. */
        int right = expr(left_binding_power(LogicalOr));
        return left || right;
      }
      case Question: {
        /* The middle expression can be almost anything.
         * We use 0 so it only stops at the ':' (since Colon has a precedence of 0). */
        int64_t tval = expr(0);
        if (consume().type() != Colon) {
          throw std::runtime_error("Expected ':'");
        }
        /* Use (Precedence - 1) to handle right-associativity. */
        int64_t fval = expr(left_binding_power(Question) - 1);
        return left ? tval : fval;
      }
      default:
        throw std::runtime_error("Invalid operator");
    }
  }

  int left_binding_power(TokenType type)
  {
    switch (type) {
      case Multiply:
      case Divide:
      case Modulo:
        return 110;
      case Plus:
      case Minus:
        return 100;
#if 0 /* Not implemented yet. */
      case LShift:
      case RShift:
        return 90;
#endif
      case LThan:
      case LEqual:
      case GThan:
      case GEqual:
        return 80;
      case Equal:
      case NotEqual:
        return 70;
      case And:
        return 60;
      case Xor:
        return 50;
      case Or:
        return 40;
      case LogicalAnd:
        return 30;
      case LogicalOr:
        return 20;
      case Question:
        return 10;
      case Colon:
      case ParOpen:
      case ParClose:
        return 0;
      case Not:
      case BitwiseNot:
        /* Prefix operators don't bind to the left! */
        return 0;
      case Invalid: /* EndOfFile */
        return -1;
      default:
        break;
    }
    throw std::runtime_error("Invalid token");
    return 0;
  }

  Token peek() const
  {
    return tok;
  }

  Token consume()
  {
    Token t = tok;
    tok = tok.next();
    return t;
  }
};

}  // namespace blender::gpu::shader::parser
