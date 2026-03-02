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

namespace blender::gpu::shader::parser {

struct Token;

/**
 * Turns string into token.
 */
struct LexerBase {
  /** The lexer's input string. */
  std::string_view str;

  /** Compact visualization of token_types.  */
  std::string_view token_types_str;

  /* --- Structure of Array style data for tokens. --- */

  /** Token type per token. */
  MutableSpan<TokenType> token_types;
  /** End of the raw token before white-space removing. */
  MutableSpan<uint32_t> token_ends;
  /** Ranges of characters per token. */
  OffsetIndices token_offsets;

  /** Token Data. Backing memory for the spans. */
  size_t alloc_size = 0;
  char *memory = nullptr;

  ~LexerBase();

 protected:
  void ensure_memory();
  /* Create tokens based on character stream. */
  void tokenize(bool use_default_table = false);
  /* Change words into keyword (ex: `if`, `struct`, `template`). Must run before merge tokens. */
  void identify_keywords();
  /* Merge tokens (ex: '2','.','e','-','3` into '2.e-3`). */
  void merge_tokens();

  void update_string_view();
};

/**
 * Consider numbers as words (to avoid splitting identifiers).
 * Does not merge newlines and spaces.
 */
struct SimpleLexer : LexerBase {
  void lexical_analysis(std::string_view input)
  {
    str = input;
    ensure_memory();
    tokenize(true);
  }
};

/**
 * Allow recognition of common operators and numbers. Merge white-spaces.
 */
struct ExpressionLexer : LexerBase {
  void lexical_analysis(std::string_view input)
  {
    str = input;
    ensure_memory();
    tokenize(true);
    identify_keywords();
    merge_tokens();
  }
};

/**
 * Allow recognition of operators and numbers. Merge white-spaces.
 * However, doesn't merge angle bracket with other tokens in order to use them for template
 * expressions parsing.
 */
struct FullLexer : LexerBase {
  void lexical_analysis(std::string_view input)
  {
    str = input;
    ensure_memory();
    tokenize();
    identify_keywords();
    merge_tokens();
  }
};

/**
 * Create semantic scopes from token stream.
 * Also creates mapping table from token to scope to have bi-directional mapping.
 */
struct ParserBase {
  const LexerBase &lex;

  /** Compact visualization of scope_types.  */
  std::string_view scope_types_str;

  /* --- Structure of Array style data for scopes. --- */

  /** Range of token per scope. */
  std::vector<ScopeType> scope_types;
  /** Range of token per scope. */
  std::vector<IndexRange> scope_ranges;
  /** Index of bottom most scope per token. */
  std::vector<int> token_scope;

  ParserBase(const LexerBase &lex) : lex(lex) {}

  /* Return the i'th token. */
  Token operator[](int i) const;

 protected:
  void build_scope_tree(report_callback &report_error);
  void build_token_to_scope_map();

 private:
  void update_string_view();
};

/* Don't do anything. No access to scopes is allowed. */
struct NullParser : ParserBase {
  NullParser(const LexerBase &lex) : ParserBase(lex) {}

  void semantic_analysis(report_callback & /*report_error*/)
  {
    scope_types = {};
    scope_ranges = {};
  }
};

/* Do not parse. Creates a single global scope containing all tokens. */
struct DummyParser : ParserBase {
  DummyParser(const LexerBase &lex) : ParserBase(lex) {}

  void semantic_analysis(report_callback & /*report_error*/)
  {
    scope_types = {ScopeType::Global};
    scope_ranges = {IndexRange(0, lex.token_types.size())};
    build_token_to_scope_map();
  }
};

struct FullParser : ParserBase {
  FullParser(const LexerBase &lex) : ParserBase(lex) {}

  void semantic_analysis(report_callback &report_error)
  {
    build_scope_tree(report_error);
    build_token_to_scope_map();
  }
};

}  // namespace blender::gpu::shader::parser
