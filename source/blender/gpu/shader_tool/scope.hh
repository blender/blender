/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 *
 */

#pragma once

#include "parser.hh"
#include "token.hh"

#include <cassert>

namespace blender::gpu::shader::parser {

enum class ScopeType : char {
  Invalid = 0,
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
  Attributes = 'B',
  Attribute = 'b',
  /* Added scope inside function body. */
  Local = 'L',
  /* Added scope inside FunctionArgs. */
  FunctionArg = 'g',
  /* Added scope inside FunctionCall. */
  FunctionParam = 'm',
  /* Added scope inside LoopArgs. */
  LoopArg = 'r',

};

struct Scope {
  /* String view for nicer debugging experience. Isn't actually used. */
  std::string_view token_view;
  std::string_view str_view;

  const Parser *data;
  int64_t index;

  static Scope from_position(const Parser *data, int64_t index)
  {
    IndexRange index_range = data->scope_ranges[index];
    int str_start = data->token_offsets[index_range.start].start;
    int str_end = data->token_offsets[index_range.last()].last();
    return {std::string_view(data->token_types).substr(index_range.start, index_range.size),
            std::string_view(data->str).substr(str_start, str_end - str_start + 1),
            data,
            index};
  }

  static Scope invalid()
  {
    return {"", "", nullptr, 0};
  }

  bool is_valid() const
  {
    return data != nullptr;
  }
  bool is_invalid() const
  {
    return data == nullptr;
  }

  Token operator[](int i)
  {
    return is_invalid() ? Token::invalid() : Token::from_position(data, range().start + i);
  }

  /* Return first token of that scope. */
  Token front() const
  {
    return is_invalid() ? Token::invalid() : Token::from_position(data, range().start);
  }

  /* Return last token of that scope. */
  Token back() const
  {
    return is_invalid() ? Token::invalid() : Token::from_position(data, range().last());
  }

  IndexRange range() const
  {
    return is_invalid() ? IndexRange(0, 0) : data->scope_ranges[index];
  }

  Token operator[](const int64_t index) const
  {
    return Token::from_position(data, range().start + index);
  }

  size_t token_count() const
  {
    return is_invalid() ? 0 : range().size;
  }

  ScopeType type() const
  {
    return is_invalid() ? ScopeType::Invalid : ScopeType(data->scope_types[index]);
  }

  /* Returns the scope that contains this scope. */
  Scope scope() const
  {
    if (is_invalid()) {
      return Scope::invalid();
    }
    const size_t scope_start = this->front().str_index_start();
    Scope scope = *this;
    while ((scope = scope.prev()).is_valid()) {
      if (scope.back().str_index_last() > scope_start) {
        return scope;
      }
    }
    return scope;
  }

  /* Returns the previous scope before this scope. Can be either the container scope or the
   * previous scope inside the same container. */
  Scope prev() const
  {
    return is_invalid() ? Scope::invalid() : front().prev().scope();
  }

  /* Returns the next scope after this scope. Can be either the container scope or the next scope
   * inside the same container. */
  Scope next() const
  {
    return is_invalid() ? Scope::invalid() : back().next().scope();
  }

  bool contains(const Scope sub) const
  {
    Scope parent = sub.scope();
    while (parent.type() != ScopeType::Global && parent != *this) {
      parent = parent.scope();
    }
    return parent == *this;
  }

  std::string str_with_whitespace() const
  {
    if (this->is_invalid()) {
      return "";
    }
    return data->str.substr(front().str_index_start(),
                            back().str_index_last() - front().str_index_start() + 1);
  }

  std::string str() const
  {
    if (this->is_invalid()) {
      return "";
    }
    return data->str.substr(front().str_index_start(),
                            back().str_index_last_no_whitespace() - front().str_index_start() + 1);
  }

  /* Return the content without the first and last token. */
  std::string str_exclusive() const
  {
    if (this->is_invalid() || this->token_count() <= 2) {
      return "";
    }
    Token start = this->front().next();
    Token end = this->back().prev();
    return data->str.substr(start.str_index_start(),
                            end.str_index_last_no_whitespace() - start.str_index_start() + 1);
  }

  /* Return first occurrence of token_type inside this scope. */
  Token find_token(const char token_type) const
  {
    if (this->is_invalid()) {
      return Token::invalid();
    }
    size_t pos = data->token_types.substr(range().start, range().size).find(token_type);
    return (pos != std::string::npos) ? Token::from_position(data, range().start + pos) :
                                        Token::invalid();
  }

  bool contains_token(const char token_type) const
  {
    return find_token(token_type).is_valid();
  }

  /* Return the first container scope that has the given type (including itself).
   * Returns invalid scope on failure. */
  Scope first_scope_of_type(const ScopeType type) const
  {
    Scope scope = *this;
    while (scope.type() != ScopeType::Global && scope.type() != type) {
      scope = scope.scope();
    }
    return scope.type() == type ? scope : Scope::invalid();
  }

  /**
   * Small pattern matching engine.
   * - pattern is expected to a be a sequence of #TokenType stored as a string.
   * - single '?' after a token will make this token optional.
   * - double '?' will match the question mark.
   * - double '.' will skip to the end of the current matched scope.
   * - callback is called for each matches with a vector of token the size of the input pattern.
   * - control tokens ('..' and '?') and unmatched optional tokens will be set to invalid in match
   *   vector.
   * IMPORTANT: 2 matches cannot overlap. The pattern matching algorithm skips the whole match
   *            after a match there is no readback. This could eventually be fixed.
   */
  void foreach_match(const std::string &pattern,
                     std::function<void(const std::vector<Token>)> callback) const
  {
    assert(!pattern.empty());
    if (this->is_invalid()) {
      return;
    }

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
          cursor = match[i - 1].scope().back().index;
          i++;
          continue;
        }

        /* Regular token. */
        if (curr_search_token == token_type) {
          match[i] = Token::from_position(data, cursor++);
        }
        else if (curr_search_token == '?' && next_search_token != '?') {
          /* We just matched an optional token in previous iteration. Continue scanning. */
          match[i] = Token::invalid();
        }
        else if (!is_last_token && curr_search_token != '?' && next_search_token == '?') {
          /* This was an optional token. Continue scanning. */
          match[i] = Token::invalid();
          i++;
          continue;
        }
        else {
          /* Token mismatch. Test next position. */
          break;
        }

        if (is_last_token) {
          callback(match);
          /* Avoid matching the same position if start of pattern is optional tokens. */
          pos = cursor - range().start - 1;
        }
      }
    }
  }

  /* Will iterate over all the scopes that are direct children. */
  void foreach_scope(ScopeType type, std::function<void(Scope)> callback) const
  {
    /* Makes no sense to iterate on global scope since it is the top level. */
    assert(type != ScopeType::Global);

    if (this->is_invalid()) {
      return;
    }
    size_t pos = this->index;
    while ((pos = data->scope_types.find(char(type), pos)) != std::string::npos) {
      Scope scope = Scope::from_position(data, pos);
      if (scope.front().index > this->back().index) {
        /* Found scope starts after this scope. End iteration. */
        break;
      }
      /* Make sure found scope is direct child of this scope. */
      Scope parent_scope = scope.scope();
      if (parent_scope.index == this->index) {
        callback(scope);
      }
      pos += 1;
    }
  }

  /* Will iterate over all the attribute if this scope is an ScopeType::Attributes. */
  void foreach_attribute(
      std::function<void(Token attribute_name, Scope attribute_props)> callback) const
  {
    assert(this->type() == ScopeType::Attributes);
    this->foreach_scope(ScopeType::Attribute, [&](Scope attr) {
      callback(attr[0], attr[1] == '(' ? attr[1].scope() : Scope::invalid());
    });
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
  void foreach_struct(
      std::function<void(Token struct_tok, Scope attributes, Token name, Scope body)> callback)
      const
  {
    foreach_match("sw{..}", [&](const std::vector<Token> matches) {
      callback(matches[0], Scope::invalid(), matches[1], matches[2].scope());
    });
    foreach_match("sw<..>{..}", [&](const std::vector<Token> matches) {
      callback(matches[0], Scope::invalid(), matches[1], matches[6].scope());
    });
    foreach_match("s[[..]]w{..}", [&](const std::vector<Token> matches) {
      callback(matches[0], matches[2].scope(), matches[7], matches[8].scope());
    });
    foreach_match("s[[..]]w<..>{..}", [&](const std::vector<Token> matches) {
      callback(matches[0], matches[2].scope(), matches[7], matches[12].scope());
    });
  }

  /* Run a callback for all existing variable declaration (without assignment). */
  void foreach_declaration(std::function<void(Scope attributes,
                                              Token const_tok,
                                              Token type,
                                              Scope template_scope,
                                              Token name,
                                              Scope array,
                                              Token decl_end)> callback) const
  {
    auto attrs = [](const std::vector<Token> &tokens) {
      Token first = tokens[0].is_valid() ? tokens[0] : tokens[2];
      Scope attributes = first.prev().prev().scope();
      attributes = (attributes.type() == ScopeType::Attributes) ? attributes : Scope::invalid();
      return attributes;
    };

    auto cb = [&](Scope attributes,
                  Token const_tok,
                  Token type,
                  Scope template_scope,
                  Token name,
                  Scope array,
                  Token decl_end) {
      if (type.scope() != *this) {
        return;
      }
      callback(attributes, const_tok, type, template_scope, name, array, decl_end);
    };

    foreach_match("c?ww;", [&](const std::vector<Token> toks) {
      cb(attrs(toks), toks[0], toks[2], Scope::invalid(), toks[3], Scope::invalid(), toks.back());
    });
    foreach_match("c?ww[..];", [&](const std::vector<Token> toks) {
      cb(attrs(toks), toks[0], toks[2], Scope::invalid(), toks[3], toks[4].scope(), toks.back());
    });
    foreach_match("c?w<..>w;", [&](const std::vector<Token> toks) {
      cb(attrs(toks), toks[0], toks[2], toks[3].scope(), toks[7], Scope::invalid(), toks.back());
    });
    foreach_match("c?w<..>w[..];", [&](const std::vector<Token> toks) {
      cb(attrs(toks), toks[0], toks[2], toks[3].scope(), toks[7], toks[8].scope(), toks.back());
    });

    foreach_match("c?w&w;", [&](const std::vector<Token> toks) {
      cb(attrs(toks), toks[0], toks[2], Scope::invalid(), toks[4], Scope::invalid(), toks.back());
    });
    foreach_match("c?w(&w)[..];", [&](const std::vector<Token> toks) {
      cb(attrs(toks), toks[0], toks[2], Scope::invalid(), toks[5], toks[7].scope(), toks.back());
    });
    foreach_match("c?w<..>&w;", [&](const std::vector<Token> toks) {
      cb(attrs(toks), toks[0], toks[2], toks[3].scope(), toks[8], Scope::invalid(), toks.back());
    });
    foreach_match("c?w<..>(&w)[..];", [&](const std::vector<Token> toks) {
      cb(attrs(toks), toks[0], toks[2], toks[3].scope(), toks[9], toks[11].scope(), toks.back());
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

}  // namespace blender::gpu::shader::parser
