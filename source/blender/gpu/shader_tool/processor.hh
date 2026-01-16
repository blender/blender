/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include "intermediate.hh"
#include "metadata.hh"

namespace blender::gpu::shader {

enum class Language {
  UNKNOWN = 0,
  /* Shared header. */
  CPP,
  /* Metal Shading Language. */
  MSL,
  /* OpenGL Shading Language. */
  GLSL,
  /* Blender Shading Language. */
  BSL,
  /* Same as GLSL but enable partial C++ feature support like template, references,
   * include system, etc ... */
  BLENDER_GLSL,
};

static inline Language language_from_filename(const std::string &filename)
{
  if (filename.find(".msl") != std::string::npos) {
    return Language::MSL;
  }
  if (filename.find(".glsl") != std::string::npos || filename.find(".bsl.hh") != std::string::npos)
  {
    return Language::GLSL;
  }
  if (filename.find(".hh") != std::string::npos) {
    return Language::CPP;
  }
  return Language::UNKNOWN;
}

/**
 * Shader source preprocessor that allow to mutate shader sources into cross API source that can be
 * interpreted by the different GPU backends. Some syntax are mutated or reported as incompatible.
 */
class SourceProcessor {
 public:
  using report_callback = parser::report_callback;
  using Parser = parser::IntermediateForm<parser::FullLexer, parser::FullParser>;
  using Scope = parser::Scope;
  using Token = parser::Token;
  using Tokens = std::vector<parser::Token>;

  /* Cannot use `__` because of some compilers complaining about reserved symbols. */
  static constexpr const char *namespace_separator = "_";
  /* Add a prefix to all member functions so that they are not clashing with local variables. */
  static constexpr const char *method_call_prefix = "_";
  static constexpr const char *linted_struct_suffix = "_host_shared_";
  static constexpr const char *uniform_struct_suffix = "uniform_";

#define ERROR_TOK(token) (token).line_number(), (token).char_number(), (token).line_str()

 private:
  const std::string source_;
  const std::string filepath_;
  metadata::Source metadata_;

  Language language_;

  report_callback report_error_;

 public:
  SourceProcessor(
      const std::string &source,
      const std::string &filepath,
      Language language,
      report_callback report_error = [](int, int, std::string, const char *) {})
      : source_(source), filepath_(filepath), language_(language), report_error_(report_error)
  {
  }

  struct Result {
    /* Resulting Intermediate Language source. */
    std::string source;
    /* Parsed metadata. */
    metadata::Source metadata;
  };

  /* Convert to intermediate language. Also outputs metadata.
   * symbols_set is the set of namespace symbols from external files / dependencies. */
  Result convert(std::vector<metadata::Symbol> symbols_set = {});

  /* Lightweight parsing. Only Source::dependencies and Source::symbol_table are populated. */
  metadata::Source parse_include_and_symbols();

  /* Return the input string with comments removed. */
  std::string remove_comments()
  {
    return remove_comments(source_);
  }

  /* String hash are outputted inside GLSL and needs to fit 32 bits. */
  static uint32_t hash_string(const std::string &str)
  {
    uint64_t hash_64 = metadata::hash(str);
    uint32_t hash_32 = uint32_t(hash_64 ^ (hash_64 >> 32));
    return hash_32;
  }

 private:
  /* --- Cleanup --- */

  /** Remove single and multi-line comments to avoid this complexity during parsing. */
  std::string remove_comments(const std::string &str);
  /* Lower preprocessor directives containing `GPU_SHADER`.
   * Avoid processing code that is not destined to be shader code and could contain unsupported
   * syntax. */
  std::string disabled_code_mutation(const std::string &str);
  /* Remove trailing white spaces. */
  template<typename ParserT> void cleanup_whitespace(ParserT &parser);
  /* Successive mutations can introduce a lot of unneeded line directives. */
  void cleanup_line_directives(Parser &parser);
  /* Successive mutations can introduce a lot of unneeded blank lines. */
  void cleanup_empty_lines(Parser &parser);

  /* --- Parsing --- */

  /* Parse defines in order to output them with the create infos.
   * This allow the create infos to use shared defines values. */
  void parse_defines(Parser &parser);
  /* Populates metadata::symbol_table by scanning all namespaces.
   * Does not parse global symbols. */
  void parse_local_symbols(Parser &parser);
  /* Legacy create info parsing and removing. */
  void parse_legacy_create_info(Parser &parser);
  /* Populates metadata::dependencies by scanning include directives. */
  void parse_includes(Parser &parser);
  /* Parse special pragma. */
  void parse_pragma_runtime_generated(Parser &parser);
  /** Populate metadata::functions for runtime node-tree compilation. */
  void parse_library_functions(Parser &parser);
  /* Populate metadata::builtins by scanning source for keywords. Can trigger false positive.
   * This is mostly legacy path as most builtin should be explicitly defined inside the BSL entry
   * points. */
  void parse_builtins(const std::string &str, const std::string &filename, bool pure_glsl = false);

  /* Legacy shared variable support. */
  std::string threadgroup_variables_parse_and_remove(const std::string &str);

  /* --- Linting --- */

  /* Make sure `if`, `else`, `for` statements are followed by braces. */
  void lint_unbraced_statements(Parser &parser);
  /* Lint for BSL reserved tokens. */
  void lint_reserved_tokens(Parser &parser);
  /* Lint for valid BSL attributes. */
  void lint_attributes(Parser &parser);
  /* Assume formatted source with our code style. Cannot be applied to python shaders. */
  void lint_global_scope_constants(Parser &parser);
  /* Search for constructor definition in active code. These are not supported. */
  void lint_constructors(Parser &parser);
  /* Forward declaration of types are not supported and makes no sense in a shader program where
   * there is no pointers. */
  void lint_forward_declared_structs(Parser &parser);

  /* --- Lowering --- */

  /**
   * Given our code-style, we don't need the disambiguation.
   * Example: `x.template foo<int>()` > `x.foo<int>()`
   */
  void lower_template_dependent_names(Parser &parser);
  /* Lower template definition and instantiation by doing simple copy paste + argument
   * substitution. */
  void lower_templates(Parser &parser);
  /* Ensures pragma once is present in headers to comply to our include semantic. */
  void lint_pragma_once(Parser &parser, const std::string &filename);
  /* Unroll loops by copy pasting content. */
  void lower_loop_unroll(Parser &parser);
  /* Convert if statements marked as static to preprocessor #if statements. */
  void lower_static_branch(Parser &parser);
  /* Lower namespaces by adding namespace prefix to all the contained structs and functions. */
  void lower_namespaces(Parser &parser);
  /**
   * Needs to run before namespace mutation so that `using` have more precedence.
   * Otherwise the following would fail.
   * \code{.cc}
   * namespace B {
   * int test(int a) {}
   * }
   *
   * namespace A {
   * int test(int a) {}
   * int func(int a) {
   *   using B::test;
   *   return test(a); // Should reference B::test and not A::test
   * }
   * \endcode
   */
  void lower_using(Parser &parser);
  /* Example: `A::B` --> `A_B` */
  void lower_scope_resolution_operators(Parser &parser);
  /* Remove preprocessor directives unsupported by target shading languages.
   * Examples `#includes`, `#pragma once`. */
  void lower_preprocessor(Parser &parser);
  /* Support for BLI swizzle syntax.
   * Examples `a.xy()` --> `a.xy`. */
  void lower_swizzle_methods(Parser &parser);
  /* Change printf calls to "recursive" call to implementation functions.
   * This allows to emulate the variadic arguments of printf. */
  void lower_printf(Parser &parser);
  /* Turn assert into a printf. */
  void lower_assert(Parser &parser, const std::string &filename);
  /* Parse SRT and interfaces, remove their attributes and create init function for SRT structs. */
  void lower_resource_table(Parser &parser);
  /* Examples `string_t s = "a" "b"` --> `string_t s = "ab"`. */
  void lower_strings_sequences(Parser &parser);
  /* Replace string literals by their hash and store the original string in the file metadata. */
  void lower_strings(Parser &parser);
  /* `class` -> `struct` */
  void lower_classes(Parser &parser);
  /* Create default initializer (empty brace) for all classes. */
  void lower_default_constructors(Parser &parser);
  /* Make all members of a class to be referenced using `this->`. */
  void lower_implicit_member(Parser &parser);
  /* Move all method definition outside of struct definition blocks. */
  void lower_method_definitions(Parser &parser);
  /* Add padding member to empty structs. */
  void lower_empty_struct(Parser &parser);
  /* Transform `a.fn(b)` into `fn(a, b)`. */
  void lower_method_calls(Parser &parser);
  /* Parse, convert to create infos, and erase declaration. */
  void lower_pipeline_definition(Parser &parser, const std::string &filename);
  /* Remove `[vertex|fragment|compute]` function attribute and add appropriate guards. */
  void lower_stage_function(Parser &parser);
  /* Add #ifdef directive around functions using SRT arguments.
   * Need to run after `lower_entry_points_signature`. */
  void lower_srt_arguments(Parser &parser);
  /* Add ifdefs guards around scopes using resource accessors. */
  void lower_resource_access_functions(Parser &parser);
  /* Lower enums to constants. */
  void lower_enums(Parser &parser);
  /* Merge attribute scopes. They are equivalent in the C++ standard.
   * This allow to simplify parsing later on.
   * `[[a]] [[b]]` > `[[a, b]]` */
  void lower_attribute_sequences(Parser &parser);
  /* Lint host shared structure for padding and alignment.
   * Remove the [[host_shared]] attribute. */
  void lower_host_shared_structures(Parser &parser);
  /* Remove noop keywords that makes subsequent lowering passes more complicated. */
  void lower_noop_keywords(Parser &parser);
  /* Example: `int a[] = {1,2,};` --> `int a[] = {1,2 };` */
  void lower_trailing_comma_in_list(Parser &parser);
  /* Allow easier parsing of struct member declaration.
   * Example: `int a, b;` --> `int a; int b;` */
  void lower_comma_separated_declarations(Parser &parser);
  /* Example: `return {1, 2};` --> `T tmp = T{1, 2}; return tmp;`. */
  void lower_implicit_return_types(Parser &parser);
  /* Example: `int a{1};` --> `int a = int{1};`. */
  void lower_initializer_implicit_types(Parser &parser);
  /* Example: `T a{.a=1};` --> `T a; a.a=1;`. */
  void lower_designated_initializers(Parser &parser);
  /* Support for **full** aggregate initialization.
   * They are converted to default constructor for GLSL. */
  void lower_aggregate_initializers(Parser &parser);
  /* Auto detect array length, and lower to GLSL compatible syntax.
   * TODO(fclem): GLSL 4.3 already supports initializer list. So port the old GLSL syntax to
   * initializer list instead. */
  void lower_array_initializations(Parser &parser);
  /**
   * Expand functions with default arguments to function overloads.
   * Expects formatted input and that function bodies are followed by newline.
   */
  void lower_function_default_arguments(Parser &parser);
  /* Limited union implementation. Create getters and setters to a raw data struct. */
  void lower_unions(Parser &parser);
  /**
   * For safety reason, union members need to be declared with the union_t template.
   * This avoid raw member access which we cannot emulate. Instead this forces the use of the `()`
   * operator for accessing the members of the enum.
   *
   * Need to run before lower_unions.
   */
  void lower_union_accessor_templates(Parser &parser);
  /**
   * For safety reason, nested resource tables need to be declared with the srt_t template.
   * This avoid chained member access which isn't well defined with the preprocessing we are doing.
   *
   * This linting phase make sure that [[resource_table]] members uses it and that no incorrect
   * usage is made. We also remove this template because it has no real meaning.
   *
   * Need to run before lower_resource_table.
   */
  void lower_srt_accessor_templates(Parser &parser);
  /* Add `srt_access` around all member access of SRT variables.
   * Need to run before local reference mutations. */
  void lower_srt_member_access(Parser &parser);
  /* Parse entry point definitions and mutating all parameter usage to global resources. */
  void lower_entry_points(Parser &parser);
  /* Removes entry point arguments to make it compatible with the legacy code.
   * Has to run after mutation related to function arguments. */
  void lower_entry_points_signature(Parser &parser);
  /* To be run after `lower_reference_arguments()`. */
  void lower_reference_variables(Parser &parser);
  /* To be run before `argument_decorator_macro_injection()`. */
  void lower_reference_arguments(Parser &parser);
  /* Example: `out float var[2]` > `_ref(float, var)[2]` */
  void lower_argument_qualifiers(Parser &parser);

  /* --- Legacy passes for GLSL --- */

  /* Example: `out float var[2]` > `out float _out_sta var _out_end[2]` */
  std::string argument_decorator_macro_injection(const std::string &str);
  /* Example: `= float[2](0.0, 0.0)` > `= ARRAY_T(float) ARRAY_V(0.0, 0.0)` */
  std::string array_constructor_macro_injection(const std::string &str);
  /* Used to make GLSL matrix constructor compatible with MSL in pyGPU shaders.
   * This syntax is not supported in blender's own shaders. */
  std::string matrix_constructor_mutation(const std::string &str);

  /* --- Utilities --- */

  /* Parse subscript scope with single integer literal and return the literal value.
   * Return the fallback value in any case of non-literal value, or failed conversion. */
  int static_array_size(const Scope &array, int fallback_value);

 public:
  /** Remove trailing white-spaces. */
  static std::string strip_whitespace(const std::string &str);

  /* Example: `VertOut<float, 1>` > `VertOutTfloatT1` */
  static std::string template_arguments_mangle(const Scope template_args);

  /* Create placeholder for GLSL declarations generated by the GPU backends (VK/GL). */
  static std::string get_create_info_placeholder(const std::string &name);

  /* Make a scope only active based on the given condition using `#if` preprocessor directives.
   * Processor contained return statements by returning 0 if scope is disabled. */
  static void guarded_scope_mutation(Parser &parser,
                                     Scope scope,
                                     const std::string &condition,
                                     Token fn_type = Token::invalid());

  /* Return `#line 1 filename\n`. */
  static std::string line_directive_prefix(const std::string &filename);
};

}  // namespace blender::gpu::shader
