/*
 * Copyright 2018, Blender Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

#  include <embree3/rtcore_geometry.h>

#  include "bvh/bvh_embree.h"

/* Kernel includes are necessary so that the filter function for Embree can access the packed BVH.
 */
#  include "kernel/bvh/bvh_embree.h"
#  include "kernel/kernel_compat_cpu.h"
#  include "kernel/kernel_globals.h"
#  include "kernel/kernel_random.h"
#  include "kernel/split/kernel_split_data_types.h"

#  include "render/hair.h"
#  include "render/mesh.h"
#  include "render/object.h"

#  include "util/util_foreach.h"
#  include "util/util_logging.h"
#  include "util/util_progress.h"
#  include "util/util_stats.h"

CCL_NAMESPACE_BEGIN

static_assert(Object::MAX_MOTION_STEPS <= RTC_MAX_TIME_STEP_COUNT,
              "Object and Embree max motion steps inconsistent");
static_assert(Object::MAX_MOTION_STEPS == Geometry::MAX_MOTION_STEPS,
              "Object and Geometry max motion steps inconsistent");

#  define IS_HAIR(x) (x & 1)

/* This gets called by Embree at every valid ray/object intersection.
 * Things like recording subsurface or shadow hits for later evaluation
 * as well as filtering for volume objects happen here.
 * Cycles' own BVH does that directly inside the traversal calls.
 */
static void rtc_filter_occluded_func(const RTCFilterFunctionNArguments *args)
{
  /* Current implementation in Cycles assumes only single-ray intersection queries. */
  assert(args->N == 1);

  const RTCRay *ray = (RTCRay *)args->ray;
  RTCHit *hit = (RTCHit *)args->hit;
  CCLIntersectContext *ctx = ((IntersectContext *)args->context)->userRayExt;
  KernelGlobals *kg = ctx->kg;

  switch (ctx->type) {
    case CCLIntersectContext::RAY_SHADOW_ALL: {
      /* Append the intersection to the end of the array. */
      if (ctx->num_hits < ctx->max_hits) {
        Intersection current_isect;
        kernel_embree_convert_hit(kg, ray, hit, &current_isect);
        for (size_t i = 0; i < ctx->max_hits; ++i) {
          if (current_isect.object == ctx->isect_s[i].object &&
              current_isect.prim == ctx->isect_s[i].prim && current_isect.t == ctx->isect_s[i].t) {
            /* This intersection was already recorded, skip it. */
            *args->valid = 0;
            break;
          }
        }
        Intersection *isect = &ctx->isect_s[ctx->num_hits];
        ++ctx->num_hits;
        *isect = current_isect;
        int prim = kernel_tex_fetch(__prim_index, isect->prim);
        int shader = 0;
        if (kernel_tex_fetch(__prim_type, isect->prim) & PRIMITIVE_ALL_TRIANGLE) {
          shader = kernel_tex_fetch(__tri_shader, prim);
        }
        else {
          float4 str = kernel_tex_fetch(__curves, prim);
          shader = __float_as_int(str.z);
        }
        int flag = kernel_tex_fetch(__shaders, shader & SHADER_MASK).flags;
        /* If no transparent shadows, all light is blocked. */
        if (flag & (SD_HAS_TRANSPARENT_SHADOW)) {
          /* This tells Embree to continue tracing. */
          *args->valid = 0;
        }
      }
      else {
        /* Increase the number of hits beyond ray.max_hits
         * so that the caller can detect this as opaque. */
        ++ctx->num_hits;
      }
      break;
    }
    case CCLIntersectContext::RAY_LOCAL:
    case CCLIntersectContext::RAY_SSS: {
      /* Check if it's hitting the correct object. */
      Intersection current_isect;
      if (ctx->type == CCLIntersectContext::RAY_SSS) {
        kernel_embree_convert_sss_hit(kg, ray, hit, &current_isect, ctx->local_object_id);
      }
      else {
        kernel_embree_convert_hit(kg, ray, hit, &current_isect);
        int object = (current_isect.object == OBJECT_NONE) ?
                         kernel_tex_fetch(__prim_object, current_isect.prim) :
                         current_isect.object;
        if (ctx->local_object_id != object) {
          /* This tells Embree to continue tracing. */
          *args->valid = 0;
          break;
        }
      }

      /* No intersection information requested, just return a hit. */
      if (ctx->max_hits == 0) {
        break;
      }

      /* Ignore curves. */
      if (IS_HAIR(hit->geomID)) {
        /* This tells Embree to continue tracing. */
        *args->valid = 0;
        break;
      }

      /* See triangle_intersect_subsurface() for the native equivalent. */
      for (int i = min(ctx->max_hits, ctx->local_isect->num_hits) - 1; i >= 0; --i) {
        if (ctx->local_isect->hits[i].t == ray->tfar) {
          /* This tells Embree to continue tracing. */
          *args->valid = 0;
          break;
        }
      }

      int hit_idx = 0;

      if (ctx->lcg_state) {

        ++ctx->local_isect->num_hits;
        if (ctx->local_isect->num_hits <= ctx->max_hits) {
          hit_idx = ctx->local_isect->num_hits - 1;
        }
        else {
          /* reservoir sampling: if we are at the maximum number of
           * hits, randomly replace element or skip it */
          hit_idx = lcg_step_uint(ctx->lcg_state) % ctx->local_isect->num_hits;

          if (hit_idx >= ctx->max_hits) {
            /* This tells Embree to continue tracing. */
            *args->valid = 0;
            break;
          }
        }
      }
      else {
        ctx->local_isect->num_hits = 1;
      }
      /* record intersection */
      ctx->local_isect->hits[hit_idx] = current_isect;
      ctx->local_isect->Ng[hit_idx] = normalize(make_float3(hit->Ng_x, hit->Ng_y, hit->Ng_z));
      /* This tells Embree to continue tracing .*/
      *args->valid = 0;
      break;
    }
    case CCLIntersectContext::RAY_VOLUME_ALL: {
      /* Append the intersection to the end of the array. */
      if (ctx->num_hits < ctx->max_hits) {
        Intersection current_isect;
        kernel_embree_convert_hit(kg, ray, hit, &current_isect);
        for (size_t i = 0; i < ctx->max_hits; ++i) {
          if (current_isect.object == ctx->isect_s[i].object &&
              current_isect.prim == ctx->isect_s[i].prim && current_isect.t == ctx->isect_s[i].t) {
            /* This intersection was already recorded, skip it. */
            *args->valid = 0;
            break;
          }
        }
        Intersection *isect = &ctx->isect_s[ctx->num_hits];
        ++ctx->num_hits;
        *isect = current_isect;
        /* Only primitives from volume object. */
        uint tri_object = (isect->object == OBJECT_NONE) ?
                              kernel_tex_fetch(__prim_object, isect->prim) :
                              isect->object;
        int object_flag = kernel_tex_fetch(__object_flag, tri_object);
        if ((object_flag & SD_OBJECT_HAS_VOLUME) == 0) {
          --ctx->num_hits;
        }
        /* This tells Embree to continue tracing. */
        *args->valid = 0;
        break;
      }
    }
    case CCLIntersectContext::RAY_REGULAR:
    default:
      /* Nothing to do here. */
      break;
  }
}

static void rtc_filter_func_thick_curve(const RTCFilterFunctionNArguments *args)
{
  const RTCRay *ray = (RTCRay *)args->ray;
  RTCHit *hit = (RTCHit *)args->hit;

  /* Always ignore backfacing intersections. */
  if (dot(make_float3(ray->dir_x, ray->dir_y, ray->dir_z),
          make_float3(hit->Ng_x, hit->Ng_y, hit->Ng_z)) > 0.0f) {
    *args->valid = 0;
    return;
  }
}

static void rtc_filter_occluded_func_thick_curve(const RTCFilterFunctionNArguments *args)
{
  const RTCRay *ray = (RTCRay *)args->ray;
  RTCHit *hit = (RTCHit *)args->hit;

  /* Always ignore backfacing intersections. */
  if (dot(make_float3(ray->dir_x, ray->dir_y, ray->dir_z),
          make_float3(hit->Ng_x, hit->Ng_y, hit->Ng_z)) > 0.0f) {
    *args->valid = 0;
    return;
  }

  rtc_filter_occluded_func(args);
}

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
  VLOG(1) << str;
}

static double progress_start_time = 0.0f;

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

void BVHEmbree::build(Progress &progress, Stats *stats, RTCDevice rtc_device_)
{
  rtc_device = rtc_device_;
  assert(rtc_device);

  rtcSetDeviceErrorFunction(rtc_device, rtc_error_func, NULL);
  rtcSetDeviceMemoryMonitorFunction(rtc_device, rtc_memory_monitor_func, stats);

  progress.set_substatus("Building BVH");

  if (scene) {
    rtcReleaseScene(scene);
    scene = NULL;
  }

  const bool dynamic = params.bvh_type == SceneParams::BVH_DYNAMIC;

  scene = rtcNewScene(rtc_device);
  const RTCSceneFlags scene_flags = (dynamic ? RTC_SCENE_FLAG_DYNAMIC : RTC_SCENE_FLAG_NONE) |
                                    RTC_SCENE_FLAG_COMPACT | RTC_SCENE_FLAG_ROBUST;
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
}

void BVHEmbree::add_instance(Object *ob, int i)
{
  BVHEmbree *instance_bvh = (BVHEmbree *)(ob->get_geometry()->bvh);
  assert(instance_bvh != NULL);

  const size_t num_object_motion_steps = ob->use_motion() ? ob->get_motion().size() : 1;
  const size_t num_motion_steps = min(num_object_motion_steps, RTC_MAX_TIME_STEP_COUNT);
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

  rtcCommitGeometry(geom_id);
  rtcAttachGeometryByID(scene, geom_id, i * 2);
  rtcReleaseGeometry(geom_id);
}

void BVHEmbree::add_triangles(const Object *ob, const Mesh *mesh, int i)
{
  size_t prim_offset = mesh->optix_prim_offset;

  const Attribute *attr_mP = NULL;
  size_t num_motion_steps = 1;
  if (mesh->has_motion_blur()) {
    attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (attr_mP) {
      num_motion_steps = mesh->get_motion_steps();
    }
  }

  assert(num_motion_steps <= RTC_MAX_TIME_STEP_COUNT);
  num_motion_steps = min(num_motion_steps, RTC_MAX_TIME_STEP_COUNT);

  const size_t num_triangles = mesh->num_triangles();

  RTCGeometry geom_id = rtcNewGeometry(rtc_device, RTC_GEOMETRY_TYPE_TRIANGLE);
  rtcSetGeometryBuildQuality(geom_id, build_quality);
  rtcSetGeometryTimeStepCount(geom_id, num_motion_steps);

  unsigned *rtc_indices = (unsigned *)rtcSetNewGeometryBuffer(
      geom_id, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, sizeof(int) * 3, num_triangles);
  assert(rtc_indices);
  if (!rtc_indices) {
    VLOG(1) << "Embree could not create new geometry buffer for mesh " << mesh->name.c_str()
            << ".\n";
    return;
  }
  for (size_t j = 0; j < num_triangles; ++j) {
    Mesh::Triangle t = mesh->get_triangle(j);
    rtc_indices[j * 3] = t.v[0];
    rtc_indices[j * 3 + 1] = t.v[1];
    rtc_indices[j * 3 + 2] = t.v[2];
  }

  set_tri_vertex_buffer(geom_id, mesh, false);

  rtcSetGeometryUserData(geom_id, (void *)prim_offset);
  rtcSetGeometryOccludedFilterFunction(geom_id, rtc_filter_occluded_func);
  rtcSetGeometryMask(geom_id, ob->visibility_for_tracing());

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

    float *rtc_verts = (update) ?
                           (float *)rtcGetGeometryBufferData(geom_id, RTC_BUFFER_TYPE_VERTEX, t) :
                           (float *)rtcSetNewGeometryBuffer(geom_id,
                                                            RTC_BUFFER_TYPE_VERTEX,
                                                            t,
                                                            RTC_FORMAT_FLOAT3,
                                                            sizeof(float) * 3,
                                                            num_verts + 1);

    assert(rtc_verts);
    if (rtc_verts) {
      for (size_t j = 0; j < num_verts; ++j) {
        rtc_verts[0] = verts[j].x;
        rtc_verts[1] = verts[j].y;
        rtc_verts[2] = verts[j].z;
        rtc_verts += 3;
      }
    }

    if (update) {
      rtcUpdateGeometryBuffer(geom_id, RTC_BUFFER_TYPE_VERTEX, t);
    }
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
    const float3 *verts;
    if (t == t_mid || attr_mP == NULL) {
      verts = &hair->get_curve_keys()[0];
    }
    else {
      int t_ = (t > t_mid) ? (t - 1) : t;
      verts = &attr_mP->data_float3()[t_ * num_keys];
    }

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
      for (size_t j = 0; j < num_curves; ++j) {
        Hair::Curve c = hair->get_curve(j);
        int fk = c.first_key;
        int k = 1;
        for (; k < c.num_keys + 1; ++k, ++fk) {
          rtc_verts[k] = float3_to_float4(verts[fk]);
          rtc_verts[k].w = curve_radius[fk];
        }
        /* Duplicate Embree's Catmull-Rom spline CVs at the start and end of each curve. */
        rtc_verts[0] = rtc_verts[1];
        rtc_verts[k] = rtc_verts[k - 1];
        rtc_verts += c.num_keys + 2;
      }
    }

    if (update) {
      rtcUpdateGeometryBuffer(geom_id, RTC_BUFFER_TYPE_VERTEX, t);
    }
  }
}

void BVHEmbree::add_curves(const Object *ob, const Hair *hair, int i)
{
  size_t prim_offset = hair->optix_prim_offset;

  const Attribute *attr_mP = NULL;
  size_t num_motion_steps = 1;
  if (hair->has_motion_blur()) {
    attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (attr_mP) {
      num_motion_steps = hair->get_motion_steps();
    }
  }

  assert(num_motion_steps <= RTC_MAX_TIME_STEP_COUNT);
  num_motion_steps = min(num_motion_steps, RTC_MAX_TIME_STEP_COUNT);

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
  if (hair->curve_shape == CURVE_RIBBON) {
    rtcSetGeometryOccludedFilterFunction(geom_id, rtc_filter_occluded_func);
  }
  else {
    rtcSetGeometryIntersectFilterFunction(geom_id, rtc_filter_func_thick_curve);
    rtcSetGeometryOccludedFilterFunction(geom_id, rtc_filter_occluded_func_thick_curve);
  }
  rtcSetGeometryMask(geom_id, ob->visibility_for_tracing());

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
          rtcSetGeometryUserData(geom, (void *)mesh->optix_prim_offset);
          rtcCommitGeometry(geom);
        }
      }
      else if (geom->geometry_type == Geometry::HAIR) {
        Hair *hair = static_cast<Hair *>(geom);
        if (hair->num_curves() > 0) {
          RTCGeometry geom = rtcGetGeometry(scene, geom_id + 1);
          set_curve_vertex_buffer(geom, hair, true);
          rtcSetGeometryUserData(geom, (void *)hair->optix_prim_offset);
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
