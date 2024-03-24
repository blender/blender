/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU geometry batch
 * Contains VAOs + VBOs + Shader representing a drawable entity.
 */

#pragma once

#include "MEM_guardedalloc.h"
#include "gpu_batch_private.hh"
#include "mtl_index_buffer.hh"
#include "mtl_primitive.hh"
#include "mtl_shader.hh"
#include "mtl_vertex_buffer.hh"

namespace blender::gpu {

class MTLContext;
class MTLShaderInterface;

#define GPU_VAO_STATIC_LEN 64

struct VertexBufferID {
  uint32_t id : 16;
  uint32_t is_instance : 15;
  uint32_t used : 1;
};

class MTLBatch : public Batch {

  /* Vertex Bind-state Caching for a given shader interface used with the Batch. */
  struct VertexDescriptorShaderInterfacePair {
    MTLVertexDescriptor vertex_descriptor{};
    const ShaderInterface *interface = nullptr;
    uint16_t attr_mask{};
    int num_buffers{};
    VertexBufferID bufferIds[GPU_BATCH_VBO_MAX_LEN] = {};
    /* Cache life index compares a cache entry with the active MTLBatch state.
     * This is initially set to the cache life index of MTLBatch. If the batch has been modified,
     * this index is incremented to cheaply invalidate existing cache entries. */
    uint32_t cache_life_index = 0;
  };

  class MTLVertexDescriptorCache {

   private:
    MTLBatch *batch_;

    VertexDescriptorShaderInterfacePair cache_[GPU_VAO_STATIC_LEN] = {};
    MTLContext *cache_context_ = nullptr;
    uint32_t cache_life_index_ = 0;

   public:
    MTLVertexDescriptorCache(MTLBatch *batch) : batch_(batch){};
    VertexDescriptorShaderInterfacePair *find(const ShaderInterface *interface);
    bool insert(VertexDescriptorShaderInterfacePair &data);

   private:
    void vertex_descriptor_cache_init(MTLContext *ctx);
    void vertex_descriptor_cache_clear();
    void vertex_descriptor_cache_ensure();
  };

 private:
  MTLShader *active_shader_ = nullptr;
  bool shader_in_use_ = false;
  MTLVertexDescriptorCache vao_cache = {this};

  /* Topology emulation. */
  gpu::MTLBuffer *emulated_topology_buffer_ = nullptr;
  GPUPrimType emulated_topology_type_;
  uint32_t topology_buffer_input_v_count_ = 0;
  uint32_t topology_buffer_output_v_count_ = 0;

 public:
  MTLBatch(){};
  ~MTLBatch(){};

  void draw(int v_first, int v_count, int i_first, int i_count) override;
  void draw_indirect(GPUStorageBuf *indirect_buf, intptr_t offset) override;
  void multi_draw_indirect(GPUStorageBuf * /*indirect_buf*/,
                           int /*count*/,
                           intptr_t /*offset*/,
                           intptr_t /*stride*/) override
  {
    /* TODO(Metal): Support indirect draw commands. */
  }

  /* Returns an initialized RenderComandEncoder for drawing if all is good.
   * Otherwise, nil. */
  id<MTLRenderCommandEncoder> bind(uint v_count);
  void unbind(id<MTLRenderCommandEncoder> rec);

  /* Convenience getters. */
  MTLIndexBuf *elem_() const
  {
    return static_cast<MTLIndexBuf *>(elem);
  }
  MTLVertBuf *verts_(const int index) const
  {
    return static_cast<MTLVertBuf *>(verts[index]);
  }
  MTLVertBuf *inst_(const int index) const
  {
    return static_cast<MTLVertBuf *>(inst[index]);
  }
  MTLShader *active_shader_get() const
  {
    return active_shader_;
  }

 private:
  void shader_bind();
  void draw_advanced(int v_first, int v_count, int i_first, int i_count);
  void draw_advanced_indirect(GPUStorageBuf *indirect_buf, intptr_t offset);
  int prepare_vertex_binding(MTLVertBuf *verts,
                             MTLRenderPipelineStateDescriptor &desc,
                             const MTLShaderInterface *interface,
                             uint16_t &attr_mask,
                             bool instanced);

  id<MTLBuffer> get_emulated_toplogy_buffer(GPUPrimType &in_out_prim_type, uint32_t &v_count);

  void prepare_vertex_descriptor_and_bindings(MTLVertBuf **buffers, int &num_buffers);

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLBatch");
};

}  // namespace blender::gpu
