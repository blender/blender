/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * GPU shader interface (C --> GLSL)
 */

#include "BLI_bitmap.h"

#include "gl_batch.hh"

#include "gl_shader_interface.hh"

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
    case GL_IMAGE_3D: {
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
/** \} */

/* -------------------------------------------------------------------- */
/** \name Creation / Destruction
 * \{ */

GLShaderInterface::GLShaderInterface(GLuint program)
{
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

  BLI_assert(ubo_len <= 16 && "enabled_ubo_mask_ is uint16_t");

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

  int input_tot_len = attr_len + ubo_len + uniform_len;
  inputs_ = (ShaderInput *)MEM_callocN(sizeof(ShaderInput) * input_tot_len, __func__);

  const uint32_t name_buffer_len = attr_len * max_attr_name_len + ubo_len * max_ubo_name_len +
                                   uniform_len * max_uniform_name_len;
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
    if (refs_[i] == NULL) {
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
      refs_[i] = NULL;
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
