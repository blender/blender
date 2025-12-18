/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include <cctype>
#include <cstdint>
#include <functional>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "intermediate.hh"

namespace blender::gpu::shader {

#define ERROR_TOK(token) (token).line_number(), (token).char_number(), (token).line_str()

/* Metadata extracted from shader source file.
 * These are then converted to their GPU module equivalent. */
/* TODO(fclem): Make GPU enums standalone and directly use them instead of using separate enums
 * and types. */
namespace metadata {

/* Compile-time hashing function which converts string to a 64bit hash. */
constexpr static uint64_t hash(const char *name)
{
  uint64_t hash = 2166136261u;
  while (*name) {
    hash = hash * 16777619u;
    hash = hash ^ *name;
    ++name;
  }
  return hash;
}

static uint64_t hash(const std::string &name)
{
  return hash(name.c_str());
}

enum Builtin : uint64_t {
  ClipDistance = hash("gl_ClipDistance"),
  FragCoord = hash("gl_FragCoord"),
  FragStencilRef = hash("gl_FragStencilRefARB"),
  FrontFacing = hash("gl_FrontFacing"),
  GlobalInvocationID = hash("gl_GlobalInvocationID"),
  InstanceIndex = hash("gpu_InstanceIndex"),
  BaseInstance = hash("gpu_BaseInstance"),
  InstanceID = hash("gl_InstanceID"),
  LocalInvocationID = hash("gl_LocalInvocationID"),
  LocalInvocationIndex = hash("gl_LocalInvocationIndex"),
  NumWorkGroup = hash("gl_NumWorkGroup"),
  PointCoord = hash("gl_PointCoord"),
  PointSize = hash("gl_PointSize"),
  PrimitiveID = hash("gl_PrimitiveID"),
  VertexID = hash("gl_VertexID"),
  WorkGroupID = hash("gl_WorkGroupID"),
  WorkGroupSize = hash("gl_WorkGroupSize"),
  drw_debug = hash("drw_debug_"),
  printf = hash("printf"),
  assert = hash("assert"),
  runtime_generated = hash("runtime_generated"),
};

enum Qualifier : uint64_t {
  in = hash("in"),
  out = hash("out"),
  inout = hash("inout"),
};

enum Type : uint64_t {
  float1 = hash("float"),
  float2 = hash("float2"),
  float3 = hash("float3"),
  float4 = hash("float4"),
  float3x3 = hash("float3x3"),
  float4x4 = hash("float4x4"),
  sampler1DArray = hash("sampler1DArray"),
  sampler2DArray = hash("sampler2DArray"),
  sampler2D = hash("sampler2D"),
  sampler3D = hash("sampler3D"),
  Closure = hash("Closure"),
};

struct ArgumentFormat {
  Qualifier qualifier;
  Type type;
};

struct FunctionFormat {
  std::string name;
  std::vector<ArgumentFormat> arguments;
};

struct PrintfFormat {
  uint32_t hash;
  std::string format;
};

struct SharedVariable {
  std::string type;
  std::string name;
};

struct ParsedResource {
  /** Line this resource was defined. */
  size_t line;

  std::string var_type;
  std::string var_name;
  std::string var_array;

  std::string res_type;
  /** For images, storage, uniforms and samplers. */
  std::string res_frequency = "PASS";
  /** For images, storage, uniforms and samplers. */
  std::string res_slot;
  /** For images & storage. */
  std::string res_qualifier;
  /** For specialization & compilation constants. */
  std::string res_value;
  /** For images. */
  std::string res_format;
  /** Optional condition to enable this resource. */
  std::string res_condition;

  std::string serialize() const
  {
    std::string res_condition_lambda;

    if (!res_condition.empty()) {
      res_condition_lambda = ", [](blender::Span<CompilationConstant> constants) { ";
      res_condition_lambda += res_condition;
      res_condition_lambda += "}";
    }

    std::stringstream ss;
    if (res_type == "legacy_info") {
      ss << "ADDITIONAL_INFO(" << var_name << ")";
    }
    else if (res_type == "resource_table") {
      if (!res_condition.empty()) {
        ss << ".additional_info_with_condition(\"" << var_type << "\"" << res_condition_lambda
           << ")";
      }
      else {
        ss << ".additional_info(\"" << var_type << "\")";
      }
    }
    else if (res_type == "sampler") {
      ss << ".sampler(" << res_slot;
      ss << ", ImageType::" << var_type;
      ss << ", \"" << var_name << "\"";
      ss << ", Frequency::" << res_frequency;
      ss << ", GPUSamplerState::internal_sampler()";
      ss << res_condition_lambda << ")";
    }
    else if (res_type == "image") {
      ss << ".image(" << res_slot;
      ss << ", blender::gpu::TextureFormat::" << res_format;
      ss << ", Qualifier::" << res_qualifier;
      ss << ", ImageReadWriteType::" << var_type;
      ss << ", \"" << var_name << "\"";
      ss << ", Frequency::" << res_frequency;
      ss << res_condition_lambda << ")";
    }
    else if (res_type == "uniform") {
      ss << ".uniform_buf(" << res_slot;
      ss << ", \"" << var_type << "\"";
      ss << ", \"" << var_name << var_array << "\"";
      ss << ", Frequency::" << res_frequency;
      ss << res_condition_lambda << ")";
    }
    else if (res_type == "storage") {
      ss << ".storage_buf(" << res_slot;
      ss << ", Qualifier::" << res_qualifier;
      ss << ", \"" << var_type << "\"";
      ss << ", \"" << var_name << var_array << "\"";
      ss << ", Frequency::" << res_frequency;
      ss << res_condition_lambda << ")";
    }
    else if (res_type == "push_constant") {
      ss << "PUSH_CONSTANT(" << var_type << ", " << var_name << ")";
    }
    else if (res_type == "compilation_constant") {
      /* Needs to be defined on the shader declaration. */
      /* TODO(fclem): Add check that shader sets an existing compilation constant. */
      // ss << "COMPILATION_CONSTANT(" << var_type << ", " << var_name << ", " << res_value << ")";
    }
    else if (res_type == "specialization_constant") {
      ss << "SPECIALIZATION_CONSTANT(" << var_type << ", " << var_name << ", " << res_value << ")";
    }
    return ss.str();
  }
};

struct ResourceTable : std::vector<ParsedResource> {
  std::string name;
};

struct ParsedAttribute {
  /* Line this resource was defined. */
  size_t line;

  std::string var_type;
  std::string var_name;

  std::string interpolation_mode;

  std::string serialize() const
  {
    std::stringstream ss;
    if (interpolation_mode == "flat") {
      ss << "FLAT(" << var_type << ", " << var_name << ")";
    }
    else if (interpolation_mode == "smooth") {
      ss << "SMOOTH(" << var_type << ", " << var_name << ")";
    }
    else if (interpolation_mode == "smooth") {
      ss << "NO_PERSPECTIVE(" << var_type << ", " << var_name << ")";
    }
    return ss.str();
  }
};

struct StageInterface : std::vector<ParsedAttribute> {
  std::string name;

  std::string serialize() const
  {
    std::stringstream ss;
    ss << "GPU_SHADER_INTERFACE_INFO(" << name << "_t)\n";

    for (const auto &res : *this) {
      ss << res.serialize() << "\n";
    }

    ss << "GPU_SHADER_INTERFACE_END()\n";
    return ss.str();
  }
};

struct ParsedFragOuput {
  /* Line this resource was defined. */
  size_t line;

  std::string var_type;
  std::string var_name;

  std::string slot;
  std::string dual_source;
  std::string raster_order_group;

  std::string serialize() const
  {
    std::stringstream ss;
    if (!dual_source.empty()) {
      ss << "FRAGMENT_OUT_DUAL(" << slot << ", " << var_type << ", " << var_name << ", "
         << dual_source << ")";
    }
    else if (!raster_order_group.empty()) {
      ss << "FRAGMENT_OUT_ROG(" << slot << ", " << var_type << ", " << var_name << ", "
         << raster_order_group << ")";
    }
    else {
      ss << "FRAGMENT_OUT(" << slot << ", " << var_type << ", " << var_name << ")";
    }
    return ss.str();
  }
};

struct FragmentOutputs : std::vector<ParsedFragOuput> {
  std::string name;

  std::string serialize() const
  {
    std::stringstream ss;
    ss << "GPU_SHADER_CREATE_INFO(" << name << ")\n";

    for (const auto &res : *this) {
      ss << res.serialize() << "\n";
    }

    ss << "GPU_SHADER_CREATE_END()\n";
    return ss.str();
  }
};

struct ParsedVertInput {
  /* Line this resource was defined. */
  size_t line;

  std::string var_type;
  std::string var_name;

  std::string slot;

  std::string serialize() const
  {
    std::stringstream ss;
    ss << "VERTEX_IN(" << slot << ", " << var_type << ", " << var_name << ")";
    return ss.str();
  }
};

struct VertexInputs : std::vector<ParsedVertInput> {
  std::string name;

  std::string serialize() const
  {
    std::stringstream ss;
    ss << "GPU_SHADER_CREATE_INFO(" << name << ")\n";

    for (const auto &res : *this) {
      ss << res.serialize() << "\n";
    }

    ss << "GPU_SHADER_CREATE_END()\n";
    return ss.str();
  }
};

struct Source {
  std::vector<Builtin> builtins;
  /* Note: Could be a set, but for now the order matters. */
  std::vector<std::string> dependencies;
  std::vector<SharedVariable> shared_variables;
  std::vector<PrintfFormat> printf_formats;
  std::vector<FunctionFormat> functions;
  std::vector<std::string> create_infos;
  std::vector<std::string> create_infos_declarations;
  std::vector<std::string> create_infos_dependencies;
  std::vector<std::string> create_infos_defines;
  std::vector<ResourceTable> resource_tables;
  std::vector<StageInterface> stage_interfaces;
  std::vector<FragmentOutputs> fragment_outputs;
  std::vector<VertexInputs> vertex_inputs;

  std::string serialize(const std::string &function_name) const
  {
    std::stringstream ss;
    ss << "static void " << function_name
       << "(GPUSource &source, GPUFunctionDictionary *g_functions, GPUPrintFormatMap *g_formats) "
          "{\n";
    for (auto function : functions) {
      ss << "  {\n";
      ss << "    Vector<metadata::ArgumentFormat> args = {\n";
      for (auto arg : function.arguments) {
        ss << "      "
           << "metadata::ArgumentFormat{"
           << "metadata::Qualifier(" << std::to_string(uint64_t(arg.qualifier)) << "LLU), "
           << "metadata::Type(" << std::to_string(uint64_t(arg.type)) << "LLU)"
           << "},\n";
      }
      ss << "    };\n";
      ss << "    source.add_function(\"" << function.name << "\", args, g_functions);\n";
      ss << "  }\n";
    }
    for (auto builtin : builtins) {
      ss << "  source.add_builtin(metadata::Builtin(" << std::to_string(builtin) << "LLU));\n";
    }
    for (auto dependency : dependencies) {
      ss << "  source.add_dependency(\"" << dependency << "\");\n";
    }
    for (auto var : shared_variables) {
      ss << "  source.add_shared_variable(Type::" << var.type << "_t, \"" << var.name << "\");\n";
    }
    for (auto format : printf_formats) {
      ss << "  source.add_printf_format(uint32_t(" << std::to_string(format.hash) << "), "
         << format.format << ", g_formats);\n";
    }
    /* Avoid warnings. */
    ss << "  UNUSED_VARS(source, g_functions, g_formats);\n";
    ss << "}\n";
    return ss.str();
  }

  std::string serialize_infos() const
  {
    std::stringstream ss;
    ss << "#pragma once\n";
    ss << "\n";
    for (auto dependency : create_infos_dependencies) {
      ss << "#include \"" << dependency << "\"\n";
    }
    ss << "\n";
    for (auto vert_inputs : vertex_inputs) {
      ss << vert_inputs.serialize() << "\n";
    }
    ss << "\n";
    for (auto frag_outputs : fragment_outputs) {
      ss << frag_outputs.serialize() << "\n";
    }
    ss << "\n";
    for (auto iface : stage_interfaces) {
      ss << iface.serialize() << "\n";
    }
    ss << "\n";
    for (auto res_table : resource_tables) {
      ss << "GPU_SHADER_CREATE_INFO(" << res_table.name << ")\n";
      for (const auto &res : res_table) {
        ss << res.serialize() << "\n";
      }
      ss << "GPU_SHADER_CREATE_END()\n";
    }
    ss << "\n";
    for (auto define : create_infos_defines) {
      ss << define;
    }
    ss << "\n";
    for (auto declaration : create_infos_declarations) {
      ss << declaration << "\n";
    }
    return ss.str();
  }
};

}  // namespace metadata

/**
 * Shader source preprocessor that allow to mutate GLSL into cross API source that can be
 * interpreted by the different GPU backends. Some syntax are mutated or reported as incompatible.
 *
 * Implementation speed is not a huge concern as we only apply this at compile time or on python
 * shaders source.
 */
class Preprocessor {
  using uint64_t = std::uint64_t;
  using report_callback = parser::report_callback;
  using Parser = shader::parser::IntermediateForm;
  using Tokens = std::vector<shader::parser::Token>;

  metadata::Source metadata;

 public:
  enum SourceLanguage {
    UNKNOWN = 0,
    CPP,
    MSL,
    GLSL,
    /* Same as GLSL but enable partial C++ feature support like template, references,
     * include system, etc ... */
    BLENDER_GLSL,
  };

  /* Cannot use `__` because of some compilers complaining about reserved symbols. */
  static constexpr const char *namespace_separator = "_";
  /* Add a prefix to all member functions so that they are not clashing with local variables. */
  static constexpr const char *method_call_prefix = "_";
  static constexpr const char *linted_struct_suffix = "_host_shared_";

  static SourceLanguage language_from_filename(const std::string &filename)
  {
    if (filename.find(".msl") != std::string::npos) {
      return MSL;
    }
    if (filename.find(".glsl") != std::string::npos ||
        filename.find(".bsl.hh") != std::string::npos)
    {
      return GLSL;
    }
    if (filename.find(".hh") != std::string::npos) {
      return CPP;
    }
    return UNKNOWN;
  }

  /* Takes a whole source file and output processed source. */
  std::string process(SourceLanguage language,
                      std::string str,
                      const std::string &filepath,
                      bool do_parse_function,
                      report_callback report_error,
                      metadata::Source &r_metadata)
  {
    if (language == UNKNOWN) {
      report_error(0, 0, "", "Unknown file type");
      return "";
    }

    const std::string filename = std::regex_replace(filepath, std::regex(R"((?:.*)\/(.*))"), "$1");

    str = remove_comments(str, report_error);
    if (language == BLENDER_GLSL || language == CPP) {
      str = disabled_code_mutation(str, report_error);
    }
    else {
      str = cleanup_whitespace(str, report_error);
    }
    str = threadgroup_variables_parse_and_remove(str, report_error);
    if (language == BLENDER_GLSL || language == CPP) {
      {
        parse_builtins(str, filename);
        Parser parser(str, report_error);

        /* Preprocessor directive parsing & linting. */
        if (language == BLENDER_GLSL) { /* TODO(fclem): Enforce in C++ header too. */
          lint_pragma_once(parser, filename, report_error);
        }
        parse_pragma_runtime_generated(parser);
        parse_includes(parser, report_error);
        parse_defines(parser, report_error);
        parse_legacy_create_info(parser, report_error);
        if (do_parse_function) {
          parse_library_functions(parser, report_error);
        }

        lower_preprocessor(parser, report_error);

        parser.apply_mutations();

        if (parser.str().find("\n#pragma no_processing") != std::string::npos) {
          /* Early out for certain files. */
          r_metadata = metadata;
          return line_directive_prefix(filename) + parser.result_get();
        }

        /* Lower high level parsing complexity.
         * Merge tokens that can be combined together,
         * remove the token that are unsupported or that are noop.
         * All these steps should be independent. */
        lower_attribute_sequences(parser, report_error);
        lower_strings_sequences(parser, report_error);
        lower_swizzle_methods(parser, report_error);
        lower_classes(parser, report_error);
        lower_noop_keywords(parser, report_error);
        lower_trailing_comma_in_list(parser, report_error);

        parser.apply_mutations();

        /* Linting phase. Detect valid syntax with invalid usage. */
        lint_unbraced_statements(parser, report_error);
        lint_reserved_tokens(parser, report_error);
        lint_attributes(parser, report_error);
        lint_global_scope_constants(parser, report_error);
        lint_constructors(parser, report_error);
        lint_forward_declared_structs(parser, report_error);

        /* Lint and remove C++ accessor templates before lowering template. */
        lower_srt_accessor_templates(parser, report_error);
        lower_union_accessor_templates(parser, report_error);
        /* Lower templates. */
        lower_template_dependent_names(parser, report_error);
        lower_templates(parser, report_error);
        /* Lower namespaces. */
        lower_using(parser, report_error);
        lower_namespaces(parser, report_error);
        lower_scope_resolution_operators(parser, report_error);
        /* Lower unions and then lint shared structures. */
        lower_unions(parser, report_error);
        lower_host_shared_structures(parser, report_error);
        /* Lower enums. */
        lower_enums(parser, report_error);
        /* Lower SRT and Interfaces. */
        lower_entry_points(parser, report_error);
        lower_pipeline_definition(parser, filename, report_error);
        lower_resource_table(parser, report_error);
        lower_resource_access_functions(parser, report_error);
        /* Lower class methods. */
        lower_function_default_arguments(parser, report_error);
        lower_implicit_member(parser, report_error);
        lower_method_definitions(parser, report_error);
        lower_method_calls(parser, report_error);
        lower_empty_struct(parser, report_error);
        /* Lower SRT accesses. */
        lower_srt_member_access(parser, report_error);
        lower_entry_points_signature(parser, report_error);
        lower_stage_function(parser, report_error);
        lower_srt_arguments(parser, report_error);
        /* Lower string, assert, printf. */
        lower_assert(parser, filename, report_error);
        lower_strings(parser, report_error);
        lower_printf(parser, report_error);
        /* Lower other C++ constructs. */
        lower_implicit_return_types(parser, report_error);
        lower_initializer_implicit_types(parser, report_error);
        lower_designated_initializers(parser, report_error);
        lower_aggregate_initializers(parser, report_error);
        lower_array_initializations(parser, report_error);
        lower_scope_resolution_operators(parser, report_error);
        /* Lower references. */
        lower_reference_arguments(parser, report_error);
        lower_reference_variables(parser, report_error);
        /* Lower control flow. */
        lower_static_branch(parser, report_error);
        /* Unroll last to avoid processing more tokens in other phases. */
        lower_loop_unroll(parser, report_error);

        /* GLSL syntax compatibility.
         * TODO(fclem): Remove. */
        lower_argument_qualifiers(parser, report_error);

        /* Cleanup to make output more human readable and smaller for runtime. */
        cleanup_whitespace(parser, report_error);
        cleanup_empty_lines(parser, report_error);
        cleanup_line_directives(parser, report_error);
        str = parser.result_get();
      }

      str = line_directive_prefix(filename) + str;
      r_metadata = metadata;
      return str;
    }

    if (language == MSL) {
      Parser parser(str, report_error);
      parse_pragma_runtime_generated(parser);
      parse_includes(parser, report_error);
      lower_preprocessor(parser, report_error);
      str = parser.result_get();
    }
    if (language == GLSL) {
      parse_builtins(str, filename, true);
#ifdef __APPLE__ /* Limiting to Apple hardware since GLSL compilers might have issues. */
      str = matrix_constructor_mutation(str);
#endif
    }
    str = argument_decorator_macro_injection(str);
    str = array_constructor_macro_injection(str);
    str = line_directive_prefix(filename) + str;
    r_metadata = metadata;
    return str;
  }

  /* Variant use for python shaders. */
  std::string process(const std::string &str, metadata::Source &r_metadata)
  {
    auto no_err_report = [](int, int, std::string, const char *) {};
    return process(GLSL, str, "", false, no_err_report, r_metadata);
  }

 private:
  std::string remove_comments(const std::string &str, const report_callback &report_error)
  {
    std::string out_str = str;
    {
      /* Multi-line comments. */
      size_t start, end = 0;
      while ((start = out_str.find("/*", end)) != std::string::npos) {
        end = out_str.find("*/", start + 2);
        if (end == std::string::npos) {
          break;
        }
        for (size_t i = start; i < end + 2; ++i) {
          if (out_str[i] != '\n') {
            out_str[i] = ' ';
          }
        }
      }

      if (end == std::string::npos) {
        report_error(parser::line_number(out_str, start),
                     parser::char_number(out_str, start),
                     parser::line_str(out_str, start),
                     "Malformed multi-line comment.");
        return out_str;
      }
    }
    {
      /* Single-line comments. */
      size_t start, end = 0;
      while ((start = out_str.find("//", end)) != std::string::npos) {
        end = out_str.find('\n', start + 2);
        if (end == std::string::npos) {
          break;
        }
        for (size_t i = start; i < end; ++i) {
          out_str[i] = ' ';
        }
      }

      if (end == std::string::npos) {
        report_error(parser::line_number(out_str, start),
                     parser::char_number(out_str, start),
                     parser::line_str(out_str, start),
                     "Malformed single line comment, missing newline.");
        return out_str;
      }
    }
    return out_str;
  }

  /* Remove trailing white spaces. */
  void cleanup_whitespace(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

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

  /* Safer version without Parser. */
  std::string cleanup_whitespace(const std::string &str, const report_callback & /*report_error*/)
  {
    /* Remove trailing white space as they make the subsequent regex much slower. */
    std::regex regex(R"((\ )*?\n)");
    return std::regex_replace(str, regex, "\n");
  }

  static std::string template_arguments_mangle(const shader::parser::Scope template_args)
  {
    using namespace std;
    using namespace shader::parser;

    string args_concat;
    template_args.foreach_scope(ScopeType::TemplateArg,
                                [&](const Scope &scope) { args_concat += 'T' + scope.str(); });
    return args_concat;
  }

  void parse_template_definition(const parser::Scope arg,
                                 std::vector<std::string> &arg_list,
                                 const parser::Scope fn_args,
                                 bool &all_template_args_in_function_signature,
                                 report_callback &report_error)
  {
    using namespace std;
    using namespace shader::parser;
    const Token type = arg.front();
    const Token name = type.next();
    const string name_str = name.str();
    const string type_str = type.str();

    arg_list.emplace_back(name_str);

    if (arg.contains_token('=')) {
      report_error(ERROR_TOK(arg[0]),
                   "Default arguments are not supported inside template declaration");
    }

    if (type_str == "typename") {
      bool found = false;
      /* Search argument list for type-names. If type-name matches, the template argument is
       * present inside the function signature. */
      fn_args.foreach_match("ww", [&](const std::vector<Token> &tokens) {
        if (tokens[0].str() == name_str) {
          found = true;
        }
      });
      all_template_args_in_function_signature &= found;
    }
    else if (type_str == "enum" || type_str == "bool") {
      /* Values cannot be resolved using type deduction. */
      all_template_args_in_function_signature = false;
    }
    else if (type_str == "int" || type_str == "uint") {
      /* Values cannot be resolved using type deduction. */
      all_template_args_in_function_signature = false;
    }
    else {
      report_error(ERROR_TOK(type), "Invalid template argument type");
    }
  }

  void process_instantiation(Parser &parser,
                             const std::vector<parser::Token> &toks,
                             const parser::Scope &parent_scope,
                             const parser::Token &fn_start,
                             const parser::Token &fn_name,
                             const std::vector<std::string> &arg_list,
                             const std::string &fn_decl,
                             const bool all_template_args_in_function_signature,
                             report_callback &report_error)
  {
    using namespace std;
    using namespace shader::parser;
    if (toks[2].scope() != parent_scope || fn_name.str() != toks[2].str() ||
        toks[2].str_index_start() < fn_name.str_index_start())
    {
      return;
    }

    const Scope inst_args = toks[3].scope();
    const Token inst_start = toks[0];
    const Token inst_end = toks[0].find_next(SemiColon);

    /* Parse template values. */
    vector<pair<string, string>> arg_name_value_pairs;
    int i = 0;
    toks[3].scope().foreach_scope(ScopeType::TemplateArg, [&](const Scope &arg) {
      if (i < arg_list.size()) {
        arg_name_value_pairs.emplace_back(arg_list[i], arg.str());
      }
      i++;
    });
    if (i != arg_list.size()) {
      report_error(ERROR_TOK(toks[3]), "Invalid amount of argument in template instantiation.");
    }

    /* Specialize template content. */
    Parser instance_parser(fn_decl, report_error, true);
    instance_parser().foreach_token(Word, [&](const Token &word) {
      string token_str = word.str();
      for (const auto &arg_name_value : arg_name_value_pairs) {
        if (token_str == arg_name_value.first) {
          instance_parser.replace(word, arg_name_value.second);
        }
      }
    });

    if (!all_template_args_in_function_signature) {
      /* Append template args after function name.
       * `void func() {}` > `void func<a, 1>() {}`. */
      size_t pos = fn_decl.find(" " + fn_name.str());
      instance_parser.insert_after(pos + fn_name.str().size(),
                                   template_arguments_mangle(inst_args));
    }
    /* Paste template content in place of instantiation. */
    string instance = instance_parser.result_get();
    parser.erase(inst_start, inst_end);
    parser.insert_line_number(inst_end, fn_start.line_number());
    parser.insert_after(inst_end, instance);
    parser.insert_line_number(inst_end, inst_end.line_number(true));
  }

  /**
   * Given our codestyle, we don't need the disambiguation.
   * Example: `x.template foo<int>()` > `x.foo<int>()`
   */
  void lower_template_dependent_names(Parser &parser, report_callback & /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("tw<..>", [&](const Tokens &toks) {
      if (toks[0].prev() == '.' || toks[0].prev() == Deref) {
        parser.erase(toks[0]);
      }
    });
    parser.apply_mutations();
  }

  void lower_templates(Parser &parser, report_callback &report_error)
  {
    using namespace std;
    using namespace shader::parser;

    /* Process templated function calls first to avoid matching them later. */

    parser().foreach_match("w<..>(..)", [&](const vector<Token> &tokens) {
      const Scope template_args = tokens[1].scope();
      template_args.foreach_match("w<..>", [&parser](const vector<Token> &tokens) {
        parser.replace(tokens[1].scope(), template_arguments_mangle(tokens[1].scope()), true);
      });
    });
    parser.apply_mutations();

    /* Then Specialization. */

    auto process_specialization = [&](const Token specialization_start,
                                      const Scope template_args) {
      parser.erase(specialization_start, specialization_start.next().next());
      parser.replace(template_args, template_arguments_mangle(template_args), true);
    };
    /* Replace full specialization by simple functions. */
    parser().foreach_match("t<>ww<", [&](const std::vector<Token> &tokens) {
      process_specialization(tokens[0], tokens[5].scope());
    });
    /* Replace full specialization by simple struct. */
    parser().foreach_match("t<>sw<..>", [&](const std::vector<Token> &tokens) {
      process_specialization(tokens[0], tokens[5].scope());
    });

    parser.apply_mutations();

    auto process_template_struct = [&](parser::Scope template_scope) {
      /* Parse template declaration. */
      Token struct_start = template_scope.back().next();
      if (struct_start != Struct) {
        return;
      }
      Token struct_name = struct_start.next();
      Scope struct_body = struct_name.next().scope();

      Token struct_end = struct_body.back().next();
      const string struct_decl = parser.substr_range_inclusive(struct_start, struct_end);

      vector<string> arg_list;
      bool all_template_args_in_function_signature = false;
      template_scope.foreach_scope(ScopeType::TemplateArg, [&](Scope arg) {
        parse_template_definition(arg,
                                  arg_list,
                                  Scope::invalid(),
                                  all_template_args_in_function_signature,
                                  report_error);
      });

      /* Remove declaration. */
      Token template_keyword = template_scope.front().prev();
      parser.erase(template_keyword, struct_end);

      /* Replace instantiations. */
      Scope parent_scope = template_scope.scope();
      parent_scope.foreach_match("tsw<", [&](const std::vector<Token> &tokens) {
        process_instantiation(parser,
                              tokens,
                              parent_scope,
                              struct_start,
                              struct_name,
                              arg_list,
                              struct_decl,
                              all_template_args_in_function_signature,
                              report_error);
      });
    };

    parser().foreach_scope(ScopeType::Template, process_template_struct);
    parser().foreach_scope(ScopeType::Namespace, [&](Scope ns_scope) {
      ns_scope.foreach_scope(ScopeType::Template, process_template_struct);
    });
    parser.apply_mutations();

    auto process_template_function = [&](const Token fn_start,
                                         const Token fn_name,
                                         const Scope fn_args,
                                         const Scope template_scope,
                                         const Token fn_end) {
      bool error = false;
      template_scope.foreach_match("=", [&](const std::vector<Token> &tokens) {
        report_error(tokens[0].line_number(),
                     tokens[0].char_number(),
                     tokens[0].line_str(),
                     "Default arguments are not supported inside template declaration");
        error = true;
      });
      if (error) {
        return;
      }

      vector<string> arg_list;
      bool all_template_args_in_function_signature = true;
      template_scope.foreach_scope(ScopeType::TemplateArg, [&](Scope arg) {
        parse_template_definition(
            arg, arg_list, fn_args, all_template_args_in_function_signature, report_error);
      });

      const string fn_decl = parser.substr_range_inclusive(fn_start, fn_end);

      /* Remove declaration. */
      Token template_keyword = template_scope.front().prev();
      parser.erase(template_keyword, fn_end);

      /* Replace instantiations. */
      Scope parent_scope = template_scope.scope();
      parent_scope.foreach_match("tww<", [&](const std::vector<Token> &tokens) {
        process_instantiation(parser,
                              tokens,
                              parent_scope,
                              fn_start,
                              fn_name,
                              arg_list,
                              fn_decl,
                              all_template_args_in_function_signature,
                              report_error);
      });
    };

    parser().foreach_match("t<..>ww(..)c?{..}", [&](const vector<Token> &tokens) {
      process_template_function(
          tokens[5], tokens[6], tokens[7].scope(), tokens[1].scope(), tokens[16]);
    });

    parser.apply_mutations();

    /* Check if there is no remaining declaration and instantiation that were not processed. */
    parser().foreach_token(Template, [&](Token tok) {
      if (tok.next() == '<') {
        report_error(ERROR_TOK(tok), "Template declaration unsupported syntax");
      }
      else {
        report_error(ERROR_TOK(tok), "Template instantiation unsupported syntax");
      }
    });

    /* Process calls to templated types or functions. */
    parser().foreach_match("w<..>", [&](const std::vector<Token> &tokens) {
      parser.replace(tokens[1].scope(), template_arguments_mangle(tokens[1].scope()), true);
    });

    parser.apply_mutations();
  }

  /* Parse defines in order to output them with the create infos.
   * This allow the create infos to use shared defines values. */
  void parse_defines(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;
    parser().foreach_match("#w", [&](const std::vector<Token> &tokens) {
      if (tokens[1].str() == "define") {
        metadata.create_infos_defines.emplace_back(tokens[1].next().scope().str_with_whitespace());
      }
      if (tokens[1].str() == "undef") {
        metadata.create_infos_defines.emplace_back(tokens[1].next().scope().str_with_whitespace());
      }
    });
  }

  std::string get_create_info_placeholder(const std::string &name)
  {
    std::string placeholder;
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
  void parse_legacy_create_info(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

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
      parser.insert_line_number(struct_keyword.str_index_start() - 1,
                                struct_keyword.line_number());
    });

    parser().foreach_match("w(..)", [&](const std::vector<Token> &tokens) {
      if (tokens[0].str() == "CREATE_INFO_VARIANT") {
        const string variant_name = tokens[1].scope().front().next().str();
        metadata.create_infos.emplace_back(variant_name);

        const string variant_decl = parser.substr_range_inclusive(tokens.front(), tokens.back());
        metadata.create_infos_declarations.emplace_back(variant_decl);

        parser.replace(tokens.front(), tokens.back(), get_create_info_placeholder(variant_name));
        return;
      }
      if (tokens[0].str() == "GPU_SHADER_CREATE_INFO") {
        const string variant_name = tokens[1].scope().front().next().str();
        metadata.create_infos.emplace_back(variant_name);

        const size_t start_end = tokens.back().str_index_last();
        const string end_tok = "GPU_SHADER_CREATE_END()";
        const size_t end_pos = parser.str().find(end_tok, start_end);
        if (end_pos == string::npos) {
          report_error(ERROR_TOK(tokens[0]), "Missing create info end.");
          return;
        }

        const string variant_decl = parser.substr_range_inclusive(tokens.front().str_index_start(),
                                                                  end_pos + end_tok.size());
        metadata.create_infos_declarations.emplace_back(variant_decl);

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
          report_error(ERROR_TOK(tokens[0]), "Missing create info end.");
          return;
        }

        end_pos = parser.str().find(')', end_pos);
        if (end_pos == string::npos) {
          report_error(ERROR_TOK(tokens[0]), "Missing parenthesis at info end.");
          return;
        }

        const string variant_decl = parser.substr_range_inclusive(tokens.front().str_index_start(),
                                                                  end_pos);
        metadata.create_infos_declarations.emplace_back(variant_decl);

        parser.erase(tokens.front().str_index_start(), end_pos);
        return;
      }
      if (tokens[0].str() == "GPU_SHADER_INTERFACE_INFO") {
        const size_t start_end = tokens.back().str_index_last();
        const string end_str = "GPU_SHADER_INTERFACE_END()";
        size_t end_pos = parser.str().find(end_str, start_end);
        if (end_pos == string::npos) {
          report_error(ERROR_TOK(tokens[0]), "Missing create info end.");
          return;
        }
        const string variant_decl = parser.substr_range_inclusive(tokens.front().str_index_start(),
                                                                  end_pos + end_str.size());
        metadata.create_infos_declarations.emplace_back(variant_decl);

        parser.erase(tokens.front().str_index_start(), end_pos + end_str.size());
        return;
      }
    });

    parser.apply_mutations();
  }

  void parse_includes(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("#w_", [&](const std::vector<Token> &tokens) {
      if (tokens[1].str() != "include") {
        return;
      }
      string dependency_name = tokens[2].str_exclusive();

      if (dependency_name.find("defines.hh") != string::npos) {
        /* Dependencies between create infos are not needed for reflections.
         * Only the dependencies on the defines are needed. */
        metadata.create_infos_dependencies.emplace_back(dependency_name);
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
      if (dependency_name.find("gpu_shader_create_info.hh") != std::string::npos) {
        /* Skip info files. They are only for IDE linting. */
        parser.erase(tokens.front(), tokens.back());
        return;
      }

      if (dependency_name.find("infos/") != std::string::npos) {
        dependency_name = dependency_name.substr(6);
      }

      metadata.dependencies.emplace_back(dependency_name);
    });
  }

  void parse_pragma_runtime_generated(Parser &parser)
  {
    if (parser.str().find("\n#pragma runtime_generated") != std::string::npos) {
      metadata.builtins.emplace_back(metadata::Builtin::runtime_generated);
    }
  }

  void lint_pragma_once(Parser &parser, const std::string &filename, report_callback report_error)
  {
    if (filename.find("_lib.") == std::string::npos && filename.find(".hh") == std::string::npos) {
      return;
    }
    if (parser.str().find("\n#pragma once") == std::string::npos) {
      report_error(0, 0, "", "Header files must contain #pragma once directive.");
    }
  }

  void lower_loop_unroll(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    auto parse_for_args =
        [&](const Scope loop_args, Scope &r_init, Scope &r_condition, Scope &r_iter) {
          r_init = r_condition = r_iter = Scope::invalid();
          loop_args.foreach_scope(ScopeType::LoopArg, [&](const Scope arg) {
            if (arg.front().prev() == '(' && arg.back().next() == ';') {
              r_init = arg;
            }
            else if (arg.front().prev() == ';' && arg.back().next() == ';') {
              r_condition = arg;
            }
            else if (arg.front().prev() == ';' && arg.back().next() == ')') {
              r_iter = arg;
            }
            else {
              report_error(ERROR_TOK(arg.front()), "Invalid loop declaration.");
            }
          });
        };

    auto process_loop = [&](const Token loop_start,
                            const int iter_count,
                            const int iter_init,
                            const int iter_incr,
                            const bool condition_is_trivial,
                            const bool iteration_is_trivial,
                            const Scope init,
                            const Scope cond,
                            const Scope iter,
                            const Scope body,
                            const string body_prefix = "",
                            const string body_suffix = "") {
      /* Check that there is no unsupported keywords in the loop body. */
      bool error = false;
      /* Checks if `continue` exists, even in switch statement inside the unrolled loop. */
      body.foreach_token(Continue, [&](const Token token) {
        if (token.scope().first_scope_of_type(ScopeType::LoopBody) == body) {
          report_error(ERROR_TOK(token), "Unrolled loop cannot contain \"continue\" statement.");
          error = true;
        }
      });
      /* Checks if `break` exists directly the unrolled loop scope. Switch statements are ok. */
      body.foreach_token(Break, [&](const Token token) {
        if (token.scope().first_scope_of_type(ScopeType::LoopBody) == body) {
          const Scope switch_scope = token.scope().first_scope_of_type(ScopeType::SwitchBody);
          if (switch_scope.is_invalid() || !body.contains(switch_scope)) {
            report_error(ERROR_TOK(token), "Unrolled loop cannot contain \"break\" statement.");
            error = true;
          }
        }
      });
      if (error) {
        return;
      }

      if (!parser.replace_try(loop_start, body.back(), "", true)) {
        /* This is the case of nested loops. This loop will be processed in another parser pass. */
        return;
      }

      string indent_init, indent_cond, indent_iter;
      if (init.is_valid()) {
        indent_init = string(init.front().char_number() - 1, ' ');
      }
      if (cond.is_valid()) {
        indent_cond = string(cond.front().char_number() - 3, ' ');
      }
      if (iter.is_valid()) {
        indent_iter = string(iter.front().char_number(), ' ');
      }
      string indent_body = string(body.front().char_number(), ' ');
      string indent_end = string(body.back().char_number(), ' ');

      /* If possible, replaces the index of the loop iteration inside the given string. */
      auto replace_index = [&](const string &str, int loop_index) {
        if (iter.is_invalid() || !iteration_is_trivial || str.empty()) {
          return str;
        }
        Parser str_parser(str, report_error);
        str_parser().foreach_token(Word, [&](const Token tok) {
          if (tok.str() == iter[0].str()) {
            str_parser.replace(tok, std::to_string(loop_index), true);
          }
        });
        return str_parser.result_get();
      };

      parser.insert_after(body.back(), "\n");
      if (init.is_valid() && !iteration_is_trivial) {
        parser.insert_line_number(body.back(), init.front().line_number());
        parser.insert_after(body.back(), indent_init + "{" + init.str_with_whitespace() + ";\n");
      }
      else {
        parser.insert_after(body.back(), "{\n");
      }
      for (int64_t i = 0, value = iter_init; i < iter_count; i++, value += iter_incr) {
        if (cond.is_valid() && !condition_is_trivial) {
          parser.insert_line_number(body.back(), cond.front().line_number());
          parser.insert_after(body.back(),
                              indent_cond + "if(" + cond.str_with_whitespace() + ")\n");
        }
        parser.insert_after(body.back(), replace_index(body_prefix, value));
        parser.insert_line_number(body.back(), body.front().line_number());
        parser.insert_after(body.back(),
                            indent_body + replace_index(body.str_with_whitespace(), value) + "\n");
        parser.insert_after(body.back(), body_suffix);
        if (iter.is_valid() && !iteration_is_trivial) {
          parser.insert_line_number(body.back(), iter.front().line_number());
          parser.insert_after(body.back(), indent_iter + iter.str_with_whitespace() + ";\n");
        }
      }
      parser.insert_line_number(body.back(), body.back().line_number());
      parser.insert_after(body.back(), indent_end + body.back().str_with_whitespace());
    };

    do {
      /* [[unroll]]. */
      parser().foreach_match("[[w]]f(..){..}", [&](const std::vector<Token> tokens) {
        if (tokens[1].scope().str_with_whitespace() != "[unroll]") {
          return;
        }
        const Token for_tok = tokens[5];
        const Scope loop_args = tokens[6].scope();
        const Scope loop_body = tokens[10].scope();

        Scope init, cond, iter;
        parse_for_args(loop_args, init, cond, iter);

        /* Init statement. */
        const Token var_type = init[0];
        const Token var_name = init[1];
        const Token var_init = init[2];
        if (var_type.str() != "int" && var_type.str() != "uint") {
          report_error(ERROR_TOK(var_init), "Can only unroll integer based loop.");
          return;
        }
        if (var_init != '=') {
          report_error(ERROR_TOK(var_init), "Expecting assignment here.");
          return;
        }
        if (init[3] != '0' && init[3] != '-') {
          report_error(ERROR_TOK(init[3]), "Expecting integer literal here.");
          return;
        }

        /* Conditional statement. */
        const Token cond_var = cond[0];
        const Token cond_type = cond[1];
        const Token cond_sign = (cond[2] == '+' || cond[2] == '-') ? cond[2] : Token::invalid();
        const Token cond_end = cond_sign.is_valid() ? cond[3] : cond[2];
        if (cond_var.str() != var_name.str()) {
          report_error(ERROR_TOK(cond_var), "Non matching loop counter variable.");
          return;
        }
        if (cond_end != '0') {
          report_error(ERROR_TOK(cond_end), "Expecting integer literal here.");
          return;
        }

        /* Iteration statement. */
        const Token iter_var = iter[0];
        const Token iter_type = iter[1];
        const Token iter_end = iter[1];
        int iter_incr = 0;
        if (iter_var.str() != var_name.str()) {
          report_error(ERROR_TOK(iter_var), "Non matching loop counter variable.");
          return;
        }
        if (iter_type == Increment) {
          iter_incr = +1;
          if (cond_type == '>') {
            report_error(ERROR_TOK(for_tok), "Unsupported condition in unrolled loop.");
            return;
          }
        }
        else if (iter_type == Decrement) {
          iter_incr = -1;
          if (cond_type == '<') {
            report_error(ERROR_TOK(for_tok), "Unsupported condition in unrolled loop.");
            return;
          }
        }
        else {
          report_error(ERROR_TOK(iter_type), "Unsupported loop expression. Expecting ++ or --.");
          return;
        }

        int64_t init_value = std::stol(
            parser.substr_range_inclusive(var_init.next(), var_init.scope().back()));
        int64_t end_value = std::stol(
            parser.substr_range_inclusive(cond_sign.is_valid() ? cond_sign : cond_end, cond_end));
        /* TODO(fclem): Support arbitrary strides (aka, arbitrary iter statement). */
        int iter_count = std::abs(end_value - init_value);
        if (cond_type == GEqual || cond_type == LEqual) {
          iter_count += 1;
        }

        bool condition_is_trivial = (cond_end == cond.back());
        bool iteration_is_trivial = (iter_end == iter.back());

        process_loop(tokens[0],
                     iter_count,
                     init_value,
                     iter_incr,
                     condition_is_trivial,
                     iteration_is_trivial,
                     init,
                     cond,
                     iter,
                     loop_body);
      });

      /* [[unroll(n)]]. */
      parser().foreach_match("[[w(0)]]f(..){..}", [&](const std::vector<Token> tokens) {
        if (tokens[2].str() != "unroll") {
          return;
        }
        const Scope loop_args = tokens[9].scope();
        const Scope loop_body = tokens[13].scope();

        Scope init, cond, iter;
        parse_for_args(loop_args, init, cond, iter);

        int iter_count = std::stol(tokens[4].str());

        process_loop(tokens[0], iter_count, 0, 0, false, false, init, cond, iter, loop_body);
      });

      /* [[unroll_define(max_n)]]. */
      parser().foreach_match("[[w(0)]]f(..){..}", [&](const std::vector<Token> tokens) {
        if (tokens[2].str() != "unroll_define") {
          return;
        }
        const Scope loop_args = tokens[9].scope();
        const Scope loop_body = tokens[13].scope();

        /* Validate format. */
        Token define_name = Token::invalid();
        Token iter_var = Token::invalid();
        loop_args.foreach_match("ww=0;w<w;wP", [&](const std::vector<Token> tokens) {
          if (tokens[1].str() != tokens[5].str() || tokens[5].str() != tokens[9].str()) {
            return;
          }
          iter_var = tokens[1];
          define_name = tokens[7];
        });

        if (define_name.is_invalid()) {
          report_error(ERROR_TOK(loop_args.front()),
                       "Incompatible loop format for [[unroll_define(max_n)]], expected "
                       "'(int i = 0; i < DEFINE; i++)'");
          return;
        }

        Scope init, cond, iter;
        parse_for_args(loop_args, init, cond, iter);

        int iter_count = std::stol(tokens[4].str());

        string body_prefix = "#if " + define_name.str() + " > " + iter_var.str() + "\n";

        process_loop(tokens[0],
                     iter_count,
                     0,
                     1,
                     true,
                     true,
                     init,
                     cond,
                     iter,
                     loop_body,
                     body_prefix,
                     "#endif\n");
      });
    } while (parser.apply_mutations());

    /* Check for remaining keywords. */
    parser().foreach_match("[[w", [&](const std::vector<Token> tokens) {
      if (tokens[2].str().find("unroll") != string::npos) {
        report_error(ERROR_TOK(tokens[0]), "Incompatible loop format for [[unroll]].");
      }
    });
  }

  void process_static_branch(Parser &parser,
                             shader::parser::Token if_tok,
                             shader::parser::Scope condition,
                             shader::parser::Token attribute,
                             shader::parser::Scope body,
                             report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    if (attribute.str() != "static_branch") {
      return;
    }

    if (condition.str().find("&&") != string::npos || condition.str().find("||") != string::npos) {
      report_error(ERROR_TOK(condition[0]), "Expecting single condition.");
      return;
    }

    if (condition[1].str() != "srt_access") {
      report_error(ERROR_TOK(if_tok), "Expecting compilation or specialization constant.");
      return;
    }

    Token before_body = body.front().prev();

    string test = "SRT_CONSTANT_" + condition[5].str();
    if (condition[7] != condition.back().prev()) {
      test += parser.substr_range_inclusive(condition[7], condition.back().prev());
    }
    string directive = (if_tok.prev() == Else ? "#elif " : "#if ");

    parser.insert_directive(before_body, directive + test);
    parser.erase(if_tok, before_body);

    if (body.back().next() == Else) {
      Token else_tok = body.back().next();
      parser.erase(else_tok);
      if (else_tok.next() == If) {
        /* Will be processed later. */
        Token next_if = else_tok.next();
        /* Ensure the rest of the if clauses also have the attribute. */
        Scope attributes = next_if.next().scope().back().next().scope();
        if (attributes.type() != ScopeType::Subscript ||
            attributes.front().next().scope().str_exclusive() != "static_branch")
        {
          report_error(ERROR_TOK(next_if),
                       "Expecting next if statement to also be a static branch.");
          return;
        }
        return;
      }
      body = else_tok.next().scope();

      parser.insert_directive(else_tok, "#else");
    }
    parser.insert_directive(body.back(), "#endif");
  };

  void lower_static_branch(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("i(..)[[w]]{..}", [&](const std::vector<Token> &tokens) {
      process_static_branch(
          parser, tokens[0], tokens[1].scope(), tokens[7], tokens[10].scope(), report_error);
    });
    parser.apply_mutations();
  }

  /* Lower namespaces by adding namespace prefix to all the contained structs and functions. */
  void lower_namespaces(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    /* Parse each namespace declaration. */
    parser().foreach_scope(ScopeType::Namespace, [&](const Scope &scope) {
      /* TODO(fclem): This could be supported using multiple passes. */
      scope.foreach_match("n", [&](const std::vector<Token> &tokens) {
        report_error(ERROR_TOK(tokens[0]), "Nested namespaces are unsupported.");
      });

      string prefix = scope.front().prev().full_symbol_name();

      auto process_symbol = [&](const Token &symbol) {
        if (symbol.next() == '<') {
          /* Template instantiation or specialization. */
          return;
        }
        /* Replace all occurrences of the non-namespace specified symbol. */
        scope.foreach_token(Word, [&](const Token &token) {
          if (token.str() != symbol.str()) {
            return;
          }
          /* Reject symbols that already have namespace specified. */
          if (token.namespace_start() != token) {
            return;
          }
          /* Reject method calls. */
          if (token.prev() == '.') {
            return;
          }
          parser.replace(token, prefix + namespace_separator + token.str(), true);
        });
      };

      unordered_set<string> processed_functions;

      scope.foreach_function([&](bool, Token, Token fn_name, Scope, bool, Scope) {
        if (fn_name.scope().type() == ScopeType::Struct) {
          /* Don't process functions inside a struct scope as the namespace must not be apply
           * to them, but to the type. Otherwise, method calls will not work. */
          return;
        }
        if (processed_functions.count(fn_name.str())) {
          /* Don't process function names twice. Can happen with overloads. */
          return;
        }
        processed_functions.emplace(fn_name.str());
        process_symbol(fn_name);
      });
      scope.foreach_struct(
          [&](Token, Scope, Token struct_name, Scope) { process_symbol(struct_name); });

      /* Pipeline declarations. */
      scope.foreach_match("ww(w", [&](vector<Token> toks) {
        if (toks[0].scope().type() != ScopeType::Namespace || toks[0].str().find("Pipeline") != 0)
        {
          return;
        }
        process_symbol(toks[1]);
      });

      Token namespace_tok = scope.front().prev().namespace_start().prev();
      if (namespace_tok == Namespace) {
        parser.erase(namespace_tok, scope.front());
        parser.erase(scope.back());
      }
      else {
        report_error(ERROR_TOK(namespace_tok), "Expected namespace token.");
      }
    });

    parser.apply_mutations();
  }

  /**
   * Needs to run before namespace mutation so that `using` have more precedence.
   * Otherwise the following would fail.
   *  ```cpp
   *  namespace B {
   *  int test(int a) {}
   *  }
   *
   *  namespace A {
   *  int test(int a) {}
   *  int func(int a) {
   *    using B::test;
   *    return test(a); // Should reference B::test and not A::test
   *  }
   *  ```
   */
  void lower_using(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("un", [&](const std::vector<Token> &tokens) {
      report_error(ERROR_TOK(tokens[0]),
                   "Unsupported `using namespace`. "
                   "Add individual `using` directives for each needed symbol.");
    });

    auto process_using = [&](const Token &using_tok,
                             const Token &from,
                             const Token &to_start,
                             const Token &to_end,
                             const Token &end_tok) {
      string to = parser.substr_range_inclusive(to_start, to_end);
      string namespace_prefix = parser.substr_range_inclusive(to_start,
                                                              to_end.prev().prev().prev());
      Scope scope = from.scope();

      /* Using the keyword in global or at namespace scope. */
      if (scope.type() == ScopeType::Global) {
        report_error(ERROR_TOK(using_tok), "The `using` keyword is not allowed in global scope.");
        return;
      }
      if (scope.type() == ScopeType::Namespace) {
        /* Ensure we are bringing symbols from the same namespace.
         * Otherwise we can have different shadowing outcome between shader and C++. */
        string namespace_name = scope.front().prev().full_symbol_name();
        if (namespace_name != namespace_prefix) {
          report_error(
              ERROR_TOK(using_tok),
              "The `using` keyword is only allowed in namespace scope to make visible symbols "
              "from the same namespace declared in another scope, potentially from another "
              "file.");
          return;
        }
      }

      /* Assignments do not allow to alias functions symbols. */
      const bool use_alias = from.str() != to_end.str();
      const bool replace_fn = !use_alias;
      /** IMPORTANT: If replace_fn is true, this can replace any symbol type if there are functions
       * and types with the same name. We could support being more explicit about the type of
       * symbol to replace using an optional attribute [[gpu::using_function]]. */

      /* Replace all occurrences of the non-namespace specified symbol. */
      scope.foreach_token(Word, [&](const Token &token) {
        /* Do not replace symbols before the using statement. */
        if (token.index <= to_end.index) {
          return;
        }
        /* Reject symbols that contain the target symbol name. */
        if (token.prev() == ':') {
          return;
        }
        if (!replace_fn && token.next() == '(') {
          return;
        }
        if (token.str() != from.str()) {
          return;
        }
        parser.replace(token, to, true);
      });

      parser.erase(using_tok, end_tok);
    };

    parser().foreach_match("uw::w", [&](const std::vector<Token> &tokens) {
      Token end = tokens.back().find_next(SemiColon);
      process_using(tokens[0], end.prev(), tokens[1], end.prev(), end);
    });

    parser().foreach_match("uw=w::w", [&](const std::vector<Token> &tokens) {
      Token end = tokens.back().find_next(SemiColon);
      process_using(tokens[0], tokens[1], tokens[3], end.prev(), end);
    });

    parser.apply_mutations();

    /* Verify all using were processed. */
    parser().foreach_token(Using, [&](const Token &token) {
      report_error(ERROR_TOK(token), "Unsupported `using` keyword usage.");
    });
  }

  void lower_scope_resolution_operators(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("::", [&](const std::vector<Token> &tokens) {
      if (tokens[0].scope().type() == ScopeType::Attribute) {
        return;
      }
      if (tokens[0].prev() != Word) {
        /* Global namespace reference. */
        parser.erase(tokens.front(), tokens.back());
      }
      else {
        /* Specific namespace reference. */
        parser.replace(tokens.front(), tokens.back(), namespace_separator);
      }
    });
    parser.apply_mutations();
  }

  std::string disabled_code_mutation(const std::string &str, report_callback &report_error)
  {
    using namespace std;
    using namespace shader::parser;

    Parser parser(str, report_error);

    auto process_disabled_scope = [&](Token start_tok) {
      /* Search for endif with the same indentation. Assume formatted input. */
      string end_str = start_tok.str_with_whitespace() + "endif";
      size_t scope_end = parser.str().find(end_str, start_tok.str_index_start());
      if (scope_end == string::npos) {
        report_error(ERROR_TOK(start_tok), "Couldn't find end of disabled scope.");
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

    parser().foreach_match("#ww", [&](const std::vector<Token> &tokens) {
      if (tokens[1].str() == "ifndef" && tokens[2].str() == "GPU_SHADER") {
        process_disabled_scope(tokens[0]);
      }
    });
    parser().foreach_match("#i!w(w)", [&](const std::vector<Token> &tokens) {
      if (tokens[1].str() == "if" && tokens[3].str() == "defined" &&
          tokens[5].str() == "GPU_SHADER")
      {
        process_disabled_scope(tokens[0]);
      }
    });
    parser().foreach_match("#i0", [&](const std::vector<Token> &tokens) {
      if (tokens[1].str() == "if" && tokens[2].str() == "0") {
        process_disabled_scope(tokens[0]);
      }
    });
    return parser.result_get();
  }

  void lower_preprocessor(Parser &parser, report_callback /*report_error*/)
  {
    /* Remove unsupported directives. */
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("#w", [&](const std::vector<Token> &tokens) {
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
  void lower_swizzle_methods(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    /* Change C++ swizzle functions into plain swizzle. */
    /** IMPORTANT: This prevent the usage of any method with a swizzle name. */
    parser().foreach_match(".w()", [&](const std::vector<Token> &tokens) {
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

  std::string threadgroup_variables_parse_and_remove(const std::string &str,
                                                     report_callback &report_error)
  {
    using namespace std;
    using namespace shader::parser;

    Parser parser(str, report_error);

    auto process_shared_var = [&](Token shared_tok, Token type, Token name, Token decl_end) {
      if (shared_tok.str() == "shared") {
        metadata.shared_variables.push_back(
            {type.str(), parser.substr_range_inclusive(name, decl_end.prev())});

        parser.erase(shared_tok, decl_end);
      }
    };
    parser().foreach_match("www;", [&](const std::vector<Token> &tokens) {
      process_shared_var(tokens[0], tokens[1], tokens[2], tokens.back());
    });
    parser().foreach_match("www[..];", [&](const std::vector<Token> &tokens) {
      process_shared_var(tokens[0], tokens[1], tokens[2], tokens.back());
    });
    parser().foreach_match("www[..][..];", [&](const std::vector<Token> &tokens) {
      process_shared_var(tokens[0], tokens[1], tokens[2], tokens.back());
    });
    parser().foreach_match("www[..][..][..];", [&](const std::vector<Token> &tokens) {
      process_shared_var(tokens[0], tokens[1], tokens[2], tokens.back());
    });
    /* If more array depth is needed, find a less dumb solution. */

    return parser.result_get();
  }

  void parse_library_functions(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;
    using namespace metadata;

    parser().foreach_function([&](bool, Token fn_type, Token fn_name, Scope fn_args, bool, Scope) {
      /* Only match void function with parameters. */
      if (fn_type.str() != "void" || fn_args.token_count() <= 3) {
        return;
      }
      /* Reject main function. */
      if (fn_name.str() == "main") {
        return;
      }
      FunctionFormat fn;
      fn.name = fn_name.str();

      fn_args.foreach_scope(ScopeType::FunctionArg, [&](Scope arg) {
        /* Note: There is no array support. */
        const Token name = arg.back();
        const Token type = name.prev() == '&' ? name.prev().prev() : name.prev();
        std::string qualifier = type.prev().str();
        if (qualifier != "out" && qualifier != "inout" && qualifier != "in") {
          if (name.prev() == '&') {
            qualifier = "out";
          }
          else if (qualifier != "const" && qualifier != "(" && qualifier != ",") {
            report_error(ERROR_TOK(type.prev()),
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

      metadata.functions.emplace_back(fn);
    });
  }

  void parse_builtins(const std::string &str, const std::string &filename, bool pure_glsl = false)
  {
    const bool skip_drw_debug = filename == "draw_debug_draw_lib.glsl" ||
                                filename == "draw_debug_infos.hh" ||
                                filename == "draw_debug_draw_display_vert.glsl" ||
                                filename == "draw_shader_shared.hh";
    using namespace metadata;
    /* TODO: This can trigger false positive caused by disabled #if blocks. */
    std::vector<std::string> tokens = {
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
      if (str.find(token) != std::string::npos) {
        metadata.builtins.emplace_back(Builtin(hash(token)));
      }
    }
  }

  /* Change printf calls to "recursive" call to implementation functions.
   * This allows to emulate the variadic arguments of printf. */
  void lower_printf(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;
    parser().foreach_match("w(..)", [&](const vector<Token> &tokens) {
      if (tokens[0].str() != "printf") {
        return;
      }

      int arg_count = 0;
      tokens[1].scope().foreach_scope(ScopeType::FunctionParam,
                                      [&](const Scope &) { arg_count++; });

      string unrolled = "print_start(" + to_string(arg_count) + ")";
      tokens[1].scope().foreach_scope(ScopeType::FunctionParam, [&](const Scope &attribute) {
        unrolled = "print_data(" + unrolled + ", " + attribute.str() + ")";
      });

      parser.replace(tokens.front(), tokens.back(), unrolled);
    });
    parser.apply_mutations();
  }

  /* Turn assert into a printf. */
  void lower_assert(Parser &parser, const std::string &filename, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

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
    (void)report_error;
#endif
    parser.apply_mutations();
  }

  /* String hash are outputted inside GLSL and needs to fit 32 bits. */
  static uint32_t hash_string(const std::string &str)
  {
    uint64_t hash_64 = metadata::hash(str);
    uint32_t hash_32 = uint32_t(hash_64 ^ (hash_64 >> 32));
    return hash_32;
  }

  /* Parse SRT and interfaces, remove their attributes and create init function for SRT structs. */
  void lower_resource_table(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    enum class SrtType {
      undefined,
      none,
      resource_table,
      vertex_input,
      vertex_output,
      fragment_output,
    };

    auto parse_resource = [&](Scope attributes, Token type, Token name, Scope array) {
      metadata::ParsedResource resource{
          type.line_number(), type.str(), name.str(), array.str_with_whitespace()};
      attributes.foreach_scope(ScopeType::Attribute, [&](const Scope &attribute) {
        std::string type = attribute[0].str();
        if (type == "sampler") {
          resource.res_type = type;
          resource.res_slot = attribute[2].str();
        }
        else if (type == "image") {
          resource.res_type = type;
          resource.res_slot = attribute[2].str();
          resource.res_qualifier = attribute[4].str();
          resource.res_format = attribute[6].str();
        }
        else if (type == "uniform") {
          resource.res_type = type;
          resource.res_slot = attribute[2].str();
        }
        else if (type == "storage") {
          resource.res_type = type;
          resource.res_slot = attribute[2].str();
          resource.res_qualifier = attribute[4].str();
        }
        else if (type == "push_constant") {
          resource.res_type = type;
        }
        else if (type == "compilation_constant") {
          resource.res_type = type;
        }
        else if (type == "specialization_constant") {
          resource.res_type = type;
          resource.res_value = attribute[2].str();
        }
        else if (type == "condition") {
          attribute[1].scope().foreach_token(Word, [&](const Token tok) {
            resource.res_condition += "int " + tok.str() + " = ";
            resource.res_condition += "ShaderCreateInfo::find_constant(constants, \"" + tok.str() +
                                      "\"); ";
          });
          resource.res_condition += "return " + attribute[1].scope().str() + ";";
        }
        else if (type == "frequency") {
          resource.res_frequency = attribute[2].str();
        }
        else if (type == "resource_table") {
          resource.res_type = type;
        }
        else if (type == "legacy_info") {
          resource.res_type = type;
        }
        else {
          report_error(ERROR_TOK(attribute[0]), "Invalid attribute in resource table");
        }
      });
      return resource;
    };

    auto parse_vertex_input = [&](Scope attributes, Token type, Token name, Scope array) {
      if (array.is_valid()) {
        report_error(ERROR_TOK(array[0]), "Array are not supported as vertex attributes");
      }

      metadata::ParsedVertInput vert_in{type.line_number(), type.str(), name.str()};

      if (vert_in.var_type == "float3x3" || vert_in.var_type == "float2x2" ||
          vert_in.var_type == "float4x4" || vert_in.var_type == "float3x4")
      {
        report_error(ERROR_TOK(name), "Matrices are not supported as vertex attributes");
      }

      attributes.foreach_scope(ScopeType::Attribute, [&](const Scope &attribute) {
        std::string type = attribute[0].str();
        if (type == "attribute") {
          vert_in.slot = attribute[2].str();
        }
        else {
          report_error(ERROR_TOK(attribute[0]), "Invalid attribute in vertex input interface");
        }
      });
      return vert_in;
    };

    auto parse_vertex_output =
        [&](Token struct_name, Scope attributes, Token type, Token name, Scope array) {
          if (array.is_valid()) {
            report_error(ERROR_TOK(array[0]), "Array are not supported in stage interface");
          }

          Token interpolation_mode = attributes[1];

          metadata::ParsedAttribute attr{type.line_number(),
                                         type.str(),
                                         struct_name.str() + "_" + name.str(),
                                         interpolation_mode.str()};

          if (attr.var_type == "float3x3" || attr.var_type == "float2x2" ||
              attr.var_type == "float4x4" || attr.var_type == "float3x4")
          {
            report_error(ERROR_TOK(name), "Matrices are not supported in stage interface");
          }

          if (attr.interpolation_mode != "smooth" && attr.interpolation_mode != "flat" &&
              attr.interpolation_mode != "no_perspective")
          {
            report_error(ERROR_TOK(attributes[0]), "Invalid attribute in shader stage interface");
          }
          return attr;
        };

    auto parse_fragment_output =
        [&](Token struct_name, Scope attributes, Token tok_type, Token name, Scope) {
          metadata::ParsedFragOuput frag_out{
              tok_type.line_number(), tok_type.str(), struct_name.str() + "_" + name.str()};

          attributes.foreach_scope(ScopeType::Attribute, [&](const Scope &attribute) {
            std::string type = attribute[0].str();
            if (type == "frag_color") {
              frag_out.slot = attribute[2].str();
            }
            else if (type == "raster_order_group") {
              frag_out.raster_order_group = attribute[2].str();
            }
            else if (type == "index") {
              frag_out.dual_source = attribute[2].str();
            }
            else {
              report_error(ERROR_TOK(attributes[0]),
                           "Invalid attribute in fragment output interface");
            }
          });
          return frag_out;
        };

    auto is_resource_table_attribute = [](Token attr) {
      string type = attr.str();
      return (type == "sampler" || type == "image" || type == "uniform" || type == "storage" ||
              type == "push_constant" || type == "compilation_constant" ||
              type == "compilation_constant" || type == "legacy_info" || type == "resource_table");
    };
    auto is_vertex_input_attribute = [](Token attr) {
      string type = attr.str();
      return (type == "attribute");
    };
    auto is_vertex_output_attribute = [](Token attr) {
      string type = attr.str();
      return (type == "flat" || type == "smooth" || type == "no_perspective");
    };
    auto is_fragment_output_attribute = [](Token attr) {
      string type = attr.str();
      return (type == "frag_color" || type == "frag_depth" || type == "frag_stencil_ref");
    };

    parser().foreach_struct([&](Token struct_tok, Scope, Token struct_name, Scope body) {
      SrtType srt_type = SrtType::undefined;
      bool has_srt_members = false;

      metadata::ResourceTable srt;
      metadata::VertexInputs vertex_in;
      metadata::StageInterface vertex_out;
      metadata::FragmentOutputs fragment_out;
      srt.name = struct_name.str();
      vertex_in.name = struct_name.str();
      vertex_out.name = struct_name.str();
      fragment_out.name = struct_name.str();

      body.foreach_declaration([&](Scope attributes,
                                   Token const_tok,
                                   Token type,
                                   Scope /*template_scope TODO */,
                                   Token name,
                                   Scope array,
                                   Token decl_end) {
        SrtType decl_type = SrtType::undefined;
        if (attributes.is_invalid()) {
          decl_type = SrtType::none;
        }
        else if (is_resource_table_attribute(attributes[1])) {
          decl_type = SrtType::resource_table;
        }
        else if (is_vertex_input_attribute(attributes[1])) {
          decl_type = SrtType::vertex_input;
        }
        else if (is_vertex_output_attribute(attributes[1])) {
          decl_type = SrtType::vertex_output;
        }
        else if (is_fragment_output_attribute(attributes[1])) {
          decl_type = SrtType::fragment_output;
        }
        else {
          return;
        }

        if (srt_type == SrtType::undefined) {
          srt_type = decl_type;
        }
        else if (srt_type != decl_type) {
          switch (srt_type) {
            case SrtType::resource_table:
              report_error(ERROR_TOK(struct_name), "Structure expected to contain resources...");
              break;
            case SrtType::vertex_input:
              report_error(ERROR_TOK(struct_name),
                           "Structure expected to contain vertex inputs...");
              break;
            case SrtType::vertex_output:
              report_error(ERROR_TOK(struct_name),
                           "Structure expected to contain vertex outputs...");
              break;
            case SrtType::fragment_output:
              report_error(ERROR_TOK(struct_name),
                           "Structure expected to contain fragment inputs...");
              break;
            case SrtType::none:
              report_error(ERROR_TOK(struct_name), "Structure expected to contain plain data...");
              break;
            case SrtType::undefined:
              break;
          }

          switch (decl_type) {
            case SrtType::resource_table:
              report_error(ERROR_TOK(attributes[1]), "...but member declared as resource.");
              break;
            case SrtType::vertex_input:
              report_error(ERROR_TOK(attributes[1]), "...but member declared as vertex input.");
              break;
            case SrtType::vertex_output:
              report_error(ERROR_TOK(attributes[1]), "...but member declared as vertex output.");
              break;
            case SrtType::fragment_output:
              report_error(ERROR_TOK(attributes[1]), "...but member declared as fragment output.");
              break;
            case SrtType::none:
              report_error(ERROR_TOK(name), "...but member declared as plain data.");
              break;
            case SrtType::undefined:
              break;
          }
        }

        switch (decl_type) {
          case SrtType::resource_table:
            srt.emplace_back(parse_resource(attributes, type, name, array));
            if (attributes[1].str() == "resource_table") {
              has_srt_members = true;
              parser.erase(attributes.scope());
              parser.erase(const_tok);
            }
            else {
              parser.erase(attributes.front().line_start(), decl_end.line_end());
            }
            break;
          case SrtType::vertex_input:
            vertex_in.emplace_back(parse_vertex_input(attributes, type, name, array));
            parser.erase(attributes.scope());
            break;
          case SrtType::vertex_output:
            vertex_out.emplace_back(
                parse_vertex_output(struct_name, attributes, type, name, array));
            parser.erase(attributes.scope());
            break;
          case SrtType::fragment_output:
            fragment_out.emplace_back(
                parse_fragment_output(struct_name, attributes, type, name, array));
            parser.erase(attributes.scope());
            break;
          case SrtType::undefined:
          case SrtType::none:
            break;
        }
      });

      switch (srt_type) {
        case SrtType::resource_table:
          metadata.resource_tables.emplace_back(srt);
          break;
        case SrtType::vertex_input:
          metadata.vertex_inputs.emplace_back(vertex_in);
          break;
        case SrtType::vertex_output:
          metadata.stage_interfaces.emplace_back(vertex_out);
          break;
        case SrtType::fragment_output:
          metadata.fragment_outputs.emplace_back(fragment_out);
          break;
        case SrtType::undefined:
        case SrtType::none:
          break;
      }

      Token end_of_srt = body.back().prev();

      if (srt_type == SrtType::resource_table) {
        /* Add static constructor.
         * These are only to avoid warnings on certain backend compilers. */
        string ctor;
        ctor += "\nstatic " + srt.name + " new_()\n";
        ctor += "{\n";
        ctor += "  " + srt.name + " result;\n";
        if (has_srt_members == false) {
          ctor += "  result._pad = 0;\n";
        }
        for (const auto &member : srt) {
          if (member.res_type == "resource_table") {
            ctor += "  result." + member.var_name + " = " + member.var_type + "::new_();\n";
          }
        }
        ctor += "  return result;\n";
        /* Avoid messing up the line count and keep empty struct empty. */
        ctor += "#line " + to_string(end_of_srt.line_number()) + "\n";
        ctor += "}\n";
        parser.insert_after(end_of_srt, ctor);

        string access_macros;
        for (const auto &member : srt) {
          if (member.res_type == "resource_table") {
            access_macros += "#define access_" + srt.name + "_" + member.var_name + "() ";
            access_macros += member.var_type + "::new_()\n";
          }
          else {
            access_macros += "#define access_" + srt.name + "_" + member.var_name + "() ";
            access_macros += member.var_name + "\n";
          }
        }
        parser.insert_before(struct_tok, access_macros);
        parser.insert_before(struct_tok, get_create_info_placeholder(srt.name));

        parser.insert_before(struct_tok, "\n");
        parser.insert_line_number(struct_tok.str_index_start() - 1, struct_tok.line_number());

        /* Insert attribute so that method mutations know that this struct is an SRT. */
        parser.insert_before(struct_tok, "[[resource_table]] ");
      }
    });
    parser.apply_mutations();
  }

  void lower_strings_sequences(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    do {
      parser().foreach_match("__", [&](const std::vector<Token> &tokens) {
        string first = tokens[0].str();
        string second = tokens[1].str();
        string between = parser.substr_range_inclusive(
            tokens[0].str_index_last_no_whitespace() + 1, tokens[1].str_index_start() - 1);
        string trailing = parser.substr_range_inclusive(
            tokens[1].str_index_last_no_whitespace() + 1, tokens[1].str_index_last());
        string merged = first.substr(0, first.length() - 1) + second.substr(1) + between +
                        trailing;
        parser.replace_try(tokens[0], tokens[1], merged);
      });
    } while (parser.apply_mutations());
  }

  /* Replace string literals by their hash and store the original string in the file metadata. */
  void lower_strings(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_token(String, [&](const Token &token) {
      uint32_t hash = hash_string(token.str());
      metadata::PrintfFormat format = {hash, token.str()};
      metadata.printf_formats.emplace_back(format);
      parser.replace(token, "string(" + std::to_string(hash) + "u)", true);
    });
    parser.apply_mutations();
  }

  /* `class` -> `struct` */
  void lower_classes(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;
    parser().foreach_token(Class, [&](const Token &token) {
      if (token.prev() != Enum) {
        parser.replace(token, "struct ");
      }
    });
  }

  /* Make all members of a class to be referenced using `this->`. */
  void lower_implicit_member(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_struct([&](Token, Scope, Token, Scope body) {
      vector<Token> members_tokens;
      vector<Token> methods_tokens;

      auto is_class_token = [&](const vector<Token> &members, const string &token) {
        for (const Token &member : members) {
          if (token == member.str()) {
            return true;
          }
        }
        return false;
      };

      auto check_shadowing = [&](const Tokens &toks) {
        if (is_class_token(members_tokens, toks[1].str())) {
          report_error(ERROR_TOK(toks[1]), "Class member shadowing.");
        }
      };

      body.foreach_declaration([&](Scope, Token, Token, Scope, Token name, Scope, Token) {
        if (name.scope() == body) {
          members_tokens.emplace_back(name);
        }
      });

      body.foreach_function(
          [&](bool is_static, Token, Token fn_name, Scope fn_args, bool, Scope fn_body) {
            if (is_static) {
              return;
            }
            fn_args.foreach_match("ww", check_shadowing);
            fn_args.foreach_match("&w", check_shadowing);
            fn_body.foreach_match("ww", check_shadowing);
            fn_body.foreach_match("&w", check_shadowing);
            methods_tokens.emplace_back(fn_name);
          });

      body.foreach_function([&](bool is_static, Token, Token, Scope, bool, Scope fn_body) {
        if (is_static) {
          return;
        }
        fn_body.foreach_token(Word, [&](Token tok) {
          if (tok.prev() != Deref && tok.prev() != Dot && tok.prev() != Colon) {
            if (tok.next() == '(') {
              if (!is_class_token(methods_tokens, tok.str())) {
                return;
              }
            }
            else {
              if (!is_class_token(members_tokens, tok.str())) {
                return;
              }
            }
            parser.insert_before(tok, "this->");
          }
        });
      });
    });

    parser.apply_mutations();
  }

  /* Move all method definition outside of struct definition blocks. */
  void lower_method_definitions(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    /* `*this` -> `this_` */
    parser().foreach_match("*T", [&](const Tokens &t) { parser.replace(t[0], t[1], "this_"); });
    /* `this->` -> `this_.` */
    parser().foreach_match("TD", [&](const Tokens &t) { parser.replace(t[0], t[1], "this_."); });

    parser.apply_mutations();

    parser().foreach_match("sw:", [&](const Tokens &toks) {
      if (toks[2] == ':') {
        report_error(ERROR_TOK(toks[2]), "class inheritance is not supported");
        return;
      }
    });

    parser().foreach_match("cww(..)c?{..}", [&](const Tokens &toks) {
      if (toks[0].prev() == Const) {
        report_error(ERROR_TOK(toks[0]),
                     "function return type is marked `const` but it makes no sense for values "
                     "and returning reference is not supported");
        return;
      }
    });

    /* Add `this` parameter and fold static keywords into function name. */
    parser().foreach_struct([&](Token struct_tok,
                                Scope,
                                const Token struct_name,
                                const Scope struct_scope) {
      const Scope attributes = struct_tok.prev().scope();
      const bool is_resource_table = (attributes.type() == ScopeType::Subscript) &&
                                     (attributes.str() == "[[resource_table]]");

      if (is_resource_table) {
        parser.replace(attributes, "");
      }

      struct_scope.foreach_function(
          [&](bool is_static, Token fn_type, Token fn_name, Scope fn_args, bool is_const, Scope) {
            const Token static_tok = is_static ? fn_type.prev() : Token::invalid();
            const Token const_tok = is_const ? fn_args.back().next() : Token::invalid();

            if (fn_name.str()[0] == '_') {
              report_error(ERROR_TOK(fn_name),
                           "function name starting with an underscore are reserved");
            }

            if (is_static) {
              parser.replace(fn_name, struct_name.str() + namespace_separator + fn_name.str());
              /* WORKAROUND: Erase the static keyword as it conflicts with the wrapper class
               * member accesses MSL. */
              parser.erase(static_tok);
            }
            else {
              const bool has_no_args = fn_args.token_count() == 2;
              const char *suffix = (has_no_args ? "" : ", ");
              const string prefix = (is_resource_table ? "[[resource_table]] " : "");

              /* Add a prefix to all member functions. */
              parser.insert_before(fn_name, method_call_prefix);

              if (is_const && !is_resource_table) {
                parser.erase(const_tok);
                parser.insert_after(fn_args.front(),
                                    prefix + "const " + struct_name.str() + " this_" + suffix);
              }
              else {
                parser.insert_after(fn_args.front(),
                                    prefix + struct_name.str() + " &this_" + suffix);
              }

              if (fn_name.str().length() > 1 &&
                  (fn_name.str().find_first_not_of("xyzw") == string::npos ||
                   fn_name.str().find_first_not_of("rgba") == string::npos))
              {
                report_error(ERROR_TOK(fn_name),
                             "Method name matching swizzles accessor are forbidden.");
              }
            }
          });
    });

    parser.apply_mutations();

    /* Copy method functions outside of struct scope. */
    parser().foreach_struct([&](Token, Scope, const Token, const Scope struct_scope) {
      const Token struct_end = struct_scope.back().next();

      bool has_methods = false;
      struct_scope.foreach_function(
          [&](bool, Token, Token, Scope, bool, Scope) { has_methods = true; });
      if (!has_methods) {
        /* Avoid uneeded preprocessor directives. */
        return;
      }

      /* First output prototypes. Not needed on metal because of wrapper class. */
      parser.insert_after(struct_end, "#ifndef GPU_METAL\n");
      struct_scope.foreach_function(
          [&](bool is_static, Token fn_type, Token, Scope fn_args, bool, Scope) {
            const Token fn_start = is_static ? fn_type.prev() : fn_type;

            string proto_str = parser.substr_range_inclusive(fn_start, fn_args.back());
            proto_str = Preprocessor::strip_whitespace(proto_str) + ";\n";
            Parser proto(proto_str, report_error);

            /* Remove [[resource_table]] and other attributes that could create issues. */
            proto().foreach_match("[[", [&](Tokens toks) { proto.replace(toks[0].scope(), ""); });

            parser.insert_after(struct_end, proto.result_get());
          });
      parser.insert_after(struct_end, "#endif\n");

      struct_scope.foreach_function(
          [&](bool is_static, Token fn_type, Token, Scope, bool, Scope fn_body) {
            const Token fn_start = is_static ? fn_type.prev() : fn_type;

            string fn_str = parser.substr_range_inclusive(fn_start, fn_body.back());
            fn_str = string(fn_start.char_number(), ' ') + fn_str;

            parser.erase(fn_start, fn_body.back());
            parser.insert_line_number(struct_end, fn_start.line_number());
            parser.insert_after(struct_end, fn_str);
          });

      parser.insert_line_number(struct_end, struct_end.line_number(true));
    });

    parser.apply_mutations();
  }

  /* Add padding member to empty structs.
   * Empty structs are useful for templating. */
  void lower_empty_struct(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("sw{};", [&](const std::vector<Token> &tokens) {
      parser.insert_after(tokens[2], "int _pad;");
    });
    parser.apply_mutations();
  }

  /* Transform `a.fn(b)` into `fn(a, b)`. */
  void lower_method_calls(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    do {
      parser().foreach_scope(ScopeType::Function, [&](Scope scope) {
        scope.foreach_match(".w(", [&](const std::vector<Token> &tokens) {
          const Token dot = tokens[0];
          const Token func = tokens[1];
          const Token par_open = tokens[2];
          const Token end_of_this = dot.prev();
          Token start_of_this = end_of_this;
          while (true) {
            if (start_of_this == ')') {
              /* Function call. Take argument scope and function name. No recursion. */
              start_of_this = start_of_this.scope().front().prev();
              break;
            }
            if (start_of_this == ']') {
              /* Array subscript. Take scope and continue. */
              start_of_this = start_of_this.scope().front().prev();
              continue;
            }
            if (start_of_this == Word) {
              /* Member. */
              if (start_of_this.prev() == '.') {
                start_of_this = start_of_this.prev().prev();
                /* Continue until we find root member. */
                continue;
              }
              /* End of chain. */
              break;
            }
            report_error(start_of_this.line_number(),
                         start_of_this.char_number(),
                         start_of_this.line_str(),
                         "lower_method_call parsing error");
            break;
          }
          string this_str = parser.substr_range_inclusive(start_of_this, end_of_this);
          string func_str = method_call_prefix + func.str();
          const bool has_no_arg = par_open.next() == ')';
          /* `a.fn(b)` -> `_fn(a, b)` */
          parser.replace_try(
              start_of_this, par_open, func_str + "(" + this_str + (has_no_arg ? "" : ", "));
        });
      });
    } while (parser.apply_mutations());
  }

  /* Parse, convert to create infos, and erase declaration. */
  void lower_pipeline_definition(Parser &parser,
                                 const std::string &filename,
                                 report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;
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
        scope.foreach_match(".w=w", process_constant);
        scope.foreach_match(".w=0", process_constant);
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

      metadata.create_infos_declarations.emplace_back(create_info_decl);
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

      metadata.create_infos_declarations.emplace_back(create_info_decl);
    };

    parser().foreach_match("ww(w", [&](const std::vector<Token> &tokens) {
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

  void lower_stage_function(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_function(
        [&](bool is_static, Token fn_type, Token, Scope, bool, Scope fn_body) {
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

  /* Add #ifdef directive around functions using SRT arguments.
   * Need to run after `lower_entry_points_signature`. */
  void lower_srt_arguments(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    /* SRT arguments. */
    parser().foreach_function([&](bool, Token fn_type, Token, Scope fn_args, bool, Scope fn_body) {
      string condition;
      fn_args.foreach_match("[[w]]c?w", [&](const std::vector<Token> &tokens) {
        if (tokens[2].str() != "resource_table") {
          return;
        }
        condition += "defined(CREATE_INFO_" + tokens[7].str() + ")";
        parser.replace(tokens[0].scope(), "");
      });

      if (!condition.empty()) {
        parser.insert_directive(fn_type.prev(), "#if " + condition);
        parser.insert_directive(fn_body.back(), "#endif");
      }
    });

    parser.apply_mutations();
  }

  /* Add ifdefs guards around scopes using resource accessors. */
  void lower_resource_access_functions(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    /* Legacy access macros. */
    parser().foreach_function([&](bool, Token fn_type, Token, Scope, bool, Scope fn_body) {
      fn_body.foreach_match("w(w,", [&](const std::vector<Token> &tokens) {
        string func_name = tokens[0].str();
        if (func_name != "specialization_constant_get" && func_name != "shared_variable_get" &&
            func_name != "push_constant_get" && func_name != "interface_get" &&
            func_name != "attribute_get" && func_name != "buffer_get" &&
            func_name != "sampler_get" && func_name != "image_get")
        {
          return;
        }
        string info_name = tokens[2].str();
        Scope scope = tokens[0].scope();
        /* We can be in expression scope. Take parent scope until we find a local scope. */
        while (scope.type() != ScopeType::Function && scope.type() != ScopeType::Local) {
          scope = scope.scope();
        }

        string condition = "defined(CREATE_INFO_" + info_name + ")";

        if (scope.type() == ScopeType::Function) {
          guarded_scope_mutation(parser, scope, condition, fn_type);
        }
        else {
          guarded_scope_mutation(parser, scope, condition);
        }
      });
    });

    parser.apply_mutations();
  }

  void guarded_scope_mutation(Parser &parser,
                              parser::Scope scope,
                              const std::string &condition,
                              parser::Token fn_type = parser::Token::invalid())
  {
    using namespace std;
    using namespace shader::parser;

    string line_start = "#line " + std::to_string(scope.front().next().line_number()) + "\n";
    string line_end = "#line " + std::to_string(scope.back().line_number()) + "\n";

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
      guard_else += "  return " + type + (is_trivial ? "(0)" : "::zero()") + ";\n";
    }
    string guard_end = "#endif";

    parser.insert_directive(scope.front(), guard_start);
    parser.insert_directive(scope.back().prev(), guard_else + guard_end);
  };

  void lower_enums(Parser &parser, report_callback report_error)
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
    using namespace std;
    using namespace shader::parser;

    auto missing_underlying_type = [&](vector<Token> tokens) {
      report_error(tokens[0].line_number(),
                   tokens[0].char_number(),
                   tokens[0].line_str(),
                   "enum declaration must explicitly use an underlying type");
    };

    parser().foreach_match("Mw{", missing_underlying_type);
    parser().foreach_match("MSw{", missing_underlying_type);

    const string placeholder_value = "=__auto__";

    auto placeholder = [&](Scope enum_scope) {
      const string &value = placeholder_value;
      const string start = " = 0" + string(enum_scope.front().prev().str()[0] == 'u' ? "u" : "");

      auto insert = [&](Token name, const string &replacement) {
        if (name.next() == ',' || name.next() == '}') {
          parser.insert_after(name, replacement);
        }
      };
      enum_scope.foreach_match("{w", [&](const Tokens &t) { insert(t[1], start); });
      enum_scope.foreach_match(",w", [&](const Tokens &t) { insert(t[1], value); });
    };

    parser().foreach_match("MSw:w{", [&](const Tokens &t) { placeholder(t[5].scope()); });
    parser().foreach_match("Mw:w{", [&](const Tokens &t) { placeholder(t[4].scope()); });
    parser().foreach_match("MS[[w]]w:w{", [&](const Tokens &t) { placeholder(t[10].scope()); });
    parser().foreach_match("M[[w]]w:w{", [&](const Tokens &t) { placeholder(t[9].scope()); });

    parser.apply_mutations();

    auto process_enum = [&](Token enum_tok,
                            Token class_tok,
                            Token enum_name,
                            Token enum_type,
                            Scope enum_scope,
                            const bool is_host_shared) {
      string type_str = enum_type.str();

      string previous_value = "error_invalid_first_value";
      enum_scope.foreach_scope(ScopeType::Assignment, [&](Scope scope) {
        Token name_tok = scope.front().prev();
        string name = name_tok.str();
        string value = scope.str();
        if (value == placeholder_value) {
          value = "= " + previous_value + " + 1" + (enum_type.str()[0] == 'u' ? "u" : "");
        }
        if (class_tok.is_valid()) {
          name = enum_name.str() + "::" + name;
        }
        string decl = "constant static constexpr " + type_str + " " + name + " " + value + ";\n";
        parser.insert_line_number(enum_tok.prev(), name_tok.line_number());
        parser.insert_after(enum_tok.prev(), decl);

        previous_value = name;
      });
      parser.insert_directive(enum_tok.prev(),
                              "#define " + enum_name.str() + " " + enum_type.str() + "\n");
      if (is_host_shared) {
        if (type_str != "uint32_t" && type_str != "int32_t") {
          report_error(
              ERROR_TOK(enum_type),
              "enum declaration must use uint32_t or int32_t underlying type for interface "
              "compatibility");
          return;
        }

        string define = "#define ";
        define += enum_name.str() + linted_struct_suffix + " " + enum_name.str() + "\n";
        parser.insert_directive(enum_tok.prev(), define);
      }
      parser.erase(enum_tok, enum_scope.back().next());
    };

    parser().foreach_match("MSw:w{", [&](vector<Token> tokens) {
      process_enum(tokens[0], tokens[1], tokens[2], tokens[4], tokens[5].scope(), false);
    });
    parser().foreach_match("Mw:w{", [&](vector<Token> tokens) {
      process_enum(tokens[0], Token::invalid(), tokens[1], tokens[3], tokens[4].scope(), false);
    });
    parser().foreach_match("MS[[w]]w:w{", [&](vector<Token> tokens) {
      process_enum(tokens[0], tokens[1], tokens[7], tokens[9], tokens[10].scope(), true);
    });
    parser().foreach_match("M[[w]]w:w{", [&](vector<Token> tokens) {
      process_enum(tokens[0], Token::invalid(), tokens[6], tokens[8], tokens[9].scope(), true);
    });

    parser.apply_mutations();

    parser().foreach_token(
        Enum, [&](Token tok) { report_error(ERROR_TOK(tok), "invalid enum declaration"); });
  }

  /* Merge attribute scopes. They are equivalent in the C++ standard.
   * This allow to simplify parsing later on.
   * `[[a]] [[b]]` > `[[a, b]]` */
  void lower_attribute_sequences(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    do {
      parser().foreach_match("[[..]][[..]]", [&](vector<Token> toks) {
        parser.insert_before(toks[4], ",");
        parser.erase(toks[4], toks[7]);
      });
    } while (parser.apply_mutations());
  }

  /* Lint host shared structure for padding and alignment.
   * Remove the [[host_shared]] attribute. */
  void lower_host_shared_structures(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

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
      if (comma.is_valid()) {
        report_error(
            ERROR_TOK(comma),
            "comma declaration is not supported in shared struct, expand to multiple definition");
        return;
      }

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
          report_error(ERROR_TOK(type), "Small types are forbidden in shader interfaces.");
        }
        else if (type_str == "float3") {
          report_error(ERROR_TOK(type), "use packed_float3 instead of float3 in shared structure");
        }
        else if (type_str == "uint3") {
          report_error(ERROR_TOK(type), "use packed_uint3 instead of uint3 in shared structure");
        }
        else if (type_str == "int3") {
          report_error(ERROR_TOK(type), "use packed_int3 instead of int3 in shared structure");
        }
        else if (type_str == "bool") {
          report_error(ERROR_TOK(type), "bool is not allowed in shared structure, use bool32_t");
        }
        else if (type_str == "float4x3") {
          report_error(ERROR_TOK(type), "float4x3 is not allowed in shared structure");
        }
        else if (type_str == "float3x3") {
          report_error(ERROR_TOK(type), "float3x3 is not allowed in shared structure");
        }
        else if (type_str == "float2x3") {
          report_error(ERROR_TOK(type), "float2x3 is not allowed in shared structure");
        }
        else if (type_str == "float4x2") {
          report_error(ERROR_TOK(type), "float4x2 is not allowed in shared structure");
        }
        else if (type_str == "float3x2") {
          report_error(ERROR_TOK(type), "float3x2 is not allowed in shared structure");
        }
        else if (type_str == "float2x2") {
          report_error(ERROR_TOK(type), "float2x2 is not allowed in shared structure");
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
          parser.replace(type, type.str() + linted_struct_suffix + " ");
        }
        else if (type.prev() == Struct) {
          /* Only 4 bytes enums are allowed. */
          type_info = {16, 16};
          /* Erase redundant struct keyword. */
          parser.erase(type.prev());
          /* Make sure that linted structs only contain other linted structs. */
          parser.replace(type, type.str() + linted_struct_suffix + " ");
        }
        else {
          report_error(ERROR_TOK(type),
                       "Unknown type, add 'enum' or 'struct' keyword before the type name");
          return;
        }

        size_t align = type_info.alignment - (offset % type_info.alignment);
        if (align != type_info.alignment) {
          string err = "Misaligned member, missing " + to_string(align) + " padding bytes";
          report_error(ERROR_TOK(type), err.c_str());
        }

        size_t array_size = 1;
        if (array.is_valid()) {
          if (array.token_count() == 3 && array[1] == Number) {
            try {
              array_size = std::stol(array[1].str());
            }
            catch (std::invalid_argument const & /*ex*/) {
              report_error(ERROR_TOK(array.front()),
                           "Invalid array size, expecting integer literal");
            }
          }
          else {
            /* Can be macro or expression. Assume value is multiple of 4. */
            array_size = 4;
          }
        };

        offset += type_info.size * array_size;
      });
      if (offset % 16 != 0) {
        string err = "Alignment issue, missing " + to_string(16 - (offset % 16)) +
                     " padding bytes";
        report_error(ERROR_TOK(struct_name), err.c_str());
      }
      /* Insert an alias to the type that will get referenced for shaders that enforce usage of
       * linted types. */
      parser.insert_directive(struct_keyword.prev(),
                              "#define " + struct_name.str() + linted_struct_suffix + " " +
                                  struct_name.str() + "\n");
    });
    parser.apply_mutations();
  }

  void lint_unbraced_statements(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    auto check_statement = [&](const Tokens &toks) {
      Token end_tok = toks.back();
      if (end_tok.next() == If || end_tok.prev() == '#') {
        return;
      }
      if (end_tok.next() == '[' && end_tok.next().next() == '[') {
        end_tok = end_tok.next().scope().back();
      }
      if (end_tok.next() != '{') {
        report_error(ERROR_TOK(end_tok), "Missing curly braces after flow control statement.");
      }
    };

    parser().foreach_match("i(..)", check_statement);
    parser().foreach_match("I", check_statement);
    parser().foreach_match("f(..)", check_statement);
    parser().foreach_match("F(..)", check_statement);
  }

  void lint_reserved_tokens(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    std::unordered_set<string> reserved_symbols = {
        "vec2",   "vec3",   "vec4",   "mat2x2", "mat2x3", "mat2x4", "mat3x2", "mat3x3",
        "mat3x4", "mat4x2", "mat4x3", "mat4x4", "mat2",   "mat3",   "mat4",   "ivec2",
        "ivec3",  "ivec4",  "uvec2",  "uvec3",  "uvec4",  "bvec2",  "bvec3",  "bvec4",
    };

    parser().foreach_token(Word, [&](Token tok) {
      if (reserved_symbols.find(tok.str()) != reserved_symbols.end()) {
        report_error(ERROR_TOK(tok), "Reserved GLSL token");
      }
    });
  }

  void lint_attributes(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_token(SquareOpen, [&](Token par_open) {
      if (par_open.next() != '[') {
        return;
      }
      Scope attributes = par_open.next().scope();
      bool invalid = false;
      attributes.foreach_attribute([&](Token attr, Scope attr_scope) {
        string attr_str = attr.str();
        if (attr_str == "base_instance" || attr_str == "clip_distance" ||
            attr_str == "compilation_constant" || attr_str == "compute" ||
            attr_str == "early_fragment_tests" || attr_str == "flat" || attr_str == "frag_coord" ||
            attr_str == "frag_stencil_ref" || attr_str == "fragment" ||
            attr_str == "front_facing" || attr_str == "global_invocation_id" || attr_str == "in" ||
            attr_str == "instance_id" || attr_str == "layer" ||
            attr_str == "local_invocation_id" || attr_str == "local_invocation_index" ||
            attr_str == "no_perspective" || attr_str == "num_work_groups" || attr_str == "out" ||
            attr_str == "point_coord" || attr_str == "point_size" || attr_str == "position" ||
            attr_str == "push_constant" || attr_str == "resource_table" || attr_str == "smooth" ||
            attr_str == "specialization_constant" || attr_str == "vertex_id" ||
            attr_str == "legacy_info" || attr_str == "vertex" || attr_str == "viewport_index" ||
            attr_str == "work_group_id" || attr_str == "maybe_unused" ||
            attr_str == "fallthrough" || attr_str == "nodiscard")
        {
          if (attr_scope.is_valid()) {
            report_error(ERROR_TOK(attr), "This attribute requires no argument");
            invalid = true;
          }
        }
        else if (attr_str == "attribute" || attr_str == "index" || attr_str == "frag_color" ||
                 attr_str == "frag_depth" || attr_str == "uniform" || attr_str == "condition" ||
                 attr_str == "sampler")
        {
          if (attr_scope.is_invalid()) {
            report_error(ERROR_TOK(attr), "This attribute requires 1 argument");
            invalid = true;
          }
        }
        else if (attr_str == "storage") {
          if (attr_scope.is_invalid()) {
            report_error(ERROR_TOK(attr), "This attribute requires 2 arguments");
            invalid = true;
          }
        }
        else if (attr_str == "image") {
          if (attr_scope.is_invalid()) {
            report_error(ERROR_TOK(attr), "This attribute requires 3 arguments");
            invalid = true;
          }
        }
        else if (attr_str == "local_size") {
          if (attr_scope.is_invalid()) {
            report_error(ERROR_TOK(attr), "This attribute requires at least 1 argument");
            invalid = true;
          }
        }
        else if (attr_str == "host_shared") {
          if (attributes.front().prev().prev() != Struct &&
              attributes.front().prev().prev() != Enum)
          {
            report_error(
                ERROR_TOK(attr),
                "host_shared attributes must be placed after a struct or an enum definition");
            invalid = true;
          }
          /* Placement already checked. */
          return;
        }
        else if (attr_str == "unroll" || attr_str == "unroll_define") {
          if (attributes.back().next().next() != For) {
            report_error(ERROR_TOK(attr),
                         "unroll attributes must be declared before a 'for' loop keyword");
            invalid = true;
          }
          /* Placement already checked. */
          return;
        }
        else if (attr_str == "static_branch") {
          if (attributes.front().prev().prev().scope().front().prev() != If) {
            report_error(ERROR_TOK(attr),
                         "[[static_branch]] attribute must be declared after a 'if' condition");
            invalid = true;
          }
          /* Placement already checked. */
          return;
        }
        else {
          std::cout << "attr_str " << attr_str << std::endl;
          report_error(ERROR_TOK(attr), "Unrecognized attribute");
          invalid = true;
          /* Attribute already invalid, don't check placement. */
          return;
        }

        if (attr_str == "fallthrough") {
          /* Placement is too complicated to check. C++ compilation should already have checked. */
          return;
        }

        Token prev_tok = attributes.front().prev().prev();
        if (prev_tok == '(' || prev_tok == '{' || prev_tok == ';' || prev_tok == ',' ||
            prev_tok == '}' || prev_tok == ')' || prev_tok == '\n' || prev_tok.is_invalid())
        {
          /* Placement is maybe correct. Could refine a bit more. */
        }
        else {
          report_error(ERROR_TOK(attr), "attribute must be declared at a start of a declaration");
          invalid = true;
        }
      });
      if (invalid) {
        /* Erase invalid attributes to avoid spawning more errors. */
        parser.erase(attributes.scope());
      }
    });
    parser.apply_mutations();
  }

  void lower_noop_keywords(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

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
        report_error(ERROR_TOK(tok), "Expecting colon ':' after access specifier");
      }
    };
    parser().foreach_token(Private, process_access);
    parser().foreach_token(Public, process_access);
  }

  void lower_trailing_comma_in_list(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match(",}", [&](const Tokens &t) { parser.erase(t[0]); });
  }

  void lower_implicit_return_types(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_function([&](bool, Token type, Token, Scope, bool, Scope fn_body) {
      fn_body.foreach_match("rw?{..};", [&](Tokens toks) {
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

  void lower_initializer_implicit_types(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    auto process_scope = [&](Scope s) {
      /* Auto insert equal. */
      s.foreach_match("ww{..}", [&](Tokens t) { parser.insert_before(t[2], " = " + t[0].str()); });
      /* Auto insert type. */
      s.foreach_match("ww={..}", [&](Tokens t) { parser.insert_before(t[3], t[0].str()); });
    };

    parser().foreach_scope(ScopeType::FunctionArg, process_scope);
    parser().foreach_scope(ScopeType::Function, process_scope);
    parser.apply_mutations();
  }

  void lower_designated_initializers(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    /* Transform to compatibility macro. */
    parser().foreach_match("w{.w=", [&](Tokens t) {
      if (t[0].prev() != '=' || t[0].prev().prev() != 'w') {
        report_error(ERROR_TOK(t[0]), "Designated initializers are only supported in assignments");
        return;
      }
      /* Lint for nested aggregates. */
      Token nested_aggregate_end = t[0].scope().find_token(BracketClose);
      if (nested_aggregate_end != t[3]) {
        Token nested_aggregate_start = nested_aggregate_end.scope().front();
        if (nested_aggregate_start.prev() != Word) {
          report_error(ERROR_TOK(nested_aggregate_start),
                       "Nested anonymous aggregate is not supported");
          return;
        }
      }
      Token assign_tok = t[0].prev();
      Token var = t[0].prev().prev();
      Scope aggrega = t[2].scope();

      parser.insert_before(assign_tok, ";");
      parser.erase(assign_tok, t[1]);
      aggrega.foreach_match(".w=", [&](Tokens t) {
        if (t[0].scope() != aggrega) {
          report_error(ERROR_TOK(t[0]), "Nested initializer lists are not supported");
          return;
        }
        parser.insert_before(t[0], var.str());
        Token value_end = t[2].scope().back();
        parser.insert_after(value_end, ";");
        if (value_end.next() == ',') {
          parser.erase(value_end.next());
        }
      });
      parser.erase(aggrega.back(), aggrega.back().next());

      /* TODO: Lint for vector/matrix type (unsafe aggregate). */
    });

    parser.apply_mutations();
  }

  /* Support for **full** aggregate initialization.
   * They are converted to default constructor for GLSL. */
  void lower_aggregate_initializers(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    std::unordered_set<string> builtin_types = {
        "float2",   "float3",   "float4",   "float2x2", "float2x3", "float2x4",
        "float3x2", "float3x3", "float3x4", "float4x2", "float4x3", "float4x4",
        "float2x2", "float3x3", "float4x4", "int2",     "int3",     "int4",
        "uint2",    "uint3",    "uint4",    "bool2",    "bool3",    "bool4",
    };

    do {
      /* Transform to compatibility macro. */
      parser().foreach_match("w{..}", [&](Tokens t) {
        if (t[0].prev() == Struct) {
          return;
        }
        if (t[1].scope().token_count() == 2) {
          report_error(ERROR_TOK(t[0]), "Empty brace initializer is not supported");
        }
        if (builtin_types.find(t[0].str()) != builtin_types.end()) {
          report_error(ERROR_TOK(t[0]),
                       "Aggregate is error prone for built-in vector and matrix types, use "
                       "constructors instead");
        }
        /* Lint for nested aggregates. */
        Token nested_aggregate_end = t[1].scope().find_token(BracketClose);
        if (nested_aggregate_end != t[4]) {
          Token nested_aggregate_start = nested_aggregate_end.scope().front();
          if (nested_aggregate_start.prev() != Word) {
            report_error(ERROR_TOK(nested_aggregate_start),
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
  void lower_array_initializations(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("ww[..]={..};", [&](vector<Token> toks) {
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
          report_error(ERROR_TOK(name_tok), "Array size must be greater than zero.");
        }
        parser.insert_after(array_scope[0], to_string(list_len));
      }
      else if (array_scope_tok_len == 3 && array_scope[1] == Number) {
        if (stol(array_scope[1].str()) == 0) {
          report_error(ERROR_TOK(name_tok), "Array size must be greater than zero.");
        }
      }

      /* Lint nested initializer list. */
      list_scope.foreach_token(BracketOpen, [&](Token tok) {
        if (tok != list_scope.front()) {
          report_error(ERROR_TOK(name_tok), "Nested initializer list is not supported.");
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

  static std::string strip_whitespace(const std::string &str)
  {
    return str.substr(0, str.find_last_not_of(" \n") + 1);
  }

  /**
   * Expand functions with default arguments to function overloads.
   * Expects formatted input and that function bodies are followed by newline.
   */
  void lower_function_default_arguments(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

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
              overload += fn_name.str() + '(' + args_decl + ")" +
                          string(fn_const ? " const" : "") + "\n";
              overload += "{\n";
              overload += "#line " + std::to_string(fn_type.line_number()) + "\n";
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
  void cleanup_line_directives(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("#w0\n", [&](vector<Token> toks) {
      if (toks[1].str() != "line") {
        return;
      }
      /* Workaround the foreach_match not matching overlapping patterns. */
      if (toks.back().next() == '#' && toks.back().next().next() == 'w' &&
          toks.back().next().next().next() == '0' &&
          toks.back().next().next().next().next() == '\n')
      {
        parser.replace(toks[0].line_start(), toks[0].line_end() + 1, "");
      }
    });
    parser.apply_mutations();

    parser().foreach_match("#w0\n#w\n", [&](vector<Token> toks) {
      if (toks[1].str() != "line") {
        return;
      }
      /* Workaround the foreach_match not matching overlapping patterns. */
      if (toks.back().next() == '#' && toks.back().next().next() == 'w' &&
          toks.back().next().next().next() == '0' &&
          toks.back().next().next().next().next() == '\n')
      {
        parser.replace(toks[0].line_start(), toks[0].line_end() + 1, "");
      }
    });
    parser.apply_mutations();

    parser().foreach_match("#w0\n", [&](vector<Token> toks) {
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
  void cleanup_empty_lines(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    const string &str = parser.str();

    {
      size_t sequence_start = 0;
      size_t sequence_end = -1;
      while ((sequence_start = str.find("\n\n\n", sequence_end + 1)) != string::npos) {
        sequence_end = str.find_first_not_of("\n", sequence_start);
        if (sequence_end == string::npos) {
          break;
        }
        size_t line = parser::line_number(str, sequence_end);
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
  std::string matrix_constructor_mutation(const std::string &str)
  {
    if (str.find("mat") == std::string::npos) {
      return str;
    }
    /* Example: `mat2(x)` > `mat2x2(x)` */
    std::regex regex_parenthesis(R"(\bmat([234])\()");
    std::string out = std::regex_replace(str, regex_parenthesis, "mat$1x$1(");
    /* Only process square matrices since this is the only types we overload the constructors. */
    /* Example: `mat2x2(x)` > `__mat2x2(x)` */
    std::regex regex(R"(\bmat(2x2|3x3|4x4)\()");
    return std::regex_replace(out, regex, "__mat$1(");
  }

  /* To be run before `argument_decorator_macro_injection()`. */
  void lower_reference_arguments(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

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
          "w(&w)", [&](const vector<Token> toks) { add_mutation(toks[0], toks[3], toks[4]); });
      scope.foreach_match(
          "w&w", [&](const vector<Token> toks) { add_mutation(toks[0], toks[2], toks[2]); });
      scope.foreach_match(
          "w&T", [&](const vector<Token> toks) { add_mutation(toks[0], toks[2], toks[2]); });
    });
    parser.apply_mutations();
  }

  void lower_unions(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

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
    parser().foreach_struct([&](Token struct_tok, Scope, Token struct_name, Scope body) {
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
                report_error(ERROR_TOK(name), "Arrays are not supported inside unions.");
              }
              members.emplace_back(Member{type.str(), name.str(), 0, 0, type.prev() == Enum});
            });

        if (members.empty()) {
          report_error(ERROR_TOK(t[0]), "Empty union");
          return;
        }
        union_members.emplace(union_type, members);

        string union_member = "struct " + union_type + " " + union_name + ";";
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
      for (Member member : value->second) {
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
      body.foreach_declaration([&](Scope, Token, Token type, Scope, Token name, Scope, Token) {
        size_t size = 4;
        if (type.prev() != Enum) {
          size = type_size_get(type);
          if (size != 0) {
            members.emplace_back(Member{type.str(), "." + name.str(), offset, size});
          }
        }
        else {
          members.emplace_back(Member{type.str(), "." + name.str(), offset, size, true});
        }
        offset += size;
      });

      struct_members.emplace(struct_name.str(), members);
    });

    /* Replace placeholder struct with a generic one. */
    auto replace_placeholder_member = [&](Scope body) {
      /* Replace placeholder struct with float members. */
      size_t size = type_size_get(body.front().next());
      if (size == 0) {
        report_error(ERROR_TOK(body.front().next()),
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
        report_error(ERROR_TOK(union_type_tok),
                     "Can't infer size of member. Type must be defined in this file and have "
                     "the [[host_shared]] attribute.");
        return "";
      }
      const Member &last_member = struct_members.back();
      if (last_member.offset + last_member.size != union_size) {
        report_error(ERROR_TOK(union_type_tok), "union has members of different sizes");
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
        report_error(ERROR_TOK(union_type_tok),
                     "Can't infer size of member. Type must be defined in this file and have "
                     "the [[host_shared]] attribute.");
        return "";
      }
      const Member &last_member = struct_members.back();
      if (last_member.offset + last_member.size != union_size) {
        report_error(ERROR_TOK(union_type_tok), "union has members of different sizes");
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
          report_error(
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
            report_error(
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
  void lower_union_accessor_templates(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_struct([&](Token, Scope, Token, Scope body) {
      body.foreach_match("o{..};", [&](const Tokens &t) {
        t[1].scope().foreach_declaration([&](Scope,
                                             Token,
                                             Token type,
                                             Scope template_scope,
                                             Token name,
                                             Scope,
                                             Token) {
          if (type.str() != "union_t") {
            report_error(
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

  /**
   * For safety reason, nested resource tables need to be declared with the srt_t template.
   * This avoid chained member access which isn't well defined with the preprocessing we are doing.
   *
   * This linting phase make sure that [[resource_table]] members uses it and that no incorrect
   * usage is made. We also remove this template because it has no real meaning.
   *
   * Need to run before lower_resource_table.
   */
  void lower_srt_accessor_templates(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_struct([&](Token, Scope, Token, Scope body) {
      body.foreach_declaration([&](Scope attributes,
                                   Token,
                                   Token type,
                                   Scope template_scope,
                                   Token name,
                                   Scope array,
                                   Token) {
        if (attributes[1].str() != "resource_table") {
          if (type.str() == "srt_t") {
            report_error(ERROR_TOK(name),
                         "The srt_t<T> template is only to be used with members declared with the "
                         "[[resource_table]] attribute.");
          }
          return;
        }

        if (type.str() != "srt_t") {
          report_error(
              ERROR_TOK(type),
              "Members declared with the [[resource_table]] attribute must wrap their type "
              "with the srt_t<T> template.");
        }

        if (array.is_valid()) {
          report_error(ERROR_TOK(name), "[[resource_table]] members cannot be arrays.");
        }

        /* Remove the template but not the wrapped type. */
        parser.erase(type);
        if (template_scope.is_valid()) {
          parser.erase(template_scope.front());
          parser.erase(template_scope.back());
        }
      });
    });
    parser.apply_mutations();
  }

  /* Add `srt_access` around all member access of SRT variables.
   * Need to run before local reference mutations. */
  void lower_srt_member_access(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    const string srt_attribute = "resource_table";

    auto memher_access_mutation = [&](parser::Scope attribute,
                                      parser::Token type,
                                      parser::Token var,
                                      parser::Scope body_scope) {
      if (attribute[2].str() != srt_attribute) {
        return;
      }

      if (attribute.scope().type() != ScopeType::FunctionArgs &&
          attribute.scope().type() != ScopeType::FunctionArg)
      {
        parser.replace(attribute, "");
      }
      string srt_type = type.str();
      string srt_var = var.str();

      body_scope.foreach_match("w.w", [&](const vector<Token> toks) {
        if (toks[0].str() != srt_var) {
          return;
        }
        parser.replace(toks[0], toks[2], "srt_access(" + srt_type + ", " + toks[2].str() + ")");
      });
    };

    parser().foreach_scope(ScopeType::FunctionArgs, [&](const Scope fn_args) {
      Scope fn_body = fn_args.next();
      if (fn_body.is_invalid()) {
        return;
      }
      fn_args.foreach_match("[[w]]c?w&w", [&](const vector<Token> toks) {
        memher_access_mutation(toks[0].scope(), toks[7], toks[9], fn_body);
      });
      fn_args.foreach_match("[[w]]c?ww", [&](const vector<Token> toks) {
        if (toks[2].str() == srt_attribute) {
          parser.erase(toks[0].scope());
          report_error(ERROR_TOK(toks[8]), "Shader Resource Table arguments must be references.");
        }
      });
    });

    parser().foreach_scope(ScopeType::Function, [&](const Scope fn_body) {
      fn_body.foreach_match("[[w]]c?w&w", [&](const vector<Token> toks) {
        memher_access_mutation(toks[0].scope(), toks[7], toks[9], toks[9].scope());
      });
      fn_body.foreach_match("[[w]]c?ww", [&](const vector<Token> toks) {
        memher_access_mutation(toks[0].scope(), toks[7], toks[8], toks[8].scope());
      });
    });

    parser.apply_mutations();
  }

  /* Parse entry point definitions and mutating all parameter usage to global resources. */
  void lower_entry_points(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;
    using namespace metadata;

    auto to_uppercase = [](std::string str) {
      for (char &c : str) {
        c = toupper(c);
      }
      return str;
    };

    parser().foreach_function(
        [&](bool, Token type, Token fn_name, Scope args, bool, Scope fn_body) {
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
            report_error(ERROR_TOK(type), "Entry point function must return void.");
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
              report_error(ERROR_TOK(type),
                           "Only compute entry point function can use [[local_size(x,y,z)]].");
            }
            else {
              create_info_decl += "LOCAL_GROUP_SIZE" + local_size + "\n";
            }
          }

          if (use_early_frag_test) {
            if (!is_fragment_func) {
              report_error(ERROR_TOK(type),
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
                report_error(ERROR_TOK(attributes[1]),
                             "[[vertex_id]] is only supported in vertex functions.");
              }
              else if (!is_const || srt_type != "int") {
                report_error(ERROR_TOK(type), "[[vertex_id]] must be declared as `const int`.");
              }
              replace_word(srt_var, "gl_VertexID");
              metadata.builtins.emplace_back(Builtin(hash("gl_VertexID")));
            }
            else if (srt_attr == "instance_id" && is_entry_point) {
              if (!is_vertex_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[instance_id]] is only supported in vertex functions.");
              }
              else if (!is_const || srt_type != "int") {
                report_error(ERROR_TOK(type), "[[instance_id]] must be declared as `const int`.");
              }
              replace_word(srt_var, "gl_InstanceID");
              metadata.builtins.emplace_back(Builtin(hash("gl_InstanceID")));
            }
            else if (srt_attr == "base_instance" && is_entry_point) {
              if (!is_vertex_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[base_instance]] is only supported in vertex functions.");
              }
              else if (!is_const || srt_type != "int") {
                report_error(ERROR_TOK(type),
                             "[[base_instance]] must be declared as "
                             "`const int`.");
              }
              replace_word(srt_var, "gl_BaseInstance");
              metadata.builtins.emplace_back(Builtin(hash("gl_BaseInstance")));
            }
            else if (srt_attr == "point_size" && is_entry_point) {
              if (!is_vertex_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[point_size]] is only supported in vertex functions.");
              }
              else if (is_const || srt_type != "float") {
                report_error(
                    ERROR_TOK(type),
                    "[[point_size]] must be declared as non-const reference (aka `float &`).");
              }
              replace_word(srt_var, "gl_PointSize");
              create_info_decl += "BUILTINS(BuiltinBits::POINT_SIZE)\n";
            }
            else if (srt_attr == "clip_distance" && is_entry_point) {
              if (!is_vertex_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[clip_distance]] is only supported in vertex functions.");
              }
              else if (is_const || srt_type != "float") {
                report_error(ERROR_TOK(type),
                             "[[clip_distance]] must be declared as non-const reference "
                             "(aka `float (&)[]`).");
              }
              replace_word(srt_var, "gl_ClipDistance");
              create_info_decl += "BUILTINS(BuiltinBits::CLIP_DISTANCES)\n";
            }
            else if (srt_attr == "layer" && is_entry_point) {
              if (is_compute_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[layer]] is only supported in vertex and fragment functions.");
              }
              else if (is_vertex_func && (is_const || srt_type != "int")) {
                report_error(ERROR_TOK(type),
                             "[[layer]] must be declared as non-const reference "
                             "(aka `int &`).");
              }
              else if (is_fragment_func && (!is_const || srt_type != "int")) {
                report_error(ERROR_TOK(type),
                             "[[layer]] must be declared as const reference "
                             "(aka `const int &`).");
              }
              replace_word(srt_var, "gl_Layer");
              create_info_decl += "BUILTINS(BuiltinBits::LAYER)\n";
            }
            else if (srt_attr == "viewport_index" && is_entry_point) {
              if (is_compute_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[viewport_index]] is only supported in vertex and "
                             "fragment functions.");
              }
              else if (is_vertex_func && (is_const || srt_type != "int")) {
                report_error(ERROR_TOK(type),
                             "[[viewport_index]] must be declared as non-const reference "
                             "(aka `int &`).");
              }
              else if (is_fragment_func && (!is_const || srt_type != "int")) {
                report_error(ERROR_TOK(type),
                             "[[viewport_index]] must be declared as const reference "
                             "(aka `const int &`).");
              }
              replace_word(srt_var, "gl_ViewportIndex");
              create_info_decl += "BUILTINS(BuiltinBits::VIEWPORT_INDEX)\n";
            }
            else if (srt_attr == "position" && is_entry_point) {
              if (!is_vertex_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[position]] is only supported in vertex functions.");
              }
              else if (is_const || srt_type != "float4") {
                report_error(
                    ERROR_TOK(type),
                    "[[position]] must be declared as non-const reference (aka `float4 &`).");
              }
              else {
                replace_word(srt_var, "gl_Position");
              }
            }
            else if (srt_attr == "frag_coord" && is_entry_point) {
              if (!is_fragment_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[frag_coord]] is only supported in fragment functions.");
              }
              else if (!is_const || srt_type != "float4") {
                report_error(ERROR_TOK(type),
                             "[[frag_coord]] must be declared as `const float4`.");
              }
              else {
                create_info_decl += "BUILTINS(BuiltinBits::FRAG_COORD)\n";
                replace_word(srt_var, "gl_FragCoord");
              }
            }
            else if (srt_attr == "point_coord" && is_entry_point) {
              if (!is_fragment_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[point_coord]] is only supported in fragment functions.");
              }
              else if (!is_const || srt_type != "float2") {
                report_error(ERROR_TOK(type),
                             "[[point_coord]] must be declared as `const float2`.");
              }
              else {
                create_info_decl += "BUILTINS(BuiltinBits::POINT_COORD)\n";
                replace_word(srt_var, "gl_PointCoord");
              }
            }
            else if (srt_attr == "front_facing" && is_entry_point) {
              if (!is_fragment_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[front_facing]] is only supported in fragment functions.");
              }
              else if (!is_const || srt_type != "bool") {
                report_error(ERROR_TOK(type),
                             "[[front_facing]] must be declared as `const bool`.");
              }
              else {
                create_info_decl += "BUILTINS(BuiltinBits::FRONT_FACING)\n";
                replace_word(srt_var, "gl_FrontFacing");
              }
            }
            else if (srt_attr == "global_invocation_id" && is_entry_point) {
              if (!is_compute_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[global_invocation_id]] is only supported in compute functions.");
              }
              else if (!is_const || srt_type != "uint3") {
                report_error(ERROR_TOK(type),
                             "[[global_invocation_id]] must be declared as `const uint3`.");
              }
              else {
                create_info_decl += "BUILTINS(BuiltinBits::GLOBAL_INVOCATION_ID)\n";
                replace_word(srt_var, "gl_GlobalInvocationID");
              }
            }
            else if (srt_attr == "local_invocation_id" && is_entry_point) {
              if (!is_compute_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[local_invocation_id]] is only supported in compute functions.");
              }
              else if (!is_const || srt_type != "uint3") {
                report_error(ERROR_TOK(type),
                             "[[local_invocation_id]] must be declared as `const uint3`.");
              }
              else {
                create_info_decl += "BUILTINS(BuiltinBits::LOCAL_INVOCATION_ID)\n";
                replace_word(srt_var, "gl_LocalInvocationID");
              }
            }
            else if (srt_attr == "local_invocation_index" && is_entry_point) {
              if (!is_compute_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[local_invocation_index]] is only supported in compute functions.");
              }
              else if (!is_const || srt_type != "uint") {
                report_error(ERROR_TOK(type),
                             "[[local_invocation_index]] must be declared as `const uint`.");
              }
              else {
                create_info_decl += "BUILTINS(BuiltinBits::LOCAL_INVOCATION_INDEX)\n";
                replace_word(srt_var, "gl_LocalInvocationIndex");
              }
            }
            else if (srt_attr == "work_group_id" && is_entry_point) {
              if (!is_compute_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[work_group_id]] is only supported in compute functions.");
              }
              else if (!is_const || srt_type != "uint3") {
                report_error(ERROR_TOK(type),
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
                report_error(ERROR_TOK(attributes[1]),
                             "[[num_work_groups]] is only supported in compute functions.");
              }
              else if (!is_const || srt_type != "uint3") {
                report_error(ERROR_TOK(type),
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
                report_error(ERROR_TOK(attributes[1]),
                             "[[in]] is only supported in vertex and fragment functions.");
              }
              else if (!is_const) {
                report_error(ERROR_TOK(type), "[[in]] must be declared as const reference.");
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
                report_error(ERROR_TOK(attributes[1]),
                             "[[out]] is only supported in vertex and fragment functions.");
              }
              else if (is_const) {
                report_error(ERROR_TOK(type), "[[out]] must be declared as non-const reference.");
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
                                    " " + srt_type + " " + srt_var + ";");
                create_info_decl += "ADDITIONAL_INFO(" + srt_type + ")\n";
              }
            }
            else if (srt_attr == "frag_depth") {
              if (srt_type != "float") {
                report_error(ERROR_TOK(type), "[[frag_depth]] needs to be declared as float");
              }
              const string mode = attributes[3].str();

              if (mode != "any" && mode != "greater" && mode != "less") {
                report_error(ERROR_TOK(attributes[3]),
                             "unrecognized mode, expecting 'any', 'greater' or 'less'");
              }
              else {
                create_info_decl += "DEPTH_WRITE(" + to_uppercase(mode) + ")\n";
                replace_word(srt_var, "gl_FragDepth");
              }
            }
            else if (srt_attr == "frag_stencil_ref") {
              if (srt_type != "int") {
                report_error(ERROR_TOK(type), "[[frag_stencil_ref]] needs to be declared as int");
              }
              else {
                create_info_decl += "BUILTINS(BuiltinBits::STENCIL_REF)\n";
                replace_word(srt_var, "gl_FragStencilRefARB");
              }
            }
            else {
              report_error(ERROR_TOK(attributes[1]), "Invalid attribute.");
            }
          };

          args.foreach_match("[[..]]c?ww", [&](const vector<Token> toks) {
            process_argument(toks[8], toks[9], toks[1].scope());
          });
          args.foreach_match("[[..]]c?w&w", [&](const vector<Token> toks) {
            process_argument(toks[8], toks[10], toks[1].scope());
          });

          args.foreach_match("[[..]]c?w(&w)", [&](const vector<Token> toks) {
            process_argument(toks[8], toks[11], toks[1].scope());
          });

          create_info_decl += "GPU_SHADER_CREATE_END()\n";

          if (is_entry_point) {
            metadata.create_infos_declarations.emplace_back(create_info_decl);
          }
        });

    parser.apply_mutations();
  }

  /* Removes entry point arguments to make it compatible with the legacy code.
   * Has to run after mutation related to function arguments. */
  void lower_entry_points_signature(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;
    using namespace metadata;

    parser().foreach_function([&](bool, Token type, Token, Scope args, bool, Scope) {
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
    });

    parser.apply_mutations();
  }

  /* To be run after `lower_reference_arguments()`. */
  void lower_reference_variables(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_function([&](bool, Token, Token, Scope fn_args, bool, Scope fn_scope) {
      fn_scope.foreach_match("c?w&w=", [&](const vector<Token> &tokens) {
        const Token name = tokens[4];
        const Scope assignment = tokens[5].scope();

        Token decl_start = tokens[0].is_valid() ? tokens[0] : tokens[2];
        /* Take attribute into account. */
        decl_start = (decl_start.prev() == ']') ? decl_start.prev().scope().front() : decl_start;
        /* Take ending ; into account. */
        const Token decl_end = assignment.back().next();

        /* Assert definition doesn't contain any side effect. */
        assignment.foreach_token(Increment, [&](const Token token) {
          report_error(ERROR_TOK(token), "Reference definitions cannot have side effects.");
        });
        assignment.foreach_token(Decrement, [&](const Token token) {
          report_error(ERROR_TOK(token), "Reference definitions cannot have side effects.");
        });
        assignment.foreach_token(ParOpen, [&](const Token token) {
          string fn_name = token.prev().str();
          if ((fn_name != "specialization_constant_get") && (fn_name != "push_constant_get") &&
              (fn_name != "interface_get") && (fn_name != "attribute_get") &&
              (fn_name != "buffer_get") && (fn_name != "srt_access") &&
              (fn_name != "sampler_get") && (fn_name != "image_get"))
          {
            report_error(ERROR_TOK(token), "Reference definitions cannot contain function calls.");
          }
        });
        assignment.foreach_scope(ScopeType::Subscript, [&](const Scope subscript) {
          if (subscript.token_count() != 3) {
            report_error(
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
          fn_args.foreach_match("c?w&?w", [&](const vector<Token> &toks) { process_decl(toks); });
          fn_scope.foreach_match("c?w&?w", [&](const vector<Token> &toks) { process_decl(toks); });

          if (!is_found) {
            report_error(ERROR_TOK(index_var),
                         "Cannot locate array subscript variable declaration. "
                         "If it is a global variable, assign it to a temporary const variable for "
                         "indexing inside the reference.");
            return;
          }
          if (!is_const) {
            report_error(ERROR_TOK(index_var),
                         "Array subscript variable must be declared as const qualified.");
            return;
          }
          if (is_ref) {
            report_error(ERROR_TOK(index_var),
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

    parser().foreach_match("c?w&w=", [&](const vector<Token> &tokens) {
      report_error(ERROR_TOK(tokens[4]),
                   "Reference is defined inside a global or unterminated scope.");
    });
  }

  void lower_argument_qualifiers(Parser &parser, report_callback /*report_error*/)
  {
    /* Example: `out float var[2]` > `REF(float, var)[2]` */
    parser().foreach_match("www", [&](const Tokens &toks) {
      if (toks[0].scope().type() == parser::ScopeType::Preprocessor) {
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

  std::string argument_decorator_macro_injection(const std::string &str)
  {
    /* Example: `out float var[2]` > `out float _out_sta var _out_end[2]` */
    std::regex regex(R"((out|inout|in|shared)\s+(\w+)\s+(\w+))");
    return std::regex_replace(str, regex, "$1 $2 _$1_sta $3 _$1_end");
  }

  std::string array_constructor_macro_injection(const std::string &str)
  {
    /* Example: `= float[2](0.0, 0.0)` > `= ARRAY_T(float) ARRAY_V(0.0, 0.0)` */
    std::regex regex(R"(=\s*(\w+)\s*\[[^\]]*\]\s*\()");
    return std::regex_replace(str, regex, "= ARRAY_T($1) ARRAY_V(");
  }

  /* Assume formatted source with our code style. Cannot be applied to python shaders. */
  void lint_global_scope_constants(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    /* Example: `const uint global_var = 1u;`. */
    parser().foreach_match("cww=", [&](const vector<Token> &tokens) {
      if (tokens[0].scope().type() == ScopeType::Global) {
        report_error(
            ERROR_TOK(tokens[2]),
            "Global scope constant expression found. These get allocated per-thread in MSL. "
            "Use Macro's or uniforms instead.");
      }
    });
  }

  /* Search for constructor definition in active code. These are not supported. */
  void lint_constructors(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_struct([&](Token, Scope, Token struct_name, Scope struct_scope) {
      struct_scope.foreach_match("w(..)", [&](const Tokens &t) {
        if (t[0].scope() != struct_scope) {
          return;
        }
        if (t[0].str() == struct_name.str()) {
          report_error(ERROR_TOK(t[0]), "Constructors are not supported.");
        }
      });
    });
  }

  /* Forward declaration of types are not supported and makes no sense in a shader program where
   * there is no pointers. */
  void lint_forward_declared_structs(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("sw;", [&](const Tokens &t) {
      if (t[0].scope().type() == ScopeType::Global) {
        report_error(ERROR_TOK(t[0]), "Forward declaration of types are not supported.");
      }
    });
  }

  std::string line_directive_prefix(const std::string &filename)
  {
    /* NOTE: This is not supported by GLSL. All line directives are muted at runtime and the
     * sources are scanned after error reporting for the locating the muted line. */
    return "#line 1 \"" + filename + "\"\n";
  }
};

}  // namespace blender::gpu::shader
