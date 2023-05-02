/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "MEM_guardedalloc.h"
#include <cstring>

#include "BLI_blenlib.h"
#include "BLI_math_base.h"

#include "gpu_backend.hh"
#include "gpu_node_graph.h"

#include "GPU_context.h"
#include "GPU_material.h"

#include "GPU_uniform_buffer.h"
#include "gpu_uniform_buffer_private.hh"

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

namespace blender::gpu {

UniformBuf::UniformBuf(size_t size, const char *name)
{
  /* Make sure that UBO is padded to size of vec4 */
  BLI_assert((size % 16) == 0);

  size_in_bytes_ = size;

  BLI_strncpy(name_, name, sizeof(name_));
}

UniformBuf::~UniformBuf()
{
  MEM_SAFE_FREE(data_);
}

}  // namespace blender::gpu

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform buffer from GPUInput list
 * \{ */

/**
 * We need to pad some data types (vec3) on the C side
 * To match the GPU expected memory block alignment.
 */
static eGPUType get_padded_gpu_type(LinkData *link)
{
  GPUInput *input = (GPUInput *)link->data;
  eGPUType gputype = input->type;
  /* Metal cannot pack floats after vec3. */
  if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
    return (gputype == GPU_VEC3) ? GPU_VEC4 : gputype;
  }
  /* Unless the vec3 is followed by a float we need to treat it as a vec4. */
  if (gputype == GPU_VEC3 && (link->next != nullptr) &&
      (((GPUInput *)link->next->data)->type != GPU_FLOAT))
  {
    gputype = GPU_VEC4;
  }
  return gputype;
}

/**
 * Returns 1 if the first item should be after second item.
 * We make sure the vec4 uniforms come first.
 */
static int inputs_cmp(const void *a, const void *b)
{
  const LinkData *link_a = (const LinkData *)a, *link_b = (const LinkData *)b;
  const GPUInput *input_a = (const GPUInput *)link_a->data;
  const GPUInput *input_b = (const GPUInput *)link_b->data;
  return input_a->type < input_b->type ? 1 : 0;
}

/**
 * Make sure we respect the expected alignment of UBOs.
 * mat4, vec4, pad vec3 as vec4, then vec2, then floats.
 */
static void buffer_from_list_inputs_sort(ListBase *inputs)
{
/* Only support up to this type, if you want to extend it, make sure static void
 * inputs_sobuffer_size_compute *inputs) padding logic is correct for the new types. */
#define MAX_UBO_GPU_TYPE GPU_MAT4

  /* Order them as mat4, vec4, vec3, vec2, float. */
  BLI_listbase_sort(inputs, inputs_cmp);

  /* Metal cannot pack floats after vec3. */
  if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
    return;
  }

  /* Creates a lookup table for the different types; */
  LinkData *inputs_lookup[MAX_UBO_GPU_TYPE + 1] = {nullptr};
  eGPUType cur_type = static_cast<eGPUType>(MAX_UBO_GPU_TYPE + 1);

  LISTBASE_FOREACH (LinkData *, link, inputs) {
    GPUInput *input = (GPUInput *)link->data;

    if (input->type == GPU_MAT3) {
      /* Alignment for mat3 is not handled currently, so not supported */
      BLI_assert_msg(0, "mat3 not supported in UBO");
      continue;
    }
    if (input->type > MAX_UBO_GPU_TYPE) {
      BLI_assert_msg(0, "GPU type not supported in UBO");
      continue;
    }

    if (input->type == cur_type) {
      continue;
    }

    inputs_lookup[input->type] = link;
    cur_type = input->type;
  }

  /* If there is no GPU_VEC3 there is no need for alignment. */
  if (inputs_lookup[GPU_VEC3] == nullptr) {
    return;
  }

  LinkData *link = inputs_lookup[GPU_VEC3];
  while (link != nullptr && ((GPUInput *)link->data)->type == GPU_VEC3) {
    LinkData *link_next = link->next;

    /* If GPU_VEC3 is followed by nothing or a GPU_FLOAT, no need for alignment. */
    if ((link_next == nullptr) || ((GPUInput *)link_next->data)->type == GPU_FLOAT) {
      break;
    }

    /* If there is a float, move it next to current vec3. */
    if (inputs_lookup[GPU_FLOAT] != nullptr) {
      LinkData *float_input = inputs_lookup[GPU_FLOAT];
      inputs_lookup[GPU_FLOAT] = float_input->next;

      BLI_remlink(inputs, float_input);
      BLI_insertlinkafter(inputs, link, float_input);
    }

    link = link_next;
  }
#undef MAX_UBO_GPU_TYPE
}

static inline size_t buffer_size_from_list(ListBase *inputs)
{
  size_t buffer_size = 0;
  LISTBASE_FOREACH (LinkData *, link, inputs) {
    const eGPUType gputype = get_padded_gpu_type(link);
    buffer_size += gputype * sizeof(float);
  }
  /* Round up to size of vec4. (Opengl Requirement) */
  size_t alignment = sizeof(float[4]);
  buffer_size = divide_ceil_u(buffer_size, alignment) * alignment;

  return buffer_size;
}

static inline void buffer_fill_from_list(void *data, ListBase *inputs)
{
  /* Now that we know the total ubo size we can start populating it. */
  float *offset = (float *)data;
  LISTBASE_FOREACH (LinkData *, link, inputs) {
    GPUInput *input = (GPUInput *)link->data;
    memcpy(offset, input->vec, input->type * sizeof(float));
    offset += get_padded_gpu_type(link);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name C-API
 * \{ */

using namespace blender::gpu;

GPUUniformBuf *GPU_uniformbuf_create_ex(size_t size, const void *data, const char *name)
{
  UniformBuf *ubo = GPUBackend::get()->uniformbuf_alloc(size, name);
  /* Direct init. */
  if (data != nullptr) {
    ubo->update(data);
  }
  return wrap(ubo);
}

GPUUniformBuf *GPU_uniformbuf_create_from_list(ListBase *inputs, const char *name)
{
  /* There is no point on creating an UBO if there is no arguments. */
  if (BLI_listbase_is_empty(inputs)) {
    return nullptr;
  }

  buffer_from_list_inputs_sort(inputs);
  size_t buffer_size = buffer_size_from_list(inputs);
  void *data = MEM_mallocN(buffer_size, __func__);
  buffer_fill_from_list(data, inputs);

  UniformBuf *ubo = GPUBackend::get()->uniformbuf_alloc(buffer_size, name);
  /* Defer data upload. */
  ubo->attach_data(data);
  return wrap(ubo);
}

void GPU_uniformbuf_free(GPUUniformBuf *ubo)
{
  delete unwrap(ubo);
}

void GPU_uniformbuf_update(GPUUniformBuf *ubo, const void *data)
{
  unwrap(ubo)->update(data);
}

void GPU_uniformbuf_bind(GPUUniformBuf *ubo, int slot)
{
  unwrap(ubo)->bind(slot);
}

void GPU_uniformbuf_bind_as_ssbo(GPUUniformBuf *ubo, int slot)
{
  unwrap(ubo)->bind_as_ssbo(slot);
}

void GPU_uniformbuf_unbind(GPUUniformBuf *ubo)
{
  unwrap(ubo)->unbind();
}

void GPU_uniformbuf_unbind_all()
{
  /* FIXME */
}

void GPU_uniformbuf_clear_to_zero(GPUUniformBuf *ubo)
{
  unwrap(ubo)->clear_to_zero();
}

/** \} */
