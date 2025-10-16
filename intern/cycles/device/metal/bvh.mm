/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_METAL

#  include <algorithm>
#  include <chrono>
#  include <thread>
#  include <vector>

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
      metal_printf("%s", str.c_str()); \
    }

// #  define BVH_THROTTLE_DIAGNOSTICS
#  ifdef BVH_THROTTLE_DIAGNOSTICS
#    define bvh_throttle_printf(...) printf("BVHMetalBuildThrottler::" __VA_ARGS__)
#  else
#    define bvh_throttle_printf(...)
#  endif

/* This flag didn't exist until Xcode 26.0, so we ensure that it is defined for
 * forward-compatibility.
 */
#  ifndef MAC_OS_VERSION_26_0
#    define MTLAccelerationStructureUsagePreferFastIntersection \
      MTLAccelerationStructureUsage(1 << 4)
#  endif

/* Limit the number of concurrent BVH builds so that we don't approach unsafe GPU working set
 * sizes. */
struct BVHMetalBuildThrottler {
  thread_mutex mutex;
  size_t wired_memory = 0;
  size_t safe_wired_limit = 0;
  int requests_in_flight = 0;

  BVHMetalBuildThrottler()
  {
    /* The default device will always be the one that supports MetalRT if the machine supports it.
     */
    id<MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();

    /* Set a conservative limit, but which will still only throttle in extreme cases. */
    safe_wired_limit = [mtlDevice recommendedMaxWorkingSetSize] / 4;
    bvh_throttle_printf("safe_wired_limit = %zu\n", safe_wired_limit);
  }

  /* Block until we're safely able to wire the requested resources. */
  void acquire(const size_t bytes_to_be_wired)
  {
    bool throttled = false;
    while (true) {
      {
        thread_scoped_lock lock(mutex);

        /* Always allow a BVH build to proceed if no other is in flight, otherwise
         * only proceed if we're within safe limits. */
        if (wired_memory == 0 || wired_memory + bytes_to_be_wired <= safe_wired_limit) {
          wired_memory += bytes_to_be_wired;
          requests_in_flight += 1;
          bvh_throttle_printf("acquire -- success (requests_in_flight = %d, wired_memory = %zu)\n",
                              requests_in_flight,
                              wired_memory);
          return;
        }

        if (!throttled) {
          bvh_throttle_printf(
              "acquire -- throttling (requests_in_flight = %d, wired_memory = %zu, "
              "bytes_to_be_wired = %zu)\n",
              requests_in_flight,
              wired_memory,
              bytes_to_be_wired);
        }
        throttled = true;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  /* Notify of resources that have stopped being wired. */
  void release(const size_t bytes_just_unwired)
  {
    thread_scoped_lock lock(mutex);
    wired_memory -= bytes_just_unwired;
    requests_in_flight -= 1;
    bvh_throttle_printf("release (requests_in_flight = %d, wired_memory = %zu)\n",
                        requests_in_flight,
                        wired_memory);
  }

  /* Wait for all outstanding work to finish. */
  void wait_for_all()
  {
    while (true) {
      {
        thread_scoped_lock lock(mutex);
        if (wired_memory == 0) {
          return;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
} g_bvh_build_throttler;

/* macOS 15.2 and 15.3 has a bug in the dynamic BVH refitting which leads to missing geometry
 * during render. The issue is fixed in the macOS 15.4, until then disable refitting even for
 * the viewport.
 * Note that dynamic BVH is still used on the scene level to speed up updates of instances and
 * such. #132782. */
static bool support_refit_blas()
{
  if (@available(macos 15.4, *)) {
    return true;
  }
  if (@available(macos 15.2, *)) {
    return false;
  }
  return true;
}

BVHMetal::BVHMetal(const BVHParams &params_,
                   const vector<Geometry *> &geometry_,
                   const vector<Object *> &objects_,
                   Device *device)
    : BVH(params_, geometry_, objects_), device(device)
{
}

BVHMetal::~BVHMetal()
{
  if (@available(macos 12.0, *)) {
    set_accel_struct(nil);
    if (null_BLAS) {
      [null_BLAS release];
    }
  }
}

API_AVAILABLE(macos(11.0))
void BVHMetal::set_accel_struct(id<MTLAccelerationStructure> new_accel_struct)
{
  if (@available(macos 12.0, *)) {
    if (accel_struct) {
      device->stats.mem_free(accel_struct.allocatedSize);
      [accel_struct release];
      accel_struct = nil;
    }

    if (new_accel_struct) {
      accel_struct = new_accel_struct;
      device->stats.mem_alloc(accel_struct.allocatedSize);
    }
  }
}

bool BVHMetal::build_BLAS_mesh(Progress &progress,
                               id<MTLDevice> mtl_device,
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

    const bool use_fast_trace_bvh = (params.bvh_type == BVH_TYPE_STATIC) || !support_refit_blas();

    const array<float3> &verts = mesh->get_verts();
    const array<int> &tris = mesh->get_triangles();
    const size_t num_verts = verts.size();
    const size_t num_indices = tris.size();

    size_t num_motion_steps = 1;
    Attribute *motion_keys = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (motion_blur && mesh->get_use_motion_blur() && motion_keys) {
      num_motion_steps = mesh->get_motion_steps();
    }

    /* Upload the mesh data to the GPU */
    id<MTLBuffer> posBuf = nil;
    id<MTLBuffer> indexBuf = [mtl_device newBufferWithBytes:tris.data()
                                                     length:num_indices * sizeof(tris.data()[0])
                                                    options:MTLResourceStorageModeShared];

    if (num_motion_steps == 1) {
      posBuf = [mtl_device newBufferWithBytes:verts.data()
                                       length:num_verts * sizeof(verts.data()[0])
                                      options:MTLResourceStorageModeShared];
    }
    else {
      posBuf = [mtl_device
          newBufferWithLength:num_verts * num_motion_steps * sizeof(verts.data()[0])
                      options:MTLResourceStorageModeShared];
      float3 *dest_data = (float3 *)[posBuf contents];
      size_t center_step = (num_motion_steps - 1) / 2;
      for (size_t step = 0; step < num_motion_steps; ++step) {
        const float3 *verts = mesh->get_verts().data();

        /* The center step for motion vertices is not stored in the attribute. */
        if (step != center_step) {
          verts = motion_keys->data_float3() + (step > center_step ? step - 1 : step) * num_verts;
        }
        std::copy_n(verts, num_verts, dest_data + num_verts * step);
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
      geomDescMotion.opaque = true;

      geomDesc = geomDescMotion;

      BVH_status("Building motion mesh BLAS | %7d tris | %s | %7d motion keyframes",
                 (int)mesh->num_triangles(),
                 geom->name.c_str(),
                 (int)num_motion_steps);
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
      geomDescNoMotion.opaque = true;

      geomDesc = geomDescNoMotion;

      BVH_status(
          "Building mesh BLAS | %7d tris | %s", (int)mesh->num_triangles(), geom->name.c_str());
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
    if (extended_limits) {
      accelDesc.usage |= MTLAccelerationStructureUsageExtendedLimits;
    }

    if (!use_fast_trace_bvh) {
      accelDesc.usage |= (MTLAccelerationStructureUsageRefit |
                          MTLAccelerationStructureUsagePreferFastBuild);
    }
    else if (@available(macos 26.0, *)) {
      accelDesc.usage |= MTLAccelerationStructureUsagePreferFastIntersection;
    }

    MTLAccelerationStructureSizes accelSizes = [mtl_device
        accelerationStructureSizesWithDescriptor:accelDesc];
    id<MTLAccelerationStructure> accel_uncompressed = [mtl_device
        newAccelerationStructureWithSize:accelSizes.accelerationStructureSize];
    id<MTLBuffer> scratchBuf = [mtl_device newBufferWithLength:accelSizes.buildScratchBufferSize
                                                       options:MTLResourceStorageModePrivate];
    id<MTLBuffer> sizeBuf = [mtl_device newBufferWithLength:8
                                                    options:MTLResourceStorageModeShared];
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

    /* Estimated size of resources that will be wired for the GPU accelerated build.
     * Acceleration-struct size is doubled to account for possible compaction step. */
    size_t wired_size = posBuf.allocatedSize + indexBuf.allocatedSize + scratchBuf.allocatedSize +
                        accel_uncompressed.allocatedSize * 2;

    [accelCommands addCompletedHandler:^(id<MTLCommandBuffer> /*command_buffer*/) {
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
          id<MTLAccelerationStructure> accel = [mtl_device
              newAccelerationStructureWithSize:compressed_size];
          [accelEnc copyAndCompactAccelerationStructure:accel_uncompressed
                                toAccelerationStructure:accel];
          [accelEnc endEncoding];
          [accelCommands addCompletedHandler:^(id<MTLCommandBuffer> /*command_buffer*/) {
            set_accel_struct(accel);
            [accel_uncompressed release];

            /* Signal that we've finished doing GPU acceleration struct build. */
            g_bvh_build_throttler.release(wired_size);
          }];
          [accelCommands commit];
        });
      }
      else {
        /* set our acceleration structure to the uncompressed structure */
        set_accel_struct(accel_uncompressed);

        /* Signal that we've finished doing GPU acceleration struct build. */
        g_bvh_build_throttler.release(wired_size);
      }

      [sizeBuf release];
    }];

    /* Wait until it's safe to proceed with GPU acceleration struct build. */
    g_bvh_build_throttler.acquire(wired_size);
    [accelCommands commit];

    return true;
  }
  return false;
}

bool BVHMetal::build_BLAS_hair(Progress &progress,
                               id<MTLDevice> mtl_device,
                               id<MTLCommandQueue> queue,
                               Geometry *const geom,
                               bool refit)
{
#  if defined(MAC_OS_VERSION_14_0)
  if (@available(macos 14.0, *)) {
    /* Build BLAS for hair curves */
    Hair *hair = static_cast<Hair *>(geom);
    if (hair->num_curves() == 0) {
      return false;
    }

    const bool use_fast_trace_bvh = (params.bvh_type == BVH_TYPE_STATIC) || !support_refit_blas();

    size_t num_motion_steps = 1;
    Attribute *motion_keys = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (motion_blur && hair->get_use_motion_blur() && motion_keys) {
      num_motion_steps = hair->get_motion_steps();
    }

    id<MTLBuffer> cpBuffer = nil;
    id<MTLBuffer> radiusBuffer = nil;
    id<MTLBuffer> idxBuffer = nil;

    MTLAccelerationStructureGeometryDescriptor *geomDesc;
    if (num_motion_steps > 1) {
      MTLAccelerationStructureMotionCurveGeometryDescriptor *geomDescCrv =
          [MTLAccelerationStructureMotionCurveGeometryDescriptor descriptor];

      uint64_t numKeys = hair->num_keys();
      uint64_t numCurves = hair->num_curves();
      const array<float> &radiuses = hair->get_curve_radius();

      /* Gather the curve geometry. */
      std::vector<float3> cpData;
      std::vector<int> idxData;
      std::vector<float> radiusData;
      cpData.reserve(numKeys);
      radiusData.reserve(numKeys);

      std::vector<int> step_offsets;
      for (size_t step = 0; step < num_motion_steps; ++step) {

        /* The center step for motion vertices is not stored in the attribute. */
        const float3 *keys = hair->get_curve_keys().data();
        size_t center_step = (num_motion_steps - 1) / 2;
        if (step != center_step) {
          size_t attr_offset = (step > center_step) ? step - 1 : step;
          /* Technically this is a float4 array, but sizeof(float3) == sizeof(float4). */
          keys = motion_keys->data_float3() + attr_offset * numKeys;
        }

        step_offsets.push_back(cpData.size());

        for (int c = 0; c < numCurves; ++c) {
          const Hair::Curve curve = hair->get_curve(c);
          int segCount = curve.num_segments();
          int firstKey = curve.first_key;
          uint64_t idxBase = cpData.size();
          if (hair->curve_shape != CURVE_THICK_LINEAR) {
            cpData.push_back(keys[firstKey]);
            radiusData.push_back(radiuses[firstKey]);
          }
          for (int s = 0; s < segCount; ++s) {
            if (step == 0) {
              idxData.push_back(idxBase + s);
            }
            cpData.push_back(keys[firstKey + s]);
            radiusData.push_back(radiuses[firstKey + s]);
          }
          cpData.push_back(keys[firstKey + curve.num_keys - 1]);
          radiusData.push_back(radiuses[firstKey + curve.num_keys - 1]);
          if (hair->curve_shape != CURVE_THICK_LINEAR) {
            cpData.push_back(keys[firstKey + curve.num_keys - 1]);
            radiusData.push_back(radiuses[firstKey + curve.num_keys - 1]);
          }
        }
      }

      /* Allocate and populate MTLBuffers for geometry. */
      idxBuffer = [mtl_device newBufferWithBytes:idxData.data()
                                          length:idxData.size() * sizeof(int)
                                         options:MTLResourceStorageModeShared];

      cpBuffer = [mtl_device newBufferWithBytes:cpData.data()
                                         length:cpData.size() * sizeof(float3)
                                        options:MTLResourceStorageModeShared];

      radiusBuffer = [mtl_device newBufferWithBytes:radiusData.data()
                                             length:radiusData.size() * sizeof(float)
                                            options:MTLResourceStorageModeShared];

      std::vector<MTLMotionKeyframeData *> cp_ptrs;
      std::vector<MTLMotionKeyframeData *> radius_ptrs;
      cp_ptrs.reserve(num_motion_steps);
      radius_ptrs.reserve(num_motion_steps);

      for (size_t step = 0; step < num_motion_steps; ++step) {
        MTLMotionKeyframeData *k = [MTLMotionKeyframeData data];
        k.buffer = cpBuffer;
        k.offset = step_offsets[step] * sizeof(float3);
        cp_ptrs.push_back(k);

        k = [MTLMotionKeyframeData data];
        k.buffer = radiusBuffer;
        k.offset = step_offsets[step] * sizeof(float);
        radius_ptrs.push_back(k);
      }

      geomDescCrv.controlPointBuffers = [NSArray arrayWithObjects:cp_ptrs.data()
                                                            count:cp_ptrs.size()];
      geomDescCrv.radiusBuffers = [NSArray arrayWithObjects:radius_ptrs.data()
                                                      count:radius_ptrs.size()];

      /* controlPointCount should specify the *per-step* control point count. */
      geomDescCrv.controlPointCount = cpData.size() / num_motion_steps;
      geomDescCrv.controlPointStride = sizeof(float3);
      geomDescCrv.controlPointFormat = MTLAttributeFormatFloat3;
      geomDescCrv.radiusStride = sizeof(float);
      geomDescCrv.radiusFormat = MTLAttributeFormatFloat;
      geomDescCrv.segmentCount = idxData.size();
      geomDescCrv.segmentControlPointCount = (hair->curve_shape == CURVE_THICK_LINEAR) ? 2 : 4;
      geomDescCrv.curveType = (hair->curve_shape == CURVE_RIBBON) ? MTLCurveTypeFlat :
                                                                    MTLCurveTypeRound;
      if (hair->curve_shape == CURVE_THICK_LINEAR) {
        geomDescCrv.curveBasis = MTLCurveBasisLinear;
        geomDescCrv.curveEndCaps = MTLCurveEndCapsSphere;
      }
      else {
        geomDescCrv.curveBasis = MTLCurveBasisCatmullRom;
        geomDescCrv.curveEndCaps = MTLCurveEndCapsDisk;
      }
      geomDescCrv.indexType = MTLIndexTypeUInt32;
      geomDescCrv.indexBuffer = idxBuffer;
      geomDescCrv.intersectionFunctionTableOffset = 1;

      /* Force a single any-hit call, so shadow record-all behavior works correctly */
      /* (Match optix behavior: unsigned int build_flags =
       * OPTIX_GEOMETRY_FLAG_REQUIRE_SINGLE_ANYHIT_CALL;) */
      geomDescCrv.allowDuplicateIntersectionFunctionInvocation = false;
      geomDescCrv.opaque = true;
      geomDesc = geomDescCrv;
    }
    else {
      MTLAccelerationStructureCurveGeometryDescriptor *geomDescCrv =
          [MTLAccelerationStructureCurveGeometryDescriptor descriptor];

      uint64_t numKeys = hair->num_keys();
      uint64_t numCurves = hair->num_curves();
      const array<float> &radiuses = hair->get_curve_radius();

      /* Gather the curve geometry. */
      std::vector<float3> cpData;
      std::vector<int> idxData;
      std::vector<float> radiusData;
      cpData.reserve(numKeys);
      radiusData.reserve(numKeys);
      auto keys = hair->get_curve_keys();
      for (int c = 0; c < numCurves; ++c) {
        const Hair::Curve curve = hair->get_curve(c);
        int segCount = curve.num_segments();
        int firstKey = curve.first_key;
        uint64_t idxBase = cpData.size();
        if (hair->curve_shape != CURVE_THICK_LINEAR) {
          cpData.push_back(keys[firstKey]);
          radiusData.push_back(radiuses[firstKey]);
        }
        for (int s = 0; s < segCount; ++s) {
          idxData.push_back(idxBase + s);
          cpData.push_back(keys[firstKey + s]);
          radiusData.push_back(radiuses[firstKey + s]);
        }
        cpData.push_back(keys[firstKey + curve.num_keys - 1]);
        radiusData.push_back(radiuses[firstKey + curve.num_keys - 1]);
        if (hair->curve_shape != CURVE_THICK_LINEAR) {
          cpData.push_back(keys[firstKey + curve.num_keys - 1]);
          radiusData.push_back(radiuses[firstKey + curve.num_keys - 1]);
        }
      }

      /* Allocate and populate MTLBuffers for geometry. */
      idxBuffer = [mtl_device newBufferWithBytes:idxData.data()
                                          length:idxData.size() * sizeof(int)
                                         options:MTLResourceStorageModeShared];

      cpBuffer = [mtl_device newBufferWithBytes:cpData.data()
                                         length:cpData.size() * sizeof(float3)
                                        options:MTLResourceStorageModeShared];

      radiusBuffer = [mtl_device newBufferWithBytes:radiusData.data()
                                             length:radiusData.size() * sizeof(float)
                                            options:MTLResourceStorageModeShared];

      geomDescCrv.controlPointBuffer = cpBuffer;
      geomDescCrv.radiusBuffer = radiusBuffer;
      geomDescCrv.controlPointCount = cpData.size();
      geomDescCrv.controlPointStride = sizeof(float3);
      geomDescCrv.controlPointFormat = MTLAttributeFormatFloat3;
      geomDescCrv.controlPointBufferOffset = 0;
      geomDescCrv.segmentCount = idxData.size();
      geomDescCrv.segmentControlPointCount = (hair->curve_shape == CURVE_THICK_LINEAR) ? 2 : 4;
      geomDescCrv.curveType = (hair->curve_shape == CURVE_RIBBON) ? MTLCurveTypeFlat :
                                                                    MTLCurveTypeRound;
      if (hair->curve_shape == CURVE_THICK_LINEAR) {
        geomDescCrv.curveBasis = MTLCurveBasisLinear;
        geomDescCrv.curveEndCaps = MTLCurveEndCapsSphere;
      }
      else {
        geomDescCrv.curveBasis = MTLCurveBasisCatmullRom;
        geomDescCrv.curveEndCaps = MTLCurveEndCapsDisk;
      }
      geomDescCrv.indexType = MTLIndexTypeUInt32;
      geomDescCrv.indexBuffer = idxBuffer;
      geomDescCrv.intersectionFunctionTableOffset = 1;

      /* Force a single any-hit call, so shadow record-all behavior works correctly */
      /* (Match optix behavior: unsigned int build_flags =
       * OPTIX_GEOMETRY_FLAG_REQUIRE_SINGLE_ANYHIT_CALL;) */
      geomDescCrv.allowDuplicateIntersectionFunctionInvocation = false;
      geomDescCrv.opaque = true;
      geomDesc = geomDescCrv;
    }

    MTLPrimitiveAccelerationStructureDescriptor *accelDesc =
        [MTLPrimitiveAccelerationStructureDescriptor descriptor];
    accelDesc.geometryDescriptors = @[ geomDesc ];

    if (num_motion_steps > 1) {
      accelDesc.motionStartTime = 0.0f;
      accelDesc.motionEndTime = 1.0f;
      accelDesc.motionStartBorderMode = MTLMotionBorderModeVanish;
      accelDesc.motionEndBorderMode = MTLMotionBorderModeVanish;
      accelDesc.motionKeyframeCount = num_motion_steps;

      BVH_status("Building motion hair BLAS | %7d curves | %s | %7d motion keyframes",
                 (int)hair->num_curves(),
                 geom->name.c_str(),
                 (int)num_motion_steps);
    }
    else {
      BVH_status(
          "Building hair BLAS | %7d curves | %s", (int)hair->num_curves(), geom->name.c_str());
    }

    if (extended_limits) {
      accelDesc.usage |= MTLAccelerationStructureUsageExtendedLimits;
    }

    if (!use_fast_trace_bvh) {
      accelDesc.usage |= (MTLAccelerationStructureUsageRefit |
                          MTLAccelerationStructureUsagePreferFastBuild);
    }
    else if (@available(macos 26.0, *)) {
      accelDesc.usage |= MTLAccelerationStructureUsagePreferFastIntersection;
    }

    MTLAccelerationStructureSizes accelSizes = [mtl_device
        accelerationStructureSizesWithDescriptor:accelDesc];
    id<MTLAccelerationStructure> accel_uncompressed = [mtl_device
        newAccelerationStructureWithSize:accelSizes.accelerationStructureSize];
    id<MTLBuffer> scratchBuf = [mtl_device newBufferWithLength:accelSizes.buildScratchBufferSize
                                                       options:MTLResourceStorageModePrivate];
    id<MTLBuffer> sizeBuf = [mtl_device newBufferWithLength:8
                                                    options:MTLResourceStorageModeShared];
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

    /* Estimated size of resources that will be wired for the GPU accelerated build.
     * Acceleration-struct size is doubled to account for possible compaction step. */
    size_t wired_size = cpBuffer.allocatedSize + radiusBuffer.allocatedSize +
                        idxBuffer.allocatedSize + scratchBuf.allocatedSize +
                        accel_uncompressed.allocatedSize * 2;

    [accelCommands addCompletedHandler:^(id<MTLCommandBuffer> /*command_buffer*/) {
      /* free temp resources */
      [scratchBuf release];
      [cpBuffer release];
      [radiusBuffer release];
      [idxBuffer release];

      if (use_fast_trace_bvh) {
        uint64_t compressed_size = *(uint64_t *)sizeBuf.contents;

        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
          id<MTLCommandBuffer> accelCommands = [queue commandBuffer];
          id<MTLAccelerationStructureCommandEncoder> accelEnc =
              [accelCommands accelerationStructureCommandEncoder];
          id<MTLAccelerationStructure> accel = [mtl_device
              newAccelerationStructureWithSize:compressed_size];
          [accelEnc copyAndCompactAccelerationStructure:accel_uncompressed
                                toAccelerationStructure:accel];
          [accelEnc endEncoding];
          [accelCommands addCompletedHandler:^(id<MTLCommandBuffer> /*command_buffer*/) {
            set_accel_struct(accel);
            [accel_uncompressed release];

            /* Signal that we've finished doing GPU acceleration struct build. */
            g_bvh_build_throttler.release(wired_size);
          }];
          [accelCommands commit];
        });
      }
      else {
        /* set our acceleration structure to the uncompressed structure */
        set_accel_struct(accel_uncompressed);

        /* Signal that we've finished doing GPU acceleration struct build. */
        g_bvh_build_throttler.release(wired_size);
      }

      [sizeBuf release];
    }];

    /* Wait until it's safe to proceed with GPU acceleration struct build. */
    g_bvh_build_throttler.acquire(wired_size);
    [accelCommands commit];

    return true;
  }
#  else  /* MAC_OS_VERSION_14_0 */
  (void)progress;
  (void)mtl_device;
  (void)queue;
  (void)geom;
  (void)(refit);
#  endif /* MAC_OS_VERSION_14_0 */
  return false;
}

bool BVHMetal::build_BLAS_pointcloud(Progress &progress,
                                     id<MTLDevice> mtl_device,
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

    const size_t num_points = pointcloud->get_points().size();
    const float3 *points = pointcloud->get_points().data();
    const float *radius = pointcloud->get_radius().data();

    const bool use_fast_trace_bvh = (params.bvh_type == BVH_TYPE_STATIC) || !support_refit_blas();

    size_t num_motion_steps = 1;
    Attribute *motion_keys = pointcloud->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (motion_blur && pointcloud->get_use_motion_blur() && motion_keys) {
      num_motion_steps = pointcloud->get_motion_steps();
    }

    const size_t num_aabbs = num_motion_steps * num_points;

    /* Allocate a GPU buffer for the AABB data and populate it */
    id<MTLBuffer> aabbBuf = [mtl_device
        newBufferWithLength:num_aabbs * sizeof(MTLAxisAlignedBoundingBox)
                    options:MTLResourceStorageModeShared];
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

    MTLAccelerationStructureGeometryDescriptor *geomDesc;
    if (num_motion_steps > 1) {
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

    if (num_motion_steps > 1) {
      accelDesc.motionStartTime = 0.0f;
      accelDesc.motionEndTime = 1.0f;
      //      accelDesc.motionStartBorderMode = MTLMotionBorderModeVanish;
      //      accelDesc.motionEndBorderMode = MTLMotionBorderModeVanish;
      accelDesc.motionKeyframeCount = num_motion_steps;

      BVH_status("Building motion pointcloud BLAS | %7d points | %s | %7d motion keyframes",
                 (int)pointcloud->num_points(),
                 geom->name.c_str(),
                 (int)num_motion_steps);
    }
    else {
      BVH_status("Building pointcloud BLAS | %7d points | %s",
                 (int)pointcloud->num_points(),
                 geom->name.c_str());
    }
    if (extended_limits) {
      accelDesc.usage |= MTLAccelerationStructureUsageExtendedLimits;
    }

    if (!use_fast_trace_bvh) {
      accelDesc.usage |= (MTLAccelerationStructureUsageRefit |
                          MTLAccelerationStructureUsagePreferFastBuild);
    }
    else if (@available(macos 26.0, *)) {
      accelDesc.usage |= MTLAccelerationStructureUsagePreferFastIntersection;
    }

    MTLAccelerationStructureSizes accelSizes = [mtl_device
        accelerationStructureSizesWithDescriptor:accelDesc];
    id<MTLAccelerationStructure> accel_uncompressed = [mtl_device
        newAccelerationStructureWithSize:accelSizes.accelerationStructureSize];
    id<MTLBuffer> scratchBuf = [mtl_device newBufferWithLength:accelSizes.buildScratchBufferSize
                                                       options:MTLResourceStorageModePrivate];
    id<MTLBuffer> sizeBuf = [mtl_device newBufferWithLength:8
                                                    options:MTLResourceStorageModeShared];
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

    /* Estimated size of resources that will be wired for the GPU accelerated build.
     * Acceleration-struct size is doubled to account for possible compaction step. */
    size_t wired_size = aabbBuf.allocatedSize + scratchBuf.allocatedSize +
                        accel_uncompressed.allocatedSize * 2;

    [accelCommands addCompletedHandler:^(id<MTLCommandBuffer> /*command_buffer*/) {
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
          id<MTLAccelerationStructure> accel = [mtl_device
              newAccelerationStructureWithSize:compressed_size];
          [accelEnc copyAndCompactAccelerationStructure:accel_uncompressed
                                toAccelerationStructure:accel];
          [accelEnc endEncoding];
          [accelCommands addCompletedHandler:^(id<MTLCommandBuffer> /*command_buffer*/) {
            set_accel_struct(accel);
            [accel_uncompressed release];

            /* Signal that we've finished doing GPU acceleration struct build. */
            g_bvh_build_throttler.release(wired_size);
          }];
          [accelCommands commit];
        });
      }
      else {
        /* set our acceleration structure to the uncompressed structure */
        set_accel_struct(accel_uncompressed);

        /* Signal that we've finished doing GPU acceleration struct build. */
        g_bvh_build_throttler.release(wired_size);
      }

      [sizeBuf release];
    }];

    /* Wait until it's safe to proceed with GPU acceleration struct build. */
    g_bvh_build_throttler.acquire(wired_size);
    [accelCommands commit];
    return true;
  }
  return false;
}

bool BVHMetal::build_BLAS(Progress &progress,
                          id<MTLDevice> mtl_device,
                          id<MTLCommandQueue> queue,
                          bool refit)
{
  assert(objects.size() == 1 && geometry.size() == 1);

  /* Build bottom level acceleration structures (BLAS) */
  Geometry *const geom = geometry[0];
  switch (geom->geometry_type) {
    case Geometry::VOLUME:
    case Geometry::MESH:
      return build_BLAS_mesh(progress, mtl_device, queue, geom, refit);
    case Geometry::HAIR:
      return build_BLAS_hair(progress, mtl_device, queue, geom, refit);
    case Geometry::POINTCLOUD:
      return build_BLAS_pointcloud(progress, mtl_device, queue, geom, refit);
    default:
      return false;
  }
  return false;
}

#  if defined(MAC_OS_VERSION_15_0)

/* Return MTLComponentTransform from a DecomposedTransform. */
static MTLComponentTransform decomposed_to_component_transform(const DecomposedTransform &src)
{
  MTLComponentTransform tfm;
  tfm.scale = MTLPackedFloat3Make(src.y.w, src.z.w, src.w.w);
  tfm.shear = MTLPackedFloat3Make(src.z.x, src.z.y, src.w.x);
  tfm.pivot = MTLPackedFloat3Make(0.0f, 0.0f, 0.0f);
  tfm.rotation = MTLPackedFloatQuaternionMake(src.x.x, src.x.y, src.x.z, src.x.w);
  tfm.translation = MTLPackedFloat3Make(src.y.x, src.y.y, src.y.z);
  return tfm;
}

/* Return unit MTLComponentTransform. */
static MTLComponentTransform component_transform_make_unit()
{
  MTLComponentTransform tfm;
  tfm.scale = MTLPackedFloat3Make(1.0f, 1.0f, 1.0f);
  tfm.shear = MTLPackedFloat3Make(0.0f, 0.0f, 0.0f);
  tfm.pivot = MTLPackedFloat3Make(0.0f, 0.0f, 0.0f);
  tfm.rotation = MTLPackedFloatQuaternionMake(0.0f, 0.0f, 0.0f, 1.0f);
  tfm.translation = MTLPackedFloat3Make(0.0f, 0.0f, 0.0f);
  return tfm;
}

#  endif

bool BVHMetal::build_TLAS(Progress &progress,
                          id<MTLDevice> mtl_device,
                          id<MTLCommandQueue> queue,
                          bool refit)
{
  /* Wait for all BLAS builds to finish. */
  g_bvh_build_throttler.wait_for_all();

  if (@available(macos 12.0, *)) {
    /* Defined inside available check, for return type to be available. */
    auto make_null_BLAS = [this](id<MTLDevice> mtl_device,
                                 id<MTLCommandQueue> queue) -> id<MTLAccelerationStructure> {
      id<MTLBuffer> nullBuf = [mtl_device newBufferWithLength:sizeof(float3)
                                                      options:MTLResourceStorageModeShared];

      /* Create an acceleration structure. */
      MTLAccelerationStructureTriangleGeometryDescriptor *geomDesc =
          [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
      geomDesc.vertexBuffer = nullBuf;
      geomDesc.vertexBufferOffset = 0;
      geomDesc.vertexStride = sizeof(float3);
      geomDesc.indexBuffer = nullBuf;
      geomDesc.indexBufferOffset = 0;
      geomDesc.indexType = MTLIndexTypeUInt32;
      geomDesc.triangleCount = 0;
      geomDesc.intersectionFunctionTableOffset = 0;
      geomDesc.opaque = true;
      geomDesc.allowDuplicateIntersectionFunctionInvocation = false;

      MTLPrimitiveAccelerationStructureDescriptor *accelDesc =
          [MTLPrimitiveAccelerationStructureDescriptor descriptor];
      accelDesc.geometryDescriptors = @[ geomDesc ];
      if (extended_limits) {
        accelDesc.usage |= MTLAccelerationStructureUsageExtendedLimits;
      }

      MTLAccelerationStructureSizes accelSizes = [mtl_device
          accelerationStructureSizesWithDescriptor:accelDesc];
      id<MTLAccelerationStructure> accel_struct = [mtl_device
          newAccelerationStructureWithSize:accelSizes.accelerationStructureSize];
      id<MTLBuffer> scratchBuf = [mtl_device newBufferWithLength:accelSizes.buildScratchBufferSize
                                                         options:MTLResourceStorageModePrivate];
      id<MTLBuffer> sizeBuf = [mtl_device newBufferWithLength:8
                                                      options:MTLResourceStorageModeShared];
      id<MTLCommandBuffer> accelCommands = [queue commandBuffer];
      id<MTLAccelerationStructureCommandEncoder> accelEnc =
          [accelCommands accelerationStructureCommandEncoder];
      [accelEnc buildAccelerationStructure:accel_struct
                                descriptor:accelDesc
                             scratchBuffer:scratchBuf
                       scratchBufferOffset:0];
      [accelEnc endEncoding];
      [accelCommands commit];
      [accelCommands waitUntilCompleted];

      /* free temp resources */
      [scratchBuf release];
      [nullBuf release];
      [sizeBuf release];

      return accel_struct;
    };

    uint32_t num_instances = 0;
    uint32_t num_motion_transforms = 0;
    uint32_t num_motion_instances = 0;
    for (Object *ob : objects) {
      num_instances++;

      if (ob->use_motion()) {
        num_motion_transforms += max((size_t)1, ob->get_motion().size());
        num_motion_instances++;
      }
      else {
        num_motion_transforms++;
      }
    }

    const bool use_instance_motion = motion_blur && num_motion_instances;
    const bool use_fast_trace_bvh = (params.bvh_type == BVH_TYPE_STATIC) || !support_refit_blas();

    NSMutableArray *all_blas = [NSMutableArray array];
    unordered_map<const BVHMetal *, int> instance_mapping;

    /* Lambda function to build/retrieve the BLAS index mapping */
    auto get_blas_index = [&](const BVHMetal *blas) {
      auto it = instance_mapping.find(blas);
      if (it != instance_mapping.end()) {
        return it->second;
      }
      int blas_index = (int)[all_blas count];
      instance_mapping[blas] = blas_index;
      if (@available(macos 12.0, *)) {
        [all_blas addObject:(blas ? blas->accel_struct : null_BLAS)];
      }
      return blas_index;
    };

    size_t instance_size;
    if (use_instance_motion) {
      instance_size = sizeof(MTLAccelerationStructureMotionInstanceDescriptor);
    }
    else {
      instance_size = sizeof(MTLAccelerationStructureUserIDInstanceDescriptor);
    }

    /* Allocate a GPU buffer for the instance data and populate it */
    id<MTLBuffer> instanceBuf = [mtl_device newBufferWithLength:num_instances * instance_size
                                                        options:MTLResourceStorageModeShared];
    id<MTLBuffer> motion_transforms_buf = nil;
    MTLPackedFloat4x3 *matrix_motion_transforms = nullptr;
#  if defined(MAC_OS_VERSION_15_0)
    MTLComponentTransform *decomposed_motion_transforms = nullptr;
#  endif
    if (use_instance_motion && num_motion_transforms) {
#  if defined(MAC_OS_VERSION_15_0)
      if (use_pcmi) {
        if (@available(macos 15.0, *)) {
          motion_transforms_buf = [mtl_device
              newBufferWithLength:num_motion_transforms * sizeof(MTLComponentTransform)
                          options:MTLResourceStorageModeShared];
          decomposed_motion_transforms = (MTLComponentTransform *)motion_transforms_buf.contents;
        }
      }
      else
#  endif
      {
        motion_transforms_buf = [mtl_device
            newBufferWithLength:num_motion_transforms * sizeof(MTLPackedFloat4x3)
                        options:MTLResourceStorageModeShared];
        matrix_motion_transforms = (MTLPackedFloat4x3 *)motion_transforms_buf.contents;
      }
    }

    uint32_t instance_index = 0;
    uint32_t motion_transform_index = 0;

    blas_array.clear();
    blas_array.reserve(num_instances);

    for (Object *ob : objects) {
      /* Skip non-traceable objects */
      const Geometry *geom = ob->get_geometry();
      const BVHMetal *blas = static_cast<const BVHMetal *>(geom->bvh.get());
      if (!blas || !blas->accel_struct || !ob->is_traceable()) {
        /* Place a degenerate instance, to ensure [[instance_id]] equals ob->get_device_index()
         * in our intersection functions */
        blas = nullptr;

        /* Workaround for issue in macOS <= 14.1: Insert degenerate BLAS instead of zero-filling
         * the descriptor. */
        if (!null_BLAS) {
          null_BLAS = make_null_BLAS(mtl_device, queue);
        }
        blas_array.push_back(null_BLAS);
      }
      else {
        blas_array.push_back(blas->accel_struct);
      }

      uint32_t accel_struct_index = get_blas_index(blas);

      /* The MetalRT visibility mask can only contain 8 bits by default. */
      uint32_t mask = ob->visibility_for_tracing() & 0xFF;

      /* Have to have at least one bit in the mask, or else instance would always be culled. */
      if (0 == mask) {
        mask = 0xFF;
      }

      /* Set user instance ID to object index */
      uint32_t primitive_offset = 0;
      int currIndex = instance_index++;

      if (geom->is_hair()) {
        /* Build BLAS for curve primitives. */
        Hair *const hair = static_cast<Hair *const>(const_cast<Geometry *>(geom));
        primitive_offset = uint32_t(hair->curve_segment_offset);
      }
      else if (geom->is_mesh() || geom->is_volume()) {
        /* Build BLAS for triangle primitives. */
        Mesh *const mesh = static_cast<Mesh *const>(const_cast<Geometry *>(geom));
        primitive_offset = uint32_t(mesh->prim_offset);
      }
      else if (geom->is_pointcloud()) {
        /* Build BLAS for points primitives. */
        PointCloud *const pointcloud = static_cast<PointCloud *const>(
            const_cast<Geometry *>(geom));
        primitive_offset = uint32_t(pointcloud->prim_offset);
      }

      /* Bake into the appropriate descriptor */
      if (use_instance_motion) {
        MTLAccelerationStructureMotionInstanceDescriptor *instances =
            (MTLAccelerationStructureMotionInstanceDescriptor *)[instanceBuf contents];
        MTLAccelerationStructureMotionInstanceDescriptor &desc = instances[currIndex];

        desc.accelerationStructureIndex = accel_struct_index;
        desc.userID = primitive_offset;
        desc.mask = mask;
        desc.motionStartTime = 0.0f;
        desc.motionEndTime = 1.0f;
        desc.motionTransformsStartIndex = motion_transform_index;
        desc.motionStartBorderMode = MTLMotionBorderModeVanish;
        desc.motionEndBorderMode = MTLMotionBorderModeVanish;
        desc.intersectionFunctionTableOffset = 0;

        array<DecomposedTransform> decomp(ob->get_motion().size());
        transform_motion_decompose(
            decomp.data(), ob->get_motion().data(), ob->get_motion().size());

        int key_count = ob->get_motion().size();
        if (key_count) {
          desc.motionTransformsCount = key_count;

#  if defined(MAC_OS_VERSION_15_0)
          if (use_pcmi) {
            for (int i = 0; i < key_count; i++) {
              decomposed_motion_transforms[motion_transform_index++] =
                  decomposed_to_component_transform(decomp[i]);
            }
          }
          else
#  endif
          {
            Transform *keys = ob->get_motion().data();
            for (int i = 0; i < key_count; i++) {
              float *t = (float *)&matrix_motion_transforms[motion_transform_index++];
              /* Transpose transform */
              const auto *src = (const float *)&keys[i];
              for (int i = 0; i < 12; i++) {
                t[i] = src[(i / 3) + 4 * (i % 3)];
              }
            }
          }
        }
        else {
          desc.motionTransformsCount = 1;

#  if defined(MAC_OS_VERSION_15_0)
          if (use_pcmi) {
            if (ob->get_geometry()->is_instanced()) {
              DecomposedTransform decomp;
              transform_motion_decompose(&decomp, &ob->get_tfm(), 1);
              decomposed_motion_transforms[motion_transform_index++] =
                  decomposed_to_component_transform(decomp);
            }
            else {
              decomposed_motion_transforms[motion_transform_index++] =
                  component_transform_make_unit();
            }
          }
          else
#  endif
          {
            float *t = (float *)&matrix_motion_transforms[motion_transform_index++];
            if (ob->get_geometry()->is_instanced()) {
              /* Transpose transform */
              const auto *src = (const float *)&ob->get_tfm();
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
      }
      else {
        MTLAccelerationStructureUserIDInstanceDescriptor *instances =
            (MTLAccelerationStructureUserIDInstanceDescriptor *)[instanceBuf contents];
        MTLAccelerationStructureUserIDInstanceDescriptor &desc = instances[currIndex];

        desc.accelerationStructureIndex = accel_struct_index;
        desc.userID = primitive_offset;
        desc.mask = mask;
        desc.intersectionFunctionTableOffset = 0;
        desc.options = MTLAccelerationStructureInstanceOptionOpaque;

        float *t = (float *)&desc.transformationMatrix;
        if (ob->get_geometry()->is_instanced()) {
          /* Transpose transform */
          const auto *src = (const float *)&ob->get_tfm();
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

    if (use_instance_motion) {
      BVH_status(
          "Building motion TLAS      | %7d instances | %7d motion instances | %7d motion "
          "transforms",
          (int)num_instances,
          (int)num_motion_instances,
          (int)num_motion_transforms);
    }
    else {
      BVH_status("Building TLAS      | %7d instances", (int)num_instances);
    }

    MTLInstanceAccelerationStructureDescriptor *accelDesc =
        [MTLInstanceAccelerationStructureDescriptor descriptor];
    accelDesc.instanceCount = num_instances;
    accelDesc.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeUserID;
    accelDesc.instanceDescriptorBuffer = instanceBuf;
    accelDesc.instanceDescriptorBufferOffset = 0;
    accelDesc.instanceDescriptorStride = instance_size;
    accelDesc.instancedAccelerationStructures = all_blas;

    if (use_instance_motion) {
      accelDesc.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeMotion;
      accelDesc.motionTransformBuffer = motion_transforms_buf;
      accelDesc.motionTransformCount = num_motion_transforms;
#  if defined(MAC_OS_VERSION_15_0)
      if (@available(macos 15.0, *)) {
        accelDesc.motionTransformStride = 0;
        accelDesc.motionTransformType = use_pcmi ? MTLTransformTypeComponent :
                                                   MTLTransformTypePackedFloat4x3;
      }
#  endif
    }

    if (extended_limits) {
      accelDesc.usage |= MTLAccelerationStructureUsageExtendedLimits;
    }
    if (!use_fast_trace_bvh) {
      accelDesc.usage |= (MTLAccelerationStructureUsageRefit |
                          MTLAccelerationStructureUsagePreferFastBuild);
    }
    else if (@available(macos 26.0, *)) {
      accelDesc.usage |= MTLAccelerationStructureUsagePreferFastIntersection;
    }

    MTLAccelerationStructureSizes accelSizes = [mtl_device
        accelerationStructureSizesWithDescriptor:accelDesc];
    id<MTLAccelerationStructure> accel = [mtl_device
        newAccelerationStructureWithSize:accelSizes.accelerationStructureSize];
    id<MTLBuffer> scratchBuf = [mtl_device newBufferWithLength:accelSizes.buildScratchBufferSize
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

    /* Cache top and bottom-level acceleration structs */
    set_accel_struct(accel);

    unique_blas_array.clear();
    unique_blas_array.reserve(all_blas.count);
    [all_blas enumerateObjectsUsingBlock:^(id<MTLAccelerationStructure> blas, NSUInteger, BOOL *) {
      unique_blas_array.push_back(blas);
    }];

    return true;
  }
  return false;
}

bool BVHMetal::build(Progress &progress,
                     id<MTLDevice> mtl_device,
                     id<MTLCommandQueue> queue,
                     bool refit)
{
  if (@available(macos 12.0, *)) {
    if (refit) {
      /* It isn't valid to refit a non-existent BVH, or one which wasn't constructed as dynamic.
       * In such cases, assert in development but try to recover in the wild. */
      if (params.bvh_type != BVH_TYPE_DYNAMIC) {
        assert(!"Can't refit static Metal BVH");
        refit = false;
      }
      else if (!accel_struct) {
        assert(!"Can't refit non-existing Metal BVH");
        refit = false;
      }
    }

    if (!refit) {
      set_accel_struct(nil);
    }
  }

  if (!support_refit_blas()) {
    refit = false;
  }

  @autoreleasepool {
    if (!params.top_level) {
      return build_BLAS(progress, mtl_device, queue, refit);
    }
    return build_TLAS(progress, mtl_device, queue, refit);
  }
}

CCL_NAMESPACE_END

#endif /* WITH_METAL */
