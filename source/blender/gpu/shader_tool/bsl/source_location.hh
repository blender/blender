/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include "token.hh"

namespace bsl {

using namespace std;
using Token = blender::gpu::shader::parser::Token;
using ParserBase = blender::gpu::shader::parser::ParserBase;

/**
 * \brief Represents a specific position within a source file or included sub-file.
 *
 * `SourceLocation` wraps a lexical `Token` to provide formatted string representations
 * (e.g., `file.hh:12:5`) for diagnostic output, as well as ordering operators
 * to compare token positions across translation units and `#include` hierarchies.
 */
struct SourceLocation {
  Token tok;

  SourceLocation(Token tok) : tok(tok) {}

  operator string() const
  {
    return tok.filename() + ':' + to_string(tok.line_number()) + ':' +
           to_string(tok.char_number() + 1);
  }

  friend bool operator<=(const SourceLocation &a, const SourceLocation &b)
  {
    return (a.include_id() != b.include_id()) ? (a.include_id() <= b.include_id()) :
                                                (a.tok.index_ <= b.tok.index_);
  }

  friend bool operator<(const SourceLocation &a, const SourceLocation &b)
  {
    return (a.include_id() != b.include_id()) ? (a.include_id() < b.include_id()) :
                                                (a.tok.index_ < b.tok.index_);
  }

 private:
  int include_id() const
  {
    return static_cast<const ParserBase *>(tok.buf_)->include_id;
  }
};

}  // namespace bsl
