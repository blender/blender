/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 *
 */

#pragma once

#include "enums.hh"
#include "utils.hh"

#include <array>

namespace blender::gpu::shader::parser {

struct Token;
struct Scope;

/**
 * Turns string into token.
 */
struct LexerBase : lexit::TokenBuffer {
  static const std::array<CharClass, 128> bsl_char_class_table;
  static const std::array<CharClass, 128> default_char_class_table;

  /** Compact visualization of token_types.  */
  std::string_view token_types_str() const
  {
    return std::string_view((const char *)types_.get(), size_);
  }

  /* Change words into keyword (ex: `if`, `struct`, `template`). */
  void identify_keywords();
};

/**
 * Only support rough tokenization.
 */
struct SimpleLexer {
  static void lexical_analysis(LexerBase &lex, std::string_view input)
  {
    lex.process(input, LexerBase::bsl_char_class_table.data());
  }
};

/**
 * Identify BSL keywords, and correctly identify float literals.
 */
struct FullLexer {
  static void lexical_analysis(LexerBase &lex, std::string_view input)
  {
    lex.process(input, LexerBase::bsl_char_class_table.data());
    lex.merge_complex_literals();
    lex.identify_keywords();
  }
};

/**
 * Create semantic scopes from token stream.
 * Also creates mapping table from token to scope to have bi-directional mapping.
 */
struct ParserBase : LexerBase {
  /** Compact visualization of scope_types.  */
  std::string_view scope_types_str;

  /* --- Structure of Array style data for scopes. --- */

  /** Range of token per scope. */
  std::vector<ScopeType> scope_types;
  /** Range of token per scope. */
  std::vector<IndexRange> scope_ranges;
  /** Index of bottom most scope per token. */
  std::vector<int> token_scope;

  /* Return the i'th token. */
  Token operator[](int i) const;

  void build_scope_tree(report_callback &report_error);
  void build_token_to_scope_map();

 private:
  void update_string_view();
};

/* Don't do anything. No access to scopes is allowed. */
struct NullParser {
  static void semantic_analysis(ParserBase &parser, report_callback & /*report_error*/)
  {
    parser.scope_types = {};
    parser.scope_ranges = {};
  }
};

/* Do not parse. Creates a single global scope containing all tokens. */
struct DummyParser {
  static void semantic_analysis(ParserBase &parser, report_callback & /*report_error*/)
  {
    parser.scope_types = {ScopeType::Global};
    parser.scope_ranges = {IndexRange(0, parser.size())};
    parser.build_token_to_scope_map();
  }
};

struct FullParser {
  static void semantic_analysis(ParserBase &parser, report_callback &report_error)
  {
    parser.build_scope_tree(report_error);
    parser.build_token_to_scope_map();
  }
};

template<typename LexerFn, typename ParserFn> struct Parser : ParserBase {
  void lexical_analysis(std::string_view input)
  {
    LexerFn::lexical_analysis(*this, input);
  }

  void semantic_analysis(report_callback &report_error)
  {
    ParserFn::semantic_analysis(*this, report_error);
  }
};

}  // namespace blender::gpu::shader::parser
