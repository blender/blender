/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_kdopbvh.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.hh"
#include "BLI_utildefines.h"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_armature.h"
#include "BKE_bvhutils.h"
#include "BKE_curve.h"
#include "BKE_duplilist.h"
#include "BKE_editmesh.h"
#include "BKE_geometry_set.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_object.h"
#include "BKE_tracking.h"

#include "DEG_depsgraph_query.h"

#include "ED_armature.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"

using blender::float3;
using blender::float4x4;
using blender::Map;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name Internal Data Types
 * \{ */

#define MAX_CLIPPLANE_LEN 3

enum eViewProj {
  VIEW_PROJ_NONE = -1,
  VIEW_PROJ_ORTHO = 0,
  VIEW_PROJ_PERSP = -1,
};

/** #SnapObjectContext.editmesh_caches */
struct SnapData_EditMesh {
  /* Verts, Edges. */
  BVHTree *bvhtree[2];
  bool cached[2];

  /* BVH tree from #BMEditMesh.looptris. */
  BVHTreeFromEditMesh treedata_editmesh;

  blender::bke::MeshRuntime *mesh_runtime;
  float min[3], max[3];

  void clear()
  {
    for (int i = 0; i < ARRAY_SIZE(this->bvhtree); i++) {
      if (!this->cached[i]) {
        BLI_bvhtree_free(this->bvhtree[i]);
      }
      this->bvhtree[i] = nullptr;
    }
    free_bvhtree_from_editmesh(&this->treedata_editmesh);
  }

  ~SnapData_EditMesh()
  {
    this->clear();
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("SnapData_EditMesh")
#endif
};

struct SnapObjectContext {
  Scene *scene;

  int flag;

  Map<const BMEditMesh *, std::unique_ptr<SnapData_EditMesh>> editmesh_caches;

  /* Filter data, returns true to check this value */
  struct {
    struct {
      bool (*test_vert_fn)(BMVert *, void *user_data);
      bool (*test_edge_fn)(BMEdge *, void *user_data);
      bool (*test_face_fn)(BMFace *, void *user_data);
      void *user_data;
    } edit_mesh;
  } callbacks;

  struct {
    Depsgraph *depsgraph;
    const ARegion *region;
    const View3D *v3d;

    float mval[2];
    float pmat[4][4];  /* perspective matrix */
    float win_size[2]; /* win x and y */
    enum eViewProj view_proj;
    float clip_plane[MAX_CLIPPLANE_LEN][4];
    short clip_plane_len;
    eSnapMode snap_to_flag;
    bool has_occlusion_plane; /* Ignore plane of occlusion in curves. */
  } runtime;

  /* Output. */
  struct {
    /* Location of snapped point on target surface. */
    float loc[3];
    /* Normal of snapped point on target surface. */
    float no[3];
    /* Index of snapped element on target object (-1 when no valid index is found). */
    int index;
    /* Matrix of target object (may not be #Object.object_to_world with dupli-instances). */
    float obmat[4][4];
    /* List of #SnapObjectHitDepth (caller must free). */
    ListBase *hit_list;
    /* Snapped object. */
    Object *ob;
    /* Snapped data. */
    ID *data;

    float dist_sq;

    bool is_edit;
  } ret;
};

/** \} */

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
/** \name Snap Object Data
 * \{ */

/**
 * Calculate the minimum and maximum coordinates of the box that encompasses this mesh.
 */
static void snap_editmesh_minmax(SnapObjectContext *sctx,
                                 BMesh *bm,
                                 float r_min[3],
                                 float r_max[3])
{
  INIT_MINMAX(r_min, r_max);
  BMIter iter;
  BMVert *v;

  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    if (sctx->callbacks.edit_mesh.test_vert_fn &&
        !sctx->callbacks.edit_mesh.test_vert_fn(v, sctx->callbacks.edit_mesh.user_data))
    {
      continue;
    }
    minmax_v3v3_v3(r_min, r_max, v->co);
  }
}

static void snap_object_data_mesh_get(const Mesh *me_eval,
                                      bool use_hide,
                                      BVHTreeFromMesh *r_treedata)
{
  const Span<float3> vert_positions = me_eval->vert_positions();
  const blender::OffsetIndices polys = me_eval->polys();
  const Span<int> corner_verts = me_eval->corner_verts();

  /* The BVHTree from looptris is always required. */
  BKE_bvhtree_from_mesh_get(
      r_treedata, me_eval, use_hide ? BVHTREE_FROM_LOOPTRI_NO_HIDDEN : BVHTREE_FROM_LOOPTRI, 4);

  BLI_assert(reinterpret_cast<const float3 *>(r_treedata->vert_positions) ==
             vert_positions.data());
  BLI_assert(r_treedata->corner_verts == corner_verts.data());
  BLI_assert(!polys.data() || r_treedata->looptri);
  BLI_assert(!r_treedata->tree || r_treedata->looptri);

  UNUSED_VARS_NDEBUG(vert_positions, polys, corner_verts);
}

/* Searches for the #Mesh_Runtime associated with the object that is most likely to be updated due
 * to changes in the `edit_mesh`. */
static blender::bke::MeshRuntime *snap_object_data_editmesh_runtime_get(Object *ob_eval)
{
  Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(ob_eval);
  if (editmesh_eval_final) {
    return editmesh_eval_final->runtime;
  }

  Mesh *editmesh_eval_cage = BKE_object_get_editmesh_eval_cage(ob_eval);
  if (editmesh_eval_cage) {
    return editmesh_eval_cage->runtime;
  }

  return ((Mesh *)ob_eval->data)->runtime;
}

static SnapData_EditMesh *snap_object_data_editmesh_get(SnapObjectContext *sctx,
                                                        Object *ob_eval,
                                                        BMEditMesh *em)
{
  SnapData_EditMesh *sod;
  bool init = false;

  if (std::unique_ptr<SnapData_EditMesh> *sod_p = sctx->editmesh_caches.lookup_ptr(em)) {
    sod = sod_p->get();
    bool is_dirty = false;
    /* Check if the geometry has changed. */
    if (sod->treedata_editmesh.em != em) {
      is_dirty = true;
    }
    else if (sod->mesh_runtime) {
      if (sod->mesh_runtime != snap_object_data_editmesh_runtime_get(ob_eval)) {
        if (G.moving) {
          /* WORKAROUND: avoid updating while transforming. */
          BLI_assert(!sod->treedata_editmesh.cached && !sod->cached[0] && !sod->cached[1]);
          sod->mesh_runtime = snap_object_data_editmesh_runtime_get(ob_eval);
        }
        else {
          is_dirty = true;
        }
      }
      else if (sod->treedata_editmesh.tree && sod->treedata_editmesh.cached &&
               !bvhcache_has_tree(sod->mesh_runtime->bvh_cache, sod->treedata_editmesh.tree))
      {
        /* The tree is owned by the EditMesh and may have been freed since we last used! */
        is_dirty = true;
      }
      else if (sod->bvhtree[0] && sod->cached[0] &&
               !bvhcache_has_tree(sod->mesh_runtime->bvh_cache, sod->bvhtree[0]))
      {
        /* The tree is owned by the EditMesh and may have been freed since we last used! */
        is_dirty = true;
      }
      else if (sod->bvhtree[1] && sod->cached[1] &&
               !bvhcache_has_tree(sod->mesh_runtime->bvh_cache, sod->bvhtree[1]))
      {
        /* The tree is owned by the EditMesh and may have been freed since we last used! */
        is_dirty = true;
      }
    }

    if (is_dirty) {
      sod->clear();
      init = true;
    }
  }
  else {
    std::unique_ptr<SnapData_EditMesh> sod_ptr = std::make_unique<SnapData_EditMesh>();
    sod = sod_ptr.get();
    sctx->editmesh_caches.add_new(em, std::move(sod_ptr));
    init = true;
  }

  if (init) {
    sod->treedata_editmesh.em = em;
    sod->mesh_runtime = snap_object_data_editmesh_runtime_get(ob_eval);
    snap_editmesh_minmax(sctx, em->bm, sod->min, sod->max);
  }

  return sod;
}

static BVHTreeFromEditMesh *snap_object_data_editmesh_treedata_get(SnapObjectContext *sctx,
                                                                   Object *ob_eval,
                                                                   BMEditMesh *em)
{
  SnapData_EditMesh *sod = snap_object_data_editmesh_get(sctx, ob_eval, em);

  BVHTreeFromEditMesh *treedata = &sod->treedata_editmesh;

  if (treedata->tree == nullptr) {
    /* Operators only update the editmesh looptris of the original mesh. */
    BLI_assert(sod->treedata_editmesh.em ==
               BKE_editmesh_from_object(DEG_get_original_object(ob_eval)));
    em = sod->treedata_editmesh.em;

    if (sctx->callbacks.edit_mesh.test_face_fn) {
      BMesh *bm = em->bm;
      BLI_assert(poly_to_tri_count(bm->totface, bm->totloop) == em->tottri);

      blender::BitVector<> elem_mask(em->tottri);
      int looptri_num_active = BM_iter_mesh_bitmap_from_filter_tessface(
          bm,
          elem_mask,
          sctx->callbacks.edit_mesh.test_face_fn,
          sctx->callbacks.edit_mesh.user_data);

      bvhtree_from_editmesh_looptri_ex(treedata, em, elem_mask, looptri_num_active, 0.0f, 4, 6);
    }
    else {
      /* Only cache if BVH-tree is created without a mask.
       * This helps keep a standardized BVH-tree in cache. */
      BKE_bvhtree_from_editmesh_get(treedata,
                                    em,
                                    4,
                                    BVHTREE_FROM_EM_LOOPTRI,
                                    /* WORKAROUND: avoid updating while transforming. */
                                    G.moving ? nullptr : &sod->mesh_runtime->bvh_cache,
                                    &sod->mesh_runtime->eval_mutex);
    }
  }
  if (treedata->tree == nullptr) {
    return nullptr;
  }

  return treedata;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Iterator
 * \{ */

using IterSnapObjsCallback = eSnapMode (*)(SnapObjectContext *sctx,
                                           const SnapObjectParams *params,
                                           Object *ob_eval,
                                           ID *ob_data,
                                           const float obmat[4][4],
                                           bool is_object_active,
                                           bool use_hide,
                                           void *data);

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
static eSnapMode iter_snap_objects(SnapObjectContext *sctx,
                                   const SnapObjectParams *params,
                                   IterSnapObjsCallback sob_callback,
                                   void *data)
{
  eSnapMode ret = SCE_SNAP_MODE_NONE;
  eSnapMode tmp;

  Scene *scene = DEG_get_input_scene(sctx->runtime.depsgraph);
  ViewLayer *view_layer = DEG_get_input_view_layer(sctx->runtime.depsgraph);
  const eSnapTargetOP snap_target_select = params->snap_target_select;
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base_act = BKE_view_layer_active_base_get(view_layer);

  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (!snap_object_is_snappable(sctx, snap_target_select, base_act, base)) {
      continue;
    }

    const bool is_object_active = (base == base_act);
    Object *obj_eval = DEG_get_evaluated_object(sctx->runtime.depsgraph, base->object);
    if (obj_eval->transflag & OB_DUPLI || BKE_object_has_geometry_set_instances(obj_eval)) {
      ListBase *lb = object_duplilist(sctx->runtime.depsgraph, sctx->scene, obj_eval);
      LISTBASE_FOREACH (DupliObject *, dupli_ob, lb) {
        BLI_assert(DEG_is_evaluated_object(dupli_ob->ob));
        if ((tmp = sob_callback(sctx,
                                params,
                                dupli_ob->ob,
                                dupli_ob->ob_data,
                                dupli_ob->mat,
                                is_object_active,
                                false,
                                data)) != SCE_SNAP_MODE_NONE)
        {
          ret = tmp;
        }
      }
      free_object_duplilist(lb);
    }

    bool use_hide = false;
    ID *ob_data = data_for_snap(obj_eval, params->edit_mode_type, &use_hide);
    if ((tmp = sob_callback(sctx,
                            params,
                            obj_eval,
                            ob_data,
                            obj_eval->object_to_world,
                            is_object_active,
                            use_hide,
                            data)) != SCE_SNAP_MODE_NONE)
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

struct RayCastAll_Data {
  void *bvhdata;

  /* internal vars for adding depths */
  BVHTree_RayCastCallback raycast_callback;

  const float (*obmat)[4];
  const float (*timat)[3];

  float len_diff;
  float local_scale;

  Object *ob_eval;
  uint ob_uuid;

  /* output data */
  ListBase *hit_list;
  bool retval;
};

static SnapObjectHitDepth *hit_depth_create(const float depth,
                                            const float co[3],
                                            const float no[3],
                                            int index,
                                            Object *ob_eval,
                                            const float obmat[4][4],
                                            uint ob_uuid)
{
  SnapObjectHitDepth *hit = MEM_new<SnapObjectHitDepth>(__func__);

  hit->depth = depth;
  copy_v3_v3(hit->co, co);
  copy_v3_v3(hit->no, no);
  hit->index = index;

  hit->ob_eval = ob_eval;
  copy_m4_m4(hit->obmat, (float(*)[4])obmat);
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

static void raycast_all_cb(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
  RayCastAll_Data *data = static_cast<RayCastAll_Data *>(userdata);
  data->raycast_callback(data->bvhdata, index, ray, hit);
  if (hit->index != -1) {
    /* Get all values in world-space. */
    float location[3], normal[3];
    float depth;

    /* World-space location. */
    mul_v3_m4v3(location, (float(*)[4])data->obmat, hit->co);
    depth = (hit->dist + data->len_diff) / data->local_scale;

    /* World-space normal. */
    copy_v3_v3(normal, hit->no);
    mul_m3_v3((float(*)[3])data->timat, normal);
    normalize_v3(normal);

    SnapObjectHitDepth *hit_item = hit_depth_create(
        depth, location, normal, hit->index, data->ob_eval, data->obmat, data->ob_uuid);
    BLI_addtail(data->hit_list, hit_item);
  }
}

static bool raycast_tri_backface_culling_test(
    const float dir[3], const float v0[3], const float v1[3], const float v2[3], float no[3])
{
  cross_tri_v3(no, v0, v1, v2);
  return dot_v3v3(no, dir) < 0.0f;
}

/* Callback to ray-cast with back-face culling (#Mesh). */
static void mesh_looptri_raycast_backface_culling_cb(void *userdata,
                                                     int index,
                                                     const BVHTreeRay *ray,
                                                     BVHTreeRayHit *hit)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const float(*vert_positions)[3] = data->vert_positions;
  const MLoopTri *lt = &data->looptri[index];
  const float *vtri_co[3] = {
      vert_positions[data->corner_verts[lt->tri[0]]],
      vert_positions[data->corner_verts[lt->tri[1]]],
      vert_positions[data->corner_verts[lt->tri[2]]],
  };
  float dist = bvhtree_ray_tri_intersection(ray, hit->dist, UNPACK3(vtri_co));

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

/* Callback to ray-cast with back-face culling (#EditMesh). */
static void editmesh_looptri_raycast_backface_culling_cb(void *userdata,
                                                         int index,
                                                         const BVHTreeRay *ray,
                                                         BVHTreeRayHit *hit)
{
  const BVHTreeFromEditMesh *data = (BVHTreeFromEditMesh *)userdata;
  BMEditMesh *em = data->em;
  const BMLoop **ltri = (const BMLoop **)em->looptris[index];

  const float *t0, *t1, *t2;
  t0 = ltri[0]->v->co;
  t1 = ltri[1]->v->co;
  t2 = ltri[2]->v->co;

  {
    float dist = bvhtree_ray_tri_intersection(ray, hit->dist, t0, t1, t2);

    if (dist >= 0 && dist < hit->dist) {
      float no[3];
      if (raycast_tri_backface_culling_test(ray->direction, t0, t1, t2, no)) {
        hit->index = index;
        hit->dist = dist;
        madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);
        normalize_v3_v3(hit->no, no);
      }
    }
  }
}

static bool raycastMesh(const SnapObjectParams *params,
                        const float ray_start[3],
                        const float ray_dir[3],
                        Object *ob_eval,
                        const Mesh *me_eval,
                        const float obmat[4][4],
                        const uint ob_index,
                        bool use_hide,
                        /* read/write args */
                        float *ray_depth,
                        /* return args */
                        float r_loc[3],
                        float r_no[3],
                        int *r_index,
                        ListBase *r_hit_list)
{
  bool retval = false;

  if (me_eval->totpoly == 0) {
    return retval;
  }

  float imat[4][4];
  float ray_start_local[3], ray_normal_local[3];
  float local_scale, local_depth, len_diff = 0.0f;

  invert_m4_m4(imat, obmat);

  copy_v3_v3(ray_start_local, ray_start);
  copy_v3_v3(ray_normal_local, ray_dir);

  mul_m4_v3(imat, ray_start_local);
  mul_mat3_m4_v3(imat, ray_normal_local);

  /* local scale in normal direction */
  local_scale = normalize_v3(ray_normal_local);
  local_depth = *ray_depth;
  if (local_depth != BVH_RAYCAST_DIST_MAX) {
    local_depth *= local_scale;
  }

  /* Test BoundBox */
  if (ob_eval->data == me_eval) {
    const BoundBox *bb = BKE_object_boundbox_get(ob_eval);
    if (bb) {
      /* was BKE_boundbox_ray_hit_check, see: cf6ca226fa58 */
      if (!isect_ray_aabb_v3_simple(
              ray_start_local, ray_normal_local, bb->vec[0], bb->vec[6], &len_diff, nullptr))
      {
        return retval;
      }
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

  BVHTreeFromMesh treedata;
  snap_object_data_mesh_get(me_eval, use_hide, &treedata);

  const blender::Span<int> looptri_polys = me_eval->looptri_polys();

  if (treedata.tree == nullptr) {
    return retval;
  }

  float timat[3][3]; /* transpose inverse matrix for normals */
  transpose_m3_m4(timat, imat);

  BLI_assert(treedata.raycast_callback != nullptr);
  if (r_hit_list) {
    RayCastAll_Data data;

    data.bvhdata = &treedata;
    data.raycast_callback = treedata.raycast_callback;
    data.obmat = obmat;
    data.timat = timat;
    data.len_diff = len_diff;
    data.local_scale = local_scale;
    data.ob_eval = ob_eval;
    data.ob_uuid = ob_index;
    data.hit_list = r_hit_list;
    data.retval = retval;

    BLI_bvhtree_ray_cast_all(
        treedata.tree, ray_start_local, ray_normal_local, 0.0f, *ray_depth, raycast_all_cb, &data);

    retval = data.retval;
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
                             params->use_backface_culling ?
                                 mesh_looptri_raycast_backface_culling_cb :
                                 treedata.raycast_callback,
                             &treedata) != -1)
    {
      hit.dist += len_diff;
      hit.dist /= local_scale;
      if (hit.dist <= *ray_depth) {
        *ray_depth = hit.dist;
        copy_v3_v3(r_loc, hit.co);

        /* Back to world-space. */
        mul_m4_v3(obmat, r_loc);

        if (r_no) {
          copy_v3_v3(r_no, hit.no);
          mul_m3_v3(timat, r_no);
          normalize_v3(r_no);
        }

        retval = true;

        if (r_index) {
          *r_index = looptri_polys[hit.index];
        }
      }
    }
  }

  return retval;
}

static bool raycastEditMesh(SnapObjectContext *sctx,
                            const SnapObjectParams *params,
                            const float ray_start[3],
                            const float ray_dir[3],
                            Object *ob_eval,
                            BMEditMesh *em,
                            const float obmat[4][4],
                            const uint ob_index,
                            /* read/write args */
                            float *ray_depth,
                            /* return args */
                            float r_loc[3],
                            float r_no[3],
                            int *r_index,
                            ListBase *r_hit_list)
{
  bool retval = false;
  if (em->bm->totface == 0) {
    return retval;
  }

  float imat[4][4];
  float ray_start_local[3], ray_normal_local[3];
  float local_scale, local_depth, len_diff = 0.0f;

  invert_m4_m4(imat, obmat);

  copy_v3_v3(ray_start_local, ray_start);
  copy_v3_v3(ray_normal_local, ray_dir);

  mul_m4_v3(imat, ray_start_local);
  mul_mat3_m4_v3(imat, ray_normal_local);

  /* local scale in normal direction */
  local_scale = normalize_v3(ray_normal_local);
  local_depth = *ray_depth;
  if (local_depth != BVH_RAYCAST_DIST_MAX) {
    local_depth *= local_scale;
  }

  SnapData_EditMesh *sod = snap_object_data_editmesh_get(sctx, ob_eval, em);

  /* Test BoundBox */

  /* was BKE_boundbox_ray_hit_check, see: cf6ca226fa58 */
  if (!isect_ray_aabb_v3_simple(
          ray_start_local, ray_normal_local, sod->min, sod->max, &len_diff, nullptr))
  {
    return retval;
  }

  /* We pass a temp ray_start, set from object's boundbox, to avoid precision issues with
   * very far away ray_start values (as returned in case of ortho view3d), see #50486, #38358.
   */
  if (len_diff > 400.0f) {
    len_diff -= local_scale; /* make temp start point a bit away from bbox hit point. */
    madd_v3_v3fl(ray_start_local, ray_normal_local, len_diff);
    local_depth -= len_diff;
  }
  else {
    len_diff = 0.0f;
  }

  BVHTreeFromEditMesh *treedata = snap_object_data_editmesh_treedata_get(sctx, ob_eval, em);
  if (treedata == nullptr) {
    return retval;
  }

  float timat[3][3]; /* transpose inverse matrix for normals */
  transpose_m3_m4(timat, imat);

  if (r_hit_list) {
    RayCastAll_Data data;

    data.bvhdata = treedata;
    data.raycast_callback = treedata->raycast_callback;
    data.obmat = obmat;
    data.timat = timat;
    data.len_diff = len_diff;
    data.local_scale = local_scale;
    data.ob_eval = ob_eval;
    data.ob_uuid = ob_index;
    data.hit_list = r_hit_list;
    data.retval = retval;

    BLI_bvhtree_ray_cast_all(treedata->tree,
                             ray_start_local,
                             ray_normal_local,
                             0.0f,
                             *ray_depth,
                             raycast_all_cb,
                             &data);

    retval = data.retval;
  }
  else {
    BVHTreeRayHit hit{};
    hit.index = -1;
    hit.dist = local_depth;

    if (BLI_bvhtree_ray_cast(treedata->tree,
                             ray_start_local,
                             ray_normal_local,
                             0.0f,
                             &hit,
                             params->use_backface_culling ?
                                 editmesh_looptri_raycast_backface_culling_cb :
                                 treedata->raycast_callback,
                             treedata) != -1)
    {
      hit.dist += len_diff;
      hit.dist /= local_scale;
      if (hit.dist <= *ray_depth) {
        *ray_depth = hit.dist;
        copy_v3_v3(r_loc, hit.co);

        /* Back to world-space. */
        mul_m4_v3(obmat, r_loc);

        if (r_no) {
          copy_v3_v3(r_no, hit.no);
          mul_m3_v3(timat, r_no);
          normalize_v3(r_no);
        }

        retval = true;

        if (r_index) {
          em = sod->treedata_editmesh.em;

          *r_index = BM_elem_index_get(em->looptris[hit.index][0]->f);
        }
      }
    }
  }

  return retval;
}

struct RaycastObjUserData {
  const float *ray_start;
  const float *ray_dir;
  uint ob_index;
  /* read/write args */
  float *ray_depth;

  uint use_occlusion_test : 1;
  uint use_occlusion_test_edit : 1;
};

/**
 * \note Duplicate args here are documented at #snapObjectsRay
 */
static eSnapMode raycast_obj_fn(SnapObjectContext *sctx,
                                const SnapObjectParams *params,
                                Object *ob_eval,
                                ID *ob_data,
                                const float obmat[4][4],
                                bool is_object_active,
                                bool use_hide,
                                void *data)
{
  RaycastObjUserData *dt = static_cast<RaycastObjUserData *>(data);
  const uint ob_index = dt->ob_index++;
  /* read/write args */
  float *ray_depth = dt->ray_depth;

  bool retval = false;
  bool is_edit = false;

  if (ob_data == nullptr) {
    if (dt->use_occlusion_test_edit && ELEM(ob_eval->dt, OB_BOUNDBOX, OB_WIRE)) {
      /* Do not hit objects that are in wire or bounding box display mode. */
      return SCE_SNAP_MODE_NONE;
    }
    if (ob_eval->type == OB_MESH) {
      BMEditMesh *em = BKE_editmesh_from_object(ob_eval);
      if (UNLIKELY(!em)) { /* See #mesh_for_snap doc-string. */
        return SCE_SNAP_MODE_NONE;
      }
      if (raycastEditMesh(sctx,
                          params,
                          dt->ray_start,
                          dt->ray_dir,
                          ob_eval,
                          em,
                          obmat,
                          ob_index,
                          ray_depth,
                          sctx->ret.loc,
                          sctx->ret.no,
                          &sctx->ret.index,
                          sctx->ret.hit_list))
      {
        retval = true;
        is_edit = true;
      }
    }
    else {
      return SCE_SNAP_MODE_NONE;
    }
  }
  else if (dt->use_occlusion_test && ELEM(ob_eval->dt, OB_BOUNDBOX, OB_WIRE)) {
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
    const Mesh *me_eval = (const Mesh *)ob_data;
    retval = raycastMesh(params,
                         dt->ray_start,
                         dt->ray_dir,
                         ob_eval,
                         me_eval,
                         obmat,
                         ob_index,
                         use_hide,
                         ray_depth,
                         sctx->ret.loc,
                         sctx->ret.no,
                         &sctx->ret.index,
                         sctx->ret.hit_list);
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
static bool raycastObjects(SnapObjectContext *sctx,
                           const SnapObjectParams *params,
                           const float ray_start[3],
                           const float ray_dir[3],
                           const bool use_occlusion_test,
                           const bool use_occlusion_test_edit,
                           /* read/write args */
                           /* Parameters below cannot be const, because they are assigned to a
                            * non-const variable (readability-non-const-parameter). */
                           float *ray_depth /* NOLINT */)
{
  RaycastObjUserData data = {};
  data.ray_start = ray_start;
  data.ray_dir = ray_dir;
  data.ob_index = 0;
  data.ray_depth = ray_depth;
  data.use_occlusion_test = use_occlusion_test;
  data.use_occlusion_test_edit = use_occlusion_test_edit;

  return iter_snap_objects(sctx, params, raycast_obj_fn, &data) != SCE_SNAP_MODE_NONE;
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

static bool nearest_world_tree(SnapObjectContext * /*sctx*/,
                               const SnapObjectParams *params,
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
  if (params->keep_on_same_target) {
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
  float step_scale_factor = 1.0f / max_ff(1.0f, float(params->face_nearest_steps));
  mul_v3_fl(delta_local, step_scale_factor);

  float co_local[3];
  float no_local[3];

  copy_v3_v3(co_local, init_co_local);

  for (int i = 0; i < params->face_nearest_steps; i++) {
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

static bool nearest_world_mesh(SnapObjectContext *sctx,
                               const SnapObjectParams *params,
                               const Mesh *me_eval,
                               const float (*obmat)[4],
                               bool use_hide,
                               const float init_co[3],
                               const float curr_co[3],
                               float *r_dist_sq,
                               float *r_loc,
                               float *r_no,
                               int *r_index)
{
  BVHTreeFromMesh treedata;
  snap_object_data_mesh_get(me_eval, use_hide, &treedata);
  if (treedata.tree == nullptr) {
    return false;
  }

  return nearest_world_tree(sctx,
                            params,
                            treedata.tree,
                            treedata.nearest_callback,
                            &treedata,
                            obmat,
                            init_co,
                            curr_co,
                            r_dist_sq,
                            r_loc,
                            r_no,
                            r_index);
}

static bool nearest_world_editmesh(SnapObjectContext *sctx,
                                   const SnapObjectParams *params,
                                   Object *ob_eval,
                                   BMEditMesh *em,
                                   const float (*obmat)[4],
                                   const float init_co[3],
                                   const float curr_co[3],
                                   float *r_dist_sq,
                                   float *r_loc,
                                   float *r_no,
                                   int *r_index)
{
  BVHTreeFromEditMesh *treedata = snap_object_data_editmesh_treedata_get(sctx, ob_eval, em);
  if (treedata == nullptr) {
    return false;
  }

  return nearest_world_tree(sctx,
                            params,
                            treedata->tree,
                            treedata->nearest_callback,
                            treedata,
                            obmat,
                            init_co,
                            curr_co,
                            r_dist_sq,
                            r_loc,
                            r_no,
                            r_index);
}
static eSnapMode nearest_world_object_fn(SnapObjectContext *sctx,
                                         const SnapObjectParams *params,
                                         Object *ob_eval,
                                         ID *ob_data,
                                         const float obmat[4][4],
                                         bool is_object_active,
                                         bool use_hide,
                                         void *data)
{
  NearestWorldObjUserData *dt = static_cast<NearestWorldObjUserData *>(data);

  bool retval = false;
  bool is_edit = false;

  if (ob_data == nullptr) {
    if (ob_eval->type == OB_MESH) {
      BMEditMesh *em = BKE_editmesh_from_object(ob_eval);
      if (UNLIKELY(!em)) { /* See #data_for_snap doc-string. */
        return SCE_SNAP_MODE_NONE;
      }
      if (nearest_world_editmesh(sctx,
                                 params,
                                 ob_eval,
                                 em,
                                 obmat,
                                 dt->init_co,
                                 dt->curr_co,
                                 &sctx->ret.dist_sq,
                                 sctx->ret.loc,
                                 sctx->ret.no,
                                 &sctx->ret.index))
      {
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
  else {
    const Mesh *me_eval = (const Mesh *)ob_data;
    retval = nearest_world_mesh(sctx,
                                params,
                                me_eval,
                                obmat,
                                use_hide,
                                dt->init_co,
                                dt->curr_co,
                                &sctx->ret.dist_sq,
                                sctx->ret.loc,
                                sctx->ret.no,
                                &sctx->ret.index);
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
static bool nearestWorldObjects(SnapObjectContext *sctx,
                                const SnapObjectParams *params,
                                const float init_co[3],
                                const float curr_co[3])
{
  NearestWorldObjUserData data = {};
  data.init_co = init_co;
  data.curr_co = curr_co;

  return iter_snap_objects(sctx, params, nearest_world_object_fn, &data) != SCE_SNAP_MODE_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Nearest utilities
 * \{ */

/* Test BoundBox */
static bool snap_bound_box_check_dist(const float min[3],
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

struct Nearest2dUserData;

using Nearest2DGetVertCoCallback = void (*)(const int index,
                                            const Nearest2dUserData *data,
                                            const float **r_co);
using Nearest2DGetEdgeVertsCallback = void (*)(const int index,
                                               const Nearest2dUserData *data,
                                               int r_v_index[2]);
using Nearest2DGetTriVertsCallback = void (*)(const int index,
                                              const Nearest2dUserData *data,
                                              int r_v_index[3]);
/* Equal the previous one */
using Nearest2DGetTriEdgesCallback = void (*)(const int index,
                                              const Nearest2dUserData *data,
                                              int r_e_index[3]);
using Nearest2DCopyVertNoCallback = void (*)(const int index,
                                             const Nearest2dUserData *data,
                                             float r_no[3]);

struct Nearest2dUserData {
  Nearest2DGetVertCoCallback get_vert_co;
  Nearest2DGetEdgeVertsCallback get_edge_verts_index;
  Nearest2DGetTriVertsCallback get_tri_verts_index;
  Nearest2DGetTriEdgesCallback get_tri_edges_index;
  Nearest2DCopyVertNoCallback copy_vert_no;

  union {
    struct {
      BMesh *bm;
    };
    struct {
      const float (*vert_positions)[3];
      const blender::float3 *vert_normals;
      const blender::int2 *edges; /* only used for #BVHTreeFromMeshEdges */
      const int *corner_verts;
      const int *corner_edges;
      const MLoopTri *looptris;
    };
  };

  bool is_persp;
  bool use_backface_culling;
};

static void cb_mvert_co_get(const int index, const Nearest2dUserData *data, const float **r_co)
{
  *r_co = data->vert_positions[index];
}

static void cb_bvert_co_get(const int index, const Nearest2dUserData *data, const float **r_co)
{
  BMVert *eve = BM_vert_at_index(data->bm, index);
  *r_co = eve->co;
}

static void cb_mvert_no_copy(const int index, const Nearest2dUserData *data, float r_no[3])
{
  copy_v3_v3(r_no, data->vert_normals[index]);
}

static void cb_bvert_no_copy(const int index, const Nearest2dUserData *data, float r_no[3])
{
  BMVert *eve = BM_vert_at_index(data->bm, index);

  copy_v3_v3(r_no, eve->no);
}

static void cb_medge_verts_get(const int index, const Nearest2dUserData *data, int r_v_index[2])
{
  const blender::int2 &edge = data->edges[index];

  r_v_index[0] = edge[0];
  r_v_index[1] = edge[1];
}

static void cb_bedge_verts_get(const int index, const Nearest2dUserData *data, int r_v_index[2])
{
  BMEdge *eed = BM_edge_at_index(data->bm, index);

  r_v_index[0] = BM_elem_index_get(eed->v1);
  r_v_index[1] = BM_elem_index_get(eed->v2);
}

static void cb_mlooptri_edges_get(const int index, const Nearest2dUserData *data, int r_v_index[3])
{
  const blender::int2 *edges = data->edges;
  const int *corner_verts = data->corner_verts;
  const int *corner_edges = data->corner_edges;
  const MLoopTri *lt = &data->looptris[index];
  for (int j = 2, j_next = 0; j_next < 3; j = j_next++) {
    const blender::int2 &edge = edges[corner_edges[lt->tri[j]]];
    const int tri_edge[2] = {corner_verts[lt->tri[j]], corner_verts[lt->tri[j_next]]};
    if (ELEM(edge[0], tri_edge[0], tri_edge[1]) && ELEM(edge[1], tri_edge[0], tri_edge[1])) {
      // printf("real edge found\n");
      r_v_index[j] = corner_edges[lt->tri[j]];
    }
    else {
      r_v_index[j] = -1;
    }
  }
}

static void cb_mlooptri_verts_get(const int index, const Nearest2dUserData *data, int r_v_index[3])
{
  const int *corner_verts = data->corner_verts;
  const MLoopTri *looptri = &data->looptris[index];

  r_v_index[0] = corner_verts[looptri->tri[0]];
  r_v_index[1] = corner_verts[looptri->tri[1]];
  r_v_index[2] = corner_verts[looptri->tri[2]];
}

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

static void cb_snap_vert(void *userdata,
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

static void cb_snap_edge(void *userdata,
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

static void cb_snap_edge_verts(void *userdata,
                               int index,
                               const DistProjectedAABBPrecalc *precalc,
                               const float (*clip_plane)[4],
                               const int clip_plane_len,
                               BVHTreeNearest *nearest)
{
  Nearest2dUserData *data = static_cast<Nearest2dUserData *>(userdata);

  int vindex[2];
  data->get_edge_verts_index(index, data, vindex);

  for (int i = 2; i--;) {
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
  Nearest2dUserData *data = static_cast<Nearest2dUserData *>(userdata);

  if (data->use_backface_culling) {
    int vindex[3];
    data->get_tri_verts_index(index, data, vindex);

    const float *t0, *t1, *t2;
    data->get_vert_co(vindex[0], data, &t0);
    data->get_vert_co(vindex[1], data, &t1);
    data->get_vert_co(vindex[2], data, &t2);
    float dummy[3];
    if (raycast_tri_backface_culling_test(precalc->ray_direction, t0, t1, t2, dummy)) {
      return;
    }
  }

  int eindex[3];
  data->get_tri_edges_index(index, data, eindex);
  for (int i = 3; i--;) {
    if (eindex[i] != -1) {
      if (eindex[i] == nearest->index) {
        continue;
      }
      cb_snap_edge(userdata, eindex[i], precalc, clip_plane, clip_plane_len, nearest);
    }
  }
}

static void cb_snap_tri_verts(void *userdata,
                              int index,
                              const DistProjectedAABBPrecalc *precalc,
                              const float (*clip_plane)[4],
                              const int clip_plane_len,
                              BVHTreeNearest *nearest)
{
  Nearest2dUserData *data = static_cast<Nearest2dUserData *>(userdata);

  int vindex[3];
  data->get_tri_verts_index(index, data, vindex);

  if (data->use_backface_culling) {
    const float *t0, *t1, *t2;
    data->get_vert_co(vindex[0], data, &t0);
    data->get_vert_co(vindex[1], data, &t1);
    data->get_vert_co(vindex[2], data, &t2);
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

static void nearest2d_data_init_mesh(const Mesh *mesh,
                                     bool is_persp,
                                     bool use_backface_culling,
                                     Nearest2dUserData *r_nearest2d)
{
  r_nearest2d->get_vert_co = cb_mvert_co_get;
  r_nearest2d->get_edge_verts_index = cb_medge_verts_get;
  r_nearest2d->copy_vert_no = cb_mvert_no_copy;
  r_nearest2d->get_tri_verts_index = cb_mlooptri_verts_get;
  r_nearest2d->get_tri_edges_index = cb_mlooptri_edges_get;

  r_nearest2d->vert_positions = BKE_mesh_vert_positions(mesh);
  r_nearest2d->vert_normals = mesh->vert_normals().data();
  r_nearest2d->edges = mesh->edges().data();
  r_nearest2d->corner_verts = mesh->corner_verts().data();
  r_nearest2d->corner_edges = mesh->corner_edges().data();
  r_nearest2d->looptris = mesh->looptris().data();

  r_nearest2d->is_persp = is_persp;
  r_nearest2d->use_backface_culling = use_backface_culling;
}

static void nearest2d_data_init_editmesh(SnapData_EditMesh *sod,
                                         bool is_persp,
                                         bool use_backface_culling,
                                         Nearest2dUserData *r_nearest2d)
{
  r_nearest2d->get_vert_co = cb_bvert_co_get;
  r_nearest2d->get_edge_verts_index = cb_bedge_verts_get;
  r_nearest2d->copy_vert_no = cb_bvert_no_copy;
  r_nearest2d->get_tri_verts_index = nullptr;
  r_nearest2d->get_tri_edges_index = nullptr;

  r_nearest2d->bm = sod->treedata_editmesh.em->bm;

  r_nearest2d->is_persp = is_persp;
  r_nearest2d->use_backface_culling = use_backface_culling;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Object Snapping API
 * \{ */

static eSnapMode snap_mesh_polygon(SnapObjectContext *sctx,
                                   const SnapObjectParams *params,
                                   /* read/write args */
                                   float *dist_px)
{
  eSnapMode elem = SCE_SNAP_MODE_NONE;

  float lpmat[4][4];
  mul_m4_m4m4(lpmat, sctx->runtime.pmat, sctx->ret.obmat);

  DistProjectedAABBPrecalc neasrest_precalc;
  dist_squared_to_projected_aabb_precalc(
      &neasrest_precalc, lpmat, sctx->runtime.win_size, sctx->runtime.mval);

  float tobmat[4][4], clip_planes_local[MAX_CLIPPLANE_LEN][4];
  transpose_m4_m4(tobmat, sctx->ret.obmat);
  for (int i = sctx->runtime.clip_plane_len; i--;) {
    mul_v4_m4v4(clip_planes_local[i], tobmat, sctx->runtime.clip_plane[i]);
  }

  BVHTreeNearest nearest{};
  nearest.index = -1;
  nearest.dist_sq = square_f(*dist_px);

  Nearest2dUserData nearest2d;
  const Mesh *mesh = sctx->ret.data && GS(sctx->ret.data->name) == ID_ME ?
                         (const Mesh *)sctx->ret.data :
                         nullptr;
  if (mesh) {
    nearest2d_data_init_mesh(mesh,
                             sctx->runtime.view_proj == VIEW_PROJ_PERSP,
                             params->use_backface_culling,
                             &nearest2d);

    const blender::IndexRange poly = mesh->polys()[sctx->ret.index];

    if (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_EDGE) {
      elem = SCE_SNAP_MODE_EDGE;
      BLI_assert(nearest2d.edges != nullptr);
      const int *poly_edges = &nearest2d.corner_edges[poly.start()];
      for (int i = poly.size(); i--;) {
        cb_snap_edge(&nearest2d,
                     poly_edges[i],
                     &neasrest_precalc,
                     clip_planes_local,
                     sctx->runtime.clip_plane_len,
                     &nearest);
      }
    }
    else {
      elem = SCE_SNAP_MODE_VERTEX;
      const int *poly_verts = &nearest2d.corner_verts[poly.start()];
      for (int i = poly.size(); i--;) {
        cb_snap_vert(&nearest2d,
                     poly_verts[i],
                     &neasrest_precalc,
                     clip_planes_local,
                     sctx->runtime.clip_plane_len,
                     &nearest);
      }
    }
  }
  else if (sctx->ret.is_edit) {
    /* The object's #BMEditMesh was used to snap instead. */
    std::unique_ptr<SnapData_EditMesh> &sod_editmesh = sctx->editmesh_caches.lookup(
        BKE_editmesh_from_object(sctx->ret.ob));
    BLI_assert(sod_editmesh.get() != nullptr);

    nearest2d_data_init_editmesh(sod_editmesh.get(),
                                 sctx->runtime.view_proj == VIEW_PROJ_PERSP,
                                 params->use_backface_culling,
                                 &nearest2d);

    BMEditMesh *em = sod_editmesh->treedata_editmesh.em;

    BM_mesh_elem_table_ensure(em->bm, BM_FACE);
    BMFace *f = BM_face_at_index(em->bm, sctx->ret.index);
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    if (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_EDGE) {
      elem = SCE_SNAP_MODE_EDGE;
      BM_mesh_elem_index_ensure(em->bm, BM_VERT | BM_EDGE);
      BM_mesh_elem_table_ensure(em->bm, BM_VERT | BM_EDGE);
      do {
        cb_snap_edge(&nearest2d,
                     BM_elem_index_get(l_iter->e),
                     &neasrest_precalc,
                     clip_planes_local,
                     sctx->runtime.clip_plane_len,
                     &nearest);
      } while ((l_iter = l_iter->next) != l_first);
    }
    else {
      elem = SCE_SNAP_MODE_VERTEX;
      BM_mesh_elem_index_ensure(em->bm, BM_VERT);
      BM_mesh_elem_table_ensure(em->bm, BM_VERT);
      do {
        cb_snap_vert(&nearest2d,
                     BM_elem_index_get(l_iter->v),
                     &neasrest_precalc,
                     clip_planes_local,
                     sctx->runtime.clip_plane_len,
                     &nearest);
      } while ((l_iter = l_iter->next) != l_first);
    }
  }

  if (nearest.index != -1) {
    *dist_px = sqrtf(nearest.dist_sq);

    copy_v3_v3(sctx->ret.loc, nearest.co);
    mul_m4_v3(sctx->ret.obmat, sctx->ret.loc);

    {
      float imat[4][4];
      invert_m4_m4(imat, sctx->ret.obmat);

      copy_v3_v3(sctx->ret.no, nearest.no);
      mul_transposed_mat3_m4_v3(imat, sctx->ret.no);
      normalize_v3(sctx->ret.no);
    }

    sctx->ret.index = nearest.index;
    return elem;
  }

  return SCE_SNAP_MODE_NONE;
}

static eSnapMode snap_mesh_edge_verts_mixed(SnapObjectContext *sctx,
                                            const SnapObjectParams *params,
                                            float original_dist_px,
                                            const float prev_co[3],
                                            /* read/write args */
                                            float *dist_px)
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
      nearest2d_data_init_mesh(mesh,
                               sctx->runtime.view_proj == VIEW_PROJ_PERSP,
                               params->use_backface_culling,
                               &nearest2d);
    }
    else if (sctx->ret.is_edit) {
      /* The object's #BMEditMesh was used to snap instead. */
      std::unique_ptr<SnapData_EditMesh> &sod_editmesh = sctx->editmesh_caches.lookup(
          BKE_editmesh_from_object(sctx->ret.ob));
      nearest2d_data_init_editmesh(sod_editmesh.get(),
                                   sctx->runtime.view_proj == VIEW_PROJ_PERSP,
                                   params->use_backface_culling,
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

    if (prev_co && (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_EDGE_PERPENDICULAR)) {
      float v_near[3], va_g[3], vb_g[3];

      mul_v3_m4v3(va_g, sctx->ret.obmat, v_pair[0]);
      mul_v3_m4v3(vb_g, sctx->ret.obmat, v_pair[1]);
      lambda = line_point_factor_v3(prev_co, va_g, vb_g);

      if (IN_RANGE(lambda, 0.0f, 1.0f)) {
        interp_v3_v3v3(v_near, va_g, vb_g, lambda);

        if (len_squared_v3v3(prev_co, v_near) > FLT_EPSILON) {
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
    *dist_px = sqrtf(nearest.dist_sq);

    copy_v3_v3(sctx->ret.loc, nearest.co);
    if (elem != SCE_SNAP_MODE_EDGE_PERPENDICULAR) {
      mul_m4_v3(sctx->ret.obmat, sctx->ret.loc);
    }

    sctx->ret.index = nearest.index;
  }

  return elem;
}

static eSnapMode snapArmature(SnapObjectContext *sctx,
                              const SnapObjectParams *params,
                              Object *ob_eval,
                              const float obmat[4][4],
                              bool is_object_active,
                              /* read/write args */
                              float *dist_px,
                              /* return args */
                              float r_loc[3],
                              float * /*r_no*/,
                              int *r_index)
{
  eSnapMode retval = SCE_SNAP_MODE_NONE;

  if (sctx->runtime.snap_to_flag == SCE_SNAP_MODE_FACE) {
    /* Currently only edge and vert. */
    return retval;
  }

  float lpmat[4][4], dist_px_sq = square_f(*dist_px);
  mul_m4_m4m4(lpmat, sctx->runtime.pmat, obmat);

  DistProjectedAABBPrecalc neasrest_precalc;
  dist_squared_to_projected_aabb_precalc(
      &neasrest_precalc, lpmat, sctx->runtime.win_size, sctx->runtime.mval);

  bArmature *arm = static_cast<bArmature *>(ob_eval->data);
  const bool is_editmode = arm->edbo != nullptr;

  if (is_editmode == false) {
    /* Test BoundBox. */
    const BoundBox *bb = BKE_armature_boundbox_get(ob_eval);
    if (bb &&
        !snap_bound_box_check_dist(
            bb->vec[0], bb->vec[6], lpmat, sctx->runtime.win_size, sctx->runtime.mval, dist_px_sq))
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
                             (params->snap_target_select & SCE_SNAP_TARGET_NOT_SELECTED);
  const bool is_persp = sctx->runtime.view_proj == VIEW_PROJ_PERSP;

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
                                                   &dist_px_sq,
                                                   r_loc);
          has_vert_snap |= test_projected_vert_dist(&neasrest_precalc,
                                                    clip_planes_local,
                                                    sctx->runtime.clip_plane_len,
                                                    is_persp,
                                                    eBone->tail,

                                                    &dist_px_sq,
                                                    r_loc);

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
                                       &dist_px_sq,
                                       r_loc))
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
                                                 &dist_px_sq,
                                                 r_loc);
        has_vert_snap |= test_projected_vert_dist(&neasrest_precalc,
                                                  clip_planes_local,
                                                  sctx->runtime.clip_plane_len,
                                                  is_persp,
                                                  tail_vec,
                                                  &dist_px_sq,
                                                  r_loc);

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
                                     &dist_px_sq,
                                     r_loc))
        {
          retval = SCE_SNAP_MODE_EDGE;
        }
      }
    }
  }

  if (retval) {
    *dist_px = sqrtf(dist_px_sq);
    mul_m4_v3(obmat, r_loc);
    if (r_index) {
      /* Does not support index. */
      *r_index = -1;
    }
    return retval;
  }

  return SCE_SNAP_MODE_NONE;
}

static eSnapMode snapCurve(SnapObjectContext *sctx,
                           const SnapObjectParams *params,
                           Object *ob_eval,
                           const float obmat[4][4],
                           /* read/write args */
                           float *dist_px,
                           /* return args */
                           float r_loc[3],
                           float * /*r_no*/,
                           int *r_index)
{
  bool has_snap = false;

  /* Only vertex snapping mode (eg control points and handles) supported for now). */
  if ((sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX) == 0) {
    return SCE_SNAP_MODE_NONE;
  }

  Curve *cu = static_cast<Curve *>(ob_eval->data);
  float dist_px_sq = square_f(*dist_px);

  float lpmat[4][4];
  mul_m4_m4m4(lpmat, sctx->runtime.pmat, obmat);

  DistProjectedAABBPrecalc neasrest_precalc;
  dist_squared_to_projected_aabb_precalc(
      &neasrest_precalc, lpmat, sctx->runtime.win_size, sctx->runtime.mval);

  const bool use_obedit = BKE_object_is_in_editmode(ob_eval);

  if (use_obedit == false) {
    /* Test BoundBox */
    BoundBox *bb = BKE_curve_boundbox_get(ob_eval);
    if (bb &&
        !snap_bound_box_check_dist(
            bb->vec[0], bb->vec[6], lpmat, sctx->runtime.win_size, sctx->runtime.mval, dist_px_sq))
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

  bool is_persp = sctx->runtime.view_proj == VIEW_PROJ_PERSP;
  bool skip_selected = params->snap_target_select & SCE_SNAP_TARGET_NOT_SELECTED;

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
                                                 &dist_px_sq,
                                                 r_loc);

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
                                                   &dist_px_sq,
                                                   r_loc);
            }

            if (!skip_selected || !(is_selected_h2 || (is_autoalign_h2 && is_selected_h1))) {
              has_snap |= test_projected_vert_dist(&neasrest_precalc,
                                                   clip_planes_local,
                                                   clip_plane_len,
                                                   is_persp,
                                                   nu->bezt[u].vec[2],
                                                   &dist_px_sq,
                                                   r_loc);
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
                                                 &dist_px_sq,
                                                 r_loc);
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
                                                   &dist_px_sq,
                                                   r_loc);
            }
            else {
              has_snap |= test_projected_vert_dist(&neasrest_precalc,
                                                   clip_planes_local,
                                                   clip_plane_len,
                                                   is_persp,
                                                   nu->bp[u].vec,
                                                   &dist_px_sq,
                                                   r_loc);
            }
          }
        }
      }
    }
  }
  if (has_snap) {
    *dist_px = sqrtf(dist_px_sq);
    mul_m4_v3(obmat, r_loc);
    if (r_index) {
      /* Does not support index yet. */
      *r_index = -1;
    }
    return SCE_SNAP_MODE_VERTEX;
  }

  return SCE_SNAP_MODE_NONE;
}

/* may extend later (for now just snaps to empty center) */
static eSnapMode snap_object_center(const SnapObjectContext *sctx,
                                    Object *ob_eval,
                                    const float obmat[4][4],
                                    /* read/write args */
                                    float *dist_px,
                                    /* return args */
                                    float r_loc[3],
                                    float * /*r_no*/,
                                    int *r_index)
{
  eSnapMode retval = SCE_SNAP_MODE_NONE;

  if (ob_eval->transflag & OB_DUPLI) {
    return retval;
  }

  /* For now only vertex supported. */
  if ((sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX) == 0) {
    return retval;
  }

  DistProjectedAABBPrecalc neasrest_precalc;
  dist_squared_to_projected_aabb_precalc(
      &neasrest_precalc, sctx->runtime.pmat, sctx->runtime.win_size, sctx->runtime.mval);

  bool is_persp = sctx->runtime.view_proj == VIEW_PROJ_PERSP;
  float dist_px_sq = square_f(*dist_px);

  if (test_projected_vert_dist(&neasrest_precalc,
                               sctx->runtime.clip_plane,
                               sctx->runtime.clip_plane_len,
                               is_persp,
                               obmat[3],
                               &dist_px_sq,
                               r_loc))
  {
    *dist_px = sqrtf(dist_px_sq);
    retval = SCE_SNAP_MODE_VERTEX;
  }

  if (retval) {
    if (r_index) {
      /* Does not support index. */
      *r_index = -1;
    }
    return retval;
  }

  return SCE_SNAP_MODE_NONE;
}

static eSnapMode snapCamera(const SnapObjectContext *sctx,
                            Object *object,
                            const float obmat[4][4],
                            /* read/write args */
                            float *dist_px,
                            /* return args */
                            float r_loc[3],
                            float *r_no,
                            int *r_index)
{
  eSnapMode retval = SCE_SNAP_MODE_NONE;

  Scene *scene = sctx->scene;

  bool is_persp = sctx->runtime.view_proj == VIEW_PROJ_PERSP;
  float dist_px_sq = square_f(*dist_px);

  float orig_camera_mat[4][4], orig_camera_imat[4][4], imat[4][4];
  MovieClip *clip = BKE_object_movieclip_get(scene, object, false);
  MovieTracking *tracking;

  if (clip == nullptr) {
    return snap_object_center(sctx, object, obmat, dist_px, r_loc, r_no, r_index);
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
                                     &dist_px_sq,
                                     r_loc))
        {
          retval = SCE_SNAP_MODE_VERTEX;
        }
      }
    }
  }

  if (retval) {
    *dist_px = sqrtf(dist_px_sq);
    if (r_index) {
      /* Does not support index. */
      *r_index = -1;
    }
    return retval;
  }

  return SCE_SNAP_MODE_NONE;
}

static eSnapMode snapMesh(SnapObjectContext *sctx,
                          const SnapObjectParams *params,
                          Object *ob_eval,
                          const Mesh *me_eval,
                          const float obmat[4][4],
                          bool use_hide,
                          /* read/write args */
                          float *dist_px,
                          /* return args */
                          float r_loc[3],
                          float r_no[3],
                          int *r_index)
{
  BLI_assert(sctx->runtime.snap_to_flag != SCE_SNAP_MODE_FACE);
  if (me_eval->totvert == 0) {
    return SCE_SNAP_MODE_NONE;
  }
  if (me_eval->totedge == 0 && !(sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX)) {
    return SCE_SNAP_MODE_NONE;
  }

  float lpmat[4][4];
  mul_m4_m4m4(lpmat, sctx->runtime.pmat, obmat);

  float dist_px_sq = square_f(*dist_px);

  /* Test BoundBox */
  if (ob_eval->data == me_eval) {
    const BoundBox *bb = BKE_object_boundbox_get(ob_eval);
    if (!snap_bound_box_check_dist(
            bb->vec[0], bb->vec[6], lpmat, sctx->runtime.win_size, sctx->runtime.mval, dist_px_sq))
    {
      return SCE_SNAP_MODE_NONE;
    }
  }

  BVHTreeFromMesh treedata, treedata_dummy;
  snap_object_data_mesh_get(me_eval, use_hide, &treedata);

  BVHTree *bvhtree[2] = {nullptr};
  bvhtree[0] = BKE_bvhtree_from_mesh_get(&treedata_dummy, me_eval, BVHTREE_FROM_LOOSEEDGES, 2);
  BLI_assert(treedata_dummy.cached);
  if (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX) {
    bvhtree[1] = BKE_bvhtree_from_mesh_get(&treedata_dummy, me_eval, BVHTREE_FROM_LOOSEVERTS, 2);
    BLI_assert(treedata_dummy.cached);
  }

  Nearest2dUserData nearest2d;
  nearest2d_data_init_mesh(me_eval,
                           sctx->runtime.view_proj == VIEW_PROJ_PERSP,
                           params->use_backface_culling,
                           &nearest2d);

  BVHTreeNearest nearest{};
  nearest.index = -1;
  nearest.dist_sq = dist_px_sq;

  int last_index = nearest.index;
  eSnapMode elem = SCE_SNAP_MODE_VERTEX;

  float tobmat[4][4], clip_planes_local[MAX_CLIPPLANE_LEN][4];
  transpose_m4_m4(tobmat, obmat);
  for (int i = sctx->runtime.clip_plane_len; i--;) {
    mul_v4_m4v4(clip_planes_local[i], tobmat, sctx->runtime.clip_plane[i]);
  }

  if (bvhtree[1]) {
    BLI_assert(sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX);
    /* snap to loose verts */
    BLI_bvhtree_find_nearest_projected(bvhtree[1],
                                       lpmat,
                                       sctx->runtime.win_size,
                                       sctx->runtime.mval,
                                       clip_planes_local,
                                       sctx->runtime.clip_plane_len,
                                       &nearest,
                                       cb_snap_vert,
                                       &nearest2d);

    last_index = nearest.index;
  }

  if (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_EDGE) {
    if (bvhtree[0]) {
      /* Snap to loose edges. */
      BLI_bvhtree_find_nearest_projected(bvhtree[0],
                                         lpmat,
                                         sctx->runtime.win_size,
                                         sctx->runtime.mval,
                                         clip_planes_local,
                                         sctx->runtime.clip_plane_len,
                                         &nearest,
                                         cb_snap_edge,
                                         &nearest2d);
    }

    if (treedata.tree) {
      /* Snap to looptris. */
      BLI_bvhtree_find_nearest_projected(treedata.tree,
                                         lpmat,
                                         sctx->runtime.win_size,
                                         sctx->runtime.mval,
                                         clip_planes_local,
                                         sctx->runtime.clip_plane_len,
                                         &nearest,
                                         cb_snap_tri_edges,
                                         &nearest2d);
    }

    if (last_index != nearest.index) {
      elem = SCE_SNAP_MODE_EDGE;
    }
  }
  else {
    BLI_assert(sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX);
    if (bvhtree[0]) {
      /* Snap to loose edge verts. */
      BLI_bvhtree_find_nearest_projected(bvhtree[0],
                                         lpmat,
                                         sctx->runtime.win_size,
                                         sctx->runtime.mval,
                                         clip_planes_local,
                                         sctx->runtime.clip_plane_len,
                                         &nearest,
                                         cb_snap_edge_verts,
                                         &nearest2d);
    }

    if (treedata.tree) {
      /* Snap to looptri verts. */
      BLI_bvhtree_find_nearest_projected(treedata.tree,
                                         lpmat,
                                         sctx->runtime.win_size,
                                         sctx->runtime.mval,
                                         clip_planes_local,
                                         sctx->runtime.clip_plane_len,
                                         &nearest,
                                         cb_snap_tri_verts,
                                         &nearest2d);
    }
  }

  if (nearest.index != -1) {
    *dist_px = sqrtf(nearest.dist_sq);

    copy_v3_v3(r_loc, nearest.co);
    mul_m4_v3(obmat, r_loc);

    if (r_no) {
      float imat[4][4];
      invert_m4_m4(imat, obmat);

      copy_v3_v3(r_no, nearest.no);
      mul_transposed_mat3_m4_v3(imat, r_no);
      normalize_v3(r_no);
    }
    if (r_index) {
      *r_index = nearest.index;
    }

    return elem;
  }

  return SCE_SNAP_MODE_NONE;
}

static eSnapMode snapEditMesh(SnapObjectContext *sctx,
                              const SnapObjectParams *params,
                              Object *ob_eval,
                              BMEditMesh *em,
                              const float obmat[4][4],
                              /* read/write args */
                              float *dist_px,
                              /* return args */
                              float r_loc[3],
                              float r_no[3],
                              int *r_index)
{
  BLI_assert(sctx->runtime.snap_to_flag != SCE_SNAP_MODE_FACE);

  if ((sctx->runtime.snap_to_flag & ~SCE_SNAP_MODE_FACE) == SCE_SNAP_MODE_VERTEX) {
    if (em->bm->totvert == 0) {
      return SCE_SNAP_MODE_NONE;
    }
  }
  else {
    if (em->bm->totedge == 0) {
      return SCE_SNAP_MODE_NONE;
    }
  }

  float lpmat[4][4];
  mul_m4_m4m4(lpmat, sctx->runtime.pmat, obmat);

  float dist_px_sq = square_f(*dist_px);

  SnapData_EditMesh *sod = snap_object_data_editmesh_get(sctx, ob_eval, em);

  /* Test BoundBox */

  /* Was BKE_boundbox_ray_hit_check, see: cf6ca226fa58. */
  if (!snap_bound_box_check_dist(
          sod->min, sod->max, lpmat, sctx->runtime.win_size, sctx->runtime.mval, dist_px_sq))
  {
    return SCE_SNAP_MODE_NONE;
  }

  if (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX) {
    BVHTreeFromEditMesh treedata{};
    treedata.tree = sod->bvhtree[0];

    if (treedata.tree == nullptr) {
      if (sctx->callbacks.edit_mesh.test_vert_fn) {
        blender::BitVector<> verts_mask(em->bm->totvert);
        const int verts_num_active = BM_iter_mesh_bitmap_from_filter(
            BM_VERTS_OF_MESH,
            em->bm,
            verts_mask,
            (bool (*)(BMElem *, void *))sctx->callbacks.edit_mesh.test_vert_fn,
            sctx->callbacks.edit_mesh.user_data);

        bvhtree_from_editmesh_verts_ex(&treedata, em, verts_mask, verts_num_active, 0.0f, 2, 6);
      }
      else {
        BKE_bvhtree_from_editmesh_get(&treedata,
                                      em,
                                      2,
                                      BVHTREE_FROM_EM_VERTS,
                                      /* WORKAROUND: avoid updating while transforming. */
                                      G.moving ? nullptr : &sod->mesh_runtime->bvh_cache,
                                      &sod->mesh_runtime->eval_mutex);
      }
      sod->bvhtree[0] = treedata.tree;
      sod->cached[0] = treedata.cached;
    }
  }

  if (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_EDGE) {
    BVHTreeFromEditMesh treedata{};
    treedata.tree = sod->bvhtree[1];

    if (treedata.tree == nullptr) {
      if (sctx->callbacks.edit_mesh.test_edge_fn) {
        blender::BitVector<> edges_mask(em->bm->totedge);
        const int edges_num_active = BM_iter_mesh_bitmap_from_filter(
            BM_EDGES_OF_MESH,
            em->bm,
            edges_mask,
            (bool (*)(BMElem *, void *))sctx->callbacks.edit_mesh.test_edge_fn,
            sctx->callbacks.edit_mesh.user_data);

        bvhtree_from_editmesh_edges_ex(&treedata, em, edges_mask, edges_num_active, 0.0f, 2, 6);
      }
      else {
        BKE_bvhtree_from_editmesh_get(&treedata,
                                      em,
                                      2,
                                      BVHTREE_FROM_EM_EDGES,
                                      /* WORKAROUND: avoid updating while transforming. */
                                      G.moving ? nullptr : &sod->mesh_runtime->bvh_cache,
                                      &sod->mesh_runtime->eval_mutex);
      }
      sod->bvhtree[1] = treedata.tree;
      sod->cached[1] = treedata.cached;
    }
  }

  Nearest2dUserData nearest2d;
  nearest2d_data_init_editmesh(
      sod, sctx->runtime.view_proj == VIEW_PROJ_PERSP, params->use_backface_culling, &nearest2d);

  BVHTreeNearest nearest{};
  nearest.index = -1;
  nearest.dist_sq = dist_px_sq;

  eSnapMode elem = SCE_SNAP_MODE_VERTEX;

  float tobmat[4][4], clip_planes_local[MAX_CLIPPLANE_LEN][4];
  transpose_m4_m4(tobmat, obmat);

  for (int i = sctx->runtime.clip_plane_len; i--;) {
    mul_v4_m4v4(clip_planes_local[i], tobmat, sctx->runtime.clip_plane[i]);
  }

  if (sod->bvhtree[0] && (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX)) {
    BM_mesh_elem_table_ensure(em->bm, BM_VERT);
    BM_mesh_elem_index_ensure(em->bm, BM_VERT);
    BLI_bvhtree_find_nearest_projected(sod->bvhtree[0],
                                       lpmat,
                                       sctx->runtime.win_size,
                                       sctx->runtime.mval,
                                       clip_planes_local,
                                       sctx->runtime.clip_plane_len,
                                       &nearest,
                                       cb_snap_vert,
                                       &nearest2d);
  }

  if (sod->bvhtree[1] && (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_EDGE)) {
    int last_index = nearest.index;
    nearest.index = -1;
    BM_mesh_elem_table_ensure(em->bm, BM_EDGE | BM_VERT);
    BM_mesh_elem_index_ensure(em->bm, BM_EDGE | BM_VERT);
    BLI_bvhtree_find_nearest_projected(sod->bvhtree[1],
                                       lpmat,
                                       sctx->runtime.win_size,
                                       sctx->runtime.mval,
                                       clip_planes_local,
                                       sctx->runtime.clip_plane_len,
                                       &nearest,
                                       cb_snap_edge,
                                       &nearest2d);

    if (nearest.index != -1) {
      elem = SCE_SNAP_MODE_EDGE;
    }
    else {
      nearest.index = last_index;
    }
  }

  if (nearest.index != -1) {
    *dist_px = sqrtf(nearest.dist_sq);

    copy_v3_v3(r_loc, nearest.co);
    mul_m4_v3(obmat, r_loc);
    if (r_no) {
      float imat[4][4];
      invert_m4_m4(imat, obmat);

      copy_v3_v3(r_no, nearest.no);
      mul_transposed_mat3_m4_v3(imat, r_no);
      normalize_v3(r_no);
    }
    if (r_index) {
      *r_index = nearest.index;
    }

    return elem;
  }

  return SCE_SNAP_MODE_NONE;
}

struct SnapObjUserData {
  /* read/write args */
  float *dist_px;
};

/**
 * \note Duplicate args here are documented at #snapObjectsRay
 */
static eSnapMode snap_obj_fn(SnapObjectContext *sctx,
                             const SnapObjectParams *params,
                             Object *ob_eval,
                             ID *ob_data,
                             const float obmat[4][4],
                             bool is_object_active,
                             bool use_hide,
                             void *data)
{
  SnapObjUserData *dt = static_cast<SnapObjUserData *>(data);
  eSnapMode retval = SCE_SNAP_MODE_NONE;
  bool is_edit = false;

  if (ob_data == nullptr && (ob_eval->type == OB_MESH)) {
    BMEditMesh *em = BKE_editmesh_from_object(ob_eval);
    if (UNLIKELY(!em)) { /* See #data_for_snap doc-string. */
      return SCE_SNAP_MODE_NONE;
    }
    retval = snapEditMesh(sctx,
                          params,
                          ob_eval,
                          em,
                          obmat,
                          dt->dist_px,
                          sctx->ret.loc,
                          sctx->ret.no,
                          &sctx->ret.index);
    if (retval) {
      is_edit = true;
    }
  }
  else if (ob_data == nullptr) {
    retval = snap_object_center(
        sctx, ob_eval, obmat, dt->dist_px, sctx->ret.loc, sctx->ret.no, &sctx->ret.index);
  }
  else {
    switch (ob_eval->type) {
      case OB_MESH: {
        if (ob_eval->dt == OB_BOUNDBOX) {
          /* Do not snap to objects that are in bounding box display mode */
          return SCE_SNAP_MODE_NONE;
        }
        if (GS(ob_data->name) == ID_ME) {
          retval = snapMesh(sctx,
                            params,
                            ob_eval,
                            (const Mesh *)ob_data,
                            obmat,
                            use_hide,
                            dt->dist_px,
                            sctx->ret.loc,
                            sctx->ret.no,
                            &sctx->ret.index);
        }
        break;
      }
      case OB_ARMATURE:
        retval = snapArmature(sctx,
                              params,
                              ob_eval,
                              obmat,
                              is_object_active,
                              dt->dist_px,
                              sctx->ret.loc,
                              sctx->ret.no,
                              &sctx->ret.index);
        break;
      case OB_CURVES_LEGACY:
      case OB_SURF:
        if (ob_eval->type == OB_CURVES_LEGACY || BKE_object_is_in_editmode(ob_eval)) {
          retval = snapCurve(sctx,
                             params,
                             ob_eval,
                             obmat,
                             dt->dist_px,
                             sctx->ret.loc,
                             sctx->ret.no,
                             &sctx->ret.index);
          if (params->edit_mode_type != SNAP_GEOM_FINAL) {
            break;
          }
        }
        ATTR_FALLTHROUGH;
      case OB_FONT: {
        const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
        if (mesh_eval) {
          retval |= snapMesh(sctx,
                             params,
                             ob_eval,
                             mesh_eval,
                             obmat,
                             false,
                             dt->dist_px,
                             sctx->ret.loc,
                             sctx->ret.no,
                             &sctx->ret.index);
        }
        break;
      }
      case OB_EMPTY:
      case OB_GPENCIL_LEGACY:
      case OB_LAMP:
        retval = snap_object_center(
            sctx, ob_eval, obmat, dt->dist_px, sctx->ret.loc, sctx->ret.no, &sctx->ret.index);
        break;
      case OB_CAMERA:
        retval = snapCamera(
            sctx, ob_eval, obmat, dt->dist_px, sctx->ret.loc, sctx->ret.no, &sctx->ret.index);
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
static eSnapMode snapObjectsRay(SnapObjectContext *sctx,
                                const SnapObjectParams *params,
                                /* read/write args */
                                /* Parameters below cannot be const, because they are assigned to a
                                 * non-const variable (readability-non-const-parameter). */
                                float *dist_px /* NOLINT */)
{
  SnapObjUserData data = {};
  data.dist_px = dist_px;

  return iter_snap_objects(sctx, params, snap_obj_fn, &data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Object Snapping API
 * \{ */

SnapObjectContext *ED_transform_snap_object_context_create(Scene *scene, int flag)
{
  SnapObjectContext *sctx = MEM_new<SnapObjectContext>(__func__);

  sctx->flag = flag;

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
  sctx->runtime.depsgraph = depsgraph;
  sctx->runtime.v3d = v3d;

  zero_v3(sctx->ret.loc);
  zero_v3(sctx->ret.no);
  sctx->ret.index = -1;
  zero_m4(sctx->ret.obmat);
  sctx->ret.hit_list = nullptr;
  sctx->ret.ob = nullptr;
  sctx->ret.data = nullptr;
  sctx->ret.dist_sq = FLT_MAX;
  sctx->ret.is_edit = false;

  if (raycastObjects(sctx,
                     params,
                     ray_start,
                     ray_normal,
                     params->use_occlusion_test,
                     params->use_occlusion_test,
                     ray_depth))
  {
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
  sctx->runtime.depsgraph = depsgraph;
  sctx->runtime.v3d = v3d;

  zero_v3(sctx->ret.loc);
  zero_v3(sctx->ret.no);
  sctx->ret.index = -1;
  zero_m4(sctx->ret.obmat);
  sctx->ret.hit_list = r_hit_list;
  sctx->ret.ob = nullptr;
  sctx->ret.data = nullptr;
  sctx->ret.dist_sq = FLT_MAX;
  sctx->ret.is_edit = false;

  if (ray_depth == -1.0f) {
    ray_depth = BVH_RAYCAST_DIST_MAX;
  }

#ifdef DEBUG
  float ray_depth_prev = ray_depth;
#endif

  if (raycastObjects(sctx,
                     params,
                     ray_start,
                     ray_normal,
                     params->use_occlusion_test,
                     params->use_occlusion_test,
                     &ray_depth))
  {
    if (sort) {
      BLI_listbase_sort(r_hit_list, hit_depth_cmp);
    }
    /* meant to be readonly for 'all' hits, ensure it is */
#ifdef DEBUG
    BLI_assert(ray_depth_prev == ray_depth);
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
static bool transform_snap_context_project_ray_impl(SnapObjectContext *sctx,
                                                    Depsgraph *depsgraph,
                                                    const View3D *v3d,
                                                    const SnapObjectParams *params,
                                                    const float ray_start[3],
                                                    const float ray_normal[3],
                                                    float *ray_depth,
                                                    float r_co[3],
                                                    float r_no[3])
{
  bool ret;

  /* try snap edge, then face if it fails */
  ret = ED_transform_snap_object_project_ray_ex(sctx,
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

  return ret;
}

bool ED_transform_snap_object_project_ray(SnapObjectContext *sctx,
                                          Depsgraph *depsgraph,
                                          const View3D *v3d,
                                          const SnapObjectParams *params,
                                          const float ray_origin[3],
                                          const float ray_direction[3],
                                          float *ray_depth,
                                          float r_co[3],
                                          float r_no[3])
{
  float ray_depth_fallback;
  if (ray_depth == nullptr) {
    ray_depth_fallback = BVH_RAYCAST_DIST_MAX;
    ray_depth = &ray_depth_fallback;
  }

  return transform_snap_context_project_ray_impl(
      sctx, depsgraph, v3d, params, ray_origin, ray_direction, ray_depth, r_co, r_no);
}

static eSnapMode transform_snap_context_project_view3d_mixed_impl(SnapObjectContext *sctx,
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
  sctx->runtime.depsgraph = depsgraph;
  sctx->runtime.region = region;
  sctx->runtime.v3d = v3d;

  zero_v3(sctx->ret.loc);
  zero_v3(sctx->ret.no);
  sctx->ret.index = -1;
  zero_m4(sctx->ret.obmat);
  sctx->ret.hit_list = nullptr;
  sctx->ret.ob = nullptr;
  sctx->ret.data = nullptr;
  sctx->ret.dist_sq = FLT_MAX;
  sctx->ret.is_edit = false;

  BLI_assert(snap_to_flag & (SCE_SNAP_MODE_GEOM | SCE_SNAP_MODE_FACE_NEAREST));

  eSnapMode retval = SCE_SNAP_MODE_NONE;

  bool has_hit = false;

  const RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  bool use_occlusion_test = params->use_occlusion_test;

  if (use_occlusion_test && XRAY_ENABLED(v3d) && (snap_to_flag & SCE_SNAP_MODE_FACE)) {
    if (snap_to_flag != SCE_SNAP_MODE_FACE) {
      /* In theory everything is visible in X-Ray except faces. */
      snap_to_flag &= ~SCE_SNAP_MODE_FACE;
      use_occlusion_test = false;
    }
  }

  if (prev_co == nullptr) {
    snap_to_flag &= ~(SCE_SNAP_MODE_EDGE_PERPENDICULAR | SCE_SNAP_MODE_FACE_NEAREST);
  }
  else if (init_co == nullptr) {
    snap_to_flag &= ~SCE_SNAP_MODE_FACE_NEAREST;
  }

  /* NOTE: if both face ray-cast and face nearest are enabled, first find result of nearest, then
   * override with ray-cast. */
  if ((snap_to_flag & SCE_SNAP_MODE_FACE_NEAREST) && !has_hit) {
    has_hit = nearestWorldObjects(sctx, params, init_co, prev_co);

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

  if ((snap_to_flag & SCE_SNAP_MODE_FACE) || use_occlusion_test) {
    float ray_start[3], ray_normal[3];
    if (!ED_view3d_win_to_ray_clipped_ex(
            depsgraph, region, v3d, mval, nullptr, ray_normal, ray_start, true))
    {
      return retval;
    }

    float dummy_ray_depth = BVH_RAYCAST_DIST_MAX;

    has_hit = raycastObjects(sctx,
                             params,
                             ray_start,
                             ray_normal,
                             use_occlusion_test,
                             use_occlusion_test && (snap_to_flag & SCE_SNAP_MODE_FACE) == 0,
                             &dummy_ray_depth);

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
    float dist_px_tmp = *dist_px;

    copy_m4_m4(sctx->runtime.pmat, rv3d->persmat);
    sctx->runtime.win_size[0] = region->winx;
    sctx->runtime.win_size[1] = region->winy;
    copy_v2_v2(sctx->runtime.mval, mval);
    sctx->runtime.view_proj = rv3d->is_persp ? VIEW_PROJ_PERSP : VIEW_PROJ_ORTHO;

    /* First snap to edge instead of middle or perpendicular. */
    sctx->runtime.snap_to_flag = static_cast<eSnapMode>(
        snap_to_flag & (SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE));
    if (snap_to_flag & (SCE_SNAP_MODE_EDGE_MIDPOINT | SCE_SNAP_MODE_EDGE_PERPENDICULAR)) {
      sctx->runtime.snap_to_flag = static_cast<eSnapMode>(sctx->runtime.snap_to_flag |
                                                          SCE_SNAP_MODE_EDGE);
    }

    planes_from_projmat(sctx->runtime.pmat,
                        nullptr,
                        nullptr,
                        nullptr,
                        nullptr,
                        sctx->runtime.clip_plane[0],
                        sctx->runtime.clip_plane[1]);

    sctx->runtime.clip_plane_len = 2;
    sctx->runtime.has_occlusion_plane = false;

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
      elem_test = snap_mesh_polygon(sctx, params, &dist_px_tmp);
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

    elem_test = snapObjectsRay(sctx, params, &dist_px_tmp);
    if (elem_test) {
      elem = elem_test;
    }

    if ((elem == SCE_SNAP_MODE_EDGE) &&
        (snap_to_flag &
         (SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE_MIDPOINT | SCE_SNAP_MODE_EDGE_PERPENDICULAR)))
    {
      sctx->runtime.snap_to_flag = snap_to_flag;
      elem = snap_mesh_edge_verts_mixed(sctx, params, *dist_px, prev_co, &dist_px_tmp);
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

      *dist_px = dist_px_tmp;
    }
  }

  return retval;
}

eSnapMode ED_transform_snap_object_project_view3d_ex(SnapObjectContext *sctx,
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
                                                     float r_no[3],
                                                     int *r_index,
                                                     Object **r_ob,
                                                     float r_obmat[4][4],
                                                     float r_face_nor[3])
{
  return transform_snap_context_project_view3d_mixed_impl(sctx,
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
                                                          r_index,
                                                          r_ob,
                                                          r_obmat,
                                                          r_face_nor);
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
