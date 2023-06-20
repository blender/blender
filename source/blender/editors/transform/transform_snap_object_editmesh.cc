/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_math.h"
#include "BLI_math_matrix_types.hh"

#include "BKE_bvhutils.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_mesh.hh"
#include "BKE_object.h"

#include "DEG_depsgraph_query.h"

#include "ED_transform_snap_object_context.h"

#include "transform_snap_object.hh"

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Snap Object Data
 * \{ */

void SnapData_EditMesh::clear()
{
  for (int i = 0; i < ARRAY_SIZE(this->bvhtree); i++) {
    if (!this->cached[i]) {
      BLI_bvhtree_free(this->bvhtree[i]);
    }
    this->bvhtree[i] = nullptr;
  }
  free_bvhtree_from_editmesh(&this->treedata_editmesh);
}

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
                                                        BMEditMesh *em,
                                                        const bool create)
{
  SnapData_EditMesh *sod = nullptr;
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
  else if (create) {
    std::unique_ptr<SnapData_EditMesh> sod_ptr = std::make_unique<SnapData_EditMesh>();
    sod = sod_ptr.get();
    sctx->editmesh_caches.add_new(em, std::move(sod_ptr));
    init = true;
  }

  if (init) {
    /* Operators only update the editmesh looptris of the original mesh. */
    BLI_assert(em == BKE_editmesh_from_object(DEG_get_original_object(ob_eval)));

    sod->treedata_editmesh.em = em;
    sod->mesh_runtime = snap_object_data_editmesh_runtime_get(ob_eval);
    snap_editmesh_minmax(sctx, em->bm, sod->min, sod->max);
  }

  return sod;
}

static BVHTreeFromEditMesh *snap_object_data_editmesh_treedata_get(SnapData_EditMesh *sod,
                                                                   SnapObjectContext *sctx,
                                                                   BMEditMesh *em)
{
  BVHTreeFromEditMesh *treedata = &sod->treedata_editmesh;

  if (treedata->tree == nullptr) {
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
/** \name Snap Object Data
 * \{ */

static eSnapMode editmesh_snap_mode_supported(BMEditMesh *em)
{
  eSnapMode snap_mode_supported = SCE_SNAP_MODE_NONE;
  if (em->bm->totface) {
    snap_mode_supported |= SCE_SNAP_MODE_FACE | SCE_SNAP_MODE_FACE_NEAREST;
  }
  if (em->bm->totedge) {
    snap_mode_supported |= SCE_SNAP_MODE_EDGE | SCE_SNAP_MODE_EDGE_MIDPOINT |
                           SCE_SNAP_MODE_EDGE_PERPENDICULAR;
  }
  if (em->bm->totvert) {
    snap_mode_supported |= SCE_SNAP_MODE_VERTEX;
  }
  return snap_mode_supported;
}

static SnapData_EditMesh *editmesh_snapdata_init(SnapObjectContext *sctx,
                                                 Object *ob_eval,
                                                 eSnapMode snap_to_flag)
{
  BMEditMesh *em = BKE_editmesh_from_object(ob_eval);
  if (em == nullptr) {
    return nullptr;
  }

  SnapData_EditMesh *sod = snap_object_data_editmesh_get(sctx, ob_eval, em, false);
  if (sod != nullptr) {
    return sod;
  }

  eSnapMode snap_mode_used = snap_to_flag & editmesh_snap_mode_supported(em);
  if (snap_mode_used == SCE_SNAP_MODE_NONE) {
    return nullptr;
  }

  return snap_object_data_editmesh_get(sctx, ob_eval, em, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ray Cast Functions
 * \{ */

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

static bool raycastEditMesh(SnapData_EditMesh *sod,
                            SnapObjectContext *sctx,
                            BMEditMesh *em,
                            const float obmat[4][4],
                            const uint ob_index)
{
  bool retval = false;

  float imat[4][4];
  float ray_start_local[3], ray_normal_local[3];
  float local_scale, local_depth, len_diff = 0.0f;

  invert_m4_m4(imat, obmat);

  copy_v3_v3(ray_start_local, sctx->runtime.ray_start);
  copy_v3_v3(ray_normal_local, sctx->runtime.ray_dir);

  mul_m4_v3(imat, ray_start_local);
  mul_mat3_m4_v3(imat, ray_normal_local);

  /* local scale in normal direction */
  local_scale = normalize_v3(ray_normal_local);
  local_depth = sctx->ret.ray_depth_max;
  if (local_depth != BVH_RAYCAST_DIST_MAX) {
    local_depth *= local_scale;
  }

  /* Test BoundBox */

  /* was BKE_boundbox_ray_hit_check, see: cf6ca226fa58 */
  if (!isect_ray_aabb_v3_simple(
          ray_start_local, ray_normal_local, sod->min, sod->max, &len_diff, nullptr))
  {
    return retval;
  }

  /* We pass a temp ray_start, set from object's bounding-box, to avoid precision issues with
   * very far away ray_start values (as returned in case of ortho view3d), see #50486, #38358. */
  if (len_diff > 400.0f) {
    len_diff -= local_scale; /* make temp start point a bit away from bounding-box hit point. */
    madd_v3_v3fl(ray_start_local, ray_normal_local, len_diff);
    local_depth -= len_diff;
  }
  else {
    len_diff = 0.0f;
  }

  BVHTreeFromEditMesh *treedata = snap_object_data_editmesh_treedata_get(sod, sctx, em);
  if (treedata == nullptr) {
    return retval;
  }

  if (sctx->ret.hit_list) {
    RayCastAll_Data data;

    data.bvhdata = treedata;
    data.raycast_callback = treedata->raycast_callback;
    data.obmat = obmat;
    data.len_diff = len_diff;
    data.local_scale = local_scale;
    data.ob_uuid = ob_index;
    data.hit_list = sctx->ret.hit_list;

    void *hit_last_prev = data.hit_list->last;
    BLI_bvhtree_ray_cast_all(treedata->tree,
                             ray_start_local,
                             ray_normal_local,
                             0.0f,
                             sctx->ret.ray_depth_max,
                             raycast_all_cb,
                             &data);

    retval = hit_last_prev != data.hit_list->last;
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
                             sctx->runtime.params.use_backface_culling ?
                                 editmesh_looptri_raycast_backface_culling_cb :
                                 treedata->raycast_callback,
                             treedata) != -1)
    {
      hit.dist += len_diff;
      hit.dist /= local_scale;
      if (hit.dist <= sctx->ret.ray_depth_max) {
        copy_v3_v3(sctx->ret.loc, hit.co);
        copy_v3_v3(sctx->ret.no, hit.no);

        mul_m4_v3(obmat, sctx->ret.loc);

        mul_transposed_mat3_m4_v3(imat, sctx->ret.no);
        normalize_v3(sctx->ret.no);

        sctx->ret.ray_depth_max = hit.dist;

        em = sod->treedata_editmesh.em;
        sctx->ret.index = BM_elem_index_get(em->looptris[hit.index][0]->f);

        retval = true;
      }
    }
  }

  return retval;
}

/* -------------------------------------------------------------------- */
/** \name Surface Snap Functions
 * \{ */

static bool nearest_world_editmesh(SnapData_EditMesh *sod,
                                   SnapObjectContext *sctx,
                                   BMEditMesh *em,
                                   const float (*obmat)[4])
{
  BVHTreeFromEditMesh *treedata = snap_object_data_editmesh_treedata_get(sod, sctx, em);
  if (treedata == nullptr) {
    return false;
  }

  return nearest_world_tree(sctx, treedata->tree, treedata->nearest_callback, treedata, obmat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks
 * \{ */

static void cb_bvert_co_get(const int index, const Nearest2dUserData *data, const float **r_co)
{
  BMVert *eve = BM_vert_at_index(data->bm, index);
  *r_co = eve->co;
}

static void cb_bvert_no_copy(const int index, const Nearest2dUserData *data, float r_no[3])
{
  BMVert *eve = BM_vert_at_index(data->bm, index);

  copy_v3_v3(r_no, eve->no);
}

static void cb_bedge_verts_get(const int index, const Nearest2dUserData *data, int r_v_index[2])
{
  BMEdge *eed = BM_edge_at_index(data->bm, index);

  r_v_index[0] = BM_elem_index_get(eed->v1);
  r_v_index[1] = BM_elem_index_get(eed->v2);
}

void nearest2d_data_init_editmesh(BMEditMesh *em, Nearest2dUserData *r_nearest2d)
{
  r_nearest2d->get_vert_co = cb_bvert_co_get;
  r_nearest2d->get_edge_verts_index = cb_bedge_verts_get;
  r_nearest2d->copy_vert_no = cb_bvert_no_copy;
  r_nearest2d->get_tri_verts_index = nullptr;
  r_nearest2d->get_tri_edges_index = nullptr;

  r_nearest2d->bm = em->bm;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Object Snapping API
 * \{ */

eSnapMode snap_polygon_editmesh(SnapObjectContext *sctx,
                                Object *ob_eval,
                                ID * /*id*/,
                                const float obmat[4][4],
                                eSnapMode snap_to_flag,
                                int polygon)
{
  eSnapMode elem = SCE_SNAP_MODE_NONE;

  SnapData_EditMesh *sod = editmesh_snapdata_init(sctx, ob_eval, snap_to_flag);
  if (sod == nullptr) {
    return elem;
  }

  BMEditMesh *em = sod->treedata_editmesh.em;

  Nearest2dUserData nearest2d(sctx, sctx->ret.dist_px_sq, float4x4(obmat));
  nearest2d.clip_planes_get(sctx, float4x4(obmat));
  nearest2d_data_init_editmesh(em, &nearest2d);

  BVHTreeNearest nearest{};
  nearest.index = -1;
  nearest.dist_sq = sctx->ret.dist_px_sq;

  BM_mesh_elem_table_ensure(em->bm, BM_FACE);
  BMFace *f = BM_face_at_index(em->bm, polygon);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  if (snap_to_flag & SCE_SNAP_MODE_EDGE) {
    elem = SCE_SNAP_MODE_EDGE;
    BM_mesh_elem_index_ensure(em->bm, BM_VERT | BM_EDGE);
    BM_mesh_elem_table_ensure(em->bm, BM_VERT | BM_EDGE);
    do {
      cb_snap_edge(&nearest2d,
                   BM_elem_index_get(l_iter->e),
                   &nearest2d.nearest_precalc,
                   reinterpret_cast<float(*)[4]>(nearest2d.clip_planes.data()),
                   nearest2d.clip_planes.size(),
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
                   &nearest2d.nearest_precalc,
                   reinterpret_cast<float(*)[4]>(nearest2d.clip_planes.data()),
                   nearest2d.clip_planes.size(),
                   &nearest);
    } while ((l_iter = l_iter->next) != l_first);
  }

  if (nearest.index != -1) {
    sctx->ret.dist_px_sq = nearest.dist_sq;

    mul_m4_v3(obmat, nearest.co);
    copy_v3_v3(sctx->ret.loc, nearest.co);

    {
      float imat[4][4];
      invert_m4_m4(imat, obmat);
      mul_transposed_mat3_m4_v3(imat, nearest.no);
      normalize_v3(nearest.no);

      copy_v3_v3(sctx->ret.no, nearest.no);
    }

    sctx->ret.index = nearest.index;
    return elem;
  }

  return SCE_SNAP_MODE_NONE;
}

static eSnapMode snapEditMesh(SnapData_EditMesh *sod,
                              SnapObjectContext *sctx,
                              BMEditMesh *em,
                              const float obmat[4][4],
                              eSnapMode snap_to_flag)
{
  BLI_assert(snap_to_flag != SCE_SNAP_MODE_FACE);

  Nearest2dUserData nearest2d(sctx, sctx->ret.dist_px_sq, float4x4(obmat));

  /* Was BKE_boundbox_ray_hit_check, see: cf6ca226fa58. */
  if (!nearest2d.snap_boundbox(sod->min, sod->max)) {
    return SCE_SNAP_MODE_NONE;
  }

  if (snap_to_flag & SCE_SNAP_MODE_VERTEX) {
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

  if (snap_to_flag & SCE_SNAP_MODE_EDGE) {
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

  nearest2d.clip_planes_get(sctx, float4x4(obmat));
  nearest2d_data_init_editmesh(em, &nearest2d);

  BVHTreeNearest nearest{};
  nearest.index = -1;
  nearest.dist_sq = sctx->ret.dist_px_sq;

  eSnapMode elem = SCE_SNAP_MODE_VERTEX;

  if (sod->bvhtree[0] && (snap_to_flag & SCE_SNAP_MODE_VERTEX)) {
    BM_mesh_elem_table_ensure(em->bm, BM_VERT);
    BM_mesh_elem_index_ensure(em->bm, BM_VERT);
    BLI_bvhtree_find_nearest_projected(sod->bvhtree[0],
                                       nearest2d.pmat_local.ptr(),
                                       sctx->runtime.win_size,
                                       sctx->runtime.mval,
                                       reinterpret_cast<float(*)[4]>(nearest2d.clip_planes.data()),
                                       nearest2d.clip_planes.size(),
                                       &nearest,
                                       cb_snap_vert,
                                       &nearest2d);
  }

  if (sod->bvhtree[1] && (snap_to_flag & SCE_SNAP_MODE_EDGE)) {
    int last_index = nearest.index;
    nearest.index = -1;
    BM_mesh_elem_table_ensure(em->bm, BM_EDGE | BM_VERT);
    BM_mesh_elem_index_ensure(em->bm, BM_EDGE | BM_VERT);
    BLI_bvhtree_find_nearest_projected(sod->bvhtree[1],
                                       nearest2d.pmat_local.ptr(),
                                       sctx->runtime.win_size,
                                       sctx->runtime.mval,
                                       reinterpret_cast<float(*)[4]>(nearest2d.clip_planes.data()),
                                       nearest2d.clip_planes.size(),
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
    copy_v3_v3(sctx->ret.loc, nearest.co);
    copy_v3_v3(sctx->ret.no, nearest.no);

    mul_m4_v3(obmat, sctx->ret.loc);

    float imat[4][4];
    invert_m4_m4(imat, obmat);
    mul_transposed_mat3_m4_v3(imat, sctx->ret.no);
    normalize_v3(sctx->ret.no);

    sctx->ret.dist_px_sq = nearest.dist_sq;
    sctx->ret.index = nearest.index;

    return elem;
  }

  return SCE_SNAP_MODE_NONE;
}

/** \} */

eSnapMode snap_object_editmesh(SnapObjectContext *sctx,
                               Object *ob_eval,
                               ID * /*id*/,
                               const float obmat[4][4],
                               eSnapMode snap_to_flag,
                               bool /*use_hide*/)
{
  eSnapMode elem = SCE_SNAP_MODE_NONE;

  SnapData_EditMesh *sod = editmesh_snapdata_init(sctx, ob_eval, snap_to_flag);
  if (sod == nullptr) {
    return elem;
  }

  BMEditMesh *em = sod->treedata_editmesh.em;

  eSnapMode snap_mode_used = snap_to_flag & editmesh_snap_mode_supported(em);
  if (snap_mode_used & (SCE_SNAP_MODE_EDGE | SCE_SNAP_MODE_EDGE_MIDPOINT |
                        SCE_SNAP_MODE_EDGE_PERPENDICULAR | SCE_SNAP_MODE_VERTEX))
  {
    elem = snapEditMesh(sod, sctx, em, obmat, snap_to_flag);
    if (elem) {
      return elem;
    }
  }

  if (snap_mode_used & SCE_SNAP_MODE_FACE) {
    if (raycastEditMesh(sod, sctx, em, obmat, sctx->runtime.object_index++)) {
      return SCE_SNAP_MODE_FACE;
    }
  }

  if (snap_mode_used & SCE_SNAP_MODE_FACE_NEAREST) {
    if (nearest_world_editmesh(sod, sctx, em, obmat)) {
      return SCE_SNAP_MODE_FACE_NEAREST;
    }
  }

  return SCE_SNAP_MODE_NONE;
}
