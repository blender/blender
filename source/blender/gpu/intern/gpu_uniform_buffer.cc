/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "MEM_guardedalloc.h"
#include <cstdint>
#include <cstring>

#include "BLI_listbase.hh"
#include "BLI_map.hh"
#include "BLI_math_base_c.hh"
#include "BLI_string.hh"

#include "BKE_global.hh"

#include "gpu_backend.hh"
#include "gpu_node_graph.hh"

#include "GPU_capabilities.hh"
#include "GPU_context.hh"
#include "GPU_material.hh"

#include "GPU_uniform_buffer.hh"
#include "gpu_context_private.hh"
#include "gpu_uniform_buffer_private.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

namespace gpu {

UniformBuf::UniformBuf(size_t size, const char *name)
{
  /* Make sure that UBO is padded to size of vec4 */
  BLI_assert((size % 16) == 0);

  size_in_bytes_ = size;

  STRNCPY(name_, name);
}

UniformBuf::~UniformBuf()
{
  MEM_SAFE_DELETE_VOID(data_);
}

}  // namespace gpu

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform buffer from GPUInput list
 * \{ */

/**
 * We need to pad some data types (vec3/int3) on the C side
 * To match the GPU expected memory block alignment.
 */

/** std140 storage layout: 4-byte base alignment. */
static constexpr size_t GPU_UBO_ALIGNMENT = 4;

static bool gpu_type_is_ubo_scalar(const GPUType type)
{
  return gpu_type_element_count(type) == 1;
}

static GPUType get_padded_gpu_type(LinkData *link)
{
  GPUInput *input = static_cast<GPUInput *>(link->data);
  GPUType gputype = input->type;
  /* Metal cannot pack scalars after vec3/int3. */
  if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
    switch (gputype) {
      case GPU_VEC3:
        return GPU_VEC4;
      case GPU_INT3:
        return GPU_INT4;
      default:
        return gputype;
    }
  }
  /* Unless vec3/int3 is followed by a scalar we need to treat it as a vec4/int4. */
  if (ELEM(gputype, GPU_VEC3, GPU_INT3) && (link->next != nullptr)) {
    const GPUType next_type = static_cast<GPUInput *>(link->next->data)->type;
    if (!gpu_type_is_ubo_scalar(next_type)) {
      return (gputype == GPU_VEC3) ? GPU_VEC4 : GPU_INT4;
    }
  }
  return gputype;
}

static void gpu_constant_populate_ubo(void *destination,
                                      const GPUInputConstantData &data,
                                      const GPUType type)
{
  switch (type) {
    case GPU_FLOAT:
    case GPU_VEC2:
    case GPU_VEC3:
    case GPU_VEC4:
    case GPU_MAT4: {
      const Span<float> span = gpu_constant_to_float_span(data, type);
      memcpy(destination, span.data(), static_cast<size_t>(span.size_in_bytes()));
      return;
    }
    case GPU_INT:
    case GPU_INT2:
    case GPU_INT3:
    case GPU_INT4: {
      const Span<int> span = gpu_constant_to_int_span(data, type);
      memcpy(destination, span.data(), static_cast<size_t>(span.size_in_bytes()));
      return;
    }
    case GPU_BOOL: {
      /* Pad bool to 4 bytes in UBO. */
      const int32_t value = gpu_constant_to_bool(data) ? 1 : 0;
      memcpy(destination, &value, sizeof(value));
      return;
    }
    default:
      break;
  }

  BLI_assert_unreachable();
}

static void buffer_reorder_scalar_after_size3_vec(ListBaseT<LinkData> *inputs,
                                                  LinkData *size3_vec_link,
                                                  Map<GPUType, LinkData *> &first_links)
{
  const GPUType size3_vec_type = static_cast<GPUInput *>(size3_vec_link->data)->type;
  BLI_assert(ELEM(size3_vec_type, GPU_VEC3, GPU_INT3));

  LinkData *link = size3_vec_link;
  while (link != nullptr && static_cast<GPUInput *>(link->data)->type == size3_vec_type) {
    LinkData *link_next = link->next;

    /* If followed by nothing or a scalar, no need for alignment. */
    if ((link_next == nullptr) ||
        gpu_type_is_ubo_scalar(static_cast<GPUInput *>(link_next->data)->type))
    {
      break;
    }

    for (const GPUType scalar_type : {GPU_FLOAT, GPU_INT, GPU_BOOL}) {
      LinkData **scalar_link_ptr = first_links.lookup_ptr(scalar_type);
      if (scalar_link_ptr != nullptr && *scalar_link_ptr != nullptr) {
        LinkData *scalar_input = *scalar_link_ptr;
        first_links.add_overwrite(scalar_type, scalar_input->next);

        BLI_remlink(inputs, scalar_input);
        BLI_insertlinkafter(inputs, link, scalar_input);
        break;
      }
    }

    link = link_next;
  }
}

/**
 * Returns 1 if the first item should be after second item.
 * We make sure the vec4 uniforms come first.
 */
static int inputs_cmp(const void *a, const void *b)
{
  const LinkData *link_a = static_cast<const LinkData *>(a),
                 *link_b = static_cast<const LinkData *>(b);
  const GPUInput *input_a = static_cast<const GPUInput *>(link_a->data);
  const GPUInput *input_b = static_cast<const GPUInput *>(link_b->data);
  return gpu_type_element_count(input_a->type) < gpu_type_element_count(input_b->type) ? 1 : 0;
}

static inline bool is_ubo_supported_type(const GPUType type)
{
  switch (type) {
    case GPU_FLOAT:
    case GPU_VEC2:
    case GPU_VEC3:
    case GPU_VEC4:
    case GPU_MAT4:
    case GPU_INT:
    case GPU_INT2:
    case GPU_INT3:
    case GPU_INT4:
    case GPU_BOOL:
      return true;
    case GPU_NONE:
    case GPU_MAT3:
    case GPU_TEX1D_ARRAY:
    case GPU_TEX2D:
    case GPU_TEX2D_ARRAY:
    case GPU_TEX3D:
    case GPU_CLOSURE:
    case GPU_ATTR:
      return false;
  }

  BLI_assert_unreachable();
  return false;
}

/**
 * Make sure we respect the expected alignment of UBOs.
 * mat4, vec4, pad vec3 as vec4, then vec2, then floats.
 */
static void buffer_from_list_inputs_sort(ListBaseT<LinkData> *inputs)
{
  /* Order them as mat4, vec4, vec3, vec2, float. */
  BLI_listbase_sort(inputs, inputs_cmp);

  /* Metal cannot pack scalars after vec3/int3. */
  if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
    return;
  }

  Map<GPUType, LinkData *> first_links;
  GPUType cur_type = GPU_NONE;

  for (LinkData &link : *inputs) {
    GPUInput *input = static_cast<GPUInput *>(link.data);

    if (input->type == GPU_MAT3) {
      /* Alignment for mat3 is not handled currently, so not supported */
      BLI_assert_msg(0, "mat3 not supported in UBO");
      continue;
    }

    if (!is_ubo_supported_type(input->type)) {
      BLI_assert_msg(0, "GPU type not supported in UBO");
      continue;
    }

    if (input->type == cur_type) {
      continue;
    }

    first_links.add_new(input->type, &link);
    cur_type = input->type;
  }

  if (first_links.contains(GPU_VEC3)) {
    buffer_reorder_scalar_after_size3_vec(inputs, first_links.lookup(GPU_VEC3), first_links);
  }
  if (first_links.contains(GPU_INT3)) {
    buffer_reorder_scalar_after_size3_vec(inputs, first_links.lookup(GPU_INT3), first_links);
  }
}

static inline size_t buffer_size_from_list(ListBaseT<LinkData> *inputs)
{
  size_t buffer_size = 0;
  for (LinkData &link : *inputs) {
    const GPUType gputype = get_padded_gpu_type(&link);
    buffer_size += gpu_type_element_count(gputype) * GPU_UBO_ALIGNMENT;
  }
  /* Round up to size of vec4. (Opengl Requirement) */
  size_t alignment = GPU_UBO_ALIGNMENT * 4;
  buffer_size = divide_ceil_u(buffer_size, alignment) * alignment;

  return buffer_size;
}

static inline void buffer_fill_from_list(void *data, ListBaseT<LinkData> *inputs)
{
  /* Now that we know the total ubo size we can start populating it. */
  char *offset = static_cast<char *>(data);
  for (LinkData &link : *inputs) {
    GPUInput *input = static_cast<GPUInput *>(link.data);
    const GPUType padded_type = get_padded_gpu_type(&link);
    gpu_constant_populate_ubo(offset, input->constant_data, input->type);
    offset += gpu_type_element_count(padded_type) * GPU_UBO_ALIGNMENT;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name C-API
 * \{ */

using namespace blender::gpu;

gpu::UniformBuf *GPU_uniformbuf_create_ex(size_t size, const void *data, const char *name)
{
  UniformBuf *ubo = GPUBackend::get()->uniformbuf_alloc(size, name);
  /* Direct init. */
  if (data != nullptr) {
    ubo->update(data);
  }
  else if (G.debug & G_DEBUG_GPU) {
    /* Fill the buffer with poison values.
     * (NaN for floats, -1 for `int` and "max value" for `uint`). */
    Vector<uchar> uninitialized_data(size, 0xFF);
    ubo->update(uninitialized_data.data());
  }
  return ubo;
}

gpu::UniformBuf *GPU_uniformbuf_create_from_list(ListBaseT<LinkData> *inputs, const char *name)
{
  /* There is no point on creating an UBO if there is no arguments. */
  if (inputs->is_empty()) {
    return nullptr;
  }

  buffer_from_list_inputs_sort(inputs);
  size_t buffer_size = buffer_size_from_list(inputs);
  void *data = MEM_new_uninitialized(buffer_size, __func__);
  buffer_fill_from_list(data, inputs);

  UniformBuf *ubo = nullptr;
  if (buffer_size <= GPU_max_uniform_buffer_size()) {
    ubo = GPUBackend::get()->uniformbuf_alloc(buffer_size, name);
    /* Defer data upload. */
    ubo->attach_data(data);
  }
  return ubo;
}

void GPU_uniformbuf_free(gpu::UniformBuf *ubo)
{
  delete ubo;
}

void GPU_uniformbuf_update(gpu::UniformBuf *ubo, const void *data)
{
  ubo->update(data);
}

void GPU_uniformbuf_bind(gpu::UniformBuf *ubo, int slot)
{
  ubo->bind(slot);
}

void GPU_uniformbuf_bind_as_ssbo(gpu::UniformBuf *ubo, int slot)
{
  ubo->bind_as_ssbo(slot);
}

void GPU_uniformbuf_unbind(gpu::UniformBuf *ubo)
{
  ubo->unbind();
}

void GPU_uniformbuf_debug_unbind_all()
{
  Context::get()->debug_unbind_all_ubo();
}

void GPU_uniformbuf_clear_to_zero(gpu::UniformBuf *ubo)
{
  ubo->clear_to_zero();
}

/** \} */

}  // namespace blender
