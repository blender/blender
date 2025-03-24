/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Shader source dependency builder that make possible to support #include directive inside the
 * shader files.
 */

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <regex>
#include <string>

#include "BLI_ghash.h"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"

#include "gpu_capabilities_private.hh"
#include "gpu_material_library.hh"
#include "gpu_shader_create_info.hh"
#include "gpu_shader_dependency_private.hh"

#include "../glsl_preprocess/glsl_preprocess.hh"

extern "C" {
#define SHADER_SOURCE(datatoc, filename, filepath) extern char datatoc[];
#include "glsl_compositor_source_list.h"
#include "glsl_draw_source_list.h"
#include "glsl_gpu_source_list.h"
#ifdef WITH_OCIO
#  include "glsl_ocio_source_list.h"
#endif
#undef SHADER_SOURCE
}

namespace blender::gpu {

using GPUPrintFormatMap = Map<uint32_t, shader::PrintfFormat>;
using GPUSourceDictionnary = Map<StringRef, struct GPUSource *>;
using GPUFunctionDictionnary = Map<StringRef, GPUFunction *>;

struct GPUSource {
  StringRefNull fullpath;
  StringRefNull filename;
  StringRefNull source;
  std::string patched_source;
  Vector<StringRef> dependencies_names;
  Vector<GPUSource *> dependencies;
  bool dependencies_init = false;
  shader::BuiltinBits builtins = shader::BuiltinBits::NONE;

  shader::BuiltinBits parse_builtin_bit(StringRef builtin)
  {
    using namespace blender::gpu::shader;
    using namespace blender::gpu::shader::metadata;
    switch (Builtin(std::stoull(builtin))) {
      case Builtin::FragCoord:
        return BuiltinBits::FRAG_COORD;
      case Builtin::FrontFacing:
        return BuiltinBits::FRONT_FACING;
      case Builtin::GlobalInvocationID:
        return BuiltinBits::GLOBAL_INVOCATION_ID;
      case Builtin::InstanceID:
        return BuiltinBits::INSTANCE_ID;
      case Builtin::LocalInvocationID:
        return BuiltinBits::LOCAL_INVOCATION_ID;
      case Builtin::LocalInvocationIndex:
        return BuiltinBits::LOCAL_INVOCATION_INDEX;
      case Builtin::NumWorkGroup:
        return BuiltinBits::NUM_WORK_GROUP;
      case Builtin::PointCoord:
        return BuiltinBits::POINT_COORD;
      case Builtin::PointSize:
        return BuiltinBits::POINT_SIZE;
      case Builtin::PrimitiveID:
        return BuiltinBits::PRIMITIVE_ID;
      case Builtin::VertexID:
        return BuiltinBits::VERTEX_ID;
      case Builtin::WorkGroupID:
        return BuiltinBits::WORK_GROUP_ID;
      case Builtin::WorkGroupSize:
        return BuiltinBits::WORK_GROUP_SIZE;
      case Builtin::drw_debug:
#ifndef NDEBUG
        return BuiltinBits::USE_DEBUG_DRAW;
#else
        return BuiltinBits::NONE;
#endif
      case Builtin::assert:
      case Builtin::printf:
#if GPU_SHADER_PRINTF_ENABLE
        return BuiltinBits::USE_PRINTF;
#else
        return BuiltinBits::NONE;
#endif
    }
    BLI_assert_unreachable();
    return BuiltinBits::NONE;
  }

  GPUFunctionQual parse_qualifier(StringRef qualifier)
  {
    using namespace blender::gpu::shader;
    switch (metadata::Qualifier(std::stoull(qualifier))) {
      case metadata::Qualifier::in:
        return FUNCTION_QUAL_IN;
      case metadata::Qualifier::out:
        return FUNCTION_QUAL_OUT;
      case metadata::Qualifier::inout:
        return FUNCTION_QUAL_INOUT;
    }
    BLI_assert_unreachable();
    return FUNCTION_QUAL_IN;
  }

  eGPUType parse_type(StringRef type)
  {
    using namespace blender::gpu::shader;
    switch (metadata::Type(std::stoull(type))) {
      case metadata::Type::vec1:
        return GPU_FLOAT;
      case metadata::Type::vec2:
        return GPU_VEC2;
      case metadata::Type::vec3:
        return GPU_VEC3;
      case metadata::Type::vec4:
        return GPU_VEC4;
      case metadata::Type::mat3:
        return GPU_MAT3;
      case metadata::Type::mat4:
        return GPU_MAT4;
      case metadata::Type::sampler1DArray:
        return GPU_TEX1D_ARRAY;
      case metadata::Type::sampler2DArray:
        return GPU_TEX2D_ARRAY;
      case metadata::Type::sampler2D:
        return GPU_TEX2D;
      case metadata::Type::sampler3D:
        return GPU_TEX3D;
      case metadata::Type::Closure:
        return GPU_CLOSURE;
    }
    BLI_assert_unreachable();
    return GPU_NONE;
  }

  StringRef split_on(StringRef &data, char token)
  {
    /* Assume lines are terminated by `\n`. */
    int64_t pos = data.find(token);
    if (pos == StringRef::not_found) {
      StringRef line = data;
      data = data.substr(0, 0);
      return line;
    }
    StringRef line = data.substr(0, pos);
    data = data.substr(pos + 1);
    return line;
  }

  StringRef pop_line(StringRef &data)
  {
    /* Assume lines are terminated by `\n`. */
    return split_on(data, '\n');
  }

  StringRef pop_token(StringRef &data)
  {
    /* Assumes tokens are split by spaces. */
    return split_on(data, ' ');
  }

  GPUSource(const char *path,
            const char *file,
            const char *datatoc,
            GPUFunctionDictionnary *g_functions,
            GPUPrintFormatMap *g_formats)
      : fullpath(path), filename(file), source(datatoc)
  {
    /* Extract metadata string. */
    int64_t sta = source.rfind("//__blender_metadata_sta");
    int64_t end = source.rfind("//__blender_metadata_end");
    StringRef metadata = source.substr(sta, end - sta);
    pop_line(metadata);

    /* Non-library files contains functions with unsupported argument types.
     * Also Non-library files are not supposed to be referenced for GPU node-tree. */
    const bool do_parse_function = is_from_material_library();

    StringRef line;
    while ((line = pop_line(metadata)).is_empty() == false) {
      using namespace blender::gpu::shader;
      /* Skip comment start. */
      pop_token(line);

      StringRef identifier = pop_token(line);
      switch (uint64_t(std::stoull(identifier))) {
        case Preprocessor::hash("function"):
          if (do_parse_function) {
            parse_function(line, g_functions);
          }
          break;
        case Preprocessor::hash("string"):
          parse_string(line, g_formats);
          break;
        case Preprocessor::hash("builtin"):
          parse_builtin(line);
          break;
        case Preprocessor::hash("dependency"):
          parse_dependency(line);
          break;
        default:
          BLI_assert_unreachable();
          break;
      }
    }
  };

  void parse_builtin(StringRef line)
  {
    builtins |= parse_builtin_bit(pop_token(line));
  }

  void parse_dependency(StringRef line)
  {
    dependencies_names.append(line);
  }

  void parse_string(StringRef line, GPUPrintFormatMap *format_map)
  {
    /* TODO(fclem): Move this to gpu log. */
    auto add_format = [&](uint32_t format_hash, std::string format) {
      if (format_map->contains(format_hash)) {
        if (format_map->lookup(format_hash).format_str != format) {
          print_error(format, 0, "printf format hash collision.");
        }
        else {
          /* The format map already have the same format. */
        }
      }
      else {
        shader::PrintfFormat fmt;
        /* Save for hash collision comparison. */
        fmt.format_str = format;

        /* Escape characters replacement. Do the most common ones. */
        format = std::regex_replace(format, std::regex(R"(\\n)"), "\n");
        format = std::regex_replace(format, std::regex(R"(\\v)"), "\v");
        format = std::regex_replace(format, std::regex(R"(\\t)"), "\t");
        format = std::regex_replace(format, std::regex(R"(\\')"), "\'");
        format = std::regex_replace(format, std::regex(R"(\\")"), "\"");
        format = std::regex_replace(format, std::regex(R"(\\\\)"), "\\");

        shader::PrintfFormat::Block::ArgumentType type =
            shader::PrintfFormat::Block::ArgumentType::NONE;
        int64_t start = 0, end = 0;
        while ((end = format.find_first_of('%', start + 1)) != -1) {
          /* Add the previous block without the newly found % character. */
          fmt.format_blocks.append({type, format.substr(start, end - start)});
          /* Format type of the next block. */
          /* TODO(fclem): This doesn't support advance formats like `%3.2f`. */
          switch (format[end + 1]) {
            case 'x':
            case 'u':
              type = shader::PrintfFormat::Block::ArgumentType::UINT;
              break;
            case 'd':
              type = shader::PrintfFormat::Block::ArgumentType::INT;
              break;
            case 'f':
              type = shader::PrintfFormat::Block::ArgumentType::FLOAT;
              break;
            default:
              BLI_assert_msg(0, "Printing format unsupported");
              break;
          }
          /* Start of the next block. */
          start = end;
        }
        fmt.format_blocks.append({type, format.substr(start, format.size() - start)});

        format_map->add(format_hash, fmt);
      }
    };

    StringRef hash = pop_token(line);
    StringRef string = line;
    add_format(uint32_t(std::stoul(hash)), string);
  }

  void parse_function(StringRef line, GPUFunctionDictionnary *g_functions)
  {
    StringRef name = pop_token(line);

    GPUFunction *func = MEM_new<GPUFunction>(__func__);
    name.copy_utf8_truncated(func->name, sizeof(func->name));
    func->source = reinterpret_cast<void *>(this);
    func->totparam = 0;
    while (true) {
      StringRef arg_qual = pop_token(line);
      StringRef arg_type = pop_token(line);
      if (arg_qual.is_empty()) {
        break;
      }
      if (func->totparam >= ARRAY_SIZE(func->paramtype)) {
        print_error(source, source.find(name), "Too many parameters in function");
        break;
      }
      func->paramqual[func->totparam] = parse_qualifier(arg_qual);
      func->paramtype[func->totparam] = parse_type(arg_type);
      func->totparam++;
    }

    bool insert = g_functions->add(func->name, func);
    /* NOTE: We allow overloading non void function, but only if the function comes from the
     * same file. Otherwise the dependency system breaks. */
    if (!insert) {
      GPUSource *other_source = reinterpret_cast<GPUSource *>(g_functions->lookup(name)->source);
      if (other_source != this) {
        const char *msg = "Function redefinition or overload in two different files ...";
        print_error(source, source.find(name), msg);
        print_error(other_source->source,
                    other_source->source.find(name),
                    "... previous definition was here");
      }
      else {
        /* Non-void function overload. */
        MEM_delete(func);
      }
    }
  }

  void print_error(const StringRef &input, int64_t offset, const StringRef message)
  {
    StringRef sub = input.substr(0, offset);
    int64_t line_number = std::count(sub.begin(), sub.end(), '\n') + 1;
    int64_t line_end = input.find("\n", offset);
    int64_t line_start = input.rfind("\n", offset) + 1;
    int64_t char_number = offset - line_start + 1;

    /* TODO Use clog. */

    std::cerr << fullpath << ":" << line_number << ":" << char_number;

    std::cerr << " error: " << message << "\n";
    std::cerr << std::setw(5) << line_number << " | "
              << input.substr(line_start, line_end - line_start) << "\n";
    std::cerr << "      | ";
    for (int64_t i = 0; i < char_number - 1; i++) {
      std::cerr << " ";
    }
    std::cerr << "^\n";
  }

  /* Return 1 one error. */
  int init_dependencies(const GPUSourceDictionnary &dict,
                        const GPUFunctionDictionnary &g_functions)
  {
    if (this->dependencies_init) {
      return 0;
    }
    this->dependencies_init = true;

    using namespace shader;
    /* Auto dependency injection for debug capabilities. */
    if ((builtins & BuiltinBits::USE_PRINTF) == BuiltinBits::USE_PRINTF) {
      dependencies.append_non_duplicates(dict.lookup("gpu_shader_print_lib.glsl"));
    }
    if ((builtins & BuiltinBits::USE_DEBUG_DRAW) == BuiltinBits::USE_DEBUG_DRAW) {
      dependencies.append_non_duplicates(dict.lookup("common_debug_draw_lib.glsl"));
    }

    for (auto dependency_name : dependencies_names) {
      GPUSource *dependency_source = dict.lookup_default(dependency_name, nullptr);
      if (dependency_source == nullptr) {
        std::string error = std::string("Dependency not found : ") + dependency_name;
        print_error(source, 0, error.c_str());
        return 1;
      }

      /* Recursive. */
      int result = dependency_source->init_dependencies(dict, g_functions);
      if (result != 0) {
        return 1;
      }

      for (auto *dep : dependency_source->dependencies) {
        dependencies.append_non_duplicates(dep);
      }
      dependencies.append_non_duplicates(dependency_source);
    }
    dependencies_names.clear();
    return 0;
  }

  /* Returns the final string with all includes done. */
  void build(Vector<StringRefNull> &result) const
  {
    for (auto *dep : dependencies) {
      result.append(dep->source);
    }
    result.append(source);
  }

  shader::BuiltinBits builtins_get() const
  {
    shader::BuiltinBits out_builtins = builtins;
    for (auto *dep : dependencies) {
      out_builtins |= dep->builtins;
    }
    return out_builtins;
  }

  bool is_from_material_library() const
  {
    return (filename.startswith("gpu_shader_material_") ||
            filename.startswith("gpu_shader_common_") ||
            filename.startswith("gpu_shader_compositor_")) &&
           filename.endswith(".glsl");
  }
};

}  // namespace blender::gpu

using namespace blender::gpu;

static GPUPrintFormatMap *g_formats = nullptr;
static GPUSourceDictionnary *g_sources = nullptr;
static GPUFunctionDictionnary *g_functions = nullptr;
static bool force_printf_injection = false;

void gpu_shader_dependency_init()
{
  g_formats = new GPUPrintFormatMap();
  g_sources = new GPUSourceDictionnary();
  g_functions = new GPUFunctionDictionnary();

#define SHADER_SOURCE(datatoc, filename, filepath) \
  g_sources->add_new(filename, new GPUSource(filepath, filename, datatoc, g_functions, g_formats));
#include "glsl_compositor_source_list.h"
#include "glsl_draw_source_list.h"
#include "glsl_gpu_source_list.h"
#ifdef WITH_OCIO
#  include "glsl_ocio_source_list.h"
#endif
#undef SHADER_SOURCE

  int errors = 0;
  for (auto *value : g_sources->values()) {
    errors += value->init_dependencies(*g_sources, *g_functions);
  }
  BLI_assert_msg(errors == 0, "Dependency errors detected: Aborting");
  UNUSED_VARS_NDEBUG(errors);

#if GPU_SHADER_PRINTF_ENABLE
  if (!g_formats->is_empty()) {
    /* Detect if there is any printf in node lib files.
     * See gpu_shader_dependency_force_gpu_print_injection(). */
    for (auto *value : g_sources->values()) {
      if (bool(value->builtins & shader::BuiltinBits::USE_PRINTF)) {
        if (value->filename.startswith("gpu_shader_material_")) {
          force_printf_injection = true;
          break;
        }
      }
    }
  }
#endif

  if (GCaps.line_directive_workaround) {
    for (auto *value : g_sources->values()) {
      value->patched_source = value->source;
      value->source = value->patched_source.c_str();
      size_t start_pos = 0;
      while ((start_pos = value->patched_source.find("#line ", start_pos)) != std::string::npos) {
        value->patched_source[start_pos] = '/';
        value->patched_source[start_pos + 1] = '/';
      }
    }
  }
}

void gpu_shader_dependency_exit()
{
  for (auto *value : g_sources->values()) {
    delete value;
  }
  for (auto *value : g_functions->values()) {
    MEM_delete(value);
  }
  delete g_formats;
  delete g_sources;
  delete g_functions;
  g_formats = nullptr;
  g_sources = nullptr;
  g_functions = nullptr;
}

GPUFunction *gpu_material_library_use_function(GSet *used_libraries, const char *name)
{
  GPUFunction *function = g_functions->lookup_default(name, nullptr);
  BLI_assert_msg(function != nullptr, "Requested function not in the function library");
  GPUSource *source = reinterpret_cast<GPUSource *>(function->source);
  BLI_gset_add(used_libraries, const_cast<char *>(source->filename.c_str()));
  return function;
}

namespace blender::gpu::shader {

bool gpu_shader_dependency_force_gpu_print_injection()
{
  /* WORKAROUND: We cannot know what shader will require printing if the printf is inside shader
   * node code. In this case, we just force injection inside all shaders. */
  return force_printf_injection;
}

bool gpu_shader_dependency_has_printf()
{
  return (g_formats != nullptr) && !g_formats->is_empty();
}

const PrintfFormat &gpu_shader_dependency_get_printf_format(uint32_t format_hash)
{
  return g_formats->lookup(format_hash);
}

BuiltinBits gpu_shader_dependency_get_builtins(const StringRefNull shader_source_name)
{
  if (shader_source_name.is_empty()) {
    return shader::BuiltinBits::NONE;
  }
  if (g_sources->contains(shader_source_name) == false) {
    std::cerr << "Error: Could not find \"" << shader_source_name
              << "\" in the list of registered source.\n";
    BLI_assert(0);
    return shader::BuiltinBits::NONE;
  }
  GPUSource *source = g_sources->lookup(shader_source_name);
  return source->builtins_get();
}

Vector<StringRefNull> gpu_shader_dependency_get_resolved_source(
    const StringRefNull shader_source_name)
{
  Vector<StringRefNull> result;
  GPUSource *src = g_sources->lookup_default(shader_source_name, nullptr);
  if (src == nullptr) {
    std::cerr << "Error source not found : " << shader_source_name << std::endl;
  }
  src->build(result);
  return result;
}

StringRefNull gpu_shader_dependency_get_source(const StringRefNull shader_source_name)
{
  GPUSource *src = g_sources->lookup_default(shader_source_name, nullptr);
  if (src == nullptr) {
    std::cerr << "Error source not found : " << shader_source_name << std::endl;
  }
  return src->source;
}

StringRefNull gpu_shader_dependency_get_filename_from_source_string(const StringRef source_string)
{
  for (auto &source : g_sources->values()) {
    if (source->source == source_string) {
      return source->filename;
    }
  }
  return "";
}

}  // namespace blender::gpu::shader
