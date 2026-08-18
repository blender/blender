/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_vector.hh"

#include "GPU_ray_tracing.hh"

#include <Metal/Metal.h>

namespace blender::gpu {

/** Bottom level acceleration structure (geometry). */
class MTLBottomLevelAS : public BottomLevelAS {
  id<MTLAccelerationStructure> acceleration_structure_ = nil;
  NSMutableArray<MTLAccelerationStructureGeometryDescriptor *> *geometry_ = nil;

 public:
  MTLBottomLevelAS(const char *name);
  ~MTLBottomLevelAS() override;

  void add_geometry(IndexBuf &index_buffer, VertBuf &vertex_buffer) override;
  void build() override;

  id<MTLAccelerationStructure> acceleration_structure() const
  {
    return acceleration_structure_;
  }

 private:
  MEM_CXX_CLASS_ALLOC_FUNCS("MTLBottomLevelAS");
};

/** Top level acceleration structure (instances). */
class MTLTopLevelAS : public TopLevelAS {
  struct Instance {
    float4x4 transform;
    const MTLBottomLevelAS *blas;
    uint8_t mask;
  };

  id<MTLAccelerationStructure> acceleration_structure_ = nil;
  Vector<Instance> instances_;
  bool is_dirty_ = true;
  /* Full rebuild needed (topology changed); otherwise a transform update can refit in place. */
  bool needs_rebuild_ = true;

 public:
  MTLTopLevelAS(const char *name);
  ~MTLTopLevelAS() override;

  InstanceID add_instance(const BottomLevelAS &blas, const float4x4 &mat, uint8_t mask) override;
  void update_instance(InstanceID instance_id, const float4x4 &mat, uint8_t mask) override;
  void build() override;
  void bind(int slot) override;

  id<MTLAccelerationStructure> acceleration_structure() const
  {
    return acceleration_structure_;
  }

  /* Keep the structure and its referenced geometry resident on a command encoder wrapper. */
  template<typename Encoder> void make_resident(Encoder &enc)
  {
    if (acceleration_structure_ != nil) {
      enc.use_resource(acceleration_structure_, MTLResourceUsageRead);
    }
    for (const Instance &inst : instances_) {
      id<MTLAccelerationStructure> blas = inst.blas->acceleration_structure();
      if (blas != nil) {
        enc.use_resource(blas, MTLResourceUsageRead);
      }
    }
  }

 private:
  MEM_CXX_CLASS_ALLOC_FUNCS("MTLTopLevelAS");
};

}  // namespace blender::gpu
