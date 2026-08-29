/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "intermediate.hh"
#include "metadata.hh"
#include "processor.hh"

#include "bsl/symbol_table.hh"

namespace blender::gpu::shader {
using namespace std;
using namespace shader::parser;
using namespace metadata;
using namespace shader::parser::ast;

SourceProcessor::Result SourceProcessor::convert_glsl()
{
  metadata_ = {};

  string str = this->source_;

  str = remove_comments(str);

  IntermediateForm<SimpleLexer, DummyParser> parser(error_handler);
  try {
    parser.language = Language::GLSL;
    parser.set_str(str);
    /* Remove trailing white space as they make the subsequent transformation much slower. */
    cleanup_whitespace(parser);
    str = parser.result_get();
    str = threadgroup_variables_parse_and_remove(str);
  }
  catch (ParserException & /*e*/) {
    /* Output the current source state for inspection. */
    return {parser.result_get(), metadata_, error_handler.err};
  }

  parse_builtins(str, filename, true);
#ifdef __APPLE__ /* Limiting to Apple hardware since GLSL compilers might have issues. */
  str = matrix_constructor_mutation(str);
#endif
  str = argument_decorator_macro_injection(str);
  str = array_constructor_macro_injection(str);
  str = line_directive_prefix(filename) + str;
  return {str, metadata_, error_handler.err};
}

SourceProcessor::Result SourceProcessor::convert_msl()
{
  metadata_ = {};

  string str = this->source_;

  str = remove_comments(str);

  {
    IntermediateForm<SimpleLexer, DummyParser> parser(str, error_handler);
    /* Remove trailing white space as they make the subsequent regex much slower. */
    cleanup_whitespace(parser);
    str = parser.result_get();
  }

  str = threadgroup_variables_parse_and_remove(str);

  Parser parser(error_handler);
  try {
    parser.language = Language::MSL;
    parser.set_str(str);
    parse_pragma_runtime_generated(parser);
    parse_includes(parser);
    lower_preprocessor(parser);
    str = parser.result_get();
  }
  catch (ParserException & /*e*/) {
    /* Output the current source state for inspection. */
    return {parser.result_get(), metadata_, error_handler.err};
  }

  str = argument_decorator_macro_injection(str);
  str = array_constructor_macro_injection(str);
  str = line_directive_prefix(filename) + str;
  return {str, metadata_, error_handler.err};
}

SourceProcessor::Result SourceProcessor::convert_bsl_legacy(
    metadata::Source external_sources_symbols)
{
  metadata_ = {};

  /* Only use symbols and templates from external sources. */
  metadata_.symbol_table.insert(metadata_.symbol_table.end(),
                                external_sources_symbols.symbol_table.begin(),
                                external_sources_symbols.symbol_table.end());
  metadata_.template_definitions.insert(metadata_.template_definitions.end(),
                                        external_sources_symbols.template_definitions.begin(),
                                        external_sources_symbols.template_definitions.end());

  /* Set line number for each symbol to 0 as they are defined outside of the target file. */
  for (auto &symbol : metadata_.symbol_table) {
    symbol.definition_line = 0;
  }

  string str = remove_comments(this->source_);

  Parser parser(error_handler);
  try {
    parser.set_str(str);

    disabled_code_mutation(parser);
    /* Legacy GLSL compat.  */
    threadgroup_variables_parse_and_remove(parser);
    parse_builtins(parser, filename);
    /* Preprocessor directive parsing & linting. */
    lint_pragma_once(parser, filename);
    parse_pragma_runtime_generated(parser);
    parse_includes(parser);
    parse_defines(parser);
    parse_library_functions(parser);

    lower_preprocessor(parser);

    parser.apply_mutations();

    /* Early out for certain files. */
    if (parser.str().find("\n#pragma no_processing") != string::npos) {
      cleanup_whitespace(parser);
      return {line_directive_prefix(filename) + parser.result_get(), metadata_, error_handler.err};
    }

    /* Lower high level parsing complexity.
     * Merge tokens that can be combined together,
     * remove the token that are unsupported or that are noop.
     * All these steps should be independent. */
    lower_namesless_parameters(parser);
    lower_attribute_sequences(parser);
    lower_strings_sequences(parser);
    lower_swizzle_methods(parser);
    lower_binary_literals(parser);
    lower_classes(parser);
    lower_noop_keywords(parser);
    lower_trailing_comma_in_list(parser);
    lower_comma_separated_declarations(parser);
    lower_assert(parser, filename);
    /* Lower implicit members before we remove SRT member from their struct. */
    lower_implicit_member(parser);

    parser.apply_mutations();

    parse_local_symbols(parser);

    /* Linting phase. Detect valid syntax with invalid usage. */
    lint_unbraced_statements(parser);
    lint_reserved_tokens(parser);
    lint_attributes(parser);
    lint_global_scope_constants(parser);
    lint_constructors(parser);
    lint_forward_declared_structs(parser);

    /* All mutations that needs to also be applied on template definitions. */
    lower_pre_template(parser);
    /* Lower templates. */
    lower_templates(parser);
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
    lower_method_definitions(parser);
    lower_method_calls(parser);
    lower_empty_struct(parser);
    /* Lower SRT accesses. */
    lower_srt_member_access(parser);
    lower_srt_arguments(parser);
    lower_entry_points_signature(parser);
    lower_stage_function(parser);
    /* Lower string, assert, printf. */
    lower_strings(parser);
    lower_printf(parser);
    /* Lower other C++ constructs. */
    lower_implicit_return_types(parser);
    lower_initializer_implicit_types(parser);
    lower_designated_initializers(parser);
    lower_aggregate_initializers(parser);
    lower_array_initializations(parser);
    lower_scope_resolution_operators(parser);
    lower_structured_bindings(parser);
    lower_tests(parser);
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
    lower_gather_component(parser);

    /* Cleanup to make output more human readable and smaller for runtime. */
    cleanup_whitespace(parser);
    cleanup_empty_lines(parser);
    cleanup_line_directives(parser);

    str = parser.result_get();
  }
  catch (ParserException & /*e*/) {
    /* Output the current source state for inspection. */
    return {parser.result_get(), metadata_, error_handler.err};
  }

  str = line_directive_prefix(filename) + str;
  return {str, metadata_, error_handler.err};
}

SourceProcessor::Result SourceProcessor::convert_bsl()
{
  metadata_ = {};

  string str = remove_comments(this->source_);
  /* Add source file line directive first so that error lines are correct. */
  str = line_directive_prefix(filename) + str;
  /* Define `BSL_530` macro for this file only. Needed for compatibility with old BSL version? */
  str = "#define BSL_530\n" + str + "\n#undef BSL_530\n";

  SourceManager sources;

  /* Init builtin parser to add builtin symbols. */
  Parser &builtin_parser = sources.new_source(error_handler);
  builtin_parser.language = Language::BSL;
  builtin_parser.set_str("#line 1 \"builtin\"\n#error This should not be emitted\n");
  builtin_parser.include_id = sources.include_id_get();
  /* Init symbol table and add builtin symbols. */
  bsl::SymbolTable symbols(builtin_parser);

  Parser &parser = sources.new_source(error_handler);
  try {
    /* Allow CPP grammar until we remove #ifndef GPU_SHADER blocks. */
    parser.language = Language::CPP;
    parser.set_str(str);

    disabled_code_mutation(parser, true);
    /* Preprocessor directive parsing & linting. */
    lint_pragma_once(parser, filename);
    parse_pragma_runtime_generated(parser);
    parse_includes(parser);
    parse_defines(parser);
    lower_tests(parser, "srt.");

    parser.only_apply_mutations();

    vector<string> visited_files;
    scan_external_symbols(sources, symbols, visited_files);

    parser.include_id = sources.include_id_get();

    parser.language = Language::BSL;
    parser.parse(error_handler);

    parse_library_functions_ast(parser);
    lower_preprocessor_ast(parser);

    /* Lower high level parsing complexity.
     * Merge tokens that can be combined together,
     * remove the token that are unsupported or that are noop.
     * All these steps should be independent. */
    lower_namesless_parameters_ast(parser);
    lower_attribute_sequences_ast(parser);
    lower_strings_sequences(parser);
    lower_swizzle_methods_ast(parser);
    lower_binary_literals(parser);
    lower_noop_keywords_ast(parser);
    lower_trailing_comma_in_list_ast(parser);
    lower_assert_ast(parser, filename);
    lower_this_keyword(parser);

    parser.apply_mutations();

    /* Linting phase. Detect valid syntax with invalid usage. */
    lint_reserved_tokens(parser);
    lint_attributes_ast(parser);

    lower_srt_accessor_templates_ast(parser);   /* Legacy. To remove. */
    lower_union_accessor_templates_ast(parser); /* Legacy. To remove. */

    symbols.parse(parser.root(), error_handler);

    lower_bsl_to_il(parser, symbols);
    /* Lower SRT and Interfaces. */
    lower_pipeline_definition(parser, filename);
    /* Lower class methods. */
    lower_method_forward_declaration(parser);
    lower_union_setters(parser);
    lower_method_calls(parser, false);
    /* Lower string, assert, printf. */
    lower_strings(parser);
    lower_printf(parser);
    /* Needs to be last. */
    lower_resource_macro_placeholder_ast(parser);
    lower_constructors(parser);

    parser.language = Language::IL;
    parser.apply_mutations();

    /* GLSL syntax compatibility. */
    lower_reference_arguments(parser);
    lower_argument_qualifiers(parser);
    lower_gather_component(parser);

    /* Cleanup to make output more human readable and smaller for runtime. */
    cleanup_whitespace(parser, true);
    cleanup_empty_lines(parser);
    cleanup_line_directives(parser);

    str = parser.result_get();
  }
  catch (ParserException & /*e*/) {
    /* Output the current source state for inspection. */
    return {parser.result_get(), metadata_, error_handler.err};
  }
  return {str, metadata_, error_handler.err};
}

SourceProcessor::Result SourceProcessor::convert_info()
{
  metadata_ = {};

  string str = remove_comments(this->source_);

  Parser parser(error_handler);
  try {
    parser.set_str(str);

    disabled_code_mutation(parser);
    /* Legacy GLSL compat.  */
    threadgroup_variables_parse_and_remove(parser);
    parse_builtins(parser, filename);
    /* Preprocessor directive parsing & linting. */
    lint_pragma_once(parser, filename);
    parse_pragma_runtime_generated(parser);
    parse_includes(parser);
    parse_defines(parser);
    parse_legacy_create_info(parser);

    lower_preprocessor(parser);

    /* Cleanup to make output more human readable and smaller for runtime. */
    cleanup_whitespace(parser);
    cleanup_empty_lines(parser);
    cleanup_line_directives(parser);

    str = parser.result_get();
  }
  catch (ParserException & /*e*/) {
    /* Output the current source state for inspection. */
    return {parser.result_get(), metadata_, error_handler.err};
  }

  return {str, metadata_, error_handler.err};
}

SourceProcessor::Result SourceProcessor::convert(metadata::Source external_sources_symbols)
{
  switch (language_) {
    case Language::INFO:
      return convert_info();
    case Language::CPP:
      /* Should become BSL, but until the new compiler is fully working, fallback
       * to the legacy path. */
      return convert_bsl_legacy(external_sources_symbols);
    case Language::BSL:
      return convert_bsl(); /* WIP */
    case Language::BLENDER_GLSL:
      return convert_bsl_legacy(external_sources_symbols);
    case Language::MSL:
      return convert_msl();
    case Language::GLSL:
      return convert_glsl();
    case Language::UNKNOWN:
    default:
      break;
  }

  metadata_ = {};
  report_error(0, 0, "", "Unknown file type");
  return {"", metadata_, error_handler.err};
}

metadata::Source SourceProcessor::parse_include_and_symbols()
{
  metadata_ = {};

  string str = remove_comments(this->source_);

  Parser parser(error_handler);
  try {
    parser.set_str(str);
    disabled_code_mutation(parser);
    parse_pragma_runtime_generated(parser);
    parse_includes(parser);

    parser.apply_mutations();

    lower_preprocessor(parser);

    parser.apply_mutations();

    /* Lower high level parsing complexity.
     * Merge tokens that can be combined together,
     * remove the token that are unsupported or that are noop.
     * All these steps should be independent. */
    lower_namesless_parameters(parser);
    lower_attribute_sequences(parser);
    lower_strings_sequences(parser);
    lower_swizzle_methods(parser);
    lower_classes(parser);
    lower_noop_keywords(parser);
    lower_trailing_comma_in_list(parser);
    lower_comma_separated_declarations(parser);
    lower_assert(parser, filename);
    /* Lower implicit members before we remove SRT member from their struct. */
    lower_implicit_member(parser);

    parser.apply_mutations();

    parse_local_symbols(parser);
  }
  catch (ParserException & /*e*/) {
    /* Expect that the parsing will generate error when the file itself is compiled. */
    return {};
  }

  return metadata_;
}

void SourceProcessor::scan_external_symbols(SourceManager &sources,
                                            bsl::SymbolTable &symbols,
                                            vector<string> &visited_files)
{
  for (const auto &dep : metadata_.dependencies) {
    string file;
    for (const auto &filename : file_list_) {
      if (filename.find(dep) != string::npos) {
        file = filename;
      }
    }

    if (file.empty()) {
      report_error(0, 0, "", "Error: Included file not found " + dep);
      throw ParserException();
    }

    if (ranges::find(visited_files, file) == visited_files.end()) {
      visited_files.emplace_back(file);

      ifstream input_file(file);
      if (!input_file) {
        report_error(0, 0, "", "Error: Could not open file " + file);
        throw ParserException();
      }

      stringstream buffer;
      buffer << input_file.rdbuf();

      Language language = language_from_filename(file);
      SourceProcessor processor(buffer.str(), file, language, file_list_);
      /* Recursive. */
      processor.parse_include_and_symbols(sources, symbols, visited_files);

      /* If an error occur, cancel everything and let the error bubble up. */
      if (processor.error_handler.err) {
        this->error_handler.err = processor.error_handler.err;
        throw ParserException();
      }
    }
  }
}

metadata::Source SourceProcessor::parse_include_and_symbols(SourceManager &sources,
                                                            bsl::SymbolTable &symbols,
                                                            vector<string> &visited_files)
{
  string str = remove_comments(this->source_);
  /* Add source file line directive first so that error lines are correct. */
  str = line_directive_prefix(filename) + str;

  Parser &parser = sources.new_source(error_handler);
  try {
    parser.set_str(str);
    disabled_code_mutation(parser, true);
    parse_pragma_runtime_generated(parser);
    parse_includes(parser);

    if (language_ == Language::INFO) {
      return metadata_;
    }

    if (language_ == Language::GLSL || language_ == Language::BLENDER_GLSL) {
      parser().foreach_match("A(A)", [&](Tokens toks) {
        string_view fn_name = toks[0].str();
        if (fn_name == "SHADER_LIBRARY_CREATE_INFO" || fn_name == "VERTEX_SHADER_CREATE_INFO" ||
            fn_name == "FRAGMENT_SHADER_CREATE_INFO" || fn_name == "COMPUTE_SHADER_CREATE_INFO")
        {
          parser.erase(toks.front(), toks.back().next() == ';' ? toks.back().next() : toks.back());
        }
      });
    }

    parser.apply_mutations();

    lower_preprocessor(parser);

    parser.language = Language::BSL;
    parser.only_apply_mutations();
    parser.parse(error_handler);

    lower_namesless_parameters_ast(parser);
    lower_attribute_sequences_ast(parser);

    parser.apply_mutations();

    scan_external_symbols(sources, symbols, visited_files);

    parser.include_id = sources.include_id_get();

    lower_srt_accessor_templates_ast(parser);   /* Legacy. To remove. */
    lower_union_accessor_templates_ast(parser); /* Legacy. To remove. */

    symbols.parse(parser.root(), error_handler);
  }
  catch (ParserException & /*e*/) {
    /* Expect that the parsing will generate error when the file itself is compiled. */
    return {};
  }

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
      report_error(line_number(out_str, start),
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

void SourceProcessor::remove_comments(Parser &parser)
{
  parser().foreach_token(TokenType::Comment, [&](Token tok) { parser.erase(tok); });
  parser.apply_mutations();
}

/* Remove trailing white spaces. */
template<typename ParserT>
void SourceProcessor::cleanup_whitespace(ParserT &parser, bool do_leading)
{
  const string &str = parser.str();

  if (do_leading) {
    /* Cleanup leading white-spaces at the start of the file.
     * Only to be done if there is a line directive at the top of the file. */
    size_t first_char = str.find_first_not_of(" \n");
    if (first_char != 0 && first_char != string::npos) {
      parser.replace(0, first_char - 1, "");
    }
  }

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
  parser().foreach_match<true>("#A", [&](const vector<Token> &tokens) {
    if (tokens[1].str() == "define") {
      if (tokens[1].next().str().starts_with("LIGHT_STACK_SIZE_")) {
        /* WORKAROUND: Avoid warning caused by EEVEE macro setup. */
        return;
      }
      if (tokens[1].next().str() == "GBUFFER_LAYER_MAX") {
        /* WORKAROUND: Avoid warning caused by EEVEE macro setup. */
        return;
      }
      if (tokens[1].next().str().starts_with("gather_")) {
        /* WORKAROUND: Avoid warning caused by EEVEE macro setup. */
        return;
      }

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
    parser.insert_before(struct_keyword, get_create_info_placeholder(string(type.str())));
    parser.insert_line_number(struct_keyword.str_index_start() - 1, struct_keyword.line_number());
  });

  parser().foreach_match("A(..)", [&](const vector<Token> &tokens) {
    if (tokens[0].str() == "CREATE_INFO_VARIANT") {
      const string variant_name(tokens[1].scope().front().next().str());
      metadata_.create_infos.emplace_back(variant_name);

      const string variant_decl = parser.substr_range_inclusive(tokens.front(), tokens.back());
      metadata_.create_infos_declarations.emplace_back(variant_decl);

      parser.replace(tokens.front(), tokens.back(), get_create_info_placeholder(variant_name));
      return;
    }
    if (tokens[0].str() == "GPU_SHADER_CREATE_INFO") {
      const string variant_name(tokens[1].scope().front().next().str());
      metadata_.create_infos.emplace_back(variant_name);

      const size_t start_end = tokens.back().str_index_last();
      const string end_tok = "GPU_SHADER_CREATE_END()";
      const size_t end_pos = parser.str().find(end_tok, start_end);
      if (end_pos == string::npos) {
        report_error(tokens[0], "Missing create info end.");
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
        report_error(tokens[0], "Missing create info end.");
        return;
      }

      end_pos = parser.str().find(')', end_pos);
      if (end_pos == string::npos) {
        report_error(tokens[0], "Missing parenthesis at info end.");
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
        report_error(tokens[0], "Missing create info end.");
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

/* Return the content without the first and last characters. */
static std::string_view str_view_exclusive(Token tok)
{
  std::string_view str = tok.str();
  if (str.length() < 2) {
    return "";
  }
  return str.substr(1, str.length() - 2);
}

void SourceProcessor::parse_includes(Parser &parser)
{
  parser().foreach_match<true>("#A\"", [&](const vector<Token> &tokens) {
    if (tokens[1].str() != "include") {
      return;
    }
    string_view dependency_name = str_view_exclusive(tokens[2]);

    if (dependency_name.find("defines.hh") != string::npos) {
      /* Dependencies between create infos are not needed for reflections.
       * Only the dependencies on the defines are needed. */
      metadata_.create_infos_dependencies.emplace_back(dependency_name);
    }

    if (dependency_name == "BLI_utildefines_variadic.hh") {
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

    if (dependency_name == filename) {
      report_error(tokens[2], "Recursive include");
    }
    metadata_.dependencies.emplace_back(dependency_name);
  });
  parser.apply_mutations();
}

bool SourceProcessor::has_pragma(Parser &parser, string_view pragma_str)
{
  bool has_pragma = false;
  /* Can't use foreach_match because it skips preprocessor scopes. */
  parser().foreach_token(Hash, [&](Token tok) {
    if (tok.scope().type() == ScopeType::Preprocessor && tok.next(1).str() == "pragma" &&
        tok.next(2).str() == pragma_str)
    {
      has_pragma = true;
    }
  });
  return has_pragma;
}

void SourceProcessor::parse_pragma_runtime_generated(Parser &parser)
{
  if (has_pragma(parser, "runtime_generated")) {
    metadata_.builtins.emplace_back(metadata::Builtin::runtime_generated);
  }
}

void SourceProcessor::lint_pragma_once(Parser &parser, const string &filename)
{
  if (filename.find("_lib.") == string::npos && filename.find(".hh") == string::npos) {
    return;
  }
  if (!has_pragma(parser, "once")) {
    report_error(parser[0], "Header files must contain #pragma once directive.");
  }
}

void SourceProcessor::lower_namesless_parameters(Parser &parser)
{
  parser().foreach_token(ParOpen, [&](Token tok) {
    if (tok.scope().type() != ScopeType::FunctionArgs) {
      return;
    }
    if (tok.prev(2).str().starts_with("Pipeline")) {
      return;
    }
    /* Make sure we matched a function definition and not a macro call. */
    if (tok.prev() != '>' && tok.prev(2) != Word && tok.prev(2) != '>') {
      return;
    }
    int i = 0;
    tok.scope().foreach_scope(ScopeType::FunctionArg, [&](Scope arg) {
      if (arg.token_count() == 1 || arg.back().prev() == TokenType::Const || arg.back() == '&' ||
          arg.back() == '>')
      {
        /* Append a name for nameless argument. */
        parser.replace(arg.back().str_index_last_no_whitespace() + 1,
                       arg.back().str_index_last(),
                       " _" + std::to_string(i++));
      }
    });
  });
}

void SourceProcessor::lower_namesless_parameters_ast(Parser &parser)
{
  for (FuncDecl fn : parser.root().descendants_of_type<FuncDecl>()) {
    int i = 0;
    for (FuncArg arg : fn.arguments().children_of_type<FuncArg>()) {
      if (!arg.identifier().is_valid()) {
        bool is_ref = arg.is_reference();
        Token arg_back(is_ref ? arg.declarator().reference().back() : arg.back());
        /* Append a name for nameless argument. */
        parser.replace(arg_back.str_index_last_no_whitespace() + 1,
                       arg_back.str_index_last(),
                       " _" + std::to_string(i++));
      }
    }
  }
}

void SourceProcessor::disabled_code_mutation(Parser &parser, bool new_bsl_compiler)
{
  auto process_disabled_scope = [&](Token start_tok) {
    /* Search for endif with the same indentation. Assume formatted input. */
    string end_str = string(start_tok.str_with_whitespace()) + "endif";
    size_t scope_end = parser.str().find(end_str, start_tok.str_index_start());
    if (scope_end == string::npos) {
      report_error(start_tok, "Couldn't find end of disabled scope.");
      return;
    }
    /* Search for else/elif with the same indentation. Assume formatted input. */
    string else_str = string(start_tok.str_with_whitespace()) + "el";
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

  auto process_enabled_scope = [&](Token start_tok) {
    /* Search for endif with the same indentation. Assume formatted input. */
    string end_str = string(start_tok.str_with_whitespace()) + "endif";
    size_t scope_end = parser.str().find(end_str, start_tok.str_index_start());
    if (scope_end == string::npos) {
      report_error(start_tok, "Couldn't find end of enabled scope.");
      return;
    }

    /* Find where the #if 1 line ends to start keeping content */
    size_t code_start = start_tok.line_end() + 1;

    /* Search for else/elif with the same indentation. */
    string else_str = string(start_tok.str_with_whitespace()) + "el";
    size_t scope_else = parser.str().find(else_str, start_tok.str_index_start());

    /* Erase the initial #if 1 directive line */
    parser.erase(start_tok.str_index_start(), code_start - 1);

    if (scope_else != string::npos && scope_else < scope_end) {
      /* Erase the disabled #else/#elif block up to the end of #endif */
      parser.erase(scope_else, scope_end + end_str.size());
    }
    else {
      /* If there's no #else branch, erase just the #endif directive line */
      parser.erase(scope_end, scope_end + end_str.size());
    }
  };

  parser().foreach_match<true>("#AA", [&](const vector<Token> &tokens) {
    if (tokens[1].str() != "ifndef") {
      return;
    }
    if (tokens[2].str() == "GPU_SHADER" ||
        (new_bsl_compiler ? tokens[2].str() == "BSL_530" : false))
    {
      process_disabled_scope(tokens[0]);
    }
  });
  parser().foreach_match<true>("#i!A(A)", [&](const vector<Token> &tokens) {
    if (tokens[1].str() != "if" || tokens[3].str() != "defined") {
      return;
    }
    if (tokens[5].str() == "GPU_SHADER" ||
        (new_bsl_compiler ? tokens[5].str() == "BSL_530" : false))
    {
      process_disabled_scope(tokens[0]);
    }
  });
  parser().foreach_match<true>("#i1", [&](const vector<Token> &tokens) {
    if (tokens[1].str() == "if") {
      if (tokens[2].str() == "0") {
        process_disabled_scope(tokens[0]);
      }
      else if (tokens[2].str() == "1") {
        process_enabled_scope(tokens[0]);
      }
    }
  });

  parser.apply_mutations();
}

string SourceProcessor::disabled_code_mutation(const string &str)
{
  Parser parser(str, error_handler);
  disabled_code_mutation(parser);
  return parser.result_get();
}

void SourceProcessor::lower_preprocessor(Parser &parser)
{
  /* Remove unsupported directives. */

  parser().foreach_match<true>("#A", [&](const vector<Token> &tokens) {
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
  parser.apply_mutations();
}

void SourceProcessor::lower_preprocessor_ast(Parser &parser)
{
  /* Remove unsupported directives. */
  for (Preprocessor directive : parser.root().descendants_of_type<Preprocessor>()) {
    Token type = directive.front().next();
    if (type.str() == "pragma") {
      Token pragma = type.next();
      if (pragma.str() == "once") {
        parser.erase(directive);
      }
      else if (pragma.str() == "runtime_generated") {
        parser.erase(directive);
      }
    }
    else if (type.str() == "include" && type.next() == String) {
      parser.erase(directive);
    }
  }
  parser.apply_mutations();
}

/* Support for BLI swizzle syntax. */
void SourceProcessor::lower_swizzle_methods(Parser &parser)
{
  /* Change C++ swizzle functions into plain swizzle. */
  /** IMPORTANT: This prevent the usage of any method with a swizzle name. */
  parser().foreach_match(".A()", [&](const vector<Token> &tokens) {
    string_view method_name(tokens[1].str());
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

void SourceProcessor::lower_swizzle_methods_ast(Parser &parser)
{
  /* Change C++ swizzle functions into plain swizzle. */
  /** IMPORTANT: This prevent the usage of any method with a swizzle name. */
  for (FuncCall call : parser.root().descendants_of_type<FuncCall>()) {
    ast::FuncParamList params = call.parameters();
    if (call.front().prev() != Dot || !params.is_empty()) {
      continue;
    }

    string_view method_name = call.identifier().str();
    if (method_name.length() > 1 && method_name.length() <= 4 &&
        (method_name.find_first_not_of("xyzw") == string::npos ||
         method_name.find_first_not_of("rgba") == string::npos))
    {
      /* `.xyz()` -> `.xyz  ` */
      parser.erase(params);
    }
  }
}

/* Support for C++ binary literal syntax for integers. */
void SourceProcessor::lower_binary_literals(Parser &parser)
{
  parser().foreach_token(Number, [&](const Token tok) {
    string_view str = tok.str();
    if (str.starts_with("0b") || str.starts_with("0B")) {
      int64_t value = std::stoll(string(str.substr(2)), nullptr, 2);
      parser.replace(tok.str_index_start(),
                     tok.str_index_last_no_whitespace(),
                     std::to_string(value) + (str.ends_with("u") ? "u" : ""));
    }
  });
}

void SourceProcessor::threadgroup_variables_parse_and_remove(Parser &parser)
{
  auto process_shared_var = [&](Token shared_tok, Token type, Token name, Token decl_end) {
    if (shared_tok.str() == "shared") {
      metadata_.shared_variables.push_back(
          {string(type.str()), parser.substr_range_inclusive(name, decl_end.prev())});

      parser.erase(shared_tok, decl_end);
    }
  };
  parser().foreach_match("AAA", [&](const vector<Token> &tokens) {
    Token end = tokens[2].find_next(lexit::SemiColon);
    process_shared_var(tokens[0], tokens[1], tokens[2], end);
  });
  parser.apply_mutations();
}

string SourceProcessor::threadgroup_variables_parse_and_remove(const string &str)
{
  IntermediateForm<FullLexer, DummyParser> parser(str, error_handler);
  auto process_shared_var = [&](Token shared_tok, Token type, Token name, Token decl_end) {
    if (shared_tok.str() == "shared") {
      metadata_.shared_variables.push_back(
          {string(type.str()), parser.substr_range_inclusive(name, decl_end.prev())});

      parser.erase(shared_tok, decl_end);
    }
  };
  parser().foreach_match("AAA", [&](const vector<Token> &tokens) {
    process_shared_var(tokens[0], tokens[1], tokens[2], tokens[2].find_next(lexit::SemiColon));
  });
  parser.apply_mutations();
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
          report_error(fn_type, "Expected void return type for node function");
          return;
        }
        if (fn_args.token_count() <= 3) {
          report_error(fn_type, "Expected at least one argument for node function");
          return;
        }
        FunctionFormat fn;
        fn.name = fn_name.str();

        fn_args.foreach_scope(ScopeType::FunctionArg, [&](Scope arg) {
          /* Note: There is no array support. */
          const Token name = arg.back();
          const Token type = name.prev() == '&' ? name.prev().prev() : name.prev();
          string qualifier(type.prev().str());
          if (qualifier != "out" && qualifier != "inout" && qualifier != "in") {
            if (name.prev() == '&') {
              qualifier = "out";
            }
            else if (qualifier != "const" && qualifier != "(" && qualifier != ",") {
              report_error(type.prev(),
                           "Unrecognized qualifier, expecting 'const', 'in', 'out' or 'inout'.");
              qualifier = "in";
            }
            else {
              qualifier = "in";
            }
          }
          fn.arguments.emplace_back(ArgumentFormat{metadata::Qualifier(hash(qualifier)),
                                                   metadata::Type(hash(string(type.str())))});
        });

        metadata_.functions.emplace_back(fn);
      });
}

void SourceProcessor::parse_library_functions_ast(Parser &parser)
{
  using namespace metadata;
  for (FuncDecl func : parser.root().children_of_type<FuncDecl>()) {
    if (!func.attributes().contains_attr("node")) {
      return;
    }
    if (func.return_type().str() != "void") {
      report_error(func.return_type(), "Expected void return type for node function");
      return;
    }
    if (func.arguments().is_empty()) {
      report_error(func.identifier(), "Expected at least one argument for node function");
      return;
    }

    FunctionFormat fn;
    fn.name = func.identifier().str();

    for (FuncArg arg : func.arguments().children_of_type<FuncArg>()) {
      if (arg.declarator().array().is_valid()) {
        report_error(arg.declarator().array(),
                     "Array arguments are not supported in node functions.");
      }

      Type type = Type(hash(string(arg.type().str())));
      Qualifier qualifier;
      if (arg.is_reference() && !arg.is_const()) {
        qualifier = Qualifier(hash("inout"));
      }
      else {
        qualifier = Qualifier(hash("in"));
      }

      fn.arguments.emplace_back(qualifier, type);
    }
    metadata_.functions.emplace_back(fn);
  }
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

void SourceProcessor::parse_builtins(Parser &parser, const std::string &filename)
{
  parser.apply_mutations();
  parse_builtins(parser.str(), filename);
}

/* Add padding member to empty structs.
 * Empty structs are useful for templating. */
void SourceProcessor::lower_empty_struct(Parser &parser)
{
  parser().foreach_struct([&](Token, Scope, Token, Scope body) {
    int decl_count = 0;
    body.foreach_declaration(
        [&](Scope, Token, Token, Scope, Token, Scope, Token) { decl_count += 1; });

    if (decl_count == 0) {
      parser.insert_before(body.back(), "int _pad;");
    }
  });

  parser.apply_mutations();
}

/* Parse, convert to create infos, and erase declaration. */
void SourceProcessor::lower_pipeline_definition(Parser &parser, const string &filename)
{
  using namespace metadata;

  auto process_compilation_constants = [&](Token tok) {
    string create_info_decl;

    while (tok == ',') {
      Token struct_name = tok.next();
      Scope scope = struct_name.next().scope();
      if (scope.token_count() == 2) {
        report_error(struct_name,
                     "Empty brace constructor is an error in Pipeline declaration. "
                     "Either remove it or add compilation constant values to it.");
      }
      auto process_constant = [&](const vector<Token> &toks) {
        create_info_decl += "COMPILATION_CONSTANT(";
        create_info_decl += (toks[3] == Number) ?
                                ((toks[3].str().back() == 'u') ? "uint" : "int") :
                                "bool";
        create_info_decl += ", " + string(toks[1].str());
        create_info_decl += ", " + string(toks[3].str());
        create_info_decl += ")\n";
      };
      scope.foreach_match(".A=A", process_constant);
      scope.foreach_match(".A=1", process_constant);
      tok = scope.back().next();
    }

    return create_info_decl;
  };

  auto validate_fn_name = [&](Token fn_name) {
    if (fn_name == '&') {
      report_error(fn_name, "Double function reference, remove '&'");
    }
    else if (fn_name != Word) {
      report_error(fn_name, "Expected function name");
    }
    return fn_name;
  };

  auto process_graphic_pipeline = [&](Token pipeline_name, Scope params) {
    Token vertex_fn = validate_fn_name(params[1]);
    Token fragment_fn = validate_fn_name(params[3]);
    /* For now, just emit good old create info macros. */
    string create_info_decl;
    create_info_decl += "GPU_SHADER_CREATE_INFO(" + string(pipeline_name.str()) + ")\n";
    create_info_decl += "GRAPHIC_SOURCE(\"" + filename + "\")\n";
    create_info_decl += "VERTEX_FUNCTION(\"" + string(vertex_fn.str()) + "\")\n";
    create_info_decl += "FRAGMENT_FUNCTION(\"" + string(fragment_fn.str()) + "\")\n";
    create_info_decl += "ADDITIONAL_INFO(" + string(vertex_fn.str()) + "_infos_)\n";
    create_info_decl += "ADDITIONAL_INFO(" + string(fragment_fn.str()) + "_infos_)\n";
    create_info_decl += process_compilation_constants(params[4]);
    create_info_decl += "DO_STATIC_COMPILATION()\n";
    create_info_decl += "GPU_SHADER_CREATE_END()\n";

    metadata_.create_infos_declarations.emplace_back(create_info_decl);
  };

  auto process_compute_pipeline = [&](Token pipeline_name, Scope params) {
    Token compute_fn = validate_fn_name(params[1]);
    /* For now, just emit good old create info macros. */
    string create_info_decl;
    create_info_decl += "GPU_SHADER_CREATE_INFO(" + string(pipeline_name.str()) + ")\n";
    create_info_decl += "COMPUTE_SOURCE(\"" + filename + "\")\n";
    create_info_decl += "COMPUTE_FUNCTION(\"" + string(compute_fn.str()) + "\")\n";
    create_info_decl += "ADDITIONAL_INFO(" + string(compute_fn.str()) + "_infos_)\n";
    create_info_decl += process_compilation_constants(params[2]);
    create_info_decl += "DO_STATIC_COMPILATION()\n";
    create_info_decl += "GPU_SHADER_CREATE_END()\n";

    metadata_.create_infos_declarations.emplace_back(create_info_decl);
  };

  parser().foreach_match("AA(", [&](const vector<Token> &tokens) {
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
      const string_view attr = attr_tok.str();
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

    guarded_scope_mutation(parser, fn_body, condition, Token(parser));
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
    string type(fn_type.str());
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
        type == "float4x2" || type == "float4x3" || type == "float4x4" || type == "bool")
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
      report_error(
          comma,
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
      string_view type_str(type.str());

      if (type_str.find("char") != string::npos || type_str.find("short") != string::npos ||
          type_str.find("half") != string::npos)
      {
        report_error(type, "Small types are forbidden in shader interfaces.");
      }
      else if (type_str == "float3") {
        report_error(type, "use packed_float3 instead of float3 in shared structure");
      }
      else if (type_str == "uint3") {
        report_error(type, "use packed_uint3 instead of uint3 in shared structure");
      }
      else if (type_str == "int3") {
        report_error(type, "use packed_int3 instead of int3 in shared structure");
      }
      else if (type_str == "bool") {
        report_error(type, "bool is not allowed in shared structure, use bool32_t");
      }
      else if (type_str == "float4x3") {
        report_error(type, "float4x3 is not allowed in shared structure");
      }
      else if (type_str == "float3x3") {
        report_error(type, "float3x3 is not allowed in shared structure");
      }
      else if (type_str == "float2x3") {
        report_error(type, "float2x3 is not allowed in shared structure");
      }
      else if (type_str == "float4x2") {
        report_error(type, "float4x2 is not allowed in shared structure");
      }
      else if (type_str == "float3x2") {
        report_error(type, "float3x2 is not allowed in shared structure");
      }
      else if (type_str == "float2x2") {
        report_error(type, "float2x2 is not allowed in shared structure");
      }

      auto sz = sizeof_types.find(string(type_str));

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
        report_error(type, "Unknown type, add 'enum' or 'struct' keyword before the type name");
        return;
      }

      if (type_info.size == 12) {
        has_vec3 = true;
      }

      size_t align = type_info.alignment - (offset % type_info.alignment);
      if (align != type_info.alignment) {
        string err = "Misaligned member, missing " + to_string(align) + " padding bytes";
        report_error(type, err);
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
      report_error(struct_name, err);
    }
    /* Insert an alias to the type that will get referenced for shaders that enforce usage of
     * linted types. */
    string directive = "#define " + string(struct_name.str()) + linted_struct_suffix + " " +
                       string(struct_name.str()) + "\n";
    if (is_std140_compatible) {
      directive += "#define " + string(struct_name.str()) + linted_struct_suffix +
                   uniform_struct_suffix + " " + string(struct_name.str()) + "\n";
    }
    parser.insert_directive(struct_keyword.prev(), directive);
  });
  parser.apply_mutations();
}

void SourceProcessor::lint_unbraced_statements(Parser &parser)
{
  auto check_statement = [&](const Tokens &toks) {
    Token end_tok = toks.back();
    if (end_tok.next() == If || end_tok.scope().type() == ScopeType::Preprocessor) {
      return;
    }
    if (end_tok.next() == '[' && end_tok.next().next() == '[') {
      end_tok = end_tok.next().scope().back();
    }
    if (end_tok.next() != '{') {
      report_error(end_tok, "Missing curly braces after flow control statement.");
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
      "vec2",   "vec3",     "vec4",      "mat2x2",   "mat2x3",    "mat2x4",   "mat3x2",
      "mat3x3", "mat3x4",   "mat4x2",    "mat4x3",   "mat4x4",    "mat2",     "mat3",
      "mat4",   "ivec2",    "ivec3",     "ivec4",    "uvec2",     "uvec3",    "uvec4",
      "bvec2",  "bvec3",    "bvec4",     "common",   "partition", "active",   "typedef",
      "packed", "resource", "goto",      "noinline", "extern",    "external", "interface",
      "long",   "fixed",    "unsigned",  "superp",   "input",     "output",   "hvec2",
      "hvec3",  "hvec4",    "fvec2",     "fvec3",    "fvec4",     "sample",   "sampler3DRect",
      "filter", "cast",     "row_major", "inout",
  };

  parser().foreach_token(Word, [&](Token tok) {
    if (reserved_symbols.contains(string(tok.str()))) {
      string err = string(tok.str()) + " is a reserved token";
      report_error(tok, err);
    }
  });
}

void SourceProcessor::lower_tests(Parser &parser, const string &prefix)
{
  parser().foreach_function([&](bool, Token type, Token, Scope, bool, Scope fn_body) {
    if (type.str() != "void") {
      return;
    }
    /* Note: Assume any function containing tests are entry points. */
    int test_id = 0;
    fn_body.foreach_match("A(A,A){..}", [&](Tokens toks) {
      if (toks[0].str() != "TEST") {
        return;
      }
      Scope test_body = toks[6].scope();
      parser.erase(toks[0], toks[5]);
      test_body.foreach_match("A(..)", [&](Tokens toks) {
        if (toks[0].str().starts_with("EXPECT_")) {
          int id = test_id;
          parser.insert_before(toks[0], prefix + "out_test[" + to_string(id) + "] = ");
          parser.insert_after(toks[4],
                              "; " + prefix + "out_test[" + to_string(id) +
                                  "].line = " + to_string(toks[0].line_number()));
          test_id++;
        }
      });
    });
  });

  parser.apply_mutations();
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
      if (tok.next() != Constexpr) {
        parser.erase(tok);
      }
    }
  });

  /* Erase `public:` and `private:` keywords. Access is checked by C++ compilation. */
  auto process_access = [&](Token tok) {
    if (tok.next() == ':') {
      parser.erase(tok, tok.next());
    }
    else {
      report_error(tok, "Expecting colon ':' after access specifier");
    }
  };
  parser().foreach_token(Private, process_access);
  parser().foreach_token(Public, process_access);

  lower_template_dependent_names(parser);
}

void SourceProcessor::lower_noop_keywords_ast(Parser &parser)
{
  /* inline has no equivalent in GLSL and is making parsing more complicated. */
  parser().foreach_token(Inline, [&](Token tok) { parser.erase(tok); });
  /* Erase `public:` and `private:` keywords. Access is checked by C++ compilation. */
  for (AccessSpecifier node : parser.root().descendants_of_type<AccessSpecifier>()) {
    parser.erase(node);
  }
  /* Given our code-style, we don't need the disambiguation. */
  for (TemplateExplicit node : parser.root().descendants_of_type<TemplateExplicit>()) {
    parser.erase(node.front());
  }
  /* Remove `struct`, `class`, `enum`, `union` from type declaration. */
  for (IdType type : parser.root().descendants_of_type<IdType>()) {
    Token tok = type.identifier().front().prev();
    if (tok == Struct || tok == Class || tok == Enum || tok == Union) {
      parser.erase(tok);
    }
  }
}

void SourceProcessor::lower_trailing_comma_in_list(Parser &parser)
{
  parser().foreach_match(",}", [&](const Tokens &t) { parser.erase(t[0]); });
}

void SourceProcessor::lower_trailing_comma_in_list_ast(Parser &parser)
{
  for (InitializerList decl : parser.root().descendants_of_type<InitializerList>()) {
    if (decl.back().prev() == ',') {
      parser.erase(decl.back().prev());
    }
  }
}

/* Allow easier parsing of struct member declaration.
 * Example: `int a, b;` > `int a; int b;` */
void SourceProcessor::lower_comma_separated_declarations(Parser &parser)
{
  auto process_decl = [&](const Tokens &t) {
    if (t[0].scope().type() != ScopeType::Struct) {
      return;
    }
    string type(t[0].str());
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
                             "{" + string(type.str()) + " _tmp = " + string(type.str()) +
                                 string(list.str()) + "; ");
        const Token start = toks[1].is_valid() ? toks[1] : list.front();
        parser.replace(start, list.back(), "_tmp;}");
      }
      else if (toks[1].is_invalid()) {
        /* Regular initializer list. Keep it simple. */
        parser.insert_after(toks[0], string(type.str()));
      }
    });
  });
}

void SourceProcessor::lower_implicit_return_types_ast(Parser &parser)
{
  for (FuncDecl func : parser.root().descendants_of_type<FuncDecl>()) {
    for (ReturnStmt stmt : func.body().descendants_of_type<ReturnStmt>()) {
      Expr expr = stmt.expression();
      if (!expr.is_valid()) {
        return;
      }
      InitializerList list;
      Node node = expr.child_first();
      if (node == NodeType::InitializerList) {
        list = node;
      }
      else if (node == NodeType::Constructor) {
        list = node.child_first();
      }
      else {
        return;
      }

      const string type_str(func.return_type().str());
      if (list.child_first() == NodeType::DesignatedInitializer) {
        /* `return {1, 2};` > `T tmp = T{1, 2}; return tmp;`
         * This syntax allow to support designated initializer. */
        parser.replace(
            stmt, "{" + type_str + " _tmp" + string(list.str()) + "; return _tmp;}", true);
      }
      else {
        /* Regular initializer list. Keep it simple. */
        parser.insert_before(list.front(), type_str);
      }
    }
  }
}

void SourceProcessor::lower_initializer_implicit_types(Parser &parser)
{
  auto process_scope = [&](Scope s) {
    /* Auto insert equal. */
    s.foreach_match("AA{..}",
                    [&](Tokens t) { parser.insert_before(t[2], " = " + string(t[0].str())); });
    /* Auto insert type. */
    s.foreach_match("AA={..}", [&](Tokens t) { parser.insert_before(t[3], string(t[0].str())); });
  };

  parser().foreach_scope(ScopeType::FunctionArg, process_scope);
  parser().foreach_scope(ScopeType::Function, process_scope);
  parser.apply_mutations();
}

void SourceProcessor::lower_initializer_implicit_types_ast(Parser &parser)
{
  for (VarDecl decl : parser.root().descendants_of_type<VarDecl>()) {
    for (Declarator var : decl.children_of_type<Declarator>()) {
      InitializerList init_list = var.initializer_list();
      if (init_list.is_valid()) {
        /* Insert assignment. */
        parser.insert_before(init_list.front(), " = " + string(decl.type().str()));
        return;
      }

      AssignStmt assign = var.initial_value();
      if (assign.is_valid()) {
        InitializerList init_list = assign.initializer_list();
        if (init_list.is_valid()) {
          /* Insert type. */
          parser.insert_before(init_list.front(), string(decl.type().str()));
          return;
        }
      }
    }
  }

  parser.apply_mutations();
}

void SourceProcessor::lower_designated_initializers(Parser &parser)
{
  /* Transform to compatibility macro. */
  parser().foreach_match("A{.A=", [&](Tokens t) {
    if (t[0].prev() != '=' || t[0].prev().prev() != Word) {
      report_error(t[0], "Designated initializers are only supported in assignments");
      return;
    }
    /* Lint for nested aggregates. */
    Token nested_aggregate_end = t[0].scope().find_token(BracketClose);
    if (nested_aggregate_end != t[3]) {
      Token nested_aggregate_start = nested_aggregate_end.scope().front();
      if (nested_aggregate_start.prev() != Word) {
        report_error(nested_aggregate_start, "Nested anonymous aggregate is not supported");
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
        report_error(t[0], "Nested initializer lists are not supported");
        return;
      }
      parser.insert_before(t[0], string(var.str()));
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
      if (builtin_types.contains(string(t[0].str()))) {
        report_error(t[0],
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
          report_error(nested_aggregate_start, "Nested anonymous aggregate is not supported");
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

/* Support for **full** aggregate initialization.
 * They are converted to default constructor for GLSL. */
void SourceProcessor::lower_aggregate_initializers_ast(Parser &parser)
{
  unordered_set<string> builtin_types = {
      "float2",   "float3",   "float4",   "float2x2", "float2x3", "float2x4",
      "float3x2", "float3x3", "float3x4", "float4x2", "float4x3", "float4x4",
      "float2x2", "float3x3", "float4x4", "int2",     "int3",     "int4",
      "uint2",    "uint3",    "uint4",    "bool2",    "bool3",    "bool4",
  };

  /* Transform aggregate to compatibility macro. */
  for (InitializerList list : parser.root().descendants_of_type<InitializerList>()) {
    IdType type(list.prev());
    if (!type.is_valid()) {
      return;
    }
    /* Lint unsafe use with vector types. */
    if (builtin_types.contains(string(type.str()))) {
      report_error(type.front(),
                   "Aggregate is error prone for built-in vector and matrix types, use "
                   "constructors instead");
    }
    /* Call generated default ctor for empty bracket initializer. */
    if (list.is_empty()) {
      parser.insert_after(type.back(), "_ctor_");
      parser.replace(list, "()", true);
      return;
    }
    /* Lint for nested aggregates. */
    for (InitializerList nested_list : list.descendants_of_type<InitializerList>()) {
      if (!IdType(nested_list.prev()).is_valid()) {
        report_error(nested_list.front(), "Nested anonymous aggregate is not supported");
      }
    }
    /* `A{1,}` -> `_agg(A,1)` */
    parser.insert_before(type.front(), "_ctor(");
    parser.insert_after(type.back(), ",");
    parser.erase(list.front());
    if (list.back().prev() == ',') {
      parser.erase(list.back().prev());
    }
    parser.insert_before(list.back(), " _rotc()");
    parser.erase(list.back());
  }

  parser.apply_mutations();
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
        report_error(name_tok, "Array size must be greater than zero.");
      }
      parser.insert_after(array_scope[0], to_string(list_len));
    }
    else if (array_scope_tok_len == 3 && array_scope[1] == Number) {
      if (stol(string(array_scope[1].str())) == 0) {
        report_error(name_tok, "Array size must be greater than zero.");
      }
    }

    /* Lint nested initializer list. */
    list_scope.foreach_token(BracketOpen, [&](Token tok) {
      if (tok != list_scope.front()) {
        report_error(name_tok, "Nested initializer list is not supported.");
      }
    });

    /* Mutation to compatible syntax. */
    parser.insert_before(list_scope.front(), "ARRAY_T(" + string(type_tok.str()) + ") ARRAY_V(");
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
  parser().foreach_function([&](const bool is_static,
                                Token fn_type,
                                Token fn_name,
                                Scope fn_args,
                                const bool fn_const,
                                Scope fn_body) {
    if (!fn_args.contains_token('=')) {
      return;
    }

    const bool has_non_void_return_type = fn_type.str() != "void";
    const bool is_method = fn_type.scope().type() == ScopeType::Struct;

    string args_decl;
    string args_names;
    string struct_name;

    if (is_method) {
      struct_name = fn_type.scope().front().prev().str();
    }

    vector<string> fn_overloads;

    fn_args.foreach_scope(ScopeType::FunctionArg, [&](Scope arg) {
      Token equal = arg.find_token('=');
      const char *comma = (args_decl.empty() ? "" : ", ");
      if (equal.is_invalid()) {
        args_decl += comma + string(arg.str_with_whitespace());
        args_names += comma + string(arg.back().str());
      }
      else {
        string arg_name(equal.prev().str());
        string value = parser.substr_range_inclusive(equal.next(), arg.back());
        string decl = parser.substr_range_inclusive(arg.front(), equal.prev());

        string fn_call = string(fn_name.str()) + '(' + args_names + comma + value + ");";
        if (is_method) {
          if (is_static) {
            fn_call = struct_name + namespace_separator + fn_call;
          }
          else {
            fn_call = "this->" + fn_call;
          }
        }
        if (has_non_void_return_type) {
          fn_call = "return " + fn_call;
        }
        string overload;
        overload += string(fn_type.str()) + " ";
        overload += string(fn_name.str()) + '(' + args_decl + ")" +
                    string(fn_const ? " const" : "") + "\n";
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
  parser().foreach_match<true>("#A1", [&](vector<Token> toks) {
    if (toks[1].str() != "line") {
      return;
    }
    if (toks[2].next() == String) {
      /* Do not process directives with filenames. */
      return;
    }
    /* Workaround the foreach_match not matching overlapping patterns. */
    if (toks.back().next() == '#' && toks.back().next().next() == Word &&
        toks.back().next().next().next() == Number)
    {
      parser.replace(toks[0].line_start(), toks[0].line_end() + 1, "");
    }
  });
  parser.apply_mutations();

  parser().foreach_match<true>("#A1#A", [&](vector<Token> toks) {
    if (toks[1].str() != "line") {
      return;
    }
    /* Workaround the foreach_match not matching overlapping patterns. */
    if (toks.back().next() == '#' && toks.back().next().next() == Word &&
        toks.back().next().next().next() == Number)
    {
      parser.replace(toks[0].line_start(), toks[0].line_end() + 1, "");
    }
  });
  parser.apply_mutations();

  parser().foreach_match<true>("#A1", [&](vector<Token> toks) {
    if (toks[1].str() != "line") {
      return;
    }
    if (toks[2].next() == String) {
      /* Do not process directives with filenames. */
      return;
    }
    int line = toks[0].line_number();
    int value = stol(string(toks[2].str()));

    Token prev = toks[0].prev();
    Token next = toks[2].next();
    /* True if the directive splits a logical line and the parts do not overlap. */
    if (prev.line_number() == value) {
      /* Backtrack to find the first token of the previous line. */
      Token first_on_prev_line = prev;
      Token peek = first_on_prev_line.prev();
      while (peek.is_valid() && peek.str_with_whitespace().find_first_of('\n') == string::npos) {
        first_on_prev_line = peek;
        peek = first_on_prev_line.prev();
      }

      /* Check if the previous line is a preprocessor directive. */
      bool is_prev_directive = (first_on_prev_line.is_valid() && first_on_prev_line == '#');

      /* Only merge if the previous line is NOT a preprocessor directive. */
      if (!is_prev_directive) {
        int prev_end_col = prev.char_number() + prev.str().length();
        int next_start_col = next.char_number();

        if (prev_end_col < next_start_col) {
          int spaces_needed = next_start_col - prev_end_col;
          parser.replace(prev.str_index_last_no_whitespace() + 1,
                         next.str_index_start() - 1,
                         std::string(spaces_needed, ' '));
          return;
        }
      }
    }
    /* True if directive is noop. */
    if (line == value) {
      parser.replace(toks[0].line_start(), toks[0].line_end() + 1, "");
    }
    /* True if directive is not better than 1 newline. */
    if (line == value - 1) {
      parser.replace(toks[0].line_start(), toks[0].line_end(), "");
    }
    /* True if directive is not better than 2 newline. */
    if (line == value - 2) {
      parser.replace(toks[0].line_start(), toks[0].line_end(), "\n");
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

  IntermediateForm<FullLexer, DummyParser> parser(str, error_handler);
  parser().foreach_token(ParOpen, [&](const Token t) {
    if (t.prev() == Word) {
      Token fn_name = t.prev();
      string_view fn_name_str = fn_name.str();
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
    if (type.prev() == TokenType::Const) {
      parser.replace(type.prev(), last_tok, string(type.str()) + " " + string(arg_name.str()));
    }
    else {
      parser.replace(type, last_tok, "inout " + string(type.str()) + " " + string(arg_name.str()));
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
        report_error(token, "Reference definitions cannot have side effects.");
      });
      assignment.foreach_token(Decrement, [&](const Token token) {
        report_error(token, "Reference definitions cannot have side effects.");
      });
      assignment.foreach_token(ParOpen, [&](const Token token) {
        string_view fn_name = token.prev().str();
        if ((fn_name != "specialization_constant_get") && (fn_name != "push_constant_get") &&
            (fn_name != "interface_get") && (fn_name != "resource_table_get") &&
            (fn_name != "attribute_get") && (fn_name != "buffer_get") &&
            (fn_name != "srt_access") && (fn_name != "sampler_get") && (fn_name != "image_get"))
        {
          report_error(token, "Reference definitions cannot contain function calls.");
        }
      });
      assignment.foreach_scope(ScopeType::Subscript, [&](const Scope subscript) {
        if (subscript.token_count() != 3) {
          report_error(subscript.front(),
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
          report_error(index_var,
                       "Cannot locate array subscript variable declaration. "
                       "If it is a global variable, assign it to a temporary const variable for "
                       "indexing inside the reference.");
          return;
        }
        if (!is_const) {
          report_error(index_var, "Array subscript variable must be declared as const qualified.");
          return;
        }
        if (is_ref) {
          report_error(index_var, "Array subscript variable must not be declared as reference.");
          return;
        }
      });

      string definition = parser.substr_range_inclusive(assignment[1], assignment.back());

      bool error = false;
      /* Replace declaration. */
      parser.erase(decl_start, decl_end);
      /* Replace all occurrences with definition. */
      name.scope().foreach_token(Word, [&](const Token token) {
        /* Do not match member access or function calls. */
        if (error || token.prev() == '.' || token.next() == '(') {
          return;
        }
        if (token.str_index_start() > decl_end.str_index_last() && token.str() == name.str()) {
          if (token.prev() == '&' && token.next() == '=') {
            report_error(token, "Local reference shadowing is not allowed.");
            error = true;
          }
          else {
            parser.replace(token, definition);
          }
        }
      });
    });
  });
  parser.apply_mutations();

  parser().foreach_match("c?A&A=", [&](const vector<Token> &tokens) {
    report_error(tokens[4], "Reference is defined inside a global or unterminated scope.");
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

void SourceProcessor::lower_gather_component(Parser &parser)
{
  parser().foreach_match("A(..)", [&](const Tokens &toks) {
    if (toks[0].scope().type() == ScopeType::Preprocessor) {
      /* Don't mutate the actual implementation. */
      return;
    }
    Token component = toks.back().prev();
    /* Assume that if there is a number at the end of argument list, it is the component id. */
    if (toks[0].str() == "textureGather" && component == Number && component.prev() == Comma) {
      parser.insert_after(toks[0], string(component.str()));
      parser.erase(component.prev(), component);
    }
  });
  parser.apply_mutations();
}

string SourceProcessor::argument_decorator_macro_injection(const string &str)
{
  IntermediateForm<FullLexer, DummyParser> parser(str, error_handler);
  /* Example: `out float foo` > `out float _out_sta foo _out_end` */
  parser().foreach_match("AAA", [&](const Tokens &t) {
    string_view qualifier = t[0].str();
    if (qualifier == "out" || qualifier == "inout" || qualifier == "in" || qualifier == "shared") {
      parser.insert_after(t[1], " _" + string(qualifier) + "_sta ");
      parser.insert_after(t[2], " _" + string(qualifier) + "_end ");
    }
  });
  return parser.result_get();
}

string SourceProcessor::array_constructor_macro_injection(const string &str)
{
  IntermediateForm<FullLexer, DummyParser> parser(str, error_handler);
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
      report_error(
          tokens[2],
          "Global scope constant expression found. These get allocated per-thread in MSL. "
          "Use Macro's or uniforms instead.");
    }
  });
}

void SourceProcessor::lint_global_scope_constants_ast(Parser &parser)
{
  /* Example: `const uint global_var = 1u;`. */
  for (VarDecl decl : parser.root().children_of_type<VarDecl>()) {
    if (decl.is_const()) {
      report_error(
          decl,
          "Global scope constant expression found. These get allocated per-thread in MSL. "
          "Use Macro's or uniforms instead.");
    }
  }
}

int SourceProcessor::static_array_size(const Scope &array, int fallback_value)
{
  if (array.token_count() == 3 && array[1] == Number) {
    try {
      return stol(string(array[1].str()));
    }
    catch (invalid_argument const & /*ex*/) {
      report_error(array.front(), "Invalid array size, expecting integer literal");
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
