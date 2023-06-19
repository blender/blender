/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_math.h"
#include "BLI_math_matrix_types.hh"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_armature.h"
#include "BKE_bvhutils.h"
#include "BKE_curve.h"
#include "BKE_duplilist.h"
#include "BKE_editmesh.h"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_mesh.hh"
#include "BKE_object.h"
#include "BKE_tracking.h"

#include "DEG_depsgraph_query.h"

#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"

#include "transform_snap_object.hh"

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
static ID *data_for_snap(Object *ob_eval, eSnapEditType edit_mode_type, bool *r_use_hide)
{
  bool use_hide = false;

  switch (ob_eval->type) {
    case OB_MESH: {
      Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);
      if (BKE_object_is_in_editmode(ob_eval)) {
        if (edit_mode_type == SNAP_GEOM_EDIT) {
          return nullptr;
        }

        Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(ob_eval);
        Mesh *editmesh_eval_cage = BKE_object_get_editmesh_eval_cage(ob_eval);

        if ((edit_mode_type == SNAP_GEOM_FINAL) && editmesh_eval_final) {
          if (editmesh_eval_final->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH) {
            return nullptr;
          }
          me_eval = editmesh_eval_final;
          use_hide = true;
        }
        else if ((edit_mode_type == SNAP_GEOM_CAGE) && editmesh_eval_cage) {
          if (editmesh_eval_cage->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH) {
            return nullptr;
          }
          me_eval = editmesh_eval_cage;
          use_hide = true;
        }
      }
      if (r_use_hide) {
        *r_use_hide = use_hide;
      }
      return (ID *)me_eval;
    }
    default:
      break;
  }
  if (r_use_hide) {
    *r_use_hide = use_hide;
  }
  return (ID *)ob_eval->data;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Iterator
 * \{ */

using IterSnapObjsCallback = eSnapMode (*)(SnapObjectContext *sctx,
                                           Object *ob_eval,
                                           ID *ob_data,
                                           const float obmat[4][4],
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

  if ((snap_target_select == SCE_SNAP_TARGET_ALL) ||
      (base->flag_legacy & BA_TRANSFORM_LOCKED_IN_PLACE))
  {
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
  eSnapMode ret = SCE_SNAP_MODE_NONE;
  eSnapMode tmp;

  Scene *scene = DEG_get_input_scene(sctx->runtime.depsgraph);
  ViewLayer *view_layer = DEG_get_input_view_layer(sctx->runtime.depsgraph);
  const eSnapTargetOP snap_target_select = sctx->runtime.params.snap_target_select;
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base_act = BKE_view_layer_active_base_get(view_layer);

  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (!snap_object_is_snappable(sctx, snap_target_select, base_act, base)) {
      continue;
    }

    const bool is_object_active = (base == base_act);
    Object *obj_eval = DEG_get_evaluated_object(sctx->runtime.depsgraph, base->object);
    if (obj_eval->transflag & OB_DUPLI ||
        blender::bke::object_has_geometry_set_instances(*obj_eval)) {
      ListBase *lb = object_duplilist(sctx->runtime.depsgraph, sctx->scene, obj_eval);
      LISTBASE_FOREACH (DupliObject *, dupli_ob, lb) {
        BLI_assert(DEG_is_evaluated_object(dupli_ob->ob));
        if ((tmp = sob_callback(
                 sctx, dupli_ob->ob, dupli_ob->ob_data, dupli_ob->mat, is_object_active, false)) !=
            SCE_SNAP_MODE_NONE)
        {
          ret = tmp;
        }
      }
      free_object_duplilist(lb);
    }

    bool use_hide = false;
    ID *ob_data = data_for_snap(obj_eval, sctx->runtime.params.edit_mode_type, &use_hide);
    if ((tmp = sob_callback(
             sctx, obj_eval, ob_data, obj_eval->object_to_world, is_object_active, use_hide)) !=
        SCE_SNAP_MODE_NONE)
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
    mul_v3_m4v3(location, (float(*)[4])data->obmat, hit->co);
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
 * \note Duplicate args here are documented at #snapObjectsRay
 */
static eSnapMode raycast_obj_fn(SnapObjectContext *sctx,
                                Object *ob_eval,
                                ID *ob_data,
                                const float obmat[4][4],
                                bool is_object_active,
                                bool use_hide)
{
  bool retval = false;
  bool is_edit = false;

  if (ob_data == nullptr) {
    if (sctx->runtime.use_occlusion_test_edit && ELEM(ob_eval->dt, OB_BOUNDBOX, OB_WIRE)) {
      /* Do not hit objects that are in wire or bounding box display mode. */
      return SCE_SNAP_MODE_NONE;
    }
    if (ob_eval->type == OB_MESH) {
      if (snap_object_editmesh(sctx, ob_eval, nullptr, obmat, SCE_SNAP_MODE_FACE, use_hide)) {
        retval = true;
        is_edit = true;
      }
    }
    else {
      return SCE_SNAP_MODE_NONE;
    }
  }
  else if (sctx->runtime.params.use_occlusion_test && ELEM(ob_eval->dt, OB_BOUNDBOX, OB_WIRE)) {
    /* Do not hit objects that are in wire or bounding box display mode. */
    return SCE_SNAP_MODE_NONE;
  }
  else if (GS(ob_data->name) != ID_ME) {
    return SCE_SNAP_MODE_NONE;
  }
  else if (is_object_active && ELEM(ob_eval->type, OB_CURVES_LEGACY, OB_SURF, OB_FONT)) {
    return SCE_SNAP_MODE_NONE;
  }
  else {
    retval = snap_object_mesh(sctx, ob_eval, ob_data, obmat, SCE_SNAP_MODE_FACE, use_hide);
  }

  if (retval) {
    copy_m4_m4(sctx->ret.obmat, obmat);
    sctx->ret.ob = ob_eval;
    sctx->ret.data = ob_data;
    sctx->ret.is_edit = is_edit;
    return SCE_SNAP_MODE_FACE;
  }
  return SCE_SNAP_MODE_NONE;
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
  return iter_snap_objects(sctx, raycast_obj_fn) != SCE_SNAP_MODE_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface Snap Functions
 * \{ */

struct NearestWorldObjUserData {
  const float *init_co;
  const float *curr_co;
};

static void nearest_world_tree_co(BVHTree *tree,
                                  BVHTree_NearestPointCallback nearest_cb,
                                  void *treedata,
                                  float co[3],
                                  float r_co[3],
                                  float r_no[3],
                                  int *r_index,
                                  float *r_dist_sq)
{
  BVHTreeNearest nearest = {};
  nearest.index = -1;
  copy_v3_fl(nearest.co, FLT_MAX);
  nearest.dist_sq = FLT_MAX;

  BLI_bvhtree_find_nearest(tree, co, &nearest, nearest_cb, treedata);

  if (r_co) {
    copy_v3_v3(r_co, nearest.co);
  }
  if (r_no) {
    copy_v3_v3(r_no, nearest.no);
  }
  if (r_index) {
    *r_index = nearest.index;
  }
  if (r_dist_sq) {
    float diff[3];
    sub_v3_v3v3(diff, co, nearest.co);
    *r_dist_sq = len_squared_v3(diff);
  }
}

bool nearest_world_tree(SnapObjectContext *sctx,
                        BVHTree *tree,
                        BVHTree_NearestPointCallback nearest_cb,
                        void *treedata,
                        const float (*obmat)[4],
                        const float init_co[3],
                        const float curr_co[3],
                        float *r_dist_sq,
                        float *r_loc,
                        float *r_no,
                        int *r_index)
{
  if (curr_co == nullptr || init_co == nullptr) {
    /* No location to work with, so just return. */
    return false;
  }

  float imat[4][4];
  invert_m4_m4(imat, obmat);

  float timat[3][3]; /* transpose inverse matrix for normals */
  transpose_m3_m4(timat, imat);

  /* compute offset between init co and prev co in local space */
  float init_co_local[3], curr_co_local[3];
  float delta_local[3];
  mul_v3_m4v3(init_co_local, imat, init_co);
  mul_v3_m4v3(curr_co_local, imat, curr_co);
  sub_v3_v3v3(delta_local, curr_co_local, init_co_local);

  float dist_sq;
  if (sctx->runtime.params.keep_on_same_target) {
    nearest_world_tree_co(
        tree, nearest_cb, treedata, init_co_local, nullptr, nullptr, nullptr, &dist_sq);
  }
  else {
    /* NOTE: when `params->face_nearest_steps == 1`, the return variables of function below contain
     * the answer.  We could return immediately after updating r_loc, r_no, r_index, but that would
     * also complicate the code. Foregoing slight optimization for code clarity. */
    nearest_world_tree_co(
        tree, nearest_cb, treedata, curr_co_local, nullptr, nullptr, nullptr, &dist_sq);
  }
  if (*r_dist_sq <= dist_sq) {
    return false;
  }
  *r_dist_sq = dist_sq;

  /* scale to make `snap_face_nearest_steps` steps */
  float step_scale_factor = 1.0f / max_ff(1.0f, float(sctx->runtime.params.face_nearest_steps));
  mul_v3_fl(delta_local, step_scale_factor);

  float co_local[3];
  float no_local[3];

  copy_v3_v3(co_local, init_co_local);

  for (int i = 0; i < sctx->runtime.params.face_nearest_steps; i++) {
    add_v3_v3(co_local, delta_local);
    nearest_world_tree_co(
        tree, nearest_cb, treedata, co_local, co_local, no_local, r_index, nullptr);
  }

  mul_v3_m4v3(r_loc, obmat, co_local);

  if (r_no) {
    mul_v3_m3v3(r_no, timat, no_local);
    normalize_v3(r_no);
  }

  return true;
}

static eSnapMode nearest_world_object_fn(SnapObjectContext *sctx,
                                         Object *ob_eval,
                                         ID *ob_data,
                                         const float obmat[4][4],
                                         bool is_object_active,
                                         bool use_hide)
{
  bool retval = false;
  bool is_edit = false;

  if (ob_data == nullptr) {
    if (ob_eval->type == OB_MESH) {
      if (snap_object_editmesh(
              sctx, ob_eval, nullptr, obmat, SCE_SNAP_MODE_FACE_NEAREST, use_hide)) {
        retval = true;
        is_edit = true;
      }
    }
    else {
      return SCE_SNAP_MODE_NONE;
    }
  }
  else if (GS(ob_data->name) != ID_ME) {
    return SCE_SNAP_MODE_NONE;
  }
  else if (is_object_active && ELEM(ob_eval->type, OB_CURVES_LEGACY, OB_SURF, OB_FONT)) {
    return SCE_SNAP_MODE_NONE;
  }
  else if (snap_object_mesh(sctx, ob_eval, ob_data, obmat, SCE_SNAP_MODE_FACE_NEAREST, use_hide)) {
    retval = true;
  }

  if (retval) {
    copy_m4_m4(sctx->ret.obmat, obmat);
    sctx->ret.ob = ob_eval;
    sctx->ret.data = ob_data;
    sctx->ret.is_edit = is_edit;
    return SCE_SNAP_MODE_FACE_NEAREST;
  }
  return SCE_SNAP_MODE_NONE;
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
  return iter_snap_objects(sctx, nearest_world_object_fn) != SCE_SNAP_MODE_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Nearest utilities
 * \{ */

/* Test BoundBox */
bool snap_bound_box_check_dist(const float min[3],
                               const float max[3],
                               const float lpmat[4][4],
                               const float win_size[2],
                               const float mval[2],
                               float dist_px_sq)
{
  /* In vertex and edges you need to get the pixel distance from ray to BoundBox,
   * see: #46099, #46816 */

  DistProjectedAABBPrecalc data_precalc;
  dist_squared_to_projected_aabb_precalc(&data_precalc, lpmat, win_size, mval);

  bool dummy[3];
  float bb_dist_px_sq = dist_squared_to_projected_aabb(&data_precalc, min, max, dummy);

  if (bb_dist_px_sq > dist_px_sq) {
    return false;
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks
 * \{ */

static bool test_projected_vert_dist(const DistProjectedAABBPrecalc *precalc,
                                     const float (*clip_plane)[4],
                                     const int clip_plane_len,
                                     const bool is_persp,
                                     const float co[3],
                                     float *dist_px_sq,
                                     float r_co[3])
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
  if (dist_sq < *dist_px_sq) {
    copy_v3_v3(r_co, co);
    *dist_px_sq = dist_sq;
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
                                     float *dist_px_sq,
                                     float r_co[3])
{
  float near_co[3], lambda;
  if (!isect_ray_line_v3(precalc->ray_origin, precalc->ray_direction, va, vb, &lambda)) {
    copy_v3_v3(near_co, va);
  }
  else {
    if (lambda <= 0.0f) {
      copy_v3_v3(near_co, va);
    }
    else if (lambda >= 1.0f) {
      copy_v3_v3(near_co, vb);
    }
    else {
      interp_v3_v3v3(near_co, va, vb, lambda);
    }
  }

  return test_projected_vert_dist(
      precalc, clip_plane, clip_plane_len, is_persp, near_co, dist_px_sq, r_co);
}

void cb_snap_vert(void *userdata,
                  int index,
                  const DistProjectedAABBPrecalc *precalc,
                  const float (*clip_plane)[4],
                  const int clip_plane_len,
                  BVHTreeNearest *nearest)
{
  Nearest2dUserData *data = static_cast<Nearest2dUserData *>(userdata);

  const float *co;
  data->get_vert_co(index, data, &co);

  if (test_projected_vert_dist(
          precalc, clip_plane, clip_plane_len, data->is_persp, co, &nearest->dist_sq, nearest->co))
  {
    data->copy_vert_no(index, data, nearest->no);
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
  Nearest2dUserData *data = static_cast<Nearest2dUserData *>(userdata);

  int vindex[2];
  data->get_edge_verts_index(index, data, vindex);

  const float *v_pair[2];
  data->get_vert_co(vindex[0], data, &v_pair[0]);
  data->get_vert_co(vindex[1], data, &v_pair[1]);

  if (test_projected_edge_dist(precalc,
                               clip_plane,
                               clip_plane_len,
                               data->is_persp,
                               v_pair[0],
                               v_pair[1],
                               &nearest->dist_sq,
                               nearest->co))
  {
    sub_v3_v3v3(nearest->no, v_pair[0], v_pair[1]);
    nearest->index = index;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Object Snapping API
 * \{ */

static eSnapMode snap_polygon(SnapObjectContext *sctx, eSnapMode snap_to_flag)
{
  float lpmat[4][4];
  mul_m4_m4m4(lpmat, sctx->runtime.pmat, sctx->ret.obmat);

  float tobmat[4][4], clip_planes_local[MAX_CLIPPLANE_LEN][4];
  transpose_m4_m4(tobmat, sctx->ret.obmat);
  for (int i = sctx->runtime.clip_plane_len; i--;) {
    mul_v4_m4v4(clip_planes_local[i], tobmat, sctx->runtime.clip_plane[i]);
  }

  const Mesh *mesh = sctx->ret.data && GS(sctx->ret.data->name) == ID_ME ?
                         (const Mesh *)sctx->ret.data :
                         nullptr;
  if (mesh) {
    return snap_polygon_mesh(sctx,
                             sctx->ret.ob,
                             sctx->ret.data,
                             sctx->ret.obmat,
                             snap_to_flag,
                             sctx->ret.index,
                             clip_planes_local);
  }
  else if (sctx->ret.is_edit) {
    return snap_polygon_editmesh(sctx,
                                 sctx->ret.ob,
                                 sctx->ret.data,
                                 sctx->ret.obmat,
                                 snap_to_flag,
                                 sctx->ret.index,
                                 clip_planes_local);
  }

  return SCE_SNAP_MODE_NONE;
}

static eSnapMode snap_mesh_edge_verts_mixed(SnapObjectContext *sctx, float original_dist_px)
{
  eSnapMode elem = SCE_SNAP_MODE_EDGE;

  if (sctx->ret.ob->type != OB_MESH) {
    return elem;
  }

  Nearest2dUserData nearest2d;
  {
    const Mesh *mesh = sctx->ret.data && GS(sctx->ret.data->name) == ID_ME ?
                           (const Mesh *)sctx->ret.data :
                           nullptr;
    if (mesh) {
      nearest2d_data_init_mesh(
          mesh, sctx->runtime.is_persp, sctx->runtime.params.use_backface_culling, &nearest2d);
    }
    else if (sctx->ret.is_edit) {
      /* The object's #BMEditMesh was used to snap instead. */
      nearest2d_data_init_editmesh(BKE_editmesh_from_object(sctx->ret.ob),
                                   sctx->runtime.is_persp,
                                   sctx->runtime.params.use_backface_culling,
                                   &nearest2d);
    }
    else {
      return elem;
    }
  }

  int vindex[2];
  nearest2d.get_edge_verts_index(sctx->ret.index, &nearest2d, vindex);

  const float *v_pair[2];
  nearest2d.get_vert_co(vindex[0], &nearest2d, &v_pair[0]);
  nearest2d.get_vert_co(vindex[1], &nearest2d, &v_pair[1]);

  DistProjectedAABBPrecalc neasrest_precalc;
  {
    float lpmat[4][4];
    mul_m4_m4m4(lpmat, sctx->runtime.pmat, sctx->ret.obmat);

    dist_squared_to_projected_aabb_precalc(
        &neasrest_precalc, lpmat, sctx->runtime.win_size, sctx->runtime.mval);
  }

  BVHTreeNearest nearest{};
  nearest.index = -1;
  nearest.dist_sq = square_f(original_dist_px);

  float lambda;
  if (!isect_ray_line_v3(neasrest_precalc.ray_origin,
                         neasrest_precalc.ray_direction,
                         v_pair[0],
                         v_pair[1],
                         &lambda))
  {
    /* Do nothing. */
  }
  else {
    short snap_to_flag = sctx->runtime.snap_to_flag;
    int e_mode_len = ((snap_to_flag & SCE_SNAP_MODE_EDGE) != 0) +
                     ((snap_to_flag & SCE_SNAP_MODE_VERTEX) != 0) +
                     ((snap_to_flag & SCE_SNAP_MODE_EDGE_MIDPOINT) != 0);

    float range = 1.0f / (2 * e_mode_len - 1);
    if (snap_to_flag & SCE_SNAP_MODE_VERTEX) {
      if (lambda < (range) || (1.0f - range) < lambda) {
        int v_id = lambda < 0.5f ? 0 : 1;

        if (test_projected_vert_dist(&neasrest_precalc,
                                     nullptr,
                                     0,
                                     nearest2d.is_persp,
                                     v_pair[v_id],
                                     &nearest.dist_sq,
                                     nearest.co))
        {
          nearest.index = vindex[v_id];
          elem = SCE_SNAP_MODE_VERTEX;
          {
            float imat[4][4];
            invert_m4_m4(imat, sctx->ret.obmat);
            nearest2d.copy_vert_no(vindex[v_id], &nearest2d, sctx->ret.no);
            mul_transposed_mat3_m4_v3(imat, sctx->ret.no);
            normalize_v3(sctx->ret.no);
          }
        }
      }
    }

    if (snap_to_flag & SCE_SNAP_MODE_EDGE_MIDPOINT) {
      range *= e_mode_len - 1;
      if ((range) < lambda && lambda < (1.0f - range)) {
        float vmid[3];
        mid_v3_v3v3(vmid, v_pair[0], v_pair[1]);

        if (test_projected_vert_dist(&neasrest_precalc,
                                     nullptr,
                                     0,
                                     nearest2d.is_persp,
                                     vmid,
                                     &nearest.dist_sq,
                                     nearest.co))
        {
          nearest.index = sctx->ret.index;
          elem = SCE_SNAP_MODE_EDGE_MIDPOINT;
        }
      }
    }

    if (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_EDGE_PERPENDICULAR) {
      float v_near[3], va_g[3], vb_g[3];

      mul_v3_m4v3(va_g, sctx->ret.obmat, v_pair[0]);
      mul_v3_m4v3(vb_g, sctx->ret.obmat, v_pair[1]);
      lambda = line_point_factor_v3(sctx->runtime.curr_co, va_g, vb_g);

      if (IN_RANGE(lambda, 0.0f, 1.0f)) {
        interp_v3_v3v3(v_near, va_g, vb_g, lambda);

        if (len_squared_v3v3(sctx->runtime.curr_co, v_near) > FLT_EPSILON) {
          dist_squared_to_projected_aabb_precalc(
              &neasrest_precalc, sctx->runtime.pmat, sctx->runtime.win_size, sctx->runtime.mval);

          if (test_projected_vert_dist(&neasrest_precalc,
                                       nullptr,
                                       0,
                                       nearest2d.is_persp,
                                       v_near,
                                       &nearest.dist_sq,
                                       nearest.co))
          {
            nearest.index = sctx->ret.index;
            elem = SCE_SNAP_MODE_EDGE_PERPENDICULAR;
          }
        }
      }
    }
  }

  if (nearest.index != -1) {
    sctx->ret.dist_px_sq = nearest.dist_sq;

    copy_v3_v3(sctx->ret.loc, nearest.co);
    if (elem != SCE_SNAP_MODE_EDGE_PERPENDICULAR) {
      mul_m4_v3(sctx->ret.obmat, sctx->ret.loc);
    }

    sctx->ret.index = nearest.index;
  }

  return elem;
}

static eSnapMode snapArmature(SnapObjectContext *sctx,
                              Object *ob_eval,
                              const float obmat[4][4],
                              bool is_object_active)
{
  eSnapMode retval = SCE_SNAP_MODE_NONE;

  if (sctx->runtime.snap_to_flag == SCE_SNAP_MODE_FACE) {
    /* Currently only edge and vert. */
    return retval;
  }

  float lpmat[4][4];
  mul_m4_m4m4(lpmat, sctx->runtime.pmat, obmat);

  DistProjectedAABBPrecalc neasrest_precalc;
  dist_squared_to_projected_aabb_precalc(
      &neasrest_precalc, lpmat, sctx->runtime.win_size, sctx->runtime.mval);

  bArmature *arm = static_cast<bArmature *>(ob_eval->data);
  const bool is_editmode = arm->edbo != nullptr;

  if (is_editmode == false) {
    /* Test BoundBox. */
    const BoundBox *bb = BKE_armature_boundbox_get(ob_eval);
    if (bb && !snap_bound_box_check_dist(bb->vec[0],
                                         bb->vec[6],
                                         lpmat,
                                         sctx->runtime.win_size,
                                         sctx->runtime.mval,
                                         sctx->ret.dist_px_sq))
    {
      return retval;
    }
  }

  float tobmat[4][4], clip_planes_local[MAX_CLIPPLANE_LEN][4];
  transpose_m4_m4(tobmat, obmat);
  for (int i = sctx->runtime.clip_plane_len; i--;) {
    mul_v4_m4v4(clip_planes_local[i], tobmat, sctx->runtime.clip_plane[i]);
  }

  const bool is_posemode = is_object_active && (ob_eval->mode & OB_MODE_POSE);
  const bool skip_selected = (is_editmode || is_posemode) &&
                             (sctx->runtime.params.snap_target_select &
                              SCE_SNAP_TARGET_NOT_SELECTED);
  const bool is_persp = sctx->runtime.is_persp;

  if (arm->edbo) {
    LISTBASE_FOREACH (EditBone *, eBone, arm->edbo) {
      if (eBone->layer & arm->layer) {
        if (eBone->flag & BONE_HIDDEN_A) {
          /* Skip hidden bones. */
          continue;
        }

        const bool is_selected = (eBone->flag & (BONE_ROOTSEL | BONE_TIPSEL)) != 0;
        if (is_selected && skip_selected) {
          continue;
        }
        bool has_vert_snap = false;

        if (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX) {
          has_vert_snap = test_projected_vert_dist(&neasrest_precalc,
                                                   clip_planes_local,
                                                   sctx->runtime.clip_plane_len,
                                                   is_persp,
                                                   eBone->head,
                                                   &sctx->ret.dist_px_sq,
                                                   sctx->ret.loc);
          has_vert_snap |= test_projected_vert_dist(&neasrest_precalc,
                                                    clip_planes_local,
                                                    sctx->runtime.clip_plane_len,
                                                    is_persp,
                                                    eBone->tail,

                                                    &sctx->ret.dist_px_sq,
                                                    sctx->ret.loc);

          if (has_vert_snap) {
            retval = SCE_SNAP_MODE_VERTEX;
          }
        }
        if (!has_vert_snap && sctx->runtime.snap_to_flag & SCE_SNAP_MODE_EDGE) {
          if (test_projected_edge_dist(&neasrest_precalc,
                                       clip_planes_local,
                                       sctx->runtime.clip_plane_len,
                                       is_persp,
                                       eBone->head,
                                       eBone->tail,
                                       &sctx->ret.dist_px_sq,
                                       sctx->ret.loc))
          {
            retval = SCE_SNAP_MODE_EDGE;
          }
        }
      }
    }
  }
  else if (ob_eval->pose && ob_eval->pose->chanbase.first) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob_eval->pose->chanbase) {
      Bone *bone = pchan->bone;
      if (!bone || (bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG))) {
        /* Skip hidden bones. */
        continue;
      }

      const bool is_selected = (bone->flag & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL)) != 0;
      if (is_selected && skip_selected) {
        continue;
      }

      bool has_vert_snap = false;
      const float *head_vec = pchan->pose_head;
      const float *tail_vec = pchan->pose_tail;

      if (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX) {
        has_vert_snap = test_projected_vert_dist(&neasrest_precalc,
                                                 clip_planes_local,
                                                 sctx->runtime.clip_plane_len,
                                                 is_persp,
                                                 head_vec,
                                                 &sctx->ret.dist_px_sq,
                                                 sctx->ret.loc);
        has_vert_snap |= test_projected_vert_dist(&neasrest_precalc,
                                                  clip_planes_local,
                                                  sctx->runtime.clip_plane_len,
                                                  is_persp,
                                                  tail_vec,
                                                  &sctx->ret.dist_px_sq,
                                                  sctx->ret.loc);

        if (has_vert_snap) {
          retval = SCE_SNAP_MODE_VERTEX;
        }
      }
      if (!has_vert_snap && sctx->runtime.snap_to_flag & SCE_SNAP_MODE_EDGE) {
        if (test_projected_edge_dist(&neasrest_precalc,
                                     clip_planes_local,
                                     sctx->runtime.clip_plane_len,
                                     is_persp,
                                     head_vec,
                                     tail_vec,
                                     &sctx->ret.dist_px_sq,
                                     sctx->ret.loc))
        {
          retval = SCE_SNAP_MODE_EDGE;
        }
      }
    }
  }

  if (retval) {
    mul_m4_v3(obmat, sctx->ret.loc);
    /* Does not support index. */
    sctx->ret.index = -1;
    return retval;
  }

  return SCE_SNAP_MODE_NONE;
}

static eSnapMode snapCurve(SnapObjectContext *sctx, Object *ob_eval, const float obmat[4][4])
{
  bool has_snap = false;

  /* Only vertex snapping mode (eg control points and handles) supported for now). */
  if ((sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX) == 0) {
    return SCE_SNAP_MODE_NONE;
  }

  Curve *cu = static_cast<Curve *>(ob_eval->data);

  float lpmat[4][4];
  mul_m4_m4m4(lpmat, sctx->runtime.pmat, obmat);

  DistProjectedAABBPrecalc neasrest_precalc;
  dist_squared_to_projected_aabb_precalc(
      &neasrest_precalc, lpmat, sctx->runtime.win_size, sctx->runtime.mval);

  const bool use_obedit = BKE_object_is_in_editmode(ob_eval);

  if (use_obedit == false) {
    /* Test BoundBox */
    BoundBox *bb = BKE_curve_boundbox_get(ob_eval);
    if (bb && !snap_bound_box_check_dist(bb->vec[0],
                                         bb->vec[6],
                                         lpmat,
                                         sctx->runtime.win_size,
                                         sctx->runtime.mval,
                                         sctx->ret.dist_px_sq))
    {
      return SCE_SNAP_MODE_NONE;
    }
  }

  float tobmat[4][4];
  transpose_m4_m4(tobmat, obmat);

  float(*clip_planes)[4] = sctx->runtime.clip_plane;
  int clip_plane_len = sctx->runtime.clip_plane_len;

  if (sctx->runtime.has_occlusion_plane) {
    /* We snap to vertices even if occluded. */
    clip_planes++;
    clip_plane_len--;
  }

  float clip_planes_local[MAX_CLIPPLANE_LEN][4];
  for (int i = clip_plane_len; i--;) {
    mul_v4_m4v4(clip_planes_local[i], tobmat, clip_planes[i]);
  }

  bool is_persp = sctx->runtime.is_persp;
  bool skip_selected = (sctx->runtime.params.snap_target_select & SCE_SNAP_TARGET_NOT_SELECTED) !=
                       0;

  LISTBASE_FOREACH (Nurb *, nu, (use_obedit ? &cu->editnurb->nurbs : &cu->nurb)) {
    for (int u = 0; u < nu->pntsu; u++) {
      if (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX) {
        if (use_obedit) {
          if (nu->bezt) {
            if (nu->bezt[u].hide) {
              /* Skip hidden. */
              continue;
            }

            bool is_selected = (nu->bezt[u].f2 & SELECT) != 0;
            if (is_selected && skip_selected) {
              continue;
            }
            has_snap |= test_projected_vert_dist(&neasrest_precalc,
                                                 clip_planes_local,
                                                 clip_plane_len,
                                                 is_persp,
                                                 nu->bezt[u].vec[1],
                                                 &sctx->ret.dist_px_sq,
                                                 sctx->ret.loc);

            /* Don't snap if handle is selected (moving),
             * or if it is aligning to a moving handle. */
            bool is_selected_h1 = (nu->bezt[u].f1 & SELECT) != 0;
            bool is_selected_h2 = (nu->bezt[u].f3 & SELECT) != 0;
            bool is_autoalign_h1 = (nu->bezt[u].h1 & HD_ALIGN) != 0;
            bool is_autoalign_h2 = (nu->bezt[u].h2 & HD_ALIGN) != 0;
            if (!skip_selected || !(is_selected_h1 || (is_autoalign_h1 && is_selected_h2))) {
              has_snap |= test_projected_vert_dist(&neasrest_precalc,
                                                   clip_planes_local,
                                                   clip_plane_len,
                                                   is_persp,
                                                   nu->bezt[u].vec[0],
                                                   &sctx->ret.dist_px_sq,
                                                   sctx->ret.loc);
            }

            if (!skip_selected || !(is_selected_h2 || (is_autoalign_h2 && is_selected_h1))) {
              has_snap |= test_projected_vert_dist(&neasrest_precalc,
                                                   clip_planes_local,
                                                   clip_plane_len,
                                                   is_persp,
                                                   nu->bezt[u].vec[2],
                                                   &sctx->ret.dist_px_sq,
                                                   sctx->ret.loc);
            }
          }
          else {
            if (nu->bp[u].hide) {
              /* Skip hidden. */
              continue;
            }

            bool is_selected = (nu->bp[u].f1 & SELECT) != 0;
            if (is_selected && skip_selected) {
              continue;
            }

            has_snap |= test_projected_vert_dist(&neasrest_precalc,
                                                 clip_planes_local,
                                                 clip_plane_len,
                                                 is_persp,
                                                 nu->bp[u].vec,
                                                 &sctx->ret.dist_px_sq,
                                                 sctx->ret.loc);
          }
        }
        else {
          /* Curve is not visible outside editmode if nurb length less than two. */
          if (nu->pntsu > 1) {
            if (nu->bezt) {
              has_snap |= test_projected_vert_dist(&neasrest_precalc,
                                                   clip_planes_local,
                                                   clip_plane_len,
                                                   is_persp,
                                                   nu->bezt[u].vec[1],
                                                   &sctx->ret.dist_px_sq,
                                                   sctx->ret.loc);
            }
            else {
              has_snap |= test_projected_vert_dist(&neasrest_precalc,
                                                   clip_planes_local,
                                                   clip_plane_len,
                                                   is_persp,
                                                   nu->bp[u].vec,
                                                   &sctx->ret.dist_px_sq,
                                                   sctx->ret.loc);
            }
          }
        }
      }
    }
  }
  if (has_snap) {
    mul_m4_v3(obmat, sctx->ret.loc);
    /* Does not support index yet. */
    sctx->ret.index = -1;
    return SCE_SNAP_MODE_VERTEX;
  }

  return SCE_SNAP_MODE_NONE;
}

/* may extend later (for now just snaps to empty center) */
static eSnapMode snap_object_center(SnapObjectContext *sctx,
                                    Object *ob_eval,
                                    const float obmat[4][4],
                                    eSnapMode snap_to_flag)
{
  eSnapMode retval = SCE_SNAP_MODE_NONE;

  if (ob_eval->transflag & OB_DUPLI) {
    return retval;
  }

  /* For now only vertex supported. */
  if ((snap_to_flag & SCE_SNAP_MODE_VERTEX) == 0) {
    return retval;
  }

  DistProjectedAABBPrecalc neasrest_precalc;
  dist_squared_to_projected_aabb_precalc(
      &neasrest_precalc, sctx->runtime.pmat, sctx->runtime.win_size, sctx->runtime.mval);

  bool is_persp = sctx->runtime.is_persp;

  if (test_projected_vert_dist(&neasrest_precalc,
                               sctx->runtime.clip_plane,
                               sctx->runtime.clip_plane_len,
                               is_persp,
                               obmat[3],
                               &sctx->ret.dist_px_sq,
                               sctx->ret.loc))
  {
    retval = SCE_SNAP_MODE_VERTEX;
  }

  if (retval) {
    sctx->ret.index = -1;
    return retval;
  }

  return SCE_SNAP_MODE_NONE;
}

static eSnapMode snapCamera(SnapObjectContext *sctx,
                            Object *object,
                            const float obmat[4][4],
                            eSnapMode snap_to_flag)
{
  eSnapMode retval = SCE_SNAP_MODE_NONE;

  Scene *scene = sctx->scene;

  bool is_persp = sctx->runtime.is_persp;

  float orig_camera_mat[4][4], orig_camera_imat[4][4], imat[4][4];
  MovieClip *clip = BKE_object_movieclip_get(scene, object, false);
  MovieTracking *tracking;

  if (clip == nullptr) {
    return snap_object_center(sctx, object, obmat, snap_to_flag);
  }
  if (object->transflag & OB_DUPLI) {
    return retval;
  }

  tracking = &clip->tracking;

  BKE_tracking_get_camera_object_matrix(object, orig_camera_mat);

  invert_m4_m4(orig_camera_imat, orig_camera_mat);
  invert_m4_m4(imat, obmat);

  if (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX) {
    DistProjectedAABBPrecalc neasrest_precalc;
    dist_squared_to_projected_aabb_precalc(
        &neasrest_precalc, sctx->runtime.pmat, sctx->runtime.win_size, sctx->runtime.mval);

    LISTBASE_FOREACH (MovieTrackingObject *, tracking_object, &tracking->objects) {
      float reconstructed_camera_mat[4][4], reconstructed_camera_imat[4][4];
      const float(*vertex_obmat)[4];

      if ((tracking_object->flag & TRACKING_OBJECT_CAMERA) == 0) {
        BKE_tracking_camera_get_reconstructed_interpolate(
            tracking, tracking_object, scene->r.cfra, reconstructed_camera_mat);

        invert_m4_m4(reconstructed_camera_imat, reconstructed_camera_mat);
      }

      LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
        float bundle_pos[3];

        if ((track->flag & TRACK_HAS_BUNDLE) == 0) {
          continue;
        }

        copy_v3_v3(bundle_pos, track->bundle_pos);
        if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
          vertex_obmat = orig_camera_mat;
        }
        else {
          mul_m4_v3(reconstructed_camera_imat, bundle_pos);
          vertex_obmat = obmat;
        }

        mul_m4_v3(vertex_obmat, bundle_pos);
        if (test_projected_vert_dist(&neasrest_precalc,
                                     sctx->runtime.clip_plane,
                                     sctx->runtime.clip_plane_len,
                                     is_persp,
                                     bundle_pos,
                                     &sctx->ret.dist_px_sq,
                                     sctx->ret.loc))
        {
          retval = SCE_SNAP_MODE_VERTEX;
        }
      }
    }
  }

  if (retval) {
    /* Does not support index. */
    sctx->ret.index = -1;
    return retval;
  }

  return SCE_SNAP_MODE_NONE;
}

/**
 * \note Duplicate args here are documented at #snapObjectsRay
 */
static eSnapMode snap_obj_fn(SnapObjectContext *sctx,
                             Object *ob_eval,
                             ID *ob_data,
                             const float obmat[4][4],
                             bool is_object_active,
                             bool use_hide)
{
  eSnapMode retval = SCE_SNAP_MODE_NONE;
  bool is_edit = false;

  if (ob_data == nullptr && (ob_eval->type == OB_MESH)) {
    retval = snap_object_editmesh(
        sctx, ob_eval, nullptr, obmat, sctx->runtime.snap_to_flag, use_hide);
    if (retval) {
      is_edit = true;
    }
  }
  else if (ob_data == nullptr) {
    retval = snap_object_center(sctx, ob_eval, obmat, sctx->runtime.snap_to_flag);
  }
  else {
    switch (ob_eval->type) {
      case OB_MESH: {
        if (ob_eval->dt == OB_BOUNDBOX) {
          /* Do not snap to objects that are in bounding box display mode */
          return SCE_SNAP_MODE_NONE;
        }
        if (GS(ob_data->name) == ID_ME) {
          retval = snap_object_mesh(
              sctx, ob_eval, ob_data, obmat, sctx->runtime.snap_to_flag, use_hide);
        }
        break;
      }
      case OB_ARMATURE:
        retval = snapArmature(sctx, ob_eval, obmat, is_object_active);
        break;
      case OB_CURVES_LEGACY:
      case OB_SURF:
        if (ob_eval->type == OB_CURVES_LEGACY || BKE_object_is_in_editmode(ob_eval)) {
          retval = snapCurve(sctx, ob_eval, obmat);
          if (sctx->runtime.params.edit_mode_type != SNAP_GEOM_FINAL) {
            break;
          }
        }
        ATTR_FALLTHROUGH;
      case OB_FONT: {
        const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
        if (mesh_eval) {
          retval |= snap_object_mesh(
              sctx, ob_eval, (ID *)mesh_eval, obmat, sctx->runtime.snap_to_flag, use_hide);
        }
        break;
      }
      case OB_EMPTY:
      case OB_GPENCIL_LEGACY:
      case OB_LAMP:
        retval = snap_object_center(sctx, ob_eval, obmat, sctx->runtime.snap_to_flag);
        break;
      case OB_CAMERA:
        retval = snapCamera(sctx, ob_eval, obmat, sctx->runtime.snap_to_flag);
        break;
    }
  }

  if (retval) {
    copy_m4_m4(sctx->ret.obmat, obmat);
    sctx->ret.ob = ob_eval;
    sctx->ret.data = ob_data;
    sctx->ret.is_edit = is_edit;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Object Snapping API
 * \{ */

SnapObjectContext *ED_transform_snap_object_context_create(Scene *scene, int /*flag*/)
{
  SnapObjectContext *sctx = MEM_new<SnapObjectContext>(__func__);

  sctx->scene = scene;

  return sctx;
}

void ED_transform_snap_object_context_destroy(SnapObjectContext *sctx)
{
  MEM_delete(sctx);
}

void ED_transform_snap_object_context_set_editmesh_callbacks(
    SnapObjectContext *sctx,
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
                                             const SnapObjectParams *params,
                                             const float ray_start[3],
                                             const float ray_dir[3],
                                             const float ray_depth,
                                             const float mval[2],
                                             const float init_co[3],
                                             const float prev_co[3],
                                             const float dist_px_sq,
                                             ListBase *hit_list,
                                             bool use_occlusion_test)
{
  if (snap_to_flag & (SCE_SNAP_MODE_EDGE_PERPENDICULAR | SCE_SNAP_MODE_FACE_NEAREST)) {
    if (prev_co) {
      copy_v3_v3(sctx->runtime.curr_co, prev_co);
      if (init_co) {
        copy_v3_v3(sctx->runtime.init_co, init_co);
      }
      else {
        snap_to_flag &= ~SCE_SNAP_MODE_FACE_NEAREST;
      }
    }
    else {
      snap_to_flag &= ~(SCE_SNAP_MODE_EDGE_PERPENDICULAR | SCE_SNAP_MODE_FACE_NEAREST);
    }
  }

  if (snap_to_flag == SCE_SNAP_MODE_NONE) {
    return false;
  }

  sctx->runtime.depsgraph = depsgraph;
  sctx->runtime.region = region;
  sctx->runtime.v3d = v3d;
  sctx->runtime.snap_to_flag = snap_to_flag;
  sctx->runtime.params = *params;
  sctx->runtime.params.use_occlusion_test = use_occlusion_test;
  sctx->runtime.use_occlusion_test_edit = use_occlusion_test &&
                                          (snap_to_flag & SCE_SNAP_MODE_FACE) == 0;
  sctx->runtime.has_occlusion_plane = false;
  sctx->runtime.object_index = 0;

  copy_v3_v3(sctx->runtime.ray_start, ray_start);
  copy_v3_v3(sctx->runtime.ray_dir, ray_dir);

  if (mval) {
    copy_v2_v2(sctx->runtime.mval, mval);
  }

  if (region) {
    const RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
    copy_m4_m4(sctx->runtime.pmat, rv3d->persmat);
    sctx->runtime.win_size[0] = region->winx;
    sctx->runtime.win_size[1] = region->winy;

    planes_from_projmat(sctx->runtime.pmat,
                        nullptr,
                        nullptr,
                        nullptr,
                        nullptr,
                        sctx->runtime.clip_plane[0],
                        sctx->runtime.clip_plane[1]);

    sctx->runtime.clip_plane_len = 2;
    sctx->runtime.is_persp = rv3d->is_persp;
  }

  sctx->ret.ray_depth_max = ray_depth;
  zero_v3(sctx->ret.loc);
  zero_v3(sctx->ret.no);
  sctx->ret.index = -1;
  zero_m4(sctx->ret.obmat);
  sctx->ret.hit_list = hit_list;
  sctx->ret.ob = nullptr;
  sctx->ret.data = nullptr;
  sctx->ret.dist_px_sq = dist_px_sq;
  sctx->ret.is_edit = false;

  return true;
}

bool ED_transform_snap_object_project_ray_ex(SnapObjectContext *sctx,
                                             Depsgraph *depsgraph,
                                             const View3D *v3d,
                                             const SnapObjectParams *params,
                                             const float ray_start[3],
                                             const float ray_normal[3],
                                             float *ray_depth,
                                             float r_loc[3],
                                             float r_no[3],
                                             int *r_index,
                                             Object **r_ob,
                                             float r_obmat[4][4])
{
  if (!snap_object_context_runtime_init(sctx,
                                        depsgraph,
                                        nullptr,
                                        v3d,
                                        SCE_SNAP_MODE_FACE,
                                        params,
                                        ray_start,
                                        ray_normal,
                                        !ray_depth || *ray_depth == -1.0f ? BVH_RAYCAST_DIST_MAX :
                                                                            *ray_depth,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        0,
                                        nullptr,
                                        params->use_occlusion_test))
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
      copy_m4_m4(r_obmat, sctx->ret.obmat);
    }
    if (ray_depth) {
      *ray_depth = sctx->ret.ray_depth_max;
    }
    return true;
  }
  return false;
}

bool ED_transform_snap_object_project_ray_all(SnapObjectContext *sctx,
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
                                        SCE_SNAP_MODE_FACE,
                                        params,
                                        ray_start,
                                        ray_normal,
                                        ray_depth == -1.0f ? BVH_RAYCAST_DIST_MAX : ray_depth,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        0,
                                        r_hit_list,
                                        params->use_occlusion_test))
  {
    return false;
  }

#ifdef DEBUG
  float ray_depth_prev = sctx->ret.ray_depth_max;
#endif
  if (raycastObjects(sctx)) {
    if (sort) {
      BLI_listbase_sort(r_hit_list, hit_depth_cmp);
    }
    /* meant to be readonly for 'all' hits, ensure it is */
#ifdef DEBUG
    BLI_assert(ray_depth_prev == sctx->ret.ray_depth_max);
#endif
    return true;
  }
  return false;
}

/**
 * Convenience function for snap ray-casting.
 *
 * Given a ray, cast it into the scene (snapping to faces).
 *
 * \return Snap success
 */
bool ED_transform_snap_object_project_ray(SnapObjectContext *sctx,
                                          Depsgraph *depsgraph,
                                          const View3D *v3d,
                                          const SnapObjectParams *params,
                                          const float ray_start[3],
                                          const float ray_normal[3],
                                          float *ray_depth,
                                          float r_co[3],
                                          float r_no[3])
{
  return ED_transform_snap_object_project_ray_ex(sctx,
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

eSnapMode ED_transform_snap_object_project_view3d_ex(SnapObjectContext *sctx,
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
                                                     Object **r_ob,
                                                     float r_obmat[4][4],
                                                     float r_face_nor[3])
{
  eSnapMode retval = SCE_SNAP_MODE_NONE;

  bool use_occlusion_test = params->use_occlusion_test;
  if (use_occlusion_test && XRAY_ENABLED(v3d)) {
    if (snap_to_flag != SCE_SNAP_MODE_FACE) {
      /* In theory everything is visible in X-Ray except faces. */
      snap_to_flag &= ~SCE_SNAP_MODE_FACE;
      use_occlusion_test = false;
    }
  }

  if (use_occlusion_test || (snap_to_flag & SCE_SNAP_MODE_FACE)) {
    if (!ED_view3d_win_to_ray_clipped_ex(depsgraph,
                                         region,
                                         v3d,
                                         mval,
                                         nullptr,
                                         sctx->runtime.ray_dir,
                                         sctx->runtime.ray_start,
                                         true))
    {
      snap_to_flag &= ~SCE_SNAP_MODE_FACE;
      use_occlusion_test = false;
    }
  }

  if (!snap_object_context_runtime_init(sctx,
                                        depsgraph,
                                        region,
                                        v3d,
                                        snap_to_flag,
                                        params,
                                        sctx->runtime.ray_start,
                                        sctx->runtime.ray_dir,
                                        BVH_RAYCAST_DIST_MAX,
                                        mval,
                                        init_co,
                                        prev_co,
                                        square_f(*dist_px),
                                        nullptr,
                                        use_occlusion_test))
  {
    return retval;
  }

  snap_to_flag = sctx->runtime.snap_to_flag;

  BLI_assert(snap_to_flag & (SCE_SNAP_MODE_GEOM | SCE_SNAP_MODE_FACE_NEAREST));

  bool has_hit = false;

  /* NOTE: if both face ray-cast and face nearest are enabled, first find result of nearest, then
   * override with ray-cast. */
  if ((snap_to_flag & SCE_SNAP_MODE_FACE_NEAREST) && !has_hit) {
    has_hit = nearestWorldObjects(sctx);

    if (has_hit) {
      retval = SCE_SNAP_MODE_FACE_NEAREST;

      copy_v3_v3(r_loc, sctx->ret.loc);
      if (r_no) {
        copy_v3_v3(r_no, sctx->ret.no);
      }
      if (r_ob) {
        *r_ob = sctx->ret.ob;
      }
      if (r_obmat) {
        copy_m4_m4(r_obmat, sctx->ret.obmat);
      }
      if (r_index) {
        *r_index = sctx->ret.index;
      }
    }
  }

  if ((snap_to_flag & SCE_SNAP_MODE_FACE) || sctx->runtime.params.use_occlusion_test) {
    has_hit = raycastObjects(sctx);

    if (has_hit) {
      if (r_face_nor) {
        copy_v3_v3(r_face_nor, sctx->ret.no);
      }

      if (snap_to_flag & SCE_SNAP_MODE_FACE) {
        retval = SCE_SNAP_MODE_FACE;

        copy_v3_v3(r_loc, sctx->ret.loc);
        if (r_no) {
          copy_v3_v3(r_no, sctx->ret.no);
        }
        if (r_ob) {
          *r_ob = sctx->ret.ob;
        }
        if (r_obmat) {
          copy_m4_m4(r_obmat, sctx->ret.obmat);
        }
        if (r_index) {
          *r_index = sctx->ret.index;
        }
      }
    }
  }

  if (snap_to_flag & (SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE | SCE_SNAP_MODE_EDGE_MIDPOINT |
                      SCE_SNAP_MODE_EDGE_PERPENDICULAR))
  {
    eSnapMode elem_test, elem = SCE_SNAP_MODE_NONE;

    /* First snap to edge instead of middle or perpendicular. */
    sctx->runtime.snap_to_flag &= (SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE);
    if (snap_to_flag & (SCE_SNAP_MODE_EDGE_MIDPOINT | SCE_SNAP_MODE_EDGE_PERPENDICULAR)) {
      sctx->runtime.snap_to_flag |= SCE_SNAP_MODE_EDGE;
    }

    /* By convention we only snap to the original elements of a curve. */
    if (has_hit && sctx->ret.ob->type != OB_CURVES_LEGACY) {
      /* Compute the new clip_pane but do not add it yet. */
      float new_clipplane[4];
      BLI_ASSERT_UNIT_V3(sctx->ret.no);
      plane_from_point_normal_v3(new_clipplane, sctx->ret.loc, sctx->ret.no);
      if (dot_v3v3(sctx->runtime.clip_plane[0], new_clipplane) > 0.0f) {
        /* The plane is facing the wrong direction. */
        negate_v4(new_clipplane);
      }

      /* Small offset to simulate a kind of volume for edges and vertices. */
      new_clipplane[3] += 0.01f;

      /* Try to snap only to the polygon. */
      elem_test = snap_polygon(sctx, sctx->runtime.snap_to_flag);
      if (elem_test) {
        elem = elem_test;
      }

      /* Add the new clip plane to the beginning of the list. */
      for (int i = sctx->runtime.clip_plane_len; i != 0; i--) {
        copy_v4_v4(sctx->runtime.clip_plane[i], sctx->runtime.clip_plane[i - 1]);
      }
      copy_v4_v4(sctx->runtime.clip_plane[0], new_clipplane);
      sctx->runtime.clip_plane_len++;
      sctx->runtime.has_occlusion_plane = true;
    }

    elem_test = snapObjectsRay(sctx);
    if (elem_test) {
      elem = elem_test;
    }

    if ((elem == SCE_SNAP_MODE_EDGE) &&
        (snap_to_flag &
         (SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE_MIDPOINT | SCE_SNAP_MODE_EDGE_PERPENDICULAR)))
    {
      sctx->runtime.snap_to_flag = snap_to_flag;
      elem = snap_mesh_edge_verts_mixed(sctx, *dist_px);
    }

    if (elem & snap_to_flag) {
      retval = elem;

      copy_v3_v3(r_loc, sctx->ret.loc);
      if (r_no) {
        copy_v3_v3(r_no, sctx->ret.no);
      }
      if (r_ob) {
        *r_ob = sctx->ret.ob;
      }
      if (r_obmat) {
        copy_m4_m4(r_obmat, sctx->ret.obmat);
      }
      if (r_index) {
        *r_index = sctx->ret.index;
      }

      *dist_px = blender::math::sqrt(sctx->ret.dist_px_sq);
    }
  }

  return retval;
}

eSnapMode ED_transform_snap_object_project_view3d(SnapObjectContext *sctx,
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
  return ED_transform_snap_object_project_view3d_ex(sctx,
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

bool ED_transform_snap_object_project_all_view3d_ex(SnapObjectContext *sctx,
                                                    Depsgraph *depsgraph,
                                                    const ARegion *region,
                                                    const View3D *v3d,
                                                    const SnapObjectParams *params,
                                                    const float mval[2],
                                                    float ray_depth,
                                                    bool sort,
                                                    ListBase *r_hit_list)
{
  float ray_start[3], ray_normal[3];

  if (!ED_view3d_win_to_ray_clipped_ex(
          depsgraph, region, v3d, mval, nullptr, ray_normal, ray_start, true))
  {
    return false;
  }

  return ED_transform_snap_object_project_ray_all(
      sctx, depsgraph, v3d, params, ray_start, ray_normal, ray_depth, sort, r_hit_list);
}

/** \} */
