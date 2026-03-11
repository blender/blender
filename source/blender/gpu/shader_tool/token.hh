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

/**
 * Semantic token adding access to ParserBase data.
 * It is also safer than lexit::Token as invalid token will not result invalid behavior.
 */
struct Token : lexit::Token {
  Token(const lexit::Token &tok) : lexit::Token(tok) {}

  Token(const ParserBase &data, int64_t index) : lexit::Token(&data, index) {}
  /* Create an invalid token. */
  Token(const ParserBase &data) : lexit::Token(&data, -1) {}

  Token prev(int i = 1) const
  {
    return static_cast<const lexit::Token *>(this)->prev(i);
  }
  Token next(int i = 1) const
  {
    return static_cast<const lexit::Token *>(this)->next(i);
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
    size_t start = namespace_start().str_index_start();
    size_t end = str_index_last_no_whitespace();
    return std::string(buf_->str_.substr(start, end - start + 1));
  }

  /* Returns the scope that contains this token. */
  Scope scope() const;

  size_t str_index_start() const
  {
    return buf_->offsets_[index_];
  }

  size_t str_index_last() const
  {
    return buf_->offsets_[index_ + 1] - 1;
  }

  size_t str_index_last_no_whitespace() const
  {
    return buf_->offsets_end_[index_] - 1;
  }

  /* Index of the first character of the line this token is. */
  size_t line_start() const
  {
    size_t pos = buf_->str_.rfind('\n', str_index_start());
    return (pos == std::string::npos) ? 0 : (pos + 1);
  }

  /* Index of the last character of the line this token is, excluding `\n`. */
  size_t line_end() const
  {
    size_t pos = buf_->str_.find('\n', str_index_start());
    return (pos == std::string::npos) ? (buf_->str_.size() - 1) : (pos - 1);
  }

  std::string_view str_with_whitespace() const
  {
    if (is_invalid()) {
      return "";
    }
    return static_cast<const lexit::Token *>(this)->str_with_whitespace();
  }

  std::string_view str() const
  {
    if (is_invalid()) {
      return "";
    }
    return static_cast<const lexit::Token *>(this)->str();
  }

  /* Return the line number this token is found at. Take into account the #line directives.
   * If `at_end` is true, return the line number after this token. */
  size_t line_number(bool at_end = false) const
  {
    if (is_invalid()) {
      return 0;
    }
    int index = at_end ? str_index_last() : str_index_last_no_whitespace();
    int line_num = parser::line_number(buf_->str_, index);
    /* Add the last char (not counted by line_number). */
    return line_num + int(at_end && buf_->str_[index] == '\n');
  }

  /* Return the offset to the start of the line. */
  size_t char_number() const
  {
    if (is_invalid()) {
      return 0;
    }
    return parser::char_number(buf_->str_, str_index_start());
  }

  /* Return the line the token is at. */
  std::string line_str() const
  {
    return parser::line_str(buf_->str_, str_index_start());
  }

  TokenType type() const
  {
    if (is_invalid()) {
      return Invalid;
    }
    return TokenType(buf_->types_[index_]);
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
};

}  // namespace blender::gpu::shader::parser
