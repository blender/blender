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

#include "MEM_guardedalloc.h"
#include "BKE_global.h"

#include "GPU_shader_interface.h"

#include "gpu_batch_private.h"
#include "gpu_context_private.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define DEBUG_SHADER_INTERFACE 0
#define DEBUG_SHADER_UNIFORMS 0

#if DEBUG_SHADER_INTERFACE
#  include <stdio.h>
#endif

static const char *BuiltinUniform_name(GPUUniformBuiltin u)
{
  static const char *names[] = {
      [GPU_UNIFORM_NONE] = NULL,

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
      [GPU_UNIFORM_CALLID] = "callId",
      [GPU_UNIFORM_OBJECT_INFO] = "unfobjectinfo",

      [GPU_UNIFORM_CUSTOM] = NULL,
      [GPU_NUM_UNIFORMS] = NULL,
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

GPU_INLINE void set_input_name(GPUShaderInterface *shaderface,
                               GPUShaderInput *input,
                               const char *name,
                               uint32_t name_len)
{
  input->name_offset = shaderface->name_buffer_offset;
  input->name_hash = hash_string(name);
  shaderface->name_buffer_offset += name_len + 1; /* include NULL terminator */
}

GPU_INLINE void shader_input_to_bucket(GPUShaderInput *input,
                                       GPUShaderInput *buckets[GPU_NUM_SHADERINTERFACE_BUCKETS])
{
  const uint bucket_index = input->name_hash % GPU_NUM_SHADERINTERFACE_BUCKETS;
  input->next = buckets[bucket_index];
  buckets[bucket_index] = input;
}

GPU_INLINE const GPUShaderInput *buckets_lookup(
    GPUShaderInput *const buckets[GPU_NUM_SHADERINTERFACE_BUCKETS],
    const char *name_buffer,
    const char *name)
{
  const uint name_hash = hash_string(name);
  const uint bucket_index = name_hash % GPU_NUM_SHADERINTERFACE_BUCKETS;
  const GPUShaderInput *input = buckets[bucket_index];
  if (input == NULL) {
    /* Requested uniform is not found at all. */
    return NULL;
  }
  /* Optimization bit: if there is no hash collision detected when constructing shader interface
   * it means we can only request the single possible uniform. Surely, it's possible we request
   * uniform which causes hash collision, but that will be detected in debug builds. */
  if (input->next == NULL) {
    if (name_hash == input->name_hash) {
#if TRUST_NO_ONE
      assert(match(name_buffer + input->name_offset, name));
#endif
      return input;
    }
    return NULL;
  }
  /* Work through possible collisions. */
  const GPUShaderInput *next = input;
  while (next != NULL) {
    input = next;
    next = input->next;
    if (input->name_hash != name_hash) {
      continue;
    }
    if (match(name_buffer + input->name_offset, name)) {
      return input;
    }
  }
  return NULL; /* not found */
}

GPU_INLINE void buckets_free(GPUShaderInput *buckets[GPU_NUM_SHADERINTERFACE_BUCKETS])
{
  for (uint bucket_index = 0; bucket_index < GPU_NUM_SHADERINTERFACE_BUCKETS; ++bucket_index) {
    GPUShaderInput *input = buckets[bucket_index];
    while (input != NULL) {
      GPUShaderInput *input_next = input->next;
      MEM_freeN(input);
      input = input_next;
    }
  }
}

static bool setup_builtin_uniform(GPUShaderInput *input, const char *name)
{
  /* TODO: reject DOUBLE, IMAGE, ATOMIC_COUNTER gl_types */

  /* detect built-in uniforms (name must match) */
  for (GPUUniformBuiltin u = GPU_UNIFORM_NONE + 1; u < GPU_UNIFORM_CUSTOM; ++u) {
    const char *builtin_name = BuiltinUniform_name(u);
    if (match(name, builtin_name)) {
      input->builtin_type = u;
      return true;
    }
  }
  input->builtin_type = GPU_UNIFORM_CUSTOM;
  return false;
}

static const GPUShaderInput *add_uniform(GPUShaderInterface *shaderface, const char *name)
{
  GPUShaderInput *input = MEM_mallocN(sizeof(GPUShaderInput), "GPUShaderInput Unif");

  input->location = glGetUniformLocation(shaderface->program, name);

  const uint name_len = strlen(name);
  /* Include NULL terminator. */
  shaderface->name_buffer = MEM_reallocN(shaderface->name_buffer,
                                         shaderface->name_buffer_offset + name_len + 1);
  char *name_buffer = shaderface->name_buffer + shaderface->name_buffer_offset;
  strcpy(name_buffer, name);

  set_input_name(shaderface, input, name, name_len);
  setup_builtin_uniform(input, name);

  shader_input_to_bucket(input, shaderface->uniform_buckets);
  if (input->builtin_type != GPU_UNIFORM_NONE && input->builtin_type != GPU_UNIFORM_CUSTOM) {
    shaderface->builtin_uniforms[input->builtin_type] = input;
  }
#if DEBUG_SHADER_INTERFACE
  printf("GPUShaderInterface %p, program %d, uniform[] '%s' at location %d\n",
         shaderface,
         shaderface->program,
         name,
         input->location);
#endif
  return input;
}

GPUShaderInterface *GPU_shaderinterface_create(int32_t program)
{
  GPUShaderInterface *shaderface = MEM_callocN(sizeof(GPUShaderInterface), "GPUShaderInterface");
  shaderface->program = program;

#if DEBUG_SHADER_INTERFACE
  printf("%s {\n", __func__); /* enter function */
  printf("GPUShaderInterface %p, program %d\n", shaderface, program);
#endif

  GLint max_attr_name_len = 0, attr_len = 0;
  glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_attr_name_len);
  glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attr_len);

  GLint max_ubo_name_len = 0, ubo_len = 0;
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &max_ubo_name_len);
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &ubo_len);

  /* Work around driver bug with Intel HD 4600 on Windows 7/8, where
   * GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH does not work. */
  if (attr_len > 0 && max_attr_name_len == 0) {
    max_attr_name_len = 256;
  }
  if (ubo_len > 0 && max_ubo_name_len == 0) {
    max_ubo_name_len = 256;
  }

  const uint32_t name_buffer_len = attr_len * max_attr_name_len + ubo_len * max_ubo_name_len;
  shaderface->name_buffer = MEM_mallocN(name_buffer_len, "name_buffer");

  /* Attributes */
  for (uint32_t i = 0; i < attr_len; ++i) {
    GPUShaderInput *input = MEM_mallocN(sizeof(GPUShaderInput), "GPUShaderInput Attr");
    GLsizei remaining_buffer = name_buffer_len - shaderface->name_buffer_offset;
    char *name = shaderface->name_buffer + shaderface->name_buffer_offset;
    GLsizei name_len = 0;

    glGetActiveAttrib(
        program, i, remaining_buffer, &name_len, &input->size, &input->gl_type, name);

    /* remove "[0]" from array name */
    if (name[name_len - 1] == ']') {
      name[name_len - 3] = '\0';
      name_len -= 3;
    }

    /* TODO: reject DOUBLE gl_types */

    input->location = glGetAttribLocation(program, name);

    set_input_name(shaderface, input, name, name_len);

    shader_input_to_bucket(input, shaderface->attr_buckets);

#if DEBUG_SHADER_INTERFACE
    printf("attr[%u] '%s' at location %d\n", i, name, input->location);
#endif
  }
  /* Uniform Blocks */
  for (uint32_t i = 0; i < ubo_len; ++i) {
    GPUShaderInput *input = MEM_mallocN(sizeof(GPUShaderInput), "GPUShaderInput UBO");
    GLsizei remaining_buffer = name_buffer_len - shaderface->name_buffer_offset;
    char *name = shaderface->name_buffer + shaderface->name_buffer_offset;
    GLsizei name_len = 0;

    glGetActiveUniformBlockName(program, i, remaining_buffer, &name_len, name);

    input->location = i;

    set_input_name(shaderface, input, name, name_len);

    shader_input_to_bucket(input, shaderface->ubo_buckets);

#if DEBUG_SHADER_INTERFACE
    printf("ubo '%s' at location %d\n", name, input->location);
#endif
  }
  /* Builtin Uniforms */
  for (GPUUniformBuiltin u = GPU_UNIFORM_NONE + 1; u < GPU_UNIFORM_CUSTOM; ++u) {
    const char *builtin_name = BuiltinUniform_name(u);
    if (glGetUniformLocation(program, builtin_name) != -1) {
      add_uniform((GPUShaderInterface *)shaderface, builtin_name);
    }
  }
  /* Batches ref buffer */
  shaderface->batches_len = GPU_SHADERINTERFACE_REF_ALLOC_COUNT;
  shaderface->batches = MEM_callocN(shaderface->batches_len * sizeof(GPUBatch *),
                                    "GPUShaderInterface batches");

  return shaderface;
}

void GPU_shaderinterface_discard(GPUShaderInterface *shaderface)
{
  /* Free memory used by buckets and has entries. */
  buckets_free(shaderface->uniform_buckets);
  buckets_free(shaderface->attr_buckets);
  buckets_free(shaderface->ubo_buckets);
  /* Free memory used by name_buffer. */
  MEM_freeN(shaderface->name_buffer);
  /* Remove this interface from all linked Batches vao cache. */
  for (int i = 0; i < shaderface->batches_len; ++i) {
    if (shaderface->batches[i] != NULL) {
      gpu_batch_remove_interface_ref(shaderface->batches[i], shaderface);
    }
  }
  MEM_freeN(shaderface->batches);
  /* Free memory used by shader interface by its self. */
  MEM_freeN(shaderface);
}

const GPUShaderInput *GPU_shaderinterface_uniform(const GPUShaderInterface *shaderface,
                                                  const char *name)
{
  return buckets_lookup(shaderface->uniform_buckets, shaderface->name_buffer, name);
}

const GPUShaderInput *GPU_shaderinterface_uniform_ensure(const GPUShaderInterface *shaderface,
                                                         const char *name)
{
  const GPUShaderInput *input = GPU_shaderinterface_uniform(shaderface, name);
  /* If input is not found add it so it's found next time. */
  if (input == NULL) {
    input = add_uniform((GPUShaderInterface *)shaderface, name);

    if ((G.debug & G_DEBUG_GPU) && (input->location == -1)) {
      fprintf(stderr, "GPUShaderInterface: Warning: Uniform '%s' not found!\n", name);
    }
  }

#if DEBUG_SHADER_UNIFORMS
  if ((G.debug & G_DEBUG_GPU) && input->builtin_type != GPU_UNIFORM_NONE &&
      input->builtin_type != GPU_UNIFORM_CUSTOM) {
    /* Warn if we find a matching builtin, since these can be looked up much quicker. */
    fprintf(stderr,
            "GPUShaderInterface: Warning: Uniform '%s' is a builtin uniform but not queried as "
            "such!\n",
            name);
  }
#endif
  return (input->location != -1) ? input : NULL;
}

const GPUShaderInput *GPU_shaderinterface_uniform_builtin(const GPUShaderInterface *shaderface,
                                                          GPUUniformBuiltin builtin)
{
#if TRUST_NO_ONE
  assert(builtin != GPU_UNIFORM_NONE);
  assert(builtin != GPU_UNIFORM_CUSTOM);
  assert(builtin != GPU_NUM_UNIFORMS);
#endif
  return shaderface->builtin_uniforms[builtin];
}

const GPUShaderInput *GPU_shaderinterface_ubo(const GPUShaderInterface *shaderface,
                                              const char *name)
{
  return buckets_lookup(shaderface->ubo_buckets, shaderface->name_buffer, name);
}

const GPUShaderInput *GPU_shaderinterface_attr(const GPUShaderInterface *shaderface,
                                               const char *name)
{
  return buckets_lookup(shaderface->attr_buckets, shaderface->name_buffer, name);
}

void GPU_shaderinterface_add_batch_ref(GPUShaderInterface *shaderface, GPUBatch *batch)
{
  int i; /* find first unused slot */
  for (i = 0; i < shaderface->batches_len; ++i) {
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
  for (int i = 0; i < shaderface->batches_len; ++i) {
    if (shaderface->batches[i] == batch) {
      shaderface->batches[i] = NULL;
      break; /* cannot have duplicates */
    }
  }
}
