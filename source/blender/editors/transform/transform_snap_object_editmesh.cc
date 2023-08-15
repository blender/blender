/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"

#include "BKE_bvhutils.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_mesh.hh"
#include "BKE_object.h"

#include "DEG_depsgraph_query.h"

#include "ED_transform_snap_object_context.hh"
#include "ED_view3d.hh"

#include "transform_snap_object.hh"

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Snap Object Data
 * \{ */

struct SnapCache_EditMesh : public SnapObjectContext::SnapCache {
  /* Loose Verts, Edges, Triangles. */
  BVHTree *bvhtree[3];
  bool cached[3];

  BMEditMesh *em;

  /** Default callbacks to BVH nearest and ray-cast used only for triangles. */
  BVHTree_NearestPointCallback nearest_callback;
  BVHTree_RayCastCallback raycast_callback;

  bke::MeshRuntime *mesh_runtime;
  float min[3], max[3];

  void clear()
  {
    for (int i = 0; i < ARRAY_SIZE(this->bvhtree); i++) {
      if (!this->cached[i]) {
        BLI_bvhtree_free(this->bvhtree[i]);
      }
      this->bvhtree[i] = nullptr;
    }
  }

  ~SnapCache_EditMesh()
  {
    this->clear();
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("SnapData_EditMesh")
#endif
};

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

static SnapCache_EditMesh *snap_object_data_editmesh_get(SnapObjectContext *sctx,
                                                         Object *ob_eval,
                                                         BMEditMesh *em,
                                                         const bool create)
{
  SnapCache_EditMesh *em_cache = nullptr;
  bool init = false;

  if (std::unique_ptr<SnapObjectContext::SnapCache> *em_cache_p = sctx->editmesh_caches.lookup_ptr(
          em))
  {
    em_cache = static_cast<SnapCache_EditMesh *>(em_cache_p->get());
    bool is_dirty = false;
    /* Check if the geometry has changed. */
    if (em_cache->em != em) {
      is_dirty = true;
    }
    else if (em_cache->mesh_runtime) {
      if (em_cache->mesh_runtime != snap_object_data_editmesh_runtime_get(ob_eval)) {
        if (G.moving) {
          /* WORKAROUND: avoid updating while transforming. */
          BLI_assert(!em_cache->cached[0] && !em_cache->cached[1] && !em_cache->cached[2]);
          em_cache->mesh_runtime = snap_object_data_editmesh_runtime_get(ob_eval);
        }
        else {
          is_dirty = true;
        }
      }
      else if (em_cache->bvhtree[0] && em_cache->cached[0] &&
               !bvhcache_has_tree(em_cache->mesh_runtime->bvh_cache, em_cache->bvhtree[0]))
      {
        /* The tree is owned by the EditMesh and may have been freed since we last used! */
        is_dirty = true;
      }
      else if (em_cache->bvhtree[1] && em_cache->cached[1] &&
               !bvhcache_has_tree(em_cache->mesh_runtime->bvh_cache, em_cache->bvhtree[1]))
      {
        /* The tree is owned by the EditMesh and may have been freed since we last used! */
        is_dirty = true;
      }
      else if (em_cache->bvhtree[2] && em_cache->cached[2] &&
               !bvhcache_has_tree(em_cache->mesh_runtime->bvh_cache, em_cache->bvhtree[2]))
      {
        /* The tree is owned by the EditMesh and may have been freed since we last used! */
        is_dirty = true;
      }
    }

    if (is_dirty) {
      em_cache->clear();
      init = true;
    }
  }
  else if (create) {
    std::unique_ptr<SnapCache_EditMesh> em_cache_ptr = std::make_unique<SnapCache_EditMesh>();
    em_cache = em_cache_ptr.get();
    sctx->editmesh_caches.add_new(em, std::move(em_cache_ptr));
    init = true;
  }

  if (init) {
    /* Operators only update the editmesh looptris of the original mesh. */
    BLI_assert(em == BKE_editmesh_from_object(DEG_get_original_object(ob_eval)));

    em_cache->em = em;
    em_cache->mesh_runtime = snap_object_data_editmesh_runtime_get(ob_eval);
    snap_editmesh_minmax(sctx, em->bm, em_cache->min, em_cache->max);
  }

  return em_cache;
}

static void snap_cache_tri_ensure(SnapCache_EditMesh *em_cache, SnapObjectContext *sctx)
{
  if (em_cache->bvhtree[2] == nullptr) {
    BVHTreeFromEditMesh treedata{};
    BMEditMesh *em = em_cache->em;

    if (sctx->callbacks.edit_mesh.test_face_fn) {
      BMesh *bm = em->bm;
      BLI_assert(poly_to_tri_count(bm->totface, bm->totloop) == em->tottri);

      blender::BitVector<> elem_mask(em->tottri);
      int looptri_num_active = BM_iter_mesh_bitmap_from_filter_tessface(
          bm,
          elem_mask,
          sctx->callbacks.edit_mesh.test_face_fn,
          sctx->callbacks.edit_mesh.user_data);

      bvhtree_from_editmesh_looptri_ex(&treedata, em, elem_mask, looptri_num_active, 0.0f, 4, 6);
    }
    else {
      /* Only cache if BVH-tree is created without a mask.
       * This helps keep a standardized BVH-tree in cache. */
      BKE_bvhtree_from_editmesh_get(&treedata,
                                    em,
                                    4,
                                    BVHTREE_FROM_EM_LOOPTRI,
                                    /* WORKAROUND: avoid updating while transforming. */
                                    G.moving ? nullptr : &em_cache->mesh_runtime->bvh_cache,
                                    &em_cache->mesh_runtime->eval_mutex);
    }
    em_cache->bvhtree[2] = treedata.tree;
    em_cache->cached[2] = treedata.cached;
    em_cache->nearest_callback = treedata.nearest_callback;
    em_cache->raycast_callback = treedata.raycast_callback;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Object Data
 * \{ */

static eSnapMode editmesh_snap_mode_supported(BMesh *bm)
{
  eSnapMode snap_mode_supported = SCE_SNAP_TO_NONE;
  if (bm->totface) {
    snap_mode_supported |= SCE_SNAP_TO_FACE | SCE_SNAP_INDIVIDUAL_NEAREST | SNAP_TO_EDGE_ELEMENTS |
                           SCE_SNAP_TO_POINT;
  }
  else if (bm->totedge) {
    snap_mode_supported |= SNAP_TO_EDGE_ELEMENTS | SCE_SNAP_TO_POINT;
  }
  else if (bm->totvert) {
    snap_mode_supported |= SCE_SNAP_TO_POINT;
  }
  return snap_mode_supported;
}

static SnapCache_EditMesh *editmesh_snapdata_init(SnapObjectContext *sctx,
                                                  Object *ob_eval,
                                                  eSnapMode snap_to_flag)
{
  BMEditMesh *em = BKE_editmesh_from_object(ob_eval);
  if (em == nullptr) {
    return nullptr;
  }

  SnapCache_EditMesh *em_cache = snap_object_data_editmesh_get(sctx, ob_eval, em, false);
  if (em_cache != nullptr) {
    return em_cache;
  }

  eSnapMode snap_mode_used = snap_to_flag & editmesh_snap_mode_supported(em->bm);
  if (snap_mode_used == SCE_SNAP_TO_NONE) {
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
  BMEditMesh *em = static_cast<BMEditMesh *>(userdata);
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

static bool raycastEditMesh(SnapCache_EditMesh *em_cache,
                            SnapObjectContext *sctx,
                            BMEditMesh *em,
                            const float4x4 &obmat,
                            const uint ob_index)
{
  bool retval = false;

  float4x4 imat = math::invert(obmat);
  float3 ray_start_local = math::transform_point(imat, sctx->runtime.ray_start);
  float3 ray_normal_local = math::transform_direction(imat, sctx->runtime.ray_dir);
  float local_scale, local_depth, len_diff = 0.0f;

  /* local scale in normal direction */
  ray_normal_local = math::normalize_and_get_length(ray_normal_local, local_scale);

  local_depth = sctx->ret.ray_depth_max;
  if (local_depth != BVH_RAYCAST_DIST_MAX) {
    local_depth *= local_scale;
  }

  /* Test BoundBox */

  /* was BKE_boundbox_ray_hit_check, see: cf6ca226fa58 */
  if (!isect_ray_aabb_v3_simple(
          ray_start_local, ray_normal_local, em_cache->min, em_cache->max, &len_diff, nullptr))
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

  snap_cache_tri_ensure(em_cache, sctx);
  if (em_cache->bvhtree[2] == nullptr) {
    return retval;
  }

  if (sctx->ret.hit_list) {
    RayCastAll_Data data;

    data.bvhdata = em;
    data.raycast_callback = em_cache->raycast_callback;
    data.obmat = &obmat;
    data.len_diff = len_diff;
    data.local_scale = local_scale;
    data.ob_uuid = ob_index;
    data.hit_list = sctx->ret.hit_list;

    void *hit_last_prev = data.hit_list->last;
    BLI_bvhtree_ray_cast_all(em_cache->bvhtree[2],
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

    if (BLI_bvhtree_ray_cast(em_cache->bvhtree[2],
                             ray_start_local,
                             ray_normal_local,
                             0.0f,
                             &hit,
                             sctx->runtime.params.use_backface_culling ?
                                 editmesh_looptri_raycast_backface_culling_cb :
                                 em_cache->raycast_callback,
                             em) != -1)
    {
      hit.dist += len_diff;
      hit.dist /= local_scale;
      if (hit.dist <= sctx->ret.ray_depth_max) {
        sctx->ret.loc = math::transform_point(obmat, float3(hit.co));
        sctx->ret.no = math::normalize(math::transform_direction(obmat, float3(hit.no)));

        sctx->ret.ray_depth_max = hit.dist;

        sctx->ret.index = BM_elem_index_get(em->looptris[hit.index][0]->f);

        retval = true;
      }
    }
  }

  return retval;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface Snap Functions
 * \{ */

static bool nearest_world_editmesh(SnapCache_EditMesh *em_cache,
                                   SnapObjectContext *sctx,
                                   Object *ob_eval,
                                   BMEditMesh *em,
                                   const float4x4 &obmat)
{
  snap_cache_tri_ensure(em_cache, sctx);
  if (em_cache->bvhtree[2] == nullptr) {
    return false;
  }

  float4x4 imat = math::invert(obmat);
  float3 init_co = math::transform_point(imat, float3(sctx->runtime.init_co));
  float3 curr_co = math::transform_point(imat, float3(sctx->runtime.curr_co));

  BVHTreeNearest nearest{};
  nearest.dist_sq = sctx->ret.dist_px_sq;
  if (nearest_world_tree(
          sctx, em_cache->bvhtree[2], em_cache->nearest_callback, init_co, curr_co, em, &nearest))
  {
    SnapData::register_result(sctx, ob_eval, nullptr, obmat, &nearest);
    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Subclass for Snapping to Edges or Points of an EditMesh
 * \{ */

class SnapData_EditMesh : public SnapData {
 public:
  BMesh *bm;

  SnapData_EditMesh(SnapObjectContext *sctx, BMesh *bm, const float4x4 &obmat)
      : SnapData(sctx, obmat), bm(bm){};

  void get_vert_co(const int index, const float **r_co)
  {
    BMVert *eve = BM_vert_at_index(this->bm, index);
    *r_co = eve->co;
  }

  void get_edge_verts_index(const int index, int r_v_index[2])
  {
    BMEdge *eed = BM_edge_at_index(this->bm, index);
    r_v_index[0] = BM_elem_index_get(eed->v1);
    r_v_index[1] = BM_elem_index_get(eed->v2);
  }

  void copy_vert_no(const int index, float r_no[3])
  {
    BMVert *eve = BM_vert_at_index(this->bm, index);
    copy_v3_v3(r_no, eve->no);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Object Snapping API
 * \{ */

eSnapMode snap_polygon_editmesh(SnapObjectContext *sctx,
                                Object *ob_eval,
                                const ID * /*id*/,
                                const float4x4 &obmat,
                                eSnapMode snap_to_flag,
                                int face)
{
  eSnapMode elem = SCE_SNAP_TO_NONE;

  BMEditMesh *em = BKE_editmesh_from_object(ob_eval);
  SnapData_EditMesh nearest2d(sctx, em->bm, obmat);
  nearest2d.clip_planes_enable(sctx);

  BVHTreeNearest nearest{};
  nearest.index = -1;
  nearest.dist_sq = sctx->ret.dist_px_sq;

  BM_mesh_elem_table_ensure(em->bm, BM_FACE);
  BMFace *f = BM_face_at_index(em->bm, face);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  if (snap_to_flag & SCE_SNAP_TO_EDGE) {
    elem = SCE_SNAP_TO_EDGE;
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
    elem = SCE_SNAP_TO_EDGE_ENDPOINT;
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
    nearest2d.nearest_point = nearest;
    nearest2d.register_result(sctx, ob_eval, nullptr);
    return elem;
  }

  return SCE_SNAP_TO_NONE;
}

eSnapMode snap_edge_points_editmesh(SnapObjectContext *sctx,
                                    Object *ob_eval,
                                    const ID * /*id*/,
                                    const float4x4 &obmat,
                                    float dist_pex_sq_orig,
                                    int edge)
{
  BMEditMesh *em = BKE_editmesh_from_object(ob_eval);
  SnapData_EditMesh nearest2d(sctx, em->bm, obmat);
  eSnapMode elem = nearest2d.snap_edge_points_impl(sctx, edge, dist_pex_sq_orig);
  if (nearest2d.nearest_point.index != -2) {
    nearest2d.register_result(sctx, ob_eval, nullptr);
  }
  return elem;
}

static eSnapMode snapEditMesh(SnapCache_EditMesh *em_cache,
                              SnapObjectContext *sctx,
                              Object *ob_eval,
                              BMEditMesh *em,
                              const float4x4 &obmat,
                              eSnapMode snap_to_flag)
{
  BLI_assert(snap_to_flag != SCE_SNAP_TO_FACE);

  SnapData_EditMesh nearest2d(sctx, em->bm, obmat);

  /* Was BKE_boundbox_ray_hit_check, see: cf6ca226fa58. */
  if (!nearest2d.snap_boundbox(em_cache->min, em_cache->max)) {
    return SCE_SNAP_TO_NONE;
  }

  if (snap_to_flag & SCE_SNAP_TO_POINT) {
    BVHTreeFromEditMesh treedata{};
    treedata.tree = em_cache->bvhtree[0];

    if (treedata.tree == nullptr) {
      if (sctx->callbacks.edit_mesh.test_vert_fn) {
        auto test_looseverts_fn = [](BMElem *elem, void *user_data) {
          SnapObjectContext *sctx_ = static_cast<SnapObjectContext *>(user_data);
          BMVert *v = reinterpret_cast<BMVert *>(elem);
          if (v->e) {
            return false;
          }
          return sctx_->callbacks.edit_mesh.test_vert_fn(v, sctx_->callbacks.edit_mesh.user_data);
        };
        blender::BitVector<> verts_mask(em->bm->totvert);
        const int verts_num_active = BM_iter_mesh_bitmap_from_filter(
            BM_VERTS_OF_MESH, em->bm, verts_mask, test_looseverts_fn, sctx);

        bvhtree_from_editmesh_verts_ex(&treedata, em, verts_mask, verts_num_active, 0.0f, 2, 6);
      }
      else {
        BKE_bvhtree_from_editmesh_get(&treedata,
                                      em,
                                      2,
                                      BVHTREE_FROM_EM_LOOSEVERTS,
                                      /* WORKAROUND: avoid updating while transforming. */
                                      G.moving ? nullptr : &em_cache->mesh_runtime->bvh_cache,
                                      &em_cache->mesh_runtime->eval_mutex);
      }
      em_cache->bvhtree[0] = treedata.tree;
      em_cache->cached[0] = treedata.cached;
    }
  }

  if (snap_to_flag & SNAP_TO_EDGE_ELEMENTS) {
    BVHTreeFromEditMesh treedata{};
    treedata.tree = em_cache->bvhtree[1];

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
                                      G.moving ? nullptr : &em_cache->mesh_runtime->bvh_cache,
                                      &em_cache->mesh_runtime->eval_mutex);
      }
      em_cache->bvhtree[1] = treedata.tree;
      em_cache->cached[1] = treedata.cached;
    }
  }

  /* #XRAY_ENABLED can return false even with the XRAY flag enabled, this happens because the
   * alpha is 1.0 in this case. But even with the alpha being 1.0, the edit mesh is still not
   * occluded. */
  const bool skip_occlusion_plane = XRAY_FLAG_ENABLED(sctx->runtime.v3d);
  nearest2d.clip_planes_enable(sctx, skip_occlusion_plane);

  BVHTreeNearest nearest{};
  nearest.index = -1;
  nearest.dist_sq = sctx->ret.dist_px_sq;

  eSnapMode elem = SCE_SNAP_TO_POINT;

  if (em_cache->bvhtree[0] && (snap_to_flag & SCE_SNAP_TO_POINT)) {
    BM_mesh_elem_table_ensure(em->bm, BM_VERT);
    BM_mesh_elem_index_ensure(em->bm, BM_VERT);
    BLI_bvhtree_find_nearest_projected(em_cache->bvhtree[0],
                                       nearest2d.pmat_local.ptr(),
                                       sctx->runtime.win_size,
                                       sctx->runtime.mval,
                                       reinterpret_cast<float(*)[4]>(nearest2d.clip_planes.data()),
                                       nearest2d.clip_planes.size(),
                                       &nearest,
                                       cb_snap_vert,
                                       &nearest2d);
  }

  if (em_cache->bvhtree[1] && (snap_to_flag & SNAP_TO_EDGE_ELEMENTS)) {
    int last_index = nearest.index;
    nearest.index = -1;
    BM_mesh_elem_table_ensure(em->bm, BM_EDGE | BM_VERT);
    BM_mesh_elem_index_ensure(em->bm, BM_EDGE | BM_VERT);
    BLI_bvhtree_find_nearest_projected(em_cache->bvhtree[1],
                                       nearest2d.pmat_local.ptr(),
                                       sctx->runtime.win_size,
                                       sctx->runtime.mval,
                                       reinterpret_cast<float(*)[4]>(nearest2d.clip_planes.data()),
                                       nearest2d.clip_planes.size(),
                                       &nearest,
                                       cb_snap_edge,
                                       &nearest2d);

    if (nearest.index != -1) {
      elem = SCE_SNAP_TO_EDGE;
    }
    else {
      nearest.index = last_index;
    }
  }

  if (nearest.index != -1) {
    nearest2d.nearest_point = nearest;
    nearest2d.register_result(sctx, ob_eval, nullptr);
    return elem;
  }

  return SCE_SNAP_TO_NONE;
}

/** \} */

eSnapMode snap_object_editmesh(SnapObjectContext *sctx,
                               Object *ob_eval,
                               const ID * /*id*/,
                               const float4x4 &obmat,
                               eSnapMode snap_to_flag,
                               bool /*use_hide*/)
{
  eSnapMode elem = SCE_SNAP_TO_NONE;

  SnapCache_EditMesh *em_cache = editmesh_snapdata_init(sctx, ob_eval, snap_to_flag);
  if (em_cache == nullptr) {
    return elem;
  }

  BMEditMesh *em = em_cache->em;
  eSnapMode snap_mode_used = snap_to_flag & editmesh_snap_mode_supported(em->bm);
  if (snap_mode_used & (SNAP_TO_EDGE_ELEMENTS | SCE_SNAP_TO_POINT)) {
    elem = snapEditMesh(em_cache, sctx, ob_eval, em, obmat, snap_mode_used);
    if (elem) {
      return elem;
    }
  }

  if (snap_mode_used & SCE_SNAP_TO_FACE) {
    if (raycastEditMesh(em_cache, sctx, em, obmat, sctx->runtime.object_index++)) {
      return SCE_SNAP_TO_FACE;
    }
  }

  if (snap_mode_used & SCE_SNAP_INDIVIDUAL_NEAREST) {
    if (nearest_world_editmesh(em_cache, sctx, ob_eval, em, obmat)) {
      return SCE_SNAP_INDIVIDUAL_NEAREST;
    }
  }

  return SCE_SNAP_TO_NONE;
}
