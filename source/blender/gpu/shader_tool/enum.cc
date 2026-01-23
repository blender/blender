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

void SourceProcessor::lower_enums(Parser &parser)
{
  /**
   * Transform C,C++ enum declaration into GLSL compatible defines and constants:
   *
   * \code{.cpp}
   * enum MyEnum : uint {
   *   ENUM_1 = 0u,
   *   ENUM_2 = 1u,
   *   ENUM_3 = 2u,
   * };
   * \endcode
   *
   * becomes
   *
   * \code{.glsl}
   * #define MyEnum uint
   * constant static constexpr uint ENUM_1 = 0u;
   * constant static constexpr uint ENUM_2 = 1u;
   * constant static constexpr uint ENUM_3 = 2u;
   *
   * \endcode
   *
   * It is made like so to avoid messing with error lines, allowing to point at the exact
   * location inside the source file.
   *
   * IMPORTANT: This has some requirements:
   * - Enums needs to have underlying types set to uint32_t to make them usable in UBO and SSBO.
   */

  auto missing_underlying_type = [&](vector<Token> tokens) {
    report_error_(tokens[0].line_number(),
                  tokens[0].char_number(),
                  tokens[0].line_str(),
                  "enum declaration must explicitly use an underlying type");
  };

  parser().foreach_match("MA{", missing_underlying_type);
  parser().foreach_match("MSA{", missing_underlying_type);

  const string placeholder_value = "=__auto__";

  auto placeholder = [&](Scope enum_scope) {
    const string &value = placeholder_value;
    const string start = " = 0" + string(enum_scope.front().prev().str()[0] == 'u' ? "u" : "");

    auto insert = [&](Token name, const string &replacement) {
      if (name.next() == ',' || name.next() == '}') {
        parser.insert_after(name, replacement);
      }
    };
    enum_scope.foreach_match("{A", [&](const Tokens &t) { insert(t[1], start); });
    enum_scope.foreach_match(",A", [&](const Tokens &t) { insert(t[1], value); });
  };

  parser().foreach_match("MSA:A{", [&](const Tokens &t) { placeholder(t[5].scope()); });
  parser().foreach_match("MA:A{", [&](const Tokens &t) { placeholder(t[4].scope()); });
  parser().foreach_match("MS[[A]]A:A{", [&](const Tokens &t) { placeholder(t[10].scope()); });
  parser().foreach_match("M[[A]]A:A{", [&](const Tokens &t) { placeholder(t[9].scope()); });

  parser.apply_mutations();

  auto process_enum = [&](Token enum_tok,
                          Token class_tok,
                          Token enum_name,
                          Token enum_type,
                          Scope enum_scope,
                          const bool is_host_shared) {
    const string type_str = enum_type.str();
    const string enum_name_str = enum_name.str();

    string previous_value = "error_invalid_first_value";
    enum_scope.foreach_scope(ScopeType::Assignment, [&](Scope scope) {
      Token name_tok = scope.front().prev();
      string name = name_tok.str();
      string value = scope.str();
      if (value == placeholder_value) {
        value = "= " + previous_value + " + 1" + (enum_type.str()[0] == 'u' ? "u" : "");
      }
      if (class_tok.is_valid()) {
        name = enum_name_str + "::" + name;
      }
      string decl = "constant static constexpr " + type_str + " " + name + " " + value + ";\n";
      parser.insert_line_number(enum_tok.prev(), name_tok.line_number());
      parser.insert_after(enum_tok.prev(), decl);

      previous_value = name;
    });
    parser.insert_directive(enum_tok.prev(),
                            "#define " + enum_name_str + " " + enum_type.str() + "\n");
    if (is_host_shared) {
      if (type_str != "uint32_t" && type_str != "int32_t") {
        report_error_(
            ERROR_TOK(enum_type),
            "enum declaration must use uint32_t or int32_t underlying type for interface "
            "compatibility");
        return;
      }

      string define = "#define ";
      define += enum_name_str + linted_struct_suffix + " " + enum_name_str + "\n";
      parser.insert_directive(enum_tok.prev(), define);
    }
    const string ctor_decl = enum_name_str + " " + enum_name_str + "_ctor_() { return " +
                             enum_name_str + "(0); }";
    parser.insert_directive(enum_tok.prev(), ctor_decl);
    parser.erase(enum_tok, enum_scope.back().next());
  };

  parser().foreach_match("MSA:A{", [&](vector<Token> tokens) {
    process_enum(tokens[0], tokens[1], tokens[2], tokens[4], tokens[5].scope(), false);
  });
  parser().foreach_match("MA:A{", [&](vector<Token> tokens) {
    process_enum(tokens[0], Token::invalid(), tokens[1], tokens[3], tokens[4].scope(), false);
  });
  parser().foreach_match("MS[[A]]A:A{", [&](vector<Token> tokens) {
    process_enum(tokens[0], tokens[1], tokens[7], tokens[9], tokens[10].scope(), true);
  });
  parser().foreach_match("M[[A]]A:A{", [&](vector<Token> tokens) {
    process_enum(tokens[0], Token::invalid(), tokens[6], tokens[8], tokens[9].scope(), true);
  });

  parser.apply_mutations();

  parser().foreach_token(
      Enum, [&](Token tok) { report_error_(ERROR_TOK(tok), "invalid enum declaration"); });
}

}  // namespace blender::gpu::shader
