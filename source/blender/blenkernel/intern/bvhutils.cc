/* SPDX-FileCopyrightText: Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstdio>
#include <cstring>

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_bit_vector.hh"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_span.hh"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_attribute.hh"
#include "BKE_bvhutils.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.h"
#include "BKE_pointcloud.h"

#include "MEM_guardedalloc.h"

using blender::BitSpan;
using blender::BitVector;
using blender::float3;
using blender::IndexRange;
using blender::Span;
using blender::VArray;

/* -------------------------------------------------------------------- */
/** \name BVHCache
 * \{ */

struct BVHCacheItem {
  bool is_filled;
  BVHTree *tree;
};

struct BVHCache {
  BVHCacheItem items[BVHTREE_MAX_ITEM];
  ThreadMutex mutex;
};

/**
 * Queries a bvhcache for the cache bvhtree of the request type
 *
 * When the `r_locked` is filled and the tree could not be found the caches mutex will be
 * locked. This mutex can be unlocked by calling `bvhcache_unlock`.
 *
 * When `r_locked` is used the `mesh_eval_mutex` must contain the `MeshRuntime.eval_mutex`.
 */
static bool bvhcache_find(BVHCache **bvh_cache_p,
                          BVHCacheType type,
                          BVHTree **r_tree,
                          bool *r_locked,
                          std::mutex *mesh_eval_mutex)
{
  bool do_lock = r_locked;
  if (r_locked) {
    *r_locked = false;
  }
  if (*bvh_cache_p == nullptr) {
    if (!do_lock) {
      /* Cache does not exist and no lock is requested. */
      return false;
    }
    /* Lazy initialization of the bvh_cache using the `mesh_eval_mutex`. */
    std::lock_guard lock{*mesh_eval_mutex};
    if (*bvh_cache_p == nullptr) {
      *bvh_cache_p = bvhcache_init();
    }
  }
  BVHCache *bvh_cache = *bvh_cache_p;

  if (bvh_cache->items[type].is_filled) {
    *r_tree = bvh_cache->items[type].tree;
    return true;
  }
  if (do_lock) {
    BLI_mutex_lock(&bvh_cache->mutex);
    bool in_cache = bvhcache_find(bvh_cache_p, type, r_tree, nullptr, nullptr);
    if (in_cache) {
      BLI_mutex_unlock(&bvh_cache->mutex);
      return in_cache;
    }
    *r_locked = true;
  }
  return false;
}

static void bvhcache_unlock(BVHCache *bvh_cache, bool lock_started)
{
  if (lock_started) {
    BLI_mutex_unlock(&bvh_cache->mutex);
  }
}

bool bvhcache_has_tree(const BVHCache *bvh_cache, const BVHTree *tree)
{
  if (bvh_cache == nullptr) {
    return false;
  }

  for (int i = 0; i < BVHTREE_MAX_ITEM; i++) {
    if (bvh_cache->items[i].tree == tree) {
      return true;
    }
  }
  return false;
}

BVHCache *bvhcache_init()
{
  BVHCache *cache = MEM_cnew<BVHCache>(__func__);
  BLI_mutex_init(&cache->mutex);
  return cache;
}
/**
 * Inserts a BVHTree of the given type under the cache
 * After that the caller no longer needs to worry when to free the BVHTree
 * as that will be done when the cache is freed.
 *
 * A call to this assumes that there was no previous cached tree of the given type
 * \warning The #BVHTree can be nullptr.
 */
static void bvhcache_insert(BVHCache *bvh_cache, BVHTree *tree, BVHCacheType type)
{
  BVHCacheItem *item = &bvh_cache->items[type];
  BLI_assert(!item->is_filled);
  item->tree = tree;
  item->is_filled = true;
}

void bvhcache_free(BVHCache *bvh_cache)
{
  for (int index = 0; index < BVHTREE_MAX_ITEM; index++) {
    BVHCacheItem *item = &bvh_cache->items[index];
    BLI_bvhtree_free(item->tree);
    item->tree = nullptr;
  }
  BLI_mutex_end(&bvh_cache->mutex);
  MEM_freeN(bvh_cache);
}

/**
 * BVH-tree balancing inside a mutex lock must be run in isolation. Balancing
 * is multithreaded, and we do not want the current thread to start another task
 * that may involve acquiring the same mutex lock that it is waiting for.
 */
static void bvhtree_balance_isolated(void *userdata)
{
  BLI_bvhtree_balance((BVHTree *)userdata);
}

static void bvhtree_balance(BVHTree *tree, const bool isolate)
{
  if (tree) {
    if (isolate) {
      BLI_task_isolate(bvhtree_balance_isolated, tree);
    }
    else {
      BLI_bvhtree_balance(tree);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Local Callbacks
 * \{ */

/* Math stuff for ray casting on mesh faces and for nearest surface */

float bvhtree_ray_tri_intersection(const BVHTreeRay *ray,
                                   const float /*m_dist*/,
                                   const float v0[3],
                                   const float v1[3],
                                   const float v2[3])
{
  float dist;

#ifdef USE_KDOPBVH_WATERTIGHT
  if (isect_ray_tri_watertight_v3(ray->origin, ray->isect_precalc, v0, v1, v2, &dist, nullptr))
#else
  if (isect_ray_tri_epsilon_v3(
          ray->origin, ray->direction, v0, v1, v2, &dist, nullptr, FLT_EPSILON))
#endif
  {
    return dist;
  }

  return FLT_MAX;
}

float bvhtree_sphereray_tri_intersection(const BVHTreeRay *ray,
                                         float radius,
                                         const float m_dist,
                                         const float v0[3],
                                         const float v1[3],
                                         const float v2[3])
{

  float idist;
  float p1[3];
  float hit_point[3];

  madd_v3_v3v3fl(p1, ray->origin, ray->direction, m_dist);
  if (isect_sweeping_sphere_tri_v3(ray->origin, p1, radius, v0, v1, v2, &idist, hit_point)) {
    return idist * m_dist;
  }

  return FLT_MAX;
}

/*
 * BVH from meshes callbacks
 */

/**
 * Callback to BVH-tree nearest point.
 * The tree must have been built using #bvhtree_from_mesh_faces.
 *
 * \param userdata: Must be a #BVHMeshCallbackUserdata built from the same mesh as the tree.
 */
static void mesh_faces_nearest_point(void *userdata,
                                     int index,
                                     const float co[3],
                                     BVHTreeNearest *nearest)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const float(*positions)[3] = data->vert_positions;
  const MFace *face = data->face + index;

  const float *t0, *t1, *t2, *t3;
  t0 = positions[face->v1];
  t1 = positions[face->v2];
  t2 = positions[face->v3];
  t3 = face->v4 ? positions[face->v4] : nullptr;

  do {
    float nearest_tmp[3], dist_sq;

    closest_on_tri_to_point_v3(nearest_tmp, co, t0, t1, t2);
    dist_sq = len_squared_v3v3(co, nearest_tmp);

    if (dist_sq < nearest->dist_sq) {
      nearest->index = index;
      nearest->dist_sq = dist_sq;
      copy_v3_v3(nearest->co, nearest_tmp);
      normal_tri_v3(nearest->no, t0, t1, t2);
    }

    t1 = t2;
    t2 = t3;
    t3 = nullptr;

  } while (t2);
}
/* copy of function above */
static void mesh_looptri_nearest_point(void *userdata,
                                       int index,
                                       const float co[3],
                                       BVHTreeNearest *nearest)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const float(*positions)[3] = data->vert_positions;
  const MLoopTri *lt = &data->looptri[index];
  const float *vtri_co[3] = {
      positions[data->corner_verts[lt->tri[0]]],
      positions[data->corner_verts[lt->tri[1]]],
      positions[data->corner_verts[lt->tri[2]]],
  };
  float nearest_tmp[3], dist_sq;

  closest_on_tri_to_point_v3(nearest_tmp, co, UNPACK3(vtri_co));
  dist_sq = len_squared_v3v3(co, nearest_tmp);

  if (dist_sq < nearest->dist_sq) {
    nearest->index = index;
    nearest->dist_sq = dist_sq;
    copy_v3_v3(nearest->co, nearest_tmp);
    normal_tri_v3(nearest->no, UNPACK3(vtri_co));
  }
}
/* copy of function above (warning, should de-duplicate with editmesh_bvh.c) */
static void editmesh_looptri_nearest_point(void *userdata,
                                           int index,
                                           const float co[3],
                                           BVHTreeNearest *nearest)
{
  const BVHTreeFromEditMesh *data = (const BVHTreeFromEditMesh *)userdata;
  BMEditMesh *em = data->em;
  const BMLoop **ltri = (const BMLoop **)em->looptris[index];

  const float *t0, *t1, *t2;
  t0 = ltri[0]->v->co;
  t1 = ltri[1]->v->co;
  t2 = ltri[2]->v->co;

  {
    float nearest_tmp[3], dist_sq;

    closest_on_tri_to_point_v3(nearest_tmp, co, t0, t1, t2);
    dist_sq = len_squared_v3v3(co, nearest_tmp);

    if (dist_sq < nearest->dist_sq) {
      nearest->index = index;
      nearest->dist_sq = dist_sq;
      copy_v3_v3(nearest->co, nearest_tmp);
      normal_tri_v3(nearest->no, t0, t1, t2);
    }
  }
}

/**
 * Callback to BVH-tree ray-cast.
 * The tree must have been built using bvhtree_from_mesh_faces.
 *
 * \param userdata: Must be a #BVHMeshCallbackUserdata built from the same mesh as the tree.
 */
static void mesh_faces_spherecast(void *userdata,
                                  int index,
                                  const BVHTreeRay *ray,
                                  BVHTreeRayHit *hit)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const float(*positions)[3] = data->vert_positions;
  const MFace *face = &data->face[index];

  const float *t0, *t1, *t2, *t3;
  t0 = positions[face->v1];
  t1 = positions[face->v2];
  t2 = positions[face->v3];
  t3 = face->v4 ? positions[face->v4] : nullptr;

  do {
    float dist;
    if (ray->radius == 0.0f) {
      dist = bvhtree_ray_tri_intersection(ray, hit->dist, t0, t1, t2);
    }
    else {
      dist = bvhtree_sphereray_tri_intersection(ray, ray->radius, hit->dist, t0, t1, t2);
    }

    if (dist >= 0 && dist < hit->dist) {
      hit->index = index;
      hit->dist = dist;
      madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

      normal_tri_v3(hit->no, t0, t1, t2);
    }

    t1 = t2;
    t2 = t3;
    t3 = nullptr;

  } while (t2);
}
/* copy of function above */
static void mesh_looptri_spherecast(void *userdata,
                                    int index,
                                    const BVHTreeRay *ray,
                                    BVHTreeRayHit *hit)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const float(*positions)[3] = data->vert_positions;
  const MLoopTri *lt = &data->looptri[index];
  const float *vtri_co[3] = {
      positions[data->corner_verts[lt->tri[0]]],
      positions[data->corner_verts[lt->tri[1]]],
      positions[data->corner_verts[lt->tri[2]]],
  };
  float dist;

  if (ray->radius == 0.0f) {
    dist = bvhtree_ray_tri_intersection(ray, hit->dist, UNPACK3(vtri_co));
  }
  else {
    dist = bvhtree_sphereray_tri_intersection(ray, ray->radius, hit->dist, UNPACK3(vtri_co));
  }

  if (dist >= 0 && dist < hit->dist) {
    hit->index = index;
    hit->dist = dist;
    madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

    normal_tri_v3(hit->no, UNPACK3(vtri_co));
  }
}
/* copy of function above (warning, should de-duplicate with editmesh_bvh.c) */
static void editmesh_looptri_spherecast(void *userdata,
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
    float dist;
    if (ray->radius == 0.0f) {
      dist = bvhtree_ray_tri_intersection(ray, hit->dist, t0, t1, t2);
    }
    else {
      dist = bvhtree_sphereray_tri_intersection(ray, ray->radius, hit->dist, t0, t1, t2);
    }

    if (dist >= 0 && dist < hit->dist) {
      hit->index = index;
      hit->dist = dist;
      madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

      normal_tri_v3(hit->no, t0, t1, t2);
    }
  }
}

/**
 * Callback to BVH-tree nearest point.
 * The tree must have been built using #bvhtree_from_mesh_edges.
 *
 * \param userdata: Must be a #BVHMeshCallbackUserdata built from the same mesh as the tree.
 */
static void mesh_edges_nearest_point(void *userdata,
                                     int index,
                                     const float co[3],
                                     BVHTreeNearest *nearest)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const float(*positions)[3] = data->vert_positions;
  const blender::int2 &edge = reinterpret_cast<const blender::int2 *>(data->edge)[index];
  float nearest_tmp[3], dist_sq;

  const float *t0, *t1;
  t0 = positions[edge[0]];
  t1 = positions[edge[1]];

  closest_to_line_segment_v3(nearest_tmp, co, t0, t1);
  dist_sq = len_squared_v3v3(nearest_tmp, co);

  if (dist_sq < nearest->dist_sq) {
    nearest->index = index;
    nearest->dist_sq = dist_sq;
    copy_v3_v3(nearest->co, nearest_tmp);
    sub_v3_v3v3(nearest->no, t0, t1);
    normalize_v3(nearest->no);
  }
}

/* Helper, does all the point-sphere-cast work actually. */
static void mesh_verts_spherecast_do(int index,
                                     const float v[3],
                                     const BVHTreeRay *ray,
                                     BVHTreeRayHit *hit)
{
  float dist;
  const float *r1;
  float r2[3], i1[3];
  r1 = ray->origin;
  add_v3_v3v3(r2, r1, ray->direction);

  closest_to_line_segment_v3(i1, v, r1, r2);

  /* No hit if closest point is 'behind' the origin of the ray, or too far away from it. */
  if ((dot_v3v3v3(r1, i1, r2) >= 0.0f) && ((dist = len_v3v3(r1, i1)) < hit->dist)) {
    hit->index = index;
    hit->dist = dist;
    copy_v3_v3(hit->co, i1);
  }
}

static void editmesh_verts_spherecast(void *userdata,
                                      int index,
                                      const BVHTreeRay *ray,
                                      BVHTreeRayHit *hit)
{
  const BVHTreeFromEditMesh *data = (const BVHTreeFromEditMesh *)userdata;
  BMVert *eve = BM_vert_at_index(data->em->bm, index);

  mesh_verts_spherecast_do(index, eve->co, ray, hit);
}

/**
 * Callback to BVH-tree ray-cast.
 * The tree must have been built using bvhtree_from_mesh_verts.
 *
 * \param userdata: Must be a #BVHMeshCallbackUserdata built from the same mesh as the tree.
 */
static void mesh_verts_spherecast(void *userdata,
                                  int index,
                                  const BVHTreeRay *ray,
                                  BVHTreeRayHit *hit)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const float *v = data->vert_positions[index];

  mesh_verts_spherecast_do(index, v, ray, hit);
}

/**
 * Callback to BVH-tree ray-cast.
 * The tree must have been built using bvhtree_from_mesh_edges.
 *
 * \param userdata: Must be a #BVHMeshCallbackUserdata built from the same mesh as the tree.
 */
static void mesh_edges_spherecast(void *userdata,
                                  int index,
                                  const BVHTreeRay *ray,
                                  BVHTreeRayHit *hit)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const float(*positions)[3] = data->vert_positions;
  const blender::int2 &edge = reinterpret_cast<const blender::int2 *>(data->edge)[index];

  const float radius_sq = square_f(ray->radius);
  float dist;
  const float *v1, *v2, *r1;
  float r2[3], i1[3], i2[3];
  v1 = positions[edge[0]];
  v2 = positions[edge[1]];

  /* In case we get a zero-length edge, handle it as a point! */
  if (equals_v3v3(v1, v2)) {
    mesh_verts_spherecast_do(index, v1, ray, hit);
    return;
  }

  r1 = ray->origin;
  add_v3_v3v3(r2, r1, ray->direction);

  if (isect_line_line_v3(v1, v2, r1, r2, i1, i2)) {
    /* No hit if intersection point is 'behind' the origin of the ray, or too far away from it. */
    if ((dot_v3v3v3(r1, i2, r2) >= 0.0f) && ((dist = len_v3v3(r1, i2)) < hit->dist)) {
      const float e_fac = line_point_factor_v3(i1, v1, v2);
      if (e_fac < 0.0f) {
        copy_v3_v3(i1, v1);
      }
      else if (e_fac > 1.0f) {
        copy_v3_v3(i1, v2);
      }
      /* Ensure ray is really close enough from edge! */
      if (len_squared_v3v3(i1, i2) <= radius_sq) {
        hit->index = index;
        hit->dist = dist;
        copy_v3_v3(hit->co, i2);
      }
    }
  }
}

/** \} */

/*
 * BVH builders
 */

/* -------------------------------------------------------------------- */
/** \name Common Utils
 * \{ */

static void bvhtree_from_mesh_setup_data(BVHTree *tree,
                                         const BVHCacheType bvh_cache_type,
                                         const float (*positions)[3],
                                         const blender::int2 *edge,
                                         const MFace *face,
                                         const int *corner_verts,
                                         const Span<MLoopTri> looptris,
                                         BVHTreeFromMesh *r_data)
{
  memset(r_data, 0, sizeof(*r_data));

  r_data->tree = tree;

  r_data->vert_positions = positions;
  r_data->edge = reinterpret_cast<const vec2i *>(edge);
  r_data->face = face;
  r_data->corner_verts = corner_verts;
  r_data->looptri = looptris.data();

  switch (bvh_cache_type) {
    case BVHTREE_FROM_VERTS:
    case BVHTREE_FROM_LOOSEVERTS:
      /* a nullptr nearest callback works fine
       * remember the min distance to point is the same as the min distance to BV of point */
      r_data->nearest_callback = nullptr;
      r_data->raycast_callback = mesh_verts_spherecast;
      break;

    case BVHTREE_FROM_EDGES:
    case BVHTREE_FROM_LOOSEEDGES:
      r_data->nearest_callback = mesh_edges_nearest_point;
      r_data->raycast_callback = mesh_edges_spherecast;
      break;
    case BVHTREE_FROM_FACES:
      r_data->nearest_callback = mesh_faces_nearest_point;
      r_data->raycast_callback = mesh_faces_spherecast;
      break;
    case BVHTREE_FROM_LOOPTRI:
    case BVHTREE_FROM_LOOPTRI_NO_HIDDEN:
      r_data->nearest_callback = mesh_looptri_nearest_point;
      r_data->raycast_callback = mesh_looptri_spherecast;
      break;
    case BVHTREE_FROM_EM_VERTS:
    case BVHTREE_FROM_EM_EDGES:
    case BVHTREE_FROM_EM_LOOPTRI:
    case BVHTREE_MAX_ITEM:
      BLI_assert(false);
      break;
  }
}

static void bvhtree_from_editmesh_setup_data(BVHTree *tree,
                                             const BVHCacheType bvh_cache_type,
                                             BMEditMesh *em,
                                             BVHTreeFromEditMesh *r_data)
{
  memset(r_data, 0, sizeof(*r_data));

  r_data->tree = tree;

  r_data->em = em;

  switch (bvh_cache_type) {
    case BVHTREE_FROM_EM_VERTS:
      r_data->nearest_callback = nullptr;
      r_data->raycast_callback = editmesh_verts_spherecast;
      break;
    case BVHTREE_FROM_EM_EDGES:
      r_data->nearest_callback = nullptr; /* TODO */
      r_data->raycast_callback = nullptr; /* TODO */
      break;
    case BVHTREE_FROM_EM_LOOPTRI:
      r_data->nearest_callback = editmesh_looptri_nearest_point;
      r_data->raycast_callback = editmesh_looptri_spherecast;
      break;

    case BVHTREE_FROM_VERTS:
    case BVHTREE_FROM_LOOSEVERTS:
    case BVHTREE_FROM_EDGES:
    case BVHTREE_FROM_LOOSEEDGES:
    case BVHTREE_FROM_FACES:
    case BVHTREE_FROM_LOOPTRI:
    case BVHTREE_FROM_LOOPTRI_NO_HIDDEN:
    case BVHTREE_MAX_ITEM:
      BLI_assert(false);
      break;
  }
}

static BVHTree *bvhtree_new_common(
    float epsilon, int tree_type, int axis, int elems_num, int &elems_num_active)
{
  if (elems_num_active != -1) {
    BLI_assert(IN_RANGE_INCL(elems_num_active, 0, elems_num));
  }
  else {
    elems_num_active = elems_num;
  }

  if (elems_num_active == 0) {
    return nullptr;
  }

  return BLI_bvhtree_new(elems_num_active, epsilon, tree_type, axis);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Builder
 * \{ */

static BVHTree *bvhtree_from_editmesh_verts_create_tree(float epsilon,
                                                        int tree_type,
                                                        int axis,
                                                        BMEditMesh *em,
                                                        const BitSpan verts_mask,
                                                        int verts_num_active)
{
  const int verts_num = em->bm->totvert;

  BVHTree *tree = bvhtree_new_common(epsilon, tree_type, axis, verts_num, verts_num_active);
  if (!tree) {
    return nullptr;
  }

  BM_mesh_elem_table_ensure(em->bm, BM_VERT);

  for (int i = 0; i < verts_num; i++) {
    if (!verts_mask.is_empty() && !verts_mask[i]) {
      continue;
    }
    BMVert *eve = BM_vert_at_index(em->bm, i);
    BLI_bvhtree_insert(tree, i, eve->co, 1);
  }
  BLI_assert(BLI_bvhtree_get_len(tree) == verts_num_active);

  return tree;
}

static BVHTree *bvhtree_from_mesh_verts_create_tree(float epsilon,
                                                    int tree_type,
                                                    int axis,
                                                    const float (*positions)[3],
                                                    const int verts_num,
                                                    const BitSpan verts_mask,
                                                    int verts_num_active)
{
  BVHTree *tree = bvhtree_new_common(epsilon, tree_type, axis, verts_num, verts_num_active);
  if (!tree) {
    return nullptr;
  }

  for (int i = 0; i < verts_num; i++) {
    if (!verts_mask.is_empty() && !verts_mask[i]) {
      continue;
    }
    BLI_bvhtree_insert(tree, i, positions[i], 1);
  }
  BLI_assert(BLI_bvhtree_get_len(tree) == verts_num_active);

  return tree;
}

BVHTree *bvhtree_from_editmesh_verts_ex(BVHTreeFromEditMesh *data,
                                        BMEditMesh *em,
                                        const BitSpan verts_mask,
                                        int verts_num_active,
                                        float epsilon,
                                        int tree_type,
                                        int axis)
{
  BVHTree *tree = bvhtree_from_editmesh_verts_create_tree(
      epsilon, tree_type, axis, em, verts_mask, verts_num_active);

  bvhtree_balance(tree, false);

  if (data) {
    bvhtree_from_editmesh_setup_data(tree, BVHTREE_FROM_EM_VERTS, em, data);
  }

  return tree;
}

BVHTree *bvhtree_from_editmesh_verts(
    BVHTreeFromEditMesh *data, BMEditMesh *em, float epsilon, int tree_type, int axis)
{
  return bvhtree_from_editmesh_verts_ex(data, em, {}, -1, epsilon, tree_type, axis);
}

BVHTree *bvhtree_from_mesh_verts_ex(BVHTreeFromMesh *data,
                                    const float (*vert_positions)[3],
                                    const int verts_num,
                                    const BitSpan verts_mask,
                                    int verts_num_active,
                                    float epsilon,
                                    int tree_type,
                                    int axis)
{
  BVHTree *tree = bvhtree_from_mesh_verts_create_tree(
      epsilon, tree_type, axis, vert_positions, verts_num, verts_mask, verts_num_active);

  bvhtree_balance(tree, false);

  if (data) {
    /* Setup BVHTreeFromMesh */
    bvhtree_from_mesh_setup_data(
        tree, BVHTREE_FROM_VERTS, vert_positions, nullptr, nullptr, nullptr, {}, data);
  }

  return tree;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge Builder
 * \{ */

static BVHTree *bvhtree_from_editmesh_edges_create_tree(float epsilon,
                                                        int tree_type,
                                                        int axis,
                                                        BMEditMesh *em,
                                                        const BitSpan edges_mask,
                                                        int edges_num_active)
{
  const int edges_num = em->bm->totedge;

  BVHTree *tree = bvhtree_new_common(epsilon, tree_type, axis, edges_num, edges_num_active);
  if (!tree) {
    return nullptr;
  }

  BM_mesh_elem_table_ensure(em->bm, BM_EDGE);

  int i;
  BMIter iter;
  BMEdge *eed;
  BM_ITER_MESH_INDEX (eed, &iter, em->bm, BM_EDGES_OF_MESH, i) {
    if (!edges_mask.is_empty() && !edges_mask[i]) {
      continue;
    }
    float co[2][3];
    copy_v3_v3(co[0], eed->v1->co);
    copy_v3_v3(co[1], eed->v2->co);

    BLI_bvhtree_insert(tree, i, co[0], 2);
  }
  BLI_assert(BLI_bvhtree_get_len(tree) == edges_num_active);

  return tree;
}

static BVHTree *bvhtree_from_mesh_edges_create_tree(const float (*positions)[3],
                                                    blender::Span<blender::int2> edges,
                                                    const BitSpan edges_mask,
                                                    int edges_num_active,
                                                    float epsilon,
                                                    int tree_type,
                                                    int axis)
{
  BVHTree *tree = bvhtree_new_common(epsilon, tree_type, axis, edges.size(), edges_num_active);
  if (!tree) {
    return nullptr;
  }

  for (const int i : edges.index_range()) {
    if (!edges_mask.is_empty() && !edges_mask[i]) {
      continue;
    }
    float co[2][3];
    copy_v3_v3(co[0], positions[edges[i][0]]);
    copy_v3_v3(co[1], positions[edges[i][1]]);

    BLI_bvhtree_insert(tree, i, co[0], 2);
  }

  return tree;
}

BVHTree *bvhtree_from_editmesh_edges_ex(BVHTreeFromEditMesh *data,
                                        BMEditMesh *em,
                                        const BitSpan edges_mask,
                                        int edges_num_active,
                                        float epsilon,
                                        int tree_type,
                                        int axis)
{
  BVHTree *tree = bvhtree_from_editmesh_edges_create_tree(
      epsilon, tree_type, axis, em, edges_mask, edges_num_active);

  bvhtree_balance(tree, false);

  if (data) {
    bvhtree_from_editmesh_setup_data(tree, BVHTREE_FROM_EM_EDGES, em, data);
  }

  return tree;
}

BVHTree *bvhtree_from_editmesh_edges(
    BVHTreeFromEditMesh *data, BMEditMesh *em, float epsilon, int tree_type, int axis)
{
  return bvhtree_from_editmesh_edges_ex(data, em, {}, -1, epsilon, tree_type, axis);
}

BVHTree *bvhtree_from_mesh_edges_ex(BVHTreeFromMesh *data,
                                    const float (*vert_positions)[3],
                                    const blender::int2 *edge,
                                    const int edges_num,
                                    const BitSpan edges_mask,
                                    int edges_num_active,
                                    float epsilon,
                                    int tree_type,
                                    int axis)
{
  BVHTree *tree = bvhtree_from_mesh_edges_create_tree(
      vert_positions, {edge, edges_num}, edges_mask, edges_num_active, epsilon, tree_type, axis);

  bvhtree_balance(tree, false);

  if (data) {
    /* Setup BVHTreeFromMesh */
    bvhtree_from_mesh_setup_data(
        tree, BVHTREE_FROM_EDGES, vert_positions, edge, nullptr, nullptr, {}, data);
  }

  return tree;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tessellated Face Builder
 * \{ */

static BVHTree *bvhtree_from_mesh_faces_create_tree(float epsilon,
                                                    int tree_type,
                                                    int axis,
                                                    const float (*positions)[3],
                                                    const MFace *face,
                                                    const int faces_num,
                                                    const BitSpan faces_mask,
                                                    int faces_num_active)
{
  BVHTree *tree = bvhtree_new_common(epsilon, tree_type, axis, faces_num, faces_num_active);
  if (!tree) {
    return nullptr;
  }

  if (positions && face) {
    for (int i = 0; i < faces_num; i++) {
      float co[4][3];
      if (!faces_mask.is_empty() && !faces_mask[i]) {
        continue;
      }

      copy_v3_v3(co[0], positions[face[i].v1]);
      copy_v3_v3(co[1], positions[face[i].v2]);
      copy_v3_v3(co[2], positions[face[i].v3]);
      if (face[i].v4) {
        copy_v3_v3(co[3], positions[face[i].v4]);
      }

      BLI_bvhtree_insert(tree, i, co[0], face[i].v4 ? 4 : 3);
    }
  }
  BLI_assert(BLI_bvhtree_get_len(tree) == faces_num_active);

  return tree;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name LoopTri Face Builder
 * \{ */

static BVHTree *bvhtree_from_editmesh_looptri_create_tree(float epsilon,
                                                          int tree_type,
                                                          int axis,
                                                          BMEditMesh *em,
                                                          const BitSpan looptri_mask,
                                                          int looptri_num_active)
{
  const int looptri_num = em->tottri;

  BVHTree *tree = bvhtree_new_common(epsilon, tree_type, axis, looptri_num, looptri_num_active);
  if (!tree) {
    return nullptr;
  }

  const BMLoop *(*looptris)[3] = (const BMLoop *(*)[3])em->looptris;

  /* Insert BMesh-tessellation triangles into the BVH-tree, unless they are hidden
   * and/or selected. Even if the faces themselves are not selected for the snapped
   * transform, having a vertex selected means the face (and thus it's tessellated
   * triangles) will be moving and will not be a good snap targets. */
  for (int i = 0; i < looptri_num; i++) {
    const BMLoop **ltri = looptris[i];
    bool insert = !looptri_mask.is_empty() ? looptri_mask[i] : true;

    if (insert) {
      /* No reason found to block hit-testing the triangle for snap, so insert it now. */
      float co[3][3];
      copy_v3_v3(co[0], ltri[0]->v->co);
      copy_v3_v3(co[1], ltri[1]->v->co);
      copy_v3_v3(co[2], ltri[2]->v->co);

      BLI_bvhtree_insert(tree, i, co[0], 3);
    }
  }
  BLI_assert(BLI_bvhtree_get_len(tree) == looptri_num_active);

  return tree;
}

static BVHTree *bvhtree_from_mesh_looptri_create_tree(float epsilon,
                                                      int tree_type,
                                                      int axis,
                                                      const float (*positions)[3],
                                                      const int *corner_verts,
                                                      const Span<MLoopTri> looptris,
                                                      const BitSpan looptri_mask,
                                                      int looptri_num_active)
{
  if (positions == nullptr) {
    return nullptr;
  }

  BVHTree *tree = bvhtree_new_common(
      epsilon, tree_type, axis, looptris.size(), looptri_num_active);

  if (!tree) {
    return nullptr;
  }

  for (const int i : looptris.index_range()) {
    float co[3][3];
    if (!looptri_mask.is_empty() && !looptri_mask[i]) {
      continue;
    }

    copy_v3_v3(co[0], positions[corner_verts[looptris[i].tri[0]]]);
    copy_v3_v3(co[1], positions[corner_verts[looptris[i].tri[1]]]);
    copy_v3_v3(co[2], positions[corner_verts[looptris[i].tri[2]]]);

    BLI_bvhtree_insert(tree, i, co[0], 3);
  }

  BLI_assert(BLI_bvhtree_get_len(tree) == looptri_num_active);

  return tree;
}

BVHTree *bvhtree_from_editmesh_looptri_ex(BVHTreeFromEditMesh *data,
                                          BMEditMesh *em,
                                          const BitSpan looptri_mask,
                                          int looptri_num_active,
                                          float epsilon,
                                          int tree_type,
                                          int axis)
{
  /* BMESH specific check that we have tessfaces,
   * we _could_ tessellate here but rather not - campbell */
  BVHTree *tree = bvhtree_from_editmesh_looptri_create_tree(
      epsilon, tree_type, axis, em, looptri_mask, looptri_num_active);

  bvhtree_balance(tree, false);

  if (data) {
    bvhtree_from_editmesh_setup_data(tree, BVHTREE_FROM_EM_LOOPTRI, em, data);
  }
  return tree;
}

BVHTree *bvhtree_from_editmesh_looptri(
    BVHTreeFromEditMesh *data, BMEditMesh *em, float epsilon, int tree_type, int axis)
{
  return bvhtree_from_editmesh_looptri_ex(data, em, {}, -1, epsilon, tree_type, axis);
}

BVHTree *bvhtree_from_mesh_looptri_ex(BVHTreeFromMesh *data,
                                      const float (*vert_positions)[3],
                                      const int *corner_verts,
                                      const MLoopTri *looptri,
                                      const int looptri_num,
                                      const BitSpan looptri_mask,
                                      int looptri_num_active,
                                      float epsilon,
                                      int tree_type,
                                      int axis)
{
  BVHTree *tree = bvhtree_from_mesh_looptri_create_tree(epsilon,
                                                        tree_type,
                                                        axis,
                                                        vert_positions,
                                                        corner_verts,
                                                        {looptri, looptri_num},
                                                        looptri_mask,
                                                        looptri_num_active);

  bvhtree_balance(tree, false);

  if (data) {
    /* Setup BVHTreeFromMesh */
    bvhtree_from_mesh_setup_data(tree,
                                 BVHTREE_FROM_LOOPTRI,
                                 vert_positions,
                                 nullptr,
                                 nullptr,
                                 corner_verts,
                                 {looptri, looptri_num},
                                 data);
  }

  return tree;
}

static BitVector<> looptri_no_hidden_map_get(const blender::OffsetIndices<int> polys,
                                             const VArray<bool> &hide_poly,
                                             const int looptri_len,
                                             int *r_looptri_active_len)
{
  if (hide_poly.is_single() && !hide_poly.get_internal_single()) {
    return {};
  }
  BitVector<> looptri_mask(looptri_len);

  int looptri_no_hidden_len = 0;
  int looptri_index = 0;
  for (const int64_t i : polys.index_range()) {
    const int triangles_num = ME_POLY_TRI_TOT(polys[i].size());
    if (hide_poly[i]) {
      looptri_index += triangles_num;
    }
    else {
      for (const int i : IndexRange(triangles_num)) {
        UNUSED_VARS(i);
        looptri_mask[looptri_index].set();
        looptri_index++;
        looptri_no_hidden_len++;
      }
    }
  }

  *r_looptri_active_len = looptri_no_hidden_len;

  return looptri_mask;
}

BVHTree *BKE_bvhtree_from_mesh_get(BVHTreeFromMesh *data,
                                   const Mesh *mesh,
                                   const BVHCacheType bvh_cache_type,
                                   const int tree_type)
{
  BVHCache **bvh_cache_p = (BVHCache **)&mesh->runtime->bvh_cache;

  Span<MLoopTri> looptris;
  if (ELEM(bvh_cache_type, BVHTREE_FROM_LOOPTRI, BVHTREE_FROM_LOOPTRI_NO_HIDDEN)) {
    looptris = mesh->looptris();
  }
  const float(*positions)[3] = reinterpret_cast<const float(*)[3]>(mesh->vert_positions().data());
  const Span<blender::int2> edges = mesh->edges();
  const Span<int> corner_verts = mesh->corner_verts();

  /* Setup BVHTreeFromMesh */
  bvhtree_from_mesh_setup_data(nullptr,
                               bvh_cache_type,
                               positions,
                               edges.data(),
                               (const MFace *)CustomData_get_layer(&mesh->fdata, CD_MFACE),
                               corner_verts.data(),
                               looptris,
                               data);

  bool lock_started = false;
  data->cached = bvhcache_find(
      bvh_cache_p, bvh_cache_type, &data->tree, &lock_started, &mesh->runtime->eval_mutex);

  if (data->cached) {
    BLI_assert(lock_started == false);

    /* NOTE: #data->tree can be nullptr. */
    return data->tree;
  }

  /* Create BVHTree. */

  switch (bvh_cache_type) {
    case BVHTREE_FROM_LOOSEVERTS: {
      const blender::bke::LooseVertCache &loose_verts = mesh->loose_verts();
      data->tree = bvhtree_from_mesh_verts_create_tree(0.0f,
                                                       tree_type,
                                                       6,
                                                       positions,
                                                       mesh->totvert,
                                                       loose_verts.is_loose_bits,
                                                       loose_verts.count);
      break;
    }
    case BVHTREE_FROM_VERTS: {
      data->tree = bvhtree_from_mesh_verts_create_tree(
          0.0f, tree_type, 6, positions, mesh->totvert, {}, -1);
      break;
    }
    case BVHTREE_FROM_LOOSEEDGES: {
      const blender::bke::LooseEdgeCache &loose_edges = mesh->loose_edges();
      data->tree = bvhtree_from_mesh_edges_create_tree(
          positions, edges, loose_edges.is_loose_bits, loose_edges.count, 0.0f, tree_type, 6);
      break;
    }
    case BVHTREE_FROM_EDGES: {
      data->tree = bvhtree_from_mesh_edges_create_tree(
          positions, edges, {}, -1, 0.0f, tree_type, 6);
      break;
    }
    case BVHTREE_FROM_FACES: {
      BLI_assert(!(mesh->totface == 0 && mesh->totpoly != 0));
      data->tree = bvhtree_from_mesh_faces_create_tree(
          0.0f,
          tree_type,
          6,
          positions,
          (const MFace *)CustomData_get_layer(&mesh->fdata, CD_MFACE),
          mesh->totface,
          {},
          -1);
      break;
    }
    case BVHTREE_FROM_LOOPTRI_NO_HIDDEN: {
      blender::bke::AttributeAccessor attributes = mesh->attributes();
      int mask_bits_act_len = -1;
      const BitVector<> mask = looptri_no_hidden_map_get(
          mesh->polys(),
          *attributes.lookup_or_default(".hide_poly", ATTR_DOMAIN_FACE, false),
          looptris.size(),
          &mask_bits_act_len);
      data->tree = bvhtree_from_mesh_looptri_create_tree(
          0.0f, tree_type, 6, positions, corner_verts.data(), looptris, mask, mask_bits_act_len);
      break;
    }
    case BVHTREE_FROM_LOOPTRI: {
      data->tree = bvhtree_from_mesh_looptri_create_tree(
          0.0f, tree_type, 6, positions, corner_verts.data(), looptris, {}, -1);
      break;
    }
    case BVHTREE_FROM_EM_VERTS:
    case BVHTREE_FROM_EM_EDGES:
    case BVHTREE_FROM_EM_LOOPTRI:
    case BVHTREE_MAX_ITEM:
      BLI_assert_unreachable();
      break;
  }

  bvhtree_balance(data->tree, lock_started);

  /* Save on cache for later use */
  // printf("BVHTree built and saved on cache\n");
  BLI_assert(data->cached == false);
  data->cached = true;
  bvhcache_insert(*bvh_cache_p, data->tree, bvh_cache_type);
  bvhcache_unlock(*bvh_cache_p, lock_started);

#ifdef DEBUG
  if (data->tree != nullptr) {
    if (BLI_bvhtree_get_tree_type(data->tree) != tree_type) {
      printf("tree_type %d obtained instead of %d\n",
             BLI_bvhtree_get_tree_type(data->tree),
             tree_type);
    }
  }
#endif

  return data->tree;
}

BVHTree *BKE_bvhtree_from_editmesh_get(BVHTreeFromEditMesh *data,
                                       BMEditMesh *em,
                                       const int tree_type,
                                       const BVHCacheType bvh_cache_type,
                                       BVHCache **bvh_cache_p,
                                       std::mutex *mesh_eval_mutex)
{
  bool lock_started = false;

  bvhtree_from_editmesh_setup_data(nullptr, bvh_cache_type, em, data);

  if (bvh_cache_p) {
    data->cached = bvhcache_find(
        bvh_cache_p, bvh_cache_type, &data->tree, &lock_started, mesh_eval_mutex);

    if (data->cached) {
      BLI_assert(lock_started == false);
      return data->tree;
    }
  }

  switch (bvh_cache_type) {
    case BVHTREE_FROM_EM_VERTS:
      data->tree = bvhtree_from_editmesh_verts_create_tree(0.0f, tree_type, 6, em, {}, -1);
      break;
    case BVHTREE_FROM_EM_EDGES:
      data->tree = bvhtree_from_editmesh_edges_create_tree(0.0f, tree_type, 6, em, {}, -1);
      break;
    case BVHTREE_FROM_EM_LOOPTRI:
      data->tree = bvhtree_from_editmesh_looptri_create_tree(0.0f, tree_type, 6, em, {}, -1);
      break;
    case BVHTREE_FROM_VERTS:
    case BVHTREE_FROM_EDGES:
    case BVHTREE_FROM_FACES:
    case BVHTREE_FROM_LOOPTRI:
    case BVHTREE_FROM_LOOPTRI_NO_HIDDEN:
    case BVHTREE_FROM_LOOSEVERTS:
    case BVHTREE_FROM_LOOSEEDGES:
    case BVHTREE_MAX_ITEM:
      BLI_assert(false);
      break;
  }

  bvhtree_balance(data->tree, lock_started);

  if (bvh_cache_p) {
    /* Save on cache for later use */
    // printf("BVHTree built and saved on cache\n");
    BLI_assert(data->cached == false);
    data->cached = true;
    bvhcache_insert(*bvh_cache_p, data->tree, bvh_cache_type);
    bvhcache_unlock(*bvh_cache_p, lock_started);
  }

#ifdef DEBUG
  if (data->tree != nullptr) {
    if (BLI_bvhtree_get_tree_type(data->tree) != tree_type) {
      printf("tree_type %d obtained instead of %d\n",
             BLI_bvhtree_get_tree_type(data->tree),
             tree_type);
    }
  }
#endif

  return data->tree;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Functions
 * \{ */

void free_bvhtree_from_editmesh(BVHTreeFromEditMesh *data)
{
  if (data->tree) {
    if (!data->cached) {
      BLI_bvhtree_free(data->tree);
    }
    memset(data, 0, sizeof(*data));
  }
}

void free_bvhtree_from_mesh(BVHTreeFromMesh *data)
{
  if (data->tree && !data->cached) {
    BLI_bvhtree_free(data->tree);
  }

  memset(data, 0, sizeof(*data));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Point Cloud BVH Building
 * \{ */

BVHTree *BKE_bvhtree_from_pointcloud_get(BVHTreeFromPointCloud *data,
                                         const PointCloud *pointcloud,
                                         const int tree_type)
{
  int tot_point = pointcloud->totpoint;
  BVHTree *tree = bvhtree_new_common(0.0f, tree_type, 6, tot_point, tot_point);
  if (!tree) {
    return nullptr;
  }

  const Span<float3> positions = pointcloud->positions();
  for (const int i : positions.index_range()) {
    BLI_bvhtree_insert(tree, i, positions[i], 1);
  }

  BLI_assert(BLI_bvhtree_get_len(tree) == tot_point);
  bvhtree_balance(tree, false);

  data->coords = (const float(*)[3])positions.data();
  data->tree = tree;
  data->nearest_callback = nullptr;

  return tree;
}

void free_bvhtree_from_pointcloud(BVHTreeFromPointCloud *data)
{
  if (data->tree) {
    BLI_bvhtree_free(data->tree);
  }
  memset(data, 0, sizeof(*data));
}

/** \} */
