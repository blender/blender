/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_HIPRT

#  include "device/hip/device_impl.h"
#  include "device/hip/kernel.h"
#  include "device/hip/queue.h"
#  include "device/hiprt/queue.h"

#  ifdef WITH_HIP_DYNLOAD
#    include <hiprtew.h>
#  else
#    include <hiprt/hiprt_types.h>
#  endif

CCL_NAMESPACE_BEGIN

class Mesh;
class Hair;
class PointCloud;
class Geometry;
class Object;
class BVHHIPRT;

class HIPRTDevice : public HIPDevice {

 public:
  BVHLayoutMask get_bvh_layout_mask(const uint kernel_features) const override;

  HIPRTDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler, bool headless);

  ~HIPRTDevice() override;
  unique_ptr<DeviceQueue> gpu_queue_create() override;
  string compile_kernel_get_common_cflags(const uint kernel_features) override;
  string compile_kernel(const uint kernel_features,
                        const char *name,
                        const char *base = "hiprt") override;

  bool load_kernels(const uint kernel_features) override;

  void const_copy_to(const char *name, void *host, const size_t size) override;

  void build_bvh(BVH *bvh, Progress &progress, bool refit) override;

  void release_bvh(BVH *bvh) override;

  hiprtContext get_hiprt_context()
  {
    return hiprt_context;
  }

  hiprtGlobalStackBuffer global_stack_buffer;

 protected:
  enum Filter_Function { Closest = 0, Shadows, Local, Volume, Max_Intersect_Filter_Function };
  enum Primitive_Type { Triangle = 0, Curve, Motion_Triangle, Point, Max_Primitive_Type };

  hiprtGeometryBuildInput prepare_triangle_blas(BVHHIPRT *bvh, Mesh *mesh);
  hiprtGeometryBuildInput prepare_curve_blas(BVHHIPRT *bvh, Hair *hair);
  hiprtGeometryBuildInput prepare_point_blas(BVHHIPRT *bvh, PointCloud *pointcloud);
  void build_blas(BVHHIPRT *bvh, Geometry *geom, hiprtBuildOptions options);
  hiprtScene build_tlas(BVHHIPRT *bvh,
                        const vector<Object *> &objects,
                        hiprtBuildOptions options,
                        bool refit);
  void free_bvh_memory_delayed();
  hiprtContext hiprt_context;
  hiprtScene scene;
  hiprtFuncTable functions_table;

  thread_mutex hiprt_mutex;
  size_t scratch_buffer_size;
  device_vector<char> scratch_buffer;

  /* This vector tracks the hiprt_geom members of BVHRT so that device memory
   * can be managed/released in HIPRTDevice.
   * Even if synchronization occurs before memory release, a GPU job may still
   * launch between synchronization and release, potentially causing the GPU
   * to access unmapped memory. */
  vector<hiprtGeometry> stale_bvh;

  /* Is this scene using motion blur? Note there might exist motion data even if
   * motion blur is disabled, for render passes. */
  bool use_motion_blur = false;

  /* The following vectors are to transfer scene information available on the host to the GPU
   * visibility, instance_transform_matrix, transform_headers, and hiprt_blas_ptr are passed to
   * hiprt to build bvh the rest are directly used in traversal functions/intersection kernels and
   * are defined on the GPU side as members of KernelParamsHIPRT struct the host memory is copied
   * to GPU through const_copy_to() function. */

  /* Originally, visibility was only passed to HIP RT but after a bug report it was noted it was
   * required for custom primitives (i.e., motion triangles). This buffer, however, has visibility
   * per object not per primitive so the same buffer as the one that is passed to HIP RT can be
   * used. */
  device_vector<uint32_t> prim_visibility;

  /* instance_transform_matrix passes transform matrix of instances converted from Cycles Transform
   * format to instanceFrames member of hiprtSceneBuildInput. */
  device_vector<hiprtFrameMatrix> instance_transform_matrix;
  /* Movement over a time interval for motion blur is captured through multiple transform matrices.
   * In this case transform matrix of an instance cannot be directly retrieved by looking up
   * instance_transform_matrix give the instance id. transform_headers maps the instance id to the
   * appropriate index to retrieve instance transform matrix (frameIndex member of
   * hiprtTransformHeader). transform_headers also has the information on how many transform
   * matrices are associated with an instance (frameCount member of hiprtTransformHeader)
   * transform_headers is passed to hiprt through instanceTransformHeaders member of
   * hiprtSceneBuildInput. */
  device_vector<hiprtTransformHeader> transform_headers;

  /* Instance/object ids are not explicitly  passed to hiprt.
   * HIP RT assigns the ids based on the order blas pointers are passed to it (through
   * instanceGeometries member of hiprtSceneBuildInput). If blas is absent for a particular
   * geometry (e.g. a plane), HIP RT removes that entry and in scenes with objects with no blas,
   * the instance id that hiprt returns for a hit point will not necessarily match the instance id
   * of the application. user_instance_id provides a map for retrieving original instance id from
   * what HIP RT returns as instance id. hiprt_blas_ptr is the list of all the valid blas pointers.
   * blas_ptr has all the valid pointers and null pointers and blas for any geometry can be
   * directly retrieved from this array (used in subsurface scattering). */
  device_vector<int> user_instance_id;
  device_vector<hiprtInstance> hiprt_blas_ptr;
  device_vector<uint64_t> blas_ptr;

  /* custom_prim_info stores custom information for custom primitives for all the primitives in a
   * scene. Primitive id that HIP RT returns is local to the geometry that was hit.
   * custom_prim_info_offset returns the offset required to add to the primitive id to retrieve
   * primitive info from custom_prim_info. */
  device_vector<int2> custom_prim_info;
  device_vector<int2> custom_prim_info_offset;

  /* prims_time stores primitive time for geometries with motion blur.
   * prim_time_offset returns the offset to add to primitive id to retrieve primitive time. */
  device_vector<float2> prims_time;
  device_vector<int> prim_time_offset;
};
CCL_NAMESPACE_END

#endif
