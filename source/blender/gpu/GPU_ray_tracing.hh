/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_math_matrix_types.hh"

#include <memory>
#include <optional>

namespace blender::gpu {
class IndexBuf;
class VertBuf;
class BottomLevelAS;

struct InstanceID {
  int64_t id;
};

/**
 * Acceleration structure for an array of instances.
 */
class TopLevelAS {
 private:
  const char *name_;

 protected:
  TopLevelAS(const char *name) : name_(name) {}

  const char *name_get() const
  {
    return name_;
  }

 public:
  virtual ~TopLevelAS() = default;

  virtual InstanceID add_instance(const BottomLevelAS &blas,
                                  const float4x4 &mat,
                                  uint8_t mask = 0xFF) = 0;
  virtual void update_instance(InstanceID instance_id,
                               const float4x4 &mat,
                               uint8_t mask = 0xFF) = 0;
  virtual void build() = 0;
  virtual void bind(int slot) = 0;
};

/**
 * Acceleration structure for an array of geometries
 */
class BottomLevelAS {
 private:
  const char *name_;

 protected:
  BottomLevelAS(const char *name) : name_(name) {}

  const char *name_get() const
  {
    return name_;
  }

 public:
  virtual ~BottomLevelAS() = default;

  /**
   * Add geometry to the BLAS.
   *
   * Geometry is defined as an index buffer and vertex buffer.
   *
   * index_buffer must use GPU_PRIM_TRIS.
   */
  virtual void add_geometry(IndexBuf &index_buffer, VertBuf &vertex_buffer) = 0;
  virtual void build() = 0;
};
}  // namespace blender::gpu

blender::gpu::TopLevelAS *GPU_ray_tracing_tlas_alloc(const char *name);
blender::gpu::BottomLevelAS *GPU_ray_tracing_blas_alloc(const char *name);
void GPU_ray_tracing_tlas_discard(blender::gpu::TopLevelAS *tlas);
void GPU_ray_tracing_blas_discard(blender::gpu::BottomLevelAS *blas);

namespace blender::gpu {
class TopLevelASDeleter {
 public:
  void operator()(TopLevelAS *tlas)
  {
    GPU_ray_tracing_tlas_discard(tlas);
  }
};
class BottomLevelASDeleter {
 public:
  void operator()(BottomLevelAS *tlas)
  {
    GPU_ray_tracing_blas_discard(tlas);
  }
};

using TopLevelASPtr = std::unique_ptr<TopLevelAS, TopLevelASDeleter>;
using BottomLevelASPtr = std::unique_ptr<BottomLevelAS, BottomLevelASDeleter>;
}  // namespace blender::gpu
