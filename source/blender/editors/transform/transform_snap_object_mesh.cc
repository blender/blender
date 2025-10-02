/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"

#include "BKE_bvhutils.hh"
#include "BKE_mesh.hh"

#include "ED_transform_snap_object_context.hh"
#include "ED_view3d.hh"

#include "transform_snap_object.hh"

#ifdef DEBUG_SNAP_TIME
#  if WIN32 and NDEBUG
#    pragma optimize("t", on)
#  endif
#endif

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Snap Object Data
 * \{ */

static void snap_object_data_mesh_get(const Mesh *mesh_eval,
                                      bool skip_hidden,
                                      bke::BVHTreeFromMesh *r_treedata)
{
  /* The BVHTree from corner_tris is always required. */
  if (skip_hidden) {
    *r_treedata = mesh_eval->bvh_corner_tris_no_hidden();
  }
  else {
    *r_treedata = mesh_eval->bvh_corner_tris();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ray Cast Functions
 * \{ */

/* Store all ray-hits
 * Support for storing all depths, not just the first (ray-cast 'all'). */

/* Callback to ray-cast with back-face culling (#Mesh). */
static void mesh_corner_tris_raycast_backface_culling_cb(void *userdata,
                                                         int index,
                                                         const BVHTreeRay *ray,
                                                         BVHTreeRayHit *hit)
{
  const bke::BVHTreeFromMesh *data = (bke::BVHTreeFromMesh *)userdata;
  const Span<float3> positions = data->vert_positions;
  const int3 &tri = data->corner_tris[index];
  const float *vtri_co[3] = {
      positions[data->corner_verts[tri[0]]],
      positions[data->corner_verts[tri[1]]],
      positions[data->corner_verts[tri[2]]],
  };
  float dist = bke::bvhtree_ray_tri_intersection(ray, hit->dist, UNPACK3(vtri_co));

  if (dist >= 0 && dist < hit->dist) {
    float no[3];
    if (raycast_tri_backface_culling_test(ray->direction, UNPACK3(vtri_co), no)) {
      hit->index = index;
      hit->dist = dist;
      madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);
      normalize_v3_v3(hit->no, no);
    }
  }
}

static bool raycastMesh(SnapObjectContext *sctx,
                        const Object *ob_eval,
                        const Mesh *mesh_eval,
                        const float4x4 &obmat,
                        const uint ob_index,
                        bool use_hide)
{
  bool retval = false;

  if (mesh_eval->faces_num == 0) {
    return retval;
  }

  float4x4 imat = math::invert(obmat);
  float3 ray_start_local = math::transform_point(imat, sctx->runtime.ray_start);
  float3 ray_normal_local = math::transform_direction(imat, sctx->runtime.ray_dir);
  float local_scale, local_depth, len_diff = 0.0f;

  /* Local scale in normal direction. */
  ray_normal_local = math::normalize_and_get_length(ray_normal_local, local_scale);

  const bool is_in_front = (sctx->runtime.params.occlusion_test == SNAP_OCCLUSION_AS_SEEM) &&
                           (ob_eval->dtx & OB_DRAW_IN_FRONT);
  const float depth_max = is_in_front ? sctx->ret.ray_depth_max_in_front : sctx->ret.ray_depth_max;
  local_depth = depth_max;
  if (local_depth != BVH_RAYCAST_DIST_MAX) {
    local_depth *= local_scale;
  }

  /* Test bounding box. */
  if (std::optional<Bounds<float3>> bounds = mesh_eval->bounds_min_max()) {
    /* Was #BKE_boundbox_ray_hit_check, see: cf6ca226fa58 */
    if (!isect_ray_aabb_v3_simple(
            ray_start_local, ray_normal_local, bounds->min, bounds->max, &len_diff, nullptr))
    {
      return retval;
    }
  }

  /* We pass a temp ray_start, set from object's boundbox, to avoid precision issues with
   * very far away ray_start values (as returned in case of ortho view3d), see #50486, #38358.
   */
  if (len_diff > 400.0f) {
    /* Make temporary start point a bit away from bounding-box hit point. */
    len_diff -= local_scale;
    madd_v3_v3fl(ray_start_local, ray_normal_local, len_diff);
    local_depth -= len_diff;
  }
  else {
    len_diff = 0.0f;
  }

  bke::BVHTreeFromMesh treedata;
  snap_object_data_mesh_get(mesh_eval, use_hide, &treedata);

  const Span<int> tri_faces = mesh_eval->corner_tri_faces();

  if (treedata.tree == nullptr) {
    return retval;
  }

  BLI_assert(treedata.raycast_callback != nullptr);
  if (sctx->runtime.hit_list) {
    RayCastAll_Data data;

    data.bvhdata = &treedata;
    data.raycast_callback = treedata.raycast_callback;
    data.obmat = &obmat;
    data.len_diff = len_diff;
    data.local_scale = local_scale;
    data.ob_uuid = ob_index;
    data.hit_list = sctx->runtime.hit_list;

    void *hit_last_prev = data.hit_list->last;
    BLI_bvhtree_ray_cast_all(
        treedata.tree, ray_start_local, ray_normal_local, 0.0f, depth_max, raycast_all_cb, &data);

    retval = hit_last_prev != data.hit_list->last;
  }
  else {
    BVHTreeRayHit hit{};
    hit.index = -1;
    hit.dist = local_depth;

    if (BLI_bvhtree_ray_cast(treedata.tree,
                             ray_start_local,
                             ray_normal_local,
                             0.0f,
                             &hit,
                             sctx->runtime.params.use_backface_culling ?
                                 mesh_corner_tris_raycast_backface_culling_cb :
                                 treedata.raycast_callback,
                             &treedata) != -1)
    {
      hit.dist += len_diff;
      hit.dist /= local_scale;
      if (hit.dist <= depth_max) {
        hit.index = tri_faces[hit.index];
        retval = true;
      }
      SnapData::register_result_raycast(sctx, ob_eval, &mesh_eval->id, obmat, &hit, is_in_front);
    }
  }

  return retval;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface Snap Functions
 * \{ */

static bool nearest_world_mesh(SnapObjectContext *sctx,
                               const Object *ob_eval,
                               const Mesh *mesh_eval,
                               const float4x4 &obmat,
                               bool use_hide)
{
  bke::BVHTreeFromMesh treedata;
  snap_object_data_mesh_get(mesh_eval, use_hide, &treedata);
  if (treedata.tree == nullptr) {
    return false;
  }

  BVHTreeNearest nearest{};
  nearest.dist_sq = sctx->ret.dist_nearest_sq;
  if (nearest_world_tree(
          sctx, treedata.tree, treedata.nearest_callback, obmat, &treedata, &nearest))
  {
    SnapData::register_result(sctx, ob_eval, &mesh_eval->id, obmat, &nearest);
    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Subclass for Snapping to Edges or Points of a Mesh
 * \{ */

class SnapData_Mesh : public SnapData {
 public:
  const float3 *vert_positions;
  const float3 *vert_normals;
  const int2 *edges; /* Only used for #BVHTreeFromMeshEdges. */
  const int *corner_verts;
  const int *corner_edges;
  const int3 *corner_tris;

  SnapData_Mesh(SnapObjectContext *sctx, const Mesh *mesh_eval, const float4x4 &obmat)
      : SnapData(sctx, obmat)
  {
    this->vert_positions = mesh_eval->vert_positions().data();
    this->vert_normals = mesh_eval->vert_normals().data();
    this->edges = mesh_eval->edges().data();
    this->corner_verts = mesh_eval->corner_verts().data();
    this->corner_edges = mesh_eval->corner_edges().data();
    this->corner_tris = mesh_eval->corner_tris().data();
  };

  void get_vert_co(const int index, const float **r_co) override
  {
    *r_co = this->vert_positions[index];
  }

  void get_edge_verts_index(const int index, int r_v_index[2]) override
  {
    const int2 &edge = this->edges[index];
    r_v_index[0] = edge[0];
    r_v_index[1] = edge[1];
  }

  void copy_vert_no(const int index, float r_no[3]) override
  {
    copy_v3_v3(r_no, this->vert_normals[index]);
  }
};

static void cb_snap_edge_verts(void *userdata,
                               int index,
                               const DistProjectedAABBPrecalc *precalc,
                               const float (*clip_plane)[4],
                               const int clip_plane_len,
                               BVHTreeNearest *nearest)
{
  SnapData_Mesh *data = static_cast<SnapData_Mesh *>(userdata);

  int vindex[2];
  data->get_edge_verts_index(index, vindex);

  for (int i = 2; i--;) {
    if (vindex[i] == nearest->index) {
      continue;
    }
    cb_snap_vert(userdata, vindex[i], precalc, clip_plane, clip_plane_len, nearest);
  }
}

static void cb_snap_tri_verts(void *userdata,
                              int index,
                              const DistProjectedAABBPrecalc *precalc,
                              const float (*clip_plane)[4],
                              const int clip_plane_len,
                              BVHTreeNearest *nearest)
{
  SnapData_Mesh *data = static_cast<SnapData_Mesh *>(userdata);

  int vindex[3];
  const int *corner_verts = data->corner_verts;
  const int3 &tri = data->corner_tris[index];
  vindex[0] = corner_verts[tri[0]];
  vindex[1] = corner_verts[tri[1]];
  vindex[2] = corner_verts[tri[2]];

  if (data->use_backface_culling) {
    const float3 *vert_positions = data->vert_positions;
    const float3 &t0 = vert_positions[vindex[0]];
    const float3 &t1 = vert_positions[vindex[1]];
    const float3 &t2 = vert_positions[vindex[2]];
    float dummy[3];
    if (raycast_tri_backface_culling_test(precalc->ray_direction, t0, t1, t2, dummy)) {
      return;
    }
  }

  for (int i = 3; i--;) {
    if (vindex[i] == nearest->index) {
      continue;
    }
    cb_snap_vert(userdata, vindex[i], precalc, clip_plane, clip_plane_len, nearest);
  }
}

static void cb_snap_tri_edges(void *userdata,
                              int index,
                              const DistProjectedAABBPrecalc *precalc,
                              const float (*clip_plane)[4],
                              const int clip_plane_len,
                              BVHTreeNearest *nearest)
{
  SnapData_Mesh *data = static_cast<SnapData_Mesh *>(userdata);
  const int *corner_verts = data->corner_verts;
  const int3 &tri = data->corner_tris[index];

  if (data->use_backface_culling) {
    const float3 *vert_positions = data->vert_positions;
    const float3 &t0 = vert_positions[corner_verts[tri[0]]];
    const float3 &t1 = vert_positions[corner_verts[tri[1]]];
    const float3 &t2 = vert_positions[corner_verts[tri[2]]];
    float dummy[3];
    if (raycast_tri_backface_culling_test(precalc->ray_direction, t0, t1, t2, dummy)) {
      return;
    }
  }

  const int2 *edges = data->edges;
  const int *corner_edges = data->corner_edges;
  for (int j = 2, j_next = 0; j_next < 3; j = j_next++) {
    int eindex = corner_edges[tri[j]];
    const int2 &edge = edges[eindex];
    const int2 tri_edge = {corner_verts[tri[j]], corner_verts[tri[j_next]]};
    if (ELEM(edge[0], tri_edge[0], tri_edge[1]) && ELEM(edge[1], tri_edge[0], tri_edge[1])) {
      if (eindex == nearest->index) {
        continue;
      }
      cb_snap_edge(userdata, eindex, precalc, clip_plane, clip_plane_len, nearest);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Object Snapping API
 * \{ */

eSnapMode snap_polygon_mesh(SnapObjectContext *sctx,
                            const Object *ob_eval,
                            const ID *id,
                            const float4x4 &obmat,
                            eSnapMode snap_to_flag,
                            int face_index)
{
  eSnapMode elem = SCE_SNAP_TO_NONE;

  const Mesh *mesh_eval = reinterpret_cast<const Mesh *>(id);

  SnapData_Mesh nearest2d(sctx, mesh_eval, obmat);
  nearest2d.clip_planes_enable(sctx, ob_eval);

  BVHTreeNearest nearest{};
  nearest.index = -1;
  nearest.dist_sq = sctx->ret.dist_px_sq;

  const IndexRange face = mesh_eval->faces()[face_index];

  if (snap_to_flag &
      (SCE_SNAP_TO_EDGE | SCE_SNAP_TO_EDGE_MIDPOINT | SCE_SNAP_TO_EDGE_PERPENDICULAR))
  {
    /* We return "Snap to Edge" even if the intent is "Snap to Edge Midpoint" or
     * "Snap to Edge Perpendicular".
     * This avoids complexity. These snap points will be tested in `snap_edge_points`. */
    elem = SCE_SNAP_TO_EDGE;
    BLI_assert(nearest2d.edges != nullptr);
    const int *face_edges = &nearest2d.corner_edges[face.start()];
    for (int i = face.size(); i--;) {
      cb_snap_edge(&nearest2d,
                   face_edges[i],
                   &nearest2d.nearest_precalc,
                   reinterpret_cast<float (*)[4]>(nearest2d.clip_planes.data()),
                   nearest2d.clip_planes.size(),
                   &nearest);
    }
  }
  else {
    elem = SCE_SNAP_TO_EDGE_ENDPOINT;
    const int *face_verts = &nearest2d.corner_verts[face.start()];
    for (int i = face.size(); i--;) {
      cb_snap_vert(&nearest2d,
                   face_verts[i],
                   &nearest2d.nearest_precalc,
                   reinterpret_cast<float (*)[4]>(nearest2d.clip_planes.data()),
                   nearest2d.clip_planes.size(),
                   &nearest);
    }
  }

  if (nearest.index != -1) {
    nearest2d.nearest_point = nearest;
    nearest2d.register_result(sctx, ob_eval, id);
    return elem;
  }

  return SCE_SNAP_TO_NONE;
}

eSnapMode snap_edge_points_mesh(SnapObjectContext *sctx,
                                const Object *ob_eval,
                                const ID *id,
                                const float4x4 &obmat,
                                float dist_px_sq_orig,
                                int edge_index)
{
  SnapData_Mesh nearest2d(sctx, reinterpret_cast<const Mesh *>(id), obmat);
  eSnapMode elem = nearest2d.snap_edge_points_impl(sctx, edge_index, dist_px_sq_orig);
  if (nearest2d.nearest_point.index != -2) {
    nearest2d.register_result(sctx, ob_eval, id);
  }
  return elem;
}

static eSnapMode mesh_snap_mode_supported(const Mesh *mesh, bool skip_hidden)
{
  /* When skipping hidden geometry, we still cannot obtain the number of loose verts
   * until computing #BVHTREE_FROM_LOOSEVERTS_NO_HIDDEN. Therefore, consider #SCE_SNAP_TO_POINT
   * supported even if the mesh has no loose vertices in this case. */
  eSnapMode snap_mode_supported = (skip_hidden || mesh->loose_verts().count) ? SCE_SNAP_TO_POINT :
                                                                               SCE_SNAP_TO_NONE;
  if (mesh->faces_num) {
    snap_mode_supported |= SCE_SNAP_TO_FACE | SCE_SNAP_INDIVIDUAL_NEAREST | SNAP_TO_EDGE_ELEMENTS;
  }
  else if (mesh->edges_num) {
    snap_mode_supported |= SNAP_TO_EDGE_ELEMENTS;
  }

  return snap_mode_supported;
}

static eSnapMode snapMesh(SnapObjectContext *sctx,
                          const Object *ob_eval,
                          const Mesh *mesh_eval,
                          const float4x4 &obmat,
                          bool skip_hidden,
                          bool is_editmesh,
                          eSnapMode snap_to)
{
  BLI_assert(snap_to != SCE_SNAP_TO_FACE);
  SnapData_Mesh nearest2d(sctx, mesh_eval, obmat);
  if (is_editmesh) {
    nearest2d.use_backface_culling = false;
  }

  if (std::optional<Bounds<float3>> bounds = mesh_eval->bounds_min_max()) {
    if (!nearest2d.snap_boundbox(bounds->min, bounds->max)) {
      return SCE_SNAP_TO_NONE;
    }
  }

  snap_to &= mesh_snap_mode_supported(mesh_eval, skip_hidden) &
             (SNAP_TO_EDGE_ELEMENTS | SCE_SNAP_TO_POINT);
  if (snap_to == SCE_SNAP_TO_NONE) {
    return SCE_SNAP_TO_NONE;
  }

  bke::BVHTreeFromMesh treedata;
  snap_object_data_mesh_get(mesh_eval, skip_hidden, &treedata);

  const BVHTree *bvhtree[2] = {nullptr};
  bvhtree[0] = skip_hidden ? mesh_eval->bvh_loose_no_hidden_edges().tree :
                             mesh_eval->bvh_loose_edges().tree;
  if (snap_to & SCE_SNAP_TO_POINT) {
    bvhtree[1] = skip_hidden ? mesh_eval->bvh_loose_no_hidden_verts().tree :
                               mesh_eval->bvh_loose_verts().tree;
  }

  /* #XRAY_ENABLED can return false even with the XRAY flag enabled, this happens because the
   * alpha is 1.0 in this case. But even with the alpha being 1.0, the edit mesh is still not
   * occluded. */
  const bool skip_occlusion_plane = is_editmesh && XRAY_FLAG_ENABLED(sctx->runtime.v3d);
  nearest2d.clip_planes_enable(sctx, ob_eval, skip_occlusion_plane);

  BVHTreeNearest nearest{};
  nearest.index = -1;
  nearest.dist_sq = sctx->ret.dist_px_sq;

  int last_index = nearest.index;
  eSnapMode elem = SCE_SNAP_TO_NONE;

  if (bvhtree[1]) {
    BLI_assert(snap_to & SCE_SNAP_TO_POINT);
    /* Snap to loose verts. */
    BLI_bvhtree_find_nearest_projected(
        bvhtree[1],
        nearest2d.pmat_local.ptr(),
        sctx->runtime.win_size,
        sctx->runtime.mval,
        reinterpret_cast<float (*)[4]>(nearest2d.clip_planes.data()),
        nearest2d.clip_planes.size(),
        &nearest,
        cb_snap_vert,
        &nearest2d);

    if (nearest.index != -1) {
      last_index = nearest.index;
      elem = SCE_SNAP_TO_POINT;
    }
  }

  if (snap_to & (SNAP_TO_EDGE_ELEMENTS & ~SCE_SNAP_TO_EDGE_ENDPOINT)) {
    if (bvhtree[0]) {
      /* Snap to loose edges. */
      BLI_bvhtree_find_nearest_projected(
          bvhtree[0],
          nearest2d.pmat_local.ptr(),
          sctx->runtime.win_size,
          sctx->runtime.mval,
          reinterpret_cast<float (*)[4]>(nearest2d.clip_planes.data()),
          nearest2d.clip_planes.size(),
          &nearest,
          cb_snap_edge,
          &nearest2d);
    }

    if (treedata.tree) {
      /* Snap to corner_tris. */
      BLI_bvhtree_find_nearest_projected(
          treedata.tree,
          nearest2d.pmat_local.ptr(),
          sctx->runtime.win_size,
          sctx->runtime.mval,
          reinterpret_cast<float (*)[4]>(nearest2d.clip_planes.data()),
          nearest2d.clip_planes.size(),
          &nearest,
          cb_snap_tri_edges,
          &nearest2d);
    }

    if (last_index != nearest.index) {
      elem = SCE_SNAP_TO_EDGE;
    }
  }
  else if (snap_to & SCE_SNAP_TO_EDGE_ENDPOINT) {
    if (bvhtree[0]) {
      /* Snap to loose edges verts. */
      BLI_bvhtree_find_nearest_projected(
          bvhtree[0],
          nearest2d.pmat_local.ptr(),
          sctx->runtime.win_size,
          sctx->runtime.mval,
          reinterpret_cast<float (*)[4]>(nearest2d.clip_planes.data()),
          nearest2d.clip_planes.size(),
          &nearest,
          cb_snap_edge_verts,
          &nearest2d);
    }

    if (treedata.tree) {
      /* Snap to corner_tris verts. */
      BLI_bvhtree_find_nearest_projected(
          treedata.tree,
          nearest2d.pmat_local.ptr(),
          sctx->runtime.win_size,
          sctx->runtime.mval,
          reinterpret_cast<float (*)[4]>(nearest2d.clip_planes.data()),
          nearest2d.clip_planes.size(),
          &nearest,
          cb_snap_tri_verts,
          &nearest2d);
    }

    if (last_index != nearest.index) {
      elem = SCE_SNAP_TO_EDGE_ENDPOINT;
    }
  }

  if (nearest.index != -1) {
    nearest2d.nearest_point = nearest;
    nearest2d.register_result(sctx, ob_eval, &mesh_eval->id);
    return elem;
  }

  return SCE_SNAP_TO_NONE;
}

/** \} */

eSnapMode snap_object_mesh(SnapObjectContext *sctx,
                           const Object *ob_eval,
                           const ID *id,
                           const float4x4 &obmat,
                           eSnapMode snap_to_flag,
                           bool skip_hidden,
                           bool is_editmesh)
{
  eSnapMode elem = SCE_SNAP_TO_NONE;
  const Mesh *mesh_eval = reinterpret_cast<const Mesh *>(id);

  if (snap_to_flag & (SNAP_TO_EDGE_ELEMENTS | SCE_SNAP_TO_POINT)) {
    elem = snapMesh(sctx, ob_eval, mesh_eval, obmat, skip_hidden, is_editmesh, snap_to_flag);
    if (elem) {
      return elem;
    }
  }

  if (snap_to_flag & SCE_SNAP_TO_FACE) {
    if (raycastMesh(sctx, ob_eval, mesh_eval, obmat, sctx->runtime.object_index++, skip_hidden)) {
      return SCE_SNAP_TO_FACE;
    }
  }

  if (snap_to_flag & SCE_SNAP_INDIVIDUAL_NEAREST) {
    if (nearest_world_mesh(sctx, ob_eval, mesh_eval, obmat, skip_hidden)) {
      return SCE_SNAP_INDIVIDUAL_NEAREST;
    }
  }

  return SCE_SNAP_TO_NONE;
}

}  // namespace blender::ed::transform
