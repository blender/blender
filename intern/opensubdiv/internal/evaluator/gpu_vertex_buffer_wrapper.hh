/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_context.hh"
#include "GPU_vertex_buffer.hh"

namespace blender::opensubdiv {

/**
 * GLVertexBuffer compatible API wrapped around a blender::gpu::VertBuf
 *
 * The blender::gpu::VertBuf is owned by the wrapper.
 * Vertex buffer is used as its API is able to wrap around SSBOs as well.
 */
class GPUVertexBuffer {
  gpu::VertBuf &gpu_vertex_buffer_;

  /** Number of float elements/components does a single vertex have. */
  int element_count_;

  /** Should we upload data directly to the GPU, or should we use a staging buffer. */
  bool use_update_sub_;

 public:
  GPUVertexBuffer(gpu::VertBuf &gpu_vertex_buffer, int element_count, bool use_update_sub)
      : gpu_vertex_buffer_(gpu_vertex_buffer),
        element_count_(element_count),
        use_update_sub_(use_update_sub)
  {
  }

  /**
   * Create a new gpu::VertBuf wrapped in a GPUVertexBuffer.
   *
   * @param element_count: Number of elements per vertex
   * @param vertex_len: Number of vertices
   * @param device_context: Unused.
   */

  static GPUVertexBuffer *Create(int element_count, int vertex_len, void *device_context = nullptr)
  {
    using namespace blender::gpu;
    (void)device_context;
    GPUVertFormat format;
    GPU_vertformat_clear(&format);
    switch (element_count) {
      case 4:
        GPU_vertformat_attr_add(&format, "elements", VertAttrType::SFLOAT_32_32_32_32);
        break;
      case 3:
        GPU_vertformat_attr_add(&format, "elements", VertAttrType::SFLOAT_32_32_32);
        break;
      case 2:
        GPU_vertformat_attr_add(&format, "elements", VertAttrType::SFLOAT_32_32);
        break;
      case 1:
        GPU_vertformat_attr_add(&format, "elements", VertAttrType::SFLOAT_32);
        break;
      default:
        assert(0);
        break;
    }
    const bool use_update_sub = GPU_backend_get_type() != GPU_BACKEND_VULKAN;
    gpu::VertBuf *vertex_buffer = nullptr;
    if (use_update_sub) {
      vertex_buffer = GPU_vertbuf_calloc();
      GPU_vertbuf_init_build_on_device(*vertex_buffer, format, vertex_len);
    }
    else {
      vertex_buffer = GPU_vertbuf_create_with_format_ex(format, GPU_USAGE_DYNAMIC);
      GPU_vertbuf_data_alloc(*vertex_buffer, vertex_len);
    }
    return new GPUVertexBuffer(*vertex_buffer, element_count, use_update_sub);
  }

  /// Destructor.
  ~GPUVertexBuffer()
  {
    GPU_vertbuf_discard(&gpu_vertex_buffer_);
  }

  /// This method is meant to be used in client code in order to provide coarse
  /// vertices data to Osd.
  void UpdateData(const float *src,
                  int start_vertex,
                  int num_vertices,
                  void *device_context = NULL)
  {
    (void)device_context;
    if (use_update_sub_) {
      GPU_vertbuf_use(&gpu_vertex_buffer_);
      size_t offset = start_vertex * element_count_ * sizeof(float);
      size_t data_len = num_vertices * element_count_ * sizeof(float);
      GPU_vertbuf_update_sub(&gpu_vertex_buffer_, offset, data_len, src);
    }
    else {
      MutableSpan<float> buffer_nodes = gpu_vertex_buffer_.data<float>();
      buffer_nodes = buffer_nodes.drop_front(start_vertex * element_count_);
      memcpy(buffer_nodes.data(), src, sizeof(float) * element_count_ * num_vertices);
      GPU_vertbuf_tag_dirty(&gpu_vertex_buffer_);
    }
  }

  /// Returns how many vertices allocated in this vertex buffer.
  int GetNumVertices() const
  {
    return GPU_vertbuf_get_vertex_len(&gpu_vertex_buffer_);
  }

  gpu::VertBuf *get_vertex_buffer()
  {
    return &gpu_vertex_buffer_;
  }
};

}  // namespace blender::opensubdiv
