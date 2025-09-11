/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup glsl_preprocess
 *
 * Very simple parsing of our shader file that are a subset of C++. It allows to traverse the
 * semantic using tokens and scopes instead of trying to match string patterns throughout the whole
 * input string.
 *
 * The goal of this representation is to output code that doesn't modify the style of the input
 * string and keep the same line numbers (to match compilation error with input source).
 *
 * The `Parser` class contain a copy of the given string to apply string substitutions (called
 * `Mutation`). It is usually faster to record all of them and apply them all at once after
 * scanning through the whole semantic representation. In the rare case where mutation need to
 * overlap (recursive processing), it is better to do them in passes until there is no mutation to
 * do.
 *
 * `Token` and `Scope` are read only interfaces to the data stored inside the `ParserData`.
 * The data is stored as SoA (Structure of Arrays) for fast traversal.
 * The types of token and scopes are defined as readable chars to easily create sequences of token
 * type.
 *
 * The `Parser` object needs to be fed a well formed source (without preprocessor directive, see
 * below), otherwise a crash can occur. The `Parser` doesn't apply any preprocessor. All
 * preprocessor directive are parsed as `Preprocessor` scope but they are not expanded.
 *
 * By default, whitespaces are merged with the previous token. Only a handful of processing
 * requires access to whitespaces as individual tokens.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stack>
#include <string>
#include <vector>

namespace blender::gpu::shader::parser {

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
};

enum class ScopeType : char {
  /* Use ascii chars to store them in string, and for easy debugging / testing. */
  Global = 'G',
  Namespace = 'N',
  Struct = 'S',
  Function = 'F',
  LoopArgs = 'l',
  LoopBody = 'p',
  SwitchArg = 'w',
  SwitchBody = 'W',
  FunctionArgs = 'f',
  FunctionCall = 'c',
  Template = 'T',
  TemplateArg = 't',
  Subscript = 'A',
  Preprocessor = 'P',
  Assignment = 'a',
  /* Added scope inside function body. */
  Local = 'L',
  /* Added scope inside FunctionArgs. */
  FunctionArg = 'g',
  /* Added scope inside LoopArgs. */
  LoopArg = 'r',

};

/* Poor man's IndexRange. */
struct IndexRange {
  int64_t start;
  int64_t size;

  IndexRange(size_t start, size_t size) : start(start), size(size) {}

  bool overlaps(IndexRange other) const
  {
    return ((start < other.start) && (other.start < (start + size))) ||
           ((other.start < start) && (start < (other.start + other.size)));
  }

  int64_t last()
  {
    return start + size - 1;
  }
};

/* Poor man's OffsetIndices. */
struct OffsetIndices {
  std::vector<size_t> offsets;

  IndexRange operator[](const int64_t index) const
  {
    return {offsets[index], offsets[index + 1] - offsets[index]};
  }

  void clear()
  {
    offsets.clear();
  };
};

struct Scope;

struct ParserData {
  std::string str;

  std::string token_types;
  std::string scope_types;
  /* Ranges of characters per token. */
  OffsetIndices token_offsets;
  /* Index of bottom most scope per token. */
  std::vector<int> token_scope;
  /* Range of token per scope. */
  std::vector<IndexRange> scope_ranges;

  /* If keep_whitespace is false, whitespaces are merged with the previous token. */
  void tokenize(const bool keep_whitespace)
  {
    if (str.empty()) {
      *this = {};
      return;
    }

    {
      /* Tokenization. */
      token_types.clear();
      token_offsets.clear();

      token_types += char(to_type(str[0]));
      token_offsets.offsets.emplace_back(0);

      /* When doing whitespace merging, keep knowledge about whether previous char was whitespace.
       * This allows to still split words on spaces. */
      bool prev_was_whitespace = (token_types[0] == NewLine || token_types[0] == Space);
      bool inside_preprocessor_directive = token_types[0] == Hash;
      bool next_character_is_escape = false;
      bool inside_string = false;

      int offset = 0;
      for (const char &c : str.substr(1)) {
        offset++;
        TokenType type = to_type(c);
        TokenType prev = TokenType(token_types.back());

        /* Merge string literal. */
        if (inside_string) {
          if (!next_character_is_escape && c == '\"') {
            inside_string = false;
          }
          next_character_is_escape = c == '\\';
          continue;
        }
        if (c == '\"') {
          inside_string = true;
        }
        /* Detect preprocessor directive newlines `\\\n`. */
        if (prev == Backslash && type == NewLine) {
          token_types.back() = PreprocessorNewline;
          continue;
        }
        /* Make sure to keep the ending newline for a preprocessor directive. */
        if (inside_preprocessor_directive && type == NewLine) {
          inside_preprocessor_directive = false;
          token_types += char(type);
          token_offsets.offsets.emplace_back(offset);
          continue;
        }
        if (type == Hash) {
          inside_preprocessor_directive = true;
        }
        /* Merge newlines and spaces with previous token. */
        if (!keep_whitespace && (type == NewLine || type == Space)) {
          prev_was_whitespace = true;
          continue;
        }
        /* Merge '=='. */
        if (prev == Assign && type == Assign) {
          token_types.back() = Equal;
          continue;
        }
        /* Merge '!='. */
        if (prev == '!' && type == Assign) {
          token_types.back() = NotEqual;
          continue;
        }
        /* Merge '>='. */
        if (prev == '>' && type == Assign) {
          token_types.back() = GEqual;
          continue;
        }
        /* Merge '<='. */
        if (prev == '<' && type == Assign) {
          token_types.back() = LEqual;
          continue;
        }
        /* Merge '->'. */
        if (prev == '-' && type == '>') {
          token_types.back() = Deref;
          continue;
        }
        /* If digit is part of word. */
        if (type == Number && prev == Word && !prev_was_whitespace) {
          continue;
        }
        /* If 'x' is part of hex literal. */
        if (c == 'x' && prev == Number) {
          continue;
        }
        /* If 'A-F' is part of hex literal. */
        if (c >= 'A' && c <= 'F' && prev == Number) {
          continue;
        }
        /* If 'a-f' is part of hex literal. */
        if (c >= 'a' && c <= 'f' && prev == Number) {
          continue;
        }
        /* If 'u' is part of unsigned int literal. */
        if (c == 'u' && prev == Number) {
          continue;
        }
        /* If dot is part of float literal. */
        if (type == Dot && prev == Number) {
          continue;
        }
        /* If 'f' suffix is part of float literal. */
        if (c == 'f' && prev == Number) {
          continue;
        }
        /* If 'e' is part of float literal. */
        if (c == 'e' && prev == Number) {
          continue;
        }
        /* If sign is part of float literal after exponent. */
        if ((c == '+' || c == '-') && prev == Number) {
          continue;
        }
        /* Detect increment. */
        if (type == '+' && prev == '+') {
          token_types.back() = Increment;
          continue;
        }
        /* Detect decrement. */
        if (type == '+' && prev == '+') {
          token_types.back() = Decrement;
          continue;
        }
        /* Only merge these token. Otherwise, always emit a token. */
        if (type != Word && type != NewLine && type != Space && type != Number) {
          prev = Word;
        }
        /* Split words on whitespaces even when merging. */
        if (!keep_whitespace && type == Word && prev_was_whitespace) {
          prev = Space;
          prev_was_whitespace = false;
        }
        /* Emit a token if we don't merge. */
        if (type != prev) {
          token_types += char(type);
          token_offsets.offsets.emplace_back(offset);
        }
      }
      offset++;
      token_offsets.offsets.emplace_back(offset);
    }
    {
      /* Keywords detection. */
      int tok_id = -1;
      for (char &c : token_types) {
        tok_id++;
        if (TokenType(c) == Word) {
          IndexRange range = token_offsets[tok_id];
          std::string word = str.substr(range.start, range.size);
          if (!keep_whitespace) {
            size_t last_non_whitespace = word.find_last_not_of(" \n");
            if (last_non_whitespace != std::string::npos) {
              word = word.substr(0, last_non_whitespace + 1);
            }
          }

          if (word == "namespace") {
            c = Namespace;
          }
          else if (word == "struct") {
            c = Struct;
          }
          else if (word == "class") {
            c = Class;
          }
          else if (word == "const") {
            c = Const;
          }
          else if (word == "constexpr") {
            c = Constexpr;
          }
          else if (word == "return") {
            c = Return;
          }
          else if (word == "break") {
            c = Break;
          }
          else if (word == "continue") {
            c = Continue;
          }
          else if (word == "case") {
            c = Case;
          }
          else if (word == "switch") {
            c = Switch;
          }
          else if (word == "if") {
            c = If;
          }
          else if (word == "else") {
            c = Else;
          }
          else if (word == "while") {
            c = While;
          }
          else if (word == "do") {
            c = Do;
          }
          else if (word == "for") {
            c = For;
          }
          else if (word == "template") {
            c = Template;
          }
          else if (word == "this") {
            c = This;
          }
          else if (word == "static") {
            c = Static;
          }
          else if (word == "private") {
            c = Private;
          }
          else if (word == "public") {
            c = Public;
          }
          else if (word == "enum") {
            c = Enum;
          }
          else if (word == "using") {
            c = Using;
          }
        }
      }
    }
  }

  using report_callback = std::function<void(
      int error_line, int error_char, std::string error_line_string, const char *error_str)>;

  void parse_scopes(report_callback &report_error);

 private:
  TokenType to_type(const char c)
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
      case '9':
        return TokenType::Number;
      default:
        return TokenType::Word;
    }
  }
};

struct Token {
  /* String view for nicer debugging experience. Isn't actually used. */
  std::string_view str_view;

  const ParserData *data;
  int64_t index;

  static Token invalid()
  {
    return {"", nullptr, 0};
  }

  static Token from_position(const ParserData *data, int64_t index)
  {
    if (index < 0 || index > (data->token_offsets.offsets.size() - 2)) {
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

  /* Return the first container scope of this token that has the given type.
   * Returns invalid scope on failure. */
  Scope first_containing_scope_of_type(const ScopeType type) const;

  /* Return start of namespace identifier is the token is part of one. */
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

  /* Return the line number this token is found at. Take into account the #line directives. */
  size_t line_number() const
  {
    std::string directive = "#line ";
    /* String to count the number of line. */
    std::string sub_str = data->str.substr(0, str_index_start());
    size_t nearest_line_directive = sub_str.rfind(directive);
    size_t line_count = 1;
    if (nearest_line_directive != std::string::npos) {
      sub_str = sub_str.substr(nearest_line_directive + directive.size());
      line_count = std::stoll(sub_str) - 1;
    }
    return line_count + std::count(sub_str.begin(), sub_str.end(), '\n');
  }

  /* Return the offset to the start of the line. */
  size_t char_number() const
  {
    std::string sub_str = data->str.substr(0, str_index_start());
    size_t nearest_line_directive = sub_str.rfind('\n');
    return (nearest_line_directive == std::string::npos) ?
               (sub_str.size()) :
               (sub_str.size() - nearest_line_directive - 1);
  }

  /* Return the line the token is at. */
  std::string line_str() const
  {
    size_t start = data->str.rfind('\n', str_index_start());
    size_t end = data->str.find('\n', str_index_start());
    start = (start != std::string::npos) ? start + 1 : 0;
    return data->str.substr(start, end - start);
  }

  TokenType type() const
  {
    if (is_invalid()) {
      return Invalid;
    }
    return TokenType(data->token_types[index]);
  }

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

struct Scope {
  /* String view for nicer debugging experience. Isn't actually used. */
  std::string_view token_view;
  std::string_view str_view;

  const ParserData *data;
  int64_t index;

  static Scope from_position(const ParserData *data, int64_t index)
  {
    IndexRange index_range = data->scope_ranges[index];
    int str_start = data->token_offsets[index_range.start].start;
    int str_end = data->token_offsets[index_range.last()].last();
    return {std::string_view(data->token_types).substr(index_range.start, index_range.size),
            std::string_view(data->str).substr(str_start, str_end - str_start + 1),
            data,
            index};
  }

  Token start() const
  {
    return Token::from_position(data, range().start);
  }

  Token end() const
  {
    return Token::from_position(data, range().last());
  }

  IndexRange range() const
  {
    return data->scope_ranges[index];
  }

  Token operator[](const int64_t index) const
  {
    return Token::from_position(data, range().start + index);
  }

  size_t token_count() const
  {
    return range().size;
  }

  ScopeType type() const
  {
    return ScopeType(data->scope_types[index]);
  }

  /* Returns the scope that contains this scope. */
  Scope scope() const
  {
    return start().prev().scope();
  }

  static Scope invalid()
  {
    return {"", "", nullptr, -1};
  }

  bool is_valid() const
  {
    return data != nullptr && index >= 0;
  }
  bool is_invalid() const
  {
    return !is_valid();
  }

  bool contains(const Scope sub) const
  {
    Scope parent = sub.scope();
    while (parent.type() != ScopeType::Global && parent != *this) {
      parent = parent.scope();
    }
    return parent == *this;
  }

  std::string str() const
  {
    if (this->is_invalid()) {
      return "";
    }
    return data->str.substr(start().str_index_start(),
                            end().str_index_last() - start().str_index_start() + 1);
  }

  /* Return the content without the first and last characters. */
  std::string str_exclusive() const
  {
    if (this->is_invalid()) {
      return "";
    }
    return data->str.substr(start().str_index_start() + 1,
                            end().str_index_last() - start().str_index_start() - 1);
  }

  Token find_token(const char token_type) const
  {
    size_t pos = data->token_types.substr(range().start, range().size).find(token_type);
    return (pos != std::string::npos) ? Token::from_position(data, range().start + pos) :
                                        Token::invalid();
  }

  bool contains_token(const char token_type) const
  {
    return find_token(token_type).is_valid();
  }

  void foreach_match(const std::string &pattern,
                     std::function<void(const std::vector<Token>)> callback) const
  {
    assert(!pattern.empty());
    const std::string_view scope_tokens =
        std::string_view(data->token_types).substr(range().start, range().size);

    auto count_match = [](const std::string_view &s, const std::string_view &pattern) {
      size_t pos = 0, occurrences = 0;
      while ((pos = s.find(pattern, pos)) != std::string::npos) {
        occurrences += 1;
        pos += pattern.length();
      }
      return occurrences;
    };
    const int control_token_count = count_match(pattern, "?") * 2 + count_match(pattern, "..") * 2;

    if (range().size < pattern.size() - control_token_count) {
      return;
    }

    const size_t searchable_range = scope_tokens.size() -
                                    (pattern.size() - 1 - control_token_count);

    std::vector<Token> match;
    match.resize(pattern.size());

    for (size_t pos = 0; pos < searchable_range; pos++) {
      size_t cursor = range().start + pos;

      for (int i = 0; i < pattern.size(); i++) {
        bool is_last_token = i == pattern.size() - 1;
        TokenType token_type = TokenType(data->token_types[cursor]);
        TokenType curr_search_token = TokenType(pattern[i]);
        TokenType next_search_token = TokenType(is_last_token ? '\0' : pattern[i + 1]);

        /* Scope skipping. */
        if (!is_last_token && curr_search_token == '.' && next_search_token == '.') {
          cursor = match[i - 1].scope().end().index;
          i++;
          continue;
        }

        /* Regular token. */
        if (curr_search_token == token_type) {
          match[i] = Token::from_position(data, cursor++);

          if (is_last_token) {
            callback(match);
          }
        }
        else if (!is_last_token && curr_search_token != '?' && next_search_token == '?') {
          /* This was and optional token. Continue scanning. */
          match[i] = Token::invalid();
          i++;
        }
        else {
          /* Token mismatch. Test next position. */
          break;
        }
      }
    }
  }

  /* Will iterate over all the scopes that are direct children. */
  void foreach_scope(ScopeType type, std::function<void(Scope)> callback) const
  {
    size_t pos = this->index;
    while ((pos = data->scope_types.find(char(type), pos)) != std::string::npos) {
      Scope scope = Scope::from_position(data, pos);
      if (scope.start().index > this->end().index) {
        /* Found scope starts after this scope. End iteration. */
        break;
      }
      /* Make sure found scope is direct child of this scope. */
      if (scope.start().scope().scope().index == this->index) {
        callback(scope);
      }
      pos += 1;
    }
  }

  void foreach_token(const TokenType token_type, std::function<void(const Token)> callback) const
  {
    const char str[2] = {token_type, '\0'};
    foreach_match(str, [&](const std::vector<Token> &tokens) { callback(tokens[0]); });
  }

  /* Run a callback for all existing function scopes. */
  void foreach_function(
      std::function<void(
          bool is_static, Token type, Token name, Scope args, bool is_const, Scope body)> callback)
      const
  {
    foreach_match("m?ww(..)c?{..}", [&](const std::vector<Token> matches) {
      callback(matches[0] == Static,
               matches[2],
               matches[3],
               matches[4].scope(),
               matches[8] == Const,
               matches[10].scope());
    });
    foreach_match("m?ww::w(..)c?{..}", [&](const std::vector<Token> matches) {
      callback(matches[0] == Static,
               matches[2],
               matches[6],
               matches[7].scope(),
               matches[11] == Const,
               matches[13].scope());
    });
    foreach_match("m?ww<..>(..)c?{..}", [&](const std::vector<Token> matches) {
      callback(matches[0] == Static,
               matches[2],
               matches[3],
               matches[8].scope(),
               matches[12] == Const,
               matches[14].scope());
    });
  }

  /* Run a callback for all existing struct scopes. */
  void foreach_struct(std::function<void(Token struct_tok, Token name, Scope body)> callback) const
  {
    foreach_match("sw{..}", [&](const std::vector<Token> matches) {
      callback(matches[0], matches[1], matches[2].scope());
    });
    foreach_match("Sw{..}", [&](const std::vector<Token> matches) {
      callback(matches[0], matches[1], matches[2].scope());
    });
    foreach_match("sw<..>{..}", [&](const std::vector<Token> matches) {
      callback(matches[0], matches[1], matches[6].scope());
    });
    foreach_match("Sw<..>{..}", [&](const std::vector<Token> matches) {
      callback(matches[0], matches[1], matches[6].scope());
    });
  }

  bool operator==(const Scope &other) const
  {
    return this->index == other.index && this->data == other.data;
  }
  bool operator!=(const Scope &other) const
  {
    return !(*this == other);
  }
};

inline Scope Token::scope() const
{
  return Scope::from_position(data, data->token_scope[index]);
}

inline Scope Token::first_containing_scope_of_type(const ScopeType type) const
{
  Scope scope = this->scope();
  while (scope.type() != ScopeType::Global && scope.type() != type) {
    scope = scope.scope();
  }
  return scope.type() == type ? scope : Scope::invalid();
}

inline void ParserData::parse_scopes(report_callback &report_error)
{
  {
    /* Scope detection. */
    scope_ranges.clear();
    scope_types.clear();

    struct ScopeItem {
      ScopeType type;
      size_t start;
      int index;
    };

    int scope_index = 0;
    std::stack<ScopeItem> scopes;

    auto enter_scope = [&](ScopeType type, size_t start_tok_id) {
      scopes.emplace(ScopeItem{type, start_tok_id, scope_index++});
      scope_ranges.emplace_back(start_tok_id, 1);
      scope_types += char(type);
    };

    auto exit_scope = [&](int end_tok_id) {
      ScopeItem scope = scopes.top();
      scope_ranges[scope.index].size = end_tok_id - scope.start + 1;
      scopes.pop();
    };

    enter_scope(ScopeType::Global, 0);

    int in_template = 0;

    int tok_id = -1;
    for (char &c : token_types) {
      tok_id++;

      if (scopes.top().type == ScopeType::Preprocessor) {
        if (TokenType(c) == NewLine) {
          exit_scope(tok_id);
        }
        else {
          /* Do nothing. Enclose all preprocessor lines together. */
          continue;
        }
      }

      switch (TokenType(c)) {
        case Hash:
          enter_scope(ScopeType::Preprocessor, tok_id);
          break;
        case Assign:
          if (scopes.top().type == ScopeType::Assignment) {
            /* Chained assignments. */
            exit_scope(tok_id - 1);
          }
          enter_scope(ScopeType::Assignment, tok_id);
          break;
        case BracketOpen: {
          /* Scan back identifier that could contain namespaces. */
          TokenType keyword;
          int pos = 2;
          do {
            keyword = (tok_id >= pos) ? TokenType(token_types[tok_id - pos]) : TokenType::Invalid;
            pos += 3;
          } while (keyword != Invalid && keyword == Colon);

          if (keyword == Struct) {
            enter_scope(ScopeType::Local, tok_id);
          }
          else if (keyword == Enum) {
            enter_scope(ScopeType::Local, tok_id);
          }
          else if (keyword == Namespace) {
            enter_scope(ScopeType::Namespace, tok_id);
          }
          else if (ScopeType(scope_types.back()) == ScopeType::LoopArg) {
            enter_scope(ScopeType::LoopBody, tok_id);
          }
          else if (ScopeType(scope_types.back()) == ScopeType::SwitchArg) {
            enter_scope(ScopeType::SwitchBody, tok_id);
          }
          else if (scopes.top().type == ScopeType::Global) {
            enter_scope(ScopeType::Function, tok_id);
          }
          else if (scopes.top().type == ScopeType::Struct) {
            enter_scope(ScopeType::Function, tok_id);
          }
          else if (scopes.top().type == ScopeType::Namespace) {
            enter_scope(ScopeType::Function, tok_id);
          }
          else {
            enter_scope(ScopeType::Local, tok_id);
          }
          break;
        }
        case ParOpen:
          if ((tok_id >= 1 && token_types[tok_id - 1] == For) ||
              (tok_id >= 1 && token_types[tok_id - 1] == While))
          {
            enter_scope(ScopeType::LoopArgs, tok_id);
          }
          else if (tok_id >= 1 && token_types[tok_id - 1] == Switch) {
            enter_scope(ScopeType::SwitchArg, tok_id);
          }
          else if (scopes.top().type == ScopeType::Global) {
            enter_scope(ScopeType::FunctionArgs, tok_id);
          }
          else if (scopes.top().type == ScopeType::Struct) {
            enter_scope(ScopeType::FunctionArgs, tok_id);
          }
          else if ((scopes.top().type == ScopeType::Function ||
                    scopes.top().type == ScopeType::Local) &&
                   (tok_id >= 1 && token_types[tok_id - 1] == Word))
          {
            enter_scope(ScopeType::FunctionCall, tok_id);
          }
          else {
            enter_scope(ScopeType::Local, tok_id);
          }
          break;
        case SquareOpen:
          enter_scope(ScopeType::Subscript, tok_id);
          break;
        case AngleOpen:
          if (tok_id >= 1) {
            char prev_char = str[token_offsets[tok_id - 1].last()];
            /* Rely on the fact that template are formatted without spaces but comparison isn't. */
            if ((prev_char != ' ' && prev_char != '\n' && prev_char != '<') ||
                token_types[tok_id - 1] == Template)
            {
              enter_scope(ScopeType::Template, tok_id);
              in_template++;
            }
          }
          break;
        case AngleClose:
          if (in_template > 0 && scopes.top().type == ScopeType::Assignment) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::TemplateArg) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::Template) {
            exit_scope(tok_id);
            in_template--;
          }
          break;
        case BracketClose:
        case ParClose:
          if (scopes.top().type == ScopeType::Assignment) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::FunctionArg) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::LoopArg) {
            exit_scope(tok_id - 1);
          }
          exit_scope(tok_id);
          break;
        case SquareClose:
          exit_scope(tok_id);
          break;
        case SemiColon:
          if (scopes.top().type == ScopeType::Assignment) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::FunctionArg) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::TemplateArg) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::LoopArg) {
            exit_scope(tok_id - 1);
          }
          break;
        case Comma:
          if (scopes.top().type == ScopeType::Assignment) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::FunctionArg) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::TemplateArg) {
            exit_scope(tok_id - 1);
          }
          break;
        default:
          if (scopes.top().type == ScopeType::FunctionArgs) {
            enter_scope(ScopeType::FunctionArg, tok_id);
          }
          if (scopes.top().type == ScopeType::LoopArgs) {
            enter_scope(ScopeType::LoopArg, tok_id);
          }
          if (scopes.top().type == ScopeType::Template) {
            enter_scope(ScopeType::TemplateArg, tok_id);
          }
          break;
      }
    }

    if (scopes.top().type == ScopeType::Preprocessor) {
      exit_scope(tok_id - 1);
    }

    if (scopes.top().type != ScopeType::Global) {
      ScopeItem scope_item = scopes.top();
      Token token = Token::from_position(this, scope_ranges[scope_item.index].start);
      report_error(
          token.line_number(), token.char_number(), token.line_str(), "unterminated scope");

      /* Avoid out of bound access for the rest of the processing. Empty everything. */
      *this = {};
      return;
    }

    exit_scope(tok_id);
  }
  {
    token_scope.clear();
    token_scope.resize(scope_ranges[0].size);

    int scope_id = -1;
    for (const IndexRange &range : scope_ranges) {
      scope_id++;
      for (int i = 0; i < range.size; i++) {
        int j = range.start + i;
        token_scope[j] = scope_id;
      }
    }
  }
}

struct Parser {
 private:
  ParserData data_;

  /* If false, the whitespaces are fused with the tokens. Otherwise they are kept as separate space
   * and newline tokens. */
  bool keep_whitespace_;

  struct Mutation {
    /* Range of the original string to replace. */
    IndexRange src_range;
    /* The replacement string. */
    std::string replacement;

    Mutation(IndexRange src_range, std::string replacement)
        : src_range(src_range), replacement(replacement)
    {
    }

    /* Define operator in order to sort the mutation by starting position.
     * Otherwise, applying them in one pass will not work. */
    friend bool operator<(const Mutation &a, const Mutation &b)
    {
      return a.src_range.start < b.src_range.start;
    }
  };
  std::vector<Mutation> mutations_;

  ParserData::report_callback &report_error;

 public:
  Parser(const std::string &input,
         ParserData::report_callback &report_error,
         bool keep_whitespace = false)
      : keep_whitespace_(keep_whitespace), report_error(report_error)
  {
    data_.str = input;
    parse(report_error);
  }

  /* Run a callback for all existing scopes of a given type. */
  void foreach_scope(ScopeType type, std::function<void(Scope)> callback)
  {
    size_t pos = 0;
    while ((pos = data_.scope_types.find(char(type), pos)) != std::string::npos) {
      callback(Scope::from_position(&data_, pos));
      pos += 1;
    }
  }

  void foreach_match(const std::string &pattern,
                     std::function<void(const std::vector<Token>)> callback)
  {
    foreach_scope(ScopeType::Global,
                  [&](const Scope scope) { scope.foreach_match(pattern, callback); });
  }

  void foreach_token(const TokenType token_type, std::function<void(const Token)> callback)
  {
    const char str[2] = {token_type, '\0'};
    foreach_match(str, [&](const std::vector<Token> &tokens) { callback(tokens[0]); });
  }

  /* Run a callback for all existing function scopes. */
  void foreach_function(
      std::function<void(
          bool is_static, Token type, Token name, Scope args, bool is_const, Scope body)> callback)
  {
    foreach_match("m?ww(..)c?{..}", [&](const std::vector<Token> matches) {
      callback(matches[0] == Static,
               matches[2],
               matches[3],
               matches[4].scope(),
               matches[8] == Const,
               matches[10].scope());
    });
    foreach_match("m?ww::w(..)c?{..}", [&](const std::vector<Token> matches) {
      callback(matches[0] == Static,
               matches[2],
               matches[6],
               matches[7].scope(),
               matches[11] == Const,
               matches[13].scope());
    });
    foreach_match("m?ww<..>(..)c?{..}", [&](const std::vector<Token> matches) {
      callback(matches[0] == Static,
               matches[2],
               matches[3],
               matches[8].scope(),
               matches[12] == Const,
               matches[14].scope());
    });
  }

  std::string substr_range_inclusive(size_t start, size_t end)
  {
    return data_.str.substr(start, end - start + 1);
  }
  std::string substr_range_inclusive(Token start, Token end)
  {
    return substr_range_inclusive(start.str_index_start(), end.str_index_last());
  }

  /* Replace everything from `from` to `to` (inclusive).
   * Return true on success. */
  bool replace_try(size_t from, size_t to, const std::string &replacement)
  {
    IndexRange range = IndexRange(from, to + 1 - from);
    for (const Mutation &mut : mutations_) {
      if (mut.src_range.overlaps(range)) {
        return false;
      }
    }
    mutations_.emplace_back(range, replacement);
    return true;
  }
  /* Replace everything from `from` to `to` (inclusive).
   * Return true on success. */
  bool replace_try(Token from,
                   Token to,
                   const std::string &replacement,
                   bool keep_trailing_whitespaces = false)
  {
    if (keep_trailing_whitespaces) {
      return replace_try(from.str_index_start(), to.str_index_last_no_whitespace(), replacement);
    }
    return replace_try(from.str_index_start(), to.str_index_last(), replacement);
  }

  /* Replace everything from `from` to `to` (inclusive). */
  void replace(size_t from, size_t to, const std::string &replacement)
  {
    bool success = replace_try(from, to, replacement);
    assert(success);
    (void)success;
  }
  /* Replace everything from `from` to `to` (inclusive). */
  void replace(Token from, Token to, const std::string &replacement)
  {
    replace(from.str_index_start(), to.str_index_last(), replacement);
  }
  /* Replace token by string. */
  void replace(Token tok, const std::string &replacement, bool keep_trailing_whitespaces = false)
  {
    if (keep_trailing_whitespaces) {
      replace(tok.str_index_start(), tok.str_index_last_no_whitespace(), replacement);
    }
    else {
      replace(tok.str_index_start(), tok.str_index_last(), replacement);
    }
  }
  /* Replace Scope by string. */
  void replace(Scope scope, const std::string &replacement, bool keep_trailing_whitespaces = false)
  {
    if (keep_trailing_whitespaces) {
      replace(scope.start().str_index_start(),
              scope.end().str_index_last_no_whitespace(),
              replacement);
    }
    else {
      replace(scope.start(), scope.end(), replacement);
    }
  }

  /* Replace the content from `from` to `to` (inclusive) by whitespaces without changing
   * line count and keep the remaining indentation spaces. */
  void erase(size_t from, size_t to)
  {
    IndexRange range = IndexRange(from, to + 1 - from);
    std::string content = data_.str.substr(range.start, range.size);
    size_t lines = std::count(content.begin(), content.end(), '\n');
    size_t spaces = content.find_last_not_of(" ");
    if (spaces != std::string::npos) {
      spaces = content.length() - (spaces + 1);
    }
    replace(from, to, std::string(lines, '\n') + std::string(spaces, ' '));
  }
  /* Replace the content from `from` to `to` (inclusive) by whitespaces without changing
   * line count and keep the remaining indentation spaces. */
  void erase(Token from, Token to)
  {
    erase(from.str_index_start(), to.str_index_last());
  }
  /* Replace the content from `from` to `to` (inclusive) by whitespaces without changing
   * line count and keep the remaining indentation spaces. */
  void erase(Token tok)
  {
    erase(tok, tok);
  }
  /* Replace the content of the scope by whitespaces without changing
   * line count and keep the remaining indentation spaces. */
  void erase(Scope scope)
  {
    erase(scope.start(), scope.end());
  }

  void insert_after(size_t at, const std::string &content)
  {
    IndexRange range = IndexRange(at + 1, 0);
    mutations_.emplace_back(range, content);
  }
  void insert_after(Token at, const std::string &content)
  {
    insert_after(at.str_index_last(), content);
  }

  void insert_line_number(size_t at, int line)
  {
    insert_after(at, "#line " + std::to_string(line) + "\n");
  }
  void insert_line_number(Token at, int line)
  {
    insert_line_number(at.str_index_last(), line);
  }

  void insert_before(size_t at, const std::string &content)
  {
    IndexRange range = IndexRange(at, 0);
    mutations_.emplace_back(range, content);
  }
  void insert_before(Token at, const std::string &content)
  {
    insert_after(at.str_index_start(), content);
  }

  /* Return true if any mutation was applied. */
  bool only_apply_mutations()
  {
    if (mutations_.empty()) {
      return false;
    }

    /* Order mutations so that they can be applied in one pass. */
    std::stable_sort(mutations_.begin(), mutations_.end());

    /* Make sure to pad the input string in case of insertion after the last char. */
    bool added_trailing_new_line = false;
    if (data_.str.back() != '\n') {
      data_.str += '\n';
      added_trailing_new_line = true;
    }

    int64_t offset = 0;
    for (const Mutation &mut : mutations_) {
      data_.str.replace(mut.src_range.start + offset, mut.src_range.size, mut.replacement);
      offset += mut.replacement.size() - mut.src_range.size;
    }
    mutations_.clear();

    if (added_trailing_new_line) {
      data_.str.pop_back();
    }
    return true;
  }

  bool apply_mutations()
  {
    bool applied = only_apply_mutations();
    if (applied) {
      this->parse(report_error);
    }
    return applied;
  }

  /* Apply mutations if any and get resulting string. */
  const std::string &result_get()
  {
    only_apply_mutations();
    return data_.str;
  }

  /* For testing. */
  const ParserData &data_get()
  {
    return data_;
  }

  /* For testing. */
  std::string serialize_mutations() const
  {
    std::string out;
    for (const Mutation &mut : mutations_) {
      out += "Replace ";
      out += std::to_string(mut.src_range.start);
      out += " - ";
      out += std::to_string(mut.src_range.size);
      out += " \"";
      out += data_.str.substr(mut.src_range.start, mut.src_range.size);
      out += "\" by \"";
      out += mut.replacement;
      out += "\"\n";
    }
    return out;
  }

 private:
  using Duration = std::chrono::microseconds;
  Duration tokenize_time;
  Duration parse_scope_time;

  struct TimeIt {
    Duration &time;
    std::chrono::high_resolution_clock::time_point start;

    TimeIt(Duration &time) : time(time)
    {
      start = std::chrono::high_resolution_clock::now();
    }
    ~TimeIt()
    {
      auto end = std::chrono::high_resolution_clock::now();
      time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    }
  };

  void parse(ParserData::report_callback &report_error)
  {
    {
      TimeIt time_it(parse_scope_time);
      data_.tokenize(keep_whitespace_);
    }
    {
      TimeIt time_it(tokenize_time);
      data_.parse_scopes(report_error);
    }
  }

 public:
  void print_stats()
  {
    std::cout << "Tokenize time: " << tokenize_time.count() << " µs" << std::endl;
    std::cout << "Parser time:   " << parse_scope_time.count() << " µs" << std::endl;
    std::cout << "String len: " << std::to_string(data_.str.size()) << std::endl;
    std::cout << "Token len:  " << std::to_string(data_.token_types.size()) << std::endl;
    std::cout << "Scope len:  " << std::to_string(data_.scope_types.size()) << std::endl;
  }

  void debug_print()
  {
    std::cout << "Input: \n" << data_.str << " \nEnd of Input\n" << std::endl;
    std::cout << "Token Types: \"" << data_.token_types << "\"" << std::endl;
    std::cout << "Scope Types: \"" << data_.scope_types << "\"" << std::endl;
  }
};

}  // namespace blender::gpu::shader::parser
