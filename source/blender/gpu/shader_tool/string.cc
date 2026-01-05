/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "intermediate.hh"
#include "metadata.hh"
#include "processor.hh"

namespace blender::gpu::shader {
using namespace std;
using namespace shader::parser;
using namespace metadata;

void SourceProcessor::lower_strings_sequences(Parser &parser)
{
  do {
    parser().foreach_match("__", [&](const vector<Token> &tokens) {
      string first = tokens[0].str();
      string second = tokens[1].str();
      string between = parser.substr_range_inclusive(tokens[0].str_index_last_no_whitespace() + 1,
                                                     tokens[1].str_index_start() - 1);
      string trailing = parser.substr_range_inclusive(tokens[1].str_index_last_no_whitespace() + 1,
                                                      tokens[1].str_index_last());
      string merged = first.substr(0, first.length() - 1) + second.substr(1) + between + trailing;
      parser.replace_try(tokens[0], tokens[1], merged);
    });
  } while (parser.apply_mutations());
}

/* Turn assert into a printf. */
void SourceProcessor::lower_assert(Parser &parser, const string &filename)
{
  /* Example: `assert(i < 0)` > `if (!(i < 0)) { printf(...); }` */
  parser().foreach_match("w(..)", [&](const vector<Token> &tokens) {
    if (tokens[0].str() != "assert") {
      return;
    }
    string replacement;
#ifdef WITH_GPU_SHADER_ASSERT
    string condition = tokens[1].scope().str();
    replacement += "if (!" + condition + ") ";
    replacement += "{";
    replacement += " printf(\"";
    replacement += "Assertion failed: " + condition + ", ";
    replacement += "file " + filename + ", ";
    replacement += "line %d, ";
    replacement += "thread (%u,%u,%u).\\n";
    replacement += "\"";
    replacement += ", __LINE__, GPU_THREAD.x, GPU_THREAD.y, GPU_THREAD.z); ";
    replacement += "}";
#endif
    parser.replace(tokens[0], tokens[4], replacement);
  });
#ifndef WITH_GPU_SHADER_ASSERT
  (void)filename;
  (void)report_error_;
#endif
  parser.apply_mutations();
}

/* Replace string literals by their hash and store the original string in the file metadata. */
void SourceProcessor::lower_strings(Parser &parser)
{
  parser().foreach_token(String, [&](const Token &token) {
    uint32_t hash = hash_string(token.str());
    metadata::PrintfFormat format = {hash, token.str()};
    metadata_.printf_formats.emplace_back(format);
    parser.replace(token, "string_t(" + to_string(hash) + "u)", true);
  });
  parser.apply_mutations();
}

/* Change printf calls to "recursive" call to implementation functions.
 * This allows to emulate the variadic arguments of printf. */
void SourceProcessor::lower_printf(Parser &parser)
{
  parser().foreach_match("w(..)", [&](const vector<Token> &tokens) {
    if (tokens[0].str() != "printf") {
      return;
    }

    int arg_count = 0;
    tokens[1].scope().foreach_scope(ScopeType::FunctionParam, [&](const Scope &) { arg_count++; });

    string unrolled = "print_start(" + to_string(arg_count) + ")";
    tokens[1].scope().foreach_scope(ScopeType::FunctionParam, [&](const Scope &attribute) {
      unrolled = "print_data(" + unrolled + ", " + attribute.str() + ")";
    });

    parser.replace(tokens.front(), tokens.back(), unrolled);
  });
  parser.apply_mutations();
}

}  // namespace blender::gpu::shader
