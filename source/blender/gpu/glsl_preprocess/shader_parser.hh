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
#include <cctype>
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
  Literal = '0',
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
  Namespace = 'n',
  Struct = 's',
  Class = 'S',
  Const = 'c',
  Constexpr = 'C',
  Return = 'r',
  Switch = 'h',
  Case = 'H',
  If = 'i',
  Else = 'I',
  For = 'f',
  While = 'F',
  Do = 'd',
  Template = 't',
  This = 'T',
  Deref = 'D',
  Static = 'm',
  PreprocessorNewline = 'N',
  Equal = 'E',
  NotEqual = 'e',
  GEqual = 'G',
  LEqual = 'L',
  Increment = 'P',
  Decrement = 'D',
  Private = 'v',
  Public = 'V',
};

enum class ScopeType : char {
  /* Use ascii chars to store them in string, and for easy debugging / testing. */
  Global = 'G',
  Namespace = 'N',
  Struct = 'S',
  Function = 'F',
  FunctionArgs = 'f',
  Template = 'T',
  TemplateArg = 't',
  Subscript = 'A',
  Preprocessor = 'P',
  Assignment = 'a',
  /* Added scope inside function body. */
  Local = 'L',
  /* Added scope inside FunctionArgs. */
  FunctionArg = 'g',
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
    {
      /* Tokenization. */
      token_types.clear();
      token_offsets.clear();

      token_types += char(to_type(str[0]));
      token_offsets.offsets.emplace_back(0);

      /* When doing whitespace merging, keep knowledge about whether previous char was whitespace.
       * This allows to still split words on spaces. */
      bool prev_was_whitespace = (token_types[0] == NewLine || token_types[0] == Space);
      bool inside_preprocessor_directive = false;

      int offset = 0;
      for (const char &c : str.substr(1)) {
        offset++;
        TokenType type = to_type(c);
        TokenType prev = TokenType(token_types.back());

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
        if (type == Literal && prev == Word) {
          continue;
        }
        /* If 'x' is part of hex literal. */
        if (c == 'x' && prev == Literal) {
          continue;
        }
        /* If 'A-F' is part of hex literal. */
        if (c >= 'A' && c <= 'F' && prev == Literal) {
          continue;
        }
        /* If 'a-f' is part of hex literal. */
        if (c >= 'a' && c <= 'f' && prev == Literal) {
          continue;
        }
        /* If 'u' is part of unsigned int literal. */
        if (c == 'u' && prev == Literal) {
          continue;
        }
        /* If dot is part of float literal. */
        if (type == Dot && prev == Literal) {
          continue;
        }
        /* If 'f' suffix is part of float literal. */
        if (c == 'f' && prev == Literal) {
          continue;
        }
        /* If 'e' is part of float literal. */
        if (c == 'e' && prev == Literal) {
          continue;
        }
        /* If sign is part of float literal after exponent. */
        if ((c == '+' || c == '-') && prev == Literal) {
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
        if (type != Word && type != NewLine && type != Space && type != Literal) {
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
        }
      }
    }
  }

  void parse_scopes()
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

      bool in_template = false;

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
          case BracketOpen:
            if (tok_id >= 2 && token_types[tok_id - 2] == Struct) {
              enter_scope(ScopeType::Local, tok_id);
            }
            else if (tok_id >= 2 && token_types[tok_id - 2] == Namespace) {
              enter_scope(ScopeType::Namespace, tok_id);
            }
            else if (scopes.top().type == ScopeType::Global) {
              enter_scope(ScopeType::Function, tok_id);
            }
            else if (scopes.top().type == ScopeType::Struct) {
              enter_scope(ScopeType::Function, tok_id);
            }
            else {
              enter_scope(ScopeType::Local, tok_id);
            }
            break;
          case ParOpen:
            if (scopes.top().type == ScopeType::Global) {
              enter_scope(ScopeType::FunctionArgs, tok_id);
            }
            else if (scopes.top().type == ScopeType::Struct) {
              enter_scope(ScopeType::FunctionArgs, tok_id);
            }
            else {
              enter_scope(ScopeType::Local, tok_id);
            }
            break;
          case SquareOpen:
            enter_scope(ScopeType::Subscript, tok_id);
            break;
          case AngleOpen:
            if ((tok_id >= 1 && token_types[tok_id - 1] == Template) ||
                /* Catch case of specialized declaration. */
                ScopeType(scope_types.back()) == ScopeType::Template)
            {
              enter_scope(ScopeType::Template, tok_id);
              in_template = true;
            }
            break;
          case AngleClose:
            if (in_template && scopes.top().type == ScopeType::Assignment) {
              exit_scope(tok_id - 1);
            }
            if (scopes.top().type == ScopeType::TemplateArg) {
              exit_scope(tok_id - 1);
            }
            if (scopes.top().type == ScopeType::Template) {
              exit_scope(tok_id);
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
            exit_scope(tok_id);
            break;
          case SquareClose:
            exit_scope(tok_id);
            break;
          case SemiColon:
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
            if (scopes.top().type == ScopeType::Template) {
              enter_scope(ScopeType::TemplateArg, tok_id);
            }
            break;
        }
      }
      exit_scope(tok_id);
      /* Some syntax confuses the parser. Bisect the error by removing things in the source file
       * until the error is found. Then either fix the unsupported syntax in the parser or use
       * alternative syntax. */
      assert(scopes.empty());
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
        return TokenType::Literal;
      default:
        return TokenType::Word;
    }
  }
};

struct Token {
  const ParserData *data;
  int64_t index;

  static Token invalid()
  {
    return {nullptr, 0};
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
    return {data, index - 1};
  }
  Token next() const
  {
    return {data, index + 1};
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

  std::string str() const
  {
    return data->str.substr(index_range().start, index_range().size);
  }

  std::string str_no_whitespace() const
  {
    std::string str = this->str();
    return str.substr(0, str.find_last_not_of(" \n") + 1);
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
};

struct Scope {
  const ParserData *data;
  size_t index;

  Token start() const
  {
    return {data, range().start};
  }

  Token end() const
  {
    return {data, range().last()};
  }

  IndexRange range() const
  {
    return data->scope_ranges[index];
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

  std::string str() const
  {
    return data->str.substr(start().str_index_start(),
                            end().str_index_last() - start().str_index_start() + 1);
  }

  Token find_token(const char token_type) const
  {
    size_t pos = data->token_types.substr(range().start, range().size).find(token_type);
    return (pos != std::string::npos) ? Token{data, int64_t(range().start + pos)} :
                                        Token::invalid();
  }

  bool contains_token(const char token_type) const
  {
    return find_token(token_type).is_valid();
  }

  void foreach_match(const std::string &pattern,
                     std::function<void(const std::vector<Token>)> callback) const
  {
    const std::string scope_tokens = data->token_types.substr(range().start, range().size);

    std::vector<Token> match;
    match.resize(pattern.size());

    size_t pos = 0;
    while ((pos = scope_tokens.find(pattern, pos)) != std::string::npos) {
      match[0] = {data, int64_t(range().start + pos)};
      /* Do not match preprocessor directive by default. */
      if (match[0].scope().type() != ScopeType::Preprocessor) {
        for (int i = 1; i < pattern.size(); i++) {
          match[i] = Token{data, int64_t(range().start + pos + i)};
        }
        callback(match);
      }
      pos += 1;
    }
  }

  /* Will iterate over all the scopes that are direct children. */
  void foreach_scope(ScopeType type, std::function<void(Scope)> callback) const
  {
    size_t pos = this->index;
    while ((pos = data->scope_types.find(char(type), pos)) != std::string::npos) {
      Scope scope{data, pos};
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
};

inline Scope Token::scope() const
{
  return {data, size_t(data->token_scope[index])};
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

 public:
  Parser(const std::string &input, bool keep_whitespace = false)
      : keep_whitespace_(keep_whitespace)
  {
    data_.str = input;
    parse();
  }

  /* Run a callback for all existing scopes of a given type. */
  void foreach_scope(ScopeType type, std::function<void(Scope)> callback)
  {
    size_t pos = 0;
    while ((pos = data_.scope_types.find(char(type), pos)) != std::string::npos) {
      callback(Scope{&data_, pos});
      pos += 1;
    }
  }

  void foreach_match(const std::string &pattern,
                     std::function<void(const std::vector<Token>)> callback)
  {
    foreach_scope(ScopeType::Global,
                  [&](const Scope scope) { scope.foreach_match(pattern, callback); });
  }

  /* Run a callback for all existing function scopes. */
  void foreach_function(
      std::function<void(
          bool is_static, Token type, Token name, Scope args, bool is_const, Scope body)> callback)
  {
    foreach_scope(ScopeType::FunctionArgs, [&](const Scope args) {
      const bool is_const = args.end().next() == Const;
      Token next = (is_const ? args.end().next() : args.end()).next();
      if (next != '{') {
        /* Function Prototype. */
        return;
      }
      const bool is_static = args.start().prev().prev().prev() == Static;
      Token type = args.start().prev().prev();
      Token name = args.start().prev();
      Scope body = next.scope();
      callback(is_static, type, name, args, is_const, body);
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
  bool replace_try(Token from, Token to, const std::string &replacement)
  {
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
  void replace(Token tok, const std::string &replacement)
  {
    replace(tok.str_index_start(), tok.str_index_last(), replacement);
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

    int64_t offset = 0;
    for (const Mutation &mut : mutations_) {
      data_.str.replace(mut.src_range.start + offset, mut.src_range.size, mut.replacement);
      offset += mut.replacement.size() - mut.src_range.size;
    }
    mutations_.clear();
    return true;
  }

  bool apply_mutations()
  {
    bool applied = only_apply_mutations();
    if (applied) {
      this->parse();
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

  void parse()
  {
    {
      TimeIt time_it(parse_scope_time);
      data_.tokenize(keep_whitespace_);
    }
    {
      TimeIt time_it(tokenize_time);
      data_.parse_scopes();
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
