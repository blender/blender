/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
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

#include "parser.hh"
#include "scope.hh"
#include "token.hh"
#include "utils.hh"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace blender::gpu::shader::parser {

/* Structure holding an intermediate form of the source code.
 * It is made for fast traversal and mutation of source code. */
struct IntermediateForm {
 private:
  Parser data_;

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

  report_callback &report_error;

 public:
  IntermediateForm(const std::string &input,
                   report_callback &report_error,
                   bool keep_whitespace = false)
      : keep_whitespace_(keep_whitespace), report_error(report_error)
  {
    data_.str = input;
    parse(report_error);
  }

  /* Main access operator. Returns the root scope (aka global scope). */
  Scope operator()() const
  {
    if (data_.scope_types.empty()) {
      return Scope::invalid();
    }
    return Scope::from_position(&data_, 0);
  }

  /* Access internal string without applying pending mutations. */
  std::string substr_range_inclusive(size_t start, size_t end)
  {
    return data_.str.substr(start, end - start + 1);
  }
  /* Access internal string without applying pending mutations. */
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
      replace(scope.front().str_index_start(),
              scope.back().str_index_last_no_whitespace(),
              replacement);
    }
    else {
      replace(scope.front(), scope.back(), replacement);
    }
  }

  /* Replace the content from `from` to `to` (inclusive) by whitespaces without changing
   * line count and keep the remaining indentation spaces. */
  void erase(size_t from, size_t to)
  {
    IndexRange range = IndexRange(from, to + 1 - from);
    std::string content = data_.str.substr(range.start, range.size);
    size_t lines = std::count(content.begin(), content.end(), '\n');
    size_t spaces = content.find_last_of("\n");
    if (spaces != std::string::npos) {
      spaces = content.length() - (spaces + 1);
    }
    else {
      spaces = content.length();
    }
    replace(from, to, std::string(lines, '\n') + std::string(spaces, ' '));
  }
  /* Replace the content from `from` to `to` (inclusive) by whitespaces without changing
   * line count and keep the remaining indentation spaces. */
  void erase(Token from, Token to)
  {
    if (from.is_invalid() && to.is_invalid()) {
      return;
    }
    assert(from.index <= to.index);
    erase(from.str_index_start(), to.str_index_last());
  }
  /* Replace the content from `from` to `to` (inclusive) by whitespaces without changing
   * line count and keep the remaining indentation spaces. */
  void erase(Token tok)
  {
    if (tok.is_invalid()) {
      return;
    }
    erase(tok, tok);
  }
  /* Replace the content of the scope by whitespaces without changing
   * line count and keep the remaining indentation spaces. */
  void erase(Scope scope)
  {
    erase(scope.front(), scope.back());
  }

  void insert_before(size_t at, const std::string &content)
  {
    IndexRange range = IndexRange(at, 0);
    mutations_.emplace_back(range, content);
  }
  void insert_before(Token at, const std::string &content)
  {
    insert_before(at.str_index_start(), content);
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

  /* Insert a preprocessor directive after the given token.
   * This also insert a line directive to keep correct error reporting. */
  void insert_directive(Token at, const std::string directive)
  {
    insert_after(at, "\n" + directive + "\n");
    std::string content = at.str_with_whitespace();
    size_t lines = std::count(content.begin(), content.end(), '\n');
    insert_line_number(at, at.line_number() + lines);
    size_t line_break = data_.str.find_last_of("\n", at.str_index_last() + 1);
    size_t spaces = at.str_index_last() - line_break;
    insert_after(at, std::string(spaces, ' '));
  }

  /* Return true if any mutation was applied. */
  bool only_apply_mutations();

  /* Apply pending mutation and parse the resulting string.
   * Return true if any mutation was applied. */
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

  /* Get internal string. Does not apply pending mutation. */
  const std::string &str()
  {
    return data_.str;
  }

  /* For testing. */
  const Parser &data_get()
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
  TimeIt::Duration tokenize_time;
  TimeIt::Duration parse_scope_time;

  void parse(report_callback &report_error)
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
