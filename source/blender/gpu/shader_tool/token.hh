/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 *
 */

#pragma once

#include "token_stream.hh"

namespace blender::gpu::shader::parser {

struct Scope;

struct Token {
#ifndef NDEBUG
  /* String view for nicer debugging experience. Isn't actually used. */
  std::string_view str_view_debug;
#endif

  const ParserBase *data = nullptr;
  int64_t index = 0;

  static Token invalid()
  {
    return {};
  }

  static Token from_position(const ParserBase *data, int64_t index)
  {
    if (data == nullptr || index < 0 || index >= data->lex.token_offsets.size()) {
      return invalid();
    }
#ifndef NDEBUG
    IndexRange index_range = data->lex.token_offsets[index];
    return {data->lex.str.substr(index_range.start, index_range.size), data, index};
#else
    return {data, index};
#endif
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
    return data->lex.token_offsets[index];
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
    return std::string(data->lex.str.substr(start, end - start + 1));
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
    return data->lex.str.find_last_not_of(" \n", str_index_last());
  }

  /* Index of the first character of the line this token is. */
  size_t line_start() const
  {
    size_t pos = data->lex.str.rfind('\n', str_index_start());
    return (pos == std::string::npos) ? 0 : (pos + 1);
  }

  /* Index of the last character of the line this token is, excluding `\n`. */
  size_t line_end() const
  {
    size_t pos = data->lex.str.find('\n', str_index_start());
    return (pos == std::string::npos) ? (data->lex.str.size() - 1) : (pos - 1);
  }

  std::string_view str_view_with_whitespace() const
  {
    if (is_invalid()) {
      return "";
    }
    return data->lex.str.substr(index_range().start, index_range().size);
  }

  std::string str_with_whitespace() const
  {
    return std::string(str_view_with_whitespace());
  }

  std::string_view str_view() const
  {
    std::string_view str = this->str_view_with_whitespace();
    return str.substr(0, str.find_last_not_of(" \n") + 1);
  }

  std::string str() const
  {
    return std::string(str_view());
  }

  /* Return the content without the first and last characters. */
  std::string_view str_view_exclusive() const
  {
    std::string_view str = this->str_view();
    if (str.length() < 2) {
      return "";
    }
    return str.substr(1, str.length() - 2);
  }

  /* Return the content without the first and last characters. */
  std::string str_exclusive() const
  {
    return std::string(str_view_exclusive());
  }

  /* Return the line number this token is found at. Take into account the #line directives.
   * If `at_end` is true, return the line number after this token. */
  size_t line_number(bool at_end = false) const
  {
    if (is_invalid()) {
      return 0;
    }
    if (at_end) {
      return parser::line_number(data->lex.str, str_index_last()) +
             int(data->lex.str[str_index_last()] == '\n');
    }
    return parser::line_number(data->lex.str, str_index_start());
  }

  /* Return the offset to the start of the line. */
  size_t char_number() const
  {
    if (is_invalid()) {
      return 0;
    }
    return parser::char_number(data->lex.str, str_index_start());
  }

  /* Return the line the token is at. */
  std::string line_str() const
  {
    return parser::line_str(data->lex.str, str_index_start());
  }

  TokenType type() const
  {
    if (is_invalid()) {
      return Invalid;
    }
    return TokenType(data->lex.token_types[index]);
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
