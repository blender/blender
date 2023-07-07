/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

#ifdef WITH_METAL

#  include "scene/hair.h"
#  include "scene/mesh.h"
#  include "scene/object.h"
#  include "scene/pointcloud.h"

#  include "util/progress.h"

#  include "device/metal/bvh.h"
#  include "device/metal/util.h"

CCL_NAMESPACE_BEGIN

#  define BVH_status(...) \
    { \
      string str = string_printf(__VA_ARGS__); \
      progress.set_substatus(str); \
      metal_printf("%s\n", str.c_str()); \
    }

BVHMetal::BVHMetal(const BVHParams &params_,
                   const vector<Geometry *> &geometry_,
                   const vector<Object *> &objects_,
                   Device *device)
    : BVH(params_, geometry_, objects_), stats(device->stats)
{
}

BVHMetal::~BVHMetal()
{
  if (@available(macos 12.0, *)) {
    if (accel_struct) {
      stats.mem_free(accel_struct.allocatedSize);
      [accel_struct release];
    }
  }
}

bool BVHMetal::build_BLAS_mesh(Progress &progress,
                               id<MTLDevice> device,
                               id<MTLCommandQueue> queue,
                               Geometry *const geom,
                               bool refit)
{
  if (@available(macos 12.0, *)) {
    /* Build BLAS for triangle primitives */
    Mesh *const mesh = static_cast<Mesh *const>(geom);
    if (mesh->num_triangles() == 0) {
      return false;
    }

    /*------------------------------------------------*/
    BVH_status(
        "Building mesh BLAS | %7d tris | %s", (int)mesh->num_triangles(), geom->name.c_str());
    /*------------------------------------------------*/

    const bool use_fast_trace_bvh = (params.bvh_type == BVH_TYPE_STATIC);

    const array<float3> &verts = mesh->get_verts();
    const array<int> &tris = mesh->get_triangles();
    const size_t num_verts = verts.size();
    const size_t num_indices = tris.size();

    size_t num_motion_steps = 1;
    Attribute *motion_keys = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (motion_blur && mesh->get_use_motion_blur() && motion_keys) {
      num_motion_steps = mesh->get_motion_steps();
    }

    MTLResourceOptions storage_mode;
    if (device.hasUnifiedMemory) {
      storage_mode = MTLResourceStorageModeShared;
    }
    else {
      storage_mode = MTLResourceStorageModeManaged;
    }

    /* Upload the mesh data to the GPU */
    id<MTLBuffer> posBuf = nil;
    id<MTLBuffer> indexBuf = [device newBufferWithBytes:tris.data()
                                                 length:num_indices * sizeof(tris.data()[0])
                                                options:storage_mode];

    if (num_motion_steps == 1) {
      posBuf = [device newBufferWithBytes:verts.data()
                                   length:num_verts * sizeof(verts.data()[0])
                                  options:storage_mode];
    }
    else {
      posBuf = [device newBufferWithLength:num_verts * num_motion_steps * sizeof(verts.data()[0])
                                   options:storage_mode];
      float3 *dest_data = (float3 *)[posBuf contents];
      size_t center_step = (num_motion_steps - 1) / 2;
      for (size_t step = 0; step < num_motion_steps; ++step) {
        const float3 *verts = mesh->get_verts().data();

        /* The center step for motion vertices is not stored in the attribute. */
        if (step != center_step) {
          verts = motion_keys->data_float3() + (step > center_step ? step - 1 : step) * num_verts;
        }
        memcpy(dest_data + num_verts * step, verts, num_verts * sizeof(float3));
      }
      if (storage_mode == MTLResourceStorageModeManaged) {
        [posBuf didModifyRange:NSMakeRange(0, posBuf.length)];
      }
    }

    /* Create an acceleration structure. */
    MTLAccelerationStructureGeometryDescriptor *geomDesc;
    if (num_motion_steps > 1) {
      std::vector<MTLMotionKeyframeData *> vertex_ptrs;
      vertex_ptrs.reserve(num_motion_steps);
      for (size_t step = 0; step < num_motion_steps; ++step) {
        MTLMotionKeyframeData *k = [MTLMotionKeyframeData data];
        k.buffer = posBuf;
        k.offset = num_verts * step * sizeof(float3);
        vertex_ptrs.push_back(k);
      }

      MTLAccelerationStructureMotionTriangleGeometryDescriptor *geomDescMotion =
          [MTLAccelerationStructureMotionTriangleGeometryDescriptor descriptor];
      geomDescMotion.vertexBuffers = [NSArray arrayWithObjects:vertex_ptrs.data()
                                                         count:vertex_ptrs.size()];
      geomDescMotion.vertexStride = sizeof(verts.data()[0]);
      geomDescMotion.indexBuffer = indexBuf;
      geomDescMotion.indexBufferOffset = 0;
      geomDescMotion.indexType = MTLIndexTypeUInt32;
      geomDescMotion.triangleCount = num_indices / 3;
      geomDescMotion.intersectionFunctionTableOffset = 0;

      geomDesc = geomDescMotion;
    }
    else {
      MTLAccelerationStructureTriangleGeometryDescriptor *geomDescNoMotion =
          [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
      geomDescNoMotion.vertexBuffer = posBuf;
      geomDescNoMotion.vertexBufferOffset = 0;
      geomDescNoMotion.vertexStride = sizeof(verts.data()[0]);
      geomDescNoMotion.indexBuffer = indexBuf;
      geomDescNoMotion.indexBufferOffset = 0;
      geomDescNoMotion.indexType = MTLIndexTypeUInt32;
      geomDescNoMotion.triangleCount = num_indices / 3;
      geomDescNoMotion.intersectionFunctionTableOffset = 0;

      geomDesc = geomDescNoMotion;
    }

    /* Force a single any-hit call, so shadow record-all behavior works correctly */
    /* (Match optix behavior: unsigned int build_flags =
     * OPTIX_GEOMETRY_FLAG_REQUIRE_SINGLE_ANYHIT_CALL;) */
    geomDesc.allowDuplicateIntersectionFunctionInvocation = false;

    MTLPrimitiveAccelerationStructureDescriptor *accelDesc =
        [MTLPrimitiveAccelerationStructureDescriptor descriptor];
    accelDesc.geometryDescriptors = @[ geomDesc ];
    if (num_motion_steps > 1) {
      accelDesc.motionStartTime = 0.0f;
      accelDesc.motionEndTime = 1.0f;
      accelDesc.motionStartBorderMode = MTLMotionBorderModeClamp;
      accelDesc.motionEndBorderMode = MTLMotionBorderModeClamp;
      accelDesc.motionKeyframeCount = num_motion_steps;
    }

    if (!use_fast_trace_bvh) {
      accelDesc.usage |= (MTLAccelerationStructureUsageRefit |
                          MTLAccelerationStructureUsagePreferFastBuild);
    }

    MTLAccelerationStructureSizes accelSizes = [device
        accelerationStructureSizesWithDescriptor:accelDesc];
    id<MTLAccelerationStructure> accel_uncompressed = [device
        newAccelerationStructureWithSize:accelSizes.accelerationStructureSize];
    id<MTLBuffer> scratchBuf = [device newBufferWithLength:accelSizes.buildScratchBufferSize
                                                   options:MTLResourceStorageModePrivate];
    id<MTLBuffer> sizeBuf = [device newBufferWithLength:8 options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> accelCommands = [queue commandBuffer];
    id<MTLAccelerationStructureCommandEncoder> accelEnc =
        [accelCommands accelerationStructureCommandEncoder];
    if (refit) {
      [accelEnc refitAccelerationStructure:accel_struct
                                descriptor:accelDesc
                               destination:accel_uncompressed
                             scratchBuffer:scratchBuf
                       scratchBufferOffset:0];
    }
    else {
      [accelEnc buildAccelerationStructure:accel_uncompressed
                                descriptor:accelDesc
                             scratchBuffer:scratchBuf
                       scratchBufferOffset:0];
    }
    if (use_fast_trace_bvh) {
      [accelEnc writeCompactedAccelerationStructureSize:accel_uncompressed
                                               toBuffer:sizeBuf
                                                 offset:0
                                           sizeDataType:MTLDataTypeULong];
    }
    [accelEnc endEncoding];
    [accelCommands addCompletedHandler:^(id<MTLCommandBuffer> command_buffer) {
      /* free temp resources */
      [scratchBuf release];
      [indexBuf release];
      [posBuf release];

      if (use_fast_trace_bvh) {
        /* Compact the accel structure */
        uint64_t compressed_size = *(uint64_t *)sizeBuf.contents;

        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
          id<MTLCommandBuffer> accelCommands = [queue commandBuffer];
          id<MTLAccelerationStructureCommandEncoder> accelEnc =
              [accelCommands accelerationStructureCommandEncoder];
          id<MTLAccelerationStructure> accel = [device
              newAccelerationStructureWithSize:compressed_size];
          [accelEnc copyAndCompactAccelerationStructure:accel_uncompressed
                                toAccelerationStructure:accel];
          [accelEnc endEncoding];
          [accelCommands addCompletedHandler:^(id<MTLCommandBuffer> command_buffer) {
            uint64_t allocated_size = [accel allocatedSize];
            stats.mem_alloc(allocated_size);
            accel_struct = accel;
            [accel_uncompressed release];
            accel_struct_building = false;
          }];
          [accelCommands commit];
        });
      }
      else {
        /* set our acceleration structure to the uncompressed structure */
        accel_struct = accel_uncompressed;

        uint64_t allocated_size = [accel_struct allocatedSize];
        stats.mem_alloc(allocated_size);
        accel_struct_building = false;
      }
      [sizeBuf release];
    }];

    accel_struct_building = true;
    [accelCommands commit];

    return true;
  }
  return false;
}

bool BVHMetal::build_BLAS_hair(Progress &progress,
                               id<MTLDevice> device,
                               id<MTLCommandQueue> queue,
                               Geometry *const geom,
                               bool refit)
{
  if (@available(macos 12.0, *)) {
    /* Build BLAS for hair curves */
    Hair *hair = static_cast<Hair *>(geom);
    if (hair->num_curves() == 0) {
      return false;
    }

    /*------------------------------------------------*/
    BVH_status(
        "Building hair BLAS | %7d curves | %s", (int)hair->num_curves(), geom->name.c_str());
    /*------------------------------------------------*/

    const bool use_fast_trace_bvh = (params.bvh_type == BVH_TYPE_STATIC);
    const size_t num_segments = hair->num_segments();

    size_t num_motion_steps = 1;
    Attribute *motion_keys = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (motion_blur && hair->get_use_motion_blur() && motion_keys) {
      num_motion_steps = hair->get_motion_steps();
    }

    const size_t num_aabbs = num_segments * num_motion_steps;

    MTLResourceOptions storage_mode;
    if (device.hasUnifiedMemory) {
      storage_mode = MTLResourceStorageModeShared;
    }
    else {
      storage_mode = MTLResourceStorageModeManaged;
    }

    /* Allocate a GPU buffer for the AABB data and populate it */
    id<MTLBuffer> aabbBuf = [device
        newBufferWithLength:num_aabbs * sizeof(MTLAxisAlignedBoundingBox)
                    options:storage_mode];
    MTLAxisAlignedBoundingBox *aabb_data = (MTLAxisAlignedBoundingBox *)[aabbBuf contents];

    /* Get AABBs for each motion step */
    size_t center_step = (num_motion_steps - 1) / 2;
    for (size_t step = 0; step < num_motion_steps; ++step) {
      /* The center step for motion vertices is not stored in the attribute */
      const float3 *keys = hair->get_curve_keys().data();
      if (step != center_step) {
        size_t attr_offset = (step > center_step) ? step - 1 : step;
        /* Technically this is a float4 array, but sizeof(float3) == sizeof(float4) */
        keys = motion_keys->data_float3() + attr_offset * hair->get_curve_keys().size();
      }

      for (size_t j = 0, i = 0; j < hair->num_curves(); ++j) {
        const Hair::Curve curve = hair->get_curve(j);

        for (int segment = 0; segment < curve.num_segments(); ++segment, ++i) {
          {
            BoundBox bounds = BoundBox::empty;
            curve.bounds_grow(segment, keys, hair->get_curve_radius().data(), bounds);

            const size_t index = step * num_segments + i;
            aabb_data[index].min = (MTLPackedFloat3 &)bounds.min;
            aabb_data[index].max = (MTLPackedFloat3 &)bounds.max;
          }
        }
      }
    }

    if (storage_mode == MTLResourceStorageModeManaged) {
      [aabbBuf didModifyRange:NSMakeRange(0, aabbBuf.length)];
    }

#  if 0
    for (size_t i=0; i<num_aabbs && i < 400; i++) {
      MTLAxisAlignedBoundingBox& bb = aabb_data[i];
      printf("  %d:   %.1f,%.1f,%.1f -- %.1f,%.1f,%.1f\n", int(i), bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z);
    }
#  endif

    MTLAccelerationStructureGeometryDescriptor *geomDesc;
    if (motion_blur) {
      std::vector<MTLMotionKeyframeData *> aabb_ptrs;
      aabb_ptrs.reserve(num_motion_steps);
      for (size_t step = 0; step < num_motion_steps; ++step) {
        MTLMotionKeyframeData *k = [MTLMotionKeyframeData data];
        k.buffer = aabbBuf;
        k.offset = step * num_segments * sizeof(MTLAxisAlignedBoundingBox);
        aabb_ptrs.push_back(k);
      }

      MTLAccelerationStructureMotionBoundingBoxGeometryDescriptor *geomDescMotion =
          [MTLAccelerationStructureMotionBoundingBoxGeometryDescriptor descriptor];
      geomDescMotion.boundingBoxBuffers = [NSArray arrayWithObjects:aabb_ptrs.data()
                                                              count:aabb_ptrs.size()];
      geomDescMotion.boundingBoxCount = num_segments;
      geomDescMotion.boundingBoxStride = sizeof(aabb_data[0]);
      geomDescMotion.intersectionFunctionTableOffset = 1;

      /* Force a single any-hit call, so shadow record-all behavior works correctly */
      /* (Match optix behavior: unsigned int build_flags =
       * OPTIX_GEOMETRY_FLAG_REQUIRE_SINGLE_ANYHIT_CALL;) */
      geomDescMotion.allowDuplicateIntersectionFunctionInvocation = false;
      geomDescMotion.opaque = true;
      geomDesc = geomDescMotion;
    }
    else {
      MTLAccelerationStructureBoundingBoxGeometryDescriptor *geomDescNoMotion =
          [MTLAccelerationStructureBoundingBoxGeometryDescriptor descriptor];
      geomDescNoMotion.boundingBoxBuffer = aabbBuf;
      geomDescNoMotion.boundingBoxBufferOffset = 0;
      geomDescNoMotion.boundingBoxCount = int(num_aabbs);
      geomDescNoMotion.boundingBoxStride = sizeof(aabb_data[0]);
      geomDescNoMotion.intersectionFunctionTableOffset = 1;

      /* Force a single any-hit call, so shadow record-all behavior works correctly */
      /* (Match optix behavior: unsigned int build_flags =
       * OPTIX_GEOMETRY_FLAG_REQUIRE_SINGLE_ANYHIT_CALL;) */
      geomDescNoMotion.allowDuplicateIntersectionFunctionInvocation = false;
      geomDescNoMotion.opaque = true;
      geomDesc = geomDescNoMotion;
    }

    MTLPrimitiveAccelerationStructureDescriptor *accelDesc =
        [MTLPrimitiveAccelerationStructureDescriptor descriptor];
    accelDesc.geometryDescriptors = @[ geomDesc ];

    if (motion_blur) {
      accelDesc.motionStartTime = 0.0f;
      accelDesc.motionEndTime = 1.0f;
      accelDesc.motionStartBorderMode = MTLMotionBorderModeVanish;
      accelDesc.motionEndBorderMode = MTLMotionBorderModeVanish;
      accelDesc.motionKeyframeCount = num_motion_steps;
    }

    if (!use_fast_trace_bvh) {
      accelDesc.usage |= (MTLAccelerationStructureUsageRefit |
                          MTLAccelerationStructureUsagePreferFastBuild);
    }

    MTLAccelerationStructureSizes accelSizes = [device
        accelerationStructureSizesWithDescriptor:accelDesc];
    id<MTLAccelerationStructure> accel_uncompressed = [device
        newAccelerationStructureWithSize:accelSizes.accelerationStructureSize];
    id<MTLBuffer> scratchBuf = [device newBufferWithLength:accelSizes.buildScratchBufferSize
                                                   options:MTLResourceStorageModePrivate];
    id<MTLBuffer> sizeBuf = [device newBufferWithLength:8 options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> accelCommands = [queue commandBuffer];
    id<MTLAccelerationStructureCommandEncoder> accelEnc =
        [accelCommands accelerationStructureCommandEncoder];
    if (refit) {
      [accelEnc refitAccelerationStructure:accel_struct
                                descriptor:accelDesc
                               destination:accel_uncompressed
                             scratchBuffer:scratchBuf
                       scratchBufferOffset:0];
    }
    else {
      [accelEnc buildAccelerationStructure:accel_uncompressed
                                descriptor:accelDesc
                             scratchBuffer:scratchBuf
                       scratchBufferOffset:0];
    }
    if (use_fast_trace_bvh) {
      [accelEnc writeCompactedAccelerationStructureSize:accel_uncompressed
                                               toBuffer:sizeBuf
                                                 offset:0
                                           sizeDataType:MTLDataTypeULong];
    }
    [accelEnc endEncoding];
    [accelCommands addCompletedHandler:^(id<MTLCommandBuffer> command_buffer) {
      /* free temp resources */
      [scratchBuf release];
      [aabbBuf release];

      if (use_fast_trace_bvh) {
        /* Compact the accel structure */
        uint64_t compressed_size = *(uint64_t *)sizeBuf.contents;

        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
          id<MTLCommandBuffer> accelCommands = [queue commandBuffer];
          id<MTLAccelerationStructureCommandEncoder> accelEnc =
              [accelCommands accelerationStructureCommandEncoder];
          id<MTLAccelerationStructure> accel = [device
              newAccelerationStructureWithSize:compressed_size];
          [accelEnc copyAndCompactAccelerationStructure:accel_uncompressed
                                toAccelerationStructure:accel];
          [accelEnc endEncoding];
          [accelCommands addCompletedHandler:^(id<MTLCommandBuffer> command_buffer) {
            uint64_t allocated_size = [accel allocatedSize];
            stats.mem_alloc(allocated_size);
            accel_struct = accel;
            [accel_uncompressed release];
            accel_struct_building = false;
          }];
          [accelCommands commit];
        });
      }
      else {
        /* set our acceleration structure to the uncompressed structure */
        accel_struct = accel_uncompressed;

        uint64_t allocated_size = [accel_struct allocatedSize];
        stats.mem_alloc(allocated_size);
        accel_struct_building = false;
      }
      [sizeBuf release];
    }];

    accel_struct_building = true;
    [accelCommands commit];
    return true;
  }
  return false;
}

bool BVHMetal::build_BLAS_pointcloud(Progress &progress,
                                     id<MTLDevice> device,
                                     id<MTLCommandQueue> queue,
                                     Geometry *const geom,
                                     bool refit)
{
  if (@available(macos 12.0, *)) {
    /* Build BLAS for point cloud */
    PointCloud *pointcloud = static_cast<PointCloud *>(geom);
    if (pointcloud->num_points() == 0) {
      return false;
    }

    /*------------------------------------------------*/
    BVH_status("Building pointcloud BLAS | %7d points | %s",
               (int)pointcloud->num_points(),
               geom->name.c_str());
    /*------------------------------------------------*/

    const size_t num_points = pointcloud->get_points().size();
    const float3 *points = pointcloud->get_points().data();
    const float *radius = pointcloud->get_radius().data();

    const bool use_fast_trace_bvh = (params.bvh_type == BVH_TYPE_STATIC);

    size_t num_motion_steps = 1;
    Attribute *motion_keys = pointcloud->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (motion_blur && pointcloud->get_use_motion_blur() && motion_keys) {
      num_motion_steps = pointcloud->get_motion_steps();
    }

    const size_t num_aabbs = num_motion_steps * num_points;

    MTLResourceOptions storage_mode;
    if (device.hasUnifiedMemory) {
      storage_mode = MTLResourceStorageModeShared;
    }
    else {
      storage_mode = MTLResourceStorageModeManaged;
    }

    /* Allocate a GPU buffer for the AABB data and populate it */
    id<MTLBuffer> aabbBuf = [device
        newBufferWithLength:num_aabbs * sizeof(MTLAxisAlignedBoundingBox)
                    options:storage_mode];
    MTLAxisAlignedBoundingBox *aabb_data = (MTLAxisAlignedBoundingBox *)[aabbBuf contents];

    /* Get AABBs for each motion step */
    size_t center_step = (num_motion_steps - 1) / 2;
    for (size_t step = 0; step < num_motion_steps; ++step) {
      if (step == center_step) {
        /* The center step for motion vertices is not stored in the attribute */
        for (size_t j = 0; j < num_points; ++j) {
          const PointCloud::Point point = pointcloud->get_point(j);
          BoundBox bounds = BoundBox::empty;
          point.bounds_grow(points, radius, bounds);

          const size_t index = step * num_points + j;
          aabb_data[index].min = (MTLPackedFloat3 &)bounds.min;
          aabb_data[index].max = (MTLPackedFloat3 &)bounds.max;
        }
      }
      else {
        size_t attr_offset = (step > center_step) ? step - 1 : step;
        float4 *motion_points = motion_keys->data_float4() + attr_offset * num_points;

        for (size_t j = 0; j < num_points; ++j) {
          const PointCloud::Point point = pointcloud->get_point(j);
          BoundBox bounds = BoundBox::empty;
          point.bounds_grow(motion_points[j], bounds);

          const size_t index = step * num_points + j;
          aabb_data[index].min = (MTLPackedFloat3 &)bounds.min;
          aabb_data[index].max = (MTLPackedFloat3 &)bounds.max;
        }
      }
    }

    if (storage_mode == MTLResourceStorageModeManaged) {
      [aabbBuf didModifyRange:NSMakeRange(0, aabbBuf.length)];
    }

#  if 0
    for (size_t i=0; i<num_aabbs && i < 400; i++) {
      MTLAxisAlignedBoundingBox& bb = aabb_data[i];
      printf("  %d:   %.1f,%.1f,%.1f -- %.1f,%.1f,%.1f\n", int(i), bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z);
    }
#  endif

    MTLAccelerationStructureGeometryDescriptor *geomDesc;
    if (motion_blur) {
      std::vector<MTLMotionKeyframeData *> aabb_ptrs;
      aabb_ptrs.reserve(num_motion_steps);
      for (size_t step = 0; step < num_motion_steps; ++step) {
        MTLMotionKeyframeData *k = [MTLMotionKeyframeData data];
        k.buffer = aabbBuf;
        k.offset = step * num_points * sizeof(MTLAxisAlignedBoundingBox);
        aabb_ptrs.push_back(k);
      }

      MTLAccelerationStructureMotionBoundingBoxGeometryDescriptor *geomDescMotion =
          [MTLAccelerationStructureMotionBoundingBoxGeometryDescriptor descriptor];
      geomDescMotion.boundingBoxBuffers = [NSArray arrayWithObjects:aabb_ptrs.data()
                                                              count:aabb_ptrs.size()];
      geomDescMotion.boundingBoxCount = num_points;
      geomDescMotion.boundingBoxStride = sizeof(aabb_data[0]);
      geomDescMotion.intersectionFunctionTableOffset = 2;

      /* Force a single any-hit call, so shadow record-all behavior works correctly */
      /* (Match optix behavior: unsigned int build_flags =
       * OPTIX_GEOMETRY_FLAG_REQUIRE_SINGLE_ANYHIT_CALL;) */
      geomDescMotion.allowDuplicateIntersectionFunctionInvocation = false;
      geomDescMotion.opaque = true;
      geomDesc = geomDescMotion;
    }
    else {
      MTLAccelerationStructureBoundingBoxGeometryDescriptor *geomDescNoMotion =
          [MTLAccelerationStructureBoundingBoxGeometryDescriptor descriptor];
      geomDescNoMotion.boundingBoxBuffer = aabbBuf;
      geomDescNoMotion.boundingBoxBufferOffset = 0;
      geomDescNoMotion.boundingBoxCount = int(num_aabbs);
      geomDescNoMotion.boundingBoxStride = sizeof(aabb_data[0]);
      geomDescNoMotion.intersectionFunctionTableOffset = 2;

      /* Force a single any-hit call, so shadow record-all behavior works correctly */
      /* (Match optix behavior: unsigned int build_flags =
       * OPTIX_GEOMETRY_FLAG_REQUIRE_SINGLE_ANYHIT_CALL;) */
      geomDescNoMotion.allowDuplicateIntersectionFunctionInvocation = false;
      geomDescNoMotion.opaque = true;
      geomDesc = geomDescNoMotion;
    }

    MTLPrimitiveAccelerationStructureDescriptor *accelDesc =
        [MTLPrimitiveAccelerationStructureDescriptor descriptor];
    accelDesc.geometryDescriptors = @[ geomDesc ];

    if (motion_blur) {
      accelDesc.motionStartTime = 0.0f;
      accelDesc.motionEndTime = 1.0f;
      accelDesc.motionStartBorderMode = MTLMotionBorderModeVanish;
      accelDesc.motionEndBorderMode = MTLMotionBorderModeVanish;
      accelDesc.motionKeyframeCount = num_motion_steps;
    }

    if (!use_fast_trace_bvh) {
      accelDesc.usage |= (MTLAccelerationStructureUsageRefit |
                          MTLAccelerationStructureUsagePreferFastBuild);
    }

    MTLAccelerationStructureSizes accelSizes = [device
        accelerationStructureSizesWithDescriptor:accelDesc];
    id<MTLAccelerationStructure> accel_uncompressed = [device
        newAccelerationStructureWithSize:accelSizes.accelerationStructureSize];
    id<MTLBuffer> scratchBuf = [device newBufferWithLength:accelSizes.buildScratchBufferSize
                                                   options:MTLResourceStorageModePrivate];
    id<MTLBuffer> sizeBuf = [device newBufferWithLength:8 options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> accelCommands = [queue commandBuffer];
    id<MTLAccelerationStructureCommandEncoder> accelEnc =
        [accelCommands accelerationStructureCommandEncoder];
    if (refit) {
      [accelEnc refitAccelerationStructure:accel_struct
                                descriptor:accelDesc
                               destination:accel_uncompressed
                             scratchBuffer:scratchBuf
                       scratchBufferOffset:0];
    }
    else {
      [accelEnc buildAccelerationStructure:accel_uncompressed
                                descriptor:accelDesc
                             scratchBuffer:scratchBuf
                       scratchBufferOffset:0];
    }
    if (use_fast_trace_bvh) {
      [accelEnc writeCompactedAccelerationStructureSize:accel_uncompressed
                                               toBuffer:sizeBuf
                                                 offset:0
                                           sizeDataType:MTLDataTypeULong];
    }
    [accelEnc endEncoding];
    [accelCommands addCompletedHandler:^(id<MTLCommandBuffer> command_buffer) {
      /* free temp resources */
      [scratchBuf release];
      [aabbBuf release];

      if (use_fast_trace_bvh) {
        /* Compact the accel structure */
        uint64_t compressed_size = *(uint64_t *)sizeBuf.contents;

        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
          id<MTLCommandBuffer> accelCommands = [queue commandBuffer];
          id<MTLAccelerationStructureCommandEncoder> accelEnc =
              [accelCommands accelerationStructureCommandEncoder];
          id<MTLAccelerationStructure> accel = [device
              newAccelerationStructureWithSize:compressed_size];
          [accelEnc copyAndCompactAccelerationStructure:accel_uncompressed
                                toAccelerationStructure:accel];
          [accelEnc endEncoding];
          [accelCommands addCompletedHandler:^(id<MTLCommandBuffer> command_buffer) {
            uint64_t allocated_size = [accel allocatedSize];
            stats.mem_alloc(allocated_size);
            accel_struct = accel;
            [accel_uncompressed release];
            accel_struct_building = false;
          }];
          [accelCommands commit];
        });
      }
      else {
        /* set our acceleration structure to the uncompressed structure */
        accel_struct = accel_uncompressed;

        uint64_t allocated_size = [accel_struct allocatedSize];
        stats.mem_alloc(allocated_size);
        accel_struct_building = false;
      }
      [sizeBuf release];
    }];

    accel_struct_building = true;
    [accelCommands commit];
    return true;
  }
  return false;
}

bool BVHMetal::build_BLAS(Progress &progress,
                          id<MTLDevice> device,
                          id<MTLCommandQueue> queue,
                          bool refit)
{
  if (@available(macos 12.0, *)) {
    assert(objects.size() == 1 && geometry.size() == 1);

    /* Build bottom level acceleration structures (BLAS) */
    Geometry *const geom = geometry[0];
    switch (geom->geometry_type) {
      case Geometry::VOLUME:
      case Geometry::MESH:
        return build_BLAS_mesh(progress, device, queue, geom, refit);
      case Geometry::HAIR:
        return build_BLAS_hair(progress, device, queue, geom, refit);
      case Geometry::POINTCLOUD:
        return build_BLAS_pointcloud(progress, device, queue, geom, refit);
      default:
        return false;
    }
  }
  return false;
}

bool BVHMetal::build_TLAS(Progress &progress,
                          id<MTLDevice> device,
                          id<MTLCommandQueue> queue,
                          bool refit)
{
  if (@available(macos 12.0, *)) {

    /* we need to sync here and ensure that all BLAS have completed async generation by both GCD
     * and Metal */
    {
      __block bool complete_bvh = false;
      while (!complete_bvh) {
        dispatch_sync(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
          complete_bvh = true;
          for (Object *ob : objects) {
            /* Skip non-traceable objects */
            if (!ob->is_traceable())
              continue;

            Geometry const *geom = ob->get_geometry();
            BVHMetal const *blas = static_cast<BVHMetal const *>(geom->bvh);
            if (blas->accel_struct_building) {
              complete_bvh = false;

              /* We're likely waiting on a command buffer that's in flight to complete.
               * Queue up a command buffer and wait for it complete before checking the BLAS again
               */
              id<MTLCommandBuffer> command_buffer = [queue commandBuffer];
              [command_buffer commit];
              [command_buffer waitUntilCompleted];
              break;
            }
          }
        });
      }
    }

    uint32_t num_instances = 0;
    uint32_t num_motion_transforms = 0;
    for (Object *ob : objects) {
      /* Skip non-traceable objects */
      if (!ob->is_traceable())
        continue;
      num_instances++;

      if (ob->use_motion()) {
        num_motion_transforms += max((size_t)1, ob->get_motion().size());
      }
      else {
        num_motion_transforms++;
      }
    }

    if (num_instances == 0) {
      return false;
    }

    /*------------------------------------------------*/
    BVH_status("Building TLAS      | %7d instances", (int)num_instances);
    /*------------------------------------------------*/

    const bool use_fast_trace_bvh = (params.bvh_type == BVH_TYPE_STATIC);

    NSMutableArray *all_blas = [NSMutableArray array];
    unordered_map<BVHMetal const *, int> instance_mapping;

    /* Lambda function to build/retrieve the BLAS index mapping */
    auto get_blas_index = [&](BVHMetal const *blas) {
      auto it = instance_mapping.find(blas);
      if (it != instance_mapping.end()) {
        return it->second;
      }
      else {
        int blas_index = (int)[all_blas count];
        instance_mapping[blas] = blas_index;
        if (@available(macos 12.0, *)) {
          [all_blas addObject:blas->accel_struct];
        }
        return blas_index;
      }
    };

    MTLResourceOptions storage_mode;
    if (device.hasUnifiedMemory) {
      storage_mode = MTLResourceStorageModeShared;
    }
    else {
      storage_mode = MTLResourceStorageModeManaged;
    }

    size_t instance_size;
    if (motion_blur) {
      instance_size = sizeof(MTLAccelerationStructureMotionInstanceDescriptor);
    }
    else {
      instance_size = sizeof(MTLAccelerationStructureUserIDInstanceDescriptor);
    }

    /* Allocate a GPU buffer for the instance data and populate it */
    id<MTLBuffer> instanceBuf = [device newBufferWithLength:num_instances * instance_size
                                                    options:storage_mode];
    id<MTLBuffer> motion_transforms_buf = nil;
    MTLPackedFloat4x3 *motion_transforms = nullptr;
    if (motion_blur && num_motion_transforms) {
      motion_transforms_buf = [device
          newBufferWithLength:num_motion_transforms * sizeof(MTLPackedFloat4x3)
                      options:storage_mode];
      motion_transforms = (MTLPackedFloat4x3 *)motion_transforms_buf.contents;
    }

    uint32_t instance_index = 0;
    uint32_t motion_transform_index = 0;

    // allocate look up buffer for wost case scenario
    uint64_t count = objects.size();
    blas_lookup.resize(count);

    for (Object *ob : objects) {
      /* Skip non-traceable objects */
      if (!ob->is_traceable())
        continue;

      Geometry const *geom = ob->get_geometry();

      BVHMetal const *blas = static_cast<BVHMetal const *>(geom->bvh);
      uint32_t accel_struct_index = get_blas_index(blas);

      /* Add some of the object visibility bits to the mask.
       * __prim_visibility contains the combined visibility bits of all instances, so is not
       * reliable if they differ between instances.
       *
       * METAL_WIP: OptiX visibility mask can only contain 8 bits, so have to trade-off here
       * and select just a few important ones.
       */
      uint32_t mask = ob->visibility_for_tracing() & 0xFF;

      /* Have to have at least one bit in the mask, or else instance would always be culled. */
      if (0 == mask) {
        mask = 0xFF;
      }

      /* Set user instance ID to object index */
      int object_index = ob->get_device_index();
      uint32_t user_id = uint32_t(object_index);
      int currIndex = instance_index++;
      assert(user_id < blas_lookup.size());
      blas_lookup[user_id] = accel_struct_index;

      /* Bake into the appropriate descriptor */
      if (motion_blur) {
        MTLAccelerationStructureMotionInstanceDescriptor *instances =
            (MTLAccelerationStructureMotionInstanceDescriptor *)[instanceBuf contents];
        MTLAccelerationStructureMotionInstanceDescriptor &desc = instances[currIndex];

        desc.accelerationStructureIndex = accel_struct_index;
        desc.userID = user_id;
        desc.mask = mask;
        desc.motionStartTime = 0.0f;
        desc.motionEndTime = 1.0f;
        desc.motionTransformsStartIndex = motion_transform_index;
        desc.motionStartBorderMode = MTLMotionBorderModeVanish;
        desc.motionEndBorderMode = MTLMotionBorderModeVanish;
        desc.intersectionFunctionTableOffset = 0;

        int key_count = ob->get_motion().size();
        if (key_count) {
          desc.motionTransformsCount = key_count;

          Transform *keys = ob->get_motion().data();
          for (int i = 0; i < key_count; i++) {
            float *t = (float *)&motion_transforms[motion_transform_index++];
            /* Transpose transform */
            auto src = (float const *)&keys[i];
            for (int i = 0; i < 12; i++) {
              t[i] = src[(i / 3) + 4 * (i % 3)];
            }
          }
        }
        else {
          desc.motionTransformsCount = 1;

          float *t = (float *)&motion_transforms[motion_transform_index++];
          if (ob->get_geometry()->is_instanced()) {
            /* Transpose transform */
            auto src = (float const *)&ob->get_tfm();
            for (int i = 0; i < 12; i++) {
              t[i] = src[(i / 3) + 4 * (i % 3)];
            }
          }
          else {
            /* Clear transform to identity matrix */
            t[0] = t[4] = t[8] = 1.0f;
          }
        }
      }
      else {
        MTLAccelerationStructureUserIDInstanceDescriptor *instances =
            (MTLAccelerationStructureUserIDInstanceDescriptor *)[instanceBuf contents];
        MTLAccelerationStructureUserIDInstanceDescriptor &desc = instances[currIndex];

        desc.accelerationStructureIndex = accel_struct_index;
        desc.userID = user_id;
        desc.mask = mask;
        desc.intersectionFunctionTableOffset = 0;

        float *t = (float *)&desc.transformationMatrix;
        if (ob->get_geometry()->is_instanced()) {
          /* Transpose transform */
          auto src = (float const *)&ob->get_tfm();
          for (int i = 0; i < 12; i++) {
            t[i] = src[(i / 3) + 4 * (i % 3)];
          }
        }
        else {
          /* Clear transform to identity matrix */
          t[0] = t[4] = t[8] = 1.0f;
        }
      }
    }

    if (storage_mode == MTLResourceStorageModeManaged) {
      [instanceBuf didModifyRange:NSMakeRange(0, instanceBuf.length)];
      if (motion_transforms_buf) {
        [motion_transforms_buf didModifyRange:NSMakeRange(0, motion_transforms_buf.length)];
        assert(num_motion_transforms == motion_transform_index);
      }
    }

    MTLInstanceAccelerationStructureDescriptor *accelDesc =
        [MTLInstanceAccelerationStructureDescriptor descriptor];
    accelDesc.instanceCount = num_instances;
    accelDesc.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeUserID;
    accelDesc.instanceDescriptorBuffer = instanceBuf;
    accelDesc.instanceDescriptorBufferOffset = 0;
    accelDesc.instanceDescriptorStride = instance_size;
    accelDesc.instancedAccelerationStructures = all_blas;

    if (motion_blur) {
      accelDesc.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeMotion;
      accelDesc.motionTransformBuffer = motion_transforms_buf;
      accelDesc.motionTransformCount = num_motion_transforms;
    }

    if (!use_fast_trace_bvh) {
      accelDesc.usage |= (MTLAccelerationStructureUsageRefit |
                          MTLAccelerationStructureUsagePreferFastBuild);
    }

    MTLAccelerationStructureSizes accelSizes = [device
        accelerationStructureSizesWithDescriptor:accelDesc];
    id<MTLAccelerationStructure> accel = [device
        newAccelerationStructureWithSize:accelSizes.accelerationStructureSize];
    id<MTLBuffer> scratchBuf = [device newBufferWithLength:accelSizes.buildScratchBufferSize
                                                   options:MTLResourceStorageModePrivate];
    id<MTLCommandBuffer> accelCommands = [queue commandBuffer];
    id<MTLAccelerationStructureCommandEncoder> accelEnc =
        [accelCommands accelerationStructureCommandEncoder];
    if (refit) {
      [accelEnc refitAccelerationStructure:accel_struct
                                descriptor:accelDesc
                               destination:accel
                             scratchBuffer:scratchBuf
                       scratchBufferOffset:0];
    }
    else {
      [accelEnc buildAccelerationStructure:accel
                                descriptor:accelDesc
                             scratchBuffer:scratchBuf
                       scratchBufferOffset:0];
    }
    [accelEnc endEncoding];
    [accelCommands commit];
    [accelCommands waitUntilCompleted];

    if (motion_transforms_buf) {
      [motion_transforms_buf release];
    }
    [instanceBuf release];
    [scratchBuf release];

    uint64_t allocated_size = [accel allocatedSize];
    stats.mem_alloc(allocated_size);

    /* Cache top and bottom-level acceleration structs */
    accel_struct = accel;
    blas_array.clear();
    blas_array.reserve(all_blas.count);
    for (id<MTLAccelerationStructure> blas in all_blas) {
      blas_array.push_back(blas);
    }

    return true;
  }
  return false;
}

bool BVHMetal::build(Progress &progress,
                     id<MTLDevice> device,
                     id<MTLCommandQueue> queue,
                     bool refit)
{
  if (@available(macos 12.0, *)) {
    if (refit && params.bvh_type != BVH_TYPE_STATIC) {
      assert(accel_struct);
    }
    else {
      if (accel_struct) {
        stats.mem_free(accel_struct.allocatedSize);
        [accel_struct release];
        accel_struct = nil;
      }
    }
  }

  if (!params.top_level) {
    return build_BLAS(progress, device, queue, refit);
  }
  else {
    return build_TLAS(progress, device, queue, refit);
  }
}

CCL_NAMESPACE_END

#endif /* WITH_METAL */
