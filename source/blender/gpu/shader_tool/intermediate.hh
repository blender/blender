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
 * The `TokenStream` class contain a copy of the given string to apply string substitutions (called
 * `Mutation`). It is usually faster to record all of them and apply them all at once after
 * scanning through the whole semantic representation. In the rare case where mutation need to
 * overlap (recursive processing), it is better to do them in passes until there is no mutation to
 * do.
 *
 * `Token` and `Scope` are read only interfaces to the data stored inside the `TokenStream`.
 * The data is stored as SoA (Structure of Arrays) for fast traversal.
 * The types of token and scopes are defined as readable chars to easily create sequences of token
 * type.
 *
 * The parsing phase doesn't apply any preprocessor. All  preprocessor directive are parsed as
 * `Preprocessor` scope but they are not expanded.
 */

#pragma once

#include "scope.hh"
#include "token.hh"
#include "token_stream.hh"
#include "utils.hh"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace blender::gpu::shader::parser {

struct MutableString {
  std::string str_;

  struct Mutation {
    /* Range of the original string to replace. */
    IndexRange src_range;
    /* The replacement string. */
    std::string replacement;

    Mutation(IndexRange src_range, std::string replacement)
        : src_range(src_range), replacement(replacement)
    {
      assert(src_range.size >= 0);
      assert(src_range.start >= 0);
    }

    /* Define operator in order to sort the mutation by starting position.
     * Otherwise, applying them in one pass will not work. */
    friend bool operator<(const Mutation &a, const Mutation &b)
    {
      return a.src_range.start < b.src_range.start;
    }
  };
  std::vector<Mutation> mutations_;

  MutableString(const std::string_view input) : str_(input) {}

  /* Disable copy construction and assignment. */
  MutableString(const MutableString &other) = delete;
  MutableString &operator=(const MutableString &other) = delete;
  /* Explicitly enable default construction, move construction and move assignment. */
  MutableString(MutableString &&other) = default;
  MutableString &operator=(MutableString &&other) = default;

  /* Access internal string without applying pending mutations. */
  std::string substr_range_inclusive(size_t start, size_t end)
  {
    return str_.substr(start, end - start + 1);
  }
  /* Access internal string without applying pending mutations. */
  std::string substr_range_inclusive(Token start, Token end)
  {
    return substr_range_inclusive(start.str_index_start(), end.str_index_last());
  }

  /* Access internal string without applying pending mutations. */
  std::string_view substr_range_inclusive_view(size_t start, size_t end)
  {
    return std::string_view(str_).substr(start, end - start + 1);
  }
  /* Access internal string without applying pending mutations. */
  std::string_view substr_range_inclusive_view(Token start, Token end)
  {
    return substr_range_inclusive_view(start.str_index_start(), end.str_index_last());
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
#ifndef NDEBUG
    bool success = replace_try(from, to, replacement);
    assert(success);
    (void)success;
#else
    /* No check in release. */
    IndexRange range = IndexRange(from, to + 1 - from);
    mutations_.emplace_back(range, replacement);
#endif
  }
  /* Replace everything from `from` to `to` (inclusive). */
  void replace(Token from,
               Token to,
               const std::string &replacement,
               bool keep_trailing_whitespaces = false)
  {
    if (keep_trailing_whitespaces) {
      replace(from.str_index_start(), to.str_index_last_no_whitespace(), replacement);
    }
    else {
      replace(from.str_index_start(), to.str_index_last(), replacement);
    }
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
    std::string content = str_.substr(range.start, range.size);
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

  /* If prepend is true, will prepend the new content to the list of modifications.
   * With this enabled, in case of overlapping mutation, the last one added will be first.  */
  void insert_before(size_t at, const std::string &content, bool prepend = false)
  {
    IndexRange range = IndexRange(at, 0);
    if (prepend) {
      mutations_.insert(mutations_.begin(), {range, content});
    }
    else {
      mutations_.emplace_back(range, content);
    }
  }
  void insert_before(Token at, const std::string &content, bool prepend = false)
  {
    insert_before(at.str_index_start(), content, prepend);
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
    std::string_view content = at.str_view_with_whitespace();
    size_t lines = std::count(content.begin(), content.end(), '\n');
    insert_line_number(at, at.line_number() + lines);
    size_t line_break = str_.find_last_of("\n", at.str_index_last() + 1);
    size_t spaces = at.str_index_last() - line_break;
    insert_after(at, std::string(spaces, ' '));
  }

  /* Return true if any mutation was applied.
   * Update lexer string view if needed. */
  bool apply_mutations(LexerBase &lexer, const bool all_mutation_ordered = false);

  /* Get internal string. Does not apply pending mutation. */
  const std::string &str()
  {
    return str_;
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
      out += str_.substr(mut.src_range.start, mut.src_range.size);
      out += "\" by \"";
      out += mut.replacement;
      out += "\"\n";
    }
    return out;
  }
};

/* Structure holding an intermediate form of the source code.
 * It is made for fast traversal and mutation of source code. */
template<typename LexerClass, typename ParserClass> struct IntermediateForm : MutableString {
 protected:
  LexerClass lex_;
  ParserClass parser_;

  report_callback &report_error;

 public:
  IntermediateForm(const std::string_view input, report_callback &report_error)
      : MutableString(input), parser_(lex_), report_error(report_error)
  {
    parse(report_error);
  }

  /* Main access operator. Returns the root scope (aka global scope). */
  Scope operator()() const
  {
    if (parser_.scope_types.empty()) {
      return Scope::invalid();
    }
    return Scope::from_position(parser_, 0);
  }

  /* Return true if any mutation was applied. */
  bool only_apply_mutations(const bool all_mutation_ordered = false)
  {
    return static_cast<MutableString *>(this)->apply_mutations(lex_, all_mutation_ordered);
  }

  /* Apply pending mutation and parse the resulting string.
   * Return true if any mutation was applied. */
  bool apply_mutations(const bool all_mutation_ordered = false)
  {
    bool applied = only_apply_mutations(all_mutation_ordered);
    if (applied) {
      parse(report_error);
    }
    return applied;
  }

  /* Apply mutations if any and get resulting string. */
  const std::string &result_get(const bool all_mutation_ordered = false)
  {
    only_apply_mutations(all_mutation_ordered);
    return str_;
  }

  /* For testing. */
  const ParserBase &data_get()
  {
    return parser_;
  }

  void parse(report_callback &report_error)
  {
    lex_.lexical_analysis(str_);
    parser_.semantic_analysis(report_error);
  }

  void debug_print()
  {
    std::cout << "Input: \n" << str_ << " \nEnd of Input\n" << std::endl;
    std::cout << "Token Types: \"" << lex_.token_types_str << "\"" << std::endl;
    std::cout << "Scope Types: \"" << parser_.scope_types_str << "\"" << std::endl;
  }
};

}  // namespace blender::gpu::shader::parser
