/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include <cctype>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "intermediate.hh"
#include "metadata.hh"
#include "processor.hh"

namespace blender::gpu::shader {
using namespace std;
using namespace shader::parser;
using namespace metadata;

#define ERROR_TOK(token) (token).line_number(), (token).char_number(), (token).line_str()

SourceProcessor::Result SourceProcessor::convert(vector<Symbol> symbols_set)
{
  metadata_ = {};

  if (language_ == Language::UNKNOWN) {
    report_error_(0, 0, "", "Unknown file type");
    return {"", metadata_};
  }
  /* Extend. */
  metadata_.symbol_table.insert(
      metadata_.symbol_table.end(), symbols_set.begin(), symbols_set.end());

  const string filename = filepath_.substr(filepath_.find_last_of('/') + 1);

  string str = this->source_;

  str = remove_comments(str);
  if (language_ == Language::BLENDER_GLSL || language_ == Language::CPP) {
    str = disabled_code_mutation(str);
  }
  else {
    IntermediateForm<SimpleLexer, DummyParser> parser(str, report_error_);
    /* Remove trailing white space as they make the subsequent regex much slower. */
    cleanup_whitespace(parser);
    str = parser.result_get();
  }
  str = threadgroup_variables_parse_and_remove(str);
  if (language_ == Language::BLENDER_GLSL || language_ == Language::CPP) {
    {
      parse_builtins(str, filename);
      Parser parser(str, report_error_);

      /* Preprocessor directive parsing & linting. */
      if (language_ == Language::BLENDER_GLSL) { /* TODO(fclem): Enforce in C++ header too. */
        lint_pragma_once(parser, filename);
      }
      parse_pragma_runtime_generated(parser);
      parse_includes(parser);
      parse_defines(parser);
      parse_legacy_create_info(parser);
      parse_library_functions(parser);

      lower_preprocessor(parser);

      parser.apply_mutations();

      /* Early out for certain files. */
      if (parser.str().find("\n#pragma no_processing") != string::npos) {
        cleanup_whitespace(parser);
        return {line_directive_prefix(filename) + parser.result_get(), metadata_};
      }

      parse_local_symbols(parser);

      /* Lower high level parsing complexity.
       * Merge tokens that can be combined together,
       * remove the token that are unsupported or that are noop.
       * All these steps should be independent. */
      lower_attribute_sequences(parser);
      lower_strings_sequences(parser);
      lower_swizzle_methods(parser);
      lower_classes(parser);
      lower_noop_keywords(parser);
      lower_trailing_comma_in_list(parser);
      lower_comma_separated_declarations(parser);

      parser.apply_mutations();

      /* Linting phase. Detect valid syntax with invalid usage. */
      lint_unbraced_statements(parser);
      lint_reserved_tokens(parser);
      lint_attributes(parser);
      lint_global_scope_constants(parser);
      lint_constructors(parser);
      lint_forward_declared_structs(parser);

      /* Lint and remove C++ accessor templates before lowering template. */
      lower_srt_accessor_templates(parser);
      lower_union_accessor_templates(parser);
      /* Lower templates. */
      lower_template_dependent_names(parser);
      lower_templates(parser);
      /* Lower namespaces. */
      lower_using(parser);
      lower_namespaces(parser);
      lower_scope_resolution_operators(parser);
      /* Lower unions and then lint shared structures. */
      lower_unions(parser);
      lower_host_shared_structures(parser);
      /* Lower enums. */
      lower_enums(parser);
      /* Lower SRT and Interfaces. */
      lower_entry_points(parser);
      lower_pipeline_definition(parser, filename);
      lower_resource_table(parser);
      lower_resource_access_functions(parser);
      /* Lower class methods. */
      lower_default_constructors(parser);
      lower_function_default_arguments(parser);
      lower_implicit_member(parser);
      lower_method_definitions(parser);
      lower_method_calls(parser);
      lower_empty_struct(parser);
      /* Lower SRT accesses. */
      lower_srt_member_access(parser);
      lower_srt_arguments(parser);
      lower_entry_points_signature(parser);
      lower_stage_function(parser);
      /* Lower string, assert, printf. */
      lower_assert(parser, filename);
      lower_strings(parser);
      lower_printf(parser);
      /* Lower other C++ constructs. */
      lower_implicit_return_types(parser);
      lower_initializer_implicit_types(parser);
      lower_designated_initializers(parser);
      lower_aggregate_initializers(parser);
      lower_array_initializations(parser);
      lower_scope_resolution_operators(parser);
      /* Lower references. */
      lower_reference_arguments(parser);
      lower_reference_variables(parser);
      /* Lower control flow. */
      lower_static_branch(parser);
      /* Unroll last to avoid processing more tokens in other phases. */
      lower_loop_unroll(parser);

      /* GLSL syntax compatibility.
       * TODO(fclem): Remove. */
      lower_argument_qualifiers(parser);

      /* Cleanup to make output more human readable and smaller for runtime. */
      cleanup_whitespace(parser);
      cleanup_empty_lines(parser);
      cleanup_line_directives(parser);
      str = parser.result_get();
    }

    str = line_directive_prefix(filename) + str;
    return {str, metadata_};
  }

  if (language_ == Language::MSL) {
    Parser parser(str, report_error_);
    parse_pragma_runtime_generated(parser);
    parse_includes(parser);
    lower_preprocessor(parser);
    str = parser.result_get();
  }
  if (language_ == Language::GLSL) {
    parse_builtins(str, filename, true);
#ifdef __APPLE__ /* Limiting to Apple hardware since GLSL compilers might have issues. */
    str = matrix_constructor_mutation(str);
#endif
  }
  str = argument_decorator_macro_injection(str);
  str = array_constructor_macro_injection(str);
  str = line_directive_prefix(filename) + str;
  return {str, metadata_};
}

metadata::Source SourceProcessor::parse_include_and_symbols()
{
  metadata_ = {};

  string str = this->source_;
  str = remove_comments(str);
  str = disabled_code_mutation(str);

  Parser parser(str, report_error_);
  parse_pragma_runtime_generated(parser);
  parse_includes(parser);

  parser.apply_mutations();

  lower_preprocessor(parser);

  parser.apply_mutations();

  parse_local_symbols(parser);

  return metadata_;
}

string SourceProcessor::remove_comments(const string &str)
{
  string out_str = str;
  {
    /* Multi-line comments. */
    size_t start, end = 0;
    while ((start = out_str.find("/*", end)) != string::npos) {
      end = out_str.find("*/", start + 2);
      if (end == string::npos) {
        break;
      }
      for (size_t i = start; i < end + 2; ++i) {
        if (out_str[i] != '\n') {
          out_str[i] = ' ';
        }
      }
    }

    if (end == string::npos) {
      report_error_(line_number(out_str, start),
                    char_number(out_str, start),
                    line_str(out_str, start),
                    "Malformed multi-line comment.");
      return out_str;
    }
  }
  {
    /* Single-line comments. */
    size_t start, end = 0;
    while ((start = out_str.find("//", end)) != string::npos) {
      end = out_str.find('\n', start + 2);
      if (end == string::npos) {
        end = out_str.size();
      }
      for (size_t i = start; i < end; ++i) {
        out_str[i] = ' ';
      }
    }
  }
  return out_str;
}

/* Remove trailing white spaces. */
template<typename ParserT> void SourceProcessor::cleanup_whitespace(ParserT &parser)
{
  const string &str = parser.str();

  size_t last_whitespace = -1;
  while ((last_whitespace = str.find(" \n", last_whitespace + 1)) != string::npos) {
    size_t first_not_whitespace = str.find_last_not_of(" ", last_whitespace);
    if (first_not_whitespace == string::npos) {
      first_not_whitespace = 0;
    }
    parser.replace(first_not_whitespace + 1, last_whitespace, "");
  }
  parser.apply_mutations();
}

/* Parse defines in order to output them with the create infos.
 * This allow the create infos to use shared defines values. */
void SourceProcessor::parse_defines(Parser &parser)
{
  parser().foreach_match("#A", [&](const vector<Token> &tokens) {
    if (tokens[1].str() == "define") {
      metadata_.create_infos_defines.emplace_back(tokens[1].next().scope().str_with_whitespace());
    }
    if (tokens[1].str() == "undef") {
      metadata_.create_infos_defines.emplace_back(tokens[1].next().scope().str_with_whitespace());
    }
  });
}

string SourceProcessor::get_create_info_placeholder(const string &name)
{
  string placeholder;
  placeholder += "#ifdef CREATE_INFO_RES_PASS_" + name + "\n";
  placeholder += "CREATE_INFO_RES_PASS_" + name + "\n";
  placeholder += "#endif\n";
  placeholder += "#ifdef CREATE_INFO_RES_BATCH_" + name + "\n";
  placeholder += "CREATE_INFO_RES_BATCH_" + name + "\n";
  placeholder += "#endif\n";
  placeholder += "#ifdef CREATE_INFO_RES_GEOMETRY_" + name + "\n";
  placeholder += "CREATE_INFO_RES_GEOMETRY_" + name + "\n";
  placeholder += "#endif\n";
  placeholder += "#ifdef CREATE_INFO_RES_SHARED_VARS_" + name + "\n";
  placeholder += "CREATE_INFO_RES_SHARED_VARS_" + name + "\n";
  placeholder += "#endif\n";
  return placeholder;
};

/* Legacy create info parsing and removing. */
void SourceProcessor::parse_legacy_create_info(Parser &parser)
{
  parser().foreach_scope(ScopeType::Attributes, [&](const Scope attrs) {
    if (attrs.str_with_whitespace() != "[resource_table]") {
      return;
    }
    Token type = attrs.scope().back().next();
    Token struct_keyword = attrs.scope().front().prev();
    if (type != Word || struct_keyword != Struct) {
      return;
    }
    parser.insert_before(struct_keyword, get_create_info_placeholder(type.str()));
    parser.insert_line_number(struct_keyword.str_index_start() - 1, struct_keyword.line_number());
  });

  parser().foreach_match("A(..)", [&](const vector<Token> &tokens) {
    if (tokens[0].str() == "CREATE_INFO_VARIANT") {
      const string variant_name = tokens[1].scope().front().next().str();
      metadata_.create_infos.emplace_back(variant_name);

      const string variant_decl = parser.substr_range_inclusive(tokens.front(), tokens.back());
      metadata_.create_infos_declarations.emplace_back(variant_decl);

      parser.replace(tokens.front(), tokens.back(), get_create_info_placeholder(variant_name));
      return;
    }
    if (tokens[0].str() == "GPU_SHADER_CREATE_INFO") {
      const string variant_name = tokens[1].scope().front().next().str();
      metadata_.create_infos.emplace_back(variant_name);

      const size_t start_end = tokens.back().str_index_last();
      const string end_tok = "GPU_SHADER_CREATE_END()";
      const size_t end_pos = parser.str().find(end_tok, start_end);
      if (end_pos == string::npos) {
        report_error_(ERROR_TOK(tokens[0]), "Missing create info end.");
        return;
      }

      const string variant_decl = parser.substr_range_inclusive(tokens.front().str_index_start(),
                                                                end_pos + end_tok.size());
      metadata_.create_infos_declarations.emplace_back(variant_decl);

      parser.replace(tokens.front().str_index_start(),
                     end_pos + end_tok.size(),
                     get_create_info_placeholder(variant_name));
      return;
    }
    if (tokens[0].str() == "GPU_SHADER_NAMED_INTERFACE_INFO") {
      const size_t start_end = tokens.back().str_index_last();
      const string end_str = "GPU_SHADER_NAMED_INTERFACE_END(";
      size_t end_pos = parser.str().find(end_str, start_end);
      if (end_pos == string::npos) {
        report_error_(ERROR_TOK(tokens[0]), "Missing create info end.");
        return;
      }

      end_pos = parser.str().find(')', end_pos);
      if (end_pos == string::npos) {
        report_error_(ERROR_TOK(tokens[0]), "Missing parenthesis at info end.");
        return;
      }

      const string variant_decl = parser.substr_range_inclusive(tokens.front().str_index_start(),
                                                                end_pos);
      metadata_.create_infos_declarations.emplace_back(variant_decl);

      parser.erase(tokens.front().str_index_start(), end_pos);
      return;
    }
    if (tokens[0].str() == "GPU_SHADER_INTERFACE_INFO") {
      const size_t start_end = tokens.back().str_index_last();
      const string end_str = "GPU_SHADER_INTERFACE_END()";
      size_t end_pos = parser.str().find(end_str, start_end);
      if (end_pos == string::npos) {
        report_error_(ERROR_TOK(tokens[0]), "Missing create info end.");
        return;
      }
      const string variant_decl = parser.substr_range_inclusive(tokens.front().str_index_start(),
                                                                end_pos + end_str.size());
      metadata_.create_infos_declarations.emplace_back(variant_decl);

      parser.erase(tokens.front().str_index_start(), end_pos + end_str.size());
      return;
    }
  });

  parser.apply_mutations();
}

void SourceProcessor::parse_includes(Parser &parser)
{
  parser().foreach_match("#A\"", [&](const vector<Token> &tokens) {
    if (tokens[1].str() != "include") {
      return;
    }
    string dependency_name = tokens[2].str_exclusive();

    if (dependency_name.find("defines.hh") != string::npos) {
      /* Dependencies between create infos are not needed for reflections.
       * Only the dependencies on the defines are needed. */
      metadata_.create_infos_dependencies.emplace_back(dependency_name);
    }

    if (dependency_name == "BLI_utildefines_variadic.h") {
      /* Skip GLSL-C++ stubs. They are only for IDE linting. */
      parser.erase(tokens.front(), tokens.back());
      return;
    }
    if (dependency_name == "gpu_shader_compat.hh") {
      /* Skip GLSL-C++ stubs. They are only for IDE linting. */
      parser.erase(tokens.front(), tokens.back());
      return;
    }
    if (dependency_name.find("gpu_shader_create_info.hh") != string::npos) {
      /* Skip info files. They are only for IDE linting. */
      parser.erase(tokens.front(), tokens.back());
      return;
    }

    if (dependency_name.find("infos/") != string::npos) {
      dependency_name = dependency_name.substr(6);
    }

    metadata_.dependencies.emplace_back(dependency_name);
  });
}

void SourceProcessor::parse_pragma_runtime_generated(Parser &parser)
{
  if (parser.str().find("\n#pragma runtime_generated") != string::npos) {
    metadata_.builtins.emplace_back(metadata::Builtin::runtime_generated);
  }
}

void SourceProcessor::lint_pragma_once(Parser &parser, const string &filename)
{
  if (filename.find("_lib.") == string::npos && filename.find(".hh") == string::npos) {
    return;
  }
  if (parser.str().find("\n#pragma once") == string::npos) {
    report_error_(0, 0, "", "Header files must contain #pragma once directive.");
  }
}

string SourceProcessor::disabled_code_mutation(const string &str)
{
  Parser parser(str, report_error_);

  auto process_disabled_scope = [&](Token start_tok) {
    /* Search for endif with the same indentation. Assume formatted input. */
    string end_str = start_tok.str_with_whitespace() + "endif";
    size_t scope_end = parser.str().find(end_str, start_tok.str_index_start());
    if (scope_end == string::npos) {
      report_error_(ERROR_TOK(start_tok), "Couldn't find end of disabled scope.");
      return;
    }
    /* Search for else/elif with the same indentation. Assume formatted input. */
    string else_str = start_tok.str_with_whitespace() + "el";
    size_t scope_else = parser.str().find(else_str, start_tok.str_index_start());
    if (scope_else != string::npos && scope_else < scope_end) {
      /* Only erase the content and keep the preprocessor directives. */
      parser.erase(start_tok.line_end() + 1, scope_else - 1);
    }
    else {
      /* Erase the content and the preprocessor directives. */
      parser.erase(start_tok.str_index_start(), scope_end + end_str.size());
    }
  };

  parser().foreach_match("#AA", [&](const vector<Token> &tokens) {
    if (tokens[1].str() == "ifndef" && tokens[2].str() == "GPU_SHADER") {
      process_disabled_scope(tokens[0]);
    }
  });
  parser().foreach_match("#i!A(A)", [&](const vector<Token> &tokens) {
    if (tokens[1].str() == "if" && tokens[3].str() == "defined" && tokens[5].str() == "GPU_SHADER")
    {
      process_disabled_scope(tokens[0]);
    }
  });
  parser().foreach_match("#i1", [&](const vector<Token> &tokens) {
    if (tokens[1].str() == "if" && tokens[2].str() == "0") {
      process_disabled_scope(tokens[0]);
    }
  });
  return parser.result_get();
}

void SourceProcessor::lower_preprocessor(Parser &parser)
{
  /* Remove unsupported directives. */

  parser().foreach_match("#A", [&](const vector<Token> &tokens) {
    if (tokens[1].str() == "pragma") {
      Token next = tokens[1].next();
      if (next.str() == "once") {
        parser.erase(tokens.front(), next);
      }
      else if (next.str() == "runtime_generated") {
        parser.erase(tokens.front(), next);
      }
    }
    else if (tokens[1].str() == "include" && tokens[1].next() == String) {
      parser.erase(tokens.front(), tokens[1].next());
    }
  });
}

/* Support for BLI swizzle syntax. */
void SourceProcessor::lower_swizzle_methods(Parser &parser)
{
  /* Change C++ swizzle functions into plain swizzle. */
  /** IMPORTANT: This prevent the usage of any method with a swizzle name. */
  parser().foreach_match(".A()", [&](const vector<Token> &tokens) {
    string method_name = tokens[1].str();
    if (method_name.length() > 1 && method_name.length() <= 4 &&
        (method_name.find_first_not_of("xyzw") == string::npos ||
         method_name.find_first_not_of("rgba") == string::npos))
    {
      /* `.xyz()` -> `.xyz` */
      /* Keep character count the same. Replace parenthesis by spaces. */
      parser.erase(tokens[2], tokens[3]);
    }
  });
}

string SourceProcessor::threadgroup_variables_parse_and_remove(const string &str)
{
  Parser parser(str, report_error_);

  auto process_shared_var = [&](Token shared_tok, Token type, Token name, Token decl_end) {
    if (shared_tok.str() == "shared") {
      metadata_.shared_variables.push_back(
          {type.str(), parser.substr_range_inclusive(name, decl_end.prev())});

      parser.erase(shared_tok, decl_end);
    }
  };
  parser().foreach_match("AAA;", [&](const vector<Token> &tokens) {
    process_shared_var(tokens[0], tokens[1], tokens[2], tokens.back());
  });
  parser().foreach_match("AAA[..];", [&](const vector<Token> &tokens) {
    process_shared_var(tokens[0], tokens[1], tokens[2], tokens.back());
  });
  parser().foreach_match("AAA[..][..];", [&](const vector<Token> &tokens) {
    process_shared_var(tokens[0], tokens[1], tokens[2], tokens.back());
  });
  parser().foreach_match("AAA[..][..][..];", [&](const vector<Token> &tokens) {
    process_shared_var(tokens[0], tokens[1], tokens[2], tokens.back());
  });
  /* If more array depth is needed, find a less dumb solution. */

  return parser.result_get();
}

void SourceProcessor::parse_library_functions(Parser &parser)
{
  using namespace metadata;

  parser().foreach_function(
      [&](bool is_static, Token fn_type, Token fn_name, Scope fn_args, bool, Scope) {
        Token first_tok = is_static ? fn_type.prev() : fn_type;
        Scope attributes = first_tok.attribute_before();
        if (!attributes.contains("node")) {
          return;
        }
        if (fn_type.str() != "void") {
          report_error_(ERROR_TOK(fn_type), "Expected void return type for node function");
          return;
        }
        if (fn_args.token_count() <= 3) {
          report_error_(ERROR_TOK(fn_type), "Expected at least one argument for node function");
          return;
        }
        FunctionFormat fn;
        fn.name = fn_name.str();

        fn_args.foreach_scope(ScopeType::FunctionArg, [&](Scope arg) {
          /* Note: There is no array support. */
          const Token name = arg.back();
          const Token type = name.prev() == '&' ? name.prev().prev() : name.prev();
          string qualifier = type.prev().str();
          if (qualifier != "out" && qualifier != "inout" && qualifier != "in") {
            if (name.prev() == '&') {
              qualifier = "out";
            }
            else if (qualifier != "const" && qualifier != "(" && qualifier != ",") {
              report_error_(ERROR_TOK(type.prev()),
                            "Unrecognized qualifier, expecting 'const', 'in', 'out' or 'inout'.");
              qualifier = "in";
            }
            else {
              qualifier = "in";
            }
          }
          fn.arguments.emplace_back(ArgumentFormat{metadata::Qualifier(hash(qualifier)),
                                                   metadata::Type(hash(type.str()))});
        });

        metadata_.functions.emplace_back(fn);
      });
}

void SourceProcessor::parse_builtins(const string &str, const string &filename, bool pure_glsl)
{
  const bool skip_drw_debug = filename == "draw_debug_draw_lib.glsl" ||
                              filename == "draw_debug_infos.hh" ||
                              filename == "draw_debug_draw_display_vert.glsl" ||
                              filename == "draw_shader_shared.hh";
  using namespace metadata;
  /* TODO: This can trigger false positive caused by disabled #if blocks. */
  vector<string> tokens = {
      "gl_FragCoord",
      "gl_FragStencilRefARB",
      "gl_FrontFacing",
      "gl_GlobalInvocationID",
      "gpu_InstanceIndex",
      "gpu_BaseInstance",
      "gl_InstanceID",
      "gl_LocalInvocationID",
      "gl_LocalInvocationIndex",
      "gl_NumWorkGroup",
      "gl_PointCoord",
      "gl_PointSize",
      "gl_PrimitiveID",
      "gl_VertexID",
      "gl_WorkGroupID",
      "gl_WorkGroupSize",
  };

  if (pure_glsl) {
    /* Only parsed for Python GLSL sources as false positive of this are costly. */
    tokens.emplace_back("gl_ClipDistance");
  }
  else {
    /* Assume blender GLSL or BSL. */
    tokens.emplace_back("drw_debug_");
    tokens.emplace_back("printf");
#ifdef WITH_GPU_SHADER_ASSERT
    tokens.emplace_back("assert");
#endif
  }

  for (auto &token : tokens) {
    if (skip_drw_debug && token == "drw_debug_") {
      continue;
    }
    if (str.find(token) != string::npos) {
      metadata_.builtins.emplace_back(Builtin(hash(token)));
    }
  }
}

/* Add padding member to empty structs.
 * Empty structs are useful for templating. */
void SourceProcessor::lower_empty_struct(Parser &parser)
{
  parser().foreach_match(
      "sA{};", [&](const vector<Token> &tokens) { parser.insert_after(tokens[2], "int _pad;"); });
  parser.apply_mutations();
}

/* Parse, convert to create infos, and erase declaration. */
void SourceProcessor::lower_pipeline_definition(Parser &parser, const string &filename)
{
  using namespace metadata;

  auto process_compilation_constants = [&](Token tok) {
    string create_info_decl;

    while (tok == ',') {
      Scope scope = tok.next().next().scope();
      auto process_constant = [&](const vector<Token> &toks) {
        create_info_decl += "COMPILATION_CONSTANT(";
        create_info_decl += (toks[3] == Number) ?
                                ((toks[3].str().back() == 'u') ? "uint" : "int") :
                                "bool";
        create_info_decl += ", " + toks[1].str();
        create_info_decl += ", " + toks[3].str();
        create_info_decl += ")\n";
      };
      scope.foreach_match(".A=A", process_constant);
      scope.foreach_match(".A=1", process_constant);
      tok = scope.back().next();
    }

    return create_info_decl;
  };

  auto process_graphic_pipeline = [&](Token pipeline_name, Scope params) {
    Token vertex_fn = params[1];
    Token fragment_fn = params[3];
    /* For now, just emit good old create info macros. */
    string create_info_decl;
    create_info_decl += "GPU_SHADER_CREATE_INFO(" + pipeline_name.str() + ")\n";
    create_info_decl += "GRAPHIC_SOURCE(\"" + filename + "\")\n";
    create_info_decl += "VERTEX_FUNCTION(\"" + vertex_fn.str() + "\")\n";
    create_info_decl += "FRAGMENT_FUNCTION(\"" + fragment_fn.str() + "\")\n";
    create_info_decl += "ADDITIONAL_INFO(" + vertex_fn.str() + "_infos_)\n";
    create_info_decl += "ADDITIONAL_INFO(" + fragment_fn.str() + "_infos_)\n";
    create_info_decl += process_compilation_constants(params[4]);
    create_info_decl += "DO_STATIC_COMPILATION()\n";
    create_info_decl += "GPU_SHADER_CREATE_END()\n";

    metadata_.create_infos_declarations.emplace_back(create_info_decl);
  };

  auto process_compute_pipeline = [&](Token pipeline_name, Scope params) {
    Token compute_fn = params[1];
    /* For now, just emit good old create info macros. */
    string create_info_decl;
    create_info_decl += "GPU_SHADER_CREATE_INFO(" + pipeline_name.str() + ")\n";
    create_info_decl += "COMPUTE_SOURCE(\"" + filename + "\")\n";
    create_info_decl += "COMPUTE_FUNCTION(\"" + compute_fn.str() + "\")\n";
    create_info_decl += "ADDITIONAL_INFO(" + compute_fn.str() + "_infos_)\n";
    create_info_decl += process_compilation_constants(params[2]);
    create_info_decl += "DO_STATIC_COMPILATION()\n";
    create_info_decl += "GPU_SHADER_CREATE_END()\n";

    metadata_.create_infos_declarations.emplace_back(create_info_decl);
  };

  parser().foreach_match("AA(A", [&](const vector<Token> &tokens) {
    Scope parameters = tokens[2].scope();
    if (tokens[0].str() == "PipelineGraphic") {
      process_graphic_pipeline(tokens[1], parameters);
      parser.erase(tokens.front(), parameters.back().next());
    }
    else if (tokens[0].str() == "PipelineCompute") {
      process_compute_pipeline(tokens[1], parameters);
      parser.erase(tokens.front(), parameters.back().next());
    }
  });
}

void SourceProcessor::lower_stage_function(Parser &parser)
{
  parser().foreach_function([&](bool is_static, Token fn_type, Token, Scope, bool, Scope fn_body) {
    Token attr_tok = (is_static) ? fn_type.prev().prev() : fn_type.prev();
    if (attr_tok.is_invalid() || attr_tok != ']' || attr_tok.prev() != ']') {
      return;
    }
    Scope attributes = attr_tok.prev().scope();
    if (attributes.type() != ScopeType::Attributes) {
      return;
    }

    parser.erase(attributes.scope());

    string condition;
    attributes.foreach_attribute([&](Token attr_tok, Scope) {
      const string attr = attr_tok.str();
      if (attr == "vertex") {
        condition += "GPU_VERTEX_SHADER";
      }
      else if (attr == "fragment") {
        condition += "GPU_FRAGMENT_SHADER";
      }
      else if (attr == "compute") {
        condition += "GPU_COMPUTE_SHADER";
      }
    });
    if (condition.empty()) {
      return;
    }
    condition = "defined(" + condition + ")";

    guarded_scope_mutation(parser, fn_body, condition);
  });
  parser.apply_mutations();
}

void SourceProcessor::guarded_scope_mutation(Parser &parser,
                                             Scope scope,
                                             const string &condition,
                                             Token fn_type)
{
  string line_start = "#line " + to_string(scope.front().next().line_number()) + "\n";

  string guard_start = "#if " + condition;
  string guard_else;
  if (fn_type.is_valid() && fn_type.str() != "void") {
    string type = fn_type.str();
    bool is_trivial = false;
    if (type == "float" || type == "float2" || type == "float3" || type == "float4" ||
        /**/
        type == "int" || type == "int2" || type == "int3" || type == "int4" ||
        /**/
        type == "uint" || type == "uint2" || type == "uint3" || type == "uint4" ||
        /**/
        type == "float2x2" || type == "float2x3" || type == "float2x4" ||
        /**/
        type == "float3x2" || type == "float3x3" || type == "float3x4" ||
        /**/
        type == "float4x2" || type == "float4x3" || type == "float4x4")
    {
      is_trivial = true;
    }
    guard_else += "#else\n";
    guard_else += line_start;
    guard_else += "  return " + type + (is_trivial ? "(0)" : "{}") + ";\n";
  }
  string guard_end = "#endif";

  parser.insert_directive(scope.front(), guard_start);
  parser.insert_directive(scope.back().prev(), guard_else + guard_end);
};

/* Lint host shared structure for padding and alignment.
 * Remove the [[host_shared]] attribute. */
void SourceProcessor::lower_host_shared_structures(Parser &parser)
{
  parser().foreach_struct([&](Token struct_keyword,
                              Scope attributes,
                              Token struct_name,
                              Scope body) {
    if (attributes.is_invalid()) {
      return;
    }
    parser.erase(attributes.scope());
    bool is_shared = false;
    attributes.foreach_attribute([&](Token attr, Scope) {
      if (attr.str() == "host_shared") {
        is_shared = true;
      }
    });
    if (!is_shared) {
      return;
    }

    Token comma = body.find_token(',');
    if (comma.is_valid() && comma.scope() == body) {
      report_error_(
          ERROR_TOK(comma),
          "comma declaration is not supported in shared struct, expand to multiple definition");
      return;
    }

    bool is_std140_compatible = true;
    bool has_vec3 = false;

    struct Type {
      size_t size;
      size_t alignment;
    };
    unordered_map<string, Type> sizeof_types = {
        {"float", {4, 4}},
        {"float2", {8, 8}},
        {"float4", {16, 16}},
        {"float2x4", {16 * 2, 16}},
        {"float3x4", {16 * 3, 16}},
        {"float4x4", {16 * 4, 16}},
        {"bool32_t", {4, 4}},
        {"int", {4, 4}},
        {"int2", {8, 8}},
        {"int4", {16, 16}},
        {"uint", {4, 4}},
        {"uint2", {8, 8}},
        {"uint4", {16, 16}},
        {"string_t", {4, 4}},
        {"packed_float3", {12, 16}},
        {"packed_int3", {12, 16}},
        {"packed_uint3", {12, 16}},
    };

    size_t offset = 0;
    body.foreach_declaration([&](Scope, Token, Token type, Scope, Token, Scope array, Token) {
      string type_str = type.str();

      if (type_str.find("char") != string::npos || type_str.find("short") != string::npos ||
          type_str.find("half") != string::npos)
      {
        report_error_(ERROR_TOK(type), "Small types are forbidden in shader interfaces.");
      }
      else if (type_str == "float3") {
        report_error_(ERROR_TOK(type), "use packed_float3 instead of float3 in shared structure");
      }
      else if (type_str == "uint3") {
        report_error_(ERROR_TOK(type), "use packed_uint3 instead of uint3 in shared structure");
      }
      else if (type_str == "int3") {
        report_error_(ERROR_TOK(type), "use packed_int3 instead of int3 in shared structure");
      }
      else if (type_str == "bool") {
        report_error_(ERROR_TOK(type), "bool is not allowed in shared structure, use bool32_t");
      }
      else if (type_str == "float4x3") {
        report_error_(ERROR_TOK(type), "float4x3 is not allowed in shared structure");
      }
      else if (type_str == "float3x3") {
        report_error_(ERROR_TOK(type), "float3x3 is not allowed in shared structure");
      }
      else if (type_str == "float2x3") {
        report_error_(ERROR_TOK(type), "float2x3 is not allowed in shared structure");
      }
      else if (type_str == "float4x2") {
        report_error_(ERROR_TOK(type), "float4x2 is not allowed in shared structure");
      }
      else if (type_str == "float3x2") {
        report_error_(ERROR_TOK(type), "float3x2 is not allowed in shared structure");
      }
      else if (type_str == "float2x2") {
        report_error_(ERROR_TOK(type), "float2x2 is not allowed in shared structure");
      }

      auto sz = sizeof_types.find(type_str);

      Type type_info{16, 16};
      if (sz != sizeof_types.end()) {
        type_info = sz->second;
      }
      else if (type.prev() == Enum) {
        /* Only 4 bytes enums are allowed. */
        type_info = {4, 4};
        parser.erase(type.prev());
        /* Make sure that linted structs only contain other linted structs. */
        /* TODO(fclem): Conflicts with default ctor. */
        // parser.replace(type, type.str() + linted_struct_suffix + " ");
      }
      else if (type.prev() == Struct) {
        /* Only 4 bytes enums are allowed. */
        type_info = {16, 16};
        /* Erase redundant struct keyword. */
        parser.erase(type.prev());
        /* Make sure that linted structs only contain other linted structs. */
        /* TODO(fclem): Conflicts with default ctor. */
        // parser.replace(type, type.str() + linted_struct_suffix + " ");
      }
      else {
        report_error_(ERROR_TOK(type),
                      "Unknown type, add 'enum' or 'struct' keyword before the type name");
        return;
      }

      if (type_info.size == 12) {
        has_vec3 = true;
      }

      size_t align = type_info.alignment - (offset % type_info.alignment);
      if (align != type_info.alignment) {
        string err = "Misaligned member, missing " + to_string(align) + " padding bytes";
        report_error_(ERROR_TOK(type), err.c_str());
      }

      size_t array_size = 1;
      if (array.is_valid()) {
        if (array_size > 1 && type_info.size < 16) {
          /* Arrays of non-vec4 are padded and should not be used inside std140. */
          is_std140_compatible = false;
        }

        /* For macro or expression assume value is multiple of 4. */
        array_size = static_array_size(array, 4);
      }

      offset += type_info.size * array_size;
    });

    /* Only check for std140 padding for bigger structs. Otherwise consider the struct to be for
     * storage buffers. Eventually we could add an attribute for that usage. */
    if (offset < 32) {
      is_std140_compatible = ((offset % 16) == 0);
    }
    else if (offset % 16 != 0) {
      string err = "Alignment issue, missing " + to_string(16 - (offset % 16)) + " padding bytes";
      report_error_(ERROR_TOK(struct_name), err.c_str());
    }
    /* Insert an alias to the type that will get referenced for shaders that enforce usage of
     * linted types. */
    string directive = "#define " + struct_name.str() + linted_struct_suffix + " " +
                       struct_name.str() + "\n";
    if (is_std140_compatible) {
      directive += "#define " + struct_name.str() + linted_struct_suffix + uniform_struct_suffix +
                   " " + struct_name.str() + "\n";
    }
    parser.insert_directive(struct_keyword.prev(), directive);
  });
  parser.apply_mutations();
}

void SourceProcessor::lint_unbraced_statements(Parser &parser)
{
  auto check_statement = [&](const Tokens &toks) {
    Token end_tok = toks.back();
    if (end_tok.next() == If || end_tok.prev() == '#') {
      return;
    }
    if (end_tok.next() == '[' && end_tok.next().next() == '[') {
      end_tok = end_tok.next().scope().back();
    }
    if (end_tok.next() != '{') {
      report_error_(ERROR_TOK(end_tok), "Missing curly braces after flow control statement.");
    }
  };

  parser().foreach_match("i(..)", check_statement);
  parser().foreach_match("I", check_statement);
  parser().foreach_match("f(..)", check_statement);
  parser().foreach_match("F(..)", check_statement);
}

void SourceProcessor::lint_reserved_tokens(Parser &parser)
{
  unordered_set<string> reserved_symbols = {
      "vec2",   "vec3",   "vec4",   "mat2x2", "mat2x3", "mat2x4", "mat3x2", "mat3x3",
      "mat3x4", "mat4x2", "mat4x3", "mat4x4", "mat2",   "mat3",   "mat4",   "ivec2",
      "ivec3",  "ivec4",  "uvec2",  "uvec3",  "uvec4",  "bvec2",  "bvec3",  "bvec4",
  };

  parser().foreach_token(Word, [&](Token tok) {
    if (reserved_symbols.find(tok.str()) != reserved_symbols.end()) {
      report_error_(ERROR_TOK(tok), "Reserved GLSL token");
    }
  });
}

void SourceProcessor::lower_noop_keywords(Parser &parser)
{
  /* inline has no equivalent in GLSL and is making parsing more complicated. */
  parser().foreach_token(Inline, [&](Token tok) { parser.erase(tok); });
  /* static have no meaning for the shading language when not inside a struct.
   * Removing to make parsing easier. */
  parser().foreach_token(Static, [&](Token tok) {
    ScopeType scope_type = tok.scope().type();
    if (scope_type != ScopeType::Struct && scope_type != ScopeType::Preprocessor) {
      parser.erase(tok);
    }
  });

  /* Erase `public:` and `private:` keywords. Access is checked by C++ compilation. */
  auto process_access = [&](Token tok) {
    if (tok.next() == ':') {
      parser.erase(tok, tok.next());
    }
    else {
      report_error_(ERROR_TOK(tok), "Expecting colon ':' after access specifier");
    }
  };
  parser().foreach_token(Private, process_access);
  parser().foreach_token(Public, process_access);
}

void SourceProcessor::lower_trailing_comma_in_list(Parser &parser)
{
  parser().foreach_match(",}", [&](const Tokens &t) { parser.erase(t[0]); });
}

/* Allow easier parsing of struct member declaration.
 * Example: `int a, b;` > `int a; int b;` */
void SourceProcessor::lower_comma_separated_declarations(Parser &parser)
{
  auto process_decl = [&](const Tokens &t) {
    if (t[0].scope().type() != ScopeType::Struct) {
      return;
    }
    string type = t[0].str();
    Token comma = t[2];
    while (comma == ',' || comma == '[') {
      if (comma == '[') {
        comma = comma.scope().back().next();
        continue;
      }
      parser.replace(comma, ";" + type, true);
      comma = comma.next().next();
    }
  };

  parser().foreach_match("AA,", [&](const Tokens &t) { process_decl(t); });
  parser().foreach_match("AA[..],", [&](const Tokens &t) { process_decl(t); });
}

void SourceProcessor::lower_implicit_return_types(Parser &parser)
{
  parser().foreach_function([&](bool, Token type, Token, Scope, bool, Scope fn_body) {
    fn_body.foreach_match("rA?{..};", [&](Tokens toks) {
      Scope list = toks[3].scope();
      if (list.front().next() == '.') {
        /* `return {1, 2};` > `T tmp = T{1, 2}; return tmp;`
         * This syntax allow to support designated initializer. */
        parser.insert_before(toks[0],
                             "{" + type.str() + " _tmp = " + type.str() + list.str() + "; ");
        parser.replace(list, "_tmp;}");
      }
      else if (toks[1].is_invalid()) {
        /* Regular initializer list. Keep it simple. */
        parser.insert_after(toks[0], type.str());
      }
    });
  });
}

void SourceProcessor::lower_initializer_implicit_types(Parser &parser)
{
  auto process_scope = [&](Scope s) {
    /* Auto insert equal. */
    s.foreach_match("AA{..}", [&](Tokens t) { parser.insert_before(t[2], " = " + t[0].str()); });
    /* Auto insert type. */
    s.foreach_match("AA={..}", [&](Tokens t) { parser.insert_before(t[3], t[0].str()); });
  };

  parser().foreach_scope(ScopeType::FunctionArg, process_scope);
  parser().foreach_scope(ScopeType::Function, process_scope);
  parser.apply_mutations();
}

void SourceProcessor::lower_designated_initializers(Parser &parser)
{
  /* Transform to compatibility macro. */
  parser().foreach_match("A{.A=", [&](Tokens t) {
    if (t[0].prev() != '=' || t[0].prev().prev() != Word) {
      report_error_(ERROR_TOK(t[0]), "Designated initializers are only supported in assignments");
      return;
    }
    /* Lint for nested aggregates. */
    Token nested_aggregate_end = t[0].scope().find_token(BracketClose);
    if (nested_aggregate_end != t[3]) {
      Token nested_aggregate_start = nested_aggregate_end.scope().front();
      if (nested_aggregate_start.prev() != Word) {
        report_error_(ERROR_TOK(nested_aggregate_start),
                      "Nested anonymous aggregate is not supported");
        return;
      }
    }
    Token assign_tok = t[0].prev();
    Token var = t[0].prev().prev();
    Scope aggregate = t[2].scope();

    parser.insert_before(assign_tok, ";");
    parser.erase(assign_tok, t[1]);
    aggregate.foreach_match(".A=", [&](Tokens t) {
      if (t[0].scope() != aggregate) {
        report_error_(ERROR_TOK(t[0]), "Nested initializer lists are not supported");
        return;
      }
      parser.insert_before(t[0], var.str());
      Token value_end = t[2].scope().back();
      parser.insert_after(value_end, ";");
      if (value_end.next() == ',') {
        parser.erase(value_end.next());
      }
    });
    parser.erase(aggregate.back(), aggregate.back().next());

    /* TODO: Lint for vector/matrix type (unsafe aggregate). */
  });

  parser.apply_mutations();
}

/* Support for **full** aggregate initialization.
 * They are converted to default constructor for GLSL. */
void SourceProcessor::lower_aggregate_initializers(Parser &parser)
{
  unordered_set<string> builtin_types = {
      "float2",   "float3",   "float4",   "float2x2", "float2x3", "float2x4",
      "float3x2", "float3x3", "float3x4", "float4x2", "float4x3", "float4x4",
      "float2x2", "float3x3", "float4x4", "int2",     "int3",     "int4",
      "uint2",    "uint3",    "uint4",    "bool2",    "bool3",    "bool4",
  };

  do {
    /* Transform to compatibility macro. */
    parser().foreach_match("A{..}", [&](Tokens t) {
      if (t[0].prev() == Struct) {
        return;
      }
      if (builtin_types.find(t[0].str()) != builtin_types.end()) {
        report_error_(ERROR_TOK(t[0]),
                      "Aggregate is error prone for built-in vector and matrix types, use "
                      "constructors instead");
      }
      if (t[1].scope().token_count() == 2) {
        /* Call generated default ctor. */
        parser.insert_after(t.front(), "_ctor_");
        parser.replace(t[1], t[4], "()");
        return;
      }
      /* Lint for nested aggregates. */
      Token nested_aggregate_end = t[1].scope().find_token(BracketClose);
      if (nested_aggregate_end != t[4]) {
        Token nested_aggregate_start = nested_aggregate_end.scope().front();
        if (nested_aggregate_start.prev() != Word) {
          report_error_(ERROR_TOK(nested_aggregate_start),
                        "Nested anonymous aggregate is not supported");
        }
      }
      parser.insert_before(t[0], "_ctor(");
      parser.insert_before(t[1], ")");
      parser.erase(t[1]);
      if (t[4].prev() == ',') {
        parser.erase(t[4].prev());
      }
      parser.insert_before(t[4], " _rotc()");
      parser.erase(t[4]);

      /* TODO: Lint for vector/matrix type (unsafe aggregate). */
    });
  } while (parser.apply_mutations());
}

/* Auto detect array length, and lower to GLSL compatible syntax.
 * TODO(fclem): GLSL 4.3 already supports initializer list. So port the old GLSL syntax to
 * initializer list instead. */
void SourceProcessor::lower_array_initializations(Parser &parser)
{
  parser().foreach_match("AA[..]={..};", [&](vector<Token> toks) {
    const Token type_tok = toks[0];
    const Token name_tok = toks[1];
    const Scope array_scope = toks[2].scope();
    const Scope list_scope = toks[7].scope();

    /* Auto array size. */
    int array_scope_tok_len = array_scope.token_count();
    if (array_scope_tok_len == 2) {
      int comma_count = 0;
      list_scope.foreach_token(Comma, [&](Token t) {
        if (t.scope() == list_scope) {
          comma_count++;
        }
      });
      const int list_len = (comma_count > 0) ? comma_count + 1 : 0;
      if (list_len == 0) {
        report_error_(ERROR_TOK(name_tok), "Array size must be greater than zero.");
      }
      parser.insert_after(array_scope[0], to_string(list_len));
    }
    else if (array_scope_tok_len == 3 && array_scope[1] == Number) {
      if (stol(array_scope[1].str()) == 0) {
        report_error_(ERROR_TOK(name_tok), "Array size must be greater than zero.");
      }
    }

    /* Lint nested initializer list. */
    list_scope.foreach_token(BracketOpen, [&](Token tok) {
      if (tok != list_scope.front()) {
        report_error_(ERROR_TOK(name_tok), "Nested initializer list is not supported.");
      }
    });

    /* Mutation to compatible syntax. */
    parser.insert_before(list_scope.front(), "ARRAY_T(" + type_tok.str() + ") ARRAY_V(");
    parser.insert_after(list_scope.back(), ")");
    parser.erase(list_scope.front());
    parser.erase(list_scope.back());
    if (list_scope.back().prev() == ',') {
      parser.erase(list_scope.back().prev());
    }
  });
  parser.apply_mutations();
}

string SourceProcessor::strip_whitespace(const string &str)
{
  return str.substr(0, str.find_last_not_of(" \n") + 1);
}

/**
 * Expand functions with default arguments to function overloads.
 * Expects formatted input and that function bodies are followed by newline.
 */
void SourceProcessor::lower_function_default_arguments(Parser &parser)
{
  parser().foreach_function(
      [&](bool, Token fn_type, Token fn_name, Scope fn_args, bool fn_const, Scope fn_body) {
        if (!fn_args.contains_token('=')) {
          return;
        }

        const bool has_non_void_return_type = fn_type.str() != "void";

        string args_decl;
        string args_names;

        vector<string> fn_overloads;

        fn_args.foreach_scope(ScopeType::FunctionArg, [&](Scope arg) {
          Token equal = arg.find_token('=');
          const char *comma = (args_decl.empty() ? "" : ", ");
          if (equal.is_invalid()) {
            args_decl += comma + arg.str_with_whitespace();
            args_names += comma + arg.back().str();
          }
          else {
            string arg_name = equal.prev().str();
            string value = parser.substr_range_inclusive(equal.next(), arg.back());
            string decl = parser.substr_range_inclusive(arg.front(), equal.prev());

            string fn_call = fn_name.str() + '(' + args_names + comma + value + ");";
            if (has_non_void_return_type) {
              fn_call = "return " + fn_call;
            }
            string overload;
            overload += fn_type.str() + " ";
            overload += fn_name.str() + '(' + args_decl + ")" + string(fn_const ? " const" : "") +
                        "\n";
            overload += "{\n";
            overload += "#line " + to_string(fn_type.line_number()) + "\n";
            overload += "  " + fn_call + "\n}\n";
            fn_overloads.emplace_back(overload);

            args_decl += comma + strip_whitespace(decl);
            args_names += comma + arg_name;
            /* Erase the value assignment and keep the declaration. */
            parser.erase(equal.scope());
          }
        });
        size_t end_of_fn_char = fn_body.back().line_end() + 1;
        /* Have to reverse the declaration order. */
        for (auto it = fn_overloads.rbegin(); it != fn_overloads.rend(); ++it) {
          parser.insert_line_number(end_of_fn_char, fn_type.line_number());
          parser.insert_after(end_of_fn_char, *it);
        }
        parser.insert_line_number(end_of_fn_char, fn_body.back().line_number() + 1);
      });

  parser.apply_mutations();
}

/* Successive mutations can introduce a lot of unneeded line directives. */
void SourceProcessor::cleanup_line_directives(Parser &parser)
{
  parser().foreach_match("#A1\n", [&](vector<Token> toks) {
    if (toks[1].str() != "line") {
      return;
    }
    /* Workaround the foreach_match not matching overlapping patterns. */
    if (toks.back().next() == '#' && toks.back().next().next() == Word &&
        toks.back().next().next().next() == Number &&
        toks.back().next().next().next().next() == '\n')
    {
      parser.replace(toks[0].line_start(), toks[0].line_end() + 1, "");
    }
  });
  parser.apply_mutations();

  parser().foreach_match("#A1\n#A\n", [&](vector<Token> toks) {
    if (toks[1].str() != "line") {
      return;
    }
    /* Workaround the foreach_match not matching overlapping patterns. */
    if (toks.back().next() == '#' && toks.back().next().next() == Word &&
        toks.back().next().next().next() == Number &&
        toks.back().next().next().next().next() == '\n')
    {
      parser.replace(toks[0].line_start(), toks[0].line_end() + 1, "");
    }
  });
  parser.apply_mutations();

  parser().foreach_match("#A1\n", [&](vector<Token> toks) {
    if (toks[1].str() != "line") {
      return;
    }
    /* True if directive is noop. */
    if (toks[0].line_number() == stol(toks[2].str())) {
      parser.replace(toks[0].line_start(), toks[0].line_end() + 1, "");
    }
  });
  parser.apply_mutations();
}

/* Successive mutations can introduce a lot of unneeded blank lines. */
void SourceProcessor::cleanup_empty_lines(Parser &parser)
{
  const string &str = parser.str();

  {
    size_t sequence_start = 0;
    size_t sequence_end = -1;
    while ((sequence_start = str.find("\n\n\n", sequence_end + 1)) != string::npos) {
      sequence_end = str.find_first_not_of("\n", sequence_start);
      if (sequence_end == string::npos) {
        break;
      }
      size_t line = line_number(str, sequence_end);
      parser.replace(sequence_start + 2, sequence_end - 1, "#line " + to_string(line) + "\n");
    }
    parser.apply_mutations();
  }
  {
    size_t sequence_start = 0;
    size_t sequence_end = -1;
    while ((sequence_end = str.find("\n\n#line ", sequence_end + 1)) != string::npos) {
      sequence_start = str.find_last_not_of("\n", sequence_end) + 1;
      if (sequence_start == string::npos) {
        continue;
      }
      parser.replace(sequence_start, sequence_end, "");
    }
    parser.apply_mutations();
  }
}

/* Used to make GLSL matrix constructor compatible with MSL in pyGPU shaders.
 * This syntax is not supported in blender's own shaders. */
string SourceProcessor::matrix_constructor_mutation(const string &str)
{
  if (str.find("mat") == string::npos) {
    return str;
  }

  IntermediateForm<ExpressionLexer, DummyParser> parser(str, report_error_);
  parser().foreach_token(ParOpen, [&](const Token t) {
    if (t.prev() == Word) {
      Token fn_name = t.prev();
      string_view fn_name_str = fn_name.str_view();
      if (fn_name_str.size() == 4) {
        /* Example: `mat2(x)` > `__mat2x2(x)` */
        if (fn_name_str == "mat2") {
          parser.replace(fn_name, "__mat2x2", true);
        }
        else if (fn_name_str == "mat3") {
          parser.replace(fn_name, "__mat3x3", true);
        }
        else if (fn_name_str == "mat4") {
          parser.replace(fn_name, "__mat4x4", true);
        }
      }
      else if (fn_name_str.size() == 6) {
        if (fn_name_str == "mat2x2" || fn_name_str == "mat3x3" || fn_name_str == "mat4x4") {
          /* Only process square matrices since this is the only types we overload the
           * constructors. */
          /* Example: `mat2x2(x)` > `__mat2x2(x)` */
          parser.insert_before(fn_name, "__");
        }
      }
    }
  });
  return parser.result_get();
}

/* To be run before `argument_decorator_macro_injection()`. */
void SourceProcessor::lower_reference_arguments(Parser &parser)
{
  auto add_mutation = [&](Token type, Token arg_name, Token last_tok) {
    if (type.prev() == Const) {
      parser.replace(type.prev(), last_tok, type.str() + " " + arg_name.str());
    }
    else {
      parser.replace(type, last_tok, "inout " + type.str() + " " + arg_name.str());
    }
  };

  parser().foreach_scope(ScopeType::FunctionArgs, [&](const Scope scope) {
    scope.foreach_match(
        "A(&A)", [&](const vector<Token> toks) { add_mutation(toks[0], toks[3], toks[4]); });
    scope.foreach_match(
        "A&A", [&](const vector<Token> toks) { add_mutation(toks[0], toks[2], toks[2]); });
    scope.foreach_match(
        "A&T", [&](const vector<Token> toks) { add_mutation(toks[0], toks[2], toks[2]); });
  });
  parser.apply_mutations();
}

/* To be run after `lower_reference_arguments()`. */
void SourceProcessor::lower_reference_variables(Parser &parser)
{
  parser().foreach_function([&](bool, Token, Token, Scope fn_args, bool, Scope fn_scope) {
    fn_scope.foreach_match("c?A&A=", [&](const vector<Token> &tokens) {
      const Token name = tokens[4];
      const Scope assignment = tokens[5].scope();

      Token decl_start = tokens[0].is_valid() ? tokens[0] : tokens[2];
      /* Take attribute into account. */
      decl_start = (decl_start.prev() == ']') ? decl_start.prev().scope().front() : decl_start;
      /* Take ending ; into account. */
      const Token decl_end = assignment.back().next();

      /* Assert definition doesn't contain any side effect. */
      assignment.foreach_token(Increment, [&](const Token token) {
        report_error_(ERROR_TOK(token), "Reference definitions cannot have side effects.");
      });
      assignment.foreach_token(Decrement, [&](const Token token) {
        report_error_(ERROR_TOK(token), "Reference definitions cannot have side effects.");
      });
      assignment.foreach_token(ParOpen, [&](const Token token) {
        string fn_name = token.prev().str();
        if ((fn_name != "specialization_constant_get") && (fn_name != "push_constant_get") &&
            (fn_name != "interface_get") && (fn_name != "attribute_get") &&
            (fn_name != "buffer_get") && (fn_name != "srt_access") && (fn_name != "sampler_get") &&
            (fn_name != "image_get"))
        {
          report_error_(ERROR_TOK(token), "Reference definitions cannot contain function calls.");
        }
      });
      assignment.foreach_scope(ScopeType::Subscript, [&](const Scope subscript) {
        if (subscript.token_count() != 3) {
          report_error_(
              ERROR_TOK(subscript.front()),
              "Array subscript inside reference declaration must be a single variable or "
              "a constant, not an expression.");
          return;
        }

        const Token index_var = subscript[1];

        if (index_var == Number) {
          /* Literals are fine. */
          return;
        }

        /* Search if index variable definition qualifies it as `const`. */
        bool is_const = false;
        bool is_ref = false;
        bool is_found = false;

        auto process_decl = [&](const vector<Token> &tokens) {
          if (tokens[5].str_index_start() < index_var.str_index_start() &&
              tokens[5].str() == index_var.str())
          {
            is_const = tokens[0].is_valid();
            is_ref = tokens[3].is_valid();
            is_found = true;
          }
        };
        fn_args.foreach_match("c?A&?A", [&](const vector<Token> &toks) { process_decl(toks); });
        fn_scope.foreach_match("c?A&?A", [&](const vector<Token> &toks) { process_decl(toks); });

        if (!is_found) {
          report_error_(ERROR_TOK(index_var),
                        "Cannot locate array subscript variable declaration. "
                        "If it is a global variable, assign it to a temporary const variable for "
                        "indexing inside the reference.");
          return;
        }
        if (!is_const) {
          report_error_(ERROR_TOK(index_var),
                        "Array subscript variable must be declared as const qualified.");
          return;
        }
        if (is_ref) {
          report_error_(ERROR_TOK(index_var),
                        "Array subscript variable must not be declared as reference.");
          return;
        }
      });

      string definition = parser.substr_range_inclusive(assignment[1], assignment.back());

      /* Replace declaration. */
      parser.erase(decl_start, decl_end);
      /* Replace all occurrences with definition. */
      name.scope().foreach_token(Word, [&](const Token token) {
        /* Do not match member access or function calls. */
        if (token.prev() == '.' || token.next() == '(') {
          return;
        }
        if (token.str_index_start() > decl_end.str_index_last() && token.str() == name.str()) {
          parser.replace(token, definition);
        }
      });
    });
  });
  parser.apply_mutations();

  parser().foreach_match("c?A&A=", [&](const vector<Token> &tokens) {
    report_error_(ERROR_TOK(tokens[4]),
                  "Reference is defined inside a global or unterminated scope.");
  });
}

void SourceProcessor::lower_argument_qualifiers(Parser &parser)
{
  parser().foreach_match("AAA", [&](const Tokens &toks) {
    if (toks[0].scope().type() == ScopeType::Preprocessor) {
      /* Don't mutate the actual implementation. */
      return;
    }
    if (toks[0].str() == "inout" || toks[0].str() == "out") {
      parser.replace(toks[0], "_ref(");
      parser.insert_after(toks[1], ",");
      parser.insert_after(toks[2], ")");
    }
  });
  parser.apply_mutations();
}

string SourceProcessor::argument_decorator_macro_injection(const string &str)
{
  IntermediateForm<ExpressionLexer, DummyParser> parser(str, report_error_);
  /* Example: `out float foo` > `out float _out_sta foo _out_end` */
  parser().foreach_match("AAA", [&](const Tokens &t) {
    string_view qualifier = t[0].str_view();
    if (qualifier == "out" || qualifier == "inout" || qualifier == "in" || qualifier == "shared") {
      parser.insert_after(t[1], " _" + string(qualifier) + "_sta ");
      parser.insert_after(t[2], " _" + string(qualifier) + "_end ");
    }
  });
  return parser.result_get();
}

string SourceProcessor::array_constructor_macro_injection(const string &str)
{
  IntermediateForm<ExpressionLexer, DummyParser> parser(str, report_error_);
  parser().foreach_match("=A[", [&](const Tokens toks) {
    Token array_len_start = toks.back();
    Token array_len_end = array_len_start.find_next(SquareClose);
    if (array_len_end.is_valid()) {
      Token type = toks[1];
      Token array_start = array_len_end.next();
      if (array_start == '(') {
        parser.insert_before(type, " ARRAY_T(");
        parser.replace(array_len_start, array_len_end, ") ");
        parser.insert_before(array_start, "ARRAY_V");
      }
    }
  });
  return parser.result_get();
}

/* Assume formatted source with our code style. Cannot be applied to python shaders. */
void SourceProcessor::lint_global_scope_constants(Parser &parser)
{
  /* Example: `const uint global_var = 1u;`. */
  parser().foreach_match("cAA=", [&](const vector<Token> &tokens) {
    if (tokens[0].scope().type() == ScopeType::Global) {
      report_error_(
          ERROR_TOK(tokens[2]),
          "Global scope constant expression found. These get allocated per-thread in MSL. "
          "Use Macro's or uniforms instead.");
    }
  });
}

int SourceProcessor::static_array_size(const Scope &array, int fallback_value)
{
  if (array.token_count() == 3 && array[1] == Number) {
    try {
      return stol(array[1].str());
    }
    catch (invalid_argument const & /*ex*/) {
      report_error_(ERROR_TOK(array.front()), "Invalid array size, expecting integer literal");
    }
  }
  return fallback_value;
}

string SourceProcessor::line_directive_prefix(const string &filename)
{
  /* NOTE: This is not supported by GLSL. All line directives are muted at runtime and the
   * sources are scanned after error reporting for the locating the muted line. */
  return "#line 1 \"" + filename + "\"\n";
}

}  // namespace blender::gpu::shader
