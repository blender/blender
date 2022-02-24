/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * Shader source dependency builder that make possible to support #include directive inside the
 * shader files.
 */

#include <iostream>

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"

#include "gpu_shader_create_info.hh"
#include "gpu_shader_dependency_private.h"

extern "C" {
#define SHADER_SOURCE(datatoc, filename, filepath) extern char datatoc[];
#include "glsl_draw_source_list.h"
#include "glsl_gpu_source_list.h"
#ifdef WITH_OCIO
#  include "glsl_ocio_source_list.h"
#endif
#undef SHADER_SOURCE
}

namespace blender::gpu {

using GPUSourceDictionnary = Map<StringRef, struct GPUSource *>;

struct GPUSource {
  StringRefNull fullpath;
  StringRefNull filename;
  StringRefNull source;
  Vector<GPUSource *> dependencies;
  bool dependencies_init = false;
  shader::BuiltinBits builtins = (shader::BuiltinBits)0;
  std::string processed_source;

  GPUSource(const char *path, const char *file, const char *datatoc)
      : fullpath(path), filename(file), source(datatoc)
  {
    /* Scan for builtins. */
    /* FIXME: This can trigger false positive caused by disabled #if blocks. */
    /* TODO(fclem): Could be made faster by scanning once. */
    if (source.find("gl_FragCoord", 0)) {
      builtins |= shader::BuiltinBits::FRAG_COORD;
    }
    if (source.find("gl_FrontFacing", 0)) {
      builtins |= shader::BuiltinBits::FRONT_FACING;
    }
    if (source.find("gl_GlobalInvocationID", 0)) {
      builtins |= shader::BuiltinBits::GLOBAL_INVOCATION_ID;
    }
    if (source.find("gl_InstanceID", 0)) {
      builtins |= shader::BuiltinBits::INSTANCE_ID;
    }
    if (source.find("gl_LocalInvocationID", 0)) {
      builtins |= shader::BuiltinBits::LOCAL_INVOCATION_ID;
    }
    if (source.find("gl_LocalInvocationIndex", 0)) {
      builtins |= shader::BuiltinBits::LOCAL_INVOCATION_INDEX;
    }
    if (source.find("gl_NumWorkGroup", 0)) {
      builtins |= shader::BuiltinBits::NUM_WORK_GROUP;
    }
    if (source.find("gl_PointCoord", 0)) {
      builtins |= shader::BuiltinBits::POINT_COORD;
    }
    if (source.find("gl_PointSize", 0)) {
      builtins |= shader::BuiltinBits::POINT_SIZE;
    }
    if (source.find("gl_PrimitiveID", 0)) {
      builtins |= shader::BuiltinBits::PRIMITIVE_ID;
    }
    if (source.find("gl_VertexID", 0)) {
      builtins |= shader::BuiltinBits::VERTEX_ID;
    }
    if (source.find("gl_WorkGroupID", 0)) {
      builtins |= shader::BuiltinBits::WORK_GROUP_ID;
    }
    if (source.find("gl_WorkGroupSize", 0)) {
      builtins |= shader::BuiltinBits::WORK_GROUP_SIZE;
    }

    /* TODO(fclem): We could do that at compile time. */
    /* Limit to shared header files to avoid the temptation to use C++ syntax in .glsl files. */
    if (filename.endswith(".h") || filename.endswith(".hh")) {
      enum_preprocess();
    }
  };

  bool is_in_comment(const StringRef &input, int64_t offset)
  {
    return (input.rfind("/*", offset) > input.rfind("*/", offset)) ||
           (input.rfind("//", offset) > input.rfind("\n", offset));
  }

  template<bool check_whole_word = true, bool reversed = false, typename T>
  int64_t find_str(const StringRef &input, const T keyword, int64_t offset = 0)
  {
    while (true) {
      if constexpr (reversed) {
        offset = input.rfind(keyword, offset);
      }
      else {
        offset = input.find(keyword, offset);
      }
      if (offset > 0) {
        if constexpr (check_whole_word) {
          /* Fix false positive if something has "enum" as suffix. */
          char previous_char = input[offset - 1];
          if (!(ELEM(previous_char, '\n', '\t', ' ', ':'))) {
            offset += (reversed) ? -1 : 1;
            continue;
          }
        }
        /* Fix case where the keyword is in a comment. */
        if (is_in_comment(input, offset)) {
          offset += (reversed) ? -1 : 1;
          continue;
        }
      }
      return offset;
    }
  }

  void print_error(const StringRef &input, int64_t offset, const StringRef message)
  {
    std::cout << " error: " << message << "\n";
    StringRef sub = input.substr(0, offset);
    int64_t line_number = std::count(sub.begin(), sub.end(), '\n') + 1;
    int64_t line_end = input.find("\n", offset);
    int64_t line_start = input.rfind("\n", offset) + 1;
    int64_t char_number = offset - line_start + 1;
    char line_prefix[16] = "";
    SNPRINTF(line_prefix, "%5ld | ", line_number);

    /* TODO Use clog. */

    std::cout << fullpath << ":" << line_number << ":" << char_number;

    std::cout << " error: " << message << "\n";
    std::cout << line_prefix << input.substr(line_start, line_end - line_start) << "\n";
    std::cout << "      | ";
    for (int64_t i = 0; i < char_number - 1; i++) {
      std::cout << " ";
    }
    std::cout << "^\n";
  }

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
   * or
   *
   * \code{.c}
   * enum eMyEnum {
   *   ENUM_1 = 0u,
   *   ENUM_2 = 1u,
   *   ENUM_3 = 2u,
   * };
   * \endcode
   *
   * becomes
   *
   * \code{.glsl}
   * #define eMyEnum uint
   * const uint ENUM_1 = 0u, ENUM_2 = 1u, ENUM_3 = 2u;
   * \endcode
   *
   * IMPORTANT: This has some requirements:
   * - Enums needs to have underlying types specified to uint32_t to make them usable in UBO/SSBO.
   * - All values needs to be specified using constant literals to avoid compiler differences.
   * - All values needs to have the 'u' suffix to avoid GLSL compiler errors.
   */
  void enum_preprocess()
  {
    const StringRefNull input = source;
    std::string output;
    int64_t cursor = 0;
    int64_t last_pos = 0;
    const bool is_cpp = filename.endswith(".hh");

#define find_keyword find_str<true, false>
#define find_token find_str<false, false>
#define rfind_token find_str<false, true>
#define CHECK(test_value, str, ofs, msg) \
  if ((test_value) == -1) { \
    print_error(str, ofs, msg); \
    cursor++; \
    continue; \
  }

    while (true) {
      cursor = find_keyword(input, "enum ", cursor);
      if (cursor == -1) {
        break;
      }
      /* Output anything between 2 enums blocks. */
      output += input.substr(last_pos, cursor - last_pos);

      /* Extract enum type name. */
      int64_t name_start = input.find(" ", cursor);

      int64_t values_start = find_token(input, '{', cursor);
      CHECK(values_start, input, cursor, "Malformed enum class. Expected \'{\' after typename.");

      StringRef enum_name = input.substr(name_start, values_start - name_start);
      if (is_cpp) {
        int64_t name_end = find_token(enum_name, ":");
        CHECK(name_end, input, name_start, "Expected \':\' after C++ enum name.");

        int64_t underlying_type = find_keyword(enum_name, "uint32_t", name_end);
        CHECK(underlying_type, input, name_start, "C++ enums needs uint32_t underlying type.");

        enum_name = input.substr(name_start, name_end);
      }

      output += "#define " + enum_name + " uint\n";

      /* Extract enum values. */
      int64_t values_end = find_token(input, '}', values_start);
      CHECK(values_end, input, cursor, "Malformed enum class. Expected \'}\' after values.");

      /* Skip opening brackets. */
      values_start += 1;

      StringRef enum_values = input.substr(values_start, values_end - values_start);

      /* Really poor check. Could be done better. */
      int64_t token = find_token(enum_values, '{');
      int64_t not_found = (token == -1) ? 0 : -1;
      CHECK(not_found, input, values_start + token, "Unexpected \'{\' token inside enum values.");

      /* Do not capture the comma after the last value (if present). */
      int64_t last_equal = rfind_token(enum_values, '=', values_end);
      int64_t last_comma = rfind_token(enum_values, ',', values_end);
      if (last_comma > last_equal) {
        enum_values = input.substr(values_start, last_comma);
      }

      output += "const uint " + enum_values;

      int64_t semicolon_found = (input[values_end + 1] == ';') ? 0 : -1;
      CHECK(semicolon_found, input, values_end + 1, "Expected \';\' after enum type declaration.");

      /* Skip the curly bracket but not the semicolon. */
      cursor = last_pos = values_end + 1;
    }
    /* If nothing has been changed, do not allocate processed_source. */
    if (last_pos == 0) {
      return;
    }

#undef find_keyword
#undef find_token
#undef rfind_token

    if (last_pos != 0) {
      output += input.substr(last_pos);
    }
    processed_source = output;
    source = processed_source.c_str();
  };

  /* Return 1 one error. */
  int init_dependencies(const GPUSourceDictionnary &dict)
  {
    if (dependencies_init) {
      return 0;
    }
    dependencies_init = true;
    int64_t pos = 0;
    while (true) {
      pos = source.find("pragma BLENDER_REQUIRE(", pos);
      if (pos == -1) {
        return 0;
      }
      int64_t start = source.find('(', pos) + 1;
      int64_t end = source.find(')', pos);
      if (end == -1) {
        /* TODO Use clog. */
        std::cout << "Error: " << filename << " : Malformed BLENDER_REQUIRE: Missing \")\"."
                  << std::endl;
        return 1;
      }
      StringRef dependency_name = source.substr(start, end - start);
      GPUSource *dependency_source = dict.lookup_default(dependency_name, nullptr);
      if (dependency_source == nullptr) {
        /* TODO Use clog. */
        std::cout << "Error: " << filename << " : Dependency not found \"" << dependency_name
                  << "\"." << std::endl;
        return 1;
      }
      /* Recursive. */
      int result = dependency_source->init_dependencies(dict);
      if (result != 0) {
        return 1;
      }

      for (auto *dep : dependency_source->dependencies) {
        dependencies.append_non_duplicates(dep);
      }
      dependencies.append_non_duplicates(dependency_source);
      pos++;
    };
  }

  /* Returns the final string with all includes done. */
  void build(Vector<const char *> &result) const
  {
    for (auto *dep : dependencies) {
      result.append(dep->source.c_str());
    }
    result.append(source.c_str());
  }

  shader::BuiltinBits builtins_get() const
  {
    shader::BuiltinBits out_builtins = shader::BuiltinBits::NONE;
    for (auto *dep : dependencies) {
      out_builtins |= dep->builtins;
    }
    return out_builtins;
  }
};

}  // namespace blender::gpu

using namespace blender::gpu;

static GPUSourceDictionnary *g_sources = nullptr;

void gpu_shader_dependency_init()
{
  g_sources = new GPUSourceDictionnary();

#define SHADER_SOURCE(datatoc, filename, filepath) \
  g_sources->add_new(filename, new GPUSource(filepath, filename, datatoc));
#include "glsl_draw_source_list.h"
#include "glsl_gpu_source_list.h"
#ifdef WITH_OCIO
#  include "glsl_ocio_source_list.h"
#endif
#undef SHADER_SOURCE

  int errors = 0;
  for (auto *value : g_sources->values()) {
    errors += value->init_dependencies(*g_sources);
  }
  BLI_assert_msg(errors == 0, "Dependency errors detected: Aborting");
}

void gpu_shader_dependency_exit()
{
  for (auto *value : g_sources->values()) {
    delete value;
  }
  delete g_sources;
}

namespace blender::gpu::shader {

BuiltinBits gpu_shader_dependency_get_builtins(const StringRefNull shader_source_name)
{
  if (shader_source_name.is_empty()) {
    return shader::BuiltinBits::NONE;
  }
  if (g_sources->contains(shader_source_name) == false) {
    std::cout << "Error: Could not find \"" << shader_source_name
              << "\" in the list of registered source.\n";
    BLI_assert(0);
    return shader::BuiltinBits::NONE;
  }
  GPUSource *source = g_sources->lookup(shader_source_name);
  return source->builtins_get();
}

Vector<const char *> gpu_shader_dependency_get_resolved_source(
    const StringRefNull shader_source_name)
{
  Vector<const char *> result;
  GPUSource *source = g_sources->lookup(shader_source_name);
  source->build(result);
  return result;
}

StringRefNull gpu_shader_dependency_get_source(const StringRefNull shader_source_name)
{
  GPUSource *src = g_sources->lookup(shader_source_name);
  return src->source;
}

StringRefNull gpu_shader_dependency_get_filename_from_source_string(
    const StringRefNull source_string)
{
  for (auto &source : g_sources->values()) {
    if (source->source.c_str() == source_string.c_str()) {
      return source->filename;
    }
  }
  return "";
}

}  // namespace blender::gpu::shader
