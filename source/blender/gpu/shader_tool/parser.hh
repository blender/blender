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

struct Parser {
  /** The parser's input string. */
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

  /** If `keep_whitespace` is false, white-spaces are merged with the previous token. */
  void tokenize(const bool keep_whitespace);

  void parse_scopes(report_callback &report_error);
};

}  // namespace blender::gpu::shader::parser
