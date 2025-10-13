/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Functions for mapping data between meshes.
 */

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_astar.h"
#include "BLI_bit_vector.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_solvers.h"
#include "BLI_math_statistics.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "DNA_modifier_enums.h"

#include "BKE_attribute.hh"
#include "BKE_bvhutils.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_mesh_remap.hh" /* own include */

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

static CLG_LogRef LOG = {"geom.mesh"};

using blender::float3;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name Some Generic Helpers
 * \{ */

static bool mesh_remap_bvhtree_query_nearest(blender::bke::BVHTreeFromMesh *treedata,
                                             BVHTreeNearest *nearest,
                                             const float co[3],
                                             const float max_dist_sq,
                                             float *r_hit_dist)
{
  /* Use local proximity heuristics (to reduce the nearest search). */
  if (nearest->index != -1) {
    nearest->dist_sq = len_squared_v3v3(co, nearest->co);
    if (nearest->dist_sq > max_dist_sq) {
      /* The previous valid index is too far away and not valid for this check. */
      nearest->dist_sq = max_dist_sq;
      nearest->index = -1;
    }
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

  return false;
}

static bool mesh_remap_bvhtree_query_raycast(blender::bke::BVHTreeFromMesh *treedata,
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

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto-match.
 *
 * Find transform of a mesh to get best match with another.
 * \{ */

float BKE_mesh_remap_calc_difference_from_mesh(const SpaceTransform *space_transform,
                                               const Span<float3> vert_positions_dst,
                                               const Mesh *me_src)
{
  BVHTreeNearest nearest = {0};
  float hit_dist;

  float result = 0.0f;
  int i;

  blender::bke::BVHTreeFromMesh treedata = me_src->bvh_verts();
  nearest.index = -1;

  for (i = 0; i < vert_positions_dst.size(); i++) {
    float tmp_co[3];

    copy_v3_v3(tmp_co, vert_positions_dst[i]);

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

  result = (float(vert_positions_dst.size()) / result) - 1.0f;

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
static void mesh_calc_eigen_matrix(const Span<float3> positions, float r_mat[4][4])
{
  float center[3], covmat[3][3];
  float eigen_val[3], eigen_vec[3][3];

  bool eigen_success;
  int i;

  unit_m4(r_mat);

  /* NOTE: here we apply sample correction to covariance matrix, since we consider the vertices
   *       as a sample of the whole 'surface' population of our mesh. */
  BLI_covariance_m3_v3n(reinterpret_cast<const float (*)[3]>(positions.data()),
                        int(positions.size()),
                        true,
                        covmat,
                        center);

  eigen_success = BLI_eigen_solve_selfadjoint_m3((const float (*)[3])covmat, eigen_val, eigen_vec);
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
    /* NOTE: not sure why we need square root of eigen values here
     * (which are equivalent to singular values, as far as I have understood),
     * but it seems to heavily reduce (if not completely nullify)
     * the error due to non-uniform scalings... */
    evi = (evi < 1e-6f && evi > -1e-6f) ? ((evi < 0.0f) ? -1e-3f : 1e-3f) : sqrtf_signed(evi);
    mul_v3_fl(eigen_vec[i], evi);
  }

  copy_m4_m3(r_mat, eigen_vec);
  copy_v3_v3(r_mat[3], center);
}

void BKE_mesh_remap_find_best_match_from_mesh(const Span<float3> vert_positions_dst,
                                              const Mesh *me_src,
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
  const float (*mirr)[3];

  float mat_src[4][4], mat_dst[4][4], best_mat_dst[4][4];
  float best_match = FLT_MAX, match;

  const Span<float3> positions_src = me_src->vert_positions();
  mesh_calc_eigen_matrix(positions_src, mat_src);
  mesh_calc_eigen_matrix(vert_positions_dst, mat_dst);

  BLI_space_transform_global_from_matrices(r_space_transform, mat_dst, mat_src);
  match = BKE_mesh_remap_calc_difference_from_mesh(r_space_transform, vert_positions_dst, me_src);
  best_match = match;
  copy_m4_m4(best_mat_dst, mat_dst);

  /* And now, we have to check the other sixth possible mirrored versions... */
  for (mirr = mirrors; (*mirr)[0]; mirr++) {
    mul_v3_fl(mat_dst[0], (*mirr)[0]);
    mul_v3_fl(mat_dst[1], (*mirr)[1]);
    mul_v3_fl(mat_dst[2], (*mirr)[2]);

    BLI_space_transform_global_from_matrices(r_space_transform, mat_dst, mat_src);
    match = BKE_mesh_remap_calc_difference_from_mesh(
        r_space_transform, vert_positions_dst, me_src);
    if (match < best_match) {
      best_match = match;
      copy_m4_m4(best_mat_dst, mat_dst);
    }
  }

  BLI_space_transform_global_from_matrices(r_space_transform, best_mat_dst, mat_src);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh to Mesh Mapping
 * \{ */

void BKE_mesh_remap_init(MeshPairRemap *map, const int items_num)
{
  MemArena *mem = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

  BKE_mesh_remap_free(map);

  map->items = static_cast<MeshPairRemapItem *>(
      BLI_memarena_alloc(mem, sizeof(*map->items) * size_t(items_num)));
  map->items_num = items_num;

  map->mem = mem;
}

void BKE_mesh_remap_free(MeshPairRemap *map)
{
  if (map->mem) {
    BLI_memarena_free(map->mem);
  }

  map->items_num = 0;
  map->items = nullptr;
  map->mem = nullptr;
}

static void mesh_remap_item_define(MeshPairRemap *map,
                                   const int index,
                                   const float /*hit_dist*/,
                                   const int island,
                                   const int sources_num,
                                   const int *indices_src,
                                   const float *weights_src)
{
  MeshPairRemapItem *mapit = &map->items[index];
  MemArena *mem = map->mem;

  if (sources_num) {
    mapit->sources_num = sources_num;
    mapit->indices_src = static_cast<int *>(
        BLI_memarena_alloc(mem, sizeof(*mapit->indices_src) * size_t(sources_num)));
    memcpy(mapit->indices_src, indices_src, sizeof(*mapit->indices_src) * size_t(sources_num));
    mapit->weights_src = static_cast<float *>(
        BLI_memarena_alloc(mem, sizeof(*mapit->weights_src) * size_t(sources_num)));
    memcpy(mapit->weights_src, weights_src, sizeof(*mapit->weights_src) * size_t(sources_num));
  }
  else {
    mapit->sources_num = 0;
    mapit->indices_src = nullptr;
    mapit->weights_src = nullptr;
  }
  // mapit->hit_dist = hit_dist;
  mapit->island = island;
}

void BKE_mesh_remap_item_define_invalid(MeshPairRemap *map, const int index)
{
  mesh_remap_item_define(map, index, FLT_MAX, 0, 0, nullptr, nullptr);
}

static int mesh_remap_interp_face_data_get(const blender::IndexRange face,
                                           const blender::Span<int> corner_verts,
                                           const blender::Span<blender::float3> positions_src,
                                           const float point[3],
                                           size_t *buff_size,
                                           float (**vcos)[3],
                                           const bool use_loops,
                                           int **indices,
                                           float **weights,
                                           const bool do_weights,
                                           int *r_closest_index)
{
  float (*vco)[3];
  float ref_dist_sq = FLT_MAX;
  int *index;
  const int sources_num = int(face.size());
  int i;

  if (size_t(sources_num) > *buff_size) {
    *buff_size = size_t(sources_num);
    *vcos = static_cast<float (*)[3]>(MEM_reallocN(*vcos, sizeof(**vcos) * *buff_size));
    *indices = static_cast<int *>(MEM_reallocN(*indices, sizeof(**indices) * *buff_size));
    if (do_weights) {
      *weights = static_cast<float *>(MEM_reallocN(*weights, sizeof(**weights) * *buff_size));
    }
  }

  for (i = 0, vco = *vcos, index = *indices; i < sources_num; i++, vco++, index++) {
    const int vert = corner_verts[face[i]];
    *index = use_loops ? int(face[i]) : vert;
    copy_v3_v3(*vco, positions_src[vert]);
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
struct IslandResult {
  /** A factor, based on which best island for a given set of elements will be selected. */
  float factor;
  /** Index of the source. */
  int index_src;
  /** The actual hit distance. */
  float hit_dist;
  /** The hit point, if relevant. */
  float hit_point[3];
};

/**
 * \note About all BVH/ray-casting stuff below:
 *
 * * We must use our ray radius as BVH epsilon too, else rays not hitting anything but
 *   'passing near' an item would be missed (since BVH handling would not detect them,
 *   'refining' callbacks won't be executed, even though they would return a valid hit).
 * * However, in 'islands' case where each hit gets a weight, 'precise' hits should have a better
 *   weight than 'approximate' hits.
 *   To address that, we simplify things with:
 *   * A first ray-cast with default, given ray-radius;
 *   * If first one fails, we do more ray-casting with bigger radius, but if hit is found
 *     it will get smaller weight.
 *
 *   This only concerns loops, currently (because of islands), and 'sampled' edges/faces norproj.
 */

/** At most N ray-casts per 'real' ray. */
#define MREMAP_RAYCAST_APPROXIMATE_NR 3
/** Each approximated ray-casts will have n times bigger radius than previous one. */
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
                                         const Span<float3> vert_positions_dst,
                                         const Mesh *me_src,
                                         Mesh *me_dst,
                                         MeshPairRemap *r_map)
{
  const float full_weight = 1.0f;
  const float max_dist_sq = max_dist * max_dist;
  int i;

  BLI_assert(mode & MREMAP_MODE_VERT);

  BKE_mesh_remap_init(r_map, int(vert_positions_dst.size()));

  if (mode == MREMAP_MODE_TOPOLOGY) {
    BLI_assert(vert_positions_dst.size() == me_src->verts_num);
    for (i = 0; i < vert_positions_dst.size(); i++) {
      mesh_remap_item_define(r_map, i, FLT_MAX, 0, 1, &i, &full_weight);
    }
  }
  else {
    blender::bke::BVHTreeFromMesh treedata{};
    BVHTreeNearest nearest = {0};
    BVHTreeRayHit rayhit = {0};
    float hit_dist;
    float tmp_co[3], tmp_no[3];

    if (mode == MREMAP_MODE_VERT_NEAREST) {
      treedata = me_src->bvh_verts();
      nearest.index = -1;

      for (i = 0; i < vert_positions_dst.size(); i++) {
        copy_v3_v3(tmp_co, vert_positions_dst[i]);

        /* Convert the vertex to tree coordinates, if needed. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, tmp_co);
        }

        if (mesh_remap_bvhtree_query_nearest(&treedata, &nearest, tmp_co, max_dist_sq, &hit_dist))
        {
          mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &nearest.index, &full_weight);
        }
        else {
          /* No source for this dest vertex! */
          BKE_mesh_remap_item_define_invalid(r_map, i);
        }
      }
    }
    else if (ELEM(mode, MREMAP_MODE_VERT_EDGE_NEAREST, MREMAP_MODE_VERT_EDGEINTERP_NEAREST)) {
      const blender::Span<blender::int2> edges_src = me_src->edges();
      const blender::Span<blender::float3> positions_src = me_src->vert_positions();

      treedata = me_src->bvh_edges();
      nearest.index = -1;

      for (i = 0; i < vert_positions_dst.size(); i++) {
        copy_v3_v3(tmp_co, vert_positions_dst[i]);

        /* Convert the vertex to tree coordinates, if needed. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, tmp_co);
        }

        if (mesh_remap_bvhtree_query_nearest(&treedata, &nearest, tmp_co, max_dist_sq, &hit_dist))
        {
          const blender::int2 &edge = edges_src[nearest.index];
          const float *v1cos = positions_src[edge[0]];
          const float *v2cos = positions_src[edge[1]];

          if (mode == MREMAP_MODE_VERT_EDGE_NEAREST) {
            const float dist_v1 = len_squared_v3v3(tmp_co, v1cos);
            const float dist_v2 = len_squared_v3v3(tmp_co, v2cos);
            const int index = (dist_v1 > dist_v2) ? edge[1] : edge[0];
            mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &index, &full_weight);
          }
          else if (mode == MREMAP_MODE_VERT_EDGEINTERP_NEAREST) {
            int indices[2];
            float weights[2];

            indices[0] = edge[0];
            indices[1] = edge[1];

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
    }
    else if (ELEM(mode,
                  MREMAP_MODE_VERT_FACE_NEAREST,
                  MREMAP_MODE_VERT_POLYINTERP_NEAREST,
                  MREMAP_MODE_VERT_POLYINTERP_VNORPROJ))
    {
      const blender::OffsetIndices faces_src = me_src->faces();
      const blender::Span<int> corner_verts_src = me_src->corner_verts();
      const blender::Span<blender::float3> positions_src = me_src->vert_positions();
      const blender::Span<blender::float3> vert_normals_dst = me_dst->vert_normals();
      const blender::Span<int> tri_faces = me_src->corner_tri_faces();

      size_t tmp_buff_size = MREMAP_DEFAULT_BUFSIZE;
      float (*vcos)[3] = MEM_malloc_arrayN<float[3]>(tmp_buff_size, __func__);
      int *indices = MEM_malloc_arrayN<int>(tmp_buff_size, __func__);
      float *weights = MEM_malloc_arrayN<float>(tmp_buff_size, __func__);

      treedata = me_src->bvh_corner_tris();

      if (mode == MREMAP_MODE_VERT_POLYINTERP_VNORPROJ) {
        for (i = 0; i < vert_positions_dst.size(); i++) {
          copy_v3_v3(tmp_co, vert_positions_dst[i]);
          copy_v3_v3(tmp_no, vert_normals_dst[i]);

          /* Convert the vertex to tree coordinates, if needed. */
          if (space_transform) {
            BLI_space_transform_apply(space_transform, tmp_co);
            BLI_space_transform_apply_normal(space_transform, tmp_no);
          }

          if (mesh_remap_bvhtree_query_raycast(
                  &treedata, &rayhit, tmp_co, tmp_no, ray_radius, max_dist, &hit_dist))
          {
            const int face_index = tri_faces[rayhit.index];
            const int sources_num = mesh_remap_interp_face_data_get(faces_src[face_index],
                                                                    corner_verts_src,
                                                                    positions_src,
                                                                    rayhit.co,
                                                                    &tmp_buff_size,
                                                                    &vcos,
                                                                    false,
                                                                    &indices,
                                                                    &weights,
                                                                    true,
                                                                    nullptr);

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

        for (i = 0; i < vert_positions_dst.size(); i++) {
          copy_v3_v3(tmp_co, vert_positions_dst[i]);

          /* Convert the vertex to tree coordinates, if needed. */
          if (space_transform) {
            BLI_space_transform_apply(space_transform, tmp_co);
          }

          if (mesh_remap_bvhtree_query_nearest(
                  &treedata, &nearest, tmp_co, max_dist_sq, &hit_dist))
          {
            const int face_index = tri_faces[nearest.index];

            if (mode == MREMAP_MODE_VERT_FACE_NEAREST) {
              int index;
              mesh_remap_interp_face_data_get(faces_src[face_index],
                                              corner_verts_src,
                                              positions_src,
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
              const int sources_num = mesh_remap_interp_face_data_get(faces_src[face_index],
                                                                      corner_verts_src,
                                                                      positions_src,
                                                                      nearest.co,
                                                                      &tmp_buff_size,
                                                                      &vcos,
                                                                      false,
                                                                      &indices,
                                                                      &weights,
                                                                      true,
                                                                      nullptr);

              mesh_remap_item_define(r_map, i, hit_dist, 0, sources_num, indices, weights);
            }
          }
          else {
            /* No source for this dest vertex! */
            BKE_mesh_remap_item_define_invalid(r_map, i);
          }
        }
      }

      MEM_freeN(vcos);
      MEM_freeN(indices);
      MEM_freeN(weights);
    }
    else {
      CLOG_WARN(&LOG, "Unsupported mesh-to-mesh vertex mapping mode (%d)!", mode);
      memset(r_map->items, 0, sizeof(*r_map->items) * size_t(vert_positions_dst.size()));
    }
  }
}

void BKE_mesh_remap_calc_edges_from_mesh(const int mode,
                                         const SpaceTransform *space_transform,
                                         const float max_dist,
                                         const float ray_radius,
                                         const Span<float3> vert_positions_dst,
                                         const Span<blender::int2> edges_dst,
                                         const Mesh *me_src,
                                         Mesh *me_dst,
                                         MeshPairRemap *r_map)
{
  using namespace blender;
  const float full_weight = 1.0f;
  const float max_dist_sq = max_dist * max_dist;
  int i;

  BLI_assert(mode & MREMAP_MODE_EDGE);

  BKE_mesh_remap_init(r_map, int(edges_dst.size()));

  if (mode == MREMAP_MODE_TOPOLOGY) {
    BLI_assert(edges_dst.size() == me_src->edges_num);
    for (i = 0; i < edges_dst.size(); i++) {
      mesh_remap_item_define(r_map, i, FLT_MAX, 0, 1, &i, &full_weight);
    }
  }
  else {
    blender::bke::BVHTreeFromMesh treedata{};
    BVHTreeNearest nearest = {0};
    BVHTreeRayHit rayhit = {0};
    float hit_dist;
    float tmp_co[3], tmp_no[3];

    if (mode == MREMAP_MODE_EDGE_VERT_NEAREST) {
      const int num_verts_src = me_src->verts_num;
      const blender::Span<blender::int2> edges_src = me_src->edges();
      const blender::Span<blender::float3> positions_src = me_src->vert_positions();

      struct HitData {
        float hit_dist;
        int index;
      };
      HitData *v_dst_to_src_map = MEM_malloc_arrayN<HitData>(size_t(vert_positions_dst.size()),
                                                             __func__);

      for (i = 0; i < vert_positions_dst.size(); i++) {
        v_dst_to_src_map[i].hit_dist = -1.0f;
      }

      Array<int> vert_to_edge_src_offsets;
      Array<int> vert_to_edge_src_indices;
      const GroupedSpan<int> vert_to_edge_src_map = bke::mesh::build_vert_to_edge_map(
          edges_src, num_verts_src, vert_to_edge_src_offsets, vert_to_edge_src_indices);

      treedata = me_src->bvh_verts();
      nearest.index = -1;

      for (i = 0; i < edges_dst.size(); i++) {
        const blender::int2 &e_dst = edges_dst[i];
        float best_totdist = FLT_MAX;
        int best_eidx_src = -1;
        int j = 2;

        while (j--) {
          const int vidx_dst = j ? e_dst[0] : e_dst[1];

          /* Compute closest verts only once! */
          if (v_dst_to_src_map[vidx_dst].hit_dist == -1.0f) {
            copy_v3_v3(tmp_co, vert_positions_dst[vidx_dst]);

            /* Convert the vertex to tree coordinates, if needed. */
            if (space_transform) {
              BLI_space_transform_apply(space_transform, tmp_co);
            }

            if (mesh_remap_bvhtree_query_nearest(
                    &treedata, &nearest, tmp_co, max_dist_sq, &hit_dist))
            {
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
          const int vidx_dst = j ? e_dst[0] : e_dst[1];
          const float first_dist = v_dst_to_src_map[vidx_dst].hit_dist;
          const int vidx_src = v_dst_to_src_map[vidx_dst].index;
          const int *eidx_src;
          int k;

          if (vidx_src < 0) {
            continue;
          }

          eidx_src = vert_to_edge_src_map[vidx_src].data();
          k = int(vert_to_edge_src_map[vidx_src].size());

          for (; k--; eidx_src++) {
            const blender::int2 &edge_src = edges_src[*eidx_src];
            const float *other_co_src =
                positions_src[blender::bke::mesh::edge_other_vert(edge_src, vidx_src)];
            const float *other_co_dst =
                vert_positions_dst[blender::bke::mesh::edge_other_vert(e_dst, int(vidx_dst))];
            const float totdist = first_dist + len_v3v3(other_co_src, other_co_dst);

            if (totdist < best_totdist) {
              best_totdist = totdist;
              best_eidx_src = *eidx_src;
            }
          }
        }

        if (best_eidx_src >= 0) {
          const float *co1_src = positions_src[edges_src[best_eidx_src][0]];
          const float *co2_src = positions_src[edges_src[best_eidx_src][1]];
          const float *co1_dst = vert_positions_dst[e_dst[0]];
          const float *co2_dst = vert_positions_dst[e_dst[1]];
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

      MEM_freeN(v_dst_to_src_map);
    }
    else if (mode == MREMAP_MODE_EDGE_NEAREST) {
      treedata = me_src->bvh_edges();
      nearest.index = -1;

      for (i = 0; i < edges_dst.size(); i++) {
        interp_v3_v3v3(tmp_co,
                       vert_positions_dst[edges_dst[i][0]],
                       vert_positions_dst[edges_dst[i][1]],
                       0.5f);

        /* Convert the vertex to tree coordinates, if needed. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, tmp_co);
        }

        if (mesh_remap_bvhtree_query_nearest(&treedata, &nearest, tmp_co, max_dist_sq, &hit_dist))
        {
          mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &nearest.index, &full_weight);
        }
        else {
          /* No source for this dest edge! */
          BKE_mesh_remap_item_define_invalid(r_map, i);
        }
      }
    }
    else if (mode == MREMAP_MODE_EDGE_POLY_NEAREST) {
      const blender::Span<blender::int2> edges_src = me_src->edges();
      const blender::OffsetIndices faces_src = me_src->faces();
      const blender::Span<int> corner_edges_src = me_src->corner_edges();
      const blender::Span<blender::float3> positions_src = me_src->vert_positions();
      const blender::Span<int> tri_faces = me_src->corner_tri_faces();

      treedata = me_src->bvh_corner_tris();

      for (i = 0; i < edges_dst.size(); i++) {
        interp_v3_v3v3(tmp_co,
                       vert_positions_dst[edges_dst[i][0]],
                       vert_positions_dst[edges_dst[i][1]],
                       0.5f);

        /* Convert the vertex to tree coordinates, if needed. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, tmp_co);
        }

        if (mesh_remap_bvhtree_query_nearest(&treedata, &nearest, tmp_co, max_dist_sq, &hit_dist))
        {
          const int face_index = tri_faces[nearest.index];
          const blender::IndexRange face_src = faces_src[face_index];
          const int *corner_edge_src = &corner_edges_src[face_src.start()];
          int nloops = int(face_src.size());
          float best_dist_sq = FLT_MAX;
          int best_eidx_src = -1;

          for (; nloops--; corner_edge_src++) {
            const blender::int2 &edge_src = edges_src[*corner_edge_src];
            const float *co1_src = positions_src[edge_src[0]];
            const float *co2_src = positions_src[edge_src[1]];
            float co_src[3];
            float dist_sq;

            interp_v3_v3v3(co_src, co1_src, co2_src, 0.5f);
            dist_sq = len_squared_v3v3(tmp_co, co_src);
            if (dist_sq < best_dist_sq) {
              best_dist_sq = dist_sq;
              best_eidx_src = *corner_edge_src;
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
    }
    else if (mode == MREMAP_MODE_EDGE_EDGEINTERP_VNORPROJ) {
      const int num_rays_min = 5, num_rays_max = 100;
      const int numedges_src = me_src->edges_num;

      /* Subtleness - this one we can allocate only max number of cast rays per edges! */
      int *indices = MEM_malloc_arrayN<int>(size_t(min_ii(numedges_src, num_rays_max)), __func__);
      /* Here it's simpler to just allocate for all edges :/ */
      float *weights = MEM_malloc_arrayN<float>(size_t(numedges_src), __func__);

      treedata = me_src->bvh_edges();

      const blender::Span<blender::float3> vert_normals_dst = me_dst->vert_normals();

      for (i = 0; i < edges_dst.size(); i++) {
        /* For each dst edge, we sample some rays from it (interpolated from its vertices)
         * and use their hits to interpolate from source edges. */
        const blender::int2 &edge = edges_dst[i];
        float v1_co[3], v2_co[3];
        float v1_no[3], v2_no[3];

        int grid_size;
        float edge_dst_len;
        float grid_step;

        float totweights = 0.0f;
        float hit_dist_accum = 0.0f;
        int sources_num = 0;
        int j;

        copy_v3_v3(v1_co, vert_positions_dst[edge[0]]);
        copy_v3_v3(v2_co, vert_positions_dst[edge[1]]);

        copy_v3_v3(v1_no, vert_normals_dst[edge[0]]);
        copy_v3_v3(v2_no, vert_normals_dst[edge[1]]);

        /* We do our transform here, allows to interpolate from normals already in src space. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, v1_co);
          BLI_space_transform_apply(space_transform, v2_co);
          BLI_space_transform_apply_normal(space_transform, v1_no);
          BLI_space_transform_apply_normal(space_transform, v2_no);
        }

        copy_vn_fl(weights, int(numedges_src), 0.0f);

        /* We adjust our ray-casting grid to ray_radius (the smaller, the more rays are cast),
         * with lower/upper bounds. */
        edge_dst_len = len_v3v3(v1_co, v2_co);

        grid_size = int((edge_dst_len / ray_radius) + 0.5f);
        CLAMP(grid_size, num_rays_min, num_rays_max); /* min 5 rays/edge, max 100. */

        /* Not actual distance here, rather an interp fac... */
        grid_step = 1.0f / float(grid_size);

        /* And now we can cast all our rays, and see what we get! */
        for (j = 0; j < grid_size; j++) {
          const float fac = grid_step * float(j);

          int n = (ray_radius > 0.0f) ? MREMAP_RAYCAST_APPROXIMATE_NR : 1;
          float w = 1.0f;

          interp_v3_v3v3(tmp_co, v1_co, v2_co, fac);
          interp_v3_v3v3_slerp_safe(tmp_no, v1_no, v2_no, fac);

          while (n--) {
            if (mesh_remap_bvhtree_query_raycast(
                    &treedata, &rayhit, tmp_co, tmp_no, ray_radius / w, max_dist, &hit_dist))
            {
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
        if (totweights > (float(grid_size) / 2.0f)) {
          for (j = 0; j < int(numedges_src); j++) {
            if (!weights[j]) {
              continue;
            }
            /* NOTE: sources_num is always <= j! */
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
      memset(r_map->items, 0, sizeof(*r_map->items) * size_t(edges_dst.size()));
    }
  }
}

#define POLY_UNSET 0
#define POLY_CENTER_INIT 1
#define POLY_COMPLETE 2

static void mesh_island_to_astar_graph_edge_process(
    MeshIslandStore *islands,
    const int island_index,
    BLI_AStarGraph *as_graph,
    const blender::Span<blender::float3> positions,
    const blender::OffsetIndices<int> faces,
    const blender::Span<int> corner_verts,
    const int edge_idx,
    BLI_bitmap *done_edges,
    const blender::GroupedSpan<int> edge_to_face_map,
    const bool is_edge_innercut,
    const int *face_island_index_map,
    float (*face_centers)[3],
    uchar *face_status)
{
  blender::Array<int, 16> face_island_indices(edge_to_face_map[edge_idx].size());
  int i, j;

  for (i = 0; i < edge_to_face_map[edge_idx].size(); i++) {
    const int pidx = edge_to_face_map[edge_idx][i];
    const blender::IndexRange face = faces[pidx];
    const int pidx_isld = islands ? face_island_index_map[pidx] : pidx;
    void *custom_data = is_edge_innercut ? POINTER_FROM_INT(edge_idx) : POINTER_FROM_INT(-1);

    if (UNLIKELY(islands && (islands->items_to_islands[face.start()] != island_index))) {
      /* face not in current island, happens with border edges... */
      face_island_indices[i] = -1;
      continue;
    }

    if (face_status[pidx_isld] == POLY_COMPLETE) {
      face_island_indices[i] = pidx_isld;
      continue;
    }

    if (face_status[pidx_isld] == POLY_UNSET) {
      copy_v3_v3(face_centers[pidx_isld],
                 blender::bke::mesh::face_center_calc(positions, corner_verts.slice(face)));
      BLI_astar_node_init(as_graph, pidx_isld, face_centers[pidx_isld]);
      face_status[pidx_isld] = POLY_CENTER_INIT;
    }

    for (j = i; j--;) {
      float dist_cost;
      const int pidx_isld_other = face_island_indices[j];

      if (pidx_isld_other == -1 || face_status[pidx_isld_other] == POLY_COMPLETE) {
        /* If the other face is complete, that link has already been added! */
        continue;
      }
      dist_cost = len_v3v3(face_centers[pidx_isld_other], face_centers[pidx_isld]);
      BLI_astar_node_link_add(as_graph, pidx_isld_other, pidx_isld, dist_cost, custom_data);
    }

    face_island_indices[i] = pidx_isld;
  }

  BLI_BITMAP_ENABLE(done_edges, edge_idx);
}

static void mesh_island_to_astar_graph(MeshIslandStore *islands,
                                       const int island_index,
                                       const blender::Span<blender::float3> positions,
                                       const blender::GroupedSpan<int> edge_to_face_map,
                                       const int numedges,
                                       const blender::OffsetIndices<int> faces,
                                       const blender::Span<int> corner_verts,
                                       const blender::Span<int> corner_edges,
                                       BLI_AStarGraph *r_as_graph)
{
  MeshElemMap *island_face_map = islands ? islands->islands[island_index] : nullptr;
  MeshElemMap *island_einnercut_map = islands ? islands->innercuts[island_index] : nullptr;

  int *face_island_index_map = nullptr;
  BLI_bitmap *done_edges = BLI_BITMAP_NEW(numedges, __func__);

  const int node_num = islands ? island_face_map->count : int(faces.size());
  uchar *face_status = MEM_calloc_arrayN<uchar>(size_t(node_num), __func__);
  float (*face_centers)[3];

  int pidx_isld;
  int i;

  BLI_astar_graph_init(r_as_graph, node_num, nullptr);
  /* face_centers is owned by graph memarena. */
  face_centers = static_cast<float (*)[3]>(
      BLI_memarena_calloc(r_as_graph->mem, sizeof(*face_centers) * size_t(node_num)));

  if (islands) {
    /* face_island_index_map is owned by graph memarena. */
    face_island_index_map = static_cast<int *>(BLI_memarena_calloc(
        r_as_graph->mem, sizeof(*face_island_index_map) * size_t(faces.size())));
    for (i = island_face_map->count; i--;) {
      face_island_index_map[island_face_map->indices[i]] = i;
    }

    r_as_graph->custom_data = face_island_index_map;

    for (i = island_einnercut_map->count; i--;) {
      mesh_island_to_astar_graph_edge_process(islands,
                                              island_index,
                                              r_as_graph,
                                              positions,
                                              faces,
                                              corner_verts,
                                              island_einnercut_map->indices[i],
                                              done_edges,
                                              edge_to_face_map,
                                              true,
                                              face_island_index_map,
                                              face_centers,
                                              face_status);
    }
  }

  for (pidx_isld = node_num; pidx_isld--;) {
    const int pidx = islands ? island_face_map->indices[pidx_isld] : pidx_isld;

    if (face_status[pidx_isld] == POLY_COMPLETE) {
      continue;
    }

    for (const int edge : corner_edges.slice(faces[pidx])) {
      if (BLI_BITMAP_TEST(done_edges, edge)) {
        continue;
      }

      mesh_island_to_astar_graph_edge_process(islands,
                                              island_index,
                                              r_as_graph,
                                              positions,
                                              faces,
                                              corner_verts,
                                              edge,
                                              done_edges,
                                              edge_to_face_map,
                                              false,
                                              face_island_index_map,
                                              face_centers,
                                              face_status);
    }
    face_status[pidx_isld] = POLY_COMPLETE;
  }

  MEM_freeN(done_edges);
  MEM_freeN(face_status);
}

#undef POLY_UNSET
#undef POLY_CENTER_INIT
#undef POLY_COMPLETE

/* Our 'f_cost' callback func, to find shortest face-path between two remapped-loops.
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
   * actual shortest face-path between next node and destination one. */
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
                                         const Mesh *mesh_dst,
                                         const Span<float3> vert_positions_dst,
                                         const Span<int> corner_verts_dst,
                                         const blender::OffsetIndices<int> faces_dst,
                                         const Mesh *me_src,
                                         MeshRemapIslandsCalc gen_islands_src,
                                         const float islands_precision_src,
                                         MeshPairRemap *r_map)
{
  using namespace blender;
  const float full_weight = 1.0f;
  const float max_dist_sq = max_dist * max_dist;

  BLI_assert(mode & MREMAP_MODE_LOOP);
  BLI_assert((islands_precision_src >= 0.0f) && (islands_precision_src <= 1.0f));

  BKE_mesh_remap_init(r_map, int(corner_verts_dst.size()));

  if (mode == MREMAP_MODE_TOPOLOGY) {
    /* In topology mapping, we assume meshes are identical, islands included! */
    BLI_assert(corner_verts_dst.size() == me_src->corners_num);
    for (int i = 0; i < corner_verts_dst.size(); i++) {
      mesh_remap_item_define(r_map, i, FLT_MAX, 0, 1, &i, &full_weight);
    }
  }
  else {
    Array<blender::bke::BVHTreeFromMesh> treedata;
    BVHTreeNearest nearest = {0};
    BVHTreeRayHit rayhit = {0};
    int num_trees = 0;
    float hit_dist;
    float tmp_co[3], tmp_no[3];

    const bool use_from_vert = (mode & MREMAP_USE_VERT);

    MeshIslandStore island_store = {0};
    bool use_islands = false;

    BLI_AStarGraph *as_graphdata = nullptr;
    BLI_AStarSolution as_solution = {0};
    const int isld_steps_src = (islands_precision_src ?
                                    max_ii(int(ASTAR_STEPS_MAX * islands_precision_src + 0.499f),
                                           1) :
                                    0);

    blender::Span<blender::float3> face_normals_src;
    blender::Span<blender::float3> loop_normals_src;

    blender::Span<blender::float3> face_normals_dst;
    blender::Span<blender::float3> loop_normals_dst;

    blender::Array<blender::float3> face_cents_src;

    GroupedSpan<int> vert_to_corner_map_src;
    GroupedSpan<int> vert_to_face_map_src;

    Array<int> edge_to_face_src_offsets;
    Array<int> edge_to_face_src_indices;
    GroupedSpan<int> edge_to_face_map_src;

    MeshElemMap *face_to_corner_tri_map_src = nullptr;
    int *face_to_corner_tri_map_src_buff = nullptr;

    /* Unlike above, those are one-to-one mappings, simpler! */
    blender::Span<int> loop_to_face_map_src;

    const blender::Span<blender::float3> positions_src = me_src->vert_positions();
    const int num_verts_src = me_src->verts_num;
    const blender::Span<blender::int2> edges_src = me_src->edges();
    const blender::OffsetIndices faces_src = me_src->faces();
    const blender::Span<int> corner_verts_src = me_src->corner_verts();
    const blender::Span<int> corner_edges_src = me_src->corner_edges();
    blender::Span<blender::int3> corner_tris_src;
    blender::Span<int> tri_faces_src;

    size_t buff_size_interp = MREMAP_DEFAULT_BUFSIZE;
    float (*vcos_interp)[3] = nullptr;
    int *indices_interp = nullptr;
    float *weights_interp = nullptr;

    int tindex, pidx_dst, lidx_dst, plidx_dst, pidx_src, lidx_src, plidx_src;

    IslandResult **islands_res;
    size_t islands_res_buff_size = MREMAP_DEFAULT_BUFSIZE;

    if (!use_from_vert) {
      vcos_interp = MEM_malloc_arrayN<float[3]>(buff_size_interp, __func__);
      indices_interp = MEM_malloc_arrayN<int>(buff_size_interp, __func__);
      weights_interp = MEM_malloc_arrayN<float>(buff_size_interp, __func__);
    }

    {
      const bool need_lnors_src = (mode & MREMAP_USE_LOOP) && (mode & MREMAP_USE_NORMAL);
      const bool need_lnors_dst = need_lnors_src || (mode & MREMAP_USE_NORPROJ);
      const bool need_pnors_src = need_lnors_src ||
                                  ((mode & MREMAP_USE_POLY) && (mode & MREMAP_USE_NORMAL));
      const bool need_pnors_dst = need_lnors_dst || need_pnors_src;

      if (need_pnors_dst) {
        face_normals_dst = mesh_dst->face_normals();
      }
      if (need_lnors_dst) {
        loop_normals_dst = mesh_dst->corner_normals();
      }
      if (need_pnors_src) {
        face_normals_src = me_src->face_normals();
      }
      if (need_lnors_src) {
        loop_normals_src = me_src->corner_normals();
      }
    }

    if (use_from_vert) {
      vert_to_corner_map_src = me_src->vert_to_corner_map();
      if (mode & MREMAP_USE_POLY) {
        vert_to_face_map_src = me_src->vert_to_face_map();
      }
    }

    /* Needed for islands (or plain mesh) to AStar graph conversion. */
    edge_to_face_map_src = bke::mesh::build_edge_to_face_map(faces_src,
                                                             corner_edges_src,
                                                             int(edges_src.size()),
                                                             edge_to_face_src_offsets,
                                                             edge_to_face_src_indices);

    if (use_from_vert) {
      loop_to_face_map_src = me_src->corner_to_face_map();
      face_cents_src.reinitialize(faces_src.size());
      for (const int64_t i : faces_src.index_range()) {
        face_cents_src[i] = blender::bke::mesh::face_center_calc(
            positions_src, corner_verts_src.slice(faces_src[i]));
      }
    }

    /* Island makes things slightly more complex here.
     * Basically, we:
     *     * Make one treedata for each island's elements.
     *     * Check all loops of a same dest face against all treedata.
     *     * Choose the island's elements giving the best results.
     */

    /* First, generate the islands, if possible. */
    if (gen_islands_src) {
      const bke::AttributeAccessor attributes = me_src->attributes();
      const VArraySpan uv_seams = *attributes.lookup<bool>("uv_seam", bke::AttrDomain::Edge);
      use_islands = gen_islands_src(positions_src,
                                    edges_src,
                                    uv_seams,
                                    faces_src,
                                    corner_verts_src,
                                    corner_edges_src,
                                    &island_store);

      num_trees = use_islands ? island_store.islands_num : 1;
      treedata.reinitialize(num_trees);
      if (isld_steps_src) {
        as_graphdata = MEM_calloc_arrayN<BLI_AStarGraph>(size_t(num_trees), __func__);
      }

      if (use_islands) {
        /* We expect our islands to contain face indices, with edge indices of 'inner cuts',
         * and a mapping loops -> islands indices.
         * This implies all loops of a same face are in the same island. */
        BLI_assert((island_store.item_type == MISLAND_TYPE_LOOP) &&
                   (island_store.island_type == MISLAND_TYPE_POLY) &&
                   (island_store.innercut_type == MISLAND_TYPE_EDGE));
      }
    }
    else {
      num_trees = 1;
      treedata.reinitialize(1);
      if (isld_steps_src) {
        as_graphdata = MEM_callocN<BLI_AStarGraph>(__func__);
      }
    }

    /* Build our AStar graphs. */
    if (isld_steps_src) {
      for (tindex = 0; tindex < num_trees; tindex++) {
        mesh_island_to_astar_graph(use_islands ? &island_store : nullptr,
                                   tindex,
                                   positions_src,
                                   edge_to_face_map_src,
                                   int(edges_src.size()),
                                   faces_src,
                                   corner_verts_src,
                                   corner_edges_src,
                                   &as_graphdata[tindex]);
      }
    }

    /* Build our BVHtrees, either from verts or tessfaces. */
    if (use_from_vert) {
      if (use_islands) {
        blender::BitVector<> verts_active(num_verts_src);

        for (tindex = 0; tindex < num_trees; tindex++) {
          MeshElemMap *isld = island_store.islands[tindex];
          verts_active.fill(false);
          for (int i = 0; i < isld->count; i++) {
            for (const int vidx_src : corner_verts_src.slice(faces_src[isld->indices[i]])) {
              if (!verts_active[vidx_src]) {
                verts_active[vidx_src].set();
              }
            }
          }
          IndexMaskMemory memory;
          treedata[tindex] = blender::bke::bvhtree_from_mesh_verts_ex(
              positions_src, IndexMask::from_bits(verts_active, memory));
        }
      }
      else {
        BLI_assert(num_trees == 1);
        treedata[0] = me_src->bvh_verts();
      }
    }
    else { /* We use faces. */
      if (use_islands) {
        corner_tris_src = me_src->corner_tris();
        tri_faces_src = me_src->corner_tri_faces();
        blender::BitVector<> faces_active(corner_tris_src.size());

        for (tindex = 0; tindex < num_trees; tindex++) {
          faces_active.fill(false);
          for (const int64_t i : faces_src.index_range()) {
            const blender::IndexRange face = faces_src[i];
            if (island_store.items_to_islands[face.start()] == tindex) {
              faces_active[i].set();
            }
          }
          IndexMaskMemory memory;
          treedata[tindex] = blender::bke::bvhtree_from_mesh_corner_tris_ex(
              positions_src,
              faces_src,
              corner_verts_src,
              corner_tris_src,
              IndexMask::from_bits(faces_active, memory));
        }
      }
      else {
        BLI_assert(num_trees == 1);
        treedata[0] = me_src->bvh_corner_tris();
      }
    }

    /* And check each dest face! */
    islands_res = MEM_malloc_arrayN<IslandResult *>(size_t(num_trees), __func__);
    for (tindex = 0; tindex < num_trees; tindex++) {
      islands_res[tindex] = MEM_malloc_arrayN<IslandResult>(islands_res_buff_size, __func__);
    }
    const blender::Span<int> tri_faces = me_src->corner_tri_faces();

    for (pidx_dst = 0; pidx_dst < faces_dst.size(); pidx_dst++) {
      const blender::IndexRange face_dst = faces_dst[pidx_dst];
      float pnor_dst[3];

      /* Only in use_from_vert case, we may need faces' centers as fallback
       * in case we cannot decide which corner to use from normals only. */
      blender::float3 pcent_dst;
      bool pcent_dst_valid = false;

      if (mode == MREMAP_MODE_LOOP_NEAREST_POLYNOR) {
        copy_v3_v3(pnor_dst, face_normals_dst[pidx_dst]);
        if (space_transform) {
          BLI_space_transform_apply_normal(space_transform, pnor_dst);
        }
      }

      if (size_t(face_dst.size()) > islands_res_buff_size) {
        islands_res_buff_size = size_t(face_dst.size()) + MREMAP_DEFAULT_BUFSIZE;
        for (tindex = 0; tindex < num_trees; tindex++) {
          islands_res[tindex] = static_cast<IslandResult *>(
              MEM_reallocN(islands_res[tindex], sizeof(**islands_res) * islands_res_buff_size));
        }
      }

      for (tindex = 0; tindex < num_trees; tindex++) {
        blender::bke::BVHTreeFromMesh *tdata = &treedata[tindex];

        for (plidx_dst = 0; plidx_dst < face_dst.size(); plidx_dst++) {
          const int vert_dst = corner_verts_dst[face_dst.start() + plidx_dst];
          if (use_from_vert) {
            blender::Span<int> vert_to_refelem_map_src;

            copy_v3_v3(tmp_co, vert_positions_dst[vert_dst]);
            nearest.index = -1;

            /* Convert the vertex to tree coordinates, if needed. */
            if (space_transform) {
              BLI_space_transform_apply(space_transform, tmp_co);
            }

            if (mesh_remap_bvhtree_query_nearest(tdata, &nearest, tmp_co, max_dist_sq, &hit_dist))
            {
              float (*nor_dst)[3];
              blender::Span<blender::float3> nors_src;
              float best_nor_dot = -2.0f;
              float best_sqdist_fallback = FLT_MAX;
              int best_index_src = -1;

              if (mode == MREMAP_MODE_LOOP_NEAREST_LOOPNOR) {
                copy_v3_v3(tmp_no, loop_normals_dst[plidx_dst + face_dst.start()]);
                if (space_transform) {
                  BLI_space_transform_apply_normal(space_transform, tmp_no);
                }
                nor_dst = &tmp_no;
                nors_src = loop_normals_src;
                vert_to_refelem_map_src = vert_to_corner_map_src[nearest.index];
              }
              else { /* if (mode == MREMAP_MODE_LOOP_NEAREST_POLYNOR) { */
                nor_dst = &pnor_dst;
                nors_src = face_normals_src;
                vert_to_refelem_map_src = vert_to_face_map_src[nearest.index];
              }

              for (const int index_src : vert_to_refelem_map_src) {
                BLI_assert(index_src != -1);
                const float dot = dot_v3v3(nors_src[index_src], *nor_dst);

                pidx_src = ((mode == MREMAP_MODE_LOOP_NEAREST_LOOPNOR) ?
                                loop_to_face_map_src[index_src] :
                                index_src);
                /* WARNING! This is not the *real* lidx_src in case of POLYNOR, we only use it
                 *          to check we stay on current island (all loops from a given face are
                 *          on same island!). */
                lidx_src = ((mode == MREMAP_MODE_LOOP_NEAREST_LOOPNOR) ?
                                index_src :
                                int(faces_src[pidx_src].start()));

                /* A same vert may be at the boundary of several islands! Hence, we have to ensure
                 * face/loop we are currently considering *belongs* to current island! */
                if (use_islands && island_store.items_to_islands[lidx_src] != tindex) {
                  continue;
                }

                if (dot > best_nor_dot - 1e-6f) {
                  /* We need something as fallback decision in case dest normal matches several
                   * source normals (see #44522), using distance between faces' centers here. */
                  float *pcent_src;
                  float sqdist;

                  if (!pcent_dst_valid) {
                    pcent_dst = blender::bke::mesh::face_center_calc(
                        vert_positions_dst, corner_verts_dst.slice(face_dst));
                    pcent_dst_valid = true;
                  }
                  pcent_src = face_cents_src[pidx_src];
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
                /* Our best_index_src is a face one for now!
                 * Have to find its loop matching our closest vertex. */
                const blender::IndexRange face_src = faces_src[best_index_src];
                for (plidx_src = 0; plidx_src < face_src.size(); plidx_src++) {
                  const int vert_src = corner_verts_src[face_src.start() + plidx_src];
                  if (vert_src == nearest.index) {
                    best_index_src = plidx_src + int(face_src.start());
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

            copy_v3_v3(tmp_co, vert_positions_dst[vert_dst]);
            copy_v3_v3(tmp_no, loop_normals_dst[plidx_dst + face_dst.start()]);

            /* We do our transform here, since we may do several raycast/nearest queries. */
            if (space_transform) {
              BLI_space_transform_apply(space_transform, tmp_co);
              BLI_space_transform_apply_normal(space_transform, tmp_no);
            }

            while (n--) {
              if (mesh_remap_bvhtree_query_raycast(
                      tdata, &rayhit, tmp_co, tmp_no, ray_radius / w, max_dist, &hit_dist))
              {
                islands_res[tindex][plidx_dst].factor = (hit_dist ? (1.0f / hit_dist) : 1e18f) * w;
                islands_res[tindex][plidx_dst].hit_dist = hit_dist;
                islands_res[tindex][plidx_dst].index_src = tri_faces[rayhit.index];
                copy_v3_v3(islands_res[tindex][plidx_dst].hit_point, rayhit.co);
                break;
              }
              /* Next iteration will get bigger radius but smaller weight! */
              w /= MREMAP_RAYCAST_APPROXIMATE_FAC;
            }
            if (n == -1) {
              /* Fall back to 'nearest' hit here, loops usually comes in 'face group', not good to
               * have only part of one dest face's loops to map to source.
               * Note that since we give this a null weight, if whole weight for a given face
               * is null, it means none of its loop mapped to this source island,
               * hence we can skip it later.
               */
              copy_v3_v3(tmp_co, vert_positions_dst[vert_dst]);
              nearest.index = -1;

              /* Convert the vertex to tree coordinates, if needed. */
              if (space_transform) {
                BLI_space_transform_apply(space_transform, tmp_co);
              }

              /* In any case, this fallback nearest hit should have no weight at all
               * in 'best island' decision! */
              islands_res[tindex][plidx_dst].factor = 0.0f;

              if (mesh_remap_bvhtree_query_nearest(
                      tdata, &nearest, tmp_co, max_dist_sq, &hit_dist))
              {
                islands_res[tindex][plidx_dst].hit_dist = hit_dist;
                islands_res[tindex][plidx_dst].index_src = tri_faces[nearest.index];
                copy_v3_v3(islands_res[tindex][plidx_dst].hit_point, nearest.co);
              }
              else {
                /* No source for this dest loop! */
                islands_res[tindex][plidx_dst].hit_dist = FLT_MAX;
                islands_res[tindex][plidx_dst].index_src = -1;
              }
            }
          }
          else { /* Nearest face either to use all its loops/verts or just closest one. */
            copy_v3_v3(tmp_co, vert_positions_dst[vert_dst]);
            nearest.index = -1;

            /* Convert the vertex to tree coordinates, if needed. */
            if (space_transform) {
              BLI_space_transform_apply(space_transform, tmp_co);
            }

            if (mesh_remap_bvhtree_query_nearest(tdata, &nearest, tmp_co, max_dist_sq, &hit_dist))
            {
              islands_res[tindex][plidx_dst].factor = hit_dist ? (1.0f / hit_dist) : 1e18f;
              islands_res[tindex][plidx_dst].hit_dist = hit_dist;
              islands_res[tindex][plidx_dst].index_src = tri_faces[nearest.index];
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
      /* We have to first select the 'best source island' for given dst face and its loops.
       * Then, we have to check that face does not 'spread' across some island's limits
       * (like inner seams for UVs, etc.).
       * Note we only still partially support that kind of situation here, i.e.
       * Faces spreading over actual cracks
       * (like a narrow space without faces on src, splitting a 'tube-like' geometry).
       * That kind of situation should be relatively rare, though.
       */
      /* XXX This block in itself is big and complex enough to be a separate function but...
       *     it uses a bunch of locale vars.
       *     Not worth sending all that through parameters (for now at least). */
      {
        BLI_AStarGraph *as_graph = nullptr;
        int *face_island_index_map = nullptr;
        int pidx_src_prev = -1;

        MeshElemMap *best_island = nullptr;
        float best_island_fac = 0.0f;
        int best_island_index = -1;

        for (tindex = 0; tindex < num_trees; tindex++) {
          float island_fac = 0.0f;

          for (plidx_dst = 0; plidx_dst < face_dst.size(); plidx_dst++) {
            island_fac += islands_res[tindex][plidx_dst].factor;
          }
          island_fac /= float(face_dst.size());

          if (island_fac > best_island_fac) {
            best_island_fac = island_fac;
            best_island_index = tindex;
          }
        }

        if (best_island_index != -1 && isld_steps_src) {
          best_island = use_islands ? island_store.islands[best_island_index] : nullptr;
          as_graph = &as_graphdata[best_island_index];
          face_island_index_map = (int *)as_graph->custom_data;
          BLI_astar_solution_init(as_graph, &as_solution, nullptr);
        }

        for (plidx_dst = 0; plidx_dst < face_dst.size(); plidx_dst++) {
          IslandResult *isld_res;
          lidx_dst = plidx_dst + int(face_dst.start());

          if (best_island_index == -1) {
            /* No source for any loops of our dest face in any source islands. */
            BKE_mesh_remap_item_define_invalid(r_map, lidx_dst);
            continue;
          }

          as_solution.custom_data = POINTER_FROM_INT(false);

          isld_res = &islands_res[best_island_index][plidx_dst];
          if (use_from_vert) {
            /* Indices stored in islands_res are those of loops, one per dest loop. */
            lidx_src = isld_res->index_src;
            if (lidx_src >= 0) {
              pidx_src = loop_to_face_map_src[lidx_src];
              /* If prev and curr face are the same, no need to do anything more!!! */
              if (!ELEM(pidx_src_prev, -1, pidx_src) && isld_steps_src) {
                int pidx_isld_src, pidx_isld_src_prev;
                if (face_island_index_map) {
                  pidx_isld_src = face_island_index_map[pidx_src];
                  pidx_isld_src_prev = face_island_index_map[pidx_src_prev];
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
                  /* Find first 'cutting edge' on path, and bring back lidx_src on face just
                   * before that edge.
                   * Note we could try to be much smarter, g.g. Storing a whole face's indices,
                   * and making decision (on which side of cutting edge(s!) to be) on the end,
                   * but this is one more level of complexity, better to first see if
                   * simple solution works!
                   */
                  int last_valid_pidx_isld_src = -1;
                  /* Note we go backward here, from dest to src face. */
                  for (int i = as_solution.steps - 1; i--;) {
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
                    /* Find a new valid loop in that new face (nearest one for now).
                     * Note we could be much more subtle here, again that's for later... */
                    float best_dist_sq = FLT_MAX;

                    copy_v3_v3(tmp_co, vert_positions_dst[corner_verts_dst[lidx_dst]]);

                    /* We do our transform here,
                     * since we may do several raycast/nearest queries. */
                    if (space_transform) {
                      BLI_space_transform_apply(space_transform, tmp_co);
                    }

                    pidx_src = (use_islands ? best_island->indices[last_valid_pidx_isld_src] :
                                              last_valid_pidx_isld_src);
                    const blender::IndexRange face_src = faces_src[pidx_src];
                    for (const int64_t corner : face_src) {
                      const int vert_src = corner_verts_src[corner];
                      const float dist_sq = len_squared_v3v3(positions_src[vert_src], tmp_co);
                      if (dist_sq < best_dist_sq) {
                        best_dist_sq = dist_sq;
                        lidx_src = int(corner);
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
              mesh_remap_item_define(
                  r_map, lidx_dst, FLT_MAX, best_island_index, 0, nullptr, nullptr);
            }
          }
          else {
            /* Else, we use source face, indices stored in islands_res are those of faces. */
            pidx_src = isld_res->index_src;
            if (pidx_src >= 0) {
              float *hit_co = isld_res->hit_point;
              int best_loop_index_src;

              const blender::IndexRange face_src = faces_src[pidx_src];
              /* If prev and curr face are the same, no need to do anything more!!! */
              if (!ELEM(pidx_src_prev, -1, pidx_src) && isld_steps_src) {
                int pidx_isld_src, pidx_isld_src_prev;
                if (face_island_index_map) {
                  pidx_isld_src = face_island_index_map[pidx_src];
                  pidx_isld_src_prev = face_island_index_map[pidx_src_prev];
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
                  /* Find first 'cutting edge' on path, and bring back lidx_src on face just
                   * before that edge.
                   * Note we could try to be much smarter: e.g. Storing a whole face's indices,
                   * and making decision (one which side of cutting edge(s)!) to be on the end,
                   * but this is one more level of complexity, better to first see if
                   * simple solution works!
                   */
                  int last_valid_pidx_isld_src = -1;
                  /* Note we go backward here, from dest to src face. */
                  for (int i = as_solution.steps - 1; i--;) {
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
                    /* Find a new valid loop in that new face (nearest point on face for now).
                     * Note we could be much more subtle here, again that's for later... */
                    float best_dist_sq = FLT_MAX;
                    int j;

                    const int vert_dst = corner_verts_dst[lidx_dst];
                    copy_v3_v3(tmp_co, vert_positions_dst[vert_dst]);

                    /* We do our transform here,
                     * since we may do several raycast/nearest queries. */
                    if (space_transform) {
                      BLI_space_transform_apply(space_transform, tmp_co);
                    }

                    pidx_src = (use_islands ? best_island->indices[last_valid_pidx_isld_src] :
                                              last_valid_pidx_isld_src);

                    /* Create that one on demand. */
                    if (face_to_corner_tri_map_src == nullptr) {
                      BKE_mesh_origindex_map_create_corner_tri(&face_to_corner_tri_map_src,
                                                               &face_to_corner_tri_map_src_buff,
                                                               faces_src,
                                                               tri_faces_src.data(),
                                                               int(tri_faces_src.size()));
                    }

                    for (j = face_to_corner_tri_map_src[pidx_src].count; j--;) {
                      float h[3];
                      const blender::int3 &tri =
                          corner_tris_src[face_to_corner_tri_map_src[pidx_src].indices[j]];
                      float dist_sq;

                      closest_on_tri_to_point_v3(h,
                                                 tmp_co,
                                                 positions_src[corner_verts_src[tri[0]]],
                                                 positions_src[corner_verts_src[tri[1]]],
                                                 positions_src[corner_verts_src[tri[2]]]);
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
                mesh_remap_interp_face_data_get(face_src,
                                                corner_verts_src,
                                                positions_src,
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
                const int sources_num = mesh_remap_interp_face_data_get(face_src,
                                                                        corner_verts_src,
                                                                        positions_src,
                                                                        hit_co,
                                                                        &buff_size_interp,
                                                                        &vcos_interp,
                                                                        true,
                                                                        &indices_interp,
                                                                        &weights_interp,
                                                                        true,
                                                                        nullptr);

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
              mesh_remap_item_define(
                  r_map, lidx_dst, FLT_MAX, best_island_index, 0, nullptr, nullptr);
            }
          }
        }

        BLI_astar_solution_clear(&as_solution);
      }
    }

    for (tindex = 0; tindex < num_trees; tindex++) {
      MEM_freeN(islands_res[tindex]);
      if (isld_steps_src) {
        BLI_astar_graph_free(&as_graphdata[tindex]);
      }
    }
    MEM_freeN(islands_res);
    BKE_mesh_loop_islands_free(&island_store);
    if (isld_steps_src) {
      MEM_freeN(as_graphdata);
      BLI_astar_solution_free(&as_solution);
    }

    if (face_to_corner_tri_map_src) {
      MEM_freeN(face_to_corner_tri_map_src);
    }
    if (face_to_corner_tri_map_src_buff) {
      MEM_freeN(face_to_corner_tri_map_src_buff);
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

void BKE_mesh_remap_calc_faces_from_mesh(const int mode,
                                         const SpaceTransform *space_transform,
                                         const float max_dist,
                                         const float ray_radius,
                                         const Mesh *mesh_dst,
                                         const Span<float3> vert_positions_dst,
                                         const Span<int> corner_verts_dst,
                                         const blender::OffsetIndices<int> faces_dst,
                                         const Mesh *me_src,
                                         MeshPairRemap *r_map)
{
  const float full_weight = 1.0f;
  const float max_dist_sq = max_dist * max_dist;
  blender::Span<blender::float3> face_normals_dst;
  blender::float3 tmp_co, tmp_no;

  BLI_assert(mode & MREMAP_MODE_POLY);

  if (mode & (MREMAP_USE_NORMAL | MREMAP_USE_NORPROJ)) {
    face_normals_dst = mesh_dst->face_normals();
  }

  BKE_mesh_remap_init(r_map, int(faces_dst.size()));

  if (mode == MREMAP_MODE_TOPOLOGY) {
    BLI_assert(faces_dst.size() == me_src->faces_num);
    for (const int64_t i : faces_dst.index_range()) {
      const int index = int(i);
      mesh_remap_item_define(r_map, int(i), FLT_MAX, 0, 1, &index, &full_weight);
    }
  }
  else {
    BVHTreeNearest nearest = {0};
    BVHTreeRayHit rayhit = {0};
    float hit_dist;
    const blender::Span<int> tri_faces = me_src->corner_tri_faces();

    blender::bke::BVHTreeFromMesh treedata = me_src->bvh_corner_tris();

    if (mode == MREMAP_MODE_POLY_NEAREST) {
      nearest.index = -1;

      for (const int64_t i : faces_dst.index_range()) {
        const blender::IndexRange face = faces_dst[i];
        tmp_co = blender::bke::mesh::face_center_calc(vert_positions_dst,
                                                      corner_verts_dst.slice(face));

        /* Convert the vertex to tree coordinates, if needed. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, tmp_co);
        }

        if (mesh_remap_bvhtree_query_nearest(&treedata, &nearest, tmp_co, max_dist_sq, &hit_dist))
        {
          const int face_index = tri_faces[nearest.index];
          mesh_remap_item_define(r_map, int(i), hit_dist, 0, 1, &face_index, &full_weight);
        }
        else {
          /* No source for this dest face! */
          BKE_mesh_remap_item_define_invalid(r_map, int(i));
        }
      }
    }
    else if (mode == MREMAP_MODE_POLY_NOR) {
      for (const int64_t i : faces_dst.index_range()) {
        const blender::IndexRange face = faces_dst[i];

        tmp_co = blender::bke::mesh::face_center_calc(vert_positions_dst,
                                                      corner_verts_dst.slice(face));
        copy_v3_v3(tmp_no, face_normals_dst[i]);

        /* Convert the vertex to tree coordinates, if needed. */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, tmp_co);
          BLI_space_transform_apply_normal(space_transform, tmp_no);
        }

        if (mesh_remap_bvhtree_query_raycast(
                &treedata, &rayhit, tmp_co, tmp_no, ray_radius, max_dist, &hit_dist))
        {
          const int face_index = tri_faces[rayhit.index];
          mesh_remap_item_define(r_map, int(i), hit_dist, 0, 1, &face_index, &full_weight);
        }
        else {
          /* No source for this dest face! */
          BKE_mesh_remap_item_define_invalid(r_map, int(i));
        }
      }
    }
    else if (mode == MREMAP_MODE_POLY_POLYINTERP_PNORPROJ) {
      /* We cast our rays randomly, with a pseudo-even distribution
       * (since we spread across tessellated triangles,
       * with additional weighting based on each triangle's relative area). */
      RNG *rng = BLI_rng_new(0);

      const size_t numfaces_src = size_t(me_src->faces_num);

      /* Here it's simpler to just allocate for all faces :/ */
      int *indices = MEM_malloc_arrayN<int>(numfaces_src, __func__);
      float *weights = MEM_malloc_arrayN<float>(numfaces_src, __func__);

      size_t tmp_face_size = MREMAP_DEFAULT_BUFSIZE;
      float (*face_vcos_2d)[2] = MEM_malloc_arrayN<float[2]>(tmp_face_size, __func__);
      /* Tessellated 2D face, always (num_loops - 2) triangles. */
      int (*tri_vidx_2d)[3] = MEM_malloc_arrayN<int[3]>(tmp_face_size - 2, __func__);

      for (const int64_t i : faces_dst.index_range()) {
        /* For each dst face, we sample some rays from it (2D grid in pnor space)
         * and use their hits to interpolate from source faces. */
        /* NOTE: dst face is early-converted into src space! */
        const blender::IndexRange face = faces_dst[i];

        int tot_rays, done_rays = 0;
        float face_area_2d_inv, done_area = 0.0f;

        blender::float3 pcent_dst;
        float to_pnor_2d_mat[3][3], from_pnor_2d_mat[3][3];
        float faces_dst_2d_min[2], faces_dst_2d_max[2], faces_dst_2d_z;
        float faces_dst_2d_size[2];

        float totweights = 0.0f;
        float hit_dist_accum = 0.0f;
        int sources_num = 0;
        const int tris_num = int(face.size()) - 2;
        int j;

        pcent_dst = blender::bke::mesh::face_center_calc(vert_positions_dst,
                                                         corner_verts_dst.slice(face));

        copy_v3_v3(tmp_no, face_normals_dst[i]);

        /* We do our transform here, else it'd be redone by raycast helper for each ray, ugh! */
        if (space_transform) {
          BLI_space_transform_apply(space_transform, pcent_dst);
          BLI_space_transform_apply_normal(space_transform, tmp_no);
        }

        copy_vn_fl(weights, int(numfaces_src), 0.0f);

        if (UNLIKELY(size_t(face.size()) > tmp_face_size)) {
          tmp_face_size = size_t(face.size());
          face_vcos_2d = static_cast<float (*)[2]>(
              MEM_reallocN(face_vcos_2d, sizeof(*face_vcos_2d) * tmp_face_size));
          tri_vidx_2d = static_cast<int (*)[3]>(
              MEM_reallocN(tri_vidx_2d, sizeof(*tri_vidx_2d) * (tmp_face_size - 2)));
        }

        axis_dominant_v3_to_m3(to_pnor_2d_mat, tmp_no);
        invert_m3_m3(from_pnor_2d_mat, to_pnor_2d_mat);

        mul_m3_v3(to_pnor_2d_mat, pcent_dst);
        faces_dst_2d_z = pcent_dst[2];

        /* Get (2D) bounding square of our face. */
        INIT_MINMAX2(faces_dst_2d_min, faces_dst_2d_max);

        for (j = 0; j < face.size(); j++) {
          const int vert = corner_verts_dst[face[j]];
          copy_v3_v3(tmp_co, vert_positions_dst[vert]);
          if (space_transform) {
            BLI_space_transform_apply(space_transform, tmp_co);
          }
          mul_v2_m3v3(face_vcos_2d[j], to_pnor_2d_mat, tmp_co);
          minmax_v2v2_v2(faces_dst_2d_min, faces_dst_2d_max, face_vcos_2d[j]);
        }

        /* We adjust our ray-casting grid to ray_radius (the smaller, the more rays are cast),
         * with lower/upper bounds. */
        sub_v2_v2v2(faces_dst_2d_size, faces_dst_2d_max, faces_dst_2d_min);

        if (ray_radius) {
          tot_rays = int((max_ff(faces_dst_2d_size[0], faces_dst_2d_size[1]) / ray_radius) + 0.5f);
          CLAMP(tot_rays, MREMAP_RAYCAST_TRI_SAMPLES_MIN, MREMAP_RAYCAST_TRI_SAMPLES_MAX);
        }
        else {
          /* If no radius (pure rays), give max number of rays! */
          tot_rays = MREMAP_RAYCAST_TRI_SAMPLES_MIN;
        }
        tot_rays *= tot_rays;

        face_area_2d_inv = area_poly_v2(face_vcos_2d, uint(face.size()));
        /* In case we have a null-area degenerated face... */
        face_area_2d_inv = 1.0f / max_ff(face_area_2d_inv, 1e-9f);

        /* Tessellate our face. */
        if (face.size() == 3) {
          tri_vidx_2d[0][0] = 0;
          tri_vidx_2d[0][1] = 1;
          tri_vidx_2d[0][2] = 2;
        }
        if (face.size() == 4) {
          tri_vidx_2d[0][0] = 0;
          tri_vidx_2d[0][1] = 1;
          tri_vidx_2d[0][2] = 2;
          tri_vidx_2d[1][0] = 0;
          tri_vidx_2d[1][1] = 2;
          tri_vidx_2d[1][2] = 3;
        }
        else {
          BLI_polyfill_calc(face_vcos_2d, uint(face.size()), -1, (uint(*)[3])tri_vidx_2d);
        }

        for (j = 0; j < tris_num; j++) {
          float *v1 = face_vcos_2d[tri_vidx_2d[j][0]];
          float *v2 = face_vcos_2d[tri_vidx_2d[j][1]];
          float *v3 = face_vcos_2d[tri_vidx_2d[j][2]];
          int rays_num;

          /* All this allows us to get 'absolute' number of rays for each tri,
           * avoiding accumulating errors over iterations, and helping better even distribution. */
          done_area += area_tri_v2(v1, v2, v3);
          rays_num = max_ii(int(float(tot_rays) * done_area * face_area_2d_inv + 0.5f) - done_rays,
                            0);
          done_rays += rays_num;

          while (rays_num--) {
            int n = (ray_radius > 0.0f) ? MREMAP_RAYCAST_APPROXIMATE_NR : 1;
            float w = 1.0f;

            BLI_rng_get_tri_sample_float_v2(rng, v1, v2, v3, tmp_co);

            tmp_co[2] = faces_dst_2d_z;
            mul_m3_v3(from_pnor_2d_mat, tmp_co);

            /* At this point, tmp_co is a point on our face surface, in mesh_src space! */
            while (n--) {
              if (mesh_remap_bvhtree_query_raycast(
                      &treedata, &rayhit, tmp_co, tmp_no, ray_radius / w, max_dist, &hit_dist))
              {
                const int face_index = tri_faces[rayhit.index];
                weights[face_index] += w;
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
          for (j = 0; j < int(numfaces_src); j++) {
            if (!weights[j]) {
              continue;
            }
            /* NOTE: sources_num is always <= j! */
            weights[sources_num] = weights[j] / totweights;
            indices[sources_num] = j;
            sources_num++;
          }
          mesh_remap_item_define(
              r_map, int(i), hit_dist_accum / totweights, 0, sources_num, indices, weights);
        }
        else {
          /* No source for this dest face! */
          BKE_mesh_remap_item_define_invalid(r_map, int(i));
        }
      }

      MEM_freeN(tri_vidx_2d);
      MEM_freeN(face_vcos_2d);
      MEM_freeN(indices);
      MEM_freeN(weights);
      BLI_rng_free(rng);
    }
    else {
      CLOG_WARN(&LOG, "Unsupported mesh-to-mesh face mapping mode (%d)!", mode);
      memset(r_map->items, 0, sizeof(*r_map->items) * size_t(faces_dst.size()));
    }
  }
}

#undef MREMAP_RAYCAST_APPROXIMATE_NR
#undef MREMAP_RAYCAST_APPROXIMATE_FAC
#undef MREMAP_RAYCAST_TRI_SAMPLES_MIN
#undef MREMAP_RAYCAST_TRI_SAMPLES_MAX
#undef MREMAP_DEFAULT_BUFSIZE

/** \} */
