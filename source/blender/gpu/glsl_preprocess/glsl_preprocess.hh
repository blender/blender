/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup glsl_preprocess
 */

#pragma once

#include <algorithm>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace blender::gpu::shader {

/**
 * Shader source preprocessor that allow to mutate GLSL into cross API source that can be
 * interpreted by the different GPU backends. Some syntax are mutated or reported as incompatible.
 *
 * Implementation speed is not a huge concern as we only apply this at compile time or on python
 * shaders source.
 */
template<typename T, bool no_linting = false> class Preprocessor {
  T &report_error;

  struct SharedVar {
    std::string type;
    std::string name;
    std::string array;
  };
  std::vector<SharedVar> shared_vars_;

  std::stringstream output_;

 public:
  Preprocessor(T &error_cb) : report_error(error_cb) {}

  Preprocessor &operator<<(std::string str)
  {
    threadgroup_variable_parsing(str);
    matrix_constructor_linting(str);
    array_constructor_linting(str);
    str = preprocessor_directive_mutation(str);
    str = argument_decorator_macro_injection(str);
    str = array_constructor_macro_injection(str);
    output_ << str;
    return *this;
  }

  Preprocessor &operator<<(char c)
  {
    output_ << c;
    return *this;
  }

  std::string str()
  {
    return output_.str() + suffix();
  }

 private:
  std::string preprocessor_directive_mutation(const std::string &str)
  {
    /* Example: `#include "deps.glsl"` > `//include "deps.glsl"` */
    std::regex regex("#\\s*(include|pragma once)");
    return std::regex_replace(str, regex, "//$1");
  }

  void threadgroup_variable_parsing(std::string str)
  {
    std::regex regex("shared\\s+(\\w+)\\s+(\\w+)([^;]*);");
    for (std::smatch match; std::regex_search(str, match, regex); str = match.suffix()) {
      shared_vars_.push_back({match[1].str(), match[2].str(), match[3].str()});
    }
  }

  std::string argument_decorator_macro_injection(const std::string &str)
  {
    /* Example: `out float var[2]` > `out float _out_sta var _out_end[2]` */
    std::regex regex("(out|inout|in|shared)\\s+(\\w+)\\s+(\\w+)");
    return std::regex_replace(str, regex, "$1 $2 _$1_sta $3 _$1_end");
  }

  std::string array_constructor_macro_injection(const std::string &str)
  {
    /* Example: `= float[2](0.0, 0.0)` > `= ARRAY_T(float) ARRAY_V(0.0, 0.0)` */
    std::regex regex("=\\s*(\\w+)\\s*\\[[^\\]]*\\]\\s*\\(");
    return std::regex_replace(str, regex, "= ARRAY_T($1) ARRAY_V(");
  }

  /* TODO(fclem): Too many false positive and false negative to be applied to python shaders. */
  void matrix_constructor_linting(std::string str)
  {
    if constexpr (no_linting) {
      return;
    }
    /* Example: `mat4(other_mat)`. */
    std::regex regex("\\s+(mat(\\d|\\dx\\d)|float\\dx\\d)\\([^,\\s\\d]+\\)");
    for (std::smatch match; std::regex_search(str, match, regex); str = match.suffix()) {
      /* This only catches some invalid usage. For the rest, the CI will catch them. */
      const char *msg =
          "Matrix constructor is not cross API compatible. "
          "Use to_floatNxM to reshape the matrix or use other constructors instead.";
      report_error(str, match, msg);
    }
  }

  void array_constructor_linting(std::string str)
  {
    if constexpr (no_linting) {
      return;
    }
    std::regex regex("=\\s*(\\w+)\\s*\\[[^\\]]*\\]\\s*\\(");
    for (std::smatch match; std::regex_search(str, match, regex); str = match.suffix()) {
      /* This only catches some invalid usage. For the rest, the CI will catch them. */
      const char *msg =
          "Array constructor is not cross API compatible. Use type_array instead of type[].";
      report_error(str, match, msg);
    }
  }

  std::string suffix()
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
     * `
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
     * `
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

    return suffix.str();
  }
};

template<typename T> class PreprocessorPython : public Preprocessor<T, true> {
 public:
  PreprocessorPython(T &error_cb) : Preprocessor<T, true>(error_cb){};
};

}  // namespace blender::gpu::shader
