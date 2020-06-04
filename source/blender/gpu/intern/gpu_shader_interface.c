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

#include "BKE_global.h"

#include "BLI_math_base.h"

#include "MEM_guardedalloc.h"

#include "GPU_shader_interface.h"

#include "gpu_batch_private.h"
#include "gpu_context_private.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_SHADER_INTERFACE 0
#define DEBUG_SHADER_UNIFORMS 0

#if DEBUG_SHADER_INTERFACE
#  include <stdio.h>
#endif

static const char *BuiltinUniform_name(GPUUniformBuiltin u)
{
  static const char *names[] = {
      [GPU_UNIFORM_MODEL] = "ModelMatrix",
      [GPU_UNIFORM_VIEW] = "ViewMatrix",
      [GPU_UNIFORM_MODELVIEW] = "ModelViewMatrix",
      [GPU_UNIFORM_PROJECTION] = "ProjectionMatrix",
      [GPU_UNIFORM_VIEWPROJECTION] = "ViewProjectionMatrix",
      [GPU_UNIFORM_MVP] = "ModelViewProjectionMatrix",

      [GPU_UNIFORM_MODEL_INV] = "ModelMatrixInverse",
      [GPU_UNIFORM_VIEW_INV] = "ViewMatrixInverse",
      [GPU_UNIFORM_MODELVIEW_INV] = "ModelViewMatrixInverse",
      [GPU_UNIFORM_PROJECTION_INV] = "ProjectionMatrixInverse",
      [GPU_UNIFORM_VIEWPROJECTION_INV] = "ViewProjectionMatrixInverse",

      [GPU_UNIFORM_NORMAL] = "NormalMatrix",
      [GPU_UNIFORM_ORCO] = "OrcoTexCoFactors",
      [GPU_UNIFORM_CLIPPLANES] = "WorldClipPlanes",

      [GPU_UNIFORM_COLOR] = "color",
      [GPU_UNIFORM_BASE_INSTANCE] = "baseInstance",
      [GPU_UNIFORM_RESOURCE_CHUNK] = "resourceChunk",
      [GPU_UNIFORM_RESOURCE_ID] = "resourceId",
      [GPU_UNIFORM_SRGB_TRANSFORM] = "srgbTarget",

      [GPU_NUM_UNIFORMS] = NULL,
  };

  return names[u];
}

static const char *BuiltinUniformBlock_name(GPUUniformBlockBuiltin u)
{
  static const char *names[] = {
      [GPU_UNIFORM_BLOCK_VIEW] = "viewBlock",
      [GPU_UNIFORM_BLOCK_MODEL] = "modelBlock",
      [GPU_UNIFORM_BLOCK_INFO] = "infoBlock",

      [GPU_NUM_UNIFORM_BLOCKS] = NULL,
  };

  return names[u];
}

GPU_INLINE bool match(const char *a, const char *b)
{
  return strcmp(a, b) == 0;
}

GPU_INLINE uint hash_string(const char *str)
{
  uint i = 0, c;
  while ((c = *str++)) {
    i = i * 37 + c;
  }
  return i;
}

GPU_INLINE uint32_t set_input_name(GPUShaderInterface *shaderface,
                                   GPUShaderInput *input,
                                   char *name,
                                   uint32_t name_len)
{
  /* remove "[0]" from array name */
  if (name[name_len - 1] == ']') {
    name[name_len - 3] = '\0';
    name_len -= 3;
  }

  input->name_offset = (uint32_t)(name - shaderface->name_buffer);
  input->name_hash = hash_string(name);
  return name_len + 1; /* include NULL terminator */
}

GPU_INLINE const GPUShaderInput *input_lookup(const GPUShaderInterface *shaderface,
                                              const GPUShaderInput *const inputs,
                                              const uint inputs_len,
                                              const char *name)
{
  const uint name_hash = hash_string(name);
  /* Simple linear search for now. */
  for (int i = inputs_len - 1; i >= 0; i--) {
    if (inputs[i].name_hash == name_hash) {
      if ((i > 0) && UNLIKELY(inputs[i - 1].name_hash == name_hash)) {
        /* Hash colision resolve. */
        for (; i >= 0 && inputs[i].name_hash == name_hash; i--) {
          if (match(name, shaderface->name_buffer + inputs[i].name_offset)) {
            return inputs + i; /* not found */
          }
        }
        return NULL; /* not found */
      }
      else {
        /* This is a bit dangerous since we could have a hash collision.
         * where the asked uniform that does not exist has the same hash
         * as a real uniform. */
        BLI_assert(match(name, shaderface->name_buffer + inputs[i].name_offset));
        return inputs + i;
      }
    }
  }
  return NULL; /* not found */
}

/* Note that this modify the src array. */
GPU_INLINE void sort_input_list(GPUShaderInput *dst, GPUShaderInput *src, const uint input_len)
{
  for (uint i = 0; i < input_len; i++) {
    GPUShaderInput *input_src = &src[0];
    for (uint j = 1; j < input_len; j++) {
      if (src[j].name_hash > input_src->name_hash) {
        input_src = &src[j];
      }
    }
    dst[i] = *input_src;
    input_src->name_hash = 0;
  }
}

static int block_binding(int32_t program, uint32_t block_index)
{
  /* For now just assign a consecutive index. In the future, we should set it in
   * the shader using layout(binding = i) and query its value. */
  glUniformBlockBinding(program, block_index, block_index);
  return block_index;
}

static int sampler_binding(int32_t program,
                           uint32_t uniform_index,
                           int32_t uniform_location,
                           int *sampler_len)
{
  /* Identify sampler uniforms and asign sampler units to them. */
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

GPUShaderInterface *GPU_shaderinterface_create(int32_t program)
{
#ifndef NDEBUG
  GLint curr_program;
  glGetIntegerv(GL_CURRENT_PROGRAM, &curr_program);
  BLI_assert(curr_program == program);
#endif

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
  GLint *uniforms_block_index = MEM_mallocN(sizeof(GLint) * active_uniform_len, __func__);
  if (uniform_len > 0) {
    GLuint *indices = MEM_mallocN(sizeof(GLuint) * active_uniform_len, __func__);
    for (uint i = 0; i < uniform_len; i++) {
      indices[i] = i;
    }

    glGetActiveUniformsiv(
        program, uniform_len, indices, GL_UNIFORM_BLOCK_INDEX, uniforms_block_index);

    MEM_freeN(indices);

    for (int i = 0; i < active_uniform_len; i++) {
      /* If GL_UNIFORM_BLOCK_INDEX is not -1 it means the uniform belongs to a UBO. */
      if (uniforms_block_index[i] != -1) {
        uniform_len--;
      }
    }
  }

  uint32_t name_buffer_offset = 0;
  const uint32_t name_buffer_len = attr_len * max_attr_name_len + ubo_len * max_ubo_name_len +
                                   uniform_len * max_uniform_name_len;

  int input_tot_len = attr_len + ubo_len + uniform_len;
  size_t interface_size = sizeof(GPUShaderInterface) + sizeof(GPUShaderInput) * input_tot_len;

  GPUShaderInterface *shaderface = MEM_callocN(interface_size, "GPUShaderInterface");
  shaderface->attribute_len = attr_len;
  shaderface->ubo_len = ubo_len;
  shaderface->uniform_len = uniform_len;
  shaderface->name_buffer = MEM_mallocN(name_buffer_len, "name_buffer");
  GPUShaderInput *inputs = shaderface->inputs;

  /* Temp buffer. */
  int input_tmp_len = max_iii(attr_len, ubo_len, uniform_len);
  GPUShaderInput *inputs_tmp = MEM_mallocN(sizeof(GPUShaderInput) * input_tmp_len, "name_buffer");

  /* Attributes */
  shaderface->enabled_attr_mask = 0;
  for (int i = 0, idx = 0; i < attr_len; i++) {
    char *name = shaderface->name_buffer + name_buffer_offset;
    GLsizei remaining_buffer = name_buffer_len - name_buffer_offset;
    GLsizei name_len = 0;
    GLenum type;
    GLint size;

    glGetActiveAttrib(program, i, remaining_buffer, &name_len, &size, &type, name);
    GLint location = glGetAttribLocation(program, name);
    /* Ignore OpenGL names like `gl_BaseInstanceARB`, `gl_InstanceID` and `gl_VertexID`. */
    if (location == -1) {
      shaderface->attribute_len--;
      continue;
    }

    GPUShaderInput *input = &inputs_tmp[idx++];
    input->location = input->binding = location;

    name_buffer_offset += set_input_name(shaderface, input, name, name_len);
    shaderface->enabled_attr_mask |= (1 << input->location);
  }
  sort_input_list(inputs, inputs_tmp, shaderface->attribute_len);
  inputs += shaderface->attribute_len;

  /* Uniform Blocks */
  for (int i = 0, idx = 0; i < ubo_len; i++) {
    char *name = shaderface->name_buffer + name_buffer_offset;
    GLsizei remaining_buffer = name_buffer_len - name_buffer_offset;
    GLsizei name_len = 0;

    glGetActiveUniformBlockName(program, i, remaining_buffer, &name_len, name);

    GPUShaderInput *input = &inputs_tmp[idx++];
    input->binding = input->location = block_binding(program, i);

    name_buffer_offset += set_input_name(shaderface, input, name, name_len);
    shaderface->enabled_ubo_mask |= (1 << input->binding);
  }
  sort_input_list(inputs, inputs_tmp, shaderface->ubo_len);
  inputs += shaderface->ubo_len;

  /* Uniforms */
  for (int i = 0, idx = 0, sampler = 0; i < active_uniform_len; i++) {
    /* If GL_UNIFORM_BLOCK_INDEX is not -1 it means the uniform belongs to a UBO. */
    if (uniforms_block_index[i] != -1) {
      continue;
    }
    char *name = shaderface->name_buffer + name_buffer_offset;
    GLsizei remaining_buffer = name_buffer_len - name_buffer_offset;
    GLsizei name_len = 0;

    glGetActiveUniformName(program, i, remaining_buffer, &name_len, name);

    GPUShaderInput *input = &inputs_tmp[idx++];
    input->location = glGetUniformLocation(program, name);
    input->binding = sampler_binding(program, i, input->location, &sampler);

    name_buffer_offset += set_input_name(shaderface, input, name, name_len);
    shaderface->enabled_tex_mask |= (input->binding != -1) ? (1lu << input->binding) : 0lu;
  }
  sort_input_list(inputs, inputs_tmp, shaderface->uniform_len);

  /* Builtin Uniforms */
  for (GPUUniformBuiltin u = 0; u < GPU_NUM_UNIFORMS; u++) {
    shaderface->builtins[u].location = glGetUniformLocation(program, BuiltinUniform_name(u));
    shaderface->builtins[u].binding = -1;
  }

  /* Builtin Uniforms Blocks */
  for (GPUUniformBlockBuiltin u = 0; u < GPU_NUM_UNIFORM_BLOCKS; u++) {
    const GPUShaderInput *block = GPU_shaderinterface_ubo(shaderface, BuiltinUniformBlock_name(u));
    shaderface->builtin_blocks[u].location = -1;
    shaderface->builtin_blocks[u].binding = (block != NULL) ? block->binding : -1;
  }

  /* Batches ref buffer */
  shaderface->batches_len = GPU_SHADERINTERFACE_REF_ALLOC_COUNT;
  shaderface->batches = MEM_callocN(shaderface->batches_len * sizeof(GPUBatch *),
                                    "GPUShaderInterface batches");

  MEM_freeN(uniforms_block_index);
  MEM_freeN(inputs_tmp);

  /* Resize name buffer to save some memory. */
  if (name_buffer_offset < name_buffer_len) {
    shaderface->name_buffer = MEM_reallocN(shaderface->name_buffer, name_buffer_offset);
  }

#if DEBUG_SHADER_INTERFACE
  char *name_buf = shaderface->name_buffer;
  printf("--- GPUShaderInterface %p, program %d ---\n", shaderface, program);
  if (shaderface->attribute_len > 0) {
    printf("Attributes {\n");
    for (int i = 0; i < shaderface->attribute_len; i++) {
      GPUShaderInput *input = shaderface->inputs + i;
      printf("\t(location = %d) %s;\n", input->location, name_buf + input->name_offset);
    }
    printf("};\n");
  }
  if (shaderface->ubo_len > 0) {
    printf("Uniform Buffer Objects {\n");
    for (int i = 0; i < shaderface->ubo_len; i++) {
      GPUShaderInput *input = shaderface->inputs + shaderface->attribute_len + i;
      printf("\t(binding = %d) %s;\n", input->binding, name_buf + input->name_offset);
    }
    printf("};\n");
  }
  if (shaderface->enabled_tex_mask > 0) {
    printf("Samplers {\n");
    for (int i = 0; i < shaderface->uniform_len; i++) {
      GPUShaderInput *input = shaderface->inputs + shaderface->attribute_len +
                              shaderface->ubo_len + i;
      if (input->binding != -1) {
        printf("\t(location = %d, binding = %d) %s;\n",
               input->location,
               input->binding,
               name_buf + input->name_offset);
      }
    }
    printf("};\n");
  }
  if (shaderface->uniform_len > 0) {
    printf("Uniforms {\n");
    for (int i = 0; i < shaderface->uniform_len; i++) {
      GPUShaderInput *input = shaderface->inputs + shaderface->attribute_len +
                              shaderface->ubo_len + i;
      if (input->binding == -1) {
        printf("\t(location = %d) %s;\n", input->location, name_buf + input->name_offset);
      }
    }
    printf("};\n");
  }
  printf("--- GPUShaderInterface end ---\n\n");
#endif

  return shaderface;
}

void GPU_shaderinterface_discard(GPUShaderInterface *shaderface)
{
  /* Free memory used by name_buffer. */
  MEM_freeN(shaderface->name_buffer);
  /* Remove this interface from all linked Batches vao cache. */
  for (int i = 0; i < shaderface->batches_len; i++) {
    if (shaderface->batches[i] != NULL) {
      gpu_batch_remove_interface_ref(shaderface->batches[i], shaderface);
    }
  }
  MEM_freeN(shaderface->batches);
  /* Free memory used by shader interface by its self. */
  MEM_freeN(shaderface);
}

const GPUShaderInput *GPU_shaderinterface_attr(const GPUShaderInterface *shaderface,
                                               const char *name)
{
  uint ofs = 0;
  return input_lookup(shaderface, shaderface->inputs + ofs, shaderface->attribute_len, name);
}

const GPUShaderInput *GPU_shaderinterface_ubo(const GPUShaderInterface *shaderface,
                                              const char *name)
{
  uint ofs = shaderface->attribute_len;
  return input_lookup(shaderface, shaderface->inputs + ofs, shaderface->ubo_len, name);
}

const GPUShaderInput *GPU_shaderinterface_uniform(const GPUShaderInterface *shaderface,
                                                  const char *name)
{
  uint ofs = shaderface->attribute_len + shaderface->ubo_len;
  return input_lookup(shaderface, shaderface->inputs + ofs, shaderface->uniform_len, name);
}

const GPUShaderInput *GPU_shaderinterface_uniform_builtin(const GPUShaderInterface *shaderface,
                                                          GPUUniformBuiltin builtin)
{
  BLI_assert(builtin >= 0 && builtin < GPU_NUM_UNIFORMS);
  return &shaderface->builtins[builtin];
}

const GPUShaderInput *GPU_shaderinterface_block_builtin(const GPUShaderInterface *shaderface,
                                                        GPUUniformBlockBuiltin builtin)
{
  BLI_assert(builtin >= 0 && builtin < GPU_NUM_UNIFORM_BLOCKS);
  return &shaderface->builtin_blocks[builtin];
}

void GPU_shaderinterface_add_batch_ref(GPUShaderInterface *shaderface, GPUBatch *batch)
{
  int i; /* find first unused slot */
  for (i = 0; i < shaderface->batches_len; i++) {
    if (shaderface->batches[i] == NULL) {
      break;
    }
  }
  if (i == shaderface->batches_len) {
    /* Not enough place, realloc the array. */
    i = shaderface->batches_len;
    shaderface->batches_len += GPU_SHADERINTERFACE_REF_ALLOC_COUNT;
    shaderface->batches = MEM_recallocN(shaderface->batches,
                                        sizeof(GPUBatch *) * shaderface->batches_len);
  }
  shaderface->batches[i] = batch;
}

void GPU_shaderinterface_remove_batch_ref(GPUShaderInterface *shaderface, GPUBatch *batch)
{
  for (int i = 0; i < shaderface->batches_len; i++) {
    if (shaderface->batches[i] == batch) {
      shaderface->batches[i] = NULL;
      break; /* cannot have duplicates */
    }
  }
}
