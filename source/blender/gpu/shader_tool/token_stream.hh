/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 *
 */

#pragma once

#include "utils.hh"

namespace blender::gpu::shader::parser {

/* Used to select at which stage to stop.
 *  */
enum ParserStage {
  Tokenize,
  MergeTokens,
  IdentifyKeywords,
  BuildScopeTree,
};

/**
 * This a tiny bit more than a token stream as it contains ranges or tokens (called scopes) from
 * syntax. The scopes and tokens have bi-directional mapping.
 */
struct TokenStream {
  /** The lexer's input string. */
  std::string str;

  /** Actually contains a sequence of #TokenType. */
  std::string token_types;
  /** Actually contains a sequence of #ScopeType. */
  std::string scope_types;
  /** Ranges of characters per token. */
  OffsetIndices token_offsets;
  /** Index of bottom most scope per token. */
  std::vector<int> token_scope;
  /** Range of token per scope. */
  std::vector<IndexRange> scope_ranges;

  void lexical_analysis(ParserStage stop_after);

  void semantic_analysis(ParserStage stop_after, report_callback &report_error);

 private:
  /* Create tokens based on character stream. */
  void tokenize(struct TokenData &tokens);
  /* Merge tokens (ex: '2','.','e','-','3` into '2.e-3`). */
  void merge_tokens(struct TokenData &tokens);

  void identify_keywords(struct TokenData &tokens);

  void build_scope_tree(report_callback &report_error);

  void build_token_to_scope_map();
};

}  // namespace blender::gpu::shader::parser
