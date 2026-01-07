/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Uniform buffers API. Used to handle many uniforms update at once.
 * Make sure that the data structure is compatible with what the implementation expect.
 * (see "7.6.2.2 Standard Uniform Block Layout" from the OpenGL spec for more info about std140
 * layout)
 * Rule of thumb: Padding to 16bytes, don't use vec3, don't use arrays of anything that is not vec4
 * aligned.
 */

#pragma once

#include "BLI_sys_types.h"

#include "DNA_listBase.h"

namespace blender {

struct GPUInput;

namespace gpu {
class UniformBuf;
}  // namespace gpu

gpu::UniformBuf *GPU_uniformbuf_create_ex(size_t size, const void *data, const char *name);
/**
 * Create UBO from inputs list.
 *
 * \param inputs: ListBaseT<LinkData>
 * \return nullptr if failed to create or if `inputs` is empty.
 */
gpu::UniformBuf *GPU_uniformbuf_create_from_list(ListBaseT<LinkData> *inputs, const char *name);

#define GPU_uniformbuf_create(size) GPU_uniformbuf_create_ex(size, nullptr, __func__);

void GPU_uniformbuf_free(gpu::UniformBuf *ubo);

void GPU_uniformbuf_update(gpu::UniformBuf *ubo, const void *data);

void GPU_uniformbuf_bind(gpu::UniformBuf *ubo, int slot);
void GPU_uniformbuf_bind_as_ssbo(gpu::UniformBuf *ubo, int slot);
void GPU_uniformbuf_unbind(gpu::UniformBuf *ubo);
/**
 * Resets the internal slot usage tracking. But there is no guarantee that
 * this actually undo the bindings for the next draw call. Only has effect when G_DEBUG_GPU is set.
 */
void GPU_uniformbuf_debug_unbind_all();

void GPU_uniformbuf_clear_to_zero(gpu::UniformBuf *ubo);

#define GPU_UBO_BLOCK_NAME "node_tree"
#define GPU_ATTRIBUTE_UBO_BLOCK_NAME "unf_attrs"
#define GPU_LAYER_ATTRIBUTE_UBO_BLOCK_NAME "drw_layer_attrs"
constexpr static int GPU_NODE_TREE_UBO_SLOT = 0;

#define GPU_UBO_FREE_SAFE(ubo) \
  do { \
    if (ubo != nullptr) { \
      GPU_uniformbuf_free(ubo); \
      ubo = nullptr; \
    } \
  } while (0)

}  // namespace blender
