/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU shader interface (C --> GLSL)
 */

#include "BLI_bitmap.h"

#include "gl_batch.hh"
#include "gl_context.hh"

#include "gl_shader_interface.hh"

#include "GPU_capabilities.hh"

using namespace blender::gpu::shader;
namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Binding assignment
 *
 * To mimic vulkan, we assign binding at shader creation to avoid shader recompilation.
 * In the future, we should set it in the shader using layout(binding = i) and query its value.
 * \{ */

static inline int block_binding(int32_t program, uint32_t block_index)
{
  /* For now just assign a consecutive index. In the future, we should set it in
   * the shader using layout(binding = i) and query its value. */
  glUniformBlockBinding(program, block_index, block_index);
  return block_index;
}

static inline int sampler_binding(int32_t program,
                                  uint32_t uniform_index,
                                  int32_t uniform_location,
                                  int *sampler_len)
{
  /* Identify sampler uniforms and assign sampler units to them. */
  GLint type;
  glGetActiveUniformsiv(program, 1, &uniform_index, GL_UNIFORM_TYPE, &type);

  switch (type) {
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_CUBE_MAP_ARRAY_ARB: /* OpenGL 4.0 */
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_1D_ARRAY:
    case GL_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_1D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_MULTISAMPLE:
    case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_SAMPLER_BUFFER:
    case GL_INT_SAMPLER_1D:
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_1D_ARRAY:
    case GL_INT_SAMPLER_2D_ARRAY:
    case GL_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_INT_SAMPLER_BUFFER:
    case GL_UNSIGNED_INT_SAMPLER_1D:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_BUFFER: {
      /* For now just assign a consecutive index. In the future, we should set it in
       * the shader using layout(binding = i) and query its value. */
      int binding = *sampler_len;
      glUniform1i(uniform_location, binding);
      (*sampler_len)++;
      return binding;
    }
    default:
      return -1;
  }
}

static inline int image_binding(int32_t program,
                                uint32_t uniform_index,
                                int32_t uniform_location,
                                int *image_len)
{
  /* Identify image uniforms and assign image units to them. */
  GLint type;
  glGetActiveUniformsiv(program, 1, &uniform_index, GL_UNIFORM_TYPE, &type);

  switch (type) {
    case GL_IMAGE_1D:
    case GL_IMAGE_2D:
    case GL_IMAGE_3D:
    case GL_IMAGE_CUBE:
    case GL_IMAGE_BUFFER:
    case GL_IMAGE_1D_ARRAY:
    case GL_IMAGE_2D_ARRAY:
    case GL_IMAGE_CUBE_MAP_ARRAY:
    case GL_INT_IMAGE_1D:
    case GL_INT_IMAGE_2D:
    case GL_INT_IMAGE_3D:
    case GL_INT_IMAGE_CUBE:
    case GL_INT_IMAGE_BUFFER:
    case GL_INT_IMAGE_1D_ARRAY:
    case GL_INT_IMAGE_2D_ARRAY:
    case GL_INT_IMAGE_CUBE_MAP_ARRAY:
    case GL_UNSIGNED_INT_IMAGE_1D:
    case GL_UNSIGNED_INT_IMAGE_2D:
    case GL_UNSIGNED_INT_IMAGE_3D:
    case GL_UNSIGNED_INT_IMAGE_CUBE:
    case GL_UNSIGNED_INT_IMAGE_BUFFER:
    case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
    case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
    case GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY: {
      /* For now just assign a consecutive index. In the future, we should set it in
       * the shader using layout(binding = i) and query its value. */
      int binding = *image_len;
      glUniform1i(uniform_location, binding);
      (*image_len)++;
      return binding;
    }
    default:
      return -1;
  }
}

static inline int ssbo_binding(int32_t program, uint32_t ssbo_index)
{
  GLint binding = -1;
  GLenum property = GL_BUFFER_BINDING;
  GLint values_written = 0;
  glGetProgramResourceiv(
      program, GL_SHADER_STORAGE_BLOCK, ssbo_index, 1, &property, 1, &values_written, &binding);

  return binding;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Creation / Destruction
 * \{ */

GLShaderInterface::GLShaderInterface(GLuint program, const shader::ShaderCreateInfo &info)
{
  using namespace blender::gpu::shader;

  attr_len_ = info.vertex_inputs_.size();
  uniform_len_ = info.push_constants_.size();
  constant_len_ = info.specialization_constants_.size();
  ubo_len_ = 0;
  ssbo_len_ = 0;

  Vector<ShaderCreateInfo::Resource> all_resources = info.resources_get_all_();

  for (ShaderCreateInfo::Resource &res : all_resources) {
    switch (res.bind_type) {
      case ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
        ubo_len_++;
        break;
      case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
        ssbo_len_++;
        break;
      case ShaderCreateInfo::Resource::BindType::SAMPLER:
        uniform_len_++;
        break;
      case ShaderCreateInfo::Resource::BindType::IMAGE:
        uniform_len_++;
        break;
    }
  }

  size_t workaround_names_size = 0;
  Vector<StringRefNull> workaround_uniform_names;
  auto check_enabled_uniform = [&](const char *uniform_name) {
    if (glGetUniformLocation(program, uniform_name) != -1) {
      workaround_uniform_names.append(uniform_name);
      workaround_names_size += StringRefNull(uniform_name).size() + 1;
      uniform_len_++;
    }
  };

  if (!GLContext::shader_draw_parameters_support) {
    check_enabled_uniform("gpu_BaseInstance");
  }

  BLI_assert_msg(ubo_len_ <= 16, "enabled_ubo_mask_ is uint16_t");

  int input_tot_len = attr_len_ + ubo_len_ + uniform_len_ + ssbo_len_ + constant_len_;
  inputs_ = (ShaderInput *)MEM_callocN(sizeof(ShaderInput) * input_tot_len, __func__);
  ShaderInput *input = inputs_;

  name_buffer_ = (char *)MEM_mallocN(info.interface_names_size_ + workaround_names_size,
                                     "name_buffer");
  uint32_t name_buffer_offset = 0;

  /* Necessary to make #glUniform works. TODO(fclem) Remove. */
  GLuint last_program;
  glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *)&last_program);

  glUseProgram(program);

  /* Attributes */
  for (const ShaderCreateInfo::VertIn &attr : info.vertex_inputs_) {
    copy_input_name(input, attr.name, name_buffer_, name_buffer_offset);
    if (true || !GLContext::explicit_location_support) {
      input->location = input->binding = glGetAttribLocation(program, attr.name.c_str());
    }
    else {
      input->location = input->binding = attr.index;
    }
    if (input->location != -1) {
      enabled_attr_mask_ |= (1 << input->location);

      /* Used in `GPU_shader_get_attribute_info`. */
      attr_types_[input->location] = uint8_t(attr.type);
    }

    input++;
  }

  /* Uniform Blocks */
  for (const ShaderCreateInfo::Resource &res : all_resources) {
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER) {
      copy_input_name(input, res.uniformbuf.name, name_buffer_, name_buffer_offset);
      if (true || !GLContext::explicit_location_support) {
        input->location = glGetUniformBlockIndex(program, name_buffer_ + input->name_offset);
        glUniformBlockBinding(program, input->location, res.slot);
      }
      input->binding = res.slot;
      enabled_ubo_mask_ |= (1 << input->binding);
      input++;
    }
  }

  /* Uniforms & samplers & images */
  for (const ShaderCreateInfo::Resource &res : all_resources) {
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER) {
      copy_input_name(input, res.sampler.name, name_buffer_, name_buffer_offset);
      /* Until we make use of explicit uniform location or eliminate all
       * sampler manually changing. */
      if (true || !GLContext::explicit_location_support) {
        input->location = glGetUniformLocation(program, res.sampler.name.c_str());
        glUniform1i(input->location, res.slot);
      }
      input->binding = res.slot;
      enabled_tex_mask_ |= (1ull << input->binding);
      input++;
    }
    else if (res.bind_type == ShaderCreateInfo::Resource::BindType::IMAGE) {
      copy_input_name(input, res.image.name, name_buffer_, name_buffer_offset);
      /* Until we make use of explicit uniform location. */
      if (true || !GLContext::explicit_location_support) {
        input->location = glGetUniformLocation(program, res.image.name.c_str());
        glUniform1i(input->location, res.slot);
      }
      input->binding = res.slot;
      enabled_ima_mask_ |= (1 << input->binding);
      input++;
    }
  }
  for (const ShaderCreateInfo::PushConst &uni : info.push_constants_) {
    copy_input_name(input, uni.name, name_buffer_, name_buffer_offset);
    input->location = glGetUniformLocation(program, name_buffer_ + input->name_offset);
    input->binding = -1;
    input++;
  }

  /* Compatibility uniforms. */
  for (auto &name : workaround_uniform_names) {
    copy_input_name(input, name, name_buffer_, name_buffer_offset);
    input->location = glGetUniformLocation(program, name_buffer_ + input->name_offset);
    input->binding = -1;
    input++;
  }

  /* SSBOs */
  for (const ShaderCreateInfo::Resource &res : all_resources) {
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER) {
      copy_input_name(input, res.storagebuf.name, name_buffer_, name_buffer_offset);
      input->location = input->binding = res.slot;
      enabled_ssbo_mask_ |= (1 << input->binding);
      input++;
    }
  }

  for (const ShaderCreateInfo::Resource &res : info.geometry_resources_) {
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER) {
      ssbo_attr_mask_ |= (1 << res.slot);
    }
    else {
      BLI_assert_msg(0, "Resource type is not supported for Geometry frequency");
    }
  }

  /* Constants */
  int constant_id = 0;
  for (const SpecializationConstant &constant : info.specialization_constants_) {
    copy_input_name(input, constant.name, name_buffer_, name_buffer_offset);
    input->location = constant_id++;
    input++;
  }

  this->sort_inputs();

  /* Resolving builtins must happen after the inputs have been sorted. */
  /* Builtin Uniforms */
  for (int32_t u_int = 0; u_int < GPU_NUM_UNIFORMS; u_int++) {
    GPUUniformBuiltin u = static_cast<GPUUniformBuiltin>(u_int);
    const ShaderInput *uni = this->uniform_get(builtin_uniform_name(u));
    builtins_[u] = (uni != nullptr) ? uni->location : -1;
  }

  /* Builtin Uniforms Blocks */
  for (int32_t u_int = 0; u_int < GPU_NUM_UNIFORM_BLOCKS; u_int++) {
    GPUUniformBlockBuiltin u = static_cast<GPUUniformBlockBuiltin>(u_int);
    const ShaderInput *block = this->ubo_get(builtin_uniform_block_name(u));
    builtin_blocks_[u] = (block != nullptr) ? block->binding : -1;
  }

  // this->debug_print();

  glUseProgram(last_program);
}

GLShaderInterface::~GLShaderInterface()
{
  for (auto *ref : refs_) {
    if (ref != nullptr) {
      ref->remove(this);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Batch Reference
 * \{ */

void GLShaderInterface::ref_add(GLVaoCache *ref)
{
  for (int i = 0; i < refs_.size(); i++) {
    if (refs_[i] == nullptr) {
      refs_[i] = ref;
      return;
    }
  }
  refs_.append(ref);
}

void GLShaderInterface::ref_remove(GLVaoCache *ref)
{
  for (int i = 0; i < refs_.size(); i++) {
    if (refs_[i] == ref) {
      refs_[i] = nullptr;
      break; /* cannot have duplicates */
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Validation
 * TODO
 * \{ */

/** \} */

}  // namespace blender::gpu
