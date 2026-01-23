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

/* Parse entry point definitions and mutating all parameter usage to global resources. */
void SourceProcessor::lower_entry_points(Parser &parser)
{
  using namespace metadata;

  auto to_uppercase = [](string str) {
    for (char &c : str) {
      c = toupper(c);
    }
    return str;
  };

  parser().foreach_function([&](bool, Token type, Token fn_name, Scope args, bool, Scope fn_body) {
    bool is_entry_point = false;
    bool is_compute_func = false;
    bool is_vertex_func = false;
    bool is_fragment_func = false;
    bool use_early_frag_test = false;
    string local_size;

    if (type.prev() == ']') {
      Scope attributes = type.prev().prev().scope();
      attributes.foreach_attribute([&](Token attr, Scope attr_scope) {
        const string attr_str = attr.str();
        if (attr_str == "vertex") {
          is_vertex_func = true;
          is_entry_point = true;
        }
        else if (attr_str == "fragment") {
          is_fragment_func = true;
          is_entry_point = true;
        }
        else if (attr_str == "compute") {
          is_compute_func = true;
          is_entry_point = true;
        }
        else if (attr_str == "early_fragment_tests") {
          use_early_frag_test = true;
        }
        else if (attr_str == "local_size") {
          local_size = attr_scope.str();
        }
      });
    }

    if (is_entry_point && type.str() != "void") {
      report_error_(ERROR_TOK(type), "Entry point function must return void.");
      return;
    }

    auto replace_word = [&](const string &replaced, const string &replacement) {
      fn_body.foreach_token(Word, [&](const Token tok) {
        if (tok.str() == replaced) {
          parser.replace(tok, replacement, true);
        }
      });
    };

    auto replace_word_and_accessor = [&](const string &replaced, const string &replacement) {
      fn_body.foreach_token(Word, [&](const Token tok) {
        if (tok.next().type() == Dot && tok.str() == replaced) {
          parser.replace(tok, tok.next(), replacement);
        }
      });
    };

    /* For now, just emit good old create info macros. */
    string create_info_decl;
    create_info_decl += "GPU_SHADER_CREATE_INFO(" + fn_name.str() + "_infos_)\n";

    if (!local_size.empty()) {
      if (!is_compute_func) {
        report_error_(ERROR_TOK(type),
                      "Only compute entry point function can use [[local_size(x,y,z)]].");
      }
      else {
        create_info_decl += "LOCAL_GROUP_SIZE" + local_size + "\n";
      }
    }

    if (use_early_frag_test) {
      if (!is_fragment_func) {
        report_error_(ERROR_TOK(type),
                      "Only fragment entry point function can use [[use_early_frag_test]].");
      }
      else {
        create_info_decl += "EARLY_FRAGMENT_TEST(true)\n";
      }
    }

    auto process_argument = [&](Token type, Token var, Scope attributes) {
      const bool is_const = type.prev() == Const;
      string srt_type = type.str();
      string srt_var = var.str();
      string srt_attr = attributes[1].str();

      if (srt_attr == "vertex_id" && is_entry_point) {
        if (!is_vertex_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[vertex_id]] is only supported in vertex functions.");
        }
        else if (!is_const || srt_type != "int") {
          report_error_(ERROR_TOK(type), "[[vertex_id]] must be declared as `const int`.");
        }
        replace_word(srt_var, "gl_VertexID");
        metadata_.builtins.emplace_back(Builtin(hash("gl_VertexID")));
        create_info_decl += "BUILTINS(BuiltinBits::VERTEX_ID)\n";
      }
      else if (srt_attr == "instance_id" && is_entry_point) {
        if (!is_vertex_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[instance_id]] is only supported in vertex functions.");
        }
        else if (!is_const || srt_type != "int") {
          report_error_(ERROR_TOK(type), "[[instance_id]] must be declared as `const int`.");
        }
        replace_word(srt_var, "gl_InstanceID");
        metadata_.builtins.emplace_back(Builtin(hash("gl_InstanceID")));
        create_info_decl += "BUILTINS(BuiltinBits::INSTANCE_ID)\n";
      }
      else if (srt_attr == "base_instance" && is_entry_point) {
        if (!is_vertex_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[base_instance]] is only supported in vertex functions.");
        }
        else if (!is_const || srt_type != "int") {
          report_error_(ERROR_TOK(type),
                        "[[base_instance]] must be declared as "
                        "`const int`.");
        }
        replace_word(srt_var, "gl_BaseInstance");
        metadata_.builtins.emplace_back(Builtin(hash("gl_BaseInstance")));
      }
      else if (srt_attr == "point_size" && is_entry_point) {
        if (!is_vertex_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[point_size]] is only supported in vertex functions.");
        }
        else if (is_const || srt_type != "float") {
          report_error_(ERROR_TOK(type),
                        "[[point_size]] must be declared as non-const reference (aka `float &`).");
        }
        replace_word(srt_var, "gl_PointSize");
        create_info_decl += "BUILTINS(BuiltinBits::POINT_SIZE)\n";
      }
      else if (srt_attr == "clip_distance" && is_entry_point) {
        if (!is_vertex_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[clip_distance]] is only supported in vertex functions.");
        }
        else if (is_const || srt_type != "float") {
          report_error_(ERROR_TOK(type),
                        "[[clip_distance]] must be declared as non-const reference "
                        "(aka `float (&)[]`).");
        }
        replace_word(srt_var, "gl_ClipDistance");
        create_info_decl += "BUILTINS(BuiltinBits::CLIP_DISTANCES)\n";
      }
      else if (srt_attr == "layer" && is_entry_point) {
        if (is_compute_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[layer]] is only supported in vertex and fragment functions.");
        }
        else if (is_vertex_func && (is_const || srt_type != "int")) {
          report_error_(ERROR_TOK(type),
                        "[[layer]] must be declared as non-const reference "
                        "(aka `int &`).");
        }
        else if (is_fragment_func && (!is_const || srt_type != "int")) {
          report_error_(ERROR_TOK(type),
                        "[[layer]] must be declared as const reference "
                        "(aka `const int &`).");
        }
        replace_word(srt_var, "gl_Layer");
        create_info_decl += "BUILTINS(BuiltinBits::LAYER)\n";
      }
      else if (srt_attr == "viewport_index" && is_entry_point) {
        if (is_compute_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[viewport_index]] is only supported in vertex and "
                        "fragment functions.");
        }
        else if (is_vertex_func && (is_const || srt_type != "int")) {
          report_error_(ERROR_TOK(type),
                        "[[viewport_index]] must be declared as non-const reference "
                        "(aka `int &`).");
        }
        else if (is_fragment_func && (!is_const || srt_type != "int")) {
          report_error_(ERROR_TOK(type),
                        "[[viewport_index]] must be declared as const reference "
                        "(aka `const int &`).");
        }
        replace_word(srt_var, "gl_ViewportIndex");
        create_info_decl += "BUILTINS(BuiltinBits::VIEWPORT_INDEX)\n";
      }
      else if (srt_attr == "position" && is_entry_point) {
        if (!is_vertex_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[position]] is only supported in vertex functions.");
        }
        else if (is_const || srt_type != "float4") {
          report_error_(ERROR_TOK(type),
                        "[[position]] must be declared as non-const reference (aka `float4 &`).");
        }
        else {
          replace_word(srt_var, "gl_Position");
        }
      }
      else if (srt_attr == "frag_coord" && is_entry_point) {
        if (!is_fragment_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[frag_coord]] is only supported in fragment functions.");
        }
        else if (!is_const || srt_type != "float4") {
          report_error_(ERROR_TOK(type), "[[frag_coord]] must be declared as `const float4`.");
        }
        else {
          create_info_decl += "BUILTINS(BuiltinBits::FRAG_COORD)\n";
          replace_word(srt_var, "gl_FragCoord");
        }
      }
      else if (srt_attr == "point_coord" && is_entry_point) {
        if (!is_fragment_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[point_coord]] is only supported in fragment functions.");
        }
        else if (!is_const || srt_type != "float2") {
          report_error_(ERROR_TOK(type), "[[point_coord]] must be declared as `const float2`.");
        }
        else {
          create_info_decl += "BUILTINS(BuiltinBits::POINT_COORD)\n";
          replace_word(srt_var, "gl_PointCoord");
        }
      }
      else if (srt_attr == "front_facing" && is_entry_point) {
        if (!is_fragment_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[front_facing]] is only supported in fragment functions.");
        }
        else if (!is_const || srt_type != "bool") {
          report_error_(ERROR_TOK(type), "[[front_facing]] must be declared as `const bool`.");
        }
        else {
          create_info_decl += "BUILTINS(BuiltinBits::FRONT_FACING)\n";
          replace_word(srt_var, "gl_FrontFacing");
        }
      }
      else if (srt_attr == "global_invocation_id" && is_entry_point) {
        if (!is_compute_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[global_invocation_id]] is only supported in compute functions.");
        }
        else if (!is_const || srt_type != "uint3") {
          report_error_(ERROR_TOK(type),
                        "[[global_invocation_id]] must be declared as `const uint3`.");
        }
        else {
          create_info_decl += "BUILTINS(BuiltinBits::GLOBAL_INVOCATION_ID)\n";
          replace_word(srt_var, "gl_GlobalInvocationID");
        }
      }
      else if (srt_attr == "local_invocation_id" && is_entry_point) {
        if (!is_compute_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[local_invocation_id]] is only supported in compute functions.");
        }
        else if (!is_const || srt_type != "uint3") {
          report_error_(ERROR_TOK(type),
                        "[[local_invocation_id]] must be declared as `const uint3`.");
        }
        else {
          create_info_decl += "BUILTINS(BuiltinBits::LOCAL_INVOCATION_ID)\n";
          replace_word(srt_var, "gl_LocalInvocationID");
        }
      }
      else if (srt_attr == "local_invocation_index" && is_entry_point) {
        if (!is_compute_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[local_invocation_index]] is only supported in compute functions.");
        }
        else if (!is_const || srt_type != "uint") {
          report_error_(ERROR_TOK(type),
                        "[[local_invocation_index]] must be declared as `const uint`.");
        }
        else {
          create_info_decl += "BUILTINS(BuiltinBits::LOCAL_INVOCATION_INDEX)\n";
          replace_word(srt_var, "gl_LocalInvocationIndex");
        }
      }
      else if (srt_attr == "work_group_id" && is_entry_point) {
        if (!is_compute_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[work_group_id]] is only supported in compute functions.");
        }
        else if (!is_const || srt_type != "uint3") {
          report_error_(ERROR_TOK(type),
                        "[[work_group_id]] must be declared as "
                        "`const uint3`.");
        }
        else {
          create_info_decl += "BUILTINS(BuiltinBits::WORK_GROUP_ID)\n";
          replace_word(srt_var, "gl_WorkGroupID");
        }
      }
      else if (srt_attr == "num_work_groups" && is_entry_point) {
        if (!is_compute_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[num_work_groups]] is only supported in compute functions.");
        }
        else if (!is_const || srt_type != "uint3") {
          report_error_(ERROR_TOK(type),
                        "[[num_work_groups]] must be declared as "
                        "`const uint3`.");
        }
        else {
          create_info_decl += "BUILTINS(BuiltinBits::NUM_WORK_GROUP)\n";
          replace_word(srt_var, "gl_NumWorkGroups");
        }
      }
      else if (srt_attr == "in") {
        if (is_compute_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[in]] is only supported in vertex and fragment functions.");
        }
        else if (!is_const) {
          report_error_(ERROR_TOK(type), "[[in]] must be declared as const reference.");
        }
        else if (is_vertex_func) {
          replace_word_and_accessor(srt_var, "");
          create_info_decl += "ADDITIONAL_INFO(" + srt_type + ")\n";
        }
        else if (is_fragment_func) {
          replace_word_and_accessor(srt_var, srt_type + "_");
          // create_info_decl += "VERTEX_OUT(" + srt_type + ")\n";
        }
      }
      else if (srt_attr == "out") {
        if (is_compute_func) {
          report_error_(ERROR_TOK(attributes[1]),
                        "[[out]] is only supported in vertex and fragment functions.");
        }
        else if (is_const) {
          report_error_(ERROR_TOK(type), "[[out]] must be declared as non-const reference.");
        }
        else if (is_vertex_func) {
          replace_word_and_accessor(srt_var, srt_type + "_");
          create_info_decl += "VERTEX_OUT(" + srt_type + "_t)\n";
        }
        else if (is_fragment_func) {
          replace_word_and_accessor(srt_var, srt_type + "_");
          create_info_decl += "ADDITIONAL_INFO(" + srt_type + ")\n";
        }
      }
      else if (srt_attr == "resource_table") {
        if (is_entry_point) {
          /* Add dummy var at start of function body. */
          parser.insert_after(fn_body.front().str_index_start(),
                              " " + srt_type + " " + srt_var + "{};");
          create_info_decl += "ADDITIONAL_INFO(" + srt_type + ")\n";
        }
      }
      else if (srt_attr == "frag_depth") {
        if (srt_type != "float") {
          report_error_(ERROR_TOK(type), "[[frag_depth]] needs to be declared as float");
        }
        const string mode = attributes[3].str();

        if (mode != "any" && mode != "greater" && mode != "less") {
          report_error_(ERROR_TOK(attributes[3]),
                        "unrecognized mode, expecting 'any', 'greater' or 'less'");
        }
        else {
          create_info_decl += "DEPTH_WRITE(" + to_uppercase(mode) + ")\n";
          replace_word(srt_var, "gl_FragDepth");
        }
      }
      else if (srt_attr == "frag_stencil_ref") {
        if (srt_type != "int") {
          report_error_(ERROR_TOK(type), "[[frag_stencil_ref]] needs to be declared as int");
        }
        else {
          create_info_decl += "BUILTINS(BuiltinBits::STENCIL_REF)\n";
          replace_word(srt_var, "gl_FragStencilRefARB");
        }
      }
      else {
        report_error_(ERROR_TOK(attributes[1]), "Invalid attribute.");
      }
    };

    args.foreach_match("[[..]]c?AA", [&](const vector<Token> toks) {
      process_argument(toks[8], toks[9], toks[1].scope());
    });
    args.foreach_match("[[..]]c?A&A", [&](const vector<Token> toks) {
      process_argument(toks[8], toks[10], toks[1].scope());
    });

    args.foreach_match("[[..]]c?A(&A)", [&](const vector<Token> toks) {
      process_argument(toks[8], toks[11], toks[1].scope());
    });

    create_info_decl += "GPU_SHADER_CREATE_END()\n";

    if (is_entry_point) {
      metadata_.create_infos_declarations.emplace_back(create_info_decl);
    }
  });

  parser.apply_mutations();
}

/* Removes entry point arguments to make it compatible with the legacy code.
 * Has to run after mutation related to function arguments. */
void SourceProcessor::lower_entry_points_signature(Parser &parser)
{
  using namespace metadata;

  parser().foreach_function([&](bool, Token type, Token name, Scope args, bool, Scope fn_body) {
    bool is_entry_point = false;

    if (type.prev() == ']') {
      Scope attributes = type.prev().prev().scope();
      attributes.foreach_attribute([&](Token attr, Scope) {
        const string attr_str = attr.str();
        if (attr_str == "vertex" || attr_str == "fragment" || attr_str == "compute") {
          is_entry_point = true;
        }
      });
    }

    if (is_entry_point && args.str() != "()") {
      parser.erase(args.front().next(), args.back().prev());
    }

    /* Mute entry points when not enabled.
     * Could be lifted at some point, but for now required because of stage_in/out parameters. */
    if (is_entry_point) {
      /* Take attributes into account. */
      parser.insert_directive(type.prev().scope().front().prev(),
                              "#if defined(ENTRY_POINT_" + name.str() + ")");
      parser.insert_directive(fn_body.back(), "#endif");
    }
  });

  parser.apply_mutations();
}
}  // namespace blender::gpu::shader
