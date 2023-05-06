/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include <epoxy/gl.h>

#include "gpu_shader_create_info.hh"
#include "gpu_shader_private.hh"

namespace blender {
namespace gpu {

/**
 * Implementation of shader compilation and uniforms handling using OpenGL.
 */
class GLShader : public Shader {
  friend shader::ShaderCreateInfo;
  friend shader::StageInterfaceInfo;

 private:
  /** Handle for full program (links shader stages below). */
  GLuint shader_program_ = 0;
  /** Individual shader stages. */
  GLuint vert_shader_ = 0;
  GLuint geom_shader_ = 0;
  GLuint frag_shader_ = 0;
  GLuint compute_shader_ = 0;
  /** True if any shader failed to compile. */
  bool compilation_failed_ = false;

  eGPUShaderTFBType transform_feedback_type_ = GPU_SHADER_TFB_NONE;

 public:
  GLShader(const char *name);
  ~GLShader();

  /** Return true on success. */
  void vertex_shader_from_glsl(MutableSpan<const char *> sources) override;
  void geometry_shader_from_glsl(MutableSpan<const char *> sources) override;
  void fragment_shader_from_glsl(MutableSpan<const char *> sources) override;
  void compute_shader_from_glsl(MutableSpan<const char *> sources) override;
  bool finalize(const shader::ShaderCreateInfo *info = nullptr) override;
  void warm_cache(int /*limit*/) override{};

  std::string resources_declare(const shader::ShaderCreateInfo &info) const override;
  std::string vertex_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string fragment_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_layout_declare(const shader::ShaderCreateInfo &info) const override;
  std::string compute_layout_declare(const shader::ShaderCreateInfo &info) const override;

  /** Should be called before linking. */
  void transform_feedback_names_set(Span<const char *> name_list,
                                    eGPUShaderTFBType geom_type) override;
  bool transform_feedback_enable(GPUVertBuf *buf) override;
  void transform_feedback_disable() override;

  void bind() override;
  void unbind() override;

  void uniform_float(int location, int comp_len, int array_size, const float *data) override;
  void uniform_int(int location, int comp_len, int array_size, const int *data) override;

  /** DEPRECATED: Kept only because of BGL API. */
  int program_handle_get() const override;

  bool is_compute() const
  {
    return compute_shader_ != 0;
  }

 private:
  char *glsl_patch_get(GLenum gl_stage);

  /** Create, compile and attach the shader stage to the shader program. */
  GLuint create_shader_stage(GLenum gl_stage, MutableSpan<const char *> sources);

  /**
   * \brief features available on newer implementation such as native barycentric coordinates
   * and layered rendering, necessitate a geometry shader to work on older hardware.
   */
  std::string workaround_geometry_shader_source_create(const shader::ShaderCreateInfo &info);

  bool do_geometry_shader_injection(const shader::ShaderCreateInfo *info);

  MEM_CXX_CLASS_ALLOC_FUNCS("GLShader");
};

class GLLogParser : public GPULogParser {
 public:
  const char *parse_line(const char *log_line, GPULogItem &log_item) override;

 protected:
  const char *skip_severity_prefix(const char *log_line, GPULogItem &log_item);
  const char *skip_severity_keyword(const char *log_line, GPULogItem &log_item);

  MEM_CXX_CLASS_ALLOC_FUNCS("GLLogParser");
};

}  // namespace gpu
}  // namespace blender
