/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2018-2022 Blender Foundation. */

/* This class implements a ray accelerator for Cycles using Intel's Embree library.
 * It supports triangles, curves, object and deformation blur and instancing.
 *
 * Since Embree allows object to be either curves or triangles but not both, Cycles object IDs are
 * mapped to Embree IDs by multiplying by two and adding one for curves.
 *
 * This implementation shares RTCDevices between Cycles instances. Eventually each instance should
 * get a separate RTCDevice to correctly keep track of memory usage.
 *
 * Vertex and index buffers are duplicated between Cycles device arrays and Embree. These could be
 * merged, which would require changes to intersection refinement, shader setup, mesh light
 * sampling and a few other places in Cycles where direct access to vertex data is required.
 */

#ifdef WITH_EMBREE

#  if EMBREE_MAJOR_VERSION >= 4
#    include <embree4/rtcore_geometry.h>
#  else
#    include <embree3/rtcore_geometry.h>
#  endif

#  include "bvh/embree.h"

#  include "kernel/device/cpu/bvh.h"
#  include "kernel/device/cpu/compat.h"
#  include "kernel/device/cpu/globals.h"

#  include "scene/hair.h"
#  include "scene/mesh.h"
#  include "scene/object.h"
#  include "scene/pointcloud.h"

#  include "util/foreach.h"
#  include "util/log.h"
#  include "util/progress.h"
#  include "util/stats.h"

CCL_NAMESPACE_BEGIN

static_assert(Object::MAX_MOTION_STEPS <= RTC_MAX_TIME_STEP_COUNT,
              "Object and Embree max motion steps inconsistent");
static_assert(Object::MAX_MOTION_STEPS == Geometry::MAX_MOTION_STEPS,
              "Object and Geometry max motion steps inconsistent");

static size_t unaccounted_mem = 0;

static bool rtc_memory_monitor_func(void *userPtr, const ssize_t bytes, const bool)
{
  Stats *stats = (Stats *)userPtr;
  if (stats) {
    if (bytes > 0) {
      stats->mem_alloc(bytes);
    }
    else {
      stats->mem_free(-bytes);
    }
  }
  else {
    /* A stats pointer may not yet be available. Keep track of the memory usage for later. */
    if (bytes >= 0) {
      atomic_add_and_fetch_z(&unaccounted_mem, bytes);
    }
    else {
      atomic_sub_and_fetch_z(&unaccounted_mem, -bytes);
    }
  }
  return true;
}

static void rtc_error_func(void *, enum RTCError, const char *str)
{
  VLOG_WARNING << str;
}

static double progress_start_time = 0.0;

static bool rtc_progress_func(void *user_ptr, const double n)
{
  Progress *progress = (Progress *)user_ptr;

  if (time_dt() - progress_start_time < 0.25) {
    return true;
  }

  string msg = string_printf("Building BVH %.0f%%", n * 100.0);
  progress->set_substatus(msg);
  progress_start_time = time_dt();

  return !progress->get_cancel();
}

BVHEmbree::BVHEmbree(const BVHParams &params_,
                     const vector<Geometry *> &geometry_,
                     const vector<Object *> &objects_)
    : BVH(params_, geometry_, objects_),
      scene(NULL),
      rtc_device(NULL),
      build_quality(RTC_BUILD_QUALITY_REFIT)
{
  SIMD_SET_FLUSH_TO_ZERO;
}

BVHEmbree::~BVHEmbree()
{
  if (scene) {
    rtcReleaseScene(scene);
  }
}

void BVHEmbree::build(Progress &progress,
                      Stats *stats,
                      RTCDevice rtc_device_,
                      const bool rtc_device_is_sycl_)
{
  rtc_device = rtc_device_;
  rtc_device_is_sycl = rtc_device_is_sycl_;
  assert(rtc_device);

  rtcSetDeviceErrorFunction(rtc_device, rtc_error_func, NULL);
  rtcSetDeviceMemoryMonitorFunction(rtc_device, rtc_memory_monitor_func, stats);

  progress.set_substatus("Building BVH");

  if (scene) {
    rtcReleaseScene(scene);
    scene = NULL;
  }

  const bool dynamic = params.bvh_type == BVH_TYPE_DYNAMIC;
  const bool compact = params.use_compact_structure;

  scene = rtcNewScene(rtc_device);
  const RTCSceneFlags scene_flags = (dynamic ? RTC_SCENE_FLAG_DYNAMIC : RTC_SCENE_FLAG_NONE) |
                                    (compact ? RTC_SCENE_FLAG_COMPACT : RTC_SCENE_FLAG_NONE) |
                                    RTC_SCENE_FLAG_ROBUST
#  if EMBREE_MAJOR_VERSION >= 4
                                    | RTC_SCENE_FLAG_FILTER_FUNCTION_IN_ARGUMENTS
#  endif
      ;
  rtcSetSceneFlags(scene, scene_flags);
  build_quality = dynamic ? RTC_BUILD_QUALITY_LOW :
                            (params.use_spatial_split ? RTC_BUILD_QUALITY_HIGH :
                                                        RTC_BUILD_QUALITY_MEDIUM);
  rtcSetSceneBuildQuality(scene, build_quality);

  int i = 0;
  foreach (Object *ob, objects) {
    if (params.top_level) {
      if (!ob->is_traceable()) {
        ++i;
        continue;
      }
      if (!ob->get_geometry()->is_instanced()) {
        add_object(ob, i);
      }
      else {
        add_instance(ob, i);
      }
    }
    else {
      add_object(ob, i);
    }
    ++i;
    if (progress.get_cancel())
      return;
  }

  if (progress.get_cancel()) {
    return;
  }

  rtcSetSceneProgressMonitorFunction(scene, rtc_progress_func, &progress);
  rtcCommitScene(scene);
}

void BVHEmbree::add_object(Object *ob, int i)
{
  Geometry *geom = ob->get_geometry();

  if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME) {
    Mesh *mesh = static_cast<Mesh *>(geom);
    if (mesh->num_triangles() > 0) {
      add_triangles(ob, mesh, i);
    }
  }
  else if (geom->geometry_type == Geometry::HAIR) {
    Hair *hair = static_cast<Hair *>(geom);
    if (hair->num_curves() > 0) {
      add_curves(ob, hair, i);
    }
  }
  else if (geom->geometry_type == Geometry::POINTCLOUD) {
    PointCloud *pointcloud = static_cast<PointCloud *>(geom);
    if (pointcloud->num_points() > 0) {
      add_points(ob, pointcloud, i);
    }
  }
}

void BVHEmbree::add_instance(Object *ob, int i)
{
  BVHEmbree *instance_bvh = (BVHEmbree *)(ob->get_geometry()->bvh);
  assert(instance_bvh != NULL);

  const size_t num_object_motion_steps = ob->use_motion() ? ob->get_motion().size() : 1;
  const size_t num_motion_steps = min(num_object_motion_steps, (size_t)RTC_MAX_TIME_STEP_COUNT);
  assert(num_object_motion_steps <= RTC_MAX_TIME_STEP_COUNT);

  RTCGeometry geom_id = rtcNewGeometry(rtc_device, RTC_GEOMETRY_TYPE_INSTANCE);
  rtcSetGeometryInstancedScene(geom_id, instance_bvh->scene);
  rtcSetGeometryTimeStepCount(geom_id, num_motion_steps);

  if (ob->use_motion()) {
    array<DecomposedTransform> decomp(ob->get_motion().size());
    transform_motion_decompose(decomp.data(), ob->get_motion().data(), ob->get_motion().size());
    for (size_t step = 0; step < num_motion_steps; ++step) {
      RTCQuaternionDecomposition rtc_decomp;
      rtcInitQuaternionDecomposition(&rtc_decomp);
      rtcQuaternionDecompositionSetQuaternion(
          &rtc_decomp, decomp[step].x.w, decomp[step].x.x, decomp[step].x.y, decomp[step].x.z);
      rtcQuaternionDecompositionSetScale(
          &rtc_decomp, decomp[step].y.w, decomp[step].z.w, decomp[step].w.w);
      rtcQuaternionDecompositionSetTranslation(
          &rtc_decomp, decomp[step].y.x, decomp[step].y.y, decomp[step].y.z);
      rtcQuaternionDecompositionSetSkew(
          &rtc_decomp, decomp[step].z.x, decomp[step].z.y, decomp[step].w.x);
      rtcSetGeometryTransformQuaternion(geom_id, step, &rtc_decomp);
    }
  }
  else {
    rtcSetGeometryTransform(
        geom_id, 0, RTC_FORMAT_FLOAT3X4_ROW_MAJOR, (const float *)&ob->get_tfm());
  }

  rtcSetGeometryUserData(geom_id, (void *)instance_bvh->scene);
  rtcSetGeometryMask(geom_id, ob->visibility_for_tracing());
#  if EMBREE_MAJOR_VERSION >= 4
  rtcSetGeometryEnableFilterFunctionFromArguments(geom_id, true);
#  endif

  rtcCommitGeometry(geom_id);
  rtcAttachGeometryByID(scene, geom_id, i * 2);
  rtcReleaseGeometry(geom_id);
}

void BVHEmbree::add_triangles(const Object *ob, const Mesh *mesh, int i)
{
  size_t prim_offset = mesh->prim_offset;

  const Attribute *attr_mP = NULL;
  size_t num_motion_steps = 1;
  if (mesh->has_motion_blur()) {
    attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (attr_mP) {
      num_motion_steps = mesh->get_motion_steps();
    }
  }

  assert(num_motion_steps <= RTC_MAX_TIME_STEP_COUNT);
  num_motion_steps = min(num_motion_steps, (size_t)RTC_MAX_TIME_STEP_COUNT);

  const size_t num_triangles = mesh->num_triangles();

  RTCGeometry geom_id = rtcNewGeometry(rtc_device, RTC_GEOMETRY_TYPE_TRIANGLE);
  rtcSetGeometryBuildQuality(geom_id, build_quality);
  rtcSetGeometryTimeStepCount(geom_id, num_motion_steps);

  const int *triangles = mesh->get_triangles().data();
  if (!rtc_device_is_sycl) {
    rtcSetSharedGeometryBuffer(geom_id,
                               RTC_BUFFER_TYPE_INDEX,
                               0,
                               RTC_FORMAT_UINT3,
                               triangles,
                               0,
                               sizeof(int) * 3,
                               num_triangles);
  }
  else {
    /* NOTE(sirgienko): If the Embree device is a SYCL device, then Embree execution will
     * happen on GPU, and we cannot use standard host pointers at this point. So instead
     * of making a shared geometry buffer - a new Embree buffer will be created and data
     * will be copied. */
    int *triangles_buffer = (int *)rtcSetNewGeometryBuffer(
        geom_id, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, sizeof(int) * 3, num_triangles);
    assert(triangles_buffer);
    if (triangles_buffer) {
      static_assert(sizeof(int) == sizeof(uint));
      std::memcpy(triangles_buffer, triangles, sizeof(int) * 3 * (num_triangles));
    }
  }
  set_tri_vertex_buffer(geom_id, mesh, false);

  rtcSetGeometryUserData(geom_id, (void *)prim_offset);
  rtcSetGeometryMask(geom_id, ob->visibility_for_tracing());
#  if EMBREE_MAJOR_VERSION >= 4
  rtcSetGeometryEnableFilterFunctionFromArguments(geom_id, true);
#  else
  rtcSetGeometryOccludedFilterFunction(geom_id, kernel_embree_filter_occluded_func);
  rtcSetGeometryIntersectFilterFunction(geom_id, kernel_embree_filter_intersection_func);
#  endif

  rtcCommitGeometry(geom_id);
  rtcAttachGeometryByID(scene, geom_id, i * 2);
  rtcReleaseGeometry(geom_id);
}

void BVHEmbree::set_tri_vertex_buffer(RTCGeometry geom_id, const Mesh *mesh, const bool update)
{
  const Attribute *attr_mP = NULL;
  size_t num_motion_steps = 1;
  int t_mid = 0;
  if (mesh->has_motion_blur()) {
    attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (attr_mP) {
      num_motion_steps = mesh->get_motion_steps();
      t_mid = (num_motion_steps - 1) / 2;
      if (num_motion_steps > RTC_MAX_TIME_STEP_COUNT) {
        assert(0);
        num_motion_steps = RTC_MAX_TIME_STEP_COUNT;
      }
    }
  }
  const size_t num_verts = mesh->get_verts().size();

  for (int t = 0; t < num_motion_steps; ++t) {
    const float3 *verts;
    if (t == t_mid) {
      verts = mesh->get_verts().data();
    }
    else {
      int t_ = (t > t_mid) ? (t - 1) : t;
      verts = &attr_mP->data_float3()[t_ * num_verts];
    }

    if (update) {
      rtcUpdateGeometryBuffer(geom_id, RTC_BUFFER_TYPE_VERTEX, t);
    }
    else {
      if (!rtc_device_is_sycl) {
        rtcSetSharedGeometryBuffer(geom_id,
                                   RTC_BUFFER_TYPE_VERTEX,
                                   t,
                                   RTC_FORMAT_FLOAT3,
                                   verts,
                                   0,
                                   sizeof(float3),
                                   num_verts + 1);
      }
      else {
        /* NOTE(sirgienko): If the Embree device is a SYCL device, then Embree execution will
         * happen on GPU, and we cannot use standard host pointers at this point. So instead
         * of making a shared geometry buffer - a new Embree buffer will be created and data
         * will be copied. */
        /* As float3 is packed on GPU side, we map it to packed_float3. */
        packed_float3 *verts_buffer = (packed_float3 *)rtcSetNewGeometryBuffer(
            geom_id,
            RTC_BUFFER_TYPE_VERTEX,
            t,
            RTC_FORMAT_FLOAT3,
            sizeof(packed_float3),
            num_verts + 1);
        assert(verts_buffer);
        if (verts_buffer) {
          for (size_t i = (size_t)0; i < num_verts + 1; ++i) {
            verts_buffer[i].x = verts[i].x;
            verts_buffer[i].y = verts[i].y;
            verts_buffer[i].z = verts[i].z;
          }
        }
      }
    }
  }
}

/**
 * Packs the hair motion curve data control variables (CVs) into float4s as [x y z radius]
 */
template<typename T>
void pack_motion_verts(size_t num_curves,
                       const Hair *hair,
                       const T *verts,
                       const float *curve_radius,
                       float4 *rtc_verts)
{
  for (size_t j = 0; j < num_curves; ++j) {
    Hair::Curve c = hair->get_curve(j);
    int fk = c.first_key;
    int k = 1;
    for (; k < c.num_keys + 1; ++k, ++fk) {
      rtc_verts[k].x = verts[fk].x;
      rtc_verts[k].y = verts[fk].y;
      rtc_verts[k].z = verts[fk].z;
      rtc_verts[k].w = curve_radius[fk];
    }
    /* Duplicate Embree's Catmull-Rom spline CVs at the start and end of each curve. */
    rtc_verts[0] = rtc_verts[1];
    rtc_verts[k] = rtc_verts[k - 1];
    rtc_verts += c.num_keys + 2;
  }
}

void BVHEmbree::set_curve_vertex_buffer(RTCGeometry geom_id, const Hair *hair, const bool update)
{
  const Attribute *attr_mP = NULL;
  size_t num_motion_steps = 1;
  if (hair->has_motion_blur()) {
    attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (attr_mP) {
      num_motion_steps = hair->get_motion_steps();
    }
  }

  const size_t num_curves = hair->num_curves();
  size_t num_keys = 0;
  for (size_t j = 0; j < num_curves; ++j) {
    const Hair::Curve c = hair->get_curve(j);
    num_keys += c.num_keys;
  }

  /* Catmull-Rom splines need extra CVs at the beginning and end of each curve. */
  size_t num_keys_embree = num_keys;
  num_keys_embree += num_curves * 2;

  /* Copy the CV data to Embree */
  const int t_mid = (num_motion_steps - 1) / 2;
  const float *curve_radius = &hair->get_curve_radius()[0];
  for (int t = 0; t < num_motion_steps; ++t) {
    // As float4 and float3 are no longer interchangeable the 2 types need to be
    // handled separately. Attributes are float4s where the radius is stored in w and
    // the middle motion vector is from the mesh points which are stored float3s with
    // the radius stored in another array.
    float4 *rtc_verts = (update) ? (float4 *)rtcGetGeometryBufferData(
                                       geom_id, RTC_BUFFER_TYPE_VERTEX, t) :
                                   (float4 *)rtcSetNewGeometryBuffer(geom_id,
                                                                     RTC_BUFFER_TYPE_VERTEX,
                                                                     t,
                                                                     RTC_FORMAT_FLOAT4,
                                                                     sizeof(float) * 4,
                                                                     num_keys_embree);

    assert(rtc_verts);
    if (rtc_verts) {
      const size_t num_curves = hair->num_curves();
      if (t == t_mid || attr_mP == NULL) {
        const float3 *verts = &hair->get_curve_keys()[0];
        pack_motion_verts<float3>(num_curves, hair, verts, curve_radius, rtc_verts);
      }
      else {
        int t_ = (t > t_mid) ? (t - 1) : t;
        const float4 *verts = &attr_mP->data_float4()[t_ * num_keys];
        pack_motion_verts<float4>(num_curves, hair, verts, curve_radius, rtc_verts);
      }
    }

    if (update) {
      rtcUpdateGeometryBuffer(geom_id, RTC_BUFFER_TYPE_VERTEX, t);
    }
  }
}

/**
 * Pack the motion points into a float4 as [x y z radius]
 */
template<typename T>
void pack_motion_points(size_t num_points, const T *verts, const float *radius, float4 *rtc_verts)
{
  for (size_t j = 0; j < num_points; ++j) {
    rtc_verts[j].x = verts[j].x;
    rtc_verts[j].y = verts[j].y;
    rtc_verts[j].z = verts[j].z;
    rtc_verts[j].w = radius[j];
  }
}

void BVHEmbree::set_point_vertex_buffer(RTCGeometry geom_id,
                                        const PointCloud *pointcloud,
                                        const bool update)
{
  const Attribute *attr_mP = NULL;
  size_t num_motion_steps = 1;
  if (pointcloud->has_motion_blur()) {
    attr_mP = pointcloud->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (attr_mP) {
      num_motion_steps = pointcloud->get_motion_steps();
    }
  }

  const size_t num_points = pointcloud->num_points();

  /* Copy the point data to Embree */
  const int t_mid = (num_motion_steps - 1) / 2;
  const float *radius = pointcloud->get_radius().data();
  for (int t = 0; t < num_motion_steps; ++t) {
    // As float4 and float3 are no longer interchangeable the 2 types need to be
    // handled separately. Attributes are float4s where the radius is stored in w and
    // the middle motion vector is from the mesh points which are stored float3s with
    // the radius stored in another array.
    float4 *rtc_verts = (update) ? (float4 *)rtcGetGeometryBufferData(
                                       geom_id, RTC_BUFFER_TYPE_VERTEX, t) :
                                   (float4 *)rtcSetNewGeometryBuffer(geom_id,
                                                                     RTC_BUFFER_TYPE_VERTEX,
                                                                     t,
                                                                     RTC_FORMAT_FLOAT4,
                                                                     sizeof(float) * 4,
                                                                     num_points);

    assert(rtc_verts);
    if (rtc_verts) {
      if (t == t_mid || attr_mP == NULL) {
        const float3 *verts = pointcloud->get_points().data();
        pack_motion_points<float3>(num_points, verts, radius, rtc_verts);
      }
      else {
        int t_ = (t > t_mid) ? (t - 1) : t;
        const float4 *verts = &attr_mP->data_float4()[t_ * num_points];
        pack_motion_points<float4>(num_points, verts, radius, rtc_verts);
      }
    }

    if (update) {
      rtcUpdateGeometryBuffer(geom_id, RTC_BUFFER_TYPE_VERTEX, t);
    }
  }
}

void BVHEmbree::add_points(const Object *ob, const PointCloud *pointcloud, int i)
{
  size_t prim_offset = pointcloud->prim_offset;

  const Attribute *attr_mP = NULL;
  size_t num_motion_steps = 1;
  if (pointcloud->has_motion_blur()) {
    attr_mP = pointcloud->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (attr_mP) {
      num_motion_steps = pointcloud->get_motion_steps();
    }
  }

  enum RTCGeometryType type = RTC_GEOMETRY_TYPE_SPHERE_POINT;

  RTCGeometry geom_id = rtcNewGeometry(rtc_device, type);

  rtcSetGeometryBuildQuality(geom_id, build_quality);
  rtcSetGeometryTimeStepCount(geom_id, num_motion_steps);

  set_point_vertex_buffer(geom_id, pointcloud, false);

  rtcSetGeometryUserData(geom_id, (void *)prim_offset);
  rtcSetGeometryMask(geom_id, ob->visibility_for_tracing());
#  if EMBREE_MAJOR_VERSION >= 4
  rtcSetGeometryEnableFilterFunctionFromArguments(geom_id, true);
#  else
  rtcSetGeometryIntersectFilterFunction(geom_id, kernel_embree_filter_func_backface_cull);
  rtcSetGeometryOccludedFilterFunction(geom_id, kernel_embree_filter_occluded_func_backface_cull);
#  endif

  rtcCommitGeometry(geom_id);
  rtcAttachGeometryByID(scene, geom_id, i * 2);
  rtcReleaseGeometry(geom_id);
}

void BVHEmbree::add_curves(const Object *ob, const Hair *hair, int i)
{
  size_t prim_offset = hair->curve_segment_offset;

  const Attribute *attr_mP = NULL;
  size_t num_motion_steps = 1;
  if (hair->has_motion_blur()) {
    attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (attr_mP) {
      num_motion_steps = hair->get_motion_steps();
    }
  }

  assert(num_motion_steps <= RTC_MAX_TIME_STEP_COUNT);
  num_motion_steps = min(num_motion_steps, (size_t)RTC_MAX_TIME_STEP_COUNT);

  const size_t num_curves = hair->num_curves();
  size_t num_segments = 0;
  for (size_t j = 0; j < num_curves; ++j) {
    Hair::Curve c = hair->get_curve(j);
    assert(c.num_segments() > 0);
    num_segments += c.num_segments();
  }

  enum RTCGeometryType type = (hair->curve_shape == CURVE_RIBBON ?
                                   RTC_GEOMETRY_TYPE_FLAT_CATMULL_ROM_CURVE :
                                   RTC_GEOMETRY_TYPE_ROUND_CATMULL_ROM_CURVE);

  RTCGeometry geom_id = rtcNewGeometry(rtc_device, type);
  rtcSetGeometryTessellationRate(geom_id, params.curve_subdivisions + 1);
  unsigned *rtc_indices = (unsigned *)rtcSetNewGeometryBuffer(
      geom_id, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT, sizeof(int), num_segments);
  size_t rtc_index = 0;
  for (size_t j = 0; j < num_curves; ++j) {
    Hair::Curve c = hair->get_curve(j);
    for (size_t k = 0; k < c.num_segments(); ++k) {
      rtc_indices[rtc_index] = c.first_key + k;
      /* Room for extra CVs at Catmull-Rom splines. */
      rtc_indices[rtc_index] += j * 2;

      ++rtc_index;
    }
  }

  rtcSetGeometryBuildQuality(geom_id, build_quality);
  rtcSetGeometryTimeStepCount(geom_id, num_motion_steps);

  set_curve_vertex_buffer(geom_id, hair, false);

  rtcSetGeometryUserData(geom_id, (void *)prim_offset);
  rtcSetGeometryMask(geom_id, ob->visibility_for_tracing());
#  if EMBREE_MAJOR_VERSION >= 4
  rtcSetGeometryEnableFilterFunctionFromArguments(geom_id, true);
#  else
  if (hair->curve_shape == CURVE_RIBBON) {
    rtcSetGeometryIntersectFilterFunction(geom_id, kernel_embree_filter_intersection_func);
    rtcSetGeometryOccludedFilterFunction(geom_id, kernel_embree_filter_occluded_func);
  }
  else {
    rtcSetGeometryIntersectFilterFunction(geom_id, kernel_embree_filter_func_backface_cull);
    rtcSetGeometryOccludedFilterFunction(geom_id,
                                         kernel_embree_filter_occluded_func_backface_cull);
  }
#  endif

  rtcCommitGeometry(geom_id);
  rtcAttachGeometryByID(scene, geom_id, i * 2 + 1);
  rtcReleaseGeometry(geom_id);
}

void BVHEmbree::refit(Progress &progress)
{
  progress.set_substatus("Refitting BVH nodes");

  /* Update all vertex buffers, then tell Embree to rebuild/-fit the BVHs. */
  unsigned geom_id = 0;
  foreach (Object *ob, objects) {
    if (!params.top_level || (ob->is_traceable() && !ob->get_geometry()->is_instanced())) {
      Geometry *geom = ob->get_geometry();

      if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        if (mesh->num_triangles() > 0) {
          RTCGeometry geom = rtcGetGeometry(scene, geom_id);
          set_tri_vertex_buffer(geom, mesh, true);
          rtcSetGeometryUserData(geom, (void *)mesh->prim_offset);
          rtcCommitGeometry(geom);
        }
      }
      else if (geom->geometry_type == Geometry::HAIR) {
        Hair *hair = static_cast<Hair *>(geom);
        if (hair->num_curves() > 0) {
          RTCGeometry geom = rtcGetGeometry(scene, geom_id + 1);
          set_curve_vertex_buffer(geom, hair, true);
          rtcSetGeometryUserData(geom, (void *)hair->curve_segment_offset);
          rtcCommitGeometry(geom);
        }
      }
      else if (geom->geometry_type == Geometry::POINTCLOUD) {
        PointCloud *pointcloud = static_cast<PointCloud *>(geom);
        if (pointcloud->num_points() > 0) {
          RTCGeometry geom = rtcGetGeometry(scene, geom_id);
          set_point_vertex_buffer(geom, pointcloud, true);
          rtcCommitGeometry(geom);
        }
      }
    }
    geom_id += 2;
  }

  rtcCommitScene(scene);
}

CCL_NAMESPACE_END

#endif /* WITH_EMBREE */
