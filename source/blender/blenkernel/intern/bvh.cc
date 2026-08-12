/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_kdopbvh.hh"
#include "BLI_math_geom_c.hh"
#include "BLI_math_vector.hh"

#include "DNA_mesh_types.h"

#include "BKE_bvh.hh"
#include "BKE_bvhutils.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_sample.hh"

#ifdef WITH_EMBREE

#  include <embree4/rtcore.h>

#endif /* WITH_EMBREE */

/**
 * \note Embree recommends enabling flush-to-zero and denormals-are-zero. That is done for all
 * threads in #BLI_task_scheduler_init.
 */

namespace blender::bke::bvh {

Tree::Tree() = default;

Tree::Tree(Tree &&other)
{
#ifdef WITH_EMBREE
  rtc_device_ = std::exchange(other.rtc_device_, nullptr);
  rtc_scene_ = std::exchange(other.rtc_scene_, nullptr);
  index_map_by_geom_ = std::move(other.index_map_by_geom_);
#else /* WITH_EMBREE */
  fallback_tree_ = std::move(other.fallback_tree_);
#endif
}

Tree &Tree::operator=(Tree &&other)
{
  if (this != &other) {
    this->~Tree();
    new (this) Tree(std::move(other));
  }
  return *this;
}

Tree::~Tree()
{
#ifdef WITH_EMBREE
  rtcReleaseScene(this->rtc_scene_);
  this->rtc_scene_ = nullptr;
  rtcReleaseDevice(this->rtc_device_);
  this->rtc_device_ = nullptr;
  index_map_by_geom_.clear_and_shrink();
#else /* WITH_EMBREE */
  this->fallback_tree_.reset();
#endif
}

#ifdef WITH_EMBREE

static void rtc_error_func(void * /*userPtr*/, RTCError /*error*/, const char * /*str*/) {}

static bool rtc_memory_monitor_func(void * /*userPtr*/, const ssize_t /*bytes*/, const bool)
{
  return true;
}

static bool rtc_progress_func(void * /*user_ptr*/, const double /*n*/)
{
  return true;
}

/**
 * Embree and Blender use different barycentric coordinate conventions:
 * Embree: `(1 - u - v) * P[0] + u * P[1] + v * P[2]`
 * Blender: `u * P[0] + v * P[1] + (1 - u - v) * P[2]`
 */
static float3 bary_coord_embree_to_blender(const float u, const float v)
{
  return float3(1.0f - u - v, u, v);
}

#endif /* WITH_EMBREE */

#ifdef WITH_EMBREE

struct BvhBuildContext {
  RTCDevice device;
  RTCScene scene;
  RTCBuildQuality build_quality;
};

static bool all_faces_are_triangles(const Mesh &mesh)
{
  return mesh.corners_num == mesh.faces_num * 3;
}

static void add_positions(const Span<float3> positions, RTCGeometry geom_id)
{
  /* Unfortunately #rtcSetSharedGeometryBuffer cannot be used here because Embree must load past
   * the end of the buffer for SIMD loads. */
  float3 *positions_ptr = static_cast<float3 *>(rtcSetNewGeometryBuffer(
      geom_id, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, sizeof(float3), positions.size()));
  std::copy_n(positions.data(), positions.size(), positions_ptr);
}

static void add_mesh_faces(const BvhBuildContext &ctx, const int id, const Mesh &mesh)
{
  RTCGeometry geom_id = rtcNewGeometry(ctx.device, RTC_GEOMETRY_TYPE_TRIANGLE);
  rtcSetGeometryBuildQuality(geom_id, ctx.build_quality);

  const Span<float3> positions = mesh.vert_positions();
  const Span<int> corner_verts = mesh.corner_verts();
  if (all_faces_are_triangles(mesh)) {
    rtcSetSharedGeometryBuffer(geom_id,
                               RTC_BUFFER_TYPE_INDEX,
                               0,
                               RTC_FORMAT_UINT3,
                               corner_verts.data(),
                               0,
                               sizeof(int3),
                               corner_verts.cast<int3>().size());
  }
  else {
    const Span<int3> corner_tris = mesh.corner_tris();
    uint3 *rtc_indices = static_cast<uint3 *>(rtcSetNewGeometryBuffer(
        geom_id, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, sizeof(int3), corner_tris.size()));
    mesh::vert_tris_from_corner_tris(
        corner_verts, corner_tris, MutableSpan(rtc_indices, corner_tris.size()).cast<int3>());
  }
  add_positions(positions, geom_id);

  rtcCommitGeometry(geom_id);
  rtcAttachGeometryByID(ctx.scene, geom_id, id);
  rtcReleaseGeometry(geom_id);
}

static void add_mesh_faces(const BvhBuildContext &ctx,
                           Vector<Array<int, 0>> &index_map_by_geom,
                           const int id,
                           const Mesh &mesh,
                           const IndexMask &face_mask,
                           const bool map_global_indices)
{
  RTCGeometry geom_id = rtcNewGeometry(ctx.device, RTC_GEOMETRY_TYPE_TRIANGLE);
  rtcSetGeometryBuildQuality(geom_id, ctx.build_quality);

  const Span<float3> positions = mesh.vert_positions();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int3> corner_tris = mesh.corner_tris();
  /* Use #int instead of the default #int64_t for internal indices. */
  VectorSet<int,
            128,
            DefaultProbingStrategy,
            DefaultHash<int>,
            DefaultEquality<int>,
            SimpleVectorSetSlot<int, int>,
            GuardedAllocator>
      used_verts;
  used_verts.reserve(faces.size());
  face_mask.foreach_index(
      [&](const int face) { used_verts.add_multiple(corner_verts.slice(faces[face])); });

  Array<int, 128> masked_face_offset_data(face_mask.size() + 1);
  const OffsetIndices<int> mask_faces = offset_indices::gather_selected_offsets(
      faces, face_mask, masked_face_offset_data);

  const int tris_num = poly_to_tri_count(face_mask.size(), mask_faces.total_size());

  Array<int, 0> global_index_map;
  if (map_global_indices) {
    global_index_map.reinitialize(tris_num);
    face_mask.foreach_index_optimized<int>(
        [&](const int face, const int pos) {
          const IndexRange src_tris = mesh::face_triangles_range(faces, face);
          const IndexRange dst_tris = mesh::face_triangles_range(mask_faces, pos);
          for (const int i : src_tris.index_range()) {
            global_index_map[dst_tris[i]] = src_tris[i];
          }
        },
        exec_mode::grain_size(1024));
  }
  index_map_by_geom.append(std::move(global_index_map));

  uint3 *rtc_indices = static_cast<uint3 *>(rtcSetNewGeometryBuffer(
      geom_id, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, sizeof(int3), tris_num));
  face_mask.foreach_index_optimized<int>(
      [&](const int face, const int pos) {
        const IndexRange src_tris = mesh::face_triangles_range(faces, face);
        const IndexRange dst_tris = mesh::face_triangles_range(mask_faces, pos);
        for (const int i : src_tris.index_range()) {
          const int3 &tri = corner_tris[src_tris[i]];
          rtc_indices[dst_tris[i]] = uint3(used_verts.index_of(corner_verts[tri[0]]),
                                           used_verts.index_of(corner_verts[tri[1]]),
                                           used_verts.index_of(corner_verts[tri[2]]));
        }
      },
      exec_mode::grain_size(1024));

  /* If #rtcSetSharedGeometryBuffer could be used, remapping to avoid copying unnecessary
   * vertex positions could be unnecessary. Sharing the buffer currently doesn't work because
   * Embree must load past the end of the buffer for SIMD loads. */
  float3 *positions_ptr = static_cast<float3 *>(rtcSetNewGeometryBuffer(
      geom_id, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, sizeof(float3), used_verts.size()));
  array_utils::gather<float3>(positions, used_verts.as_span(), {positions_ptr, used_verts.size()});

  rtcCommitGeometry(geom_id);
  rtcAttachGeometryByID(ctx.scene, geom_id, id);
  rtcReleaseGeometry(geom_id);
}

#else /* WITH_EMBREE */

struct MeshFallbackTree : public Tree::FallbackTree {
  BVHTreeFromMesh bvh_from_mesh;

  MeshFallbackTree(const Mesh &mesh, const IndexMask &mask, const bool map_global_indices)
  {
    bvh_from_mesh = bvhtree_from_mesh_corner_tris_ex(mesh.vert_positions(),
                                                     mesh.faces(),
                                                     mesh.corner_verts(),
                                                     mesh.corner_tris(),
                                                     mask,
                                                     map_global_indices);
  }
  ~MeshFallbackTree() override = default;
};

/**
 * The BVH callbacks take a mutable `void *` user-data pointer even though they only read from it.
 * The tree data is logically const here, so casting away const is safe.
 */
static BVHTreeFromMesh *fallback_mesh_data(const Tree::FallbackTree &fallback_tree)
{
  const MeshFallbackTree &mesh_tree = static_cast<const MeshFallbackTree &>(fallback_tree);
  return const_cast<BVHTreeFromMesh *>(&mesh_tree.bvh_from_mesh);
}

#endif

Tree Tree::from_tris(const Mesh &mesh, const IndexMask &face_mask, const bool map_global_indices)
{
  if (face_mask.size() == mesh.faces_num) {
    return from_single_mesh(mesh);
  }
  Tree tree;
#ifdef WITH_EMBREE
  tree.rtc_device_ = rtcNewDevice("verbose=0");

  rtcSetDeviceErrorFunction(tree.rtc_device_, rtc_error_func, nullptr);
  rtcSetDeviceMemoryMonitorFunction(tree.rtc_device_, rtc_memory_monitor_func, nullptr);

  tree.rtc_scene_ = rtcNewScene(tree.rtc_device_);
  const RTCSceneFlags scene_flags = RTCSceneFlags(RTC_SCENE_FLAG_ROBUST |
                                                  RTC_SCENE_FLAG_FILTER_FUNCTION_IN_ARGUMENTS);
  rtcSetSceneFlags(tree.rtc_scene_, scene_flags);
  RTCBuildQuality build_quality = RTC_BUILD_QUALITY_MEDIUM;
  rtcSetSceneBuildQuality(tree.rtc_scene_, build_quality);

  BvhBuildContext ctx{tree.rtc_device_, tree.rtc_scene_, build_quality};

  add_mesh_faces(ctx, tree.index_map_by_geom_, 0, mesh, face_mask, map_global_indices);

  rtcSetSceneProgressMonitorFunction(tree.rtc_scene_, rtc_progress_func, nullptr);
  rtcCommitScene(tree.rtc_scene_);
#else  /* WITH_EMBREE */
  tree.fallback_tree_ = std::make_unique<MeshFallbackTree>(mesh, face_mask, map_global_indices);
#endif /* WITH_EMBREE */

  return tree;
}

Tree Tree::from_single_mesh(const Mesh &mesh)
{
  Tree tree;
#ifdef WITH_EMBREE
  tree.rtc_device_ = rtcNewDevice("verbose=0");

  rtcSetDeviceErrorFunction(tree.rtc_device_, rtc_error_func, nullptr);
  rtcSetDeviceMemoryMonitorFunction(tree.rtc_device_, rtc_memory_monitor_func, nullptr);

  tree.rtc_scene_ = rtcNewScene(tree.rtc_device_);
  const RTCSceneFlags scene_flags = RTCSceneFlags(RTC_SCENE_FLAG_ROBUST |
                                                  RTC_SCENE_FLAG_FILTER_FUNCTION_IN_ARGUMENTS);
  rtcSetSceneFlags(tree.rtc_scene_, scene_flags);
  RTCBuildQuality build_quality = RTC_BUILD_QUALITY_MEDIUM;
  rtcSetSceneBuildQuality(tree.rtc_scene_, build_quality);

  BvhBuildContext ctx{tree.rtc_device_, tree.rtc_scene_, build_quality};

  add_mesh_faces(ctx, 0, mesh);
  tree.index_map_by_geom_.append({});

  rtcSetSceneProgressMonitorFunction(tree.rtc_scene_, rtc_progress_func, nullptr);
  rtcCommitScene(tree.rtc_scene_);
#else  /* WITH_EMBREE */
  tree.fallback_tree_ = std::make_unique<MeshFallbackTree>(mesh, IndexMask(mesh.faces_num), true);
#endif /* WITH_EMBREE */

  return tree;
}

std::optional<RayHit> Tree::ray_intersect(const Ray &ray) const
{
#ifdef WITH_EMBREE
  RTCRayHit rtc_hit;
  rtc_hit.ray.org_x = ray.origin.x;
  rtc_hit.ray.org_y = ray.origin.y;
  rtc_hit.ray.org_z = ray.origin.z;
  rtc_hit.ray.dir_x = ray.direction.x;
  rtc_hit.ray.dir_y = ray.direction.y;
  rtc_hit.ray.dir_z = ray.direction.z;
  /* TODO: BLI_kdopbvh fallback does not support a minimum distance. */
  rtc_hit.ray.tnear = 0.0f;
  rtc_hit.ray.tfar = ray.dist_max;
  rtc_hit.ray.time = 0.0f;
  rtc_hit.ray.mask = 0xffffffff;
  rtc_hit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
  rtc_hit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
  rtcIntersect1(rtc_scene_, &rtc_hit);
  if (rtc_hit.hit.geomID == RTC_INVALID_GEOMETRY_ID ||
      rtc_hit.hit.primID == RTC_INVALID_GEOMETRY_ID)
  {
    return std::nullopt;
  }

  RayHit hit;
  hit.normal = float3(rtc_hit.hit.Ng_x, rtc_hit.hit.Ng_y, rtc_hit.hit.Ng_z);
  hit.bary_coord = bary_coord_embree_to_blender(rtc_hit.hit.u, rtc_hit.hit.v);
  hit.index = rtc_hit.hit.primID;
  if (!index_map_by_geom_[rtc_hit.hit.geomID].is_empty()) {
    hit.index = index_map_by_geom_[rtc_hit.hit.geomID][hit.index];
  }
  hit.distance = rtc_hit.ray.tfar;
  return hit;
#else /* WITH_EMBREE */
  BVHTreeFromMesh *data = fallback_mesh_data(*this->fallback_tree_);
  if (!data->tree) {
    return std::nullopt;
  }

  BVHTreeRayHit bvh_hit;
  bvh_hit.index = -1;
  bvh_hit.dist = ray.dist_max;
  BLI_bvhtree_ray_cast(
      data->tree, ray.origin, ray.direction, 0.0f, &bvh_hit, data->raycast_callback, data);
  if (bvh_hit.index == -1) {
    return std::nullopt;
  }

  RayHit hit;
  hit.normal = float3(bvh_hit.no);
  hit.index = bvh_hit.index;
  hit.distance = bvh_hit.dist;
  hit.bary_coord = bke::mesh_surface_sample::compute_bary_coord_in_triangle(
      data->vert_positions,
      data->corner_verts,
      data->corner_tris[bvh_hit.index],
      hit.position(ray));
  return hit;
#endif
}

void Tree::ray_intersect_all(const Ray &ray, FunctionRef<void(const RayHit &)> fn) const
{
#ifdef WITH_EMBREE
  struct AllHitsContext {
    RTCRayQueryContext rtc_context;
    FunctionRef<void(const RayHit &)> *fn;
    Span<Array<int, 0>> index_map_by_geom;
    float3 origin;
    float3 direction;
  };

  AllHitsContext ctx;
  rtcInitRayQueryContext(&ctx.rtc_context);
  ctx.fn = &fn;
  ctx.index_map_by_geom = index_map_by_geom_;
  ctx.origin = ray.origin;
  ctx.direction = ray.direction;

  RTCRayHit rtc_hit;
  rtc_hit.ray.org_x = ray.origin.x;
  rtc_hit.ray.org_y = ray.origin.y;
  rtc_hit.ray.org_z = ray.origin.z;
  rtc_hit.ray.dir_x = ray.direction.x;
  rtc_hit.ray.dir_y = ray.direction.y;
  rtc_hit.ray.dir_z = ray.direction.z;
  rtc_hit.ray.tnear = 0.0f;
  rtc_hit.ray.tfar = ray.dist_max;
  rtc_hit.ray.time = 0.0f;
  rtc_hit.ray.mask = 0xffffffff;
  rtc_hit.ray.id = 0;
  rtc_hit.ray.flags = 0;
  rtc_hit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
  rtc_hit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;

  RTCIntersectArguments args;
  rtcInitIntersectArguments(&args);
  args.context = &ctx.rtc_context;
  args.filter = [](const RTCFilterFunctionNArguments *filter_args) {
    AllHitsContext *ctx = reinterpret_cast<AllHitsContext *>(filter_args->context);
    const RTCHit rtc_hit = rtcGetHitFromHitN(filter_args->hit, filter_args->N, 0);
    const RTCRay rtc_ray = rtcGetRayFromRayN(filter_args->ray, filter_args->N, 0);

    RayHit hit;
    hit.normal = float3(rtc_hit.Ng_x, rtc_hit.Ng_y, rtc_hit.Ng_z);
    hit.bary_coord = bary_coord_embree_to_blender(rtc_hit.u, rtc_hit.v);
    hit.index = int(rtc_hit.primID);
    if (!ctx->index_map_by_geom[rtc_hit.geomID].is_empty()) {
      hit.index = ctx->index_map_by_geom[rtc_hit.geomID][hit.index];
    }
    hit.distance = rtc_ray.tfar;
    (*ctx->fn)(hit);

    /* Reject hit to continue traversal for all remaining intersections. */
    filter_args->valid[0] = 0;
  };

  rtcIntersect1(this->rtc_scene_, &rtc_hit, &args);
#else /* WITH_EMBREE */
  BVHTreeFromMesh *data = fallback_mesh_data(*this->fallback_tree_);
  if (!data->tree) {
    return;
  }

  struct AllHitsContext {
    Ray ray;
    const BVHTreeFromMesh *data;
    FunctionRef<void(const RayHit &)> fn;
  };
  AllHitsContext ctx{ray, data, fn};

  BLI_bvhtree_ray_cast_all(
      data->tree,
      ray.origin,
      ray.direction,
      0.0f,
      ray.dist_max,
      [](void *userdata, const int index, const BVHTreeRay *bvh_ray, BVHTreeRayHit *hit) {
        AllHitsContext &ctx = *static_cast<AllHitsContext *>(userdata);
        /* Run the intersection test into a local hit so the traversal distance stays at its
         * maximum and every intersection is reported rather than only the closest one. */
        BVHTreeRayHit local_hit;
        local_hit.index = -1;
        local_hit.dist = hit->dist;
        ctx.data->raycast_callback(
            const_cast<BVHTreeFromMesh *>(ctx.data), index, bvh_ray, &local_hit);
        if (local_hit.index == -1) {
          return;
        }
        RayHit result;
        result.normal = float3(local_hit.no);
        result.index = local_hit.index;
        result.distance = local_hit.dist;
        result.bary_coord = bke::mesh_surface_sample::compute_bary_coord_in_triangle(
            ctx.data->vert_positions,
            ctx.data->corner_verts,
            ctx.data->corner_tris[local_hit.index],
            result.position(ctx.ray));
        ctx.fn(result);
      },
      &ctx);
#endif
}

#ifdef WITH_EMBREE

struct ClosestPointUserData {
  RTCScene rtc_scene;
  ClosestPointResult &result;
  bool has_result = false;
};

static bool closest_point_fn(RTCPointQueryFunctionArguments *args)
{
  auto &user_data = *static_cast<ClosestPointUserData *>(args->userPtr);
  const RTCScene scene = user_data.rtc_scene;
  RTCGeometry geom = rtcGetGeometry(scene, args->geomID);

  const float3 *positions = static_cast<const float3 *>(
      rtcGetGeometryBufferData(geom, RTC_BUFFER_TYPE_VERTEX, 0));
  const uint3 *indices = static_cast<const uint3 *>(
      rtcGetGeometryBufferData(geom, RTC_BUFFER_TYPE_INDEX, 0));
  const uint3 tri = indices[args->primID];

  float3 nearest_position;
  float3 bary_coord;
  closest_on_tri_to_point_v3(nearest_position,
                             bary_coord,
                             &args->query->x,
                             positions[tri[0]],
                             positions[tri[1]],
                             positions[tri[2]]);

  const float distance = math::distance(float3(&args->query->x), nearest_position);
  if (distance > args->query->radius) {
    return false;
  }
  if (distance == args->query->radius) {
    if (!user_data.has_result) {
      /* The initial search radius is exclusive. */
      return false;
    }
    /* Multiple primitives can be exactly the same distance from the query point. The order can be
     * platform dependent. Break the tie by primitive so that the result is the same on every
     * platform. */
    if (args->geomID > user_data.result.geomID) {
      return false;
    }
    if (args->geomID == uint32_t(user_data.result.geomID) &&
        args->primID >= uint32_t(user_data.result.index))
    {
      return false;
    }
  }
  args->query->radius = distance;
  user_data.result.position = nearest_position;
  user_data.result.bary_coord = bary_coord;
  user_data.result.index = args->primID;
  user_data.result.geomID = args->geomID;
  user_data.has_result = true;
  return true;
}

#endif /* WITH_EMBREE */

std::optional<ClosestPointResult> Tree::closest_point(const float3 &point,
                                                      const float radius) const
{
#ifdef WITH_EMBREE
  RTCPointQuery query{};
  query.x = point.x;
  query.y = point.y;
  query.z = point.z;
  query.time = 0.0f;
  query.radius = radius;
  RTCPointQueryContext context{};
  rtcInitPointQueryContext(&context);
  ClosestPointResult result;
  ClosestPointUserData user_data(this->rtc_scene_, result);
  if (!rtcPointQuery(this->rtc_scene_, &query, &context, closest_point_fn, &user_data)) {
    return std::nullopt;
  }
  if (!index_map_by_geom_[result.geomID].is_empty()) {
    result.index = index_map_by_geom_[result.geomID][result.index];
  }
  return result;
#else /* WITH_EMBREE */
  BVHTreeFromMesh *data = fallback_mesh_data(*this->fallback_tree_);
  if (!data->tree) {
    return std::nullopt;
  }

  BVHTreeNearest nearest;
  nearest.index = -1;
  nearest.dist_sq = radius * radius;
  BLI_bvhtree_find_nearest(data->tree, point, &nearest, data->nearest_callback, data);
  if (nearest.index == -1) {
    return std::nullopt;
  }

  ClosestPointResult result;
  result.position = float3(nearest.co);
  result.bary_coord = bke::mesh_surface_sample::compute_bary_coord_in_triangle(
      data->vert_positions, data->corner_verts, data->corner_tris[nearest.index], result.position);
  result.index = nearest.index;
  result.geomID = 0;
  return result;
#endif
}

void Tree::range_query(const float3 &point, const float radius, FunctionRef<bool(int)> fn) const
{
#ifdef WITH_EMBREE
  RTCPointQuery query{};
  query.x = point.x;
  query.y = point.y;
  query.z = point.z;
  query.time = 0.0f;
  query.radius = radius;
  RTCPointQueryContext context{};
  rtcInitPointQueryContext(&context);
  struct UserData {
    const FunctionRef<bool(int)> &fn;
    Span<Array<int, 0>> index_map_by_geom;
  };
  UserData data{fn, index_map_by_geom_};
  rtcPointQuery(
      this->rtc_scene_,
      &query,
      &context,
      [](RTCPointQueryFunctionArguments *args) -> bool {
        UserData *data = static_cast<UserData *>(args->userPtr);
        int index = args->primID;
        if (!data->index_map_by_geom[args->geomID].is_empty()) {
          index = data->index_map_by_geom[args->geomID][index];
        }
        return data->fn(index);
      },
      &data);
#else /* WITH_EMBREE */
  BVHTreeFromMesh *data = fallback_mesh_data(*this->fallback_tree_);
  if (!data->tree) {
    return;
  }
  /* The kdop range query cannot stop traversal early, so once #fn requests termination by
   * returning false, skip calling it for the remaining indices to match Embree's behavior. */
  bool stop = false;
  BLI_bvhtree_range_query_cpp(
      *data->tree,
      point,
      radius,
      [&](const int index, const float3 & /*co*/, const float /*dist_sq*/) {
        if (stop) {
          return;
        }
        stop = !fn(index);
      });
#endif
}

}  // namespace blender::bke::bvh
