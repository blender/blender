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

#include "GPU_capabilities.h"

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

static Type gpu_type_from_gl_type(int gl_type)
{
  switch (gl_type) {
    case GL_FLOAT:
      return Type::FLOAT;
    case GL_FLOAT_VEC2:
      return Type::VEC2;
    case GL_FLOAT_VEC3:
      return Type::VEC3;
    case GL_FLOAT_VEC4:
      return Type::VEC4;
    case GL_FLOAT_MAT3:
      return Type::MAT3;
    case GL_FLOAT_MAT4:
      return Type::MAT4;
    case GL_UNSIGNED_INT:
      return Type::UINT;
    case GL_UNSIGNED_INT_VEC2:
      return Type::UVEC2;
    case GL_UNSIGNED_INT_VEC3:
      return Type::UVEC3;
    case GL_UNSIGNED_INT_VEC4:
      return Type::UVEC4;
    case GL_INT:
      return Type::INT;
    case GL_INT_VEC2:
      return Type::IVEC2;
    case GL_INT_VEC3:
      return Type::IVEC3;
    case GL_INT_VEC4:
      return Type::IVEC4;
    case GL_BOOL:
      return Type::BOOL;
    case GL_FLOAT_MAT2:
    case GL_FLOAT_MAT2x3:
    case GL_FLOAT_MAT2x4:
    case GL_FLOAT_MAT3x2:
    case GL_FLOAT_MAT3x4:
    case GL_FLOAT_MAT4x2:
    case GL_FLOAT_MAT4x3:
    default:
      BLI_assert(0);
  }
  return Type::FLOAT;
}

GLShaderInterface::GLShaderInterface(GLuint program)
{
  GLuint last_program;
  glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *)&last_program);

  /* Necessary to make #glUniform works. */
  glUseProgram(program);

  GLint max_attr_name_len = 0, attr_len = 0;
  glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_attr_name_len);
  glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attr_len);

  GLint max_ubo_name_len = 0, ubo_len = 0;
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &max_ubo_name_len);
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &ubo_len);

  GLint max_uniform_name_len = 0, active_uniform_len = 0, uniform_len = 0;
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_uniform_name_len);
  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &active_uniform_len);
  uniform_len = active_uniform_len;

  GLint max_ssbo_name_len = 0, ssbo_len = 0;
  glGetProgramInterfaceiv(program, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &ssbo_len);
  glGetProgramInterfaceiv(
      program, GL_SHADER_STORAGE_BLOCK, GL_MAX_NAME_LENGTH, &max_ssbo_name_len);

  BLI_assert_msg(ubo_len <= 16, "enabled_ubo_mask_ is uint16_t");

  /* Work around driver bug with Intel HD 4600 on Windows 7/8, where
   * GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH does not work. */
  if (attr_len > 0 && max_attr_name_len == 0) {
    max_attr_name_len = 256;
  }
  if (ubo_len > 0 && max_ubo_name_len == 0) {
    max_ubo_name_len = 256;
  }
  if (uniform_len > 0 && max_uniform_name_len == 0) {
    max_uniform_name_len = 256;
  }
  if (ssbo_len > 0 && max_ssbo_name_len == 0) {
    max_ssbo_name_len = 256;
  }

  /* GL_ACTIVE_UNIFORMS lied to us! Remove the UBO uniforms from the total before
   * allocating the uniform array. */
  GLint max_ubo_uni_len = 0;
  for (int i = 0; i < ubo_len; i++) {
    GLint ubo_uni_len;
    glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &ubo_uni_len);
    max_ubo_uni_len = max_ii(max_ubo_uni_len, ubo_uni_len);
    uniform_len -= ubo_uni_len;
  }
  /* Bit set to true if uniform comes from a uniform block. */
  BLI_bitmap *uniforms_from_blocks = BLI_BITMAP_NEW(active_uniform_len, __func__);
  /* Set uniforms from block for exclusion. */
  GLint *ubo_uni_ids = (GLint *)MEM_mallocN(sizeof(GLint) * max_ubo_uni_len, __func__);
  for (int i = 0; i < ubo_len; i++) {
    GLint ubo_uni_len;
    glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &ubo_uni_len);
    glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, ubo_uni_ids);
    for (int u = 0; u < ubo_uni_len; u++) {
      BLI_BITMAP_ENABLE(uniforms_from_blocks, ubo_uni_ids[u]);
    }
  }
  MEM_freeN(ubo_uni_ids);

  int input_tot_len = attr_len + ubo_len + uniform_len + ssbo_len;
  inputs_ = (ShaderInput *)MEM_callocN(sizeof(ShaderInput) * input_tot_len, __func__);

  const uint32_t name_buffer_len = attr_len * max_attr_name_len + ubo_len * max_ubo_name_len +
                                   uniform_len * max_uniform_name_len +
                                   ssbo_len * max_ssbo_name_len;
  name_buffer_ = (char *)MEM_mallocN(name_buffer_len, "name_buffer");
  uint32_t name_buffer_offset = 0;

  /* Attributes */
  enabled_attr_mask_ = 0;
  for (int i = 0; i < attr_len; i++) {
    char *name = name_buffer_ + name_buffer_offset;
    GLsizei remaining_buffer = name_buffer_len - name_buffer_offset;
    GLsizei name_len = 0;
    GLenum type;
    GLint size;

    glGetActiveAttrib(program, i, remaining_buffer, &name_len, &size, &type, name);
    GLint location = glGetAttribLocation(program, name);
    /* Ignore OpenGL names like `gl_BaseInstanceARB`, `gl_InstanceID` and `gl_VertexID`. */
    if (location == -1) {
      continue;
    }

    ShaderInput *input = &inputs_[attr_len_++];
    input->location = input->binding = location;

    name_buffer_offset += set_input_name(input, name, name_len);
    enabled_attr_mask_ |= (1 << input->location);

    /* Used in `GPU_shader_get_attribute_info`. */
    attr_types_[input->location] = uint8_t(gpu_type_from_gl_type(type));
  }

  /* Uniform Blocks */
  for (int i = 0; i < ubo_len; i++) {
    char *name = name_buffer_ + name_buffer_offset;
    GLsizei remaining_buffer = name_buffer_len - name_buffer_offset;
    GLsizei name_len = 0;

    glGetActiveUniformBlockName(program, i, remaining_buffer, &name_len, name);

    ShaderInput *input = &inputs_[attr_len_ + ubo_len_++];
    input->binding = input->location = block_binding(program, i);

    name_buffer_offset += this->set_input_name(input, name, name_len);
    enabled_ubo_mask_ |= (1 << input->binding);
  }

  /* Uniforms & samplers & images */
  for (int i = 0, sampler = 0, image = 0; i < active_uniform_len; i++) {
    if (BLI_BITMAP_TEST(uniforms_from_blocks, i)) {
      continue;
    }
    char *name = name_buffer_ + name_buffer_offset;
    GLsizei remaining_buffer = name_buffer_len - name_buffer_offset;
    GLsizei name_len = 0;

    glGetActiveUniformName(program, i, remaining_buffer, &name_len, name);

    ShaderInput *input = &inputs_[attr_len_ + ubo_len_ + uniform_len_++];
    input->location = glGetUniformLocation(program, name);
    input->binding = sampler_binding(program, i, input->location, &sampler);

    name_buffer_offset += this->set_input_name(input, name, name_len);
    enabled_tex_mask_ |= (input->binding != -1) ? (1lu << input->binding) : 0lu;

    if (input->binding == -1) {
      input->binding = image_binding(program, i, input->location, &image);

      enabled_ima_mask_ |= (input->binding != -1) ? (1lu << input->binding) : 0lu;
    }
  }

  /* SSBOs */
  for (int i = 0; i < ssbo_len; i++) {
    char *name = name_buffer_ + name_buffer_offset;
    GLsizei remaining_buffer = name_buffer_len - name_buffer_offset;
    GLsizei name_len = 0;
    glGetProgramResourceName(
        program, GL_SHADER_STORAGE_BLOCK, i, remaining_buffer, &name_len, name);

    const GLint binding = ssbo_binding(program, i);

    ShaderInput *input = &inputs_[attr_len_ + ubo_len_ + uniform_len_ + ssbo_len_++];
    input->binding = input->location = binding;

    name_buffer_offset += this->set_input_name(input, name, name_len);
    enabled_ssbo_mask_ |= (input->binding != -1) ? (1lu << input->binding) : 0lu;
  }

  /* Builtin Uniforms */
  for (int32_t u_int = 0; u_int < GPU_NUM_UNIFORMS; u_int++) {
    GPUUniformBuiltin u = static_cast<GPUUniformBuiltin>(u_int);
    builtins_[u] = glGetUniformLocation(program, builtin_uniform_name(u));
  }

  /* Builtin Uniforms Blocks */
  for (int32_t u_int = 0; u_int < GPU_NUM_UNIFORM_BLOCKS; u_int++) {
    GPUUniformBlockBuiltin u = static_cast<GPUUniformBlockBuiltin>(u_int);
    const ShaderInput *block = this->ubo_get(builtin_uniform_block_name(u));
    builtin_blocks_[u] = (block != nullptr) ? block->binding : -1;
  }

  MEM_freeN(uniforms_from_blocks);

  /* Resize name buffer to save some memory. */
  if (name_buffer_offset < name_buffer_len) {
    name_buffer_ = (char *)MEM_reallocN(name_buffer_, name_buffer_offset);
  }

  // this->debug_print();

  this->sort_inputs();

  glUseProgram(last_program);
}

GLShaderInterface::GLShaderInterface(GLuint program, const shader::ShaderCreateInfo &info)
{
  using namespace blender::gpu::shader;

  attr_len_ = info.vertex_inputs_.size();
  uniform_len_ = info.push_constants_.size();
  ubo_len_ = 0;
  ssbo_len_ = 0;

  Vector<ShaderCreateInfo::Resource> all_resources;
  all_resources.extend(info.pass_resources_);
  all_resources.extend(info.batch_resources_);

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

  int input_tot_len = attr_len_ + ubo_len_ + uniform_len_ + ssbo_len_;
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
