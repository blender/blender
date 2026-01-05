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

void SourceProcessor::lower_unions(Parser &parser)
{
  struct Member {
    string type, name;
    size_t offset, size;
    bool is_enum;

    /* Return true for builtin trivial types (e.g. uint, float3). */
    bool is_trivial() const
    {
      return type.empty();
    }
  };

  auto is_struct_size_known = [&](Scope attributes) -> bool {
    if (attributes.is_invalid()) {
      return false;
    }
    bool is_shared = false;
    attributes.foreach_attribute([&](Token attr, Scope) {
      if (attr.str() == "host_shared") {
        is_shared = true;
      }
    });
    if (!is_shared) {
      return false;
    }
    return true;
  };

  /* Description of each union types. */
  unordered_map<string, vector<Member>> union_members;

  /* First, lower anonymous unions into separate struct. */
  parser().foreach_struct([&](Token struct_tok, Scope attrs, Token struct_name, Scope body) {
    int union_index = 0;
    body.foreach_match("o{..};", [&](const Tokens &t) {
      Scope union_body = t[1].scope();

      string union_name = "union" + to_string(union_index);
      string union_type = struct_name.str() + "_" + union_name;

      /* Parse members of the union for later use. */
      vector<Member> members;
      union_body.foreach_declaration(
          [&](Scope, Token, Token type, Scope, Token name, Scope array, Token) {
            if (array.is_valid()) {
              report_error_(ERROR_TOK(name), "Arrays are not supported inside unions.");
            }
            members.emplace_back(Member{type.str(), name.str(), 0, 0, type.prev() == Enum});
          });

      if (members.empty()) {
        report_error_(ERROR_TOK(t[0]), "Empty union");
        return;
      }
      union_members.emplace(union_type, members);

      string union_member = union_type + " " + union_name + ";";
      if (attrs.contains("host_shared")) {
        union_member = "struct " + union_member;
      }
      parser.insert_before(t.front(), union_member);
      parser.erase(t.front(), t.back());

      string type_decl = "struct [[host_shared]] " + union_type + " {\n";
      /* Temporary storage (use first member, still valid since all members should have the same
       * size). The real storage can only be set once we know the size of the union, which we can
       * only know after lowering them as outside types. */
      type_decl += "  " + members.front().type + " " + members.front().name + ";\n";
      type_decl += "};\n";

      parser.insert_line_number(struct_tok.str_index_start() - 1, t[0].line_number());
      parser.insert_before(struct_tok, type_decl);
      parser.insert_line_number(struct_tok.str_index_start() - 1, struct_tok.line_number());

      union_index++;
    });
  });
  parser.apply_mutations();

  /* Map structure name to structure members. */
  unordered_map<string, vector<Member>> struct_members = {
      {"float", {{"", "", 0, 4}}},
      {"float2", {{"", "", 0, 8}}},
      {"float4", {{"", "", 0, 16}}},
      {"bool32_t", {{"", "", 0, 4}}},
      {"int", {{"", "", 0, 4}}},
      {"int2", {{"", "", 0, 8}}},
      {"int4", {{"", "", 0, 16}}},
      {"uint", {{"", "", 0, 4}}},
      {"uint2", {{"", "", 0, 8}}},
      {"uint4", {{"", "", 0, 16}}},
      {"string_t", {{"", "", 0, 4}}},
      {"packed_float3", {{"", "", 0, 12}}},
      {"packed_int3", {{"", "", 0, 12}}},
      {"packed_uint3", {{"", "", 0, 12}}},
      {"float2x4", {{"float4", "[0]", 0, 16}, {"float4", "[1]", 16, 16}}},
      {"float3x4",
       {{"float4", "[0]", 0, 16}, {"float4", "[1]", 16, 16}, {"float4", "[2]", 32, 16}}},
      {"float4x4",
       {{"float4", "[0]", 0, 16},
        {"float4", "[1]", 16, 16},
        {"float4", "[2]", 32, 16},
        {"float4", "[3]", 48, 16}}},
  };

  auto type_size_get = [&](Token type) -> size_t {
    auto value = struct_members.find(type.str());
    if (value == struct_members.end()) {
      return 0;
    }

    int total_size = 0;
    for (const Member &member : value->second) {
      total_size += member.size;
    }
    return total_size;
  };

  /* Then populate struct members. */
  parser().foreach_struct([&](Token, Scope attributes, Token struct_name, Scope body) {
    if (!is_struct_size_known(attributes)) {
      return;
    }
    vector<Member> members;
    size_t offset = 0;
    body.foreach_declaration([&](Scope, Token, Token type, Scope, Token name, Scope array, Token) {
      size_t size = 4;

      size_t array_size = 0;
      if (array.is_valid()) {
        /* Assume size to be zero by default. It will create invalid size error later on. */
        array_size = static_array_size(array, 0);
      }
      else {
        array_size = 1;
      }

      for (int i = 0; i < array_size; i++) {
        string name_str = name.str();
        if (array.is_valid()) {
          name_str += "[" + to_string(i) + "]";
        }
        if (type.prev() != Enum) {
          size = type_size_get(type);
          if (size != 0) {
            members.emplace_back(Member{type.str(), "." + name_str, offset, size});
          }
        }
        else {
          members.emplace_back(Member{type.str(), "." + name_str, offset, size, true});
        }
        offset += size;
      }
    });

    struct_members.emplace(struct_name.str(), members);
  });

  /* Replace placeholder struct with a generic one. */
  auto replace_placeholder_member = [&](Scope body) {
    /* Replace placeholder struct with float members. */
    size_t size = type_size_get(body.front().next());
    if (size == 0) {
      report_error_(ERROR_TOK(body.front().next()),
                    "Can't infer size of member. Type must be defined in this file and have "
                    "the [[host_shared]] attribute.");
    }
    for (int i = 0; i < size; i += 16) {
      size_t member_size = size - i;
      const char *data_type = "float4";
      if (member_size == 4) {
        data_type = "float";
      }
      else if (member_size == 8) {
        data_type = "float2";
      }
      else if (member_size == 12) {
        data_type = "float3";
      }
      parser.insert_after(body.front().str_index_last_no_whitespace(),
                          "\n  " + string(data_type) + " data" + to_string(i / 16) + ";");
    }
    parser.erase(body.front().next(), body.back().prev());
  };

  auto member_from_float =
      [&](const Member &union_member, const Member &struct_member, const string &access) {
        /* Account for trivial types. */
        const string &type = struct_member.is_trivial() ? union_member.type : struct_member.type;
        bool is_enum = struct_member.is_trivial() ? union_member.is_enum : struct_member.is_enum;

        if (is_enum) {
          return struct_member.type + "(floatBitsToUint(" + access + "))";
        }
        if (type.substr(0, 4) == "uint") {
          return "floatBitsToUint(" + access + ")";
        }
        if (type.substr(0, 3) == "int") {
          return "floatBitsToInt(" + access + ")";
        }
        if (type == "bool") {
          return "floatBitsToInt(" + access + ") != 0";
        }
        return access;
      };

  auto member_to_float =
      [&](const Member &union_member, const Member &struct_member, const string &access) {
        /* Account for trivial types. */
        const string &type = struct_member.is_trivial() ? union_member.type : struct_member.type;
        bool is_enum = struct_member.is_trivial() ? union_member.is_enum : struct_member.is_enum;

        if (is_enum) {
          return "uintBitsToFloat(uint(" + access + "))";
        }
        if (type.substr(0, 4) == "uint") {
          return "uintBitsToFloat(" + access + ")";
        }
        if (type.substr(0, 3) == "int") {
          return "intBitsToFloat(" + access + ")";
        }
        if (type == "bool") {
          return "intBitsToFloat(int(" + access + "))";
        }
        return access;
      };

  auto union_data_access = [&](const Member &struct_member, size_t union_size) {
    const size_t offset = struct_member.offset;
    string access = ".data" + to_string(offset / 16);

    if (struct_member.size == 12) {
      access += ".xyz";
    }
    else if (struct_member.size == 8) {
      access += ((offset % 16) == 0) ? ".xy" : ".zw";
    }
    else if (struct_member.size == 4) {
      switch (offset % 16) {
        case 0:
          /* Special case if last member is a scalar. */
          access += ((union_size - offset) == 4) ? "" : ".x";
          break;
        case 4:
          access += ".y";
          break;
        case 8:
          access += ".z";
          break;
        case 12:
          access += ".w";
          break;
      }
    }
    return access;
  };

  auto member_data_access = [&](const Member &struct_member) -> string {
    return struct_member.is_trivial() ? string() : struct_member.name;
  };

  auto create_getter = [&](/* Tokens of the union declaration inside the struct. */
                           const Token &union_type_tok,
                           const Token &union_var_tok,
                           /* Union member we are creating the accessor for. */
                           const Member &union_member,
                           /* Definition of the type of the accessed member. */
                           const vector<Member> &struct_members) -> string {
    const size_t union_size = type_size_get(union_type_tok);
    if (union_size == 0) {
      report_error_(ERROR_TOK(union_type_tok),
                    "Can't infer size of member. Type must be defined in this file and have "
                    "the [[host_shared]] attribute.");
      return "";
    }
    const Member &last_member = struct_members.back();
    if (last_member.offset + last_member.size != union_size) {
      report_error_(ERROR_TOK(union_type_tok), "union has members of different sizes");
      return "";
    }

    string fn_body = "{\n";
    /* Declare return variable of the same type as the accessed member. */
    fn_body += "  " + union_member.type + " val;\n";
    for (const auto &member : struct_members) {
      string to_var = "val" + member_data_access(member);
      string access = union_var_tok.str() + union_data_access(member, union_size);
      fn_body += "  " + to_var + " = " + member_from_float(union_member, member, access) + ";\n";
    }
    fn_body += "  return val;\n";
    fn_body += "}\n";

    return "\n" + union_member.type + " " + union_member.name + "() const " + fn_body;
  };

  auto create_setter = [&](/* Tokens of the union declaration inside the struct. */
                           const Token &union_type_tok,
                           const Token &union_var_tok,
                           /* Union member we are creating the accessor for. */
                           const Member &union_member,
                           /* Definition of the type of the accessed member. */
                           const vector<Member> &struct_members) -> string {
    const size_t union_size = type_size_get(union_type_tok);
    if (union_size == 0) {
      report_error_(ERROR_TOK(union_type_tok),
                    "Can't infer size of member. Type must be defined in this file and have "
                    "the [[host_shared]] attribute.");
      return "";
    }
    const Member &last_member = struct_members.back();
    if (last_member.offset + last_member.size != union_size) {
      report_error_(ERROR_TOK(union_type_tok), "union has members of different sizes");
      return "";
    }

    string fn_body = "{\n";
    for (const auto &member : struct_members) {
      string to_var = "this->" + union_var_tok.str() + union_data_access(member, union_size);
      string access = "value" + member_data_access(member);
      fn_body += "  " + to_var + " = " + member_to_float(union_member, member, access) + ";\n";
    }
    fn_body += "}\n";

    return "\nvoid " + union_member.name + "_set_(" + union_member.type + " value) " + fn_body;
  };

  auto flatten_members = [&](Token type, vector<Member> &members) {
    vector<Member> dst;
    dst.reserve(members.size());
    bool expanded = false;
    for (const auto &member : members) {
      if (member.is_trivial() || member.is_enum) {
        dst.emplace_back(member);
        continue;
      }
      if (struct_members.find(member.type) == struct_members.end()) {
        report_error_(
            ERROR_TOK(type),
            "Unknown type encountered while unwrapping union. Contained types must be defined "
            "in this file and decorated with [[host_shared]] attribute.");
        continue;
      }

      vector<Member> nested_structure = struct_members.find(member.type)->second;
      for (Member nested_member : nested_structure) {
        if (nested_member.is_trivial() || nested_member.is_enum) {
          dst.emplace_back(member);
        }
        else {
          expanded = true;
          nested_member.name = member.name + nested_member.name;
          nested_member.offset = member.offset + nested_member.offset;
          dst.emplace_back(nested_member);
        }
      }
    }
    members = dst;
    return expanded;
  };

  parser().foreach_struct([&](Token, Scope, Token struct_name, Scope body) {
    if (union_members.find(struct_name.str()) != union_members.end()) {
      replace_placeholder_member(body);
      return;
    }

    body.foreach_declaration([&](Scope, Token, Token type, Scope, Token name, Scope, Token) {
      if (union_members.find(type.str()) == union_members.end()) {
        return;
      }

      const vector<Member> &members = union_members.find(type.str())->second;
      for (const auto &member : members) {
        if (struct_members.find(member.type) == struct_members.end()) {
          report_error_(
              ERROR_TOK(type),
              "Unknown union member type. Type must be defined in this file and decorated "
              "with [[host_shared]] attribute.");
          return;
        }
        vector<Member> structure = struct_members.find(member.type)->second;
        /* Flatten references to other structures, recursively. */
        while (flatten_members(type, structure)) {
        }

        parser.insert_after(body.back().prev(), create_getter(type, name, member, structure));
        parser.insert_after(body.back().prev(), create_setter(type, name, member, structure));
      }
    });
  });

  /* Replace assignment pattern.
   * Example: `a.b() = c;` >  `a.b_set_(c);`
   * This pattern is currently only allowed for `union_t`. */
  parser().foreach_match("w()=", [&](const Tokens &t) {
    parser.insert_before(t[1], "_set_");
    parser.erase(t[2], t[3]);
    parser.insert_after(t[3].scope().back(), ")");
  });

  parser.apply_mutations();
}

/**
 * For safety reason, union members need to be declared with the union_t template.
 * This avoid raw member access which we cannot emulate. Instead this forces the use of the `()`
 * operator for accessing the members of the enum.
 *
 * Need to run before lower_unions.
 */
void SourceProcessor::lower_union_accessor_templates(Parser &parser)
{
  parser().foreach_struct([&](Token, Scope, Token, Scope body) {
    body.foreach_match("o{..};", [&](const Tokens &t) {
      t[1].scope().foreach_declaration(
          [&](Scope, Token, Token type, Scope template_scope, Token name, Scope, Token) {
            if (type.str() != "union_t") {
              report_error_(
                  ERROR_TOK(name),
                  "All union members must have their type wrapped using the union_t<T> template.");
              parser.erase(type, type.find_next(SemiColon));
              return;
            }

            /* Remove the template but not the wrapped type. */
            parser.erase(type);
            if (template_scope.is_valid()) {
              parser.erase(template_scope.front());
              parser.erase(template_scope.back());
            }
          });
    });
  });
  parser.apply_mutations();
}

}  // namespace blender::gpu::shader
