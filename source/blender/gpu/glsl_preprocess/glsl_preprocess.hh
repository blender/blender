/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup glsl_preprocess
 */

#pragma once

#include <cstdint>
#include <functional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace blender::gpu::shader {

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
  FrontFacing = hash("gl_FrontFacing"),
  GlobalInvocationID = hash("gl_GlobalInvocationID"),
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

struct Source {
  std::vector<Builtin> builtins;
  /* Note: Could be a set, but for now the order matters. */
  std::vector<std::string> dependencies;
  std::vector<PrintfFormat> printf_formats;
  std::vector<FunctionFormat> functions;

  std::string serialize(const std::string &function_name) const
  {
    std::stringstream ss;
    ss << "static void " << function_name
       << "(GPUSource &source, GPUFunctionDictionnary *g_functions, GPUPrintFormatMap *g_formats) "
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
    for (auto format : printf_formats) {
      ss << "  source.add_printf_format(uint32_t(" << std::to_string(format.hash) << "), "
         << format.format << ", g_formats);\n";
    }
    /* Avoid warnings. */
    ss << "  UNUSED_VARS(source, g_functions, g_formats);\n";
    ss << "}\n";
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
  using report_callback = std::function<void(const std::smatch &, const char *)>;
  struct SharedVar {
    std::string type;
    std::string name;
    std::string array;
  };

  std::vector<SharedVar> shared_vars_;

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
      report_error(std::smatch(), "Unknown file type");
      return "";
    }
    str = remove_comments(str, report_error);
    threadgroup_variables_parsing(str);
    parse_builtins(str, filename);
    if (language == BLENDER_GLSL || language == CPP) {
      if (do_parse_function) {
        parse_library_functions(str);
      }
      if (language == BLENDER_GLSL) {
        include_parse(str, report_error);
        pragma_runtime_generated_parsing(str);
        pragma_once_linting(str, filename, report_error);
      }
      str = preprocessor_directive_mutation(str);
      str = swizzle_function_mutation(str);
      if (language == BLENDER_GLSL) {
        str = stage_function_mutation(str);
        str = resource_guard_mutation(str, report_error);
        str = loop_unroll(str, report_error);
        str = assert_processing(str, filename);
        static_strings_parsing(str);
        str = static_strings_mutation(str);
        str = printf_processing(str, report_error);
        quote_linting(str, report_error);
      }
      global_scope_constant_linting(str, report_error);
      matrix_constructor_linting(str, report_error);
      array_constructor_linting(str, report_error);
      if (do_small_type_linting) {
        small_type_linting(str, report_error);
      }
      str = remove_quotes(str);
      if (language == BLENDER_GLSL) {
        str = using_mutation(str, report_error);
        str = namespace_mutation(str, report_error);
        str = namespace_separator_mutation(str);
      }
      str = enum_macro_injection(str);
      str = default_argument_mutation(str);
      str = argument_reference_mutation(str);
      str = variable_reference_mutation(str, report_error);
      str = template_definition_mutation(str, report_error);
      str = template_call_mutation(str);
    }
#ifdef __APPLE__ /* Limiting to Apple hardware since GLSL compilers might have issues. */
    if (language == GLSL) {
      str = matrix_constructor_mutation(str);
    }
#endif
    str = argument_decorator_macro_injection(str);
    str = array_constructor_macro_injection(str);
    r_metadata = metadata;
    return line_directive_prefix(filename) + str + threadgroup_variables_suffix();
  }

  /* Variant use for python shaders. */
  std::string process(const std::string &str)
  {
    auto no_err_report = [](std::smatch, const char *) {};
    metadata::Source unused;
    return process(GLSL, str, "", false, false, no_err_report, unused);
  }

 private:
  using regex_callback = std::function<void(const std::smatch &)>;

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
        /* TODO(fclem): Add line / char position to report. */
        report_error(std::smatch(), "Malformed multi-line comment.");
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
        /* TODO(fclem): Add line / char position to report. */
        report_error(std::smatch(), "Malformed single line comment, missing newline.");
        return out_str;
      }
    }
    /* Remove trailing white space as they make the subsequent regex much slower. */
    std::regex regex(R"((\ )*?\n)");
    return std::regex_replace(out_str, regex, "\n");
  }

  std::string template_definition_mutation(const std::string &str, report_callback &report_error)
  {
    if (str.find("template") == std::string::npos) {
      return str;
    }

    std::string out_str = str;
    {
      /* Transform template definition into macro declaration. */
      std::regex regex(R"(template<([\w\d\n\,\ ]+)>(\s\w+\s)(\w+)\()");
      out_str = std::regex_replace(out_str, regex, "#define $3_TEMPLATE($1)$2$3@(");
    }
    {
      /* Add backslash for each newline in template macro. */
      size_t start, end = 0;
      while ((start = out_str.find("_TEMPLATE(", end)) != std::string::npos) {
        /* Remove parameter type from macro argument list. */
        end = out_str.find(")", start);
        std::string arg_list = out_str.substr(start, end - start);
        arg_list = std::regex_replace(arg_list, std::regex(R"(\w+ (\w+))"), "$1");
        out_str.replace(start, end - start, arg_list);

        std::string template_body = get_content_between_balanced_pair(
            out_str.substr(start), '{', '}');
        if (template_body.empty()) {
          /* Empty body is unlikely to happen. This limitation can be worked-around by using a noop
           * comment inside the function body. */
          report_error(
              std::smatch(),
              "Template function declaration is missing closing bracket or has empty body.");
          break;
        }
        size_t body_end = out_str.find('{', start) + 1 + template_body.size();
        /* Contains "_TEMPLATE(macro_args) void fn@(fn_args) { body;". */
        std::string macro_body = out_str.substr(start, body_end - start);

        macro_body = std::regex_replace(macro_body, std::regex(R"(\n)"), " \\\n");

        std::string macro_args = get_content_between_balanced_pair(macro_body, '(', ')');
        /* Find function argument list.
         * Skip first 10 chars to skip "_TEMPLATE" and the argument list. */
        std::string fn_args = get_content_between_balanced_pair(
            macro_body.substr(10 + macro_args.length() + 1), '(', ')');
        /* Remove white-spaces. */
        macro_args = std::regex_replace(macro_args, std::regex(R"(\s)"), "");
        std::vector<std::string> macro_args_split = split_string(macro_args, ',');
        /* Append arguments inside the function name. */
        std::string fn_name_suffix = "_";
        bool all_args_in_function_signature = true;
        for (std::string macro_arg : macro_args_split) {
          fn_name_suffix += "##" + macro_arg + "##_";
          /* Search macro arguments inside the function arguments types. */
          if (std::regex_search(fn_args, std::regex(R"(\b)" + macro_arg + R"(\b)")) == false) {
            all_args_in_function_signature = false;
          }
        }
        if (all_args_in_function_signature) {
          /* No need for suffix. Use overload for type deduction.
           * Otherwise, we require full explicit template call. */
          fn_name_suffix = "";
        }
        size_t end_of_fn_name = macro_body.find("@");
        macro_body.replace(end_of_fn_name, 1, fn_name_suffix);

        out_str.replace(start, body_end - start, macro_body);
      }
    }
    {
      /* Replace explicit instantiation by macro call. */
      /* Only `template ret_t fn<T>(args);` syntax is supported. */
      std::regex regex_instance(R"(template \w+ (\w+)<([\w+\,\ \n]+)>\(([\w+\ \,\n]+)\);)");
      /* Notice the stupid way of keeping the number of lines the same by copying the argument list
       * inside a multi-line comment. */
      out_str = std::regex_replace(out_str, regex_instance, "$1_TEMPLATE($2)/*$3*/");
    }
    {
      /* Check if there is no remaining declaration and instantiation that were not processed. */
      if (out_str.find("template<") != std::string::npos) {
        std::regex regex_declaration(R"(\btemplate<)");
        regex_global_search(out_str, regex_declaration, [&](const std::smatch &match) {
          report_error(match, "Template declaration unsupported syntax");
        });
      }
      if (out_str.find("template ") != std::string::npos) {
        std::regex regex_instance(R"(\btemplate )");
        regex_global_search(out_str, regex_instance, [&](const std::smatch &match) {
          report_error(match, "Template instantiation unsupported syntax");
        });
      }
    }
    return out_str;
  }

  std::string template_call_mutation(std::string &str)
  {
    while (true) {
      std::smatch match;
      if (std::regex_search(str, match, std::regex(R"(([\w\d]+)<([\w\d\n, ]+)>)")) == false) {
        break;
      }
      const std::string template_name = match[1].str();
      const std::string template_args = match[2].str();

      std::string replacement = "TEMPLATE_GLUE" +
                                std::to_string(char_count(template_args, ',') + 1) + "(" +
                                template_name + ", " + template_args + ")";

      replace_all(str, match[0].str(), replacement);
    }
    return str;
  }

  std::string remove_quotes(const std::string &str)
  {
    return std::regex_replace(str, std::regex(R"(["'])"), " ");
  }

  void include_parse(const std::string &str, report_callback report_error)
  {
    /* Parse include directive before removing them. */
    std::regex regex(R"(#(\s*)include\s*\"(\w+\.\w+)\")");

    regex_global_search(str, regex, [&](const std::smatch &match) {
      std::string indent = match[1].str();
      /* Assert that includes are not nested in other preprocessor directives. */
      if (!indent.empty()) {
        report_error(match, "#include directives must not be inside #if clause");
      }
      std::string dependency_name = match[2].str();
      /* Assert that includes are at the top of the file. */
      if (dependency_name == "gpu_glsl_cpp_stubs.hh") {
        /* Skip GLSL-C++ stubs. They are only for IDE linting. */
        return;
      }
      if (dependency_name.find("info.hh") != std::string::npos) {
        /* Skip info files. They are only for IDE linting. */
        return;
      }
      metadata.dependencies.emplace_back(dependency_name);
    });
  }

  void pragma_runtime_generated_parsing(const std::string &str)
  {
    if (str.find("\n#pragma runtime_generated") != std::string::npos) {
      metadata.builtins.emplace_back(metadata::Builtin::runtime_generated);
    }
  }

  void pragma_once_linting(const std::string &str,
                           const std::string &filename,
                           report_callback report_error)
  {
    if (filename.find("_lib.") == std::string::npos) {
      return;
    }
    if (str.find("\n#pragma once") == std::string::npos) {
      std::smatch match;
      report_error(match, "Library files must contain #pragma once directive.");
    }
  }

  std::string loop_unroll(const std::string &str, report_callback report_error)
  {
    if (str.find("[[gpu::unroll") == std::string::npos) {
      return str;
    }

    struct Loop {
      /* `[[gpu::unroll]] for (int i = 0; i < 10; i++)` */
      std::string definition;
      /* `{ some_computation(i); }` */
      std::string body;
      /* `int i = 0` */
      std::string init_statement;
      /* `i < 10` */
      std::string test_statement;
      /* `i++` */
      std::string iter_statement;
      /* Spaces and newline between loop start and body. */
      std::string body_prefix;
      /* Spaces before the loop definition. */
      std::string indent;
      /* `10` */
      int64_t iter_count;
      /* Line at which the loop was defined. */
      int64_t definition_line;
      /* Line at which the body starts. */
      int64_t body_line;
      /* Line at which the body ends. */
      int64_t end_line;
    };

    std::vector<Loop> loops;

    auto add_loop = [&](Loop &loop,
                        const std::smatch &match,
                        int64_t line,
                        int64_t lines_in_content) {
      std::string suffix = match.suffix().str();
      loop.body = get_content_between_balanced_pair(loop.definition + suffix, '{', '}');
      loop.body = '{' + loop.body + '}';
      loop.definition_line = line - lines_in_content;
      loop.body_line = line;
      loop.end_line = loop.body_line + line_count(loop.body);

      /* Check that there is no unsupported keywords in the loop body. */
      if (loop.body.find(" break;") != std::string::npos ||
          loop.body.find(" continue;") != std::string::npos)
      {
        /* Expensive check. Remove other loops and switch scopes inside the unrolled loop scope and
         * check again to avoid false positive. */
        std::string modified_body = loop.body;

        std::regex regex_loop(R"( (for|while|do) )");
        regex_global_search(loop.body, regex_loop, [&](const std::smatch &match) {
          std::string inner_scope = get_content_between_balanced_pair(match.suffix(), '{', '}');
          replace_all(modified_body, inner_scope, "");
        });

        /* Checks if `continue` exists, even in switch statement inside the unrolled loop scope. */
        if (modified_body.find(" continue;") != std::string::npos) {
          report_error(match, "Error: Unrolled loop cannot contain \"continue\" statement.");
        }

        std::regex regex_switch(R"( switch )");
        regex_global_search(loop.body, regex_switch, [&](const std::smatch &match) {
          std::string inner_scope = get_content_between_balanced_pair(match.suffix(), '{', '}');
          replace_all(modified_body, inner_scope, "");
        });

        /* Checks if `break` exists inside the unrolled loop scope. */
        if (modified_body.find(" break;") != std::string::npos) {
          report_error(match, "Error: Unrolled loop cannot contain \"break\" statement.");
        }
      }
      loops.emplace_back(loop);
    };

    /* Parse the loop syntax. */
    {
      /* [[gpu::unroll]]. */
      std::regex regex(R"(( *))"
                       R"(\[\[gpu::unroll\]\])"
                       R"(\s*for\s*\()"
                       R"(\s*((?:uint|int)\s+(\w+)\s+=\s+(-?\d+));)" /* Init statement. */
                       R"(\s*((\w+)\s+(>|<)(=?)\s+(-?\d+)))"         /* Conditional statement. */
                       R"(\s*(?:&&)?\s*([^;)]+)?;)"       /* Extra conditional statement. */
                       R"(\s*(((\w+)(\+\+|\-\-))[^\)]*))" /* Iteration statement. */
                       R"(\)(\s*))");

      int64_t line = 0;

      regex_global_search(str, regex, [&](const std::smatch &match) {
        std::string counter_1 = match[3].str();
        std::string counter_2 = match[6].str();
        std::string counter_3 = match[13].str();

        std::string content = match[0].str();
        int64_t lines_in_content = line_count(content);

        line += line_count(match.prefix().str()) + lines_in_content;

        if ((counter_1 != counter_2) || (counter_1 != counter_3)) {
          report_error(match, "Error: Non matching loop counter variable.");
          return;
        }

        Loop loop;

        int64_t init = std::stol(match[4].str());
        int64_t end = std::stol(match[9].str());
        /* TODO(fclem): Support arbitrary strides (aka, arbitrary iter statement). */
        loop.iter_count = std::abs(end - init);

        std::string condition = match[7].str();
        if (condition.empty()) {
          report_error(match, "Error: Unsupported condition in unrolled loop.");
        }

        std::string equal = match[8].str();
        if (equal == "=") {
          loop.iter_count += 1;
        }

        std::string iter = match[14].str();
        if (iter == "++") {
          if (condition == ">") {
            report_error(match, "Error: Unsupported condition in unrolled loop.");
          }
        }
        else if (iter == "--") {
          if (condition == "<") {
            report_error(match, "Error: Unsupported condition in unrolled loop.");
          }
        }
        else {
          report_error(match, "Error: Unsupported for loop expression. Expecting ++ or --");
        }

        loop.definition = content;
        loop.indent = match[1].str();
        loop.init_statement = match[2].str();
        if (!match[10].str().empty()) {
          loop.test_statement = "if (" + match[10].str() + ") ";
        }
        loop.iter_statement = match[11].str();
        loop.body_prefix = match[15].str();

        add_loop(loop, match, line, lines_in_content);
      });
    }
    {
      /* [[gpu::unroll(n)]]. */
      std::regex regex(R"(( *))"
                       R"(\[\[gpu::unroll\((\d+)\)\]\])"
                       R"(\s*for\s*\()"
                       R"(\s*([^;]*);)"
                       R"(\s*([^;]*);)"
                       R"(\s*([^)]*))"
                       R"(\)(\s*))");

      int64_t line = 0;

      regex_global_search(str, regex, [&](const std::smatch &match) {
        std::string content = match[0].str();

        int64_t lines_in_content = line_count(content);

        line += line_count(match.prefix().str()) + lines_in_content;

        Loop loop;
        loop.iter_count = std::stol(match[2].str());
        loop.definition = content;
        loop.indent = match[1].str();
        loop.init_statement = match[3].str();
        loop.test_statement = "if (" + match[4].str() + ") ";
        loop.iter_statement = match[5].str();
        loop.body_prefix = match[13].str();

        add_loop(loop, match, line, lines_in_content);
      });
    }

    std::string out = str;

    /* Copy paste loop iterations. */
    for (const Loop &loop : loops) {
      std::string replacement = loop.indent + "{ " + loop.init_statement + ";";
      for (int64_t i = 0; i < loop.iter_count; i++) {
        replacement += std::string("\n#line ") + std::to_string(loop.body_line + 1) + "\n";
        replacement += loop.indent + loop.test_statement + loop.body;
        replacement += std::string("\n#line ") + std::to_string(loop.definition_line + 1) + "\n";
        replacement += loop.indent + loop.iter_statement + ";";
        if (i == loop.iter_count - 1) {
          replacement += std::string("\n#line ") + std::to_string(loop.end_line + 1) + "\n";
          replacement += loop.indent + "}";
        }
      }

      std::string replaced = loop.definition + loop.body;

      /* Replace all occurrences in case of recursive unrolling. */
      replace_all(out, replaced, replacement);
    }

    /* Check for remaining keywords. */
    if (out.find("[[gpu::unroll") != std::string::npos) {
      regex_global_search(str, std::regex(R"(\[\[gpu::unroll)"), [&](const std::smatch &match) {
        report_error(match, "Error: Incompatible format for [[gpu::unroll]].");
      });
    }

    return out;
  }

  std::string namespace_mutation(const std::string &str, report_callback report_error)
  {
    if (str.find("namespace") == std::string::npos) {
      return str;
    }

    std::string out = str;

    /* Parse each namespace declaration. */
    std::regex regex(R"(namespace (\w+(?:\:\:\w+)*))");
    regex_global_search(str, regex, [&](const std::smatch &match) {
      std::string namespace_name = match[1].str();
      std::string content = get_content_between_balanced_pair(match.suffix().str(), '{', '}');

      if (content.find("namespace") != std::string::npos) {
        report_error(match, "Nested namespaces are unsupported.");
        return;
      }

      std::string out_content = content;

      /* Parse all global symbols (struct / functions) inside the content. */
      std::regex regex(R"([\n\>] ?(?:const )?(\w+) (\w+)\(?)");
      regex_global_search(content, regex, [&](const std::smatch &match) {
        std::string return_type = match[1].str();
        if (return_type == "template") {
          /* Matched a template instantiation. */
          return;
        }
        std::string function = match[2].str();
        /* Replace all occurrences of the non-namespace specified symbol.
         * Reject symbols that contain the target symbol name. */
        std::regex regex(R"(([^:\w]))" + function + R"(([\s\(\<]))");
        out_content = std::regex_replace(
            out_content, regex, "$1" + namespace_name + "::" + function + "$2");
      });

      replace_all(out, "namespace " + namespace_name + " {" + content + "}", out_content);
    });

    return out;
  }

  /* Needs to run before namespace mutation so that `using` have more precedence. */
  std::string using_mutation(const std::string &str, report_callback report_error)
  {
    using namespace std;

    if (str.find("using ") == string::npos) {
      return str;
    }

    if (str.find("using namespace ") != string::npos) {
      regex_global_search(str, regex(R"(\busing namespace\b)"), [&](const smatch &match) {
        report_error(match,
                     "Unsupported `using namespace`. "
                     "Add individual `using` directives for each needed symbol.");
      });
      return str;
    }

    string next_str = str;

    string out_str;
    /* Using namespace symbol. Example: `using A::B;` */
    /* Using as type alias. Example: `using S = A::B;` */
    regex regex_using(R"(\busing (?:(\w+) = )?(([\w\:\<\>]+)::(\w+));)");

    smatch match;
    while (regex_search(next_str, match, regex_using)) {
      const string using_definition = match[0].str();
      const string alias = match[1].str();
      const string to = match[2].str();
      const string namespace_prefix = match[3].str();
      const string symbol = match[4].str();
      const string prefix = match.prefix().str();
      const string suffix = match.suffix().str();

      out_str += prefix;
      /* Assumes formatted input. */
      if (prefix.back() == '\n') {
        /* Using the keyword in global or at namespace scope. */
        const string parent_scope = get_content_between_balanced_pair(
            out_str + '}', '{', '}', true);
        if (parent_scope.empty()) {
          report_error(match, "The `using` keyword is not allowed in global scope.");
          break;
        }
        /* Ensure we are bringing symbols from the same namespace.
         * Otherwise we can have different shadowing outcome between shader and C++. */
        const string ns_keyword = "namespace ";
        size_t pos = out_str.rfind(ns_keyword, out_str.size() - parent_scope.size());
        if (pos == string::npos) {
          report_error(match, "Couldn't find `namespace` keyword at beginning of scope.");
          break;
        }
        size_t start = pos + ns_keyword.size();
        size_t end = out_str.size() - parent_scope.size() - start - 2;
        const string namespace_scope = out_str.substr(start, end);
        if (namespace_scope != namespace_prefix) {
          report_error(
              match,
              "The `using` keyword is only allowed in namespace scope to make visible symbols "
              "from the same namespace declared in another scope, potentially from another "
              "file.");
          break;
        }
      }
      /** IMPORTANT: `match` is invalid after the assignment. */
      next_str = using_definition + suffix;
      /* Assignments do not allow to alias functions symbols. */
      const bool replace_fn = alias.empty();
      /* Replace the alias (the left part of the assignment) or the last symbol. */
      const string from = !alias.empty() ? alias : symbol;
      /* Replace all occurrences of the non-namespace specified symbol.
       * Reject symbols that contain the target symbol name. */
      /** IMPORTANT: If replace_fn is true, this can replace any symbol type if there are functions
       * and types with the same name. We could support being more explicit about the type of
       * symbol to replace using an optional attribute [[gpu::using_function]]. */
      const regex regex(R"(([^:\w]))" + from + R"(([\s)" + (replace_fn ? R"(\()" : "") + "])");
      const string in_scope = get_content_between_balanced_pair('{' + suffix, '{', '}');
      const string out_scope = regex_replace(in_scope, regex, "$1" + to + "$2");
      replace_all(next_str, using_definition + in_scope, out_scope);
    }
    out_str += next_str;

    /* Verify all using were processed. */
    if (out_str.find("using ") != string::npos) {
      regex_global_search(out_str, regex(R"(\busing\b)"), [&](const smatch &match) {
        report_error(match, "Unsupported `using` keyword usage.");
      });
    }
    return out_str;
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

  std::string preprocessor_directive_mutation(const std::string &str)
  {
    /* Remove unsupported directives. */
    std::regex regex(R"(#\s*(?:include|pragma once|pragma runtime_generated)[^\n]*)");
    return std::regex_replace(str, regex, "");
  }

  std::string swizzle_function_mutation(const std::string &str)
  {
    /* Change C++ swizzle functions into plain swizzle. */
    std::regex regex(R"((\.[rgbaxyzw]{2,4})\(\))");
    /* Keep character count the same. Replace parenthesis by spaces. */
    return std::regex_replace(str, regex, "$1  ");
  }

  void threadgroup_variables_parsing(const std::string &str)
  {
    std::regex regex(R"(shared\s+(\w+)\s+(\w+)([^;]*);)");
    regex_global_search(str, regex, [&](const std::smatch &match) {
      shared_vars_.push_back({match[1].str(), match[2].str(), match[3].str()});
    });
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
                                filename.find("draw_debug_draw_display_vert.glsl") !=
                                    std::string::npos;
    using namespace metadata;
    /* TODO: This can trigger false positive caused by disabled #if blocks. */
    std::string tokens[] = {"gl_FragCoord",
                            "gl_FrontFacing",
                            "gl_GlobalInvocationID",
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

  template<typename ReportErrorF>
  std::string printf_processing(const std::string &str, const ReportErrorF &report_error)
  {
    std::string out_str = str;
    {
      /* Example: `printf(2, b, f(c, d));` > `printf(2@ b@ f(c@ d))$` */
      size_t start, end = 0;
      while ((start = out_str.find("printf(", end)) != std::string::npos) {
        end = out_str.find(';', start);
        if (end == std::string::npos) {
          break;
        }
        out_str[end] = '$';
        int bracket_depth = 0;
        int arg_len = 0;
        for (size_t i = start; i < end; ++i) {
          if (out_str[i] == '(') {
            bracket_depth++;
          }
          else if (out_str[i] == ')') {
            bracket_depth--;
          }
          else if (bracket_depth == 1 && out_str[i] == ',') {
            out_str[i] = '@';
            arg_len++;
          }
        }
        if (arg_len > 99) {
          report_error(std::smatch(), "Too many parameters in printf. Max is 99.");
          break;
        }
        /* Encode number of arg in the `ntf` of `printf`. */
        out_str[start + sizeof("printf") - 4] = '$';
        out_str[start + sizeof("printf") - 3] = ((arg_len / 10) > 0) ? ('0' + arg_len / 10) : '$';
        out_str[start + sizeof("printf") - 2] = '0' + arg_len % 10;
      }
      if (end == 0) {
        /* No printf in source. */
        return str;
      }
    }
    /* Example: `pri$$1(2@ b)$` > `{int c_ = print_header(1, 2); c_ = print_data(c_, b); }` */
    {
      std::regex regex(R"(pri\$\$?(\d{1,2})\()");
      out_str = std::regex_replace(out_str, regex, "{uint c_ = print_header($1u, ");
    }
    {
      std::regex regex(R"(\@)");
      out_str = std::regex_replace(out_str, regex, "); c_ = print_data(c_,");
    }
    {
      std::regex regex(R"(\$)");
      out_str = std::regex_replace(out_str, regex, "; }");
    }
    return out_str;
  }

  std::string assert_processing(const std::string &str, const std::string &filepath)
  {
    std::string filename = std::regex_replace(filepath, std::regex(R"((?:.*)\/(.*))"), "$1");
    /* Example: `assert(i < 0)` > `if (!(i < 0)) { printf(...); }` */
    std::regex regex(R"(\bassert\(([^;]*)\))");
    std::string replacement;
#ifdef WITH_GPU_SHADER_ASSERT
    replacement = "if (!($1)) { printf(\"Assertion failed: ($1), file " + filename +
                  ", line %d, thread (%u,%u,%u).\\n\", __LINE__, GPU_THREAD.x, GPU_THREAD.y, "
                  "GPU_THREAD.z); }";
#else
    (void)filename;
#endif
    return std::regex_replace(str, regex, replacement);
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

  std::string stage_function_mutation(const std::string &str)
  {
    using namespace std;

    if (str.find("_function]]") == string::npos) {
      return str;
    }

    vector<pair<string, string>> mutations;

    int64_t line = 1;
    regex regex_attr(R"(\[\[gpu::(vertex|fragment|compute)_function\]\])");
    regex_global_search(str, regex_attr, [&](const smatch &match) {
      string prefix = match.prefix().str();
      string suffix = match.suffix().str();
      string attribute = match[0].str();
      string shader_stage = match[1].str();

      line += line_count(prefix);
      string signature = suffix.substr(0, suffix.find('{'));
      string body = '{' +
                    get_content_between_balanced_pair(suffix.substr(signature.size()), '{', '}') +
                    "}\n";

      string function = signature + body;

      string check = "defined(";
      if (shader_stage == "vertex") {
        check += "GPU_VERTEX_SHADER";
      }
      else if (shader_stage == "fragment") {
        check += "GPU_FRAGMENT_SHADER";
      }
      else if (shader_stage == "compute") {
        check += "GPU_COMPUTE_SHADER";
      }
      check += ")";

      string mutated = guarded_scope_mutation(
          string(attribute.size(), ' ') + function, line, check);
      mutations.emplace_back(attribute + function, mutated);
    });

    string out = str;
    for (auto mutation : mutations) {
      replace_all(out, mutation.first, mutation.second);
    }
    return out;
  }

  std::string resource_guard_mutation(const std::string &str, report_callback report_error)
  {
    using namespace std;

    if (str.find("_get(") == string::npos) {
      return str;
    }

    vector<pair<string, string>> mutations;

    string prefix_total;
    regex regex_resource_access(R"(\b(\w+)_get\((\w+)\, \w+\))");
    regex_global_search(str, regex_resource_access, [&](const smatch &match) {
      string prefix = prefix_total + match.prefix().str();
      string suffix = match.suffix().str();
      string resource_access = match[0].str();
      string resource_type = match[1].str();
      string create_info_name = match[2].str();

      prefix_total += match.prefix().str() + resource_access;

      if (resource_type != "specialization_constant" && resource_type != "push_constant" &&
          resource_type != "interface" && resource_type != "buffer" &&
          resource_type != "attribute" && resource_type != "sampler" && resource_type != "image")
      {
        return;
      }

      string scope_start = get_content_between_balanced_pair(prefix + '}', '{', '}', true);
      string scope_end = get_content_between_balanced_pair('{' + suffix, '{', '}');
      string scope = scope_start.substr(1) + resource_access +
                     scope_end.substr(0, scope_end.rfind('\n') + 1);

      if (scope.find(" return ") != string::npos) {
        report_error(match,
                     "Return statement with values are not supported inside the same scope as "
                     "resource access function.");
        return;
      }

      size_t line_start = 1 + line_count(prefix) - line_count(scope_start) + 1;

      string check = "defined(CREATE_INFO_" + create_info_name + ")";
      string mutated = guarded_scope_mutation(scope, line_start, check);

      mutations.emplace_back(scope, mutated);
    });

    string out = str;
    for (auto mutation : mutations) {
      replace_all(out, mutation.first, mutation.second);
    }
    return out;
  }

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

  std::string enum_macro_injection(std::string str)
  {
    /**
     * Transform C,C++ enum declaration into GLSL compatible defines and constants:
     *
     * \code{.cpp}
     * enum eMyEnum : uint32_t {
     *   ENUM_1 = 0u,
     *   ENUM_2 = 1u,
     *   ENUM_3 = 2u,
     * };
     * \endcode
     *
     * becomes
     *
     * \code{.glsl}
     * _enum_decl(_eMyEnum)
     *   ENUM_1 = 0u,
     *   ENUM_2 = 1u,
     *   ENUM_3 = 2u, _enum_end
     * #define eMyEnum _enum_type(_eMyEnum)
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
    {
      /* Replaces all matches by the respective string hash. */
      std::regex regex(R"(enum\s+((\w+)\s*(?:\:\s*\w+\s*)?)\{(\n[^}]+)\n\};)");
      str = std::regex_replace(str,
                               regex,
                               "_enum_decl(_$1)$3 _enum_end\n"
                               "#define $2 _enum_type(_$2)");
    }
    {
      /* Remove trailing comma if any. */
      std::regex regex(R"(,(\s*_enum_end))");
      str = std::regex_replace(str, regex, "$1");
    }
    return str;
  }

  /**
   * Expand functions with default arguments to function overloads.
   * Expects formatted input and that function bodies are followed by newline.
   */
  std::string default_argument_mutation(std::string str)
  {
    using namespace std;
    int match = 0;
    default_argument_search(
        str, [&](int /*parenthesis_depth*/, int /*bracket_depth*/, char & /*c*/) { match++; });

    if (match == 0) {
      /* No mutation to do. Early out as the following regex is expensive. */
      return str;
    }

    vector<pair<string, string>> mutations;

    int64_t line = 0;

    /* Matches function definition. */
    regex regex_func(R"(\n((\w+)\s+(\w+)\s*\()([^{]+))");
    regex_global_search(str, regex_func, [&](const smatch &match) {
      const string prefix = match[1].str();
      const string return_type = match[2].str();
      const string func_name = match[3].str();
      const string args = get_content_between_balanced_pair('(' + match[4].str(), '(', ')');
      const string suffix = ")\n{";

      int64_t lines_in_content = line_count(match[0].str());
      line += line_count(match.prefix().str()) + lines_in_content;

      if (args.find('=') == string::npos) {
        return;
      }

      const bool has_non_void_return_type = return_type != "void";

      string line_directive = "#line " + to_string(line - lines_in_content + 2) + "\n";

      vector<string> args_split = split_string_not_between_balanced_pair(args, ',', '(', ')');
      string overloads;
      string args_defined;
      string args_called;

      /* Rewrite original definition without defaults. */
      string with_default = match[0].str();
      string no_default = with_default;

      for (const string &arg : args_split) {
        regex regex(R"(((?:const )?\w+)\s+(\w+)( = (.+))?)");
        smatch match;
        regex_search(arg, match, regex);

        string arg_type = match[1].str();
        string arg_name = match[2].str();
        string arg_assign = match[3].str();
        string arg_value = match[4].str();

        if (!arg_value.empty()) {
          string body = func_name + "(" + args_called + arg_value + ");";
          if (has_non_void_return_type) {
            body = "  return " + body;
          }
          else {
            body = "  " + body;
          }

          overloads = line_directive + prefix + args_defined + suffix + '\n' + line_directive +
                      body + "\n}\n" + overloads;

          replace_all(no_default, arg_assign, "");
        }
        if (!args_defined.empty()) {
          args_defined += ", ";
        }
        args_defined += arg_type + ' ' + arg_name;
        args_called += arg_name + ", ";
      }

      /* Get function body to put the overload after it. */
      string body_content = '{' +
                            get_content_between_balanced_pair(match.suffix().str(), '{', '}') +
                            "}\n";

      string last_line_directive =
          "#line " + to_string(line - lines_in_content + line_count(body_content) + 3) + "\n";

      mutations.emplace_back(with_default + body_content,
                             no_default + body_content + overloads + last_line_directive);
    });

    for (auto mutation : mutations) {
      replace_all(str, mutation.first, mutation.second);
    }
    return str;
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
  std::string argument_reference_mutation(std::string &str)
  {
    /* Next two REGEX checks are expensive. Check if they are needed at all. */
    bool valid_match = false;
    reference_search(str, [&](int parenthesis_depth, int bracket_depth, char &c) {
      /* Check if inside a function signature.
       * Check parenthesis_depth == 2 for array references. */
      if ((parenthesis_depth == 1 || parenthesis_depth == 2) && bracket_depth == 0) {
        valid_match = true;
        /* Modify the & into @ to make sure we only match these references in the regex
         * below. @ being forbidden in the shader language, it is safe to use a temp
         * character. */
        c = '@';
      }
    });
    if (!valid_match) {
      return str;
    }
    /* Remove parenthesis first. */
    /* Example: `float (&var)[2]` > `float &var[2]` */
    std::regex regex_parenthesis(R"((\w+ )\(@(\w+)\))");
    std::string out = std::regex_replace(str, regex_parenthesis, "$1@$2");
    /* Example: `const float &var[2]` > `inout float var[2]` */
    std::regex regex(R"((?:const)?(\s*)(\w+)\s+\@(\w+)(\[\d*\])?)");
    return std::regex_replace(out, regex, "$1 inout $2 $3$4");
  }

  /* To be run after `argument_reference_mutation()`. */
  std::string variable_reference_mutation(const std::string &str, report_callback report_error)
  {
    using namespace std;
    /* Processing regex and logic is expensive. Check if they are needed at all. */
    bool valid_match = false;
    string next_str = str;
    reference_search(next_str, [&](int parenthesis_depth, int /*bracket_depth*/, char &c) {
      /* Check if inside a function body. */
      if (parenthesis_depth == 0) {
        valid_match = true;
        /* Modify the & into @ to make sure we only match these references in the regex
         * below. @ being forbidden in the shader language, it is safe to use a temp
         * character. */
        c = '@';
      }
    });
    if (!valid_match) {
      return str;
    }
    string out_str;
    /* Example: `const float &var = value;` */
    regex regex_ref(R"(\ ?(?:const)?\s*\w+\s+\@(\w+) =\s*([^;]+);)");

    smatch match;
    while (regex_search(next_str, match, regex_ref)) {
      const string definition = match[0].str();
      const string name = match[1].str();
      const string value = match[2].str();
      const string prefix = match.prefix().str();
      const string suffix = match.suffix().str();

      out_str += prefix;

      /* Assert definition doesn't contain any side effect. */
      if (value.find("++") != string::npos || value.find("--") != string::npos) {
        report_error(match, "Reference definitions cannot have side effects.");
        return str;
      }
      if (value.find("(") != string::npos) {
        report_error(match, "Reference definitions cannot contain function calls.");
        return str;
      }
      if (value.find("[") != string::npos) {
        const string index_var = get_content_between_balanced_pair(value, '[', ']');

        if (index_var.find(' ') != string::npos) {
          report_error(match,
                       "Array subscript inside reference declaration must be a single variable or "
                       "a constant, not an expression.");
          return str;
        }

        /* Add a space to avoid empty scope breaking the loop. */
        string scope_depth = " }";
        bool found_var = false;
        while (!found_var) {
          string scope = get_content_between_balanced_pair(out_str + scope_depth, '{', '}', true);
          scope_depth += '}';

          if (scope.empty()) {
            break;
          }
          /* Remove nested scopes. Avoid variable shadowing to mess with the detection. */
          scope = regex_replace(scope, regex(R"(\{[^\}]*\})"), "{}");
          /* Search if index variable definition qualifies it as `const`. */
          regex regex_definition(R"((const)? \w+ )" + index_var + " =");
          smatch match_definition;
          if (regex_search(scope, match_definition, regex_definition)) {
            found_var = true;
            if (match_definition[1].matched == false) {
              report_error(match, "Array subscript variable must be declared as const qualified.");
              return str;
            }
          }
        }
        if (!found_var) {
          report_error(match,
                       "Cannot locate array subscript variable declaration. "
                       "If it is a global variable, assign it to a temporary const variable for "
                       "indexing inside the reference.");
          return str;
        }
      }

      /* Find scope this definition is active in. */
      const string scope = get_content_between_balanced_pair('{' + suffix, '{', '}');
      if (scope.empty()) {
        report_error(match, "Reference is defined inside a global or unterminated scope.");
        return str;
      }
      string original = definition + scope;
      string modified = original;

      /* Replace definition by nothing. Keep number of lines. */
      string newlines(line_count(definition), '\n');
      replace_all(modified, definition, newlines);
      /* Replace every occurrence of the reference. Avoid matching other symbols like class members
       * and functions with the same name. */
      modified = regex_replace(
          modified, regex(R"(([^\.])\b)" + name + R"(\b([^(]))"), "$1" + value + "$2");

      /** IMPORTANT: `match` is invalid after the assignment. */
      next_str = definition + suffix;

      /* Replace whole modified scope in output string. */
      replace_all(next_str, original, modified);
    }
    out_str += next_str;
    return out_str;
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

  /* TODO(fclem): Too many false positive and false negative to be applied to python shaders. */
  void matrix_constructor_linting(const std::string &str, report_callback report_error)
  {
    /* The following regex is expensive. Do a quick early out scan. */
    if (str.find("mat") == std::string::npos && str.find("float") == std::string::npos) {
      return;
    }
    /* Example: `mat4(other_mat)`. */
    std::regex regex(R"(\s(?:mat(?:\d|\dx\d)|float\dx\d)\()");
    regex_global_search(str, regex, [&](const std::smatch &match) {
      std::string args = get_content_between_balanced_pair("(" + match.suffix().str(), '(', ')');
      int arg_count = split_string_not_between_balanced_pair(args, ',', '(', ')').size();
      bool has_floating_point_arg = args.find('.') != std::string::npos;
      /* TODO(fclem): Check if arg count matches matrix type. */
      if (arg_count != 1 || has_floating_point_arg) {
        return;
      }
      /* This only catches some invalid usage. For the rest, the CI will catch them. */
      const char *msg =
          "Matrix constructor is not cross API compatible. "
          "Use to_floatNxM to reshape the matrix or use other constructors instead.";
      report_error(match, msg);
    });
  }

  /* Assume formatted source with our code style. Cannot be applied to python shaders. */
  void global_scope_constant_linting(const std::string &str, report_callback report_error)
  {
    /* Example: `const uint global_var = 1u;`. Matches if not indented (i.e. inside a scope). */
    std::regex regex(R"(const \w+ \w+ =)");
    regex_global_search(str, regex, [&](const std::smatch &match) {
      /* Positive look-behind is not supported in #std::regex. Do it manually. */
      if (match.prefix().str().back() == '\n') {
        const char *msg =
            "Global scope constant expression found. These get allocated per-thread in MSL. "
            "Use Macro's or uniforms instead.";
        report_error(match, msg);
      }
    });
  }

  void quote_linting(const std::string &str, report_callback report_error)
  {
    std::regex regex(R"(["'])");
    regex_global_search(str, regex, [&](const std::smatch &match) {
      /* This only catches some invalid usage. For the rest, the CI will catch them. */
      const char *msg = "Quotes are forbidden in GLSL.";
      report_error(match, msg);
    });
  }

  void array_constructor_linting(const std::string &str, report_callback report_error)
  {
    std::regex regex(R"(=\s*(\w+)\s*\[[^\]]*\]\s*\()");
    regex_global_search(str, regex, [&](const std::smatch &match) {
      /* This only catches some invalid usage. For the rest, the CI will catch them. */
      const char *msg =
          "Array constructor is not cross API compatible. Use type_array instead of type[].";
      report_error(match, msg);
    });
  }

  template<typename ReportErrorF>
  void small_type_linting(const std::string &str, const ReportErrorF &report_error)
  {
    std::regex regex(R"(\su?(char|short|half)(2|3|4)?\s)");
    regex_global_search(str, regex, [&](const std::smatch &match) {
      report_error(match, "Small types are forbidden in shader interfaces.");
    });
  }

  std::string threadgroup_variables_suffix()
  {
    if (shared_vars_.empty()) {
      return "";
    }

    std::stringstream suffix;
    /**
     * For Metal shaders to compile, shared (threadgroup) variable cannot be declared globally.
     * They must reside within a function scope. Hence, we need to extract these declarations and
     * generate shared memory blocks within the entry point function. These shared memory blocks
     * can then be passed as references to the remaining shader via the class function scope.
     *
     * The shared variable definitions from the source file are replaced with references to
     * threadgroup memory blocks (using _shared_sta and _shared_end macros), but kept in-line in
     * case external macros are used to declare the dimensions.
     *
     * Each part of the codegen is stored inside macros so that we don't have to do string
     * replacement at runtime.
     */
    suffix << "\n";
    /* Arguments of the wrapper class constructor. */
    suffix << "#undef MSL_SHARED_VARS_ARGS\n";
    /* References assignment inside wrapper class constructor. */
    suffix << "#undef MSL_SHARED_VARS_ASSIGN\n";
    /* Declaration of threadgroup variables in entry point function. */
    suffix << "#undef MSL_SHARED_VARS_DECLARE\n";
    /* Arguments for wrapper class constructor call. */
    suffix << "#undef MSL_SHARED_VARS_PASS\n";

    /**
     * Example replacement:
     *
     * \code{.cc}
     * // Source
     * shared float bar[10];                                    // Source declaration.
     * shared float foo;                                        // Source declaration.
     * // Rest of the source ...
     * // End of Source
     *
     * // Backend Output
     * class Wrapper {                                          // Added at runtime by backend.
     *
     * threadgroup float (&foo);                                // Replaced by regex and macros.
     * threadgroup float (&bar)[10];                            // Replaced by regex and macros.
     * // Rest of the source ...
     *
     * Wrapper (                                                // Added at runtime by backend.
     * threadgroup float (&_foo), threadgroup float (&_bar)[10] // MSL_SHARED_VARS_ARGS
     * )                                                        // Added at runtime by backend.
     * : foo(_foo), bar(_bar)                                   // MSL_SHARED_VARS_ASSIGN
     * {}                                                       // Added at runtime by backend.
     *
     * }; // End of Wrapper                                     // Added at runtime by backend.
     *
     * kernel entry_point() {                                   // Added at runtime by backend.
     *
     * threadgroup float foo;                                   // MSL_SHARED_VARS_DECLARE
     * threadgroup float bar[10]                                // MSL_SHARED_VARS_DECLARE
     *
     * Wrapper wrapper                                          // Added at runtime by backend.
     * (foo, bar)                                               // MSL_SHARED_VARS_PASS
     * ;                                                        // Added at runtime by backend.
     *
     * }                                                        // Added at runtime by backend.
     * // End of Backend Output
     * \endcode
     */
    std::stringstream args, assign, declare, pass;

    bool first = true;
    for (SharedVar &var : shared_vars_) {
      char sep = first ? ' ' : ',';
      /*  */
      args << sep << "threadgroup " << var.type << "(&_" << var.name << ")" << var.array;
      assign << (first ? ':' : ',') << var.name << "(_" << var.name << ")";
      declare << "threadgroup " << var.type << ' ' << var.name << var.array << ";";
      pass << sep << var.name;
      first = false;
    }

    suffix << "#define MSL_SHARED_VARS_ARGS " << args.str() << "\n";
    suffix << "#define MSL_SHARED_VARS_ASSIGN " << assign.str() << "\n";
    suffix << "#define MSL_SHARED_VARS_DECLARE " << declare.str() << "\n";
    suffix << "#define MSL_SHARED_VARS_PASS (" << pass.str() << ")\n";
    suffix << "\n";

    return suffix.str();
  }

  std::string line_directive_prefix(const std::string &filepath)
  {
    std::string filename = std::regex_replace(filepath, std::regex(R"((?:.*)\/(.*))"), "$1");

    std::stringstream suffix;
    suffix << "#line 1 ";
#ifdef __APPLE__
    /* For now, only Metal supports filename in line directive.
     * There is no way to know the actual backend, so we assume Apple uses Metal. */
    /* TODO(fclem): We could make it work using a macro to choose between the filename and the hash
     * at runtime. i.e.: `FILENAME_MACRO(12546546541, 'filename.glsl')` This should work for both
     * MSL and GLSL. */
    if (!filename.empty()) {
      suffix << "\"" << filename << "\"";
    }
#else
    uint64_t hash_value = metadata::hash(filename);
    /* Fold the value so it fits the GLSL spec. */
    hash_value = (hash_value ^ (hash_value >> 32)) & (~uint64_t(0) >> 33);
    suffix << std::to_string(uint64_t(hash_value));
#endif
    suffix << "\n";
    return suffix.str();
  }

  /* Made public for unit testing purpose. */
 public:
  static std::string get_content_between_balanced_pair(const std::string &input,
                                                       char start_delimiter,
                                                       char end_delimiter,
                                                       const bool backwards = false)
  {
    int balance = 0;
    size_t start = std::string::npos;
    size_t end = std::string::npos;

    if (backwards) {
      std::swap(start_delimiter, end_delimiter);
    }

    for (size_t i = 0; i < input.length(); ++i) {
      size_t idx = backwards ? (input.length() - 1) - i : i;
      if (input[idx] == start_delimiter) {
        if (balance == 0) {
          start = idx;
        }
        balance++;
      }
      else if (input[idx] == end_delimiter) {
        balance--;
        if (balance == 0 && start != std::string::npos) {
          end = idx;
          if (backwards) {
            std::swap(start, end);
          }
          return input.substr(start + 1, end - start - 1);
        }
      }
    }
    return "";
  }

  /* Replaces all occurrences of `from` by `to` between `start_delimiter`
   * and `end_delimiter` even inside nested delimiters pair. */
  static std::string replace_char_between_balanced_pair(const std::string &input,
                                                        const char start_delimiter,
                                                        const char end_delimiter,
                                                        const char from,
                                                        const char to)
  {
    int depth = 0;

    std::string str = input;
    for (char &string_char : str) {
      if (string_char == start_delimiter) {
        depth++;
      }
      else if (string_char == end_delimiter) {
        depth--;
      }
      else if (depth > 0 && string_char == from) {
        string_char = to;
      }
    }
    return str;
  }

  /* Function to split a string by a delimiter and return a vector of substrings. */
  static std::vector<std::string> split_string(const std::string &str, const char delimiter)
  {
    std::vector<std::string> substrings;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
      substrings.push_back(item);
    }
    return substrings;
  }

  /* Similar to split_string but only split if the delimiter is not between any pair_start and
   * pair_end. */
  static std::vector<std::string> split_string_not_between_balanced_pair(const std::string &str,
                                                                         const char delimiter,
                                                                         const char pair_start,
                                                                         const char pair_end)
  {
    const char safe_char = '@';
    const std::string safe_str = replace_char_between_balanced_pair(
        str, pair_start, pair_end, delimiter, safe_char);
    std::vector<std::string> split = split_string(safe_str, delimiter);
    for (std::string &str : split) {
      replace_all(str, safe_char, delimiter);
    }
    return split;
  }

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

  /* Match any reference definition (e.g. `int &a = b`).
   * Call the callback function for each `&` character that matches a reference definition.
   * Expects the input `str` to be formatted with balanced parenthesis and curly brackets. */
  static void reference_search(std::string &str, std::function<void(int, int, char &)> callback)
  {
    scopes_scan_for_char(
        str, '&', [&](size_t pos, int parenthesis_depth, int bracket_depth, char &c) {
          if (pos > 0 && pos <= str.length() - 2) {
            /* This is made safe by the previous check. */
            char prev_char = str[pos - 1];
            char next_char = str[pos + 1];
            /* Validate it is not an operator (`&`, `&&`, `&=`). */
            if (prev_char == ' ' || prev_char == '(') {
              if (next_char != ' ' && next_char != '\n' && next_char != '&' && next_char != '=') {
                callback(parenthesis_depth, bracket_depth, c);
              }
            }
          }
        });
  }

  /* Match any default argument definition (e.g. `void func(int a = 0)`).
   * Call the callback function for each `=` character inside a function argument list.
   * Expects the input `str` to be formatted with balanced parenthesis and curly brackets. */
  static void default_argument_search(std::string &str,
                                      std::function<void(int, int, char &)> callback)
  {
    scopes_scan_for_char(
        str, '=', [&](size_t pos, int parenthesis_depth, int bracket_depth, char &c) {
          if (pos > 0 && pos <= str.length() - 2) {
            /* This is made safe by the previous check. */
            char prev_char = str[pos - 1];
            char next_char = str[pos + 1];
            /* Validate it is not an operator (`==`, `<=`, `>=`). Expects formatted input. */
            if (prev_char == ' ' && next_char == ' ') {
              if (parenthesis_depth == 1 && bracket_depth == 0) {
                callback(parenthesis_depth, bracket_depth, c);
              }
            }
          }
        });
  }

  /* Scan through a string matching for every occurrence of a character.
   * Calls the callback with the context in which the match occurs. */
  static void scopes_scan_for_char(std::string &str,
                                   char search_char,
                                   std::function<void(size_t, int, int, char &)> callback)
  {
    size_t pos = 0;
    int parenthesis_depth = 0;
    int bracket_depth = 0;
    for (char &c : str) {
      if (c == search_char) {
        callback(pos, parenthesis_depth, bracket_depth, c);
      }
      else if (c == '(') {
        parenthesis_depth++;
      }
      else if (c == ')') {
        parenthesis_depth--;
      }
      else if (c == '{') {
        bracket_depth++;
      }
      else if (c == '}') {
        bracket_depth--;
      }
      pos++;
    }
  }
};

}  // namespace blender::gpu::shader
