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
#include <fmt/format.h>
#include <iomanip>
#include <iostream>
#include <regex>
#include <string>

#include "BLI_map.hh"
#include "BLI_string_ref.hh"

#include "CLG_log.h"

#include "gpu_capabilities_private.hh"
#include "gpu_material_library.hh"
#include "gpu_shader_create_info.hh"
#include "gpu_shader_dependency_private.hh"

#ifdef WITH_OPENSUBDIV
#  include "opensubdiv_capi_type.hh"
#  include "opensubdiv_evaluator_capi.hh"
#endif

#include "../glsl_preprocess/glsl_preprocess.hh"

extern "C" {
#define SHADER_SOURCE(filename_underscore, filename, filepath) \
  extern char datatoc_##filename_underscore[];
#include "glsl_compositor_source_list.h"
#include "glsl_draw_source_list.h"
#include "glsl_gpu_source_list.h"
#include "glsl_ocio_source_list.h"
#ifdef WITH_OPENSUBDIV
#  include "glsl_osd_source_list.h"
#endif
#undef SHADER_SOURCE
}

static CLG_LogRef LOG = {"shader.dependencies"};

namespace blender::gpu {

using GPUPrintFormatMap = Map<uint32_t, shader::PrintfFormat>;
using GPUSourceDictionary = Map<StringRef, GPUSource *>;
using GPUFunctionDictionary = Map<StringRef, GPUFunction *>;

struct GPUSource {
  StringRefNull fullpath;
  StringRefNull filename;
  StringRefNull source;
  std::string patched_source;
  Vector<StringRef> dependencies_names;
  Vector<GPUSource *> dependencies;
  bool dependencies_init = false;
  shader::BuiltinBits builtins = shader::BuiltinBits::NONE;
  /* True if this file content is supposed to be generated at runtime. */
  bool generated = false;

  /* NOTE: The next few functions are needed to keep isolation of the preprocessor.
   * Eventually, this should be revisited and the preprocessor should output
   * GPU structures. */

  shader::BuiltinBits convert_builtin_bit(shader::metadata::Builtin builtin)
  {
    using namespace blender::gpu::shader;
    using namespace blender::gpu::shader::metadata;
    switch (builtin) {
      case Builtin::FragCoord:
        return BuiltinBits::FRAG_COORD;
      case Builtin::FragStencilRef:
        return BuiltinBits::STENCIL_REF;
      case Builtin::FrontFacing:
        return BuiltinBits::FRONT_FACING;
      case Builtin::GlobalInvocationID:
        return BuiltinBits::GLOBAL_INVOCATION_ID;
      case Builtin::InstanceIndex:
      case Builtin::BaseInstance:
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
      case Builtin::runtime_generated:
        return BuiltinBits::RUNTIME_GENERATED;
    }
    BLI_assert_unreachable();
    return BuiltinBits::NONE;
  }

  GPUFunctionQual convert_qualifier(shader::metadata::Qualifier qualifier)
  {
    using namespace blender::gpu::shader;
    switch (qualifier) {
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

  GPUType convert_type(shader::metadata::Type type)
  {
    using namespace blender::gpu::shader;
    switch (type) {
      case metadata::Type::float1:
        return GPU_FLOAT;
      case metadata::Type::float2:
        return GPU_VEC2;
      case metadata::Type::float3:
        return GPU_VEC3;
      case metadata::Type::float4:
        return GPU_VEC4;
      case metadata::Type::float3x3:
        return GPU_MAT3;
      case metadata::Type::float4x4:
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

  GPUSource(
      const char *path,
      const char *file,
      const char *datatoc,
      GPUFunctionDictionary *g_functions,
      GPUPrintFormatMap *g_formats,
      std::function<void(GPUSource &, GPUFunctionDictionary *, GPUPrintFormatMap *)> metadata_fn)
      : fullpath(path), filename(file), source(datatoc)
  {
    metadata_fn(*this, g_functions, g_formats);
  };

  void add_builtin(shader::metadata::Builtin builtin)
  {
    builtins |= convert_builtin_bit(builtin);
  }

  void add_dependency(StringRef line)
  {
    dependencies_names.append(line);
  }

  void add_printf_format(uint32_t format_hash, std::string format, GPUPrintFormatMap *format_map)
  {
    /* TODO(fclem): Move this to gpu log. */
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
  }

  void add_function(StringRefNull name,
                    Span<shader::metadata::ArgumentFormat> arguments,
                    GPUFunctionDictionary *g_functions)
  {
    GPUFunction *func = MEM_new<GPUFunction>(__func__);
    name.copy_utf8_truncated(func->name, sizeof(func->name));
    func->source = reinterpret_cast<void *>(this);
    func->totparam = 0;
    for (auto arg : arguments) {
      if (func->totparam >= ARRAY_SIZE(func->paramtype)) {
        print_error(source, source.find(name), "Too many parameters in function");
        break;
      }
      func->paramqual[func->totparam] = convert_qualifier(arg.qualifier);
      func->paramtype[func->totparam] = convert_type(arg.type);
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
  int init_dependencies(const GPUSourceDictionary &dict)
  {
    if (this->dependencies_init) {
      return 0;
    }
    this->dependencies_init = true;

    using namespace shader;
    /* Auto dependency injection for debug capabilities. */
    if (flag_is_set(builtins, BuiltinBits::USE_PRINTF)) {
      dependencies.append_non_duplicates(dict.lookup("gpu_shader_print_lib.glsl"));
    }
    if (flag_is_set(builtins, BuiltinBits::USE_DEBUG_DRAW)) {
      dependencies.append_non_duplicates(dict.lookup("draw_debug_draw_lib.glsl"));
    }

    for (auto dependency_name : dependencies_names) {
      GPUSource *dependency_source = dict.lookup_default(dependency_name, nullptr);
      if (dependency_source == nullptr) {
        std::string error = std::string("Dependency not found : ") + dependency_name;
        print_error(source, 0, error.c_str());
        return 1;
      }

      /* Recursive. */
      int result = dependency_source->init_dependencies(dict);
      if (result != 0) {
        return 1;
      }
      dependencies.append_non_duplicates(dependency_source);
    }
    dependencies_names.clear();
    return 0;
  }

  void source_get(Vector<StringRefNull> &result,
                  const shader::GeneratedSourceList &generated_sources,
                  const GPUSourceDictionary &dict,
                  const GPUSource &from) const
  {
#define CLOG_FILE_INCLUDE(_from, _include) \
  if (CLOG_CHECK(&LOG, CLG_LEVEL_INFO) && (from).filename.c_str() != (_include).filename.c_str()) \
  { \
    const char *from_filename = (_from).filename.c_str(); \
    const char *include_filename = (_include).filename.c_str(); \
    const int from_size = int((_from).source.size()); \
    const int include_size = int((_include).source.size()); \
    std::string link = fmt::format( \
        "{}_{} --> {}_{}\n", from_filename, from_size, include_filename, include_size); \
    std::string style = fmt::format("style {}_{} fill:#{:x}{:x}0\n", \
                                    include_filename, \
                                    include_size, \
                                    min_uu(15, include_size / 1000), \
                                    15 - min_uu(15, include_size / 1000)); \
    CLG_log_raw(LOG.type, link.c_str()); \
    CLG_log_raw(LOG.type, style.c_str()); \
  }

    /* Check if this file was already included. */
    for (const StringRefNull &source_content : result) {
      /* Yes, compare pointer instead of string for speed.
       * Each source is guaranteed to be unique and non-moving during the building process. */
      if (source_content.c_str() == this->source.c_str()) {
        /* Already included. */
        CLOG_FILE_INCLUDE(from, *this);
        return;
      }
    }

    if (!flag_is_set(this->builtins, shader::BuiltinBits::RUNTIME_GENERATED)) {
      for (const auto &dependency : this->dependencies) {
        /* WATCH: Recursive. */
        dependency->source_get(result, generated_sources, dict, *this);
      }
      CLOG_FILE_INCLUDE(from, *this);
      result.append(this->source);
      return;
    }

    /* Linear lookup since we won't have more than a few per shaders.
     * Also avoid the complexity of a Map in info creation. */
    for (const shader::GeneratedSource &generated_src : generated_sources) {
      if (generated_src.filename == this->filename) {
        /* Include dependencies before the generated file. */
        for (const auto &dependency_name : generated_src.dependencies) {
          BLI_assert_msg(dependency_name != this->filename, "Recursive include");

          GPUSource *dependency_source = dict.lookup_default(dependency_name, nullptr);
          if (dependency_source == nullptr) {
            /* Will certainly fail compilation. But avoid crashing the application. */
            std::cerr << "Generated dependency not found : " + dependency_name << std::endl;
            return;
          }
          /* WATCH: Recursive. */
          dependency_source->source_get(result, generated_sources, dict, *this);
        }
        CLOG_FILE_INCLUDE(from, *this);
        result.append(generated_src.content);
        return;
      }
    }

    std::cerr << "warn: Generated source not provided. Using fallback for : " << this->filename
              << std::endl;
    /* Dependencies for generated sources are not folded on startup.
     * This allows for different set of dependencies at runtime. */
    for (const auto &dependency : this->dependencies) {
      /* WATCH: Recursive. */
      dependency->source_get(result, generated_sources, dict, *this);
    }
    CLOG_FILE_INCLUDE(from, *this);
    result.append(this->source);
  }

  /* Returns the final string with all includes done. */
  void build(Vector<StringRefNull> &result,
             const shader::GeneratedSourceList &generated_sources,
             const GPUSourceDictionary &dict) const
  {
    source_get(result, generated_sources, dict, *this);
  }

  shader::BuiltinBits builtins_get() const
  {
    shader::BuiltinBits out_builtins = builtins;
    for (auto *dep : dependencies) {
      out_builtins |= dep->builtins_get();
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

namespace shader {

#include "glsl_compositor_metadata_list.hh"
#include "glsl_draw_metadata_list.hh"
#include "glsl_gpu_metadata_list.hh"
#include "glsl_ocio_metadata_list.hh"
#ifdef WITH_OPENSUBDIV
#  include "glsl_osd_metadata_list.hh"
#endif

}  // namespace shader

}  // namespace blender::gpu

using namespace blender::gpu;

static GPUPrintFormatMap *g_formats = nullptr;
static GPUSourceDictionary *g_sources = nullptr;
static GPUFunctionDictionary *g_functions = nullptr;
static bool force_printf_injection = false;

void gpu_shader_dependency_init()
{
  g_formats = new GPUPrintFormatMap();
  g_sources = new GPUSourceDictionary();
  g_functions = new GPUFunctionDictionary();

#define SHADER_SOURCE(filename_underscore, filename, filepath) \
  g_sources->add_new(filename, \
                     new GPUSource(filepath, \
                                   filename, \
                                   datatoc_##filename_underscore, \
                                   g_functions, \
                                   g_formats, \
                                   blender::gpu::shader::metadata_##filename_underscore));

#include "glsl_compositor_source_list.h"
#include "glsl_draw_source_list.h"
#include "glsl_gpu_source_list.h"
#include "glsl_ocio_source_list.h"
#ifdef WITH_OPENSUBDIV
#  include "glsl_osd_source_list.h"
#endif
#undef SHADER_SOURCE
#ifdef WITH_OPENSUBDIV
  const blender::StringRefNull patch_basis_source = openSubdiv_getGLSLPatchBasisSource();
  g_sources->add_new(
      "osd_patch_basis.glsl",
      new GPUSource("osd_patch_basis.glsl",
                    "osd_patch_basis.glsl",
                    patch_basis_source.c_str(),
                    g_functions,
                    g_formats,
                    [](GPUSource &, GPUFunctionDictionary *, GPUPrintFormatMap *) {}));
#endif

  int errors = 0;
  for (auto *value : g_sources->values()) {
    errors += value->init_dependencies(*g_sources);
  }
  BLI_assert_msg(errors == 0, "Dependency errors detected: Aborting");
  UNUSED_VARS_NDEBUG(errors);

#if GPU_SHADER_PRINTF_ENABLE
  if (!g_formats->is_empty()) {
    /* Detect if there is any printf in node lib files.
     * See gpu_shader_dependency_force_gpu_print_injection(). */
    for (auto *value : g_sources->values()) {
      if (flag_is_set(value->builtins, shader::BuiltinBits::USE_PRINTF)) {
        if (value->filename.startswith("gpu_shader_material_")) {
          force_printf_injection = true;
          break;
        }
      }
    }
  }
#endif
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

GPUFunction *gpu_material_library_get_function(const char *name)
{
  GPUFunction *function = g_functions->lookup_default(name, nullptr);
  BLI_assert_msg(function != nullptr, "Requested function not in the function library");
  return function;
}

void gpu_material_library_use_function(blender::Set<blender::StringRefNull> &used_libraries,
                                       const char *name)
{
  GPUFunction *function = g_functions->lookup_default(name, nullptr);
  BLI_assert_msg(function != nullptr, "Requested function not in the function library");
  GPUSource *source = reinterpret_cast<GPUSource *>(function->source);
  used_libraries.add(source->filename.c_str());
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
    const StringRefNull shader_source_name,
    const shader::GeneratedSourceList &generated_sources,
    const StringRefNull shader_name)
{
  Vector<StringRefNull> result;
  GPUSource *src = g_sources->lookup_default(shader_source_name, nullptr);
  if (src == nullptr) {
    std::cerr << "Error source not found : " << shader_source_name << std::endl;
  }
  CLOG_INFO(&LOG, "Resolved Source Tree (Mermaid flowchart) %s", shader_name.c_str());
  if (CLOG_CHECK(&LOG, CLG_LEVEL_INFO)) {
    CLG_log_raw(LOG.type, "flowchart LR\n");
  }
  src->build(result, generated_sources, *g_sources);
  if (CLOG_CHECK(&LOG, CLG_LEVEL_INFO)) {
    CLG_log_raw(LOG.type, "\n");
  }
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
