/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "mtl_ray_tracing.hh"

#include "mtl_context.hh"
#include "mtl_index_buffer.hh"
#include "mtl_vertex_buffer.hh"

namespace blender::gpu {

/* Pack a column-major float4x4 into Metal's column-major 4x3 instance transform. */
static MTLPackedFloat4x3 pack_transform(const float4x4 &m)
{
  MTLPackedFloat4x3 out;
  for (int col = 0; col < 4; col++) {
    out.columns[col].x = m[col].x;
    out.columns[col].y = m[col].y;
    out.columns[col].z = m[col].z;
  }
  return out;
}

/* -------------------------------------------------------------------- */
/** \name Bottom level acceleration structure
 * \{ */

MTLBottomLevelAS::MTLBottomLevelAS(const char *name) : BottomLevelAS(name) {}

MTLBottomLevelAS::~MTLBottomLevelAS()
{
  if (acceleration_structure_ != nil) {
    [acceleration_structure_ release];
    acceleration_structure_ = nil;
  }
  if (geometry_ != nil) {
    [geometry_ release];
    geometry_ = nil;
  }
}

void MTLBottomLevelAS::add_geometry(IndexBuf &index_buffer, VertBuf &vertex_buffer)
{
  MTLVertBuf &mtl_vertex = static_cast<MTLVertBuf &>(vertex_buffer);
  MTLIndexBuf &mtl_index = static_cast<MTLIndexBuf &>(index_buffer);

  /* Ensure buffer contents are resident on the GPU. */
  mtl_vertex.bind();
  mtl_vertex.flag_used();
  mtl_index.upload_data();

  GPUPrimType prim_type = GPU_PRIM_TRIS;
  uint index_count = index_buffer.index_len_get();
  id<MTLBuffer> index_metal_buffer = mtl_index.get_index_buffer(prim_type, index_count);
  id<MTLBuffer> vertex_metal_buffer = mtl_vertex.get_metal_buffer();
  if (index_metal_buffer == nil || vertex_metal_buffer == nil) {
    return;
  }

  MTLAccelerationStructureTriangleGeometryDescriptor *geo =
      [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
  geo.vertexBuffer = vertex_metal_buffer;
  geo.vertexBufferOffset = 0;
  geo.vertexStride = vertex_buffer.format.stride;
  if (@available(macOS 13.0, *)) {
    geo.vertexFormat = MTLAttributeFormatFloat3;
  }
  geo.indexBuffer = index_metal_buffer;
  geo.indexBufferOffset = 0;
  geo.indexType = mtl_index.is_32bit() ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
  geo.triangleCount = index_count / 3;
  geo.opaque = YES;

  if (geometry_ == nil) {
    geometry_ = [[NSMutableArray alloc] init];
  }
  [geometry_ addObject:geo];
}

void MTLBottomLevelAS::build()
{
  @autoreleasepool {
    MTLContext *ctx = MTLContext::get();
    id<MTLDevice> device = ctx->device;

    MTLPrimitiveAccelerationStructureDescriptor *desc =
        [MTLPrimitiveAccelerationStructureDescriptor descriptor];
    desc.geometryDescriptors = geometry_;

    MTLAccelerationStructureSizes sizes = [device accelerationStructureSizesWithDescriptor:desc];

    if (acceleration_structure_ != nil) {
      [acceleration_structure_ release];
    }
    acceleration_structure_ = [device
        newAccelerationStructureWithSize:sizes.accelerationStructureSize];

    NSUInteger scratch_size = (sizes.buildScratchBufferSize > 0) ? sizes.buildScratchBufferSize :
                                                                   256;
    gpu::MTLBuffer *scratch = MTLContext::get_global_memory_manager()->allocate(scratch_size,
                                                                                false);

    id<MTLAccelerationStructureCommandEncoder> enc =
        ctx->main_command_buffer.ensure_begin_acceleration_structure_encoder();
    [enc buildAccelerationStructure:acceleration_structure_
                         descriptor:desc
                      scratchBuffer:scratch->get_metal_buffer()
                scratchBufferOffset:0];

    scratch->free();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Top level acceleration structure
 * \{ */

MTLTopLevelAS::MTLTopLevelAS(const char *name) : TopLevelAS(name) {}

MTLTopLevelAS::~MTLTopLevelAS()
{
  /* Clear any binding still referencing this structure to avoid a dangling pointer. */
  MTLContext *ctx = MTLContext::get();
  if (ctx != nullptr) {
    for (MTLAccelerationStructureBinding &binding :
         ctx->pipeline_state.acceleration_structure_bindings)
    {
      if (binding.tlas == this) {
        binding.tlas = nullptr;
        binding.bound = false;
      }
    }
  }
  if (acceleration_structure_ != nil) {
    [acceleration_structure_ release];
    acceleration_structure_ = nil;
  }
}

InstanceID MTLTopLevelAS::add_instance(const BottomLevelAS &blas,
                                       const float4x4 &mat,
                                       uint8_t mask)
{
  InstanceID id = {int64_t(instances_.size())};
  instances_.append({mat, static_cast<const MTLBottomLevelAS *>(&blas), mask});
  is_dirty_ = true;
  needs_rebuild_ = true;
  return id;
}

void MTLTopLevelAS::update_instance(InstanceID instance_id, const float4x4 &mat, uint8_t mask)
{
  if (instance_id.id < 0 || instance_id.id >= instances_.size()) {
    return;
  }
  instances_[instance_id.id].transform = mat;
  instances_[instance_id.id].mask = mask;
  is_dirty_ = true;
}

void MTLTopLevelAS::build()
{
  if (acceleration_structure_ != nil && !is_dirty_) {
    return;
  }

  @autoreleasepool {
    MTLContext *ctx = MTLContext::get();
    id<MTLDevice> device = ctx->device;

    const int64_t instance_count = instances_.size();

    /* Instance descriptors + array of the referenced bottom level structures.
     * Metal requires a non-nil instance descriptor buffer even for an empty TLAS. */
    const int64_t descriptor_count = (instance_count > 0) ? instance_count : 1;
    NSMutableArray<id<MTLAccelerationStructure>> *blas_array = [NSMutableArray
        arrayWithCapacity:instance_count];

    MTLInstanceAccelerationStructureDescriptor *desc =
        [MTLInstanceAccelerationStructureDescriptor descriptor];
    desc.usage = MTLAccelerationStructureUsageRefit;
    desc.instanceCount = instance_count;
    desc.instanceDescriptorBufferOffset = 0;

    /* The UserID instance descriptor + instanceDescriptorType are macOS 12; the ray-query feature
     * requires macOS 13, so this branch always runs when reached. userID is the Metal equivalent
     * of the per-instance custom index. */
    gpu::MTLBuffer *instance_buffer = nullptr;
    if (@available(macOS 12.0, *)) {
      instance_buffer = MTLContext::get_global_memory_manager()->allocate(
          descriptor_count * sizeof(MTLAccelerationStructureUserIDInstanceDescriptor), true);
      MTLAccelerationStructureUserIDInstanceDescriptor *descs =
          (MTLAccelerationStructureUserIDInstanceDescriptor *)instance_buffer->get_host_ptr();
      for (int64_t i = 0; i < instance_count; i++) {
        const Instance &inst = instances_[i];
        descs[i].transformationMatrix = pack_transform(inst.transform);
        descs[i].options = MTLAccelerationStructureInstanceOptionOpaque;
        descs[i].mask = inst.mask;
        descs[i].intersectionFunctionTableOffset = 0;
        descs[i].accelerationStructureIndex = uint32_t(i);
        /* Per-instance custom index; hardcoded 0 to match Vulkan until add_instance exposes it. */
        descs[i].userID = 0;
        [blas_array addObject:inst.blas->acceleration_structure()];
      }
      desc.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeUserID;
      desc.instanceDescriptorStride = sizeof(MTLAccelerationStructureUserIDInstanceDescriptor);
    }

    BLI_assert(instance_buffer != nullptr);
    desc.instanceDescriptorBuffer = instance_buffer->get_metal_buffer();
    desc.instancedAccelerationStructures = blas_array;

    MTLAccelerationStructureSizes sizes = [device accelerationStructureSizesWithDescriptor:desc];

    id<MTLAccelerationStructureCommandEncoder> enc =
        ctx->main_command_buffer.ensure_begin_acceleration_structure_encoder();

    /* Refit in place when only instance data changed since the last build (no new instances). */
    if (acceleration_structure_ != nil && !needs_rebuild_) {
      NSUInteger scratch_size = (sizes.refitScratchBufferSize > 0) ? sizes.refitScratchBufferSize :
                                                                     256;
      gpu::MTLBuffer *scratch = MTLContext::get_global_memory_manager()->allocate(scratch_size,
                                                                                  false);
      [enc refitAccelerationStructure:acceleration_structure_
                           descriptor:desc
                          destination:nil
                        scratchBuffer:scratch->get_metal_buffer()
                  scratchBufferOffset:0];
      scratch->free();
    }
    else {
      if (acceleration_structure_ != nil) {
        [acceleration_structure_ release];
      }
      acceleration_structure_ = [device
          newAccelerationStructureWithSize:sizes.accelerationStructureSize];

      NSUInteger scratch_size = (sizes.buildScratchBufferSize > 0) ? sizes.buildScratchBufferSize :
                                                                     256;
      gpu::MTLBuffer *scratch = MTLContext::get_global_memory_manager()->allocate(scratch_size,
                                                                                  false);
      [enc buildAccelerationStructure:acceleration_structure_
                           descriptor:desc
                        scratchBuffer:scratch->get_metal_buffer()
                  scratchBufferOffset:0];
      scratch->free();
    }

    instance_buffer->free();
  }

  needs_rebuild_ = false;
  is_dirty_ = false;
}

void MTLTopLevelAS::bind(int slot)
{
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);
  BLI_assert(slot >= 0 && slot < int(ctx->pipeline_state.acceleration_structure_bindings.size()));
  MTLAccelerationStructureBinding &binding =
      ctx->pipeline_state.acceleration_structure_bindings[slot];
  binding.tlas = this;
  binding.bound = true;
}

/** \} */

}  // namespace blender::gpu
