/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <algorithm>

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"

#include "DNA_screen_types.h"

#include "BKE_bvhutils.hh"
#include "BKE_duplilist.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_transform_snap_object_context.hh"
#include "ED_view3d.hh"

#include "transform_snap_object.hh"

#ifdef DEBUG_SNAP_TIME
#  include "BLI_timeit.hh"
#  include <iostream>

#  if WIN32 and NDEBUG
#    pragma optimize("t", on)
#  endif

static int64_t total_count_ = 0;
static blender::timeit::Nanoseconds duration_;
#endif

namespace blender::ed::transform {

static float4 occlusion_plane_create(float3 ray_dir, float3 ray_co, float3 ray_no)
{
  float4 plane;
  plane_from_point_normal_v3(plane, ray_co, ray_no);
  if (dot_v3v3(ray_dir, plane) > 0.0f) {
    /* The plane is facing the wrong direction. */
    negate_v4(plane);
  }

  /* Small offset to simulate a kind of volume for edges and vertices. */
  plane[3] += 0.01f;

  return plane;
}

static bool test_projected_vert_dist(const DistProjectedAABBPrecalc *precalc,
                                     const float (*clip_plane)[4],
                                     const int clip_plane_len,
                                     const bool is_persp,
                                     const float *co,
                                     BVHTreeNearest *nearest)
{
  if (!isect_point_planes_v3_negated(clip_plane, clip_plane_len, co)) {
    return false;
  }

  float co2d[2] = {
      (dot_m4_v3_row_x(precalc->pmat, co) + precalc->pmat[3][0]),
      (dot_m4_v3_row_y(precalc->pmat, co) + precalc->pmat[3][1]),
  };

  if (is_persp) {
    float w = mul_project_m4_v3_zfac(precalc->pmat, co);
    mul_v2_fl(co2d, 1.0f / w);
  }

  const float dist_sq = len_squared_v2v2(precalc->mval, co2d);
  if (dist_sq < nearest->dist_sq) {
    copy_v3_v3(nearest->co, co);
    nearest->dist_sq = dist_sq;
    return true;
  }
  return false;
}

static bool test_projected_edge_dist(const DistProjectedAABBPrecalc *precalc,
                                     const float (*clip_plane)[4],
                                     const int clip_plane_len,
                                     const bool is_persp,
                                     const float va[3],
                                     const float vb[3],
                                     BVHTreeNearest *nearest)
{
  float near_co[3];
  closest_ray_to_segment_v3(precalc->ray_origin, precalc->ray_direction, va, vb, near_co);
  return test_projected_vert_dist(precalc, clip_plane, clip_plane_len, is_persp, near_co, nearest);
}

SnapData::SnapData(SnapObjectContext *sctx, const float4x4 &obmat)
    : nearest_precalc(),
      obmat_(obmat),
      is_persp(sctx->runtime.rv3d ? sctx->runtime.rv3d->is_persp : false),
      use_backface_culling(sctx->runtime.params.use_backface_culling)
{
  if (sctx->runtime.rv3d) {
    this->pmat_local = float4x4(sctx->runtime.rv3d->persmat) * obmat;
  }
  else {
    this->pmat_local = obmat;
  }

  dist_squared_to_projected_aabb_precalc(
      &this->nearest_precalc, this->pmat_local.ptr(), sctx->runtime.win_size, sctx->runtime.mval);

  this->nearest_point.index = -2;
  this->nearest_point.dist_sq = sctx->ret.dist_px_sq;
  copy_v3_fl3(this->nearest_point.no, 0.0f, 0.0f, 1.0f);
}

void SnapData::clip_planes_enable(SnapObjectContext *sctx,
                                  const Object *ob_eval,
                                  bool skip_occlusion_plane)
{
  float4x4 tobmat = math::transpose(this->obmat_);
  if (!skip_occlusion_plane) {
    const bool is_in_front = (sctx->runtime.params.occlusion_test == SNAP_OCCLUSION_AS_SEEM) &&
                             ob_eval && (ob_eval->dtx & OB_DRAW_IN_FRONT) != 0;
    if (!is_in_front && sctx->runtime.has_occlusion_plane) {
      this->clip_planes.append(tobmat * sctx->runtime.occlusion_plane);
    }
    else if (sctx->runtime.has_occlusion_plane_in_front) {
      this->clip_planes.append(tobmat * sctx->runtime.occlusion_plane_in_front);
    }
  }

  for (float4 &plane : sctx->runtime.clip_planes) {
    this->clip_planes.append(tobmat * plane);
  }
}

bool SnapData::snap_boundbox(const float3 &min, const float3 &max)
{
  /* In vertex and edges you need to get the pixel distance from ray to bounding box,
   * see: #46099, #46816 */

#ifdef TEST_CLIPPLANES_IN_BOUNDBOX
  int isect_type = isect_aabb_planes_v3(
      reinterpret_cast<const float (*)[4]>(this->clip_planes.data()),
      this->clip_planes.size(),
      min,
      max);

  if (isect_type == ISECT_AABB_PLANE_BEHIND_ANY) {
    return false;
  }
#endif

  bool dummy[3];
  float bb_dist_px_sq = dist_squared_to_projected_aabb(&this->nearest_precalc, min, max, dummy);
  if (bb_dist_px_sq > this->nearest_point.dist_sq) {
    return false;
  }

  return true;
}

bool SnapData::snap_point(const float3 &co, int index)
{
  if (test_projected_vert_dist(&this->nearest_precalc,
                               reinterpret_cast<const float (*)[4]>(this->clip_planes.data()),
                               this->clip_planes.size(),
                               this->is_persp,
                               co,
                               &this->nearest_point))
  {
    this->nearest_point.index = index;
    return true;
  }
  return false;
}

bool SnapData::snap_edge(const float3 &va, const float3 &vb, int edge_index)
{
  if (test_projected_edge_dist(&this->nearest_precalc,
                               reinterpret_cast<const float (*)[4]>(this->clip_planes.data()),
                               this->clip_planes.size(),
                               this->is_persp,
                               va,
                               vb,
                               &this->nearest_point))
  {
    this->nearest_point.index = edge_index;
    sub_v3_v3v3(this->nearest_point.no, vb, va);
    return true;
  }
  return false;
}

eSnapMode SnapData::snap_edge_points_impl(SnapObjectContext *sctx,
                                          int edge_index,
                                          float dist_px_sq_orig)
{
  eSnapMode elem = SCE_SNAP_TO_EDGE;

  int vindex[2];
  this->get_edge_verts_index(edge_index, vindex);

  const float *v_pair[2];
  this->get_vert_co(vindex[0], &v_pair[0]);
  this->get_vert_co(vindex[1], &v_pair[1]);

  float lambda;
  if (!isect_ray_line_v3(this->nearest_precalc.ray_origin,
                         this->nearest_precalc.ray_direction,
                         v_pair[0],
                         v_pair[1],
                         &lambda))
  {
    /* Do nothing. */
  }
  else {
    this->nearest_point.dist_sq = dist_px_sq_orig;

    eSnapMode snap_to = sctx->runtime.snap_to_flag;
    int e_mode_len = ((snap_to & SCE_SNAP_TO_EDGE) != 0) +
                     ((snap_to & SCE_SNAP_TO_EDGE_ENDPOINT) != 0) +
                     ((snap_to & SCE_SNAP_TO_EDGE_MIDPOINT) != 0);

    float range = 1.0f / (2 * e_mode_len - 1);

    if (snap_to & SCE_SNAP_TO_EDGE_MIDPOINT) {
      range *= e_mode_len - 1;
      if ((range) < lambda && lambda < (1.0f - range)) {
        float vmid[3];
        mid_v3_v3v3(vmid, v_pair[0], v_pair[1]);

        if (this->snap_point(vmid, edge_index)) {
          sub_v3_v3v3(this->nearest_point.no, v_pair[1], v_pair[0]);
          elem = SCE_SNAP_TO_EDGE_MIDPOINT;
        }
      }
    }

    if (snap_to & SCE_SNAP_TO_EDGE_PERPENDICULAR) {
      float v_near[3], va_g[3], vb_g[3];

      mul_v3_m4v3(va_g, this->obmat_.ptr(), v_pair[0]);
      mul_v3_m4v3(vb_g, this->obmat_.ptr(), v_pair[1]);
      float lambda_perp = line_point_factor_v3(sctx->runtime.curr_co, va_g, vb_g);

      if (IN_RANGE(lambda_perp, 0.0f, 1.0f)) {
        interp_v3_v3v3(v_near, v_pair[0], v_pair[1], lambda_perp);

        if (this->snap_point(v_near, edge_index)) {
          sub_v3_v3v3(this->nearest_point.no, v_pair[1], v_pair[0]);
          elem = SCE_SNAP_TO_EDGE_PERPENDICULAR;
        }
      }
    }

    /* Leave this one for last so it doesn't change the normal. */
    if (snap_to & SCE_SNAP_TO_EDGE_ENDPOINT) {
      if (lambda < (range) || (1.0f - range) < lambda) {
        int v_id = lambda < 0.5f ? 0 : 1;

        if (this->snap_point(v_pair[v_id], vindex[v_id])) {
          elem = SCE_SNAP_TO_EDGE_ENDPOINT;
          this->copy_vert_no(vindex[v_id], this->nearest_point.no);
        }
      }
    }
  }

  return elem;
}

void SnapData::register_result(SnapObjectContext *sctx,
                               const Object *ob_eval,
                               const ID *id_eval,
                               const float4x4 &obmat,
                               BVHTreeNearest *r_nearest)
{
  BLI_assert(r_nearest->index != -2);

  copy_v3_v3(sctx->ret.loc, r_nearest->co);
  copy_v3_v3(sctx->ret.no, r_nearest->no);
  sctx->ret.index = r_nearest->index;
  sctx->ret.obmat = obmat;
  sctx->ret.ob = ob_eval;
  sctx->ret.data = id_eval;
  sctx->ret.dist_px_sq = r_nearest->dist_sq;

  /* Global space. */
  sctx->ret.loc = math::transform_point(obmat, sctx->ret.loc);
  sctx->ret.no = math::normalize(math::transform_direction(obmat, sctx->ret.no));

#ifndef NDEBUG
  /* Make sure this is only called once. */
  r_nearest->index = -2;
#endif
}

void SnapData::register_result(SnapObjectContext *sctx, const Object *ob_eval, const ID *id_eval)
{
  register_result(sctx, ob_eval, id_eval, this->obmat_, &this->nearest_point);
}

void SnapData::register_result_raycast(SnapObjectContext *sctx,
                                       const Object *ob_eval,
                                       const ID *id_eval,
                                       const blender::float4x4 &obmat,
                                       const BVHTreeRayHit *hit,
                                       const bool is_in_front)
{
  const float depth_max = is_in_front ? sctx->ret.ray_depth_max_in_front : sctx->ret.ray_depth_max;
  if (hit->dist <= depth_max) {
    float3 co = math::transform_point(obmat, float3(hit->co));
    float3 no = math::normalize(math::transform_direction(obmat, float3(hit->no)));

    sctx->ret.loc = co;
    sctx->ret.no = no;
    sctx->ret.index = hit->index;
    sctx->ret.obmat = obmat;
    sctx->ret.ob = ob_eval;
    sctx->ret.data = id_eval;
    sctx->ret.ray_depth_max = std::min(hit->dist, sctx->ret.ray_depth_max);

    if (is_in_front) {
      sctx->runtime.occlusion_plane_in_front = occlusion_plane_create(
          sctx->runtime.ray_dir, co, no);
      sctx->runtime.has_occlusion_plane_in_front = true;
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * Mesh used for snapping.
 *
 * - When the return value is null the `BKE_editmesh_from_object(ob_eval)` should be used.
 * - In rare cases there is no evaluated mesh available and a null result doesn't imply an
 *   edit-mesh, so callers need to account for a null edit-mesh too, see: #96536.
 */
static const ID *data_for_snap(Object *ob_eval, eSnapEditType edit_mode_type, bool *r_use_hide)
{
  if (r_use_hide) {
    *r_use_hide = false;
  }

  /* Get evaluated edit mesh when in mesh edit mode. */
  if (ob_eval->type == OB_MESH && BKE_object_is_in_editmode(ob_eval)) {
    if (edit_mode_type == SNAP_GEOM_EDIT) {
      return nullptr;
    }

    const Mesh *editmesh_eval = (edit_mode_type == SNAP_GEOM_FINAL) ?
                                    BKE_object_get_editmesh_eval_final(ob_eval) :
                                (edit_mode_type == SNAP_GEOM_CAGE) ?
                                    BKE_object_get_editmesh_eval_cage(ob_eval) :
                                    nullptr;

    if (editmesh_eval) {
      if (editmesh_eval->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH) {
        return nullptr;
      }
      if (*r_use_hide) {
        *r_use_hide = true;
      }
      return &editmesh_eval->id;
    }
  }

  /* For curves and surfaces in edit mode, use their original data when snapping.
   * Only use the evaluated mesh when snapping to the final geometry. */
  if (ELEM(ob_eval->type, OB_CURVES_LEGACY, OB_SURF) && BKE_object_is_in_editmode(ob_eval) &&
      edit_mode_type != SNAP_GEOM_FINAL)
  {
    return static_cast<const ID *>(ob_eval->data);
  }

  /* Get evaluated mesh including subdivision. This may come from a mesh object,
   * or another object type that has modifiers producing a mesh. */
  if (Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval)) {
    return &mesh_eval->id;
  }

  return static_cast<const ID *>(ob_eval->data);
}

/**
 * Mesh used for snapping (`dupli-list` instances).
 *
 * A version of #data_for_snap for instances.
 */
static const ID *data_for_snap_dupli(Object *ob_eval, ID *ob_data)
{
  /* For curve and surface objects, use the evaluated mesh so snapping
   * works with the final geometry instead of the coarse cage, see: #143060. */
  if (ELEM(ob_eval->type, OB_CURVES_LEGACY, OB_CURVES, OB_SURF)) {
    return &BKE_object_get_evaluated_mesh(ob_eval)->id;
  }
  return ob_data;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Iterator
 * \{ */

using IterSnapObjsCallback = eSnapMode (*)(SnapObjectContext *sctx,
                                           const Object *ob_eval,
                                           const ID *ob_data,
                                           const float4x4 &obmat,
                                           bool is_object_active,
                                           bool use_hide);

static bool snap_object_is_snappable(const SnapObjectContext *sctx,
                                     const eSnapTargetOP snap_target_select,
                                     const Base *base_act,
                                     const Base *base)
{
  if (!BASE_VISIBLE(sctx->runtime.v3d, base)) {
    return false;
  }

  if (snap_target_select == SCE_SNAP_TARGET_ALL) {
    return true;
  }

  if (base->flag_legacy & BA_SNAP_FIX_DEPS_FIASCO) {
    return false;
  }

  /* Get attributes of potential target. */
  const bool is_active = (base_act == base);
  const bool is_selected = (base->flag & BASE_SELECTED) || (base->flag_legacy & BA_WAS_SEL);
  const bool is_edited = (base->object->mode == OB_MODE_EDIT);
  const bool is_selectable = (base->flag & BASE_SELECTABLE);
  /* Get attributes of state. */
  const bool is_in_object_mode = (base_act == nullptr) ||
                                 (base_act->object->mode == OB_MODE_OBJECT);

  if (is_in_object_mode) {
    /* Handle target selection options that make sense for object mode. */
    if ((snap_target_select & SCE_SNAP_TARGET_NOT_SELECTED) && is_selected) {
      /* What is selectable or not is part of the object and depends on the mode. */
      return false;
    }
  }
  else {
    /* Handle target selection options that make sense for edit/pose mode. */
    if ((snap_target_select & SCE_SNAP_TARGET_NOT_ACTIVE) && is_active) {
      return false;
    }
    if ((snap_target_select & SCE_SNAP_TARGET_NOT_EDITED) && is_edited && !is_active) {
      /* Base is edited, but not active. */
      return false;
    }
    if ((snap_target_select & SCE_SNAP_TARGET_NOT_NONEDITED) && !is_edited) {
      return false;
    }
  }

  if ((snap_target_select & SCE_SNAP_TARGET_ONLY_SELECTABLE) && !is_selectable) {
    return false;
  }

  return true;
}

/**
 * Walks through all objects in the scene to create the list of objects to snap.
 */
static eSnapMode iter_snap_objects(SnapObjectContext *sctx, IterSnapObjsCallback sob_callback)
{
  eSnapMode ret = SCE_SNAP_TO_NONE;
  eSnapMode tmp;

  Scene *scene = DEG_get_input_scene(sctx->runtime.depsgraph);
  ViewLayer *view_layer = DEG_get_input_view_layer(sctx->runtime.depsgraph);
  const eSnapTargetOP snap_target_select = sctx->runtime.params.snap_target_select;
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base_act = BKE_view_layer_active_base_get(view_layer);

  DupliList duplilist;
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (!snap_object_is_snappable(sctx, snap_target_select, base_act, base)) {
      continue;
    }

    const bool is_object_active = (base == base_act);
    Object *obj_eval = DEG_get_evaluated(sctx->runtime.depsgraph, base->object);
    if (obj_eval->transflag & OB_DUPLI ||
        blender::bke::object_has_geometry_set_instances(*obj_eval))
    {
      object_duplilist(sctx->runtime.depsgraph, sctx->scene, obj_eval, nullptr, duplilist);
      for (DupliObject &dupli_ob : duplilist) {
        BLI_assert(DEG_is_evaluated(dupli_ob.ob));
        const ID *ob_data = data_for_snap_dupli(dupli_ob.ob, dupli_ob.ob_data);
        if ((tmp = sob_callback(
                 sctx, dupli_ob.ob, ob_data, float4x4(dupli_ob.mat), is_object_active, false)) !=
            SCE_SNAP_TO_NONE)
        {
          ret = tmp;
        }
      }
      duplilist.clear();
    }

    bool use_hide = false;
    const ID *ob_data = data_for_snap(obj_eval, sctx->runtime.params.edit_mode_type, &use_hide);
    if ((tmp = sob_callback(
             sctx, obj_eval, ob_data, obj_eval->object_to_world(), is_object_active, use_hide)) !=
        SCE_SNAP_TO_NONE)
    {
      ret = tmp;
    }
  }
  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ray Cast Functions
 * \{ */

/* Store all ray-hits
 * Support for storing all depths, not just the first (ray-cast 'all'). */

static SnapObjectHitDepth *hit_depth_create(const float depth, const float co[3], uint ob_uuid)
{
  SnapObjectHitDepth *hit = MEM_new<SnapObjectHitDepth>(__func__);

  hit->depth = depth;
  copy_v3_v3(hit->co, co);
  hit->ob_uuid = ob_uuid;

  return hit;
}

static int hit_depth_cmp(const void *arg1, const void *arg2)
{
  const SnapObjectHitDepth *h1 = static_cast<const SnapObjectHitDepth *>(arg1);
  const SnapObjectHitDepth *h2 = static_cast<const SnapObjectHitDepth *>(arg2);
  int val = 0;

  if (h1->depth < h2->depth) {
    val = -1;
  }
  else if (h1->depth > h2->depth) {
    val = 1;
  }

  return val;
}

void raycast_all_cb(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
  RayCastAll_Data *data = static_cast<RayCastAll_Data *>(userdata);
  data->raycast_callback(data->bvhdata, index, ray, hit);
  if (hit->index != -1) {
    /* Get all values in world-space. */
    float location[3];
    float depth;

    /* World-space location. */
    mul_v3_m4v3(location, (float (*)[4])data->obmat, hit->co);
    depth = (hit->dist + data->len_diff) / data->local_scale;

    SnapObjectHitDepth *hit_item = hit_depth_create(depth, location, data->ob_uuid);
    BLI_addtail(data->hit_list, hit_item);
  }
}

bool raycast_tri_backface_culling_test(
    const float dir[3], const float v0[3], const float v1[3], const float v2[3], float no[3])
{
  cross_tri_v3(no, v0, v1, v2);
  return dot_v3v3(no, dir) < 0.0f;
}

/**
 * \note Duplicate args here are documented at #snapObjectsRay.
 */
static eSnapMode raycast_obj_fn(SnapObjectContext *sctx,
                                const Object *ob_eval,
                                const ID *ob_data,
                                const float4x4 &obmat,
                                bool /*is_object_active*/,
                                bool use_hide)
{
  bool retval = false;

  if (ob_eval->visibility_flag & OB_HIDE_SURFACE_PICK) {
    /* Do not snap it surface picking is disabled. */
    return SCE_SNAP_TO_NONE;
  }

  if (ob_data == nullptr) {
    if ((sctx->runtime.occlusion_test_edit == SNAP_OCCLUSION_AS_SEEM) &&
        ELEM(ob_eval->dt, OB_BOUNDBOX, OB_WIRE))
    {
      /* Do not hit objects that are in wire or bounding box display mode. */
      return SCE_SNAP_TO_NONE;
    }
    if (ob_eval->type == OB_MESH) {
      if (snap_object_editmesh(sctx, ob_eval, nullptr, obmat, SCE_SNAP_TO_FACE, use_hide)) {
        retval = true;
      }
    }
    else {
      return SCE_SNAP_TO_NONE;
    }
  }
  else if ((sctx->runtime.params.occlusion_test == SNAP_OCCLUSION_AS_SEEM) &&
           ELEM(ob_eval->dt, OB_BOUNDBOX, OB_WIRE))
  {
    /* Do not hit objects that are in wire or bounding box display mode. */
    return SCE_SNAP_TO_NONE;
  }
  else if (GS(ob_data->name) != ID_ME) {
    return SCE_SNAP_TO_NONE;
  }
  else if (ELEM(ob_eval->type, OB_CURVES_LEGACY, OB_SURF) &&
           (sctx->runtime.params.edit_mode_type != SNAP_GEOM_FINAL) &&
           BKE_object_is_in_editmode(ob_eval))
  {
    /* The final Curves geometry is generated as a Mesh. Skip this Mesh if the target is not
     * #SNAP_GEOM_FINAL. This also avoids creating and destroying BVH Trees too frequently while
     * editing. */
    return SCE_SNAP_TO_NONE;
  }
  else {
    retval = snap_object_mesh(sctx, ob_eval, ob_data, obmat, SCE_SNAP_TO_FACE, use_hide);
  }

  if (retval) {
    return SCE_SNAP_TO_FACE;
  }
  return SCE_SNAP_TO_NONE;
}

/**
 * Main RayCast Function
 * ======================
 *
 * Walks through all objects in the scene to find the `hit` on object surface.
 *
 * \param sctx: Snap context to store data.
 *
 * Read/Write Args
 * ---------------
 *
 * \param ray_depth: maximum depth allowed for r_co,
 * elements deeper than this value will be ignored.
 */
static bool raycastObjects(SnapObjectContext *sctx)
{
  return iter_snap_objects(sctx, raycast_obj_fn) != SCE_SNAP_TO_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface Snap Functions
 * \{ */

static void nearest_world_tree_co(const BVHTree *tree,
                                  BVHTree_NearestPointCallback nearest_cb,
                                  void *treedata,
                                  const float3 &co,
                                  BVHTreeNearest *r_nearest)
{
  r_nearest->index = -1;
  copy_v3_fl(r_nearest->co, FLT_MAX);
  r_nearest->dist_sq = FLT_MAX;

  BLI_bvhtree_find_nearest(tree, co, r_nearest, nearest_cb, treedata);
}

bool nearest_world_tree(SnapObjectContext *sctx,
                        const BVHTree *tree,
                        BVHTree_NearestPointCallback nearest_cb,
                        const blender::float4x4 &obmat,
                        void *treedata,
                        BVHTreeNearest *r_nearest)
{
  float4x4 imat = math::invert(obmat);
  float3 init_co = math::transform_point(imat, sctx->runtime.init_co);
  float3 curr_co = math::transform_point(imat, sctx->runtime.curr_co);

  BVHTreeNearest nearest{};
  float3 vec;
  if (sctx->runtime.params.keep_on_same_target) {
    nearest_world_tree_co(tree, nearest_cb, treedata, init_co, &nearest);
    vec = float3(nearest.co) - init_co;
  }
  else {
    /* NOTE: when `params->face_nearest_steps == 1`, the return variables of function below contain
     * the answer.  We could return immediately after updating r_loc, r_no, r_index, but that would
     * also complicate the code. Foregoing slight optimization for code clarity. */
    nearest_world_tree_co(tree, nearest_cb, treedata, curr_co, &nearest);
    vec = float3(nearest.co) - curr_co;
  }

  float original_distance = math::length_squared(math::transform_direction(obmat, vec));
  if (r_nearest->dist_sq <= original_distance) {
    return false;
  }

  /* Scale to make `snap_face_nearest_steps` steps. */
  float step_scale_factor = 1.0f / max_ff(1.0f, float(sctx->runtime.params.face_nearest_steps));

  /* Compute offset between init co and prev co. */
  float3 delta = (curr_co - init_co) * step_scale_factor;

  float3 co = init_co;
  for (int i = 0; i < sctx->runtime.params.face_nearest_steps; i++) {
    co += delta;
    nearest_world_tree_co(tree, nearest_cb, treedata, co, &nearest);
    co = nearest.co;
  }

  *r_nearest = nearest;
  if (sctx->runtime.params.keep_on_same_target) {
    r_nearest->dist_sq = original_distance;
  }
  else if (sctx->runtime.params.face_nearest_steps > 1) {
    /* Recalculate the distance.
     * When multiple steps are tested, we cannot depend on the distance calculated for
     * `nearest.dist_sq`, as it reduces with each step. */
    vec = co - curr_co;
    r_nearest->dist_sq = math::length_squared(math::transform_direction(obmat, vec));
  }
  return true;
}

static eSnapMode nearest_world_object_fn(SnapObjectContext *sctx,
                                         const Object *ob_eval,
                                         const ID *ob_data,
                                         const float4x4 &obmat,
                                         bool /*is_object_active*/,
                                         bool use_hide)
{
  eSnapMode retval = SCE_SNAP_TO_NONE;

  if (ob_data == nullptr) {
    if (ob_eval->type == OB_MESH) {
      retval = snap_object_editmesh(
          sctx, ob_eval, nullptr, obmat, SCE_SNAP_INDIVIDUAL_NEAREST, use_hide);
    }
    else {
      return SCE_SNAP_TO_NONE;
    }
  }
  else if (GS(ob_data->name) != ID_ME) {
    return SCE_SNAP_TO_NONE;
  }
  else if (ELEM(ob_eval->type, OB_CURVES_LEGACY, OB_SURF) &&
           (sctx->runtime.params.edit_mode_type != SNAP_GEOM_FINAL) &&
           BKE_object_is_in_editmode(ob_eval))
  {
    /* The final Curves geometry is generated as a Mesh. Skip this Mesh if the target is not
     * #SNAP_GEOM_FINAL. This also avoids creating and destroying BVH Trees too frequently while
     * editing. */
    return SCE_SNAP_TO_NONE;
  }
  else {
    retval = snap_object_mesh(
        sctx, ob_eval, ob_data, obmat, SCE_SNAP_INDIVIDUAL_NEAREST, use_hide);
  }

  return retval;
}

/**
 * Main Nearest World Surface Function
 * ===================================
 *
 * Walks through all objects in the scene to find the nearest location on target surface.
 *
 * \param sctx: Snap context to store data.
 * \param params: Settings for snapping.
 * \param init_co: Initial location of source point.
 * \param prev_co: Current location of source point after transformation but before snapping.
 */
static bool nearestWorldObjects(SnapObjectContext *sctx)
{
  return iter_snap_objects(sctx, nearest_world_object_fn) != SCE_SNAP_TO_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks
 * \{ */

void cb_snap_vert(void *userdata,
                  int index,
                  const DistProjectedAABBPrecalc *precalc,
                  const float (*clip_plane)[4],
                  const int clip_plane_len,
                  BVHTreeNearest *nearest)
{
  SnapData *data = static_cast<SnapData *>(userdata);

  const float *co;
  data->get_vert_co(index, &co);

  if (test_projected_vert_dist(precalc, clip_plane, clip_plane_len, data->is_persp, co, nearest)) {
    data->copy_vert_no(index, nearest->no);
    nearest->index = index;
  }
}

void cb_snap_edge(void *userdata,
                  int index,
                  const DistProjectedAABBPrecalc *precalc,
                  const float (*clip_plane)[4],
                  const int clip_plane_len,
                  BVHTreeNearest *nearest)
{
  SnapData *data = static_cast<SnapData *>(userdata);

  int vindex[2];
  data->get_edge_verts_index(index, vindex);

  const float *v_pair[2];
  data->get_vert_co(vindex[0], &v_pair[0]);
  data->get_vert_co(vindex[1], &v_pair[1]);

  if (test_projected_edge_dist(
          precalc, clip_plane, clip_plane_len, data->is_persp, v_pair[0], v_pair[1], nearest))
  {
    sub_v3_v3v3(nearest->no, v_pair[1], v_pair[0]);
    nearest->index = index;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Object Snapping API
 * \{ */

static eSnapMode snap_polygon(SnapObjectContext *sctx, eSnapMode snap_to_flag)
{
  if (!sctx->ret.data || GS(sctx->ret.data->name) != ID_ME) {
    return SCE_SNAP_TO_NONE;
  }

  return snap_polygon_mesh(
      sctx, sctx->ret.ob, sctx->ret.data, sctx->ret.obmat, snap_to_flag, sctx->ret.index);
}

static eSnapMode snap_edge_points(SnapObjectContext *sctx, const float dist_px_sq_orig)
{
  if (!sctx->ret.data || GS(sctx->ret.data->name) != ID_ME) {
    return SCE_SNAP_TO_EDGE;
  }

  return snap_edge_points_mesh(
      sctx, sctx->ret.ob, sctx->ret.data, sctx->ret.obmat, dist_px_sq_orig, sctx->ret.index);
}

eSnapMode snap_object_center(SnapObjectContext *sctx,
                             const Object *ob_eval,
                             const float4x4 &obmat,
                             eSnapMode snap_to_flag)
{
  /* May extend later (for now just snaps to empty or camera center). */

  if (ob_eval->transflag & OB_DUPLI) {
    return SCE_SNAP_TO_NONE;
  }

  /* For now only vertex supported. */
  if ((snap_to_flag & SCE_SNAP_TO_POINT) == 0) {
    return SCE_SNAP_TO_NONE;
  }

  SnapData nearest2d(sctx, obmat);

  nearest2d.clip_planes_enable(sctx, ob_eval);

  if (nearest2d.snap_point(float3(0.0f))) {
    nearest2d.register_result(sctx, ob_eval, static_cast<const ID *>(ob_eval->data));
    return SCE_SNAP_TO_POINT;
  }

  return SCE_SNAP_TO_NONE;
}

/**
 * \note Duplicate args here are documented at #snapObjectsRay.
 */
static eSnapMode snap_obj_fn(SnapObjectContext *sctx,
                             const Object *ob_eval,
                             const ID *ob_data,
                             const float4x4 &obmat,
                             bool is_object_active,
                             bool use_hide)
{
  if (ob_data == nullptr && (ob_eval->type == OB_MESH)) {
    return snap_object_editmesh(
        sctx, ob_eval, nullptr, obmat, sctx->runtime.snap_to_flag, use_hide);
  }

  if (ob_data == nullptr) {
    return snap_object_center(sctx, ob_eval, obmat, sctx->runtime.snap_to_flag);
  }

  if (ob_eval->dt == OB_BOUNDBOX) {
    /* Do not snap to objects that are in bounding box display mode. */
    return SCE_SNAP_TO_NONE;
  }

  if (GS(ob_data->name) == ID_ME) {
    if (ELEM(ob_eval->type, OB_CURVES_LEGACY, OB_SURF) &&
        (sctx->runtime.params.edit_mode_type != SNAP_GEOM_FINAL) &&
        BKE_object_is_in_editmode(ob_eval))
    {
      /* The final Curves geometry is generated as a Mesh. Skip this Mesh if the target is not
       * #SNAP_GEOM_FINAL. */
      return SCE_SNAP_TO_NONE;
    }
    return snap_object_mesh(sctx, ob_eval, ob_data, obmat, sctx->runtime.snap_to_flag, use_hide);
  }

  eSnapMode retval = SCE_SNAP_TO_NONE;
  switch (ob_eval->type) {
    case OB_MESH: {
      break;
    }
    case OB_ARMATURE:
      retval = snapArmature(sctx, ob_eval, obmat, is_object_active);
      break;
    case OB_CURVES_LEGACY:
    case OB_SURF:
      if (ob_eval->type == OB_CURVES_LEGACY || BKE_object_is_in_editmode(ob_eval)) {
        retval = snapCurve(sctx, ob_eval, obmat);
      }
      break;
    case OB_FONT: {
      break;
    }
    case OB_EMPTY:
    case OB_LAMP:
      retval = snap_object_center(sctx, ob_eval, obmat, sctx->runtime.snap_to_flag);
      break;
    case OB_CAMERA:
      retval = snapCamera(sctx, ob_eval, obmat, sctx->runtime.snap_to_flag);
      break;
  }

  return retval;
}

/**
 * Main Snapping Function
 * ======================
 *
 * Walks through all objects in the scene to find the closest snap element ray.
 *
 * \param sctx: Snap context to store data.
 *
 * Read/Write Args
 * ---------------
 *
 * \param dist_px: Maximum threshold distance (in pixels).
 */
static eSnapMode snapObjectsRay(SnapObjectContext *sctx)
{
  return iter_snap_objects(sctx, snap_obj_fn);
}

static bool snap_grid(SnapObjectContext *sctx)
{
  SnapData nearest2d(sctx);
  nearest2d.clip_planes_enable(sctx, nullptr, true);

  /* Ignore the maximum pixel distance when snapping to grid.
   * This avoids undesirable jumps of the element being snapped. */
  nearest2d.nearest_point.dist_sq = FLT_MAX;

  float grid_dist = sctx->grid.size;

  if (sctx->grid.use_init_co) {
    float3 co = math::round((sctx->runtime.init_co) / grid_dist) * grid_dist;
    if (nearest2d.snap_point(co)) {
      nearest2d.register_result(sctx, nullptr, nullptr);
      return true;
    }
  }

  float ray_dist;
  for (int i = 0; i < 4; i++) {
    if (isect_ray_plane_v3(sctx->runtime.ray_start,
                           sctx->runtime.ray_dir,
                           sctx->grid.planes[i],
                           &ray_dist,
                           false) &&
        (ray_dist > 0.0f))
    {
      float3 co = math::round((sctx->runtime.ray_start + sctx->runtime.ray_dir * ray_dist) /
                              grid_dist) *
                  grid_dist;

      if (nearest2d.snap_point(co)) {
        copy_v3_v3(nearest2d.nearest_point.no, sctx->grid.planes[i]);
        if (!sctx->runtime.rv3d->is_persp && RV3D_VIEW_IS_AXIS(sctx->runtime.rv3d->view)) {
          /* Project in #sctx->runtime.curr_co plane. */
          add_v3_v3(nearest2d.nearest_point.co,
                    sctx->runtime.curr_co * float3(nearest2d.nearest_point.no));
        }
        nearest2d.register_result(sctx, nullptr, nullptr);
        return true;
      }
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Object Snapping API
 * \{ */

SnapObjectContext *snap_object_context_create(Scene *scene, int /*flag*/)
{
  SnapObjectContext *sctx = MEM_new<SnapObjectContext>(__func__);

  sctx->scene = scene;

  return sctx;
}

void snap_object_context_destroy(SnapObjectContext *sctx)
{
  MEM_delete(sctx);
}

void snap_object_context_set_editmesh_callbacks(SnapObjectContext *sctx,
                                                bool (*test_vert_fn)(BMVert *, void *user_data),
                                                bool (*test_edge_fn)(BMEdge *, void *user_data),
                                                bool (*test_face_fn)(BMFace *, void *user_data),
                                                void *user_data)
{
  bool is_cache_dirty = false;
  if (sctx->callbacks.edit_mesh.test_vert_fn != test_vert_fn) {
    sctx->callbacks.edit_mesh.test_vert_fn = test_vert_fn;
    is_cache_dirty = true;
  }
  if (sctx->callbacks.edit_mesh.test_edge_fn != test_edge_fn) {
    sctx->callbacks.edit_mesh.test_edge_fn = test_edge_fn;
    is_cache_dirty = true;
  }
  if (sctx->callbacks.edit_mesh.test_face_fn != test_face_fn) {
    sctx->callbacks.edit_mesh.test_face_fn = test_face_fn;
    is_cache_dirty = true;
  }
  if (sctx->callbacks.edit_mesh.user_data != user_data) {
    sctx->callbacks.edit_mesh.user_data = user_data;
    is_cache_dirty = true;
  }

  if (is_cache_dirty) {
    sctx->editmesh_caches.clear();
  }
}

static bool snap_object_context_runtime_init(SnapObjectContext *sctx,
                                             Depsgraph *depsgraph,
                                             const ARegion *region,
                                             const View3D *v3d,
                                             eSnapMode snap_to_flag,
                                             eSnapOcclusionTest occlusion_test,
                                             const SnapObjectParams *params,
                                             const float ray_start[3],
                                             const float ray_dir[3],
                                             const float ray_depth,
                                             const float mval[2],
                                             const float init_co[3],
                                             const float prev_co[3],
                                             const float dist_px_sq,
                                             ListBase *hit_list)
{
  if (snap_to_flag &
      (SCE_SNAP_TO_GRID | SCE_SNAP_TO_EDGE_PERPENDICULAR | SCE_SNAP_INDIVIDUAL_NEAREST))
  {
    if (prev_co) {
      copy_v3_v3(sctx->runtime.curr_co, prev_co);
      if (init_co) {
        copy_v3_v3(sctx->runtime.init_co, init_co);
      }
      else {
        snap_to_flag &= ~SCE_SNAP_INDIVIDUAL_NEAREST;
      }
    }
    else {
      snap_to_flag &= ~(SCE_SNAP_TO_EDGE_PERPENDICULAR | SCE_SNAP_INDIVIDUAL_NEAREST);
    }
  }

  if (snap_to_flag == SCE_SNAP_TO_NONE) {
    return false;
  }

  sctx->runtime.depsgraph = depsgraph;
  sctx->runtime.rv3d = nullptr;
  sctx->runtime.v3d = v3d;
  sctx->runtime.snap_to_flag = snap_to_flag;
  sctx->runtime.params = *params;
  sctx->runtime.params.occlusion_test = occlusion_test;
  sctx->runtime.occlusion_test_edit = (snap_to_flag & SCE_SNAP_TO_FACE) ? SNAP_OCCLUSION_ALWAYS :
                                                                          occlusion_test;
  sctx->runtime.has_occlusion_plane = false;
  sctx->runtime.has_occlusion_plane_in_front = false;
  sctx->runtime.object_index = 0;

  copy_v3_v3(sctx->runtime.ray_start, ray_start);
  copy_v3_v3(sctx->runtime.ray_dir, ray_dir);

  if (mval) {
    copy_v2_v2(sctx->runtime.mval, mval);
  }

  if (region) {
    const RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
    sctx->runtime.win_size[0] = region->winx;
    sctx->runtime.win_size[1] = region->winy;

    sctx->runtime.clip_planes.resize(2);

    planes_from_projmat(rv3d->persmat,
                        nullptr,
                        nullptr,
                        nullptr,
                        nullptr,
                        sctx->runtime.clip_planes[0],
                        sctx->runtime.clip_planes[1]);

    if (rv3d->rflag & RV3D_CLIPPING) {
      sctx->runtime.clip_planes.extend_unchecked(reinterpret_cast<const float4 *>(rv3d->clip), 4);
    }

    sctx->runtime.rv3d = rv3d;

    if (sctx->runtime.snap_to_flag & SCE_SNAP_TO_GRID) {
      sctx->grid.use_init_co = init_co != nullptr;
      if (params->grid_size) {
        sctx->grid.size = params->grid_size;
      }
      if (!compare_m4m4(sctx->grid.persmat.ptr(), rv3d->persmat, FLT_EPSILON)) {
        sctx->grid.persmat = float4x4(rv3d->persmat);
        if (params->grid_size == 0.0f) {
          sctx->grid.size = ED_view3d_grid_view_scale(
              sctx->scene, sctx->runtime.v3d, region, nullptr);
        }

        if (!sctx->grid.use_init_co) {
          memset(reinterpret_cast<void *>(sctx->grid.planes), 0, sizeof(sctx->grid.planes));
          sctx->grid.planes[0][2] = 1.0f;
          if (math::abs(sctx->runtime.ray_dir[0]) < math::abs(sctx->runtime.ray_dir[1])) {
            sctx->grid.planes[1][1] = 1.0f;
            sctx->grid.planes[2][0] = 1.0f;
          }
          else {
            sctx->grid.planes[1][0] = 1.0f;
            sctx->grid.planes[2][1] = 1.0f;
          }

          plane_from_point_normal_v3(
              sctx->grid.planes[3], sctx->runtime.curr_co, rv3d->viewinv[2]);
        }
      }
    }
  }
  sctx->runtime.hit_list = hit_list;

  sctx->ret.ray_depth_max = sctx->ret.ray_depth_max_in_front = ray_depth;
  sctx->ret.index = -1;
  sctx->ret.ob = nullptr;
  sctx->ret.data = nullptr;
  sctx->ret.dist_px_sq = dist_px_sq;

  return true;
}

bool snap_object_project_ray_ex(SnapObjectContext *sctx,
                                Depsgraph *depsgraph,
                                const View3D *v3d,
                                const SnapObjectParams *params,
                                const float ray_start[3],
                                const float ray_normal[3],
                                float *ray_depth,
                                float r_loc[3],
                                float r_no[3],
                                int *r_index,
                                const Object **r_ob,
                                float r_obmat[4][4])
{
  if (!snap_object_context_runtime_init(sctx,
                                        depsgraph,
                                        nullptr,
                                        v3d,
                                        SCE_SNAP_TO_FACE,
                                        params->occlusion_test,
                                        params,
                                        ray_start,
                                        ray_normal,
                                        !ray_depth || *ray_depth == -1.0f ? BVH_RAYCAST_DIST_MAX :
                                                                            *ray_depth,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        0,
                                        nullptr))
  {
    return false;
  }

  if (raycastObjects(sctx)) {
    copy_v3_v3(r_loc, sctx->ret.loc);
    if (r_no) {
      copy_v3_v3(r_no, sctx->ret.no);
    }
    if (r_index) {
      *r_index = sctx->ret.index;
    }
    if (r_ob) {
      *r_ob = sctx->ret.ob;
    }
    if (r_obmat) {
      copy_m4_m4(r_obmat, sctx->ret.obmat.ptr());
    }
    if (ray_depth) {
      *ray_depth = sctx->ret.ray_depth_max;
    }
    return true;
  }
  return false;
}

bool snap_object_project_ray_all(SnapObjectContext *sctx,
                                 Depsgraph *depsgraph,
                                 const View3D *v3d,
                                 const SnapObjectParams *params,
                                 const float ray_start[3],
                                 const float ray_normal[3],
                                 float ray_depth,
                                 bool sort,
                                 ListBase *r_hit_list)
{
  if (!snap_object_context_runtime_init(sctx,
                                        depsgraph,
                                        nullptr,
                                        v3d,
                                        SCE_SNAP_TO_FACE,
                                        params->occlusion_test,
                                        params,
                                        ray_start,
                                        ray_normal,
                                        ray_depth == -1.0f ? BVH_RAYCAST_DIST_MAX : ray_depth,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        0,
                                        r_hit_list))
  {
    return false;
  }

#ifndef NDEBUG
  float ray_depth_prev = sctx->ret.ray_depth_max;
#endif
  if (raycastObjects(sctx)) {
    if (sort) {
      BLI_listbase_sort(r_hit_list, hit_depth_cmp);
    }
    /* Meant to be read-only for 'all' hits, ensure it is. */
#ifndef NDEBUG
    BLI_assert(ray_depth_prev == sctx->ret.ray_depth_max);
#endif
    return true;
  }
  return false;
}

bool snap_object_project_ray(SnapObjectContext *sctx,
                             Depsgraph *depsgraph,
                             const View3D *v3d,
                             const SnapObjectParams *params,
                             const float ray_start[3],
                             const float ray_normal[3],
                             float *ray_depth,
                             float r_co[3],
                             float r_no[3])
{
  return snap_object_project_ray_ex(sctx,
                                    depsgraph,
                                    v3d,
                                    params,
                                    ray_start,
                                    ray_normal,
                                    ray_depth,
                                    r_co,
                                    r_no,
                                    nullptr,
                                    nullptr,
                                    nullptr);
}

eSnapMode snap_object_project_view3d_ex(SnapObjectContext *sctx,
                                        Depsgraph *depsgraph,
                                        const ARegion *region,
                                        const View3D *v3d,
                                        eSnapMode snap_to_flag,
                                        const SnapObjectParams *params,
                                        const float init_co[3],
                                        const float mval[2],
                                        const float prev_co[3],
                                        float *dist_px,
                                        float r_loc[3],
                                        float r_no[3],
                                        int *r_index,
                                        const Object **r_ob,
                                        float r_obmat[4][4],
                                        float r_face_nor[3])
{
  eSnapMode retval = SCE_SNAP_TO_NONE;
  float ray_depth_max = BVH_RAYCAST_DIST_MAX;
  bool use_occlusion_plane = false;

  /* It is required `mval` to calculate the occlusion plane. */
  if (mval && (snap_to_flag & (SCE_SNAP_TO_GEOM | SCE_SNAP_TO_GRID))) {
    if (params->occlusion_test == SNAP_OCCLUSION_AS_SEEM) {
      use_occlusion_plane = !XRAY_ENABLED(v3d);
    }
    else if (params->occlusion_test == SNAP_OCCLUSION_ALWAYS) {
      use_occlusion_plane = true;
    }
  }

  if (use_occlusion_plane || (snap_to_flag & SCE_SNAP_TO_FACE) ||
      /* Snap to Grid requires `ray_start` and `ray_dir`. */
      (snap_to_flag & SCE_SNAP_TO_GRID))
  {
    /* Calculate the direction (`ray_dir`) and starting point (`ray_start`) of the ray from the
     * viewport to a 3D point under the mouse cursor (`mval`), taking into account potential view
     * clipping. */

    const RegionView3D *rv3d = static_cast<const RegionView3D *>(region->regiondata);
    float3 ray_end;
    ED_view3d_win_to_ray_clipped_ex(depsgraph,
                                    region,
                                    v3d,
                                    mval,
                                    false,
                                    nullptr,
                                    sctx->runtime.ray_dir,
                                    sctx->runtime.ray_start,
                                    ray_end);

    if (rv3d->rflag & RV3D_CLIPPING) {
      if (clip_segment_v3_plane_n(
              sctx->runtime.ray_start, ray_end, rv3d->clip, 6, sctx->runtime.ray_start, ray_end))
      {
        ray_depth_max = math::dot(ray_end - sctx->runtime.ray_start, sctx->runtime.ray_dir);
      }
      else {
        snap_to_flag &= ~SCE_SNAP_TO_FACE;
        use_occlusion_plane = false;
      }
    }
  }

  if (!snap_object_context_runtime_init(sctx,
                                        depsgraph,
                                        region,
                                        v3d,
                                        snap_to_flag,
                                        use_occlusion_plane ? params->occlusion_test :
                                                              SNAP_OCCLUSION_NEVER,
                                        params,
                                        sctx->runtime.ray_start,
                                        sctx->runtime.ray_dir,
                                        ray_depth_max,
                                        mval,
                                        init_co,
                                        prev_co,
                                        dist_px ? square_f(*dist_px) : FLT_MAX,
                                        nullptr))
  {
    return retval;
  }

#ifdef DEBUG_SNAP_TIME
  const timeit::TimePoint start_ = timeit::Clock::now();
#endif

  snap_to_flag = sctx->runtime.snap_to_flag;

  BLI_assert(snap_to_flag & (SCE_SNAP_TO_GEOM | SCE_SNAP_TO_GRID | SCE_SNAP_INDIVIDUAL_NEAREST));

  bool has_hit = false;

  /* NOTE: if both face ray-cast and face nearest are enabled, first find result of nearest, then
   * override with ray-cast. */
  if ((snap_to_flag & SCE_SNAP_INDIVIDUAL_NEAREST) && !has_hit) {
    has_hit = nearestWorldObjects(sctx);

    if (has_hit) {
      retval |= SCE_SNAP_INDIVIDUAL_NEAREST;
    }
  }

  if (use_occlusion_plane || (snap_to_flag & SCE_SNAP_TO_FACE)) {
    has_hit = raycastObjects(sctx);

    if (has_hit) {
      if (r_face_nor) {
        copy_v3_v3(r_face_nor, sctx->ret.no);
      }

      if (snap_to_flag & SCE_SNAP_TO_FACE) {
        retval |= SCE_SNAP_TO_FACE;
      }
    }
  }

  if (snap_to_flag & (SCE_SNAP_TO_POINT | SNAP_TO_EDGE_ELEMENTS)) {
    eSnapMode elem_test, elem = SCE_SNAP_TO_NONE;

    /* Remove what has already been computed. */
    sctx->runtime.snap_to_flag &= ~(SCE_SNAP_TO_FACE | SCE_SNAP_INDIVIDUAL_NEAREST);

    SnapObjectContext::Output ret_bak{};
    if (!(sctx->runtime.snap_to_flag & SCE_SNAP_TO_EDGE) &&
        (sctx->runtime.snap_to_flag &
         (SCE_SNAP_TO_EDGE_MIDPOINT | SCE_SNAP_TO_EDGE_ENDPOINT | SCE_SNAP_TO_EDGE_PERPENDICULAR)))
    {
      /* 'Snap to Edge' may occur even if it is not included among the selected snap types.
       * Save a backup to restore the previous result if needed. */
      ret_bak = sctx->ret;
    }

    if (use_occlusion_plane && has_hit) {
      /* Compute the new clip_pane but do not add it yet. */
      BLI_ASSERT_UNIT_V3(sctx->ret.no);
      sctx->runtime.occlusion_plane = occlusion_plane_create(
          sctx->runtime.ray_dir, sctx->ret.loc, sctx->ret.no);

      /* First, snap to the geometry of the polygon obtained via raycast.
       * This is necessary because the occlusion plane may "occlude" part of the polygon's
       * geometry. It also reduces the snap distance, optimizing the process.
       *
       * Note that if 'Snap to Edge Midpoint' or 'Snap to Edge Perpendicular' is selected, 'Snap to
       * Edge' will be returned instead.
       * This is because the same point can be tested in `snapObjectsRay` and fail this time due to
       * a mismatched snap distance, also resulting in snapping to the edge instead. */
      elem_test = snap_polygon(sctx, sctx->runtime.snap_to_flag);
      if (elem_test) {
        elem = elem_test;
      }

      /* Add the new clip plane. */
      sctx->runtime.has_occlusion_plane = true;
    }

    /* `snapObjectsRay` does 'Snap to Edge' instead of 'Snap to Edge Midpoint' or 'Snap to Edge
     * Perpendicular'. These points will be tested in the `snap_edge_points` function. */
    elem_test = snapObjectsRay(sctx);
    if (elem_test != SCE_SNAP_TO_NONE) {
      elem = elem_test;
    }

    if (elem == SCE_SNAP_TO_EDGE) {
      if (snap_to_flag &
          (SCE_SNAP_TO_EDGE_ENDPOINT | SCE_SNAP_TO_EDGE_MIDPOINT | SCE_SNAP_TO_EDGE_PERPENDICULAR))
      {
        elem = snap_edge_points(sctx, square_f(*dist_px));
      }

      if (!(elem & snap_to_flag)) {
        /* Restore the previous snap. */
        elem = SCE_SNAP_TO_NONE;
        sctx->ret = ret_bak;
      }
    }

    if (elem != SCE_SNAP_TO_NONE) {
      retval = elem & snap_to_flag;
    }
  }

  if ((retval == SCE_SNAP_TO_NONE) && (snap_to_flag & SCE_SNAP_TO_GRID)) {
    if (snap_grid(sctx)) {
      retval = SCE_SNAP_TO_GRID;
    }
  }

  if (retval != SCE_SNAP_TO_NONE) {
    copy_v3_v3(r_loc, sctx->ret.loc);
    if (r_no) {
      copy_v3_v3(r_no, sctx->ret.no);
    }
    if (r_ob) {
      *r_ob = sctx->ret.ob;
    }
    if (r_obmat) {
      copy_m4_m4(r_obmat, sctx->ret.obmat.ptr());
    }
    if (r_index) {
      *r_index = sctx->ret.index;
    }

    if (dist_px) {
      *dist_px = math::sqrt(sctx->ret.dist_px_sq);
    }
  }

#ifdef DEBUG_SNAP_TIME
  duration_ += timeit::Clock::now() - start_;
  total_count_++;
#endif

  return retval;
}

eSnapMode snap_object_project_view3d(SnapObjectContext *sctx,
                                     Depsgraph *depsgraph,
                                     const ARegion *region,
                                     const View3D *v3d,
                                     const eSnapMode snap_to,
                                     const SnapObjectParams *params,
                                     const float init_co[3],
                                     const float mval[2],
                                     const float prev_co[3],
                                     float *dist_px,
                                     float r_loc[3],
                                     float r_no[3])
{
  return snap_object_project_view3d_ex(sctx,
                                       depsgraph,
                                       region,
                                       v3d,
                                       snap_to,
                                       params,
                                       init_co,
                                       mval,
                                       prev_co,
                                       dist_px,
                                       r_loc,
                                       r_no,
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       nullptr);
}

bool object_project_all_view3d_ex(SnapObjectContext *sctx,
                                  Depsgraph *depsgraph,
                                  const ARegion *region,
                                  const View3D *v3d,
                                  const SnapObjectParams *params,
                                  const float mval[2],
                                  float ray_depth,
                                  bool sort,
                                  ListBase *r_hit_list)
{
  float3 ray_start, ray_normal, ray_end;
  const RegionView3D *rv3d = static_cast<const RegionView3D *>(region->regiondata);

  if (!ED_view3d_win_to_ray_clipped_ex(
          depsgraph, region, v3d, mval, false, nullptr, ray_normal, ray_start, ray_end))
  {
    return false;
  }

  if ((rv3d->rflag & RV3D_CLIPPING) &&
      clip_segment_v3_plane_n(ray_start, ray_end, rv3d->clip, 6, ray_start, ray_end))
  {
    float ray_depth_max = math::dot(ray_end - ray_start, ray_normal);
    if ((ray_depth == -1.0f) || (ray_depth > ray_depth_max)) {
      ray_depth = ray_depth_max;
    }
  }

  return snap_object_project_ray_all(
      sctx, depsgraph, v3d, params, ray_start, ray_normal, ray_depth, sort, r_hit_list);
}

#ifdef DEBUG_SNAP_TIME
void ED_transform_snap_object_time_average_print()
{
  std::cout << "Average snapping time: ";
  std::cout << std::fixed << duration_.count() / 1.0e6 << " ms";
  std::cout << '\n';

  duration_ = timeit::Nanoseconds::zero();
  total_count_ = 0;
}
#endif

/** \} */

}  // namespace blender::ed::transform
