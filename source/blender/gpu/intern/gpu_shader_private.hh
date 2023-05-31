/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_span.hh"
#include "BLI_string_ref.hh"

#include "GPU_shader.h"
#include "gpu_shader_create_info.hh"
#include "gpu_shader_interface.hh"
#include "gpu_vertex_buffer_private.hh"

#include <string>

namespace blender {
namespace gpu {

class GPULogParser;

/**
 * Implementation of shader compilation and uniforms handling.
 * Base class which is then specialized for each implementation (GL, VK, ...).
 */
class Shader {
 public:
  /** Uniform & attribute locations for shader. */
  ShaderInterface *interface = nullptr;

 protected:
  /** For debugging purpose. */
  char name[64];

  /* Parent shader can be used for shaders which are derived from the same source material.
   * The child shader can pull information from its parent to prepare additional resources
   * such as PSOs upfront. This enables asynchronous PSO compilation which mitigates stuttering
   * when updating new materials. */
  Shader *parent_shader_ = nullptr;

 public:
  Shader(const char *name);
  virtual ~Shader();

  virtual void vertex_shader_from_glsl(MutableSpan<const char *> sources) = 0;
  virtual void geometry_shader_from_glsl(MutableSpan<const char *> sources) = 0;
  virtual void fragment_shader_from_glsl(MutableSpan<const char *> sources) = 0;
  virtual void compute_shader_from_glsl(MutableSpan<const char *> sources) = 0;
  virtual bool finalize(const shader::ShaderCreateInfo *info = nullptr) = 0;
  /* Pre-warms PSOs using parent shader's cached PSO descriptors. Limit specifies maximum PSOs to
   * warm. If -1, compiles all PSO permutations in parent shader.
   *
   * See `GPU_shader_warm_cache(..)` in `GPU_shader.h` for more information. */
  virtual void warm_cache(int limit) = 0;

  virtual void transform_feedback_names_set(Span<const char *> name_list,
                                            eGPUShaderTFBType geom_type) = 0;
  virtual bool transform_feedback_enable(GPUVertBuf *) = 0;
  virtual void transform_feedback_disable() = 0;

  virtual void bind() = 0;
  virtual void unbind() = 0;

  virtual void uniform_float(int location, int comp_len, int array_size, const float *data) = 0;
  virtual void uniform_int(int location, int comp_len, int array_size, const int *data) = 0;

  std::string defines_declare(const shader::ShaderCreateInfo &info) const;
  virtual std::string resources_declare(const shader::ShaderCreateInfo &info) const = 0;
  virtual std::string vertex_interface_declare(const shader::ShaderCreateInfo &info) const = 0;
  virtual std::string fragment_interface_declare(const shader::ShaderCreateInfo &info) const = 0;
  virtual std::string geometry_interface_declare(const shader::ShaderCreateInfo &info) const = 0;
  virtual std::string geometry_layout_declare(const shader::ShaderCreateInfo &info) const = 0;
  virtual std::string compute_layout_declare(const shader::ShaderCreateInfo &info) const = 0;

  /* DEPRECATED: Kept only because of BGL API. */
  virtual int program_handle_get() const = 0;

  inline const char *const name_get() const
  {
    return name;
  }

  inline void parent_set(Shader *parent)
  {
    parent_shader_ = parent;
  }

  inline Shader *parent_get() const
  {
    return parent_shader_;
  }

  static bool srgb_uniform_dirty_get();
  static void set_srgb_uniform(GPUShader *shader);
  static void set_framebuffer_srgb_target(int use_srgb_to_linear);

 protected:
  void print_log(Span<const char *> sources,
                 const char *log,
                 const char *stage,
                 bool error,
                 GPULogParser *parser);
};

/* Syntactic sugar. */
static inline GPUShader *wrap(Shader *vert)
{
  return reinterpret_cast<GPUShader *>(vert);
}
static inline Shader *unwrap(GPUShader *vert)
{
  return reinterpret_cast<Shader *>(vert);
}
static inline const Shader *unwrap(const GPUShader *vert)
{
  return reinterpret_cast<const Shader *>(vert);
}

enum class Severity {
  Unknown,
  Warning,
  Error,
  Note,
};

struct LogCursor {
  int source = -1;
  int row = -1;
  int column = -1;
};

struct GPULogItem {
  LogCursor cursor;
  bool source_base_row = false;
  Severity severity = Severity::Unknown;
};

class GPULogParser {
 public:
  virtual const char *parse_line(const char *log_line, GPULogItem &log_item) = 0;

 protected:
  const char *skip_severity(const char *log_line,
                            GPULogItem &log_item,
                            const char *error_msg,
                            const char *warning_msg,
                            const char *note_msg) const;
  const char *skip_separators(const char *log_line, const StringRef separators) const;
  const char *skip_until(const char *log_line, char stop_char) const;
  bool at_number(const char *log_line) const;
  bool at_any(const char *log_line, const StringRef chars) const;
  int parse_number(const char *log_line, const char **r_new_position) const;

  MEM_CXX_CLASS_ALLOC_FUNCS("GPULogParser");
};

}  // namespace gpu
}  // namespace blender

/* XXX do not use it. Special hack to use OCIO with batch API. */
GPUShader *immGetShader();
