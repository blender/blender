/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bke
 *
 * Functions for mapping data between meshes.
 */

#include <limits.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "BLI_astar.h"
#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_rand.h"

#include "BKE_bvhutils.h"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_remap.h" /* own include */
#include "BKE_mesh_runtime.h"

#include "BLI_strict_flags.h"

static CLG_LogRef LOG = {"bke.mesh"};

/* -------------------------------------------------------------------- */
/** \name Some generic helpers.
 * \{ */

static bool mesh_remap_bvhtree_query_nearest(BVHTreeFromMesh *treedata,
                                             BVHTreeNearest *nearest,
                                             const float co[3],
                                             const float max_dist_sq,
                                             float *r_hit_dist)
{
  /* Use local proximity heuristics (to reduce the nearest search). */
  if (nearest->index != -1) {
    nearest->dist_sq = min_ff(len_squared_v3v3(co, nearest->co), max_dist_sq);
  }
  else {
    nearest->dist_sq = max_dist_sq;
  }
  /* Compute and store result. If invalid (-1 index), keep FLT_MAX dist. */
  BLI_bvhtree_find_nearest(treedata->tree, co, nearest, treedata->nearest_callback, treedata);

  if ((nearest->index != -1) && (nearest->dist_sq <= max_dist_sq)) {
    *r_hit_dist = sqrtf(nearest->dist_sq);
    return true;
  }
  else {
    return false;
  }
}

static bool mesh_remap_bvhtree_query_raycast(BVHTreeFromMesh *treedata,
                                             BVHTreeRayHit *rayhit,
                                             const float co[3],
                                             const float no[3],
                                             const float radius,
                                             const float max_dist,
                                             float *r_hit_dist)
{
  BVHTreeRayHit rayhit_tmp;
  float inv_no[3];

  rayhit->index = -1;
  rayhit->dist = max_dist;
  BLI_bvhtree_ray_cast(
      treedata->tree, co, no, radius, rayhit, treedata->raycast_callback, treedata);

  /* Also cast in the other direction! */
  rayhit_tmp = *rayhit;
  negate_v3_v3(inv_no, no);
  BLI_bvhtree_ray_cast(
      treedata->tree, co, inv_no, radius, &rayhit_tmp, treedata->raycast_callback, treedata);
  if (rayhit_tmp.dist < rayhit->dist) {
    *rayhit = rayhit_tmp;
  }

  if ((rayhit->index != -1) && (rayhit->dist <= max_dist)) {
    *r_hit_dist = rayhit->dist;
    return true;
  }
  else {
    return false;
  }
}

/** \} */

/**
 * \name Auto-match.
 *
 * Find transform of a mesh to get best match with another.
 * \{ */

/**
 * Compute a value of the difference between both given meshes.
 * The smaller the result, the better the match.
 *
 * We return the inverse of the average of the inversed
 * shortest distance from each dst vertex to src ones.
 * In other words, beyond a certain (relatively small) distance, all differences have more or less
 * the same weight in final result, which allows to reduce influence of a few high differences,
 * in favor of a global good matching.
 */
float BKE_mesh_remap_calc_difference_from_mesh(const SpaceTransform *space_transform,
                                               const MVert *verts_dst,
                                               const int numverts_dst,
                                               Mesh *me_src)
{
  BVHTreeFromMesh treedata = {NULL};
  BVHTreeNearest nearest = {0};
  float hit_dist;

  float result = 0.0f;
  int i;

  BKE_bvhtree_from_mesh_get(&treedata, me_src, BVHTREE_FROM_VERTS, 2);
  nearest.index = -1;

  for (i = 0; i < numverts_dst; i++) {
    float tmp_co[3];

    copy_v3_v3(tmp_co, verts_dst[i].co);

    /* Convert the vertex to tree coordinates, if needed. */
    if (space_transform) {
      BLI_space_transform_apply(space_transform, tmp_co);
    }

    if (mesh_remap_bvhtree_query_nearest(&treedata, &nearest, tmp_co, FLT_MAX, &hit_dist)) {
      result += 1.0f / (hit_dist + 1.0f);
    }
    else {
      /* No source for this dest vertex! */
      result += 1e-18f;
    }
  }

  result = ((float)numverts_dst / result) - 1.0f;

#if 0
  printf("%s: Computed difference between meshes (the lower the better): %f\n", __func__, result);
#endif

  return result;
}

/* This helper computes the eigen values & vectors for
 * covariance matrix of all given vertices coordinates.
 *
 * Those vectors define the 'average ellipsoid' of the mesh (i.e. the 'best fitting' ellipsoid
 * containing 50% of the vertices).
 *
 * Note that it will not perform fantastic in case two or more eigen values are equal
 * (e.g. a cylinder or parallelepiped with a square section give two identical eigenvalues,
 * a sphere or tetrahedron give three identical ones, etc.), since you cannot really define all
 * axes in those cases. We default to dummy generated orthogonal vectors in this case,
 * instead of using eigen vectors.
 */
static void mesh_calc_eigen_matrix(const MVert *verts,
                                   const float (*vcos)[3],
                                   const int numverts,
                                   float r_mat[4][4])
{
  float center[3], covmat[3][3];
  float eigen_val[3], eigen_vec[3][3];
  float(*cos)[3] = NULL;

  bool eigen_success;
  int i;

  if (verts) {
    const MVert *mv;
    float(*co)[3];

    cos = MEM_mallocN(sizeof(*cos) * (size_t)numverts, __func__);
    for (i = 0, co = cos, mv = verts; i < numverts; i++, co++, mv++) {
      copy_v3_v3(*co, mv->co);
    }
    /* TODO(sergey): For until we officially drop all compilers which
     * doesn't handle casting correct we use workaround to avoid explicit
     * cast here.
     */
    vcos = (void *)cos;
  }
  unit_m4(r_mat);

  /* Note: here we apply sample correction to covariance matrix, since we consider the vertices
   *       as a sample of the whole 'surface' population of our mesh. */
  BLI_covariance_m3_v3n(vcos, numverts, true, covmat, center);

  if (cos) {
    MEM_freeN(cos);
  }

  eigen_success = BLI_eigen_solve_selfadjoint_m3((const float(*)[3])covmat, eigen_val, eigen_vec);
  BLI_assert(eigen_success);
  UNUSED_VARS_NDEBUG(eigen_success);

  /* Special handling of cases where some eigen values are (nearly) identical. */
  if (compare_ff_relative(eigen_val[0], eigen_val[1], FLT_EPSILON, 64)) {
    if (compare_ff_relative(eigen_val[0], eigen_val[2], FLT_EPSILON, 64)) {
      /* No preferred direction, that set of vertices has a spherical average,
       * so we simply returned scaled/translated identity matrix (with no rotation). */
      unit_m3(eigen_vec);
    }
    else {
      /* Ellipsoid defined by eigen values/vectors has a spherical section,
       * we can only define one axis from eigen_vec[2] (two others computed eigen vecs
       * are not so nice for us here, they tend to 'randomly' rotate around valid one).
       * Note that eigen vectors as returned by BLI_eigen_solve_selfadjoint_m3() are normalized. */
      ortho_basis_v3v3_v3(eigen_vec[0], eigen_vec[1], eigen_vec[2]);
    }
  }
  else if (compare_ff_relative(eigen_val[0], eigen_val[2], FLT_EPSILON, 64)) {
    /* Same as above, but with eigen_vec[1] as valid axis. */
    ortho_basis_v3v3_v3(eigen_vec[2], eigen_vec[0], eigen_vec[1]);
  }
  else if (compare_ff_relative(eigen_val[1], eigen_val[2], FLT_EPSILON, 64)) {
    /* Same as above, but with eigen_vec[0] as valid axis. */
    ortho_basis_v3v3_v3(eigen_vec[1], eigen_vec[2], eigen_vec[0]);
  }

  for (i = 0; i < 3; i++) {
    float evi = eigen_val[i];

    /* Protect against 1D/2D degenerated cases! */
    /* Note: not sure why we need square root of eigen values here
     * (which are equivalent to singular values, as far as I have understood),
     * but it seems to heavily reduce (if not completely nullify)
     * the error due to non-uniform scalings... */
    evi = (evi < 1e-6f && evi > -1e-6f) ? ((evi < 0.0f) ? -1e-3f : 1e-3f) : sqrtf_signed(evi);
    mul_v3_fl(eigen_vec[i], evi);
  }

  copy_m4_m3(r_mat, eigen_vec);
  copy_v3_v3(r_mat[3], center);
}

/**
 * Set r_space_transform so that best bbox of dst matches best bbox of src.
 */
void BKE_mesh_remap_find_best_match_from_mesh(const MVert *verts_dst,
                                              const int numverts_dst,
                                              Mesh *me_src,
                                              SpaceTransform *r_space_transform)
{
  /* Note that those are done so that we successively get actual mirror matrix
   * (by multiplication of columns). */
  const float mirrors[][3] = {
      {-1.0f, 1.0f, 1.0f}, /* -> -1,  1,  1 */
      {1.0f, -1.0f, 1.0f}, /* -> -1, -1,  1 */
      {1.0f, 1.0f, -1.0f}, /* -> -1, -1, -1 */
      {1.0f, -1.0f, 1.0f}, /* -> -1,  1, -1 */
      {-1.0f, 1.0f, 1.0f}, /* ->  1,  1, -1 */
      {1.0f, -1.0f, 1.0f}, /* ->  1, -1, -1 */
      {1.0f, 1.0f, -1.0f}, /* ->  1, -1,  1 */
      {0.0f, 0.0f, 0.0f},
  };
  const float(*mirr)[3];

  float mat_src[4][4], mat_dst[4][4], best_mat_dst[4][4];
  float best_match = FLT_MAX, match;

  const int numverts_src = me_src->totvert;
  float(*vcos_src)[3] = BKE_mesh_vertexCos_get(me_src, NULL);

  mesh_calc_eigen_matrix(NULL, (const float(*)[3])vcos_src, numverts_src, mat_src);
  mesh_calc_eigen_matrix(verts_dst, NULL, numverts_dst, mat_dst);

  BLI_space_transform_global_from_matrices(r_space_transform, mat_dst, mat_src);
  match = BKE_mesh_remap_calc_difference_from_mesh(
      r_space_transform, verts_dst, numverts_dst, me_src);
  best_match = match;
  copy_m4_m4(best_mat_dst, mat_dst);

  /* And now, we have to check the other sixth possible mirrored versions... */
  for (mirr = mirrors; (*mirr)[0]; mirr++) {
    mul_v3_fl(mat_dst[0], (*mirr)[0]);
    mul_v3_fl(mat_dst[1], (*mirr)[1]);
    mul_v3_fl(mat_dst[2], (*mirr)[2]);

    BLI_space_transform_global_from_matrices(r_space_transform, mat_dst, mat_src);
    match = BKE_mesh_remap_calc_difference_from_mesh(
        r_space_transform, verts_dst, numverts_dst, me_src);
    if (match < best_match) {
      best_match = match;
      copy_m4_m4(best_mat_dst, mat_dst);
    }
  }

  BLI_space_transform_global_from_matrices(r_space_transform, best_mat_dst, mat_src);

  MEM_freeN(vcos_src);
}

/** \} */

/** \name Mesh to mesh mapping
 * \{ */

void BKE_mesh_remap_calc_source_cddata_masks_from_map_modes(const int UNUSED(vert_mode),
                                                            const int UNUSED(edge_mode),
                                                            const int loop_mode,
                                                            const int UNUSED(poly_mode),
                                                            CustomData_MeshMasks *r_cddata_mask)
{
  /* vert, edge and poly mapping modes never need extra cddata from source object. */
  const bool need_lnors_src = (loop_mode & MREMAP_USE_LOOP) && (loop_mode & MREMAP_USE_NORMAL);
  const bool need_pnors_src = need_lnors_src ||
                              ((loop_mode & MREMAP_USE_POLY) && (loop_mode & MREMAP_USE_NORMAL));

  if (need_lnors_src) {
    r_cddata_mask->lmask |= CD_MASK_NORMAL;
  }
  if (need_pnors_src) {
    r_cddata_mask->pmask |= CD_MASK_NORMAL;
  }
}

void BKE_mesh_remap_init(MeshPairRemap *map, const int items_num)
{
  MemArena *mem = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

  BKE_mesh_remap_free(map);

  map->items = BLI_memarena_alloc(mem, sizeof(*map->items) * (size_t)items_num);
  map->items_num = items_num;

  map->mem = mem;
}

void BKE_mesh_remap_free(MeshPairRemap *map)
{
  if (map->mem) {
    BLI_memarena_free((MemArena *)map->mem);
  }

  map->items_num = 0;
  map->items = NULL;
  map->mem = NULL;
}

static void mesh_remap_item_define(MeshPairRemap *map,
                                   const int index,
                                   const float UNUSED(hit_dist),
                                   const int island,
                                   const int sources_num,
                                   const int *indices_src,
                                   const float *weights_src)
{
  MeshPairRemapItem *mapit = &map->items[index];
  MemArena *mem = map->mem;

  if (sources_num) {
    mapit->sources_num = sources_num;
    mapit->indices_src = BLI_memarena_alloc(mem,
                                            sizeof(*mapit->indices_src) * (size_t)sources_num);
    memcpy(mapit->indices_src, indices_src, sizeof(*mapit->indices_src) * (size_t)sources_num);
    mapit->weights_src = BLI_memarena_alloc(mem,
                                            sizeof(*mapit->weights_src) * (size_t)sources_num);
    memcpy(mapit->weights_src, weights_src, sizeof(*mapit->weights_src) * (size_t)sources_num);
  }
  else {
    mapit->sources_num = 0;
    mapit->indices_src = NULL;
    mapit->weights_src = NULL;
  }
  /* UNUSED */
  // mapit->hit_dist = hit_dist;
  mapit->island = island;
}

void BKE_mesh_remap_item_define_invalid(MeshPairRemap *map, const int index)
{
  mesh_remap_item_define(map, index, FLT_MAX, 0, 0, NULL, NULL);
}

static int mesh_remap_interp_poly_data_get(const MPoly *mp,
                                           MLoop *mloops,
                                           const float (*vcos_src)[3],
                                           const float point[3],
                                           size_t *buff_size,
                                           float (**vcos)[3],
                                           const bool use_loops,
                                           int **indices,
                                           float **weights,
                                           const bool do_weights,
                                           int *r_closest_index)
{
  MLoop *ml;
  float(*vco)[3];
  float ref_dist_sq = FLT_MAX;
  int *index;
  const int sources_num = mp->totloop;
  int i;

  if ((size_t)sources_num > *buff_size) {
    *buff_size = (size_t)sources_num;
    *vcos = MEM_reallocN(*vcos, sizeof(**vcos) * *buff_size);
    *indices = MEM_reallocN(*indices, sizeof(**indices) * *buff_size);
    if (do_weights) {
      *weights = MEM_reallocN(*weights, sizeof(**weights) * *buff_size);
    }
  }

  for (i = 0, ml = &mloops[mp->loopstart], vco = *vcos, index = *indices; i < sources_num;
       i++, ml++, vco++, index++) {
    *index = use_loops ? (int)mp->loopstart + i : (int)ml->v;
    copy_v3_v3(*vco, vcos_src[ml->v]);
    if (r_closest_index) {
      /* Find closest vert/loop in this case. */
      const float dist_sq = len_squared_v3v3(point, *vco);
      if (dist_sq < ref_dist_sq) {
        ref_dist_sq = dist_sq;
        *r_closest_index = *index;
      }
    }
  }

  if (do_weights) {
    interp_weights_poly_v3(*weights, *vcos, sources_num, point);
  }

  return sources_num;
}

/** Little helper when dealing with source islands */
typedef struct IslandResult {
  /** A factor, based on which best island for a given set of elements will be selected. */
  float factor;
  /** Index of the source. */
  int index_src;
  /** The actual hit distance. */
  float hit_dist;
  /** The hit point, if relevant. */
  float hit_point[3];
} IslandResult;

/**
 * \note About all bvh/raycasting stuff below:
 *
 * * We must use our ray radius as BVH epsilon too, else rays not hitting anything but
 *   'passing near' an item would be missed (since BVH handling would not detect them,
 *   'refining' callbacks won't be executed, even though they would return a valid hit).
 * * However, in 'islands' case where each hit gets a weight, 'precise' hits should have a better
 *   weight than 'approximate' hits.
 *   To address that, we simplify things with:
 *   * A first raycast with default, given rayradius;
 *   * If first one fails, we do more raycasting with bigger radius, but if hit is found
 *     it will get smaller weight.
 *
 *   This only concerns loops, currently (because of islands), and 'sampled' edges/polys norproj.
 */

/* At most n raycasts per 'real' ray. */
#define MREMAP_RAYCAST_APPROXIMATE_NR 3
/* Each approximated raycasts will have n times bigger radius than previous one. */
#define MREMAP_RAYCAST_APPROXIMATE_FAC 5.0f

/* min 16 rays/face, max 400. */
#define MREMAP_RAYCAST_TRI_SAMPLES_MIN 4
#define MREMAP_RAYCAST_TRI_SAMPLES_MAX 20

/* Will be enough in 99% of cases. */
#define MREMAP_DEFAULT_BUFSIZE 32

void BKE_mesh_remap_calc_verts_from_mesh(const int mode,
                                         const SpaceTransform *space_transform,
                                         const float max_dist,
                                         const float ray_radius,
                                         const MVert *verts_dst,
                                         const int numverts_dst,
                                         const bool UNUSED(dirty_nors_dst),
                                         Mesh *me_src,
                                         MeshPairRemap *r_map)
{
  const float full_weight = 1.0f;
  const float max_dist_sq = max_dist * max_dist;
  int i;

  BLI_assert(mode & MREMAP_MODE_VERT);

  BKE_mesh_remap_init(r_map, numverts_dst);

  if (mode == MREMAP_MODE_TOPOLOGY) {
    BLI_assert(numverts_dst == me_src->totvert);
    for (i = 0; i < numverts_dst; i++) {
      mesh_remap_item_define(r_map, i, FLT_MAX, 0, 1, &i, &full_weight);
    }
  }
  else {
    BVHTreeFromMesh treedata = {NULL};
    BVHTreeNearest nearest = {0};
    BVHTreeRayHit rayhit = {0};
    float hit_dist;
    float tmp_co[3], tmp_no[3];

    if (mode == MREMAP_MODE_VERT_NEAREST) {
      BKE_bvhtree_from_mesh_get(&treedata, me_src, BVHTREE_FROM_VERTS, 2);
      nearest.index = -1;

      for (i = 0; i < numverts_dst; i++) {
        copy_v3_v3(tmp_co, verts_dst[i].co);

        /* Convert the vertex to tree coordinates, if needed. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, tmp_co);
        }

        if (mesh_remap_bvhtree_query_nearest(
                &treedata, &nearest, tmp_co, max_dist_sq, &hit_dist)) {
          mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &nearest.index, &full_weight);
        }
        else {
          /* No source for this dest vertex! */
          BKE_mesh_remap_item_define_invalid(r_map, i);
        }
      }
    }
    else if (ELEM(mode, MREMAP_MODE_VERT_EDGE_NEAREST, MREMAP_MODE_VERT_EDGEINTERP_NEAREST)) {
      MEdge *edges_src = me_src->medge;
      float(*vcos_src)[3] = BKE_mesh_vertexCos_get(me_src, NULL);

      BKE_bvhtree_from_mesh_get(&treedata, me_src, BVHTREE_FROM_EDGES, 2);
      nearest.index = -1;

      for (i = 0; i < numverts_dst; i++) {
        copy_v3_v3(tmp_co, verts_dst[i].co);

        /* Convert the vertex to tree coordinates, if needed. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, tmp_co);
        }

        if (mesh_remap_bvhtree_query_nearest(
                &treedata, &nearest, tmp_co, max_dist_sq, &hit_dist)) {
          MEdge *me = &edges_src[nearest.index];
          const float *v1cos = vcos_src[me->v1];
          const float *v2cos = vcos_src[me->v2];

          if (mode == MREMAP_MODE_VERT_EDGE_NEAREST) {
            const float dist_v1 = len_squared_v3v3(tmp_co, v1cos);
            const float dist_v2 = len_squared_v3v3(tmp_co, v2cos);
            const int index = (int)((dist_v1 > dist_v2) ? me->v2 : me->v1);
            mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &index, &full_weight);
          }
          else if (mode == MREMAP_MODE_VERT_EDGEINTERP_NEAREST) {
            int indices[2];
            float weights[2];

            indices[0] = (int)me->v1;
            indices[1] = (int)me->v2;

            /* Weight is inverse of point factor here... */
            weights[0] = line_point_factor_v3(tmp_co, v2cos, v1cos);
            CLAMP(weights[0], 0.0f, 1.0f);
            weights[1] = 1.0f - weights[0];

            mesh_remap_item_define(r_map, i, hit_dist, 0, 2, indices, weights);
          }
        }
        else {
          /* No source for this dest vertex! */
          BKE_mesh_remap_item_define_invalid(r_map, i);
        }
      }

      MEM_freeN(vcos_src);
    }
    else if (ELEM(mode,
                  MREMAP_MODE_VERT_POLY_NEAREST,
                  MREMAP_MODE_VERT_POLYINTERP_NEAREST,
                  MREMAP_MODE_VERT_POLYINTERP_VNORPROJ)) {
      MPoly *polys_src = me_src->mpoly;
      MLoop *loops_src = me_src->mloop;
      float(*vcos_src)[3] = BKE_mesh_vertexCos_get(me_src, NULL);

      size_t tmp_buff_size = MREMAP_DEFAULT_BUFSIZE;
      float(*vcos)[3] = MEM_mallocN(sizeof(*vcos) * tmp_buff_size, __func__);
      int *indices = MEM_mallocN(sizeof(*indices) * tmp_buff_size, __func__);
      float *weights = MEM_mallocN(sizeof(*weights) * tmp_buff_size, __func__);

      BKE_bvhtree_from_mesh_get(&treedata, me_src, BVHTREE_FROM_LOOPTRI, 2);

      if (mode == MREMAP_MODE_VERT_POLYINTERP_VNORPROJ) {
        for (i = 0; i < numverts_dst; i++) {
          copy_v3_v3(tmp_co, verts_dst[i].co);
          normal_short_to_float_v3(tmp_no, verts_dst[i].no);

          /* Convert the vertex to tree coordinates, if needed. */
          if (space_transform) {
            BLI_space_transform_apply(space_transform, tmp_co);
            BLI_space_transform_apply_normal(space_transform, tmp_no);
          }

          if (mesh_remap_bvhtree_query_raycast(
                  &treedata, &rayhit, tmp_co, tmp_no, ray_radius, max_dist, &hit_dist)) {
            const MLoopTri *lt = &treedata.looptri[rayhit.index];
            MPoly *mp_src = &polys_src[lt->poly];
            const int sources_num = mesh_remap_interp_poly_data_get(mp_src,
                                                                    loops_src,
                                                                    (const float(*)[3])vcos_src,
                                                                    rayhit.co,
                                                                    &tmp_buff_size,
                                                                    &vcos,
                                                                    false,
                                                                    &indices,
                                                                    &weights,
                                                                    true,
                                                                    NULL);

            mesh_remap_item_define(r_map, i, hit_dist, 0, sources_num, indices, weights);
          }
          else {
            /* No source for this dest vertex! */
            BKE_mesh_remap_item_define_invalid(r_map, i);
          }
        }
      }
      else {
        nearest.index = -1;

        for (i = 0; i < numverts_dst; i++) {
          copy_v3_v3(tmp_co, verts_dst[i].co);

          /* Convert the vertex to tree coordinates, if needed. */
          if (space_transform) {
            BLI_space_transform_apply(space_transform, tmp_co);
          }

          if (mesh_remap_bvhtree_query_nearest(
                  &treedata, &nearest, tmp_co, max_dist_sq, &hit_dist)) {
            const MLoopTri *lt = &treedata.looptri[nearest.index];
            MPoly *mp = &polys_src[lt->poly];

            if (mode == MREMAP_MODE_VERT_POLY_NEAREST) {
              int index;
              mesh_remap_interp_poly_data_get(mp,
                                              loops_src,
                                              (const float(*)[3])vcos_src,
                                              nearest.co,
                                              &tmp_buff_size,
                                              &vcos,
                                              false,
                                              &indices,
                                              &weights,
                                              false,
                                              &index);

              mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &index, &full_weight);
            }
            else if (mode == MREMAP_MODE_VERT_POLYINTERP_NEAREST) {
              const int sources_num = mesh_remap_interp_poly_data_get(mp,
                                                                      loops_src,
                                                                      (const float(*)[3])vcos_src,
                                                                      nearest.co,
                                                                      &tmp_buff_size,
                                                                      &vcos,
                                                                      false,
                                                                      &indices,
                                                                      &weights,
                                                                      true,
                                                                      NULL);

              mesh_remap_item_define(r_map, i, hit_dist, 0, sources_num, indices, weights);
            }
          }
          else {
            /* No source for this dest vertex! */
            BKE_mesh_remap_item_define_invalid(r_map, i);
          }
        }
      }

      MEM_freeN(vcos_src);
      MEM_freeN(vcos);
      MEM_freeN(indices);
      MEM_freeN(weights);
    }
    else {
      CLOG_WARN(&LOG, "Unsupported mesh-to-mesh vertex mapping mode (%d)!", mode);
      memset(r_map->items, 0, sizeof(*r_map->items) * (size_t)numverts_dst);
    }

    free_bvhtree_from_mesh(&treedata);
  }
}

void BKE_mesh_remap_calc_edges_from_mesh(const int mode,
                                         const SpaceTransform *space_transform,
                                         const float max_dist,
                                         const float ray_radius,
                                         const MVert *verts_dst,
                                         const int numverts_dst,
                                         const MEdge *edges_dst,
                                         const int numedges_dst,
                                         const bool UNUSED(dirty_nors_dst),
                                         Mesh *me_src,
                                         MeshPairRemap *r_map)
{
  const float full_weight = 1.0f;
  const float max_dist_sq = max_dist * max_dist;
  int i;

  BLI_assert(mode & MREMAP_MODE_EDGE);

  BKE_mesh_remap_init(r_map, numedges_dst);

  if (mode == MREMAP_MODE_TOPOLOGY) {
    BLI_assert(numedges_dst == me_src->totedge);
    for (i = 0; i < numedges_dst; i++) {
      mesh_remap_item_define(r_map, i, FLT_MAX, 0, 1, &i, &full_weight);
    }
  }
  else {
    BVHTreeFromMesh treedata = {NULL};
    BVHTreeNearest nearest = {0};
    BVHTreeRayHit rayhit = {0};
    float hit_dist;
    float tmp_co[3], tmp_no[3];

    if (mode == MREMAP_MODE_EDGE_VERT_NEAREST) {
      const int num_verts_src = me_src->totvert;
      const int num_edges_src = me_src->totedge;
      MEdge *edges_src = me_src->medge;
      float(*vcos_src)[3] = BKE_mesh_vertexCos_get(me_src, NULL);

      MeshElemMap *vert_to_edge_src_map;
      int *vert_to_edge_src_map_mem;

      struct {
        float hit_dist;
        int index;
      } *v_dst_to_src_map = MEM_mallocN(sizeof(*v_dst_to_src_map) * (size_t)numverts_dst,
                                        __func__);

      for (i = 0; i < numverts_dst; i++) {
        v_dst_to_src_map[i].hit_dist = -1.0f;
      }

      BKE_mesh_vert_edge_map_create(&vert_to_edge_src_map,
                                    &vert_to_edge_src_map_mem,
                                    edges_src,
                                    num_verts_src,
                                    num_edges_src);

      BKE_bvhtree_from_mesh_get(&treedata, me_src, BVHTREE_FROM_VERTS, 2);
      nearest.index = -1;

      for (i = 0; i < numedges_dst; i++) {
        const MEdge *e_dst = &edges_dst[i];
        float best_totdist = FLT_MAX;
        int best_eidx_src = -1;
        int j = 2;

        while (j--) {
          const unsigned int vidx_dst = j ? e_dst->v1 : e_dst->v2;

          /* Compute closest verts only once! */
          if (v_dst_to_src_map[vidx_dst].hit_dist == -1.0f) {
            copy_v3_v3(tmp_co, verts_dst[vidx_dst].co);

            /* Convert the vertex to tree coordinates, if needed. */
            if (space_transform) {
              BLI_space_transform_apply(space_transform, tmp_co);
            }

            if (mesh_remap_bvhtree_query_nearest(
                    &treedata, &nearest, tmp_co, max_dist_sq, &hit_dist)) {
              v_dst_to_src_map[vidx_dst].hit_dist = hit_dist;
              v_dst_to_src_map[vidx_dst].index = nearest.index;
            }
            else {
              /* No source for this dest vert! */
              v_dst_to_src_map[vidx_dst].hit_dist = FLT_MAX;
            }
          }
        }

        /* Now, check all source edges of closest sources vertices,
         * and select the one giving the smallest total verts-to-verts distance. */
        for (j = 2; j--;) {
          const unsigned int vidx_dst = j ? e_dst->v1 : e_dst->v2;
          const float first_dist = v_dst_to_src_map[vidx_dst].hit_dist;
          const int vidx_src = v_dst_to_src_map[vidx_dst].index;
          int *eidx_src, k;

          if (vidx_src < 0) {
            continue;
          }

          eidx_src = vert_to_edge_src_map[vidx_src].indices;
          k = vert_to_edge_src_map[vidx_src].count;

          for (; k--; eidx_src++) {
            MEdge *e_src = &edges_src[*eidx_src];
            const float *other_co_src = vcos_src[BKE_mesh_edge_other_vert(e_src, vidx_src)];
            const float *other_co_dst =
                verts_dst[BKE_mesh_edge_other_vert(e_dst, (int)vidx_dst)].co;
            const float totdist = first_dist + len_v3v3(other_co_src, other_co_dst);

            if (totdist < best_totdist) {
              best_totdist = totdist;
              best_eidx_src = *eidx_src;
            }
          }
        }

        if (best_eidx_src >= 0) {
          const float *co1_src = vcos_src[edges_src[best_eidx_src].v1];
          const float *co2_src = vcos_src[edges_src[best_eidx_src].v2];
          const float *co1_dst = verts_dst[e_dst->v1].co;
          const float *co2_dst = verts_dst[e_dst->v2].co;
          float co_src[3], co_dst[3];

          /* TODO: would need an isect_seg_seg_v3(), actually! */
          const int isect_type = isect_line_line_v3(
              co1_src, co2_src, co1_dst, co2_dst, co_src, co_dst);
          if (isect_type != 0) {
            const float fac_src = line_point_factor_v3(co_src, co1_src, co2_src);
            const float fac_dst = line_point_factor_v3(co_dst, co1_dst, co2_dst);
            if (fac_src < 0.0f) {
              copy_v3_v3(co_src, co1_src);
            }
            else if (fac_src > 1.0f) {
              copy_v3_v3(co_src, co2_src);
            }
            if (fac_dst < 0.0f) {
              copy_v3_v3(co_dst, co1_dst);
            }
            else if (fac_dst > 1.0f) {
              copy_v3_v3(co_dst, co2_dst);
            }
          }
          hit_dist = len_v3v3(co_dst, co_src);
          mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &best_eidx_src, &full_weight);
        }
        else {
          /* No source for this dest edge! */
          BKE_mesh_remap_item_define_invalid(r_map, i);
        }
      }

      MEM_freeN(vcos_src);
      MEM_freeN(v_dst_to_src_map);
      MEM_freeN(vert_to_edge_src_map);
      MEM_freeN(vert_to_edge_src_map_mem);
    }
    else if (mode == MREMAP_MODE_EDGE_NEAREST) {
      BKE_bvhtree_from_mesh_get(&treedata, me_src, BVHTREE_FROM_EDGES, 2);
      nearest.index = -1;

      for (i = 0; i < numedges_dst; i++) {
        interp_v3_v3v3(tmp_co, verts_dst[edges_dst[i].v1].co, verts_dst[edges_dst[i].v2].co, 0.5f);

        /* Convert the vertex to tree coordinates, if needed. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, tmp_co);
        }

        if (mesh_remap_bvhtree_query_nearest(
                &treedata, &nearest, tmp_co, max_dist_sq, &hit_dist)) {
          mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &nearest.index, &full_weight);
        }
        else {
          /* No source for this dest edge! */
          BKE_mesh_remap_item_define_invalid(r_map, i);
        }
      }
    }
    else if (mode == MREMAP_MODE_EDGE_POLY_NEAREST) {
      MEdge *edges_src = me_src->medge;
      MPoly *polys_src = me_src->mpoly;
      MLoop *loops_src = me_src->mloop;
      float(*vcos_src)[3] = BKE_mesh_vertexCos_get(me_src, NULL);

      BKE_bvhtree_from_mesh_get(&treedata, me_src, BVHTREE_FROM_LOOPTRI, 2);

      for (i = 0; i < numedges_dst; i++) {
        interp_v3_v3v3(tmp_co, verts_dst[edges_dst[i].v1].co, verts_dst[edges_dst[i].v2].co, 0.5f);

        /* Convert the vertex to tree coordinates, if needed. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, tmp_co);
        }

        if (mesh_remap_bvhtree_query_nearest(
                &treedata, &nearest, tmp_co, max_dist_sq, &hit_dist)) {
          const MLoopTri *lt = &treedata.looptri[nearest.index];
          MPoly *mp_src = &polys_src[lt->poly];
          MLoop *ml_src = &loops_src[mp_src->loopstart];
          int nloops = mp_src->totloop;
          float best_dist_sq = FLT_MAX;
          int best_eidx_src = -1;

          for (; nloops--; ml_src++) {
            MEdge *med_src = &edges_src[ml_src->e];
            float *co1_src = vcos_src[med_src->v1];
            float *co2_src = vcos_src[med_src->v2];
            float co_src[3];
            float dist_sq;

            interp_v3_v3v3(co_src, co1_src, co2_src, 0.5f);
            dist_sq = len_squared_v3v3(tmp_co, co_src);
            if (dist_sq < best_dist_sq) {
              best_dist_sq = dist_sq;
              best_eidx_src = (int)ml_src->e;
            }
          }
          if (best_eidx_src >= 0) {
            mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &best_eidx_src, &full_weight);
          }
        }
        else {
          /* No source for this dest edge! */
          BKE_mesh_remap_item_define_invalid(r_map, i);
        }
      }

      MEM_freeN(vcos_src);
    }
    else if (mode == MREMAP_MODE_EDGE_EDGEINTERP_VNORPROJ) {
      const int num_rays_min = 5, num_rays_max = 100;
      const int numedges_src = me_src->totedge;

      /* Subtleness - this one we can allocate only max number of cast rays per edges! */
      int *indices = MEM_mallocN(sizeof(*indices) * (size_t)min_ii(numedges_src, num_rays_max),
                                 __func__);
      /* Here it's simpler to just allocate for all edges :/ */
      float *weights = MEM_mallocN(sizeof(*weights) * (size_t)numedges_src, __func__);

      BKE_bvhtree_from_mesh_get(&treedata, me_src, BVHTREE_FROM_EDGES, 2);

      for (i = 0; i < numedges_dst; i++) {
        /* For each dst edge, we sample some rays from it (interpolated from its vertices)
         * and use their hits to interpolate from source edges. */
        const MEdge *me = &edges_dst[i];
        float v1_co[3], v2_co[3];
        float v1_no[3], v2_no[3];

        int grid_size;
        float edge_dst_len;
        float grid_step;

        float totweights = 0.0f;
        float hit_dist_accum = 0.0f;
        int sources_num = 0;
        int j;

        copy_v3_v3(v1_co, verts_dst[me->v1].co);
        copy_v3_v3(v2_co, verts_dst[me->v2].co);

        normal_short_to_float_v3(v1_no, verts_dst[me->v1].no);
        normal_short_to_float_v3(v2_no, verts_dst[me->v2].no);

        /* We do our transform here, allows to interpolate from normals already in src space. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, v1_co);
          BLI_space_transform_apply(space_transform, v2_co);
          BLI_space_transform_apply_normal(space_transform, v1_no);
          BLI_space_transform_apply_normal(space_transform, v2_no);
        }

        copy_vn_fl(weights, (int)numedges_src, 0.0f);

        /* We adjust our ray-casting grid to ray_radius (the smaller, the more rays are cast),
         * with lower/upper bounds. */
        edge_dst_len = len_v3v3(v1_co, v2_co);

        grid_size = (int)((edge_dst_len / ray_radius) + 0.5f);
        CLAMP(grid_size, num_rays_min, num_rays_max); /* min 5 rays/edge, max 100. */

        grid_step = 1.0f /
                    (float)grid_size; /* Not actual distance here, rather an interp fac... */

        /* And now we can cast all our rays, and see what we get! */
        for (j = 0; j < grid_size; j++) {
          const float fac = grid_step * (float)j;

          int n = (ray_radius > 0.0f) ? MREMAP_RAYCAST_APPROXIMATE_NR : 1;
          float w = 1.0f;

          interp_v3_v3v3(tmp_co, v1_co, v2_co, fac);
          interp_v3_v3v3_slerp_safe(tmp_no, v1_no, v2_no, fac);

          while (n--) {
            if (mesh_remap_bvhtree_query_raycast(
                    &treedata, &rayhit, tmp_co, tmp_no, ray_radius / w, max_dist, &hit_dist)) {
              weights[rayhit.index] += w;
              totweights += w;
              hit_dist_accum += hit_dist;
              break;
            }
            /* Next iteration will get bigger radius but smaller weight! */
            w /= MREMAP_RAYCAST_APPROXIMATE_FAC;
          }
        }
        /* A sampling is valid (as in, its result can be considered as valid sources)
         * only if at least half of the rays found a source! */
        if (totweights > ((float)grid_size / 2.0f)) {
          for (j = 0; j < (int)numedges_src; j++) {
            if (!weights[j]) {
              continue;
            }
            /* Note: sources_num is always <= j! */
            weights[sources_num] = weights[j] / totweights;
            indices[sources_num] = j;
            sources_num++;
          }
          mesh_remap_item_define(
              r_map, i, hit_dist_accum / totweights, 0, sources_num, indices, weights);
        }
        else {
          /* No source for this dest edge! */
          BKE_mesh_remap_item_define_invalid(r_map, i);
        }
      }

      MEM_freeN(indices);
      MEM_freeN(weights);
    }
    else {
      CLOG_WARN(&LOG, "Unsupported mesh-to-mesh edge mapping mode (%d)!", mode);
      memset(r_map->items, 0, sizeof(*r_map->items) * (size_t)numedges_dst);
    }

    free_bvhtree_from_mesh(&treedata);
  }
}

#define POLY_UNSET 0
#define POLY_CENTER_INIT 1
#define POLY_COMPLETE 2

static void mesh_island_to_astar_graph_edge_process(MeshIslandStore *islands,
                                                    const int island_index,
                                                    BLI_AStarGraph *as_graph,
                                                    MVert *verts,
                                                    MPoly *polys,
                                                    MLoop *loops,
                                                    const int edge_idx,
                                                    BLI_bitmap *done_edges,
                                                    MeshElemMap *edge_to_poly_map,
                                                    const bool is_edge_innercut,
                                                    int *poly_island_index_map,
                                                    float (*poly_centers)[3],
                                                    unsigned char *poly_status)
{
  int *poly_island_indices = BLI_array_alloca(poly_island_indices,
                                              (size_t)edge_to_poly_map[edge_idx].count);
  int i, j;

  for (i = 0; i < edge_to_poly_map[edge_idx].count; i++) {
    const int pidx = edge_to_poly_map[edge_idx].indices[i];
    MPoly *mp = &polys[pidx];
    const int pidx_isld = islands ? poly_island_index_map[pidx] : pidx;
    void *custom_data = is_edge_innercut ? POINTER_FROM_INT(edge_idx) : POINTER_FROM_INT(-1);

    if (UNLIKELY(islands && (islands->items_to_islands[mp->loopstart] != island_index))) {
      /* poly not in current island, happens with border edges... */
      poly_island_indices[i] = -1;
      continue;
    }

    if (poly_status[pidx_isld] == POLY_COMPLETE) {
      poly_island_indices[i] = pidx_isld;
      continue;
    }

    if (poly_status[pidx_isld] == POLY_UNSET) {
      BKE_mesh_calc_poly_center(mp, &loops[mp->loopstart], verts, poly_centers[pidx_isld]);
      BLI_astar_node_init(as_graph, pidx_isld, poly_centers[pidx_isld]);
      poly_status[pidx_isld] = POLY_CENTER_INIT;
    }

    for (j = i; j--;) {
      float dist_cost;
      const int pidx_isld_other = poly_island_indices[j];

      if (pidx_isld_other == -1 || poly_status[pidx_isld_other] == POLY_COMPLETE) {
        /* If the other poly is complete, that link has already been added! */
        continue;
      }
      dist_cost = len_v3v3(poly_centers[pidx_isld_other], poly_centers[pidx_isld]);
      BLI_astar_node_link_add(as_graph, pidx_isld_other, pidx_isld, dist_cost, custom_data);
    }

    poly_island_indices[i] = pidx_isld;
  }

  BLI_BITMAP_ENABLE(done_edges, edge_idx);
}

static void mesh_island_to_astar_graph(MeshIslandStore *islands,
                                       const int island_index,
                                       MVert *verts,
                                       MeshElemMap *edge_to_poly_map,
                                       const int numedges,
                                       MLoop *loops,
                                       MPoly *polys,
                                       const int numpolys,
                                       BLI_AStarGraph *r_as_graph)
{
  MeshElemMap *island_poly_map = islands ? islands->islands[island_index] : NULL;
  MeshElemMap *island_einnercut_map = islands ? islands->innercuts[island_index] : NULL;

  int *poly_island_index_map = NULL;
  BLI_bitmap *done_edges = BLI_BITMAP_NEW(numedges, __func__);

  const int node_num = islands ? island_poly_map->count : numpolys;
  unsigned char *poly_status = MEM_callocN(sizeof(*poly_status) * (size_t)node_num, __func__);
  float(*poly_centers)[3];

  int pidx_isld;
  int i;

  BLI_astar_graph_init(r_as_graph, node_num, NULL);
  /* poly_centers is owned by graph memarena. */
  poly_centers = BLI_memarena_calloc(r_as_graph->mem, sizeof(*poly_centers) * (size_t)node_num);

  if (islands) {
    /* poly_island_index_map is owned by graph memarena. */
    poly_island_index_map = BLI_memarena_calloc(r_as_graph->mem,
                                                sizeof(*poly_island_index_map) * (size_t)numpolys);
    for (i = island_poly_map->count; i--;) {
      poly_island_index_map[island_poly_map->indices[i]] = i;
    }

    r_as_graph->custom_data = poly_island_index_map;

    for (i = island_einnercut_map->count; i--;) {
      mesh_island_to_astar_graph_edge_process(islands,
                                              island_index,
                                              r_as_graph,
                                              verts,
                                              polys,
                                              loops,
                                              island_einnercut_map->indices[i],
                                              done_edges,
                                              edge_to_poly_map,
                                              true,
                                              poly_island_index_map,
                                              poly_centers,
                                              poly_status);
    }
  }

  for (pidx_isld = node_num; pidx_isld--;) {
    const int pidx = islands ? island_poly_map->indices[pidx_isld] : pidx_isld;
    MPoly *mp = &polys[pidx];
    int pl_idx, l_idx;

    if (poly_status[pidx_isld] == POLY_COMPLETE) {
      continue;
    }

    for (pl_idx = 0, l_idx = mp->loopstart; pl_idx < mp->totloop; pl_idx++, l_idx++) {
      MLoop *ml = &loops[l_idx];

      if (BLI_BITMAP_TEST(done_edges, ml->e)) {
        continue;
      }

      mesh_island_to_astar_graph_edge_process(islands,
                                              island_index,
                                              r_as_graph,
                                              verts,
                                              polys,
                                              loops,
                                              (int)ml->e,
                                              done_edges,
                                              edge_to_poly_map,
                                              false,
                                              poly_island_index_map,
                                              poly_centers,
                                              poly_status);
    }
    poly_status[pidx_isld] = POLY_COMPLETE;
  }

  MEM_freeN(done_edges);
  MEM_freeN(poly_status);
}

#undef POLY_UNSET
#undef POLY_CENTER_INIT
#undef POLY_COMPLETE

/* Our 'f_cost' callback func, to find shortest poly-path between two remapped-loops.
 * Note we do not want to make innercuts 'walls' here,
 * just detect when the shortest path goes by those. */
static float mesh_remap_calc_loops_astar_f_cost(BLI_AStarGraph *as_graph,
                                                BLI_AStarSolution *as_solution,
                                                BLI_AStarGNLink *link,
                                                const int node_idx_curr,
                                                const int node_idx_next,
                                                const int node_idx_dst)
{
  float *co_next, *co_dest;

  if (link && (POINTER_AS_INT(link->custom_data) != -1)) {
    /* An innercut edge... We tag our solution as potentially crossing innercuts.
     * Note it might not be the case in the end (AStar will explore around optimal path), but helps
     * trimming off some processing later... */
    if (!POINTER_AS_INT(as_solution->custom_data)) {
      as_solution->custom_data = POINTER_FROM_INT(true);
    }
  }

  /* Our heuristic part of current f_cost is distance from next node to destination one.
   * It is guaranteed to be less than (or equal to)
   * actual shortest poly-path between next node and destination one. */
  co_next = (float *)as_graph->nodes[node_idx_next].custom_data;
  co_dest = (float *)as_graph->nodes[node_idx_dst].custom_data;
  return (link ? (as_solution->g_costs[node_idx_curr] + link->cost) : 0.0f) +
         len_v3v3(co_next, co_dest);
}

#define ASTAR_STEPS_MAX 64

void BKE_mesh_remap_calc_loops_from_mesh(const int mode,
                                         const SpaceTransform *space_transform,
                                         const float max_dist,
                                         const float ray_radius,
                                         MVert *verts_dst,
                                         const int numverts_dst,
                                         MEdge *edges_dst,
                                         const int numedges_dst,
                                         MLoop *loops_dst,
                                         const int numloops_dst,
                                         MPoly *polys_dst,
                                         const int numpolys_dst,
                                         CustomData *ldata_dst,
                                         CustomData *pdata_dst,
                                         const bool use_split_nors_dst,
                                         const float split_angle_dst,
                                         const bool dirty_nors_dst,
                                         Mesh *me_src,
                                         MeshRemapIslandsCalc gen_islands_src,
                                         const float islands_precision_src,
                                         MeshPairRemap *r_map)
{
  const float full_weight = 1.0f;
  const float max_dist_sq = max_dist * max_dist;

  int i;

  BLI_assert(mode & MREMAP_MODE_LOOP);
  BLI_assert((islands_precision_src >= 0.0f) && (islands_precision_src <= 1.0f));

  BKE_mesh_remap_init(r_map, numloops_dst);

  if (mode == MREMAP_MODE_TOPOLOGY) {
    /* In topology mapping, we assume meshes are identical, islands included! */
    BLI_assert(numloops_dst == me_src->totloop);
    for (i = 0; i < numloops_dst; i++) {
      mesh_remap_item_define(r_map, i, FLT_MAX, 0, 1, &i, &full_weight);
    }
  }
  else {
    BVHTreeFromMesh *treedata = NULL;
    BVHTreeNearest nearest = {0};
    BVHTreeRayHit rayhit = {0};
    int num_trees = 0;
    float hit_dist;
    float tmp_co[3], tmp_no[3];

    const bool use_from_vert = (mode & MREMAP_USE_VERT);

    MeshIslandStore island_store = {0};
    bool use_islands = false;

    BLI_AStarGraph *as_graphdata = NULL;
    BLI_AStarSolution as_solution = {0};
    const int isld_steps_src = (islands_precision_src ?
                                    max_ii((int)(ASTAR_STEPS_MAX * islands_precision_src + 0.499f),
                                           1) :
                                    0);

    float(*poly_nors_src)[3] = NULL;
    float(*loop_nors_src)[3] = NULL;
    float(*poly_nors_dst)[3] = NULL;
    float(*loop_nors_dst)[3] = NULL;

    float(*poly_cents_src)[3] = NULL;

    MeshElemMap *vert_to_loop_map_src = NULL;
    int *vert_to_loop_map_src_buff = NULL;
    MeshElemMap *vert_to_poly_map_src = NULL;
    int *vert_to_poly_map_src_buff = NULL;
    MeshElemMap *edge_to_poly_map_src = NULL;
    int *edge_to_poly_map_src_buff = NULL;
    MeshElemMap *poly_to_looptri_map_src = NULL;
    int *poly_to_looptri_map_src_buff = NULL;

    /* Unlike above, those are one-to-one mappings, simpler! */
    int *loop_to_poly_map_src = NULL;

    MVert *verts_src = me_src->mvert;
    const int num_verts_src = me_src->totvert;
    float(*vcos_src)[3] = NULL;
    MEdge *edges_src = me_src->medge;
    const int num_edges_src = me_src->totedge;
    MLoop *loops_src = me_src->mloop;
    const int num_loops_src = me_src->totloop;
    MPoly *polys_src = me_src->mpoly;
    const int num_polys_src = me_src->totpoly;
    const MLoopTri *looptri_src = NULL;
    int num_looptri_src = 0;

    size_t buff_size_interp = MREMAP_DEFAULT_BUFSIZE;
    float(*vcos_interp)[3] = NULL;
    int *indices_interp = NULL;
    float *weights_interp = NULL;

    MLoop *ml_src, *ml_dst;
    MPoly *mp_src, *mp_dst;
    int tindex, pidx_dst, lidx_dst, plidx_dst, pidx_src, lidx_src, plidx_src;

    IslandResult **islands_res;
    size_t islands_res_buff_size = MREMAP_DEFAULT_BUFSIZE;

    if (!use_from_vert) {
      vcos_src = BKE_mesh_vertexCos_get(me_src, NULL);

      vcos_interp = MEM_mallocN(sizeof(*vcos_interp) * buff_size_interp, __func__);
      indices_interp = MEM_mallocN(sizeof(*indices_interp) * buff_size_interp, __func__);
      weights_interp = MEM_mallocN(sizeof(*weights_interp) * buff_size_interp, __func__);
    }

    {
      const bool need_lnors_src = (mode & MREMAP_USE_LOOP) && (mode & MREMAP_USE_NORMAL);
      const bool need_lnors_dst = need_lnors_src || (mode & MREMAP_USE_NORPROJ);
      const bool need_pnors_src = need_lnors_src ||
                                  ((mode & MREMAP_USE_POLY) && (mode & MREMAP_USE_NORMAL));
      const bool need_pnors_dst = need_lnors_dst || need_pnors_src;

      if (need_pnors_dst) {
        /* Cache poly nors into a temp CDLayer. */
        poly_nors_dst = CustomData_get_layer(pdata_dst, CD_NORMAL);
        const bool do_poly_nors_dst = (poly_nors_dst == NULL);
        if (!poly_nors_dst) {
          poly_nors_dst = CustomData_add_layer(
              pdata_dst, CD_NORMAL, CD_CALLOC, NULL, numpolys_dst);
          CustomData_set_layer_flag(pdata_dst, CD_NORMAL, CD_FLAG_TEMPORARY);
        }
        if (dirty_nors_dst || do_poly_nors_dst) {
          BKE_mesh_calc_normals_poly(verts_dst,
                                     NULL,
                                     numverts_dst,
                                     loops_dst,
                                     polys_dst,
                                     numloops_dst,
                                     numpolys_dst,
                                     poly_nors_dst,
                                     true);
        }
      }
      if (need_lnors_dst) {
        short(*custom_nors_dst)[2] = CustomData_get_layer(ldata_dst, CD_CUSTOMLOOPNORMAL);

        /* Cache poly nors into a temp CDLayer. */
        loop_nors_dst = CustomData_get_layer(ldata_dst, CD_NORMAL);
        const bool do_loop_nors_dst = (loop_nors_dst == NULL);
        if (!loop_nors_dst) {
          loop_nors_dst = CustomData_add_layer(
              ldata_dst, CD_NORMAL, CD_CALLOC, NULL, numloops_dst);
          CustomData_set_layer_flag(ldata_dst, CD_NORMAL, CD_FLAG_TEMPORARY);
        }
        if (dirty_nors_dst || do_loop_nors_dst) {
          BKE_mesh_normals_loop_split(verts_dst,
                                      numverts_dst,
                                      edges_dst,
                                      numedges_dst,
                                      loops_dst,
                                      loop_nors_dst,
                                      numloops_dst,
                                      polys_dst,
                                      (const float(*)[3])poly_nors_dst,
                                      numpolys_dst,
                                      use_split_nors_dst,
                                      split_angle_dst,
                                      NULL,
                                      custom_nors_dst,
                                      NULL);
        }
      }
      if (need_pnors_src || need_lnors_src) {
        if (need_pnors_src) {
          poly_nors_src = CustomData_get_layer(&me_src->pdata, CD_NORMAL);
          BLI_assert(poly_nors_src != NULL);
        }
        if (need_lnors_src) {
          loop_nors_src = CustomData_get_layer(&me_src->ldata, CD_NORMAL);
          BLI_assert(loop_nors_src != NULL);
        }
      }
    }

    if (use_from_vert) {
      BKE_mesh_vert_loop_map_create(&vert_to_loop_map_src,
                                    &vert_to_loop_map_src_buff,
                                    polys_src,
                                    loops_src,
                                    num_verts_src,
                                    num_polys_src,
                                    num_loops_src);
      if (mode & MREMAP_USE_POLY) {
        BKE_mesh_vert_poly_map_create(&vert_to_poly_map_src,
                                      &vert_to_poly_map_src_buff,
                                      polys_src,
                                      loops_src,
                                      num_verts_src,
                                      num_polys_src,
                                      num_loops_src);
      }
    }

    /* Needed for islands (or plain mesh) to AStar graph conversion. */
    BKE_mesh_edge_poly_map_create(&edge_to_poly_map_src,
                                  &edge_to_poly_map_src_buff,
                                  edges_src,
                                  num_edges_src,
                                  polys_src,
                                  num_polys_src,
                                  loops_src,
                                  num_loops_src);
    if (use_from_vert) {
      loop_to_poly_map_src = MEM_mallocN(sizeof(*loop_to_poly_map_src) * (size_t)num_loops_src,
                                         __func__);
      poly_cents_src = MEM_mallocN(sizeof(*poly_cents_src) * (size_t)num_polys_src, __func__);
      for (pidx_src = 0, mp_src = polys_src; pidx_src < num_polys_src; pidx_src++, mp_src++) {
        ml_src = &loops_src[mp_src->loopstart];
        for (plidx_src = 0, lidx_src = mp_src->loopstart; plidx_src < mp_src->totloop;
             plidx_src++, lidx_src++) {
          loop_to_poly_map_src[lidx_src] = pidx_src;
        }
        BKE_mesh_calc_poly_center(mp_src, ml_src, verts_src, poly_cents_src[pidx_src]);
      }
    }

    /* Island makes things slightly more complex here.
     * Basically, we:
     *     * Make one treedata for each island's elements.
     *     * Check all loops of a same dest poly against all treedata.
     *     * Choose the island's elements giving the best results.
     */

    /* First, generate the islands, if possible. */
    if (gen_islands_src) {
      use_islands = gen_islands_src(verts_src,
                                    num_verts_src,
                                    edges_src,
                                    num_edges_src,
                                    polys_src,
                                    num_polys_src,
                                    loops_src,
                                    num_loops_src,
                                    &island_store);

      num_trees = use_islands ? island_store.islands_num : 1;
      treedata = MEM_callocN(sizeof(*treedata) * (size_t)num_trees, __func__);
      if (isld_steps_src) {
        as_graphdata = MEM_callocN(sizeof(*as_graphdata) * (size_t)num_trees, __func__);
      }

      if (use_islands) {
        /* We expect our islands to contain poly indices, with edge indices of 'inner cuts',
         * and a mapping loops -> islands indices.
         * This implies all loops of a same poly are in the same island. */
        BLI_assert((island_store.item_type == MISLAND_TYPE_LOOP) &&
                   (island_store.island_type == MISLAND_TYPE_POLY) &&
                   (island_store.innercut_type == MISLAND_TYPE_EDGE));
      }
    }
    else {
      num_trees = 1;
      treedata = MEM_callocN(sizeof(*treedata), __func__);
      if (isld_steps_src) {
        as_graphdata = MEM_callocN(sizeof(*as_graphdata), __func__);
      }
    }

    /* Build our AStar graphs. */
    if (isld_steps_src) {
      for (tindex = 0; tindex < num_trees; tindex++) {
        mesh_island_to_astar_graph(use_islands ? &island_store : NULL,
                                   tindex,
                                   verts_src,
                                   edge_to_poly_map_src,
                                   num_edges_src,
                                   loops_src,
                                   polys_src,
                                   num_polys_src,
                                   &as_graphdata[tindex]);
      }
    }

    /* Build our BVHtrees, either from verts or tessfaces. */
    if (use_from_vert) {
      if (use_islands) {
        BLI_bitmap *verts_active = BLI_BITMAP_NEW((size_t)num_verts_src, __func__);

        for (tindex = 0; tindex < num_trees; tindex++) {
          MeshElemMap *isld = island_store.islands[tindex];
          int num_verts_active = 0;
          BLI_bitmap_set_all(verts_active, false, (size_t)num_verts_src);
          for (i = 0; i < isld->count; i++) {
            mp_src = &polys_src[isld->indices[i]];
            for (lidx_src = mp_src->loopstart; lidx_src < mp_src->loopstart + mp_src->totloop;
                 lidx_src++) {
              const unsigned int vidx_src = loops_src[lidx_src].v;
              if (!BLI_BITMAP_TEST(verts_active, vidx_src)) {
                BLI_BITMAP_ENABLE(verts_active, loops_src[lidx_src].v);
                num_verts_active++;
              }
            }
          }
          bvhtree_from_mesh_verts_ex(&treedata[tindex],
                                     verts_src,
                                     num_verts_src,
                                     false,
                                     verts_active,
                                     num_verts_active,
                                     0.0,
                                     2,
                                     6);
        }

        MEM_freeN(verts_active);
      }
      else {
        BLI_assert(num_trees == 1);
        BKE_bvhtree_from_mesh_get(&treedata[0], me_src, BVHTREE_FROM_VERTS, 2);
      }
    }
    else { /* We use polygons. */
      if (use_islands) {
        /* bvhtree here uses looptri faces... */
        BLI_bitmap *looptri_active;

        looptri_src = BKE_mesh_runtime_looptri_ensure(me_src);
        num_looptri_src = me_src->runtime.looptris.len;
        looptri_active = BLI_BITMAP_NEW((size_t)num_looptri_src, __func__);

        for (tindex = 0; tindex < num_trees; tindex++) {
          int num_looptri_active = 0;
          BLI_bitmap_set_all(looptri_active, false, (size_t)num_looptri_src);
          for (i = 0; i < num_looptri_src; i++) {
            mp_src = &polys_src[looptri_src[i].poly];
            if (island_store.items_to_islands[mp_src->loopstart] == tindex) {
              BLI_BITMAP_ENABLE(looptri_active, i);
              num_looptri_active++;
            }
          }
          bvhtree_from_mesh_looptri_ex(&treedata[tindex],
                                       verts_src,
                                       false,
                                       loops_src,
                                       false,
                                       looptri_src,
                                       num_looptri_src,
                                       false,
                                       looptri_active,
                                       num_looptri_active,
                                       0.0,
                                       2,
                                       6);
        }

        MEM_freeN(looptri_active);
      }
      else {
        BLI_assert(num_trees == 1);
        BKE_bvhtree_from_mesh_get(&treedata[0], me_src, BVHTREE_FROM_LOOPTRI, 2);
      }
    }

    /* And check each dest poly! */
    islands_res = MEM_mallocN(sizeof(*islands_res) * (size_t)num_trees, __func__);
    for (tindex = 0; tindex < num_trees; tindex++) {
      islands_res[tindex] = MEM_mallocN(sizeof(**islands_res) * islands_res_buff_size, __func__);
    }

    for (pidx_dst = 0, mp_dst = polys_dst; pidx_dst < numpolys_dst; pidx_dst++, mp_dst++) {
      float pnor_dst[3];

      /* Only in use_from_vert case, we may need polys' centers as fallback
       * in case we cannot decide which corner to use from normals only. */
      float pcent_dst[3];
      bool pcent_dst_valid = false;

      if (mode == MREMAP_MODE_LOOP_NEAREST_POLYNOR) {
        copy_v3_v3(pnor_dst, poly_nors_dst[pidx_dst]);
        if (space_transform) {
          BLI_space_transform_apply_normal(space_transform, pnor_dst);
        }
      }

      if ((size_t)mp_dst->totloop > islands_res_buff_size) {
        islands_res_buff_size = (size_t)mp_dst->totloop + MREMAP_DEFAULT_BUFSIZE;
        for (tindex = 0; tindex < num_trees; tindex++) {
          islands_res[tindex] = MEM_reallocN(islands_res[tindex],
                                             sizeof(**islands_res) * islands_res_buff_size);
        }
      }

      for (tindex = 0; tindex < num_trees; tindex++) {
        BVHTreeFromMesh *tdata = &treedata[tindex];

        ml_dst = &loops_dst[mp_dst->loopstart];
        for (plidx_dst = 0; plidx_dst < mp_dst->totloop; plidx_dst++, ml_dst++) {
          if (use_from_vert) {
            MeshElemMap *vert_to_refelem_map_src = NULL;

            copy_v3_v3(tmp_co, verts_dst[ml_dst->v].co);
            nearest.index = -1;

            /* Convert the vertex to tree coordinates, if needed. */
            if (space_transform) {
              BLI_space_transform_apply(space_transform, tmp_co);
            }

            if (mesh_remap_bvhtree_query_nearest(
                    tdata, &nearest, tmp_co, max_dist_sq, &hit_dist)) {
              float(*nor_dst)[3];
              float(*nors_src)[3];
              float best_nor_dot = -2.0f;
              float best_sqdist_fallback = FLT_MAX;
              int best_index_src = -1;

              if (mode == MREMAP_MODE_LOOP_NEAREST_LOOPNOR) {
                copy_v3_v3(tmp_no, loop_nors_dst[plidx_dst + mp_dst->loopstart]);
                if (space_transform) {
                  BLI_space_transform_apply_normal(space_transform, tmp_no);
                }
                nor_dst = &tmp_no;
                nors_src = loop_nors_src;
                vert_to_refelem_map_src = vert_to_loop_map_src;
              }
              else { /* if (mode == MREMAP_MODE_LOOP_NEAREST_POLYNOR) { */
                nor_dst = &pnor_dst;
                nors_src = poly_nors_src;
                vert_to_refelem_map_src = vert_to_poly_map_src;
              }

              for (i = vert_to_refelem_map_src[nearest.index].count; i--;) {
                const int index_src = vert_to_refelem_map_src[nearest.index].indices[i];
                BLI_assert(index_src != -1);
                const float dot = dot_v3v3(nors_src[index_src], *nor_dst);

                pidx_src = ((mode == MREMAP_MODE_LOOP_NEAREST_LOOPNOR) ?
                                loop_to_poly_map_src[index_src] :
                                index_src);
                /* WARNING! This is not the *real* lidx_src in case of POLYNOR, we only use it
                 *          to check we stay on current island (all loops from a given poly are
                 *          on same island!). */
                lidx_src = ((mode == MREMAP_MODE_LOOP_NEAREST_LOOPNOR) ?
                                index_src :
                                polys_src[pidx_src].loopstart);

                /* A same vert may be at the boundary of several islands! Hence, we have to ensure
                 * poly/loop we are currently considering *belongs* to current island! */
                if (use_islands && island_store.items_to_islands[lidx_src] != tindex) {
                  continue;
                }

                if (dot > best_nor_dot - 1e-6f) {
                  /* We need something as fallback decision in case dest normal matches several
                   * source normals (see T44522), using distance between polys' centers here. */
                  float *pcent_src;
                  float sqdist;

                  mp_src = &polys_src[pidx_src];
                  ml_src = &loops_src[mp_src->loopstart];

                  if (!pcent_dst_valid) {
                    BKE_mesh_calc_poly_center(
                        mp_dst, &loops_dst[mp_dst->loopstart], verts_dst, pcent_dst);
                    pcent_dst_valid = true;
                  }
                  pcent_src = poly_cents_src[pidx_src];
                  sqdist = len_squared_v3v3(pcent_dst, pcent_src);

                  if ((dot > best_nor_dot + 1e-6f) || (sqdist < best_sqdist_fallback)) {
                    best_nor_dot = dot;
                    best_sqdist_fallback = sqdist;
                    best_index_src = index_src;
                  }
                }
              }
              if (best_index_src == -1) {
                /* We found no item to map back from closest vertex... */
                best_nor_dot = -1.0f;
                hit_dist = FLT_MAX;
              }
              else if (mode == MREMAP_MODE_LOOP_NEAREST_POLYNOR) {
                /* Our best_index_src is a poly one for now!
                 * Have to find its loop matching our closest vertex. */
                mp_src = &polys_src[best_index_src];
                ml_src = &loops_src[mp_src->loopstart];
                for (plidx_src = 0; plidx_src < mp_src->totloop; plidx_src++, ml_src++) {
                  if ((int)ml_src->v == nearest.index) {
                    best_index_src = plidx_src + mp_src->loopstart;
                    break;
                  }
                }
              }
              best_nor_dot = (best_nor_dot + 1.0f) * 0.5f;
              islands_res[tindex][plidx_dst].factor = hit_dist ? (best_nor_dot / hit_dist) : 1e18f;
              islands_res[tindex][plidx_dst].hit_dist = hit_dist;
              islands_res[tindex][plidx_dst].index_src = best_index_src;
            }
            else {
              /* No source for this dest loop! */
              islands_res[tindex][plidx_dst].factor = 0.0f;
              islands_res[tindex][plidx_dst].hit_dist = FLT_MAX;
              islands_res[tindex][plidx_dst].index_src = -1;
            }
          }
          else if (mode & MREMAP_USE_NORPROJ) {
            int n = (ray_radius > 0.0f) ? MREMAP_RAYCAST_APPROXIMATE_NR : 1;
            float w = 1.0f;

            copy_v3_v3(tmp_co, verts_dst[ml_dst->v].co);
            copy_v3_v3(tmp_no, loop_nors_dst[plidx_dst + mp_dst->loopstart]);

            /* We do our transform here, since we may do several raycast/nearest queries. */
            if (space_transform) {
              BLI_space_transform_apply(space_transform, tmp_co);
              BLI_space_transform_apply_normal(space_transform, tmp_no);
            }

            while (n--) {
              if (mesh_remap_bvhtree_query_raycast(
                      tdata, &rayhit, tmp_co, tmp_no, ray_radius / w, max_dist, &hit_dist)) {
                islands_res[tindex][plidx_dst].factor = (hit_dist ? (1.0f / hit_dist) : 1e18f) * w;
                islands_res[tindex][plidx_dst].hit_dist = hit_dist;
                islands_res[tindex][plidx_dst].index_src = (int)tdata->looptri[rayhit.index].poly;
                copy_v3_v3(islands_res[tindex][plidx_dst].hit_point, rayhit.co);
                break;
              }
              /* Next iteration will get bigger radius but smaller weight! */
              w /= MREMAP_RAYCAST_APPROXIMATE_FAC;
            }
            if (n == -1) {
              /* Fallback to 'nearest' hit here, loops usually comes in 'face group', not good to
               * have only part of one dest face's loops to map to source.
               * Note that since we give this a null weight, if whole weight for a given face
               * is null, it means none of its loop mapped to this source island,
               * hence we can skip it later.
               */
              copy_v3_v3(tmp_co, verts_dst[ml_dst->v].co);
              nearest.index = -1;

              /* Convert the vertex to tree coordinates, if needed. */
              if (space_transform) {
                BLI_space_transform_apply(space_transform, tmp_co);
              }

              /* In any case, this fallback nearest hit should have no weight at all
               * in 'best island' decision! */
              islands_res[tindex][plidx_dst].factor = 0.0f;

              if (mesh_remap_bvhtree_query_nearest(
                      tdata, &nearest, tmp_co, max_dist_sq, &hit_dist)) {
                islands_res[tindex][plidx_dst].hit_dist = hit_dist;
                islands_res[tindex][plidx_dst].index_src = (int)tdata->looptri[nearest.index].poly;
                copy_v3_v3(islands_res[tindex][plidx_dst].hit_point, nearest.co);
              }
              else {
                /* No source for this dest loop! */
                islands_res[tindex][plidx_dst].hit_dist = FLT_MAX;
                islands_res[tindex][plidx_dst].index_src = -1;
              }
            }
          }
          else { /* Nearest poly either to use all its loops/verts or just closest one. */
            copy_v3_v3(tmp_co, verts_dst[ml_dst->v].co);
            nearest.index = -1;

            /* Convert the vertex to tree coordinates, if needed. */
            if (space_transform) {
              BLI_space_transform_apply(space_transform, tmp_co);
            }

            if (mesh_remap_bvhtree_query_nearest(
                    tdata, &nearest, tmp_co, max_dist_sq, &hit_dist)) {
              islands_res[tindex][plidx_dst].factor = hit_dist ? (1.0f / hit_dist) : 1e18f;
              islands_res[tindex][plidx_dst].hit_dist = hit_dist;
              islands_res[tindex][plidx_dst].index_src = (int)tdata->looptri[nearest.index].poly;
              copy_v3_v3(islands_res[tindex][plidx_dst].hit_point, nearest.co);
            }
            else {
              /* No source for this dest loop! */
              islands_res[tindex][plidx_dst].factor = 0.0f;
              islands_res[tindex][plidx_dst].hit_dist = FLT_MAX;
              islands_res[tindex][plidx_dst].index_src = -1;
            }
          }
        }
      }

      /* And now, find best island to use! */
      /* We have to first select the 'best source island' for given dst poly and its loops.
       * Then, we have to check that poly does not 'spread' across some island's limits
       * (like inner seams for UVs, etc.).
       * Note we only still partially support that kind of situation here, i.e.
       * Polys spreading over actual cracks
       * (like a narrow space without faces on src, splitting a 'tube-like' geometry).
       * That kind of situation should be relatively rare, though.
       */
      /* XXX This block in itself is big and complex enough to be a separate function but...
       *     it uses a bunch of locale vars.
       *     Not worth sending all that through parameters (for now at least). */
      {
        BLI_AStarGraph *as_graph = NULL;
        int *poly_island_index_map = NULL;
        int pidx_src_prev = -1;

        MeshElemMap *best_island = NULL;
        float best_island_fac = 0.0f;
        int best_island_index = -1;

        for (tindex = 0; tindex < num_trees; tindex++) {
          float island_fac = 0.0f;

          for (plidx_dst = 0; plidx_dst < mp_dst->totloop; plidx_dst++) {
            island_fac += islands_res[tindex][plidx_dst].factor;
          }
          island_fac /= (float)mp_dst->totloop;

          if (island_fac > best_island_fac) {
            best_island_fac = island_fac;
            best_island_index = tindex;
          }
        }

        if (best_island_index != -1 && isld_steps_src) {
          best_island = use_islands ? island_store.islands[best_island_index] : NULL;
          as_graph = &as_graphdata[best_island_index];
          poly_island_index_map = (int *)as_graph->custom_data;
          BLI_astar_solution_init(as_graph, &as_solution, NULL);
        }

        for (plidx_dst = 0; plidx_dst < mp_dst->totloop; plidx_dst++) {
          IslandResult *isld_res;
          lidx_dst = plidx_dst + mp_dst->loopstart;

          if (best_island_index == -1) {
            /* No source for any loops of our dest poly in any source islands. */
            BKE_mesh_remap_item_define_invalid(r_map, lidx_dst);
            continue;
          }

          as_solution.custom_data = POINTER_FROM_INT(false);

          isld_res = &islands_res[best_island_index][plidx_dst];
          if (use_from_vert) {
            /* Indices stored in islands_res are those of loops, one per dest loop. */
            lidx_src = isld_res->index_src;
            if (lidx_src >= 0) {
              pidx_src = loop_to_poly_map_src[lidx_src];
              /* If prev and curr poly are the same, no need to do anything more!!! */
              if (!ELEM(pidx_src_prev, -1, pidx_src) && isld_steps_src) {
                int pidx_isld_src, pidx_isld_src_prev;
                if (poly_island_index_map) {
                  pidx_isld_src = poly_island_index_map[pidx_src];
                  pidx_isld_src_prev = poly_island_index_map[pidx_src_prev];
                }
                else {
                  pidx_isld_src = pidx_src;
                  pidx_isld_src_prev = pidx_src_prev;
                }

                BLI_astar_graph_solve(as_graph,
                                      pidx_isld_src_prev,
                                      pidx_isld_src,
                                      mesh_remap_calc_loops_astar_f_cost,
                                      &as_solution,
                                      isld_steps_src);
                if (POINTER_AS_INT(as_solution.custom_data) && (as_solution.steps > 0)) {
                  /* Find first 'cutting edge' on path, and bring back lidx_src on poly just
                   * before that edge.
                   * Note we could try to be much smarter, g.g. Storing a whole poly's indices,
                   * and making decision (on which side of cutting edge(s!) to be) on the end,
                   * but this is one more level of complexity, better to first see if
                   * simple solution works!
                   */
                  int last_valid_pidx_isld_src = -1;
                  /* Note we go backward here, from dest to src poly. */
                  for (i = as_solution.steps - 1; i--;) {
                    BLI_AStarGNLink *as_link = as_solution.prev_links[pidx_isld_src];
                    const int eidx = POINTER_AS_INT(as_link->custom_data);
                    pidx_isld_src = as_solution.prev_nodes[pidx_isld_src];
                    BLI_assert(pidx_isld_src != -1);
                    if (eidx != -1) {
                      /* we are 'crossing' a cutting edge. */
                      last_valid_pidx_isld_src = pidx_isld_src;
                    }
                  }
                  if (last_valid_pidx_isld_src != -1) {
                    /* Find a new valid loop in that new poly (nearest one for now).
                     * Note we could be much more subtle here, again that's for later... */
                    int j;
                    float best_dist_sq = FLT_MAX;

                    ml_dst = &loops_dst[lidx_dst];
                    copy_v3_v3(tmp_co, verts_dst[ml_dst->v].co);

                    /* We do our transform here,
                     * since we may do several raycast/nearest queries. */
                    if (space_transform) {
                      BLI_space_transform_apply(space_transform, tmp_co);
                    }

                    pidx_src = (use_islands ? best_island->indices[last_valid_pidx_isld_src] :
                                              last_valid_pidx_isld_src);
                    mp_src = &polys_src[pidx_src];
                    ml_src = &loops_src[mp_src->loopstart];
                    for (j = 0; j < mp_src->totloop; j++, ml_src++) {
                      const float dist_sq = len_squared_v3v3(verts_src[ml_src->v].co, tmp_co);
                      if (dist_sq < best_dist_sq) {
                        best_dist_sq = dist_sq;
                        lidx_src = mp_src->loopstart + j;
                      }
                    }
                  }
                }
              }
              mesh_remap_item_define(r_map,
                                     lidx_dst,
                                     isld_res->hit_dist,
                                     best_island_index,
                                     1,
                                     &lidx_src,
                                     &full_weight);
              pidx_src_prev = pidx_src;
            }
            else {
              /* No source for this loop in this island. */
              /* TODO: would probably be better to get a source
               * at all cost in best island anyway? */
              mesh_remap_item_define(r_map, lidx_dst, FLT_MAX, best_island_index, 0, NULL, NULL);
            }
          }
          else {
            /* Else, we use source poly, indices stored in islands_res are those of polygons. */
            pidx_src = isld_res->index_src;
            if (pidx_src >= 0) {
              float *hit_co = isld_res->hit_point;
              int best_loop_index_src;

              mp_src = &polys_src[pidx_src];
              /* If prev and curr poly are the same, no need to do anything more!!! */
              if (!ELEM(pidx_src_prev, -1, pidx_src) && isld_steps_src) {
                int pidx_isld_src, pidx_isld_src_prev;
                if (poly_island_index_map) {
                  pidx_isld_src = poly_island_index_map[pidx_src];
                  pidx_isld_src_prev = poly_island_index_map[pidx_src_prev];
                }
                else {
                  pidx_isld_src = pidx_src;
                  pidx_isld_src_prev = pidx_src_prev;
                }

                BLI_astar_graph_solve(as_graph,
                                      pidx_isld_src_prev,
                                      pidx_isld_src,
                                      mesh_remap_calc_loops_astar_f_cost,
                                      &as_solution,
                                      isld_steps_src);
                if (POINTER_AS_INT(as_solution.custom_data) && (as_solution.steps > 0)) {
                  /* Find first 'cutting edge' on path, and bring back lidx_src on poly just
                   * before that edge.
                   * Note we could try to be much smarter: e.g. Storing a whole poly's indices,
                   * and making decision (one which side of cutting edge(s)!) to be on the end,
                   * but this is one more level of complexity, better to first see if
                   * simple solution works!
                   */
                  int last_valid_pidx_isld_src = -1;
                  /* Note we go backward here, from dest to src poly. */
                  for (i = as_solution.steps - 1; i--;) {
                    BLI_AStarGNLink *as_link = as_solution.prev_links[pidx_isld_src];
                    int eidx = POINTER_AS_INT(as_link->custom_data);

                    pidx_isld_src = as_solution.prev_nodes[pidx_isld_src];
                    BLI_assert(pidx_isld_src != -1);
                    if (eidx != -1) {
                      /* we are 'crossing' a cutting edge. */
                      last_valid_pidx_isld_src = pidx_isld_src;
                    }
                  }
                  if (last_valid_pidx_isld_src != -1) {
                    /* Find a new valid loop in that new poly (nearest point on poly for now).
                     * Note we could be much more subtle here, again that's for later... */
                    float best_dist_sq = FLT_MAX;
                    int j;

                    ml_dst = &loops_dst[lidx_dst];
                    copy_v3_v3(tmp_co, verts_dst[ml_dst->v].co);

                    /* We do our transform here,
                     * since we may do several raycast/nearest queries. */
                    if (space_transform) {
                      BLI_space_transform_apply(space_transform, tmp_co);
                    }

                    pidx_src = (use_islands ? best_island->indices[last_valid_pidx_isld_src] :
                                              last_valid_pidx_isld_src);
                    mp_src = &polys_src[pidx_src];

                    /* Create that one on demand. */
                    if (poly_to_looptri_map_src == NULL) {
                      BKE_mesh_origindex_map_create_looptri(&poly_to_looptri_map_src,
                                                            &poly_to_looptri_map_src_buff,
                                                            polys_src,
                                                            num_polys_src,
                                                            looptri_src,
                                                            num_looptri_src);
                    }

                    for (j = poly_to_looptri_map_src[pidx_src].count; j--;) {
                      float h[3];
                      const MLoopTri *lt =
                          &looptri_src[poly_to_looptri_map_src[pidx_src].indices[j]];
                      float dist_sq;

                      closest_on_tri_to_point_v3(h,
                                                 tmp_co,
                                                 vcos_src[loops_src[lt->tri[0]].v],
                                                 vcos_src[loops_src[lt->tri[1]].v],
                                                 vcos_src[loops_src[lt->tri[2]].v]);
                      dist_sq = len_squared_v3v3(tmp_co, h);
                      if (dist_sq < best_dist_sq) {
                        copy_v3_v3(hit_co, h);
                        best_dist_sq = dist_sq;
                      }
                    }
                  }
                }
              }

              if (mode == MREMAP_MODE_LOOP_POLY_NEAREST) {
                mesh_remap_interp_poly_data_get(mp_src,
                                                loops_src,
                                                (const float(*)[3])vcos_src,
                                                hit_co,
                                                &buff_size_interp,
                                                &vcos_interp,
                                                true,
                                                &indices_interp,
                                                &weights_interp,
                                                false,
                                                &best_loop_index_src);

                mesh_remap_item_define(r_map,
                                       lidx_dst,
                                       isld_res->hit_dist,
                                       best_island_index,
                                       1,
                                       &best_loop_index_src,
                                       &full_weight);
              }
              else {
                const int sources_num = mesh_remap_interp_poly_data_get(
                    mp_src,
                    loops_src,
                    (const float(*)[3])vcos_src,
                    hit_co,
                    &buff_size_interp,
                    &vcos_interp,
                    true,
                    &indices_interp,
                    &weights_interp,
                    true,
                    NULL);

                mesh_remap_item_define(r_map,
                                       lidx_dst,
                                       isld_res->hit_dist,
                                       best_island_index,
                                       sources_num,
                                       indices_interp,
                                       weights_interp);
              }

              pidx_src_prev = pidx_src;
            }
            else {
              /* No source for this loop in this island. */
              /* TODO: would probably be better to get a source
               * at all cost in best island anyway? */
              mesh_remap_item_define(r_map, lidx_dst, FLT_MAX, best_island_index, 0, NULL, NULL);
            }
          }
        }

        BLI_astar_solution_clear(&as_solution);
      }
    }

    for (tindex = 0; tindex < num_trees; tindex++) {
      MEM_freeN(islands_res[tindex]);
      free_bvhtree_from_mesh(&treedata[tindex]);
      if (isld_steps_src) {
        BLI_astar_graph_free(&as_graphdata[tindex]);
      }
    }
    MEM_freeN(islands_res);
    BKE_mesh_loop_islands_free(&island_store);
    MEM_freeN(treedata);
    if (isld_steps_src) {
      MEM_freeN(as_graphdata);
      BLI_astar_solution_free(&as_solution);
    }

    if (vcos_src) {
      MEM_freeN(vcos_src);
    }
    if (vert_to_loop_map_src) {
      MEM_freeN(vert_to_loop_map_src);
    }
    if (vert_to_loop_map_src_buff) {
      MEM_freeN(vert_to_loop_map_src_buff);
    }
    if (vert_to_poly_map_src) {
      MEM_freeN(vert_to_poly_map_src);
    }
    if (vert_to_poly_map_src_buff) {
      MEM_freeN(vert_to_poly_map_src_buff);
    }
    if (edge_to_poly_map_src) {
      MEM_freeN(edge_to_poly_map_src);
    }
    if (edge_to_poly_map_src_buff) {
      MEM_freeN(edge_to_poly_map_src_buff);
    }
    if (poly_to_looptri_map_src) {
      MEM_freeN(poly_to_looptri_map_src);
    }
    if (poly_to_looptri_map_src_buff) {
      MEM_freeN(poly_to_looptri_map_src_buff);
    }
    if (loop_to_poly_map_src) {
      MEM_freeN(loop_to_poly_map_src);
    }
    if (poly_cents_src) {
      MEM_freeN(poly_cents_src);
    }
    if (vcos_interp) {
      MEM_freeN(vcos_interp);
    }
    if (indices_interp) {
      MEM_freeN(indices_interp);
    }
    if (weights_interp) {
      MEM_freeN(weights_interp);
    }
  }
}

void BKE_mesh_remap_calc_polys_from_mesh(const int mode,
                                         const SpaceTransform *space_transform,
                                         const float max_dist,
                                         const float ray_radius,
                                         MVert *verts_dst,
                                         const int numverts_dst,
                                         MLoop *loops_dst,
                                         const int numloops_dst,
                                         MPoly *polys_dst,
                                         const int numpolys_dst,
                                         CustomData *pdata_dst,
                                         const bool dirty_nors_dst,
                                         Mesh *me_src,
                                         MeshPairRemap *r_map)
{
  const float full_weight = 1.0f;
  const float max_dist_sq = max_dist * max_dist;
  float(*poly_nors_dst)[3] = NULL;
  float tmp_co[3], tmp_no[3];
  int i;

  BLI_assert(mode & MREMAP_MODE_POLY);

  if (mode & (MREMAP_USE_NORMAL | MREMAP_USE_NORPROJ)) {
    /* Cache poly nors into a temp CDLayer. */
    poly_nors_dst = CustomData_get_layer(pdata_dst, CD_NORMAL);
    if (!poly_nors_dst) {
      poly_nors_dst = CustomData_add_layer(pdata_dst, CD_NORMAL, CD_CALLOC, NULL, numpolys_dst);
      CustomData_set_layer_flag(pdata_dst, CD_NORMAL, CD_FLAG_TEMPORARY);
    }
    if (dirty_nors_dst) {
      BKE_mesh_calc_normals_poly(verts_dst,
                                 NULL,
                                 numverts_dst,
                                 loops_dst,
                                 polys_dst,
                                 numloops_dst,
                                 numpolys_dst,
                                 poly_nors_dst,
                                 true);
    }
  }

  BKE_mesh_remap_init(r_map, numpolys_dst);

  if (mode == MREMAP_MODE_TOPOLOGY) {
    BLI_assert(numpolys_dst == me_src->totpoly);
    for (i = 0; i < numpolys_dst; i++) {
      mesh_remap_item_define(r_map, i, FLT_MAX, 0, 1, &i, &full_weight);
    }
  }
  else {
    BVHTreeFromMesh treedata = {NULL};
    BVHTreeNearest nearest = {0};
    BVHTreeRayHit rayhit = {0};
    float hit_dist;

    BKE_bvhtree_from_mesh_get(&treedata, me_src, BVHTREE_FROM_LOOPTRI, 2);

    if (mode == MREMAP_MODE_POLY_NEAREST) {
      nearest.index = -1;

      for (i = 0; i < numpolys_dst; i++) {
        MPoly *mp = &polys_dst[i];

        BKE_mesh_calc_poly_center(mp, &loops_dst[mp->loopstart], verts_dst, tmp_co);

        /* Convert the vertex to tree coordinates, if needed. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, tmp_co);
        }

        if (mesh_remap_bvhtree_query_nearest(
                &treedata, &nearest, tmp_co, max_dist_sq, &hit_dist)) {
          const MLoopTri *lt = &treedata.looptri[nearest.index];
          const int poly_index = (int)lt->poly;
          mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &poly_index, &full_weight);
        }
        else {
          /* No source for this dest poly! */
          BKE_mesh_remap_item_define_invalid(r_map, i);
        }
      }
    }
    else if (mode == MREMAP_MODE_POLY_NOR) {
      BLI_assert(poly_nors_dst);

      for (i = 0; i < numpolys_dst; i++) {
        MPoly *mp = &polys_dst[i];

        BKE_mesh_calc_poly_center(mp, &loops_dst[mp->loopstart], verts_dst, tmp_co);
        copy_v3_v3(tmp_no, poly_nors_dst[i]);

        /* Convert the vertex to tree coordinates, if needed. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, tmp_co);
          BLI_space_transform_apply_normal(space_transform, tmp_no);
        }

        if (mesh_remap_bvhtree_query_raycast(
                &treedata, &rayhit, tmp_co, tmp_no, ray_radius, max_dist, &hit_dist)) {
          const MLoopTri *lt = &treedata.looptri[rayhit.index];
          const int poly_index = (int)lt->poly;

          mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &poly_index, &full_weight);
        }
        else {
          /* No source for this dest poly! */
          BKE_mesh_remap_item_define_invalid(r_map, i);
        }
      }
    }
    else if (mode == MREMAP_MODE_POLY_POLYINTERP_PNORPROJ) {
      /* We cast our rays randomly, with a pseudo-even distribution
       * (since we spread across tessellated tris,
       * with additional weighting based on each tri's relative area).
       */
      RNG *rng = BLI_rng_new(0);

      const size_t numpolys_src = (size_t)me_src->totpoly;

      /* Here it's simpler to just allocate for all polys :/ */
      int *indices = MEM_mallocN(sizeof(*indices) * numpolys_src, __func__);
      float *weights = MEM_mallocN(sizeof(*weights) * numpolys_src, __func__);

      size_t tmp_poly_size = MREMAP_DEFAULT_BUFSIZE;
      float(*poly_vcos_2d)[2] = MEM_mallocN(sizeof(*poly_vcos_2d) * tmp_poly_size, __func__);
      /* Tessellated 2D poly, always (num_loops - 2) triangles. */
      int(*tri_vidx_2d)[3] = MEM_mallocN(sizeof(*tri_vidx_2d) * (tmp_poly_size - 2), __func__);

      for (i = 0; i < numpolys_dst; i++) {
        /* For each dst poly, we sample some rays from it (2D grid in pnor space)
         * and use their hits to interpolate from source polys. */
        /* Note: dst poly is early-converted into src space! */
        MPoly *mp = &polys_dst[i];

        int tot_rays, done_rays = 0;
        float poly_area_2d_inv, done_area = 0.0f;

        float pcent_dst[3];
        float to_pnor_2d_mat[3][3], from_pnor_2d_mat[3][3];
        float poly_dst_2d_min[2], poly_dst_2d_max[2], poly_dst_2d_z;
        float poly_dst_2d_size[2];

        float totweights = 0.0f;
        float hit_dist_accum = 0.0f;
        int sources_num = 0;
        const int tris_num = mp->totloop - 2;
        int j;

        BKE_mesh_calc_poly_center(mp, &loops_dst[mp->loopstart], verts_dst, pcent_dst);
        copy_v3_v3(tmp_no, poly_nors_dst[i]);

        /* We do our transform here, else it'd be redone by raycast helper for each ray, ugh! */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, pcent_dst);
          BLI_space_transform_apply_normal(space_transform, tmp_no);
        }

        copy_vn_fl(weights, (int)numpolys_src, 0.0f);

        if (UNLIKELY((size_t)mp->totloop > tmp_poly_size)) {
          tmp_poly_size = (size_t)mp->totloop;
          poly_vcos_2d = MEM_reallocN(poly_vcos_2d, sizeof(*poly_vcos_2d) * tmp_poly_size);
          tri_vidx_2d = MEM_reallocN(tri_vidx_2d, sizeof(*tri_vidx_2d) * (tmp_poly_size - 2));
        }

        axis_dominant_v3_to_m3(to_pnor_2d_mat, tmp_no);
        invert_m3_m3(from_pnor_2d_mat, to_pnor_2d_mat);

        mul_m3_v3(to_pnor_2d_mat, pcent_dst);
        poly_dst_2d_z = pcent_dst[2];

        /* Get (2D) bounding square of our poly. */
        INIT_MINMAX2(poly_dst_2d_min, poly_dst_2d_max);

        for (j = 0; j < mp->totloop; j++) {
          MLoop *ml = &loops_dst[j + mp->loopstart];
          copy_v3_v3(tmp_co, verts_dst[ml->v].co);
          if (space_transform) {
            BLI_space_transform_apply(space_transform, tmp_co);
          }
          mul_v2_m3v3(poly_vcos_2d[j], to_pnor_2d_mat, tmp_co);
          minmax_v2v2_v2(poly_dst_2d_min, poly_dst_2d_max, poly_vcos_2d[j]);
        }

        /* We adjust our ray-casting grid to ray_radius (the smaller, the more rays are cast),
         * with lower/upper bounds. */
        sub_v2_v2v2(poly_dst_2d_size, poly_dst_2d_max, poly_dst_2d_min);

        if (ray_radius) {
          tot_rays = (int)((max_ff(poly_dst_2d_size[0], poly_dst_2d_size[1]) / ray_radius) + 0.5f);
          CLAMP(tot_rays, MREMAP_RAYCAST_TRI_SAMPLES_MIN, MREMAP_RAYCAST_TRI_SAMPLES_MAX);
        }
        else {
          /* If no radius (pure rays), give max number of rays! */
          tot_rays = MREMAP_RAYCAST_TRI_SAMPLES_MIN;
        }
        tot_rays *= tot_rays;

        poly_area_2d_inv = area_poly_v2((const float(*)[2])poly_vcos_2d,
                                        (unsigned int)mp->totloop);
        /* In case we have a null-area degenerated poly... */
        poly_area_2d_inv = 1.0f / max_ff(poly_area_2d_inv, 1e-9f);

        /* Tessellate our poly. */
        if (mp->totloop == 3) {
          tri_vidx_2d[0][0] = 0;
          tri_vidx_2d[0][1] = 1;
          tri_vidx_2d[0][2] = 2;
        }
        if (mp->totloop == 4) {
          tri_vidx_2d[0][0] = 0;
          tri_vidx_2d[0][1] = 1;
          tri_vidx_2d[0][2] = 2;
          tri_vidx_2d[1][0] = 0;
          tri_vidx_2d[1][1] = 2;
          tri_vidx_2d[1][2] = 3;
        }
        else {
          BLI_polyfill_calc(
              poly_vcos_2d, (unsigned int)mp->totloop, -1, (unsigned int(*)[3])tri_vidx_2d);
        }

        for (j = 0; j < tris_num; j++) {
          float *v1 = poly_vcos_2d[tri_vidx_2d[j][0]];
          float *v2 = poly_vcos_2d[tri_vidx_2d[j][1]];
          float *v3 = poly_vcos_2d[tri_vidx_2d[j][2]];
          int rays_num;

          /* All this allows us to get 'absolute' number of rays for each tri,
           * avoiding accumulating errors over iterations, and helping better even distribution. */
          done_area += area_tri_v2(v1, v2, v3);
          rays_num = max_ii(
              (int)((float)tot_rays * done_area * poly_area_2d_inv + 0.5f) - done_rays, 0);
          done_rays += rays_num;

          while (rays_num--) {
            int n = (ray_radius > 0.0f) ? MREMAP_RAYCAST_APPROXIMATE_NR : 1;
            float w = 1.0f;

            BLI_rng_get_tri_sample_float_v2(rng, v1, v2, v3, tmp_co);

            tmp_co[2] = poly_dst_2d_z;
            mul_m3_v3(from_pnor_2d_mat, tmp_co);

            /* At this point, tmp_co is a point on our poly surface, in mesh_src space! */
            while (n--) {
              if (mesh_remap_bvhtree_query_raycast(
                      &treedata, &rayhit, tmp_co, tmp_no, ray_radius / w, max_dist, &hit_dist)) {
                const MLoopTri *lt = &treedata.looptri[rayhit.index];

                weights[lt->poly] += w;
                totweights += w;
                hit_dist_accum += hit_dist;
                break;
              }
              /* Next iteration will get bigger radius but smaller weight! */
              w /= MREMAP_RAYCAST_APPROXIMATE_FAC;
            }
          }
        }

        if (totweights > 0.0f) {
          for (j = 0; j < (int)numpolys_src; j++) {
            if (!weights[j]) {
              continue;
            }
            /* Note: sources_num is always <= j! */
            weights[sources_num] = weights[j] / totweights;
            indices[sources_num] = j;
            sources_num++;
          }
          mesh_remap_item_define(
              r_map, i, hit_dist_accum / totweights, 0, sources_num, indices, weights);
        }
        else {
          /* No source for this dest poly! */
          BKE_mesh_remap_item_define_invalid(r_map, i);
        }
      }

      MEM_freeN(tri_vidx_2d);
      MEM_freeN(poly_vcos_2d);
      MEM_freeN(indices);
      MEM_freeN(weights);
      BLI_rng_free(rng);
    }
    else {
      CLOG_WARN(&LOG, "Unsupported mesh-to-mesh poly mapping mode (%d)!", mode);
      memset(r_map->items, 0, sizeof(*r_map->items) * (size_t)numpolys_dst);
    }

    free_bvhtree_from_mesh(&treedata);
  }
}

#undef MREMAP_RAYCAST_APPROXIMATE_NR
#undef MREMAP_RAYCAST_APPROXIMATE_FAC
#undef MREMAP_RAYCAST_TRI_SAMPLES_MIN
#undef MREMAP_RAYCAST_TRI_SAMPLES_MAX
#undef MREMAP_DEFAULT_BUFSIZE

/** \} */
