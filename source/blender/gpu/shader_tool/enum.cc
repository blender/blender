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
using namespace shader::parser::ast;
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
    report_error(tokens[0], "enum declaration must explicitly use an underlying type");
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
    const string type_str(enum_type.str());
    const string enum_name_str(enum_name.str());

    string previous_value = "error_invalid_first_value";
    enum_scope.foreach_scope(ScopeType::Assignment, [&](Scope scope) {
      Token name_tok = scope.front().prev();
      string name(name_tok.str());
      string value(scope.str());
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
                            "#define " + enum_name_str + " " + string(enum_type.str()) + "\n");
    if (is_host_shared) {
      if (type_str != "uint32_t" && type_str != "int32_t") {
        report_error(enum_type,
                     "Host shared enum declaration must use uint32_t or int32_t underlying type");
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
    process_enum(tokens[0], Token(parser), tokens[1], tokens[3], tokens[4].scope(), false);
  });
  parser().foreach_match("MS[[A]]A:A{", [&](vector<Token> tokens) {
    process_enum(tokens[0], tokens[1], tokens[7], tokens[9], tokens[10].scope(), true);
  });
  parser().foreach_match("M[[A]]A:A{", [&](vector<Token> tokens) {
    process_enum(tokens[0], Token(parser), tokens[6], tokens[8], tokens[9].scope(), true);
  });

  parser.apply_mutations();

  parser().foreach_token(Enum, [&](Token tok) {
    report_error(tok, "invalid enum declaration, likely missing underlying type");
  });
}

void SourceProcessor::lower_enums_ast(Parser &parser)
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

  parser.root().foreach_recursive<ClassDecl>([&](ClassDecl cl) {
    if (cl.front() != Enum) {
      return;
    }
    bool is_enum_class = (cl.front().next() == Class);
    string enum_name(cl.identifier().str());
    string underlying_type(cl.parent_class().str());

    bool is_unsigned = underlying_type == "uchar" || underlying_type == "ushort" ||
                       underlying_type == "uint" || underlying_type == "uint32_t";
    string suffix = is_unsigned ? "u" : "";
    string class_prefix = is_enum_class ? enum_name + namespace_separator : "";

    Id last_id;
    cl.body().foreach<EnumValue>([&](EnumValue val) {
      Id id = val.identifier();

      /* Convert to static constant. */
      parser.insert_before(id.front(),
                           "constant static constexpr " + underlying_type + " " + class_prefix);

      /* Insert value if it doesn't exists. */
      AssignStmt assign = val.value();
      if (!assign.is_valid()) {
        int anchor = id.back().str_index_last_no_whitespace();
        if (!last_id.is_valid()) {
          /* First of list. */
          parser.insert_after(anchor, "= 0" + suffix);
        }
        else {
          parser.insert_after(anchor, "= " + string(last_id.str()) + " + 1" + suffix);
        }
      }

      /* Convert or insert trailing semicolon. */
      if (val.back() == ',') {
        parser.replace(val.back(), ";", true);
      }
      else {
        parser.insert_after(val.back().str_index_last_no_whitespace(), ";");
      }

      last_id = id;
    });

    Token enum_tok = cl.front();

    if (!enum_name.empty()) {
      /* Check host shared and create alias. */
      if (cl.attributes().contains_attr("host_shared")) {
        if (underlying_type != "uint32_t" && underlying_type != "int32_t") {
          report_error(
              cl.parent_class().front(),
              "Host shared enum declaration must use uint32_t or int32_t underlying type");
          return;
        }
        string define = "#define " + enum_name + linted_struct_suffix + " " + enum_name + "\n";
        parser.insert_directive(enum_tok.prev(), define);
      }

      /* Insert constructor. */
      string ctor = enum_name + " " + enum_name + "_ctor_() { return " + enum_name + "(0); }";
      parser.insert_directive(enum_tok.prev(), ctor);

      /* Insert type alias. */
      string alias = "#define " + enum_name + " " + underlying_type + "\n";
      parser.insert_directive(enum_tok.prev(), alias);
    }

    /* Erase original class declaration. */
    parser.erase(enum_tok, cl.body().front());
    parser.erase(cl.body().back(), cl.back());
  });

  parser.apply_mutations();
}

}  // namespace blender::gpu::shader
