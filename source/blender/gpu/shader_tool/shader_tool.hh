/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include <algorithm>
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
  std::string res_frequency;
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
    std::stringstream ss;
    if (res_type == "legacy_info") {
      ss << "ADDITIONAL_INFO(" << var_name << ")";
    }
    else if (res_type == "sampler") {
      if (res_frequency.empty()) {
        ss << "SAMPLER(" << res_slot << ", " << var_type << ", " << var_name << ")";
      }
      else {
        ss << "SAMPLER_FREQ(" << res_slot << ", " << var_type << ", " << var_name << ", "
           << res_frequency << ")";
      }
    }
    else if (res_type == "image") {
      if (res_frequency.empty()) {
        ss << "IMAGE(" << res_slot << ", " << res_format << ", " << res_qualifier << ", "
           << var_type << ", " << var_name << ")";
      }
      else {
        ss << "IMAGE_FREQ(" << res_slot << ", " << res_format << ", " << res_qualifier << ", "
           << var_type << ", " << var_name << ", " << res_frequency << ")";
      }
    }
    else if (res_type == "uniform") {
      if (res_frequency.empty()) {
        ss << "UNIFORM_BUF(" << res_slot << ", " << var_type << ", " << var_name << var_array
           << ")";
      }
      else {
        ss << "UNIFORM_BUF_FREQ(" << res_slot << ", " << var_type << ", " << var_name << var_array
           << ", " << res_frequency << ")";
      }
    }
    else if (res_type == "storage") {
      if (res_frequency.empty()) {
        ss << "STORAGE_BUF(" << res_slot << ", " << res_qualifier << ", " << var_type << ", "
           << var_name << var_array << ")";
      }
      else {
        ss << "STORAGE_BUF_FREQ(" << res_slot << ", " << res_qualifier << ", " << var_type << ", "
           << var_name << var_array << ", " << res_frequency << ")";
      }
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
  using report_callback = std::function<void(
      int error_line, int error_char, std::string error_line_string, const char *error_str)>;

  metadata::Source metadata;

  using Parser = shader::parser::IntermediateForm;

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

  static SourceLanguage language_from_filename(const std::string &filename)
  {
    if (filename.find(".msl") != std::string::npos) {
      return MSL;
    }
    if (filename.find(".glsl") != std::string::npos) {
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
                      const std::string &filename,
                      bool do_parse_function,
                      bool do_small_type_linting,
                      report_callback report_error,
                      metadata::Source &r_metadata)
  {
    if (language == UNKNOWN) {
      report_error(0, 0, "", "Unknown file type");
      return "";
    }
    str = remove_comments(str, report_error);
    if (language == BLENDER_GLSL || language == CPP) {
      str = disabled_code_mutation(str, report_error);
    }
    else {
      str = remove_whitespace(str, report_error);
    }
    str = threadgroup_variables_parse_and_remove(str, report_error);
    parse_builtins(str, filename);
    if (language == BLENDER_GLSL || language == CPP) {
      if (do_parse_function) {
        parse_library_functions(str);
      }
      {
        Parser parser(str, report_error);
        if (language == BLENDER_GLSL) {
          pragma_runtime_generated_parsing(parser);
          pragma_once_linting(parser, filename, report_error);
        }
        parse_defines(parser, report_error);
        create_info_parse_and_remove(parser, report_error);
        include_parse_and_remove(parser, report_error);
        pragmas_mutation(parser, report_error);
        swizzle_function_mutation(parser, report_error);
        enum_macro_injection(parser, language == CPP, report_error);

        if (language == BLENDER_GLSL) {
          srt_template_linter_and_mutation(parser, report_error);
          using_mutation(parser, report_error);
          namespace_mutation(parser, report_error);
          template_struct_mutation(parser, report_error);
          template_definition_mutation(parser, report_error);
          template_call_mutation(parser, report_error);
          entry_point_parsing_and_mutation(parser, report_error);
          stage_function_mutation(parser, report_error);
          pipeline_parse_and_remove(parser, filename, report_error);
          resource_table_parsing(parser, report_error);
          resource_guard_mutation(parser, report_error);
          struct_method_mutation(parser, report_error);
          method_call_mutation(parser, report_error);
          srt_member_access_mutation(parser, report_error);
          static_branch_mutation(parser, report_error);
          empty_struct_mutation(parser, report_error);
          loop_unroll(parser, report_error);
          assert_processing(parser, filename, report_error);
          static_strings_merging(parser, report_error);
          static_strings_parsing_and_mutation(parser, report_error);
          printf_processing(parser, report_error);
          quote_linting(parser, report_error);
        }

        default_argument_mutation(parser, report_error);
        global_scope_constant_linting(parser, report_error);
        if (do_small_type_linting) {
          small_type_linting(parser, report_error);
        }
        remove_quotes(parser, report_error);
        srt_guard_mutation(parser, report_error);
        argument_reference_mutation(parser, report_error);
        remove_whitespace(parser, report_error);
        variable_reference_mutation(parser, report_error);
        str = parser.result_get();
      }
      if (language == BLENDER_GLSL) {
        str = namespace_separator_mutation(str);
      }
      {
        Parser parser(str, report_error);
        /* Do another whitespace pass to remove the one introduced by mutations. */
        remove_whitespace(parser, report_error);
        cleanup_empty_lines(parser, report_error);
        cleanup_line_directives(parser, report_error);
        str = parser.result_get();
      }
    }
    else if (language == MSL) {
      Parser parser(str, report_error);
      pragma_runtime_generated_parsing(parser);
      include_parse_and_remove(parser, report_error);
      pragmas_mutation(parser, report_error);
      str = parser.result_get();
    }
#ifdef __APPLE__ /* Limiting to Apple hardware since GLSL compilers might have issues. */
    if (language == GLSL) {
      str = matrix_constructor_mutation(str);
    }
#endif
    str = argument_decorator_macro_injection(str);
    str = array_constructor_macro_injection(str);
    r_metadata = metadata;
    return line_directive_prefix(filename) + str;
  }

  /* Variant use for python shaders. */
  std::string process(const std::string &str, metadata::Source &r_metadata)
  {
    auto no_err_report = [](int, int, std::string, const char *) {};
    return process(GLSL, str, "", false, false, no_err_report, r_metadata);
  }

 private:
  using regex_callback = std::function<void(const std::smatch &)>;
  using regex_callback_with_line_count = std::function<void(const std::smatch &, int64_t)>;

  /* Helper to make the code more readable in parsing functions. */
  void regex_global_search(const std::string &str,
                           const std::regex &regex,
                           regex_callback callback)
  {
    using namespace std;
    string::const_iterator it = str.begin();
    for (smatch match; regex_search(it, str.end(), match, regex); it = match.suffix().first) {
      callback(match);
    }
  }

  void regex_global_search(const std::string &str,
                           const std::regex &regex,
                           regex_callback_with_line_count callback)
  {
    using namespace std;
    int64_t line = 1;
    regex_global_search(str, regex, [&line, &callback](const std::smatch &match) {
      line += line_count(match.prefix().str());
      callback(match, line);
      line += line_count(match[0].str());
    });
  }

  template<typename ReportErrorF>
  std::string remove_comments(const std::string &str, const ReportErrorF &report_error)
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
  void remove_whitespace(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    const string &str = parser.str();

    size_t last_whitespace = -1;
    while ((last_whitespace = str.find(" \n", last_whitespace + 1)) != string::npos) {
      size_t first_not_whitespace = str.find_last_not_of(" ", last_whitespace);
      if (first_not_whitespace == string::npos) {
        return;
      }
      parser.replace(first_not_whitespace + 1, last_whitespace, "");
    }
    parser.apply_mutations();
  }

  /* Safer version without Parser. */
  std::string remove_whitespace(const std::string &str, const report_callback & /*report_error*/)
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
    template_args.foreach_scope(ScopeType::TemplateArg, [&](const Scope &scope) {
      args_concat += 'T' + scope.start().str();
    });
    return args_concat;
  }

  void template_struct_mutation(Parser &parser, report_callback &report_error)
  {
    using namespace std;
    using namespace shader::parser;
    {
      parser().foreach_match("w<..>(..)", [&](const vector<Token> &tokens) {
        const Scope template_args = tokens[1].scope();
        template_args.foreach_match("w<..>", [&parser](const vector<Token> &tokens) {
          parser.replace(tokens[1].scope(), template_arguments_mangle(tokens[1].scope()), true);
        });
      });
      parser.apply_mutations();

      /* Replace full specialization by simple struct. */
      parser().foreach_match("t<>sw<..>", [&](const std::vector<Token> &tokens) {
        parser.erase(tokens[0], tokens[2]);
        parser.replace(tokens[5].scope(), template_arguments_mangle(tokens[5].scope()), true);
      });
      parser.apply_mutations();
    }
    {
      parser().foreach_scope(ScopeType::Template, [&](Scope temp) {
        /* Parse template declaration. */
        Token struct_start = temp.end().next();
        if (struct_start != Struct) {
          return;
        }
        Token struct_name = struct_start.next();
        Scope struct_body = struct_name.next().scope();

        bool error = false;
        temp.foreach_match("=", [&](const std::vector<Token> &tokens) {
          report_error(ERROR_TOK(tokens[0]),
                       "Default arguments are not supported inside template declaration");
          error = true;
        });
        if (error) {
          return;
        }

        string arg_pattern;
        vector<string> arg_list;
        temp.foreach_scope(ScopeType::TemplateArg, [&](Scope arg) {
          const Token type = arg.start();
          const Token name = type.next();
          const string name_str = name.str();
          const string type_str = type.str();

          arg_list.emplace_back(name_str);

          if (type_str == "typename") {
            arg_pattern += ",w";
          }
          else if (type_str == "enum" || type_str == "bool") {
            arg_pattern += ",w";
          }
          else if (type_str == "int" || type_str == "uint") {
            arg_pattern += ",0";
          }
          else {
            report_error(ERROR_TOK(type), "Invalid template argument type");
          }
        });

        Token struct_end = struct_body.end();
        const string fn_decl = parser.substr_range_inclusive(struct_start.str_index_start(),
                                                             struct_end.str_index_last());

        /* Remove declaration. */
        Token template_keyword = temp.start().prev();
        parser.erase(template_keyword.str_index_start(), struct_end.line_end());

        /* Replace instantiations. */
        Scope parent_scope = temp.scope();
        string specialization_pattern = "tsw<" + arg_pattern.substr(1) + ">";
        parent_scope.foreach_match(specialization_pattern, [&](const std::vector<Token> &tokens) {
          if (struct_name.str() != tokens[2].str()) {
            return;
          }
          /* Parse template values. */
          vector<pair<string, string>> arg_name_value_pairs;
          for (int i = 0; i < arg_list.size(); i++) {
            arg_name_value_pairs.emplace_back(arg_list[i], tokens[4 + 2 * i].str());
          }
          /* Specialize template content. */
          Parser instance_parser(fn_decl, report_error, true);
          instance_parser().foreach_match("w", [&](const std::vector<Token> &tokens) {
            string token_str = tokens[0].str();
            for (const auto &arg_name_value : arg_name_value_pairs) {
              if (token_str == arg_name_value.first) {
                instance_parser.replace(tokens[0], arg_name_value.second);
              }
            }
          });

          const string template_args = parser.substr_range_inclusive(
              tokens[3], tokens[3 + arg_pattern.size()]);
          size_t pos = fn_decl.find(" " + struct_name.str());
          instance_parser.insert_after(pos + struct_name.str().size(), template_args);
          /* Paste template content in place of instantiation. */
          Token end_of_instantiation = tokens.back();
          string instance = instance_parser.result_get();
          parser.insert_line_number(tokens.front().str_index_start() - 1,
                                    struct_start.line_number());
          parser.replace(tokens.front().str_index_start(),
                         end_of_instantiation.str_index_last_no_whitespace(),
                         instance);
          parser.insert_line_number(end_of_instantiation.line_end() + 1,
                                    end_of_instantiation.line_number() + 1);
        });
      });
      parser.apply_mutations();
    }
    {
      /* This rely on our code-style that do not put spaces between template name and the opening
       * angle bracket. */
      parser().foreach_match("sw<..>", [&](const std::vector<Token> &tokens) {
        parser.replace(tokens[2].scope(), template_arguments_mangle(tokens[2].scope()), true);
      });
      parser.apply_mutations();
    }
  }

  void template_definition_mutation(Parser &parser, report_callback &report_error)
  {
    if (parser.str().find("template") == std::string::npos) {
      return;
    }

    using namespace std;
    using namespace shader::parser;

    auto process_specialization = [&](const Token specialization_start,
                                      const Scope template_args) {
      parser.erase(specialization_start, specialization_start.next().next());
      parser.replace(template_args, template_arguments_mangle(template_args), true);
    };

    /* Replace full specialization by simple functions. */
    parser().foreach_match("t<>ww<", [&](const std::vector<Token> &tokens) {
      process_specialization(tokens[0], tokens[5].scope());
    });
    parser().foreach_match("t<>ww::w<", [&](const std::vector<Token> &tokens) {
      process_specialization(tokens[0], tokens[8].scope());
    });

    parser.apply_mutations();

    auto process_template = [&](const Token fn_start,
                                const string &fn_name,
                                const Scope fn_args,
                                const Scope temp,
                                const Token fn_end) {
      bool error = false;
      temp.foreach_match("=", [&](const std::vector<Token> &tokens) {
        report_error(tokens[0].line_number(),
                     tokens[0].char_number(),
                     tokens[0].line_str(),
                     "Default arguments are not supported inside template declaration");
        error = true;
      });
      if (error) {
        return;
      }

      string arg_pattern;
      vector<string> arg_list;
      bool all_template_args_in_function_signature = true;
      temp.foreach_scope(ScopeType::TemplateArg, [&](Scope arg) {
        const Token type = arg.start();
        const Token name = type.next();
        const string name_str = name.str();
        const string type_str = type.str();

        arg_list.emplace_back(name_str);

        if (type_str == "typename") {
          arg_pattern += ",w";
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
          arg_pattern += ",w";
          /* Values cannot be resolved using type deduction. */
          all_template_args_in_function_signature = false;
        }
        else if (type_str == "int" || type_str == "uint") {
          arg_pattern += ",0";
          /* Values cannot be resolved using type deduction. */
          all_template_args_in_function_signature = false;
        }
        else {
          report_error(ERROR_TOK(type), "Invalid template argument type");
        }
      });

      const string fn_decl = parser.substr_range_inclusive(fn_start.str_index_start(),
                                                           fn_end.line_end());

      /* Remove declaration. */
      Token template_keyword = temp.start().prev();
      parser.erase(template_keyword.str_index_start(), fn_end.line_end());

      auto process_instantiation = [&](const string &inst_name,
                                       const Token inst_start,
                                       const Token inst_end,
                                       const Scope &inst_args) {
        if (fn_name != inst_name) {
          return;
        }
        /* Parse template values. */
        vector<pair<string, string>> arg_name_value_pairs;
        for (int i = 0; i < arg_list.size(); i++) {
          arg_name_value_pairs.emplace_back(arg_list[i], inst_args[1 + 2 * i].str());
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
          size_t pos = fn_decl.find(" " + fn_name);
          instance_parser.insert_after(pos + fn_name.size(), inst_args.str_with_whitespace());
        }
        /* Paste template content in place of instantiation. */
        string instance = instance_parser.result_get();
        parser.insert_line_number(inst_start.str_index_start() - 1, fn_start.line_number());
        parser.replace(
            inst_start.str_index_start(), inst_end.str_index_last_no_whitespace(), instance);
        parser.insert_line_number(inst_end.line_end() + 1, inst_end.line_number() + 1);
      };

      /* Replace instantiations. */
      Scope parent_scope = temp.scope();
      {
        string specialization_pattern = "tww<" + arg_pattern.substr(1) + ">(..);";
        parent_scope.foreach_match(specialization_pattern, [&](const vector<Token> &tokens) {
          process_instantiation(tokens[2].str(), tokens.front(), tokens.back(), tokens[3].scope());
        });
      }
      {
        string specialization_pattern = "tww::w<" + arg_pattern.substr(1) + ">(..);";
        parent_scope.foreach_match(specialization_pattern, [&](const vector<Token> &tokens) {
          const string inst_name = parser.substr_range_inclusive(tokens[2], tokens[5]);
          process_instantiation(inst_name, tokens.front(), tokens.back(), tokens[6].scope());
        });
      }
    };

    parser().foreach_match("t<..>ww(..)c?{..}", [&](const vector<Token> &tokens) {
      process_template(
          tokens[5], tokens[6].str(), tokens[7].scope(), tokens[1].scope(), tokens[16]);
    });

    parser().foreach_match("t<..>ww::w(..)c?{..}", [&](const vector<Token> &tokens) {
      const string fn_name = parser.substr_range_inclusive(tokens[6], tokens[9]);
      process_template(tokens[5], fn_name, tokens[10].scope(), tokens[1].scope(), tokens[19]);
    });

    parser.apply_mutations();

    {
      /* Check if there is no remaining declaration and instantiation that were not processed. */
      size_t error_pos;
      if ((error_pos = parser.str().find("template<")) != std::string::npos) {
        report_error(line_number(parser.str(), error_pos),
                     char_number(parser.str(), error_pos),
                     line_str(parser.str(), error_pos),
                     "Template declaration unsupported syntax");
      }
      if ((error_pos = parser.str().find("template ")) != std::string::npos) {
        report_error(line_number(parser.str(), error_pos),
                     char_number(parser.str(), error_pos),
                     line_str(parser.str(), error_pos),
                     "Template instantiation unsupported syntax");
      }
    }
  }

  void template_call_mutation(Parser &parser, report_callback & /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("w<..>", [&](const std::vector<Token> &tokens) {
      parser.replace(tokens[1].scope(), template_arguments_mangle(tokens[1].scope()), true);
    });
    parser.apply_mutations();
  }

  /* Remove remaining quotes that can be found in some unsupported C++ macros. */
  void remove_quotes(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_token(TokenType::String, [&](const Token token) { parser.erase(token); });
    parser.apply_mutations();
  }

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

  void create_info_parse_and_remove(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    auto get_placeholder = [](const string &name) {
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

    parser().foreach_scope(ScopeType::Attributes, [&](const Scope attrs) {
      if (attrs.str_with_whitespace() != "[resource_table]") {
        return;
      }
      Token type = attrs.scope().end().next();
      Token struct_keyword = attrs.scope().start().prev();
      if (type != Word || struct_keyword != Struct) {
        return;
      }
      parser.insert_before(struct_keyword, get_placeholder(type.str()));
      parser.insert_line_number(struct_keyword.str_index_start() - 1,
                                struct_keyword.line_number());
    });

    parser().foreach_match("w(..)", [&](const std::vector<Token> &tokens) {
      if (tokens[0].str() == "CREATE_INFO_VARIANT") {
        const string variant_name = tokens[1].scope().start().next().str();
        metadata.create_infos.emplace_back(variant_name);

        const string variant_decl = parser.substr_range_inclusive(tokens.front(), tokens.back());
        metadata.create_infos_declarations.emplace_back(variant_decl);

        parser.replace(tokens.front(), tokens.back(), get_placeholder(variant_name));
        return;
      }
      if (tokens[0].str() == "GPU_SHADER_CREATE_INFO") {
        const string variant_name = tokens[1].scope().start().next().str();
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
                       get_placeholder(variant_name));
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

  void include_parse_and_remove(Parser &parser, report_callback /*report_error*/)
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
      parser.erase(tokens.front(), tokens.back());
    });

    parser.apply_mutations();
  }

  void pragma_runtime_generated_parsing(Parser &parser)
  {
    if (parser.str().find("\n#pragma runtime_generated") != std::string::npos) {
      metadata.builtins.emplace_back(metadata::Builtin::runtime_generated);
    }
  }

  void pragma_once_linting(Parser &parser,
                           const std::string &filename,
                           report_callback report_error)
  {
    if (filename.find("_lib.") == std::string::npos && filename.find(".hh") == std::string::npos) {
      return;
    }
    if (parser.str().find("\n#pragma once") == std::string::npos) {
      report_error(0, 0, "", "Header files must contain #pragma once directive.");
    }
  }

  void loop_unroll(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    auto parse_for_args =
        [&](const Scope loop_args, Scope &r_init, Scope &r_condition, Scope &r_iter) {
          r_init = r_condition = r_iter = Scope::invalid();
          loop_args.foreach_scope(ScopeType::LoopArg, [&](const Scope arg) {
            if (arg.start().prev() == '(' && arg.end().next() == ';') {
              r_init = arg;
            }
            else if (arg.start().prev() == ';' && arg.end().next() == ';') {
              r_condition = arg;
            }
            else if (arg.start().prev() == ';' && arg.end().next() == ')') {
              r_iter = arg;
            }
            else {
              report_error(ERROR_TOK(arg.start()), "Invalid loop declaration.");
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

      if (!parser.replace_try(loop_start, body.end(), "", true)) {
        /* This is the case of nested loops. This loop will be processed in another parser pass. */
        return;
      }

      string indent_init, indent_cond, indent_iter;
      if (init.is_valid()) {
        indent_init = string(init.start().char_number() - 1, ' ');
      }
      if (cond.is_valid()) {
        indent_cond = string(cond.start().char_number() - 3, ' ');
      }
      if (iter.is_valid()) {
        indent_iter = string(iter.start().char_number(), ' ');
      }
      string indent_body = string(body.start().char_number(), ' ');
      string indent_end = string(body.end().char_number(), ' ');

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

      parser.insert_after(body.end(), "\n");
      if (init.is_valid() && !iteration_is_trivial) {
        parser.insert_line_number(body.end(), init.start().line_number());
        parser.insert_after(body.end(), indent_init + "{" + init.str_with_whitespace() + ";\n");
      }
      else {
        parser.insert_after(body.end(), "{\n");
      }
      for (int64_t i = 0, value = iter_init; i < iter_count; i++, value += iter_incr) {
        if (cond.is_valid() && !condition_is_trivial) {
          parser.insert_line_number(body.end(), cond.start().line_number());
          parser.insert_after(body.end(),
                              indent_cond + "if(" + cond.str_with_whitespace() + ")\n");
        }
        parser.insert_after(body.end(), replace_index(body_prefix, value));
        parser.insert_line_number(body.end(), body.start().line_number());
        parser.insert_after(body.end(),
                            indent_body + replace_index(body.str_with_whitespace(), value) + "\n");
        parser.insert_after(body.end(), body_suffix);
        if (iter.is_valid() && !iteration_is_trivial) {
          parser.insert_line_number(body.end(), iter.start().line_number());
          parser.insert_after(body.end(), indent_iter + iter.str_with_whitespace() + ";\n");
        }
      }
      parser.insert_line_number(body.end(), body.end().line_number());
      parser.insert_after(body.end(), indent_end + body.end().str_with_whitespace());
    };

    do {
      /* [[gpu::unroll]]. */
      parser().foreach_match("[[w::w]]f(..){..}", [&](const std::vector<Token> tokens) {
        if (tokens[1].scope().str_with_whitespace() != "[gpu::unroll]") {
          return;
        }
        const Token for_tok = tokens[8];
        const Scope loop_args = tokens[9].scope();
        const Scope loop_body = tokens[13].scope();

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
            parser.substr_range_inclusive(var_init.next(), var_init.scope().end()));
        int64_t end_value = std::stol(
            parser.substr_range_inclusive(cond_sign.is_valid() ? cond_sign : cond_end, cond_end));
        /* TODO(fclem): Support arbitrary strides (aka, arbitrary iter statement). */
        int iter_count = std::abs(end_value - init_value);
        if (cond_type == GEqual || cond_type == LEqual) {
          iter_count += 1;
        }

        bool condition_is_trivial = (cond_end == cond.end());
        bool iteration_is_trivial = (iter_end == iter.end());

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

      /* [[gpu::unroll(n)]]. */
      parser().foreach_match("[[w::w(0)]]f(..){..}", [&](const std::vector<Token> tokens) {
        if (tokens[5].str() != "unroll") {
          return;
        }
        const Scope loop_args = tokens[12].scope();
        const Scope loop_body = tokens[16].scope();

        Scope init, cond, iter;
        parse_for_args(loop_args, init, cond, iter);

        int iter_count = std::stol(tokens[7].str());

        process_loop(tokens[0], iter_count, 0, 0, false, false, init, cond, iter, loop_body);
      });

      /* [[gpu::unroll_define(max_n)]]. */
      parser().foreach_match("[[w::w(0)]]f(..){..}", [&](const std::vector<Token> tokens) {
        if (tokens[5].str() != "unroll_define") {
          return;
        }
        const Scope loop_args = tokens[12].scope();
        const Scope loop_body = tokens[16].scope();

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
          report_error(ERROR_TOK(loop_args.start()),
                       "Incompatible loop format for [[gpu::unroll_define(max_n)]], expected "
                       "'(int i = 0; i < DEFINE; i++)'");
          return;
        }

        Scope init, cond, iter;
        parse_for_args(loop_args, init, cond, iter);

        int iter_count = std::stol(tokens[7].str());

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
    parser().foreach_match("[[w::w", [&](const std::vector<Token> tokens) {
      if (tokens[2].str() == "gpu" && tokens[5].str() == "unroll") {
        report_error(ERROR_TOK(tokens[0]), "Incompatible loop format for [[gpu::unroll]].");
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
      report_error(ERROR_TOK(attribute), "Unrecognized attribute.");
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

    Token before_body = body.start().prev();
    string test = condition[3].str() + "_" + condition[5].str();
    string directive = (if_tok.prev() == Else ? "#elif " : "#if ");

    parser.insert_directive(before_body, directive + test);
    parser.erase(if_tok, before_body);

    if (body.end().next() == Else) {
      Token else_tok = body.end().next();
      parser.erase(else_tok);
      if (else_tok.next() == If) {
        /* Will be processed later. */
        Token next_if = else_tok.next();
        /* Ensure the rest of the if clauses also have the attribute. */
        Scope attributes = next_if.next().scope().end().next().scope();
        if (attributes.type() != ScopeType::Subscript ||
            attributes.start().next().scope().str_exclusive() != "static_branch")
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
    parser.insert_directive(body.end(), "#endif");
  };

  void static_branch_mutation(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("i(..)[[w]]{..}", [&](const std::vector<Token> &tokens) {
      process_static_branch(
          parser, tokens[0], tokens[1].scope(), tokens[7], tokens[10].scope(), report_error);
    });
    parser.apply_mutations();
  }

  void namespace_mutation(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    /* Parse each namespace declaration. */
    parser().foreach_scope(ScopeType::Namespace, [&](const Scope &scope) {
      /* TODO(fclem): This could be supported using multiple passes. */
      scope.foreach_match("n", [&](const std::vector<Token> &tokens) {
        report_error(ERROR_TOK(tokens[0]), "Nested namespaces are unsupported.");
      });

      string namespace_prefix = namespace_separator_mutation(
          scope.start().prev().full_symbol_name() + "::");
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
          parser.replace(token, namespace_prefix + token.str(), true);
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
      scope.foreach_struct([&](Token, Token struct_name, Scope) { process_symbol(struct_name); });

      /* Pipeline declarations. */
      scope.foreach_match("ww(w", [&](vector<Token> toks) {
        if (toks[0].scope().type() != ScopeType::Namespace || toks[0].str().find("Pipeline") != 0)
        {
          return;
        }
        process_symbol(toks[1]);
      });

      Token namespace_tok = scope.start().prev().namespace_start().prev();
      if (namespace_tok == Namespace) {
        parser.erase(namespace_tok, scope.start());
        parser.erase(scope.end());
      }
      else {
        report_error(ERROR_TOK(namespace_tok), "Expected namespace token.");
      }
    });

    parser.apply_mutations();
  }

  /* Needs to run before namespace mutation so that `using` have more precedence. */
  void using_mutation(Parser &parser, report_callback report_error)
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
        string namespace_name = scope.start().prev().full_symbol_name();
        if (namespace_name != namespace_prefix) {
          report_error(
              ERROR_TOK(using_tok),
              "The `using` keyword is only allowed in namespace scope to make visible symbols "
              "from the same namespace declared in another scope, potentially from another "
              "file.");
          return;
        }
      }

      to = namespace_separator_mutation(to);

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

  std::string namespace_separator_mutation(const std::string &str)
  {
    std::string out = str;

    /* Global namespace reference. */
    replace_all(out, " ::", "   ");
    /* Specific namespace reference.
     * Cannot use `__` because of some compilers complaining about reserved symbols. */
    replace_all(out, "::", "_");
    return out;
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

  void pragmas_mutation(Parser &parser, report_callback /*report_error*/)
  {
    /* Remove unsupported directives. */
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("#ww", [&](const std::vector<Token> &tokens) {
      if (tokens[1].str() == "pragma") {
        if (tokens[2].str() == "once") {
          parser.erase(tokens.front(), tokens.back());
        }
        else if (tokens[2].str() == "runtime_generated") {
          parser.erase(tokens.front(), tokens.back());
        }
      }
    });
    parser.apply_mutations();
  }

  void swizzle_function_mutation(Parser &parser, report_callback /*report_error*/)
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
        parser.replace(tokens[2], tokens[3], "  ");
      }
    });
    parser.apply_mutations();
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

  void parse_library_functions(const std::string &str)
  {
    using namespace metadata;
    std::regex regex_func(R"(void\s+(\w+)\s*\(([^)]+\))\s*\{)");
    regex_global_search(str, regex_func, [&](const std::smatch &match) {
      std::string name = match[1].str();
      std::string args = match[2].str();

      FunctionFormat fn;
      fn.name = name;

      std::regex regex_arg(R"((?:(const|in|out|inout)\s)?(\w+)\s([\w\[\]]+)(?:,|\)))");
      regex_global_search(args, regex_arg, [&](const std::smatch &arg) {
        std::string qualifier = arg[1].str();
        std::string type = arg[2].str();
        if (qualifier.empty() || qualifier == "const") {
          qualifier = "in";
        }
        fn.arguments.emplace_back(
            ArgumentFormat{metadata::Qualifier(hash(qualifier)), metadata::Type(hash(type))});
      });
      metadata.functions.emplace_back(fn);
    });
  }

  void parse_builtins(const std::string &str, const std::string &filename)
  {
    const bool skip_drw_debug = filename.find("draw_debug_draw_lib.glsl") != std::string::npos ||
                                filename.find("draw_debug_infos.hh") != std::string::npos ||
                                filename.find("draw_debug_draw_display_vert.glsl") !=
                                    std::string::npos ||
                                filename.find("draw_shader_shared.hh") != std::string::npos;
    using namespace metadata;
    /* TODO: This can trigger false positive caused by disabled #if blocks. */
    std::string tokens[] = {"gl_FragCoord",
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
                            "drw_debug_",
#ifdef WITH_GPU_SHADER_ASSERT
                            "assert",
#endif
                            "printf"};
    for (auto &token : tokens) {
      if (skip_drw_debug && token == "drw_debug_") {
        continue;
      }
      if (str.find(token) != std::string::npos) {
        metadata.builtins.emplace_back(Builtin(hash(token)));
      }
    }
  }

  void printf_processing(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;
    parser().foreach_match("w(..)", [&](const vector<Token> &tokens) {
      if (tokens[0].str() != "printf") {
        return;
      }

      int arg_count = 0;
      tokens[1].scope().foreach_scope(ScopeType::FunctionArg, [&](const Scope &) { arg_count++; });

      string unrolled;
      tokens[1].scope().foreach_scope(ScopeType::FunctionArg, [&](const Scope &attribute) {
        if (unrolled.empty()) {
          unrolled = "print_header(" + to_string(arg_count) + ", " + attribute.str() + ")";
        }
        else {
          unrolled = "print_data(" + unrolled + ", " + attribute.str() + ")";
        }
      });

      parser.replace(tokens.front(), tokens.back(), unrolled);
    });
    parser.apply_mutations();
  }

  void assert_processing(Parser &parser, const std::string &filepath, report_callback report_error)
  {
    std::string filename = std::regex_replace(filepath, std::regex(R"((?:.*)\/(.*))"), "$1");

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

  void static_strings_parsing(const std::string &str)
  {
    using namespace metadata;
    /* Matches any character inside a pair of un-escaped quote. */
    std::regex regex(R"("(?:[^"])*")");
    regex_global_search(str, regex, [&](const std::smatch &match) {
      std::string format = match[0].str();
      metadata.printf_formats.emplace_back(metadata::PrintfFormat{hash_string(format), format});
    });
  }

  std::string static_strings_mutation(std::string str)
  {
    /* Replaces all matches by the respective string hash. */
    for (const metadata::PrintfFormat &format : metadata.printf_formats) {
      const std::string &str_var = format.format;
      std::regex escape_regex(R"([\\\.\^\$\+\(\)\[\]\{\}\|\?\*])");
      std::string str_regex = std::regex_replace(str_var, escape_regex, "\\$&");

      std::regex regex(str_regex);
      str = std::regex_replace(str, regex, std::to_string(hash_string(str_var)) + 'u');
    }
    return str;
  }

  /* Move all method definition outside of struct definition blocks. */
  void resource_table_parsing(Parser &parser, report_callback report_error)
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
          resource.res_condition = attribute[1].scope().str_with_whitespace();
        }
        else if (type == "frequency") {
          resource.res_frequency = attribute[2].str();
        }
        else if (type == "resource_table") {
          resource.res_type = type;
        }
        else if (type == "legacy_info") {
          /* Name is already stored. */
        }
        else {
          report_error(ERROR_TOK(attribute[0]), "Unrecognized attribute");
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
          report_error(ERROR_TOK(attribute[0]), "Unrecognized attribute");
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
            report_error(ERROR_TOK(attributes[0]), "Unrecognized attribute");
          }
          return attr;
        };

    auto parse_fragment_output =
        [&](Token struct_name, Scope attributes, Token type, Token name, Scope) {
          metadata::ParsedFragOuput frag_out{
              type.line_number(), type.str(), struct_name.str() + "_" + name.str()};

          attributes.foreach_scope(ScopeType::Attribute, [&](const Scope &attribute) {
            std::string type = attribute[0].str();
            if (type == "color") {
              frag_out.slot = attribute[2].str();
            }
            else if (type == "raster_order_group") {
              frag_out.raster_order_group = attribute[2].str();
            }
            else if (type == "color") {
              frag_out.slot = attribute[2].str();
            }
            else if (type == "index") {
              frag_out.dual_source = attribute[2].str();
            }
            else {
              report_error(ERROR_TOK(attribute[0]), "Unrecognized attribute");
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
      return (type == "color" || type == "depth" || type == "stencil");
    };

    parser().foreach_struct([&](Token struct_tok, Token struct_name, Scope body) {
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
          report_error(ERROR_TOK(attributes[1]), "Unrecognized attribute");
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
              parser.erase(attributes.start().line_start(), decl_end.line_end());
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

      Token end_of_srt = body.end().prev();

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
        parser.insert_after(end_of_srt.next().line_end() + 1, access_macros);

        parser.insert_line_number(end_of_srt.next().line_end() + 1,
                                  end_of_srt.next().line_number() + 2);

        /* Insert attribute so that method mutations know that this struct is an SRT. */
        parser.insert_before(struct_tok, "[[resource_table]] ");
      }
    });
    parser.apply_mutations();
  }

  /* Move all method definition outside of struct definition blocks. */
  void stage_interface_parsing(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    auto parse_interface = [&](const std::vector<Token> &tokens) {
      if (tokens[2].scope().str_exclusive() == "vertex_out") {
        Token srt_name = tokens[7];
        Scope body = tokens[8].scope();

        metadata::StageInterface iface;
        iface.name = srt_name.str();

        body.foreach_match("[[..]]ww;", [&](const std::vector<Token> &tokens) {
          Token interpolation_mode = tokens[1].scope()[1];
          Token type = tokens[6];
          Token name = tokens[7];

          metadata::ParsedAttribute attr{type.line_number(),
                                         type.str(),
                                         iface.name + "_" + name.str(),
                                         interpolation_mode.str()};

          iface.emplace_back(attr);
        });

        metadata.stage_interfaces.emplace_back(iface);
        /* Erase SRT definition. The resources are defined by the backend at runtime. */
        /* Note that this might change in the future. */
        parser.erase(tokens[1], tokens[6]);
        parser.erase(tokens[8].scope().start().next(), tokens[8].scope().end().prev());
      }
    };

    parser().foreach_match("s[[..]]w{..};",
                           [&](const std::vector<Token> &tokens) { parse_interface(tokens); });
    parser.apply_mutations();
  }

  void vertex_in_parsing(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("s[[..]]w{..};", [&](const std::vector<Token> &tokens) {
      if (tokens[2].scope().str_exclusive() == "vertex_in") {
        Token srt_name = tokens[7];
        Scope body = tokens[8].scope();

        metadata::VertexInputs iface;
        iface.name = srt_name.str();

        body.foreach_match("[[..]]ww;", [&](const std::vector<Token> &tokens) {
          Scope attributes = tokens[1].scope();
          Token type = tokens[6];
          Token name = tokens[7];

          metadata::ParsedVertInput frag_out{type.line_number(), type.str(), name.str()};

          attributes.foreach_scope(ScopeType::Attribute, [&](const Scope &attribute) {
            std::string type = attribute[0].str();
            if (type == "attribute") {
              frag_out.slot = attribute[2].str();
            }
            else {
              report_error(ERROR_TOK(attribute[0]), "Unrecognized attribute");
            }
          });

          iface.emplace_back(frag_out);
        });

        metadata.vertex_inputs.emplace_back(iface);
        /* Erase SRT definition. The resources are defined by the backend at runtime. */
        /* Note that this might change in the future. */
        parser.erase(tokens[1], tokens[6]);
        parser.erase(tokens[8].scope().start().next(), tokens[8].scope().end().prev());
      }
    });
    parser.apply_mutations();
  }

  void fragment_out_parsing(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("s[[..]]w{..};", [&](const std::vector<Token> &tokens) {
      if (tokens[2].scope().str_exclusive() == "fragment_out") {
        Token srt_name = tokens[7];
        Scope body = tokens[8].scope();

        metadata::FragmentOutputs iface;
        iface.name = srt_name.str();

        body.foreach_match("[[..]]ww;", [&](const std::vector<Token> &tokens) {
          Scope attributes = tokens[1].scope();
          Token type = tokens[6];
          Token name = tokens[7];

          metadata::ParsedFragOuput frag_out{
              type.line_number(), type.str(), iface.name + "_" + name.str()};

          attributes.foreach_scope(ScopeType::Attribute, [&](const Scope &attribute) {
            std::string type = attribute[0].str();
            if (type == "color") {
              frag_out.slot = attribute[2].str();
            }
            else if (type == "raster_order_group") {
              frag_out.raster_order_group = attribute[2].str();
            }
            else if (type == "color") {
              frag_out.slot = attribute[2].str();
            }
            else if (type == "index") {
              frag_out.dual_source = attribute[2].str();
            }
            else {
              report_error(ERROR_TOK(attribute[0]), "Unrecognized attribute");
            }
          });

          iface.emplace_back(frag_out);
        });

        metadata.fragment_outputs.emplace_back(iface);
        /* Erase SRT definition. The resources are defined by the backend at runtime. */
        /* Note that this might change in the future. */
        parser.erase(tokens[1], tokens[6]);
        parser.erase(tokens[8].scope().start().next(), tokens[8].scope().end().prev());
      }
    });
    parser.apply_mutations();
  }

  void static_strings_merging(Parser &parser, report_callback /*report_error*/)
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

  void static_strings_parsing_and_mutation(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_token(String, [&](const Token &token) {
      uint32_t hash = hash_string(token.str());
      metadata::PrintfFormat format = {hash, token.str()};
      metadata.printf_formats.emplace_back(format);
      parser.replace(token, std::to_string(hash) + 'u', true);
    });
    parser.apply_mutations();
  }

  /* Move all method definition outside of struct definition blocks. */
  void struct_method_mutation(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    using Tokens = vector<Token>;

    /* `class` -> `struct` */
    parser().foreach_token(Class, [&](const Token &token) { parser.replace(token, "struct "); });

    /* Erase `public:` and `private:` keywords. */
    parser().foreach_match("v:", [&](const Tokens &t) { parser.erase(t.front(), t.back()); });
    parser().foreach_match("V:", [&](const Tokens &t) { parser.erase(t.front(), t.back()); });

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
            const Token const_tok = is_const ? fn_args.end().next() : Token::invalid();

            if (is_static) {
              parser.replace(
                  fn_name, namespace_separator_mutation(struct_name.str() + "::" + fn_name.str()));
              /* WORKAROUND: Erase the static keyword as it conflicts with the wrapper class
               * member accesses MSL. */
              parser.erase(static_tok);
            }
            else {
              const bool has_no_args = fn_args.token_count() == 2;
              const char *suffix = (has_no_args ? "" : ", ");
              const string prefix = (is_resource_table ? "[[resource_table]] " : "");

              if (is_const && !is_resource_table) {
                parser.erase(const_tok);
                parser.insert_after(fn_args.start(),
                                    prefix + "const " + struct_name.str() + " this_" + suffix);
              }
              else {
                parser.insert_after(fn_args.start(),
                                    prefix + struct_name.str() + " &this_" + suffix);
              }
            }
          });
    });

    parser.apply_mutations();

    /* Copy method functions outside of struct scope. */
    parser().foreach_struct([&](Token, const Token, const Scope struct_scope) {
      const Token struct_end = struct_scope.end().next();
      struct_scope.foreach_function(
          [&](bool is_static, Token fn_type, Token, Scope, bool, Scope fn_body) {
            const Token fn_start = is_static ? fn_type.prev() : fn_type;

            string fn_str = parser.substr_range_inclusive(fn_start.line_start(),
                                                          fn_body.end().line_end() + 1);

            parser.erase(fn_start, fn_body.end());
            parser.insert_line_number(struct_end.line_end() + 1, fn_start.line_number());
            parser.insert_after(struct_end.line_end() + 1, fn_str);
          });

      parser.insert_line_number(struct_end.line_end() + 1, struct_end.line_number() + 1);
    });

    parser.apply_mutations();
  }

  /* Add padding member to empty structs.
   * Empty structs are useful for templating. */
  void empty_struct_mutation(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("sw{};", [&](const std::vector<Token> &tokens) {
      parser.insert_after(tokens[2], "int _pad;");
    });
    parser.apply_mutations();
  }

  /* Transform `a.fn(b)` into `fn(a, b)`. */
  void method_call_mutation(Parser &parser, report_callback report_error)
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
              start_of_this = start_of_this.scope().start().prev();
              break;
            }
            if (start_of_this == ']') {
              /* Array subscript. Take scope and continue. */
              start_of_this = start_of_this.scope().start().prev();
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
                         "method_call_mutation parsing error");
            break;
          }
          string this_str = parser.substr_range_inclusive(start_of_this, end_of_this);
          string func_str = func.str();
          const bool has_no_arg = par_open.next() == ')';
          /* `a.fn(b)` -> `fn(a, b)` */
          parser.replace_try(
              start_of_this, par_open, func_str + "(" + this_str + (has_no_arg ? "" : ", "));
        });
      });
    } while (parser.apply_mutations());
  }

  void pipeline_parse_and_remove(Parser &parser,
                                 const std::string &filepath,
                                 report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;
    using namespace metadata;

    const std::string filename = std::regex_replace(filepath, std::regex(R"((?:.*)\/(.*))"), "$1");

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
        tok = scope.end().next();
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
      create_info_decl += "GPU_SHADER_CREATE_END()\n";

      metadata.create_infos_declarations.emplace_back(create_info_decl);
    };

    parser().foreach_match("ww(w", [&](const std::vector<Token> &tokens) {
      Scope parameters = tokens[2].scope();
      if (tokens[0].str() == "PipelineGraphic") {
        process_graphic_pipeline(tokens[1], parameters);
        parser.erase(tokens.front(), parameters.end().next());
      }
      else if (tokens[0].str() == "PipelineCompute") {
        process_compute_pipeline(tokens[1], parameters);
        parser.erase(tokens.front(), parameters.end().next());
      }
    });
  }

  void stage_function_mutation(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_function(
        [&](bool is_static, Token fn_type, Token, Scope, bool, Scope fn_body) {
          Token attr_tok = (is_static) ? fn_type.prev().prev() : fn_type.prev();
          if (attr_tok.is_invalid() || attr_tok != ']' || attr_tok.prev() != ']') {
            return;
          }
          Scope attribute = attr_tok.prev().scope();
          if (attribute.type() != ScopeType::Attributes) {
            return;
          }

          const string attr = attribute.str_exclusive();
          parser.erase(attribute.scope());

          string condition = "defined(";
          if (attr == "gpu::vertex_function" || attr == "vertex") {
            condition += "GPU_VERTEX_SHADER";
          }
          else if (attr == "gpu::fragment_function" || attr == "fragment") {
            condition += "GPU_FRAGMENT_SHADER";
          }
          else if (attr == "gpu::compute_function" || attr == "compute") {
            condition += "GPU_COMPUTE_SHADER";
          }
          else {
            return;
          }
          condition += ")";

          guarded_scope_mutation(parser, fn_body, condition);
        });
    parser.apply_mutations();
  }

  void srt_guard_mutation(Parser &parser, report_callback /*report_error*/)
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
        parser.erase(tokens[0].scope());
      });

      if (!condition.empty()) {
        parser.insert_directive(fn_type.prev(), "#if " + condition);
        parser.insert_directive(fn_body.end(), "#endif");
      }
    });

    parser.apply_mutations();
  }

  void resource_guard_mutation(Parser &parser, report_callback /*report_error*/)
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

    string line_start = "#line " + std::to_string(scope.start().next().line_number()) + "\n";
    string line_end = "#line " + std::to_string(scope.end().line_number()) + "\n";

    string guard_start = "#if " + condition + "\n";
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
    string guard_end = "#endif\n";

    parser.insert_after(scope.start().line_end() + 1, guard_start + line_start);
    parser.insert_before(scope.end().line_start(), guard_else + guard_end + line_end);
  };

  std::string guarded_scope_mutation(std::string content, int64_t line_start, std::string check)
  {
    int64_t line_end = line_start + line_count(content);
    std::string guarded_cope;
    guarded_cope += "#if " + check + "\n";
    guarded_cope += "#line " + std::to_string(line_start) + "\n";
    guarded_cope += content;
    guarded_cope += "#endif\n";
    guarded_cope += "#line " + std::to_string(line_end) + "\n";
    return guarded_cope;
  }

  void enum_macro_injection(Parser &parser, bool is_shared_file, report_callback report_error)
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
     * - All values needs to be specified using constant literals to avoid compiler differences.
     * - All values needs to have the 'u' suffix to avoid GLSL compiler errors.
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

    auto process_enum =
        [&](Token enum_tok, Token class_tok, Token enum_name, Token enum_type, Scope enum_scope) {
          string type_str = enum_type.str();

          if (is_shared_file) {
            if (type_str != "uint32_t" && type_str != "int32_t") {
              report_error(
                  enum_type.line_number(),
                  enum_type.char_number(),
                  enum_type.line_str(),
                  "enum declaration must use uint32_t or int32_t underlying type for interface "
                  "compatibility");
              return;
            }
          }

          size_t insert_at = enum_scope.end().line_end();
          parser.erase(enum_tok.str_index_start(), insert_at);
          parser.insert_line_number(insert_at + 1, enum_tok.line_number());
          parser.insert_after(insert_at + 1,
                              "#define " + enum_name.str() + " " + enum_type.str() + "\n");

          enum_scope.foreach_scope(ScopeType::Assignment, [&](Scope scope) {
            string name = scope.start().prev().str();
            string value = scope.str_with_whitespace();
            if (class_tok.is_valid()) {
              name = enum_name.str() + "::" + name;
            }
            string decl = "constant static constexpr " + type_str + " " + name + " " + value +
                          ";\n";
            parser.insert_line_number(insert_at + 1, scope.start().line_number());
            parser.insert_after(insert_at + 1, decl);
          });
          parser.insert_line_number(insert_at + 1, enum_scope.end().line_number() + 1);
        };

    parser().foreach_match("MSw:w{", [&](vector<Token> tokens) {
      process_enum(tokens[0], tokens[1], tokens[2], tokens[4], tokens[5].scope());
    });
    parser().foreach_match("Mw:w{", [&](vector<Token> tokens) {
      process_enum(tokens[0], Token::invalid(), tokens[1], tokens[3], tokens[4].scope());
    });

    parser.apply_mutations();

    parser().foreach_match("M", [&](vector<Token> tokens) {
      report_error(tokens[0].line_number(),
                   tokens[0].char_number(),
                   tokens[0].line_str(),
                   "invalid enum declaration");
    });
  }

  std::string strip_whitespace(const std::string &str) const
  {
    return str.substr(0, str.find_last_not_of(" \n") + 1);
  }

  /**
   * Expand functions with default arguments to function overloads.
   * Expects formatted input and that function bodies are followed by newline.
   */
  void default_argument_mutation(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_function(
        [&](bool, Token fn_type, Token fn_name, Scope fn_args, bool, Scope fn_body) {
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
              args_names += comma + arg.end().str();
            }
            else {
              string arg_name = equal.prev().str();
              string value = parser.substr_range_inclusive(equal.next(), arg.end());
              string decl = parser.substr_range_inclusive(arg.start(), equal.prev());

              string fn_call = fn_name.str() + '(' + args_names + comma + value + ");";
              if (has_non_void_return_type) {
                fn_call = "return " + fn_call;
              }
              string overload;
              overload += fn_type.str() + " ";
              overload += fn_name.str() + '(' + args_decl + ")\n";
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
          size_t end_of_fn_char = fn_body.end().line_end() + 1;
          /* Have to reverse the declaration order. */
          for (auto it = fn_overloads.rbegin(); it != fn_overloads.rend(); ++it) {
            parser.insert_line_number(end_of_fn_char, fn_type.line_number());
            parser.insert_after(end_of_fn_char, *it);
          }
          parser.insert_line_number(end_of_fn_char, fn_body.end().line_number() + 1);
        });

    parser.apply_mutations();
  }

  /* Successive mutations can introduce a lot of unneeded line directives. */
  void cleanup_line_directives(Parser &parser, report_callback /*report_error*/)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_match("#w0\n#w0\n", [&](vector<Token> toks) {
      parser.replace(toks[0].line_start(), toks[0].line_end() + 1, "");
    });
    parser.apply_mutations();

    parser().foreach_match("#w0\n#w\n#w0\n", [&](vector<Token> toks) {
      parser.replace(toks[0].line_start(), toks[0].line_end() + 1, "");
    });
    parser.apply_mutations();

    parser().foreach_match("#w0\n", [&](vector<Token> toks) {
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
  void argument_reference_mutation(Parser &parser, report_callback /*report_error*/)
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

  /**
   * For safety reason, nested resource tables need to be declared with the srt_t template.
   * This avoid chained member access which isn't well defined with the preprocessing we are doing.
   *
   * This linting phase make sure that [[resource_table]] members uses it and that no incorrect
   * usage is made. We also remove this template because it has no real meaning.
   *
   * Need to run before resource_table_parsing.
   */
  void srt_template_linter_and_mutation(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_struct([&](Token, Token, Scope body) {
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
        parser.erase(template_scope.start());
        parser.erase(template_scope.end());
      });
    });
    parser.apply_mutations();
  }

  /* Need to run before local reference mutations. */
  void srt_member_access_mutation(Parser &parser, report_callback report_error)
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

  void entry_point_parsing_and_mutation(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;
    using namespace metadata;

    parser().foreach_function(
        [&](bool, Token type, Token fn_name, Scope args, bool, Scope fn_body) {
          bool is_entry_point = false;
          bool is_compute_func = false;
          bool is_vertex_func = false;
          bool is_fragment_func = false;

          if (type.prev() == ']') {
            Scope attributes = type.prev().prev().scope();
            if (attributes.type() == ScopeType::Attributes) {
              string attribute = attributes.str_with_whitespace();

              if (attribute == "[vertex]") {
                is_vertex_func = true;
              }
              else if (attribute == "[fragment]") {
                is_fragment_func = true;
              }
              else if (attribute == "[compute]") {
                is_compute_func = true;
              }
              is_entry_point = true;
            }
          }

          if (is_entry_point && type.str() != "void") {
            report_error(ERROR_TOK(type), "Entry point function must return void.");
            return;
          }

          if (is_entry_point && args.str() != "()") {
            parser.erase(args.start().next(), args.end().prev());
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
            else if (srt_attr == "position" && is_entry_point) {
              if (is_compute_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[position]] is only supported in vertex or fragment functions.");
              }
              else if (is_vertex_func && (is_const || srt_type != "float4")) {
                report_error(
                    ERROR_TOK(type),
                    "[[position]] must be declared as non-const reference (aka `float4 &`).");
              }
              else if (is_fragment_func && (!is_const || srt_type != "float4")) {
                report_error(ERROR_TOK(type), "[[position]] must be declared as `const float4`.");
              }
              replace_word(srt_var, "gl_Position");
            }
            else if (srt_attr == "stage_in") {
              if (is_compute_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[stage_in]] is only supported in vertex and fragment functions.");
              }
              else if (!is_const) {
                report_error(ERROR_TOK(type), "[[stage_in]] must be declared as const reference.");
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
            else if (srt_attr == "stage_out") {
              if (is_compute_func) {
                report_error(ERROR_TOK(attributes[1]),
                             "[[stage_out]] is only supported in vertex and fragment functions.");
              }
              else if (is_const) {
                report_error(ERROR_TOK(type),
                             "[[stage_out]] must be declared as non-const reference.");
              }
              else if (is_vertex_func) {
                replace_word_and_accessor(srt_var, srt_type + "_");
                create_info_decl += "VERTEX_OUT(" + srt_type + ")\n";
              }
              else if (is_fragment_func) {
                replace_word_and_accessor(srt_var, srt_type + "_");
                create_info_decl += "ADDITIONAL_INFO(" + srt_type + ")\n";
              }
            }
            else if (srt_attr == "resource_table") {
              if (is_entry_point) {
                /* Add dummy var at start of function body. */
                parser.insert_after(fn_body.start().str_index_start(),
                                    " " + srt_type + " " + srt_var + ";");
                create_info_decl += "ADDITIONAL_INFO(" + srt_type + ")\n";
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

          create_info_decl += "GPU_SHADER_CREATE_END()\n";

          metadata.create_infos_declarations.emplace_back(create_info_decl);
        });

    parser.apply_mutations();
  }

  /* To be run after `argument_reference_mutation()`. */
  void variable_reference_mutation(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_function([&](bool, Token, Token, Scope fn_args, bool, Scope fn_scope) {
      fn_scope.foreach_match("c?w&w=", [&](const vector<Token> &tokens) {
        const Token name = tokens[4];
        const Scope assignment = tokens[5].scope();

        Token decl_start = tokens[0].is_valid() ? tokens[0] : tokens[2];
        /* Take attribute into account. */
        decl_start = (decl_start.prev() == ']') ? decl_start.prev().scope().start() : decl_start;
        /* Take ending ; into account. */
        const Token decl_end = assignment.end().next();

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
                ERROR_TOK(subscript.start()),
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

        string definition = parser.substr_range_inclusive(assignment[1], assignment.end());

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
  void global_scope_constant_linting(Parser &parser, report_callback report_error)
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

  void quote_linting(const Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    /* This only catches some invalid usage. For the rest, the CI will catch them. */
    parser().foreach_token(TokenType::String, [&](const Token token) {
      report_error(ERROR_TOK(token),
                   "Unprocessed string literal. "
                   "Strings are forbidden in GLSL.");
    });
  }

  void small_type_linting(Parser &parser, report_callback report_error)
  {
    using namespace std;
    using namespace shader::parser;

    parser().foreach_scope(ScopeType::Struct, [&](const Scope scope) {
      scope.foreach_match("ww;", [&](const vector<Token> tokens) {
        string type = tokens[0].str();
        if (type.find("char") != string::npos || type.find("short") != string::npos ||
            type.find("half") != string::npos)
        {
          report_error(ERROR_TOK(tokens[0]), "Small types are forbidden in shader interfaces.");
        }
      });
    });
  }

  std::string line_directive_prefix(const std::string &filepath)
  {
    std::string filename = std::regex_replace(filepath, std::regex(R"((?:.*)\/(.*))"), "$1");

    std::stringstream suffix;
    /* NOTE: This is not supported by GLSL. All line directives are muted at runtime and the
     * sources are scanned after error reporting for the locating the muted line. */
    suffix << "#line 1 \"" << filename << "\"\n";
    return suffix.str();
  }

  /* Made public for unit testing purpose. */
 public:
  static void replace_all(std::string &str, const std::string &from, const std::string &to)
  {
    if (from.empty()) {
      return;
    }
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
      str.replace(start_pos, from.length(), to);
      start_pos += to.length();
    }
  }

  static void replace_all(std::string &str, const char from, const char to)
  {
    for (char &string_char : str) {
      if (string_char == from) {
        string_char = to;
      }
    }
  }

  static int64_t char_count(const std::string &str, char c)
  {
    return std::count(str.begin(), str.end(), c);
  }

  static int64_t line_count(const std::string &str)
  {
    return char_count(str, '\n');
  }
};

}  // namespace blender::gpu::shader
