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

  void tokenize();

  void parse_scopes(report_callback &report_error);

 private:
  void token_offsets_populate();
  void token_types_populate();
};

}  // namespace blender::gpu::shader::parser
