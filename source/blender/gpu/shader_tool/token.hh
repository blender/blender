/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 *
 */

#pragma once

#include "parser.hh"

namespace blender::gpu::shader::parser {

struct Scope;

enum TokenType : char {
  Invalid = 0,
  /* Use ascii chars to store them in string, and for easy debugging / testing. */
  Word = 'w',
  NewLine = '\n',
  Space = ' ',
  Dot = '.',
  Hash = '#',
  Ampersand = '&',
  Number = '0',
  String = '_',
  ParOpen = '(',
  ParClose = ')',
  BracketOpen = '{',
  BracketClose = '}',
  SquareOpen = '[',
  SquareClose = ']',
  AngleOpen = '<',
  AngleClose = '>',
  Assign = '=',
  SemiColon = ';',
  Question = '?',
  Not = '!',
  Colon = ':',
  Comma = ',',
  Star = '*',
  Plus = '+',
  Minus = '-',
  Divide = '/',
  Tilde = '~',
  Caret = '^',
  Pipe = '|',
  Percent = '%',
  Backslash = '\\',
  /* Keywords */
  Break = 'b',
  Const = 'c',
  Constexpr = 'C',
  Decrement = 'D',
  Deref = 'D',
  Do = 'd',
  Equal = 'E',
  NotEqual = 'e',
  For = 'f',
  While = 'F',
  GEqual = 'G',
  Case = 'H',
  Switch = 'h',
  Else = 'I',
  If = 'i',
  LEqual = 'L',
  Enum = 'M',
  Static = 'm',
  Namespace = 'n',
  PreprocessorNewline = 'N',
  Continue = 'O',
  Increment = 'P',
  Return = 'r',
  Class = 'S',
  Struct = 's',
  Template = 't',
  This = 'T',
  Using = 'u',
  Private = 'v',
  Public = 'V',
  Inline = 'l',
  Union = 'o',
};

static inline TokenType to_type(const char c)
{
  switch (c) {
    case '\n':
      return TokenType::NewLine;
    case ' ':
      return TokenType::Space;
    case '#':
      return TokenType::Hash;
    case '&':
      return TokenType::Ampersand;
    case '^':
      return TokenType::Caret;
    case '|':
      return TokenType::Pipe;
    case '%':
      return TokenType::Percent;
    case '.':
      return TokenType::Dot;
    case '(':
      return TokenType::ParOpen;
    case ')':
      return TokenType::ParClose;
    case '{':
      return TokenType::BracketOpen;
    case '}':
      return TokenType::BracketClose;
    case '[':
      return TokenType::SquareOpen;
    case ']':
      return TokenType::SquareClose;
    case '<':
      return TokenType::AngleOpen;
    case '>':
      return TokenType::AngleClose;
    case '=':
      return TokenType::Assign;
    case '!':
      return TokenType::Not;
    case '*':
      return TokenType::Star;
    case '-':
      return TokenType::Minus;
    case '+':
      return TokenType::Plus;
    case '/':
      return TokenType::Divide;
    case '~':
      return TokenType::Tilde;
    case '\\':
      return TokenType::Backslash;
    case '\"':
      return TokenType::String;
    case '?':
      return TokenType::Question;
    case ':':
      return TokenType::Colon;
    case ',':
      return TokenType::Comma;
    case ';':
      return TokenType::SemiColon;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return TokenType::Number;
    default:
      return TokenType::Word;
  }
}

struct Token {
  /* String view for nicer debugging experience. Isn't actually used. */
  std::string_view str_view;

  const Parser *data = nullptr;
  int64_t index = 0;

  static Token invalid()
  {
    return {};
  }

  static Token from_position(const Parser *data, int64_t index)
  {
    if (data == nullptr || index < 0 || index > (data->token_offsets.offsets.size() - 2)) {
      return invalid();
    }
    IndexRange index_range = data->token_offsets[index];
    return {std::string_view(data->str).substr(index_range.start, index_range.size), data, index};
  }

  bool is_valid() const
  {
    return data != nullptr && index >= 0;
  }
  bool is_invalid() const
  {
    return !is_valid();
  }

  /* String index range. */
  IndexRange index_range() const
  {
    if (is_invalid()) {
      return {0, 0};
    }
    return data->token_offsets[index];
  }

  Token prev() const
  {
    return from_position(data, index - 1);
  }
  Token next() const
  {
    return from_position(data, index + 1);
  }

  Token find_next(TokenType type) const
  {
    Token tok = this->next();
    while (tok.is_valid() && tok != type) {
      tok = tok.next();
    }
    return tok;
  }

  /* Return start of namespace identifier if the token is part of one. */
  Token namespace_start() const
  {
    if (*this != Word) {
      return *this;
    }
    /* Scan back identifier that could contain namespaces. */
    Token tok = *this;
    while (tok.is_valid()) {
      if (tok.prev() == ':') {
        tok = tok.prev().prev().prev();
      }
      else {
        return tok;
      }
    }
    return tok;
  }

  /* For a word, return the name containing the prefix namespaces if present. */
  std::string full_symbol_name() const
  {
    size_t start = this->namespace_start().str_index_start();
    size_t end = this->str_index_last_no_whitespace();
    return data->str.substr(start, end - start + 1);
  }

  /* Only usable when building with whitespace. */
  Token next_not_whitespace() const
  {
    Token next = this->next();
    while (next == ' ' || next == '\n') {
      next = next.next();
    }
    return next;
  }

  /* Returns the scope that contains this token. */
  Scope scope() const;

  size_t str_index_start() const
  {
    return index_range().start;
  }

  size_t str_index_last() const
  {
    return index_range().last();
  }

  size_t str_index_last_no_whitespace() const
  {
    return data->str.find_last_not_of(" \n", str_index_last());
  }

  /* Index of the first character of the line this token is. */
  size_t line_start() const
  {
    size_t pos = data->str.rfind('\n', str_index_start());
    return (pos == std::string::npos) ? 0 : (pos + 1);
  }

  /* Index of the last character of the line this token is, excluding `\n`. */
  size_t line_end() const
  {
    size_t pos = data->str.find('\n', str_index_start());
    return (pos == std::string::npos) ? (data->str.size() - 1) : (pos - 1);
  }

  std::string str_with_whitespace() const
  {
    return data->str.substr(index_range().start, index_range().size);
  }

  std::string str() const
  {
    if (is_invalid()) {
      return "";
    }
    std::string str = this->str_with_whitespace();
    return str.substr(0, str.find_last_not_of(" \n") + 1);
  }

  /* Return the content without the first and last characters. */
  std::string str_exclusive() const
  {
    std::string str = this->str();
    if (str.length() < 2) {
      return "";
    }
    return str.substr(1, str.length() - 2);
  }

  /* Return the line number this token is found at. Take into account the #line directives.
   * If `at_end` is true, return the line number after this token. */
  size_t line_number(bool at_end = false) const
  {
    if (is_invalid()) {
      return 0;
    }
    if (at_end) {
      return parser::line_number(data->str, str_index_last()) +
             int(data->str[str_index_last()] == '\n');
    }
    return parser::line_number(data->str, str_index_start());
  }

  /* Return the offset to the start of the line. */
  size_t char_number() const
  {
    if (is_invalid()) {
      return 0;
    }
    return parser::char_number(data->str, str_index_start());
  }

  /* Return the line the token is at. */
  std::string line_str() const
  {
    return parser::line_str(data->str, str_index_start());
  }

  TokenType type() const
  {
    if (is_invalid()) {
      return Invalid;
    }
    return TokenType(data->token_types[index]);
  }

  /* Return the attribute scope before this token if it exists. */
  Scope attribute_before() const;
  /* Return the attribute scope after this token if it exists. */
  Scope attribute_after() const;

  bool operator==(TokenType type) const
  {
    return this->type() == type;
  }
  bool operator!=(TokenType type) const
  {
    return !(*this == type);
  }
  bool operator==(char type) const
  {
    return *this == TokenType(type);
  }
  bool operator!=(char type) const
  {
    return *this != TokenType(type);
  }

  bool operator==(const Token &other) const
  {
    return this->index == other.index && this->data == other.data;
  }
  bool operator!=(const Token &other) const
  {
    return !(*this == other);
  }
};

}  // namespace blender::gpu::shader::parser
