/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory.h>

#include "DNA_gpencil_modifier_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_solvers.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_attribute.hh"
#include "BKE_modifier.hh"
#include "BKE_shrinkwrap.hh"
#include "BKE_subdiv.hh"

#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh.hh" /* for OMP limits. */
#include "BKE_mesh_wrapper.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_deform.hh"

#include "DEG_depsgraph_query.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/* for timing... */
#if 0
#  include "BLI_time_utildefines.h"
#else
#  define TIMEIT_BENCH(expr, id) (expr)
#endif

/* Util macros */
#define OUT_OF_MEMORY() ((void)printf("Shrinkwrap: Out of memory\n"))

struct ShrinkwrapCalcData {
  ShrinkwrapModifierData *smd; /* shrinkwrap modifier data */

  Object *ob; /* object we are applying shrinkwrap to */

  float (*vert_positions)[3]; /* Array of verts being projected. */
  blender::Span<blender::float3> vert_normals;
  /* Vertices being shrink-wrapped. */
  float (*vertexCos)[3];
  int numVerts;

  const MDeformVert *dvert; /* Pointer to mdeform array */
  int vgroup;               /* Vertex group num */
  bool invert_vgroup;       /* invert vertex group influence */

  Mesh *target;                /* mesh we are shrinking to */
  SpaceTransform local2target; /* transform to move between local and target space */
  ShrinkwrapTreeData *tree;    /* mesh BVH tree data */

  Object *aux_target;

  float keepDist; /* Distance to keep above target surface (units are in local space) */
};

struct ShrinkwrapCalcCBData {
  ShrinkwrapCalcData *calc;

  ShrinkwrapTreeData *tree;
  ShrinkwrapTreeData *aux_tree;

  float *proj_axis;
  SpaceTransform *local2aux;
};

bool BKE_shrinkwrap_needs_normals(int shrinkType, int shrinkMode)
{
  return (shrinkType == MOD_SHRINKWRAP_TARGET_PROJECT) ||
         (shrinkType != MOD_SHRINKWRAP_NEAREST_VERTEX &&
          shrinkMode == MOD_SHRINKWRAP_ABOVE_SURFACE);
}

bool BKE_shrinkwrap_init_tree(
    ShrinkwrapTreeData *data, Mesh *mesh, int shrinkType, int shrinkMode, bool force_normals)
{
  using namespace blender::bke;
  *data = {};

  if (mesh == nullptr) {
    return false;
  }

  /* We could create a BVH tree from the edit mesh,
   * however accessing normals from the face/loop normals gets more involved.
   * Convert mesh data since this isn't typically used in edit-mode. */
  BKE_mesh_wrapper_ensure_mdata(mesh);

  if (mesh->verts_num <= 0) {
    return false;
  }

  data->mesh = mesh;
  data->edges = mesh->edges();
  data->faces = mesh->faces();
  data->corner_edges = mesh->corner_edges();
  data->vert_normals = mesh->vert_normals();
  const AttributeAccessor attributes = mesh->attributes();
  data->sharp_faces = *attributes.lookup<bool>("sharp_face", AttrDomain::Face);

  if (shrinkType == MOD_SHRINKWRAP_NEAREST_VERTEX) {
    data->treeData = mesh->bvh_verts();
    data->bvh = data->treeData.tree;
    return data->bvh != nullptr;
  }

  if (mesh->faces_num <= 0) {
    return false;
  }

  data->treeData = mesh->bvh_corner_tris();
  data->bvh = data->treeData.tree;

  if (data->bvh == nullptr) {
    return false;
  }

  if (force_normals || BKE_shrinkwrap_needs_normals(shrinkType, shrinkMode)) {
    data->face_normals = mesh->face_normals();
    if (mesh->normals_domain() == blender::bke::MeshNormalDomain::Corner) {
      data->corner_normals = mesh->corner_normals();
    }
  }

  if (shrinkType == MOD_SHRINKWRAP_TARGET_PROJECT) {
    data->boundary = &blender::bke::shrinkwrap::boundary_cache_ensure(*mesh);
  }

  return true;
}

void BKE_shrinkwrap_free_tree(ShrinkwrapTreeData * /*data*/) {}

namespace blender::bke::shrinkwrap {

/* Accumulate edge for average boundary edge direction. */
static void merge_vert_dir(ShrinkwrapBoundaryVertData *vdata,
                           MutableSpan<int8_t> status,
                           int index,
                           const float edge_dir[3],
                           signed char side)
{
  BLI_assert(index >= 0);
  float *direction = vdata[index].direction;

  /* Invert the direction vector if either:
   * - This is the second edge and both edges have the vertex as start or end.
   * - For third and above, if it points in the wrong direction.
   */
  if (status[index] >= 0 ? status[index] == side : dot_v3v3(direction, edge_dir) < 0) {
    sub_v3_v3(direction, edge_dir);
  }
  else {
    add_v3_v3(direction, edge_dir);
  }

  status[index] = (status[index] == 0) ? side : -1;
}

static ShrinkwrapBoundaryData shrinkwrap_build_boundary_data(const Mesh &mesh)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<int2> edges = mesh.edges();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int> corner_edges = mesh.corner_edges();

  /* Count faces per edge (up to 2). */
  Array<int8_t> edge_mode(edges.size(), 0);

  for (const int edge : corner_edges) {
    if (edge_mode[edge] < 2) {
      edge_mode[edge]++;
    }
  }

  /* Build the boundary edge bitmask. */
  BitVector<> edge_is_boundary(mesh.edges_num, false);

  int num_boundary_edges = 0;
  for (const int64_t i : edges.index_range()) {
    if (edge_mode[i] == 1) {
      edge_is_boundary[i].set();
      num_boundary_edges++;
    }
  }

  /* If no boundary, return nullptr. */
  if (num_boundary_edges == 0) {
    return {};
  }

  /* Allocate the data object. */
  ShrinkwrapBoundaryData data;

  /* Build the boundary corner_tris bit-mask. */
  const Span<int3> corner_tris = mesh.corner_tris();

  BitVector<> tri_has_boundary(corner_tris.size(), false);

  for (const int64_t i : corner_tris.index_range()) {
    const int3 real_edges = bke::mesh::corner_tri_get_real_edges(
        edges, corner_verts, corner_edges, corner_tris[i]);

    for (int j = 0; j < 3; j++) {
      if (real_edges[j] >= 0 && edge_is_boundary[real_edges[j]]) {
        tri_has_boundary[i].set();
        break;
      }
    }
  }

  /* Find boundary vertices and build a mapping table for compact storage of data. */
  Array<int> vert_boundary_id(mesh.verts_num, 0);

  for (const int64_t i : edges.index_range()) {
    if (edge_is_boundary[i]) {
      const int2 &edge = edges[i];
      vert_boundary_id[edge[0]] = 1;
      vert_boundary_id[edge[1]] = 1;
    }
  }

  int boundary_verts_num = 0;
  for (const int64_t i : positions.index_range()) {
    vert_boundary_id[i] = (vert_boundary_id[i] != 0) ? boundary_verts_num++ : -1;
  }

  /* Compute average directions. */
  Array<ShrinkwrapBoundaryVertData> boundary_verts(boundary_verts_num);

  Array<int8_t> vert_status(boundary_verts_num);
  for (const int64_t i : edges.index_range()) {
    if (edge_is_boundary[i]) {
      const int2 &edge = edges[i];

      float dir[3];
      sub_v3_v3v3(dir, positions[edge[1]], positions[edge[0]]);
      normalize_v3(dir);

      merge_vert_dir(boundary_verts.data(), vert_status, vert_boundary_id[edge[0]], dir, 1);
      merge_vert_dir(boundary_verts.data(), vert_status, vert_boundary_id[edge[1]], dir, 2);
    }
  }

  /* Finalize average direction and compute normal. */
  const Span<float3> vert_normals = mesh.vert_normals();
  for (const int64_t i : positions.index_range()) {
    int bidx = vert_boundary_id[i];

    if (bidx >= 0) {
      ShrinkwrapBoundaryVertData *vdata = &boundary_verts[bidx];
      float tmp[3];

      normalize_v3(vdata->direction);

      cross_v3_v3v3(tmp, vert_normals[i], vdata->direction);
      cross_v3_v3v3(vdata->normal_plane, tmp, vert_normals[i]);
      normalize_v3(vdata->normal_plane);
    }
  }

  data.edge_is_boundary = std::move(edge_is_boundary);
  data.tri_has_boundary = std::move(tri_has_boundary);
  data.vert_boundary_id = std::move(vert_boundary_id);
  data.boundary_verts = std::move(boundary_verts);

  return data;
}

const ShrinkwrapBoundaryData &boundary_cache_ensure(const Mesh &mesh)
{
  mesh.runtime->shrinkwrap_boundary_cache.ensure(
      [&](ShrinkwrapBoundaryData &r_data) { r_data = shrinkwrap_build_boundary_data(mesh); });
  return mesh.runtime->shrinkwrap_boundary_cache.data();
}

}  // namespace blender::bke::shrinkwrap

/**
 * Shrink-wrap to the nearest vertex
 *
 * it builds a BVH-tree of vertices we can attach to and then
 * for each vertex performs a nearest vertex search on the tree.
 */
static void shrinkwrap_calc_nearest_vertex_cb_ex(void *__restrict userdata,
                                                 const int i,
                                                 const TaskParallelTLS *__restrict tls)
{
  ShrinkwrapCalcCBData *data = static_cast<ShrinkwrapCalcCBData *>(userdata);

  ShrinkwrapCalcData *calc = data->calc;
  blender::bke::BVHTreeFromMesh *treeData = &data->tree->treeData;
  BVHTreeNearest *nearest = static_cast<BVHTreeNearest *>(tls->userdata_chunk);

  float *co = calc->vertexCos[i];
  float tmp_co[3];
  float weight = BKE_defvert_array_find_weight_safe(
      calc->dvert, i, calc->vgroup, calc->invert_vgroup);

  if (weight == 0.0f) {
    return;
  }

  /* Convert the vertex to tree coordinates */
  if (calc->vert_positions) {
    copy_v3_v3(tmp_co, calc->vert_positions[i]);
  }
  else {
    copy_v3_v3(tmp_co, co);
  }
  BLI_space_transform_apply(&calc->local2target, tmp_co);

  /* Use local proximity heuristics (to reduce the nearest search)
   *
   * If we already had an hit before.. we assume this vertex is going to have a close hit to that
   * other vertex so we can initiate the "nearest.dist" with the expected value to that last hit.
   * This will lead in pruning of the search tree. */
  if (nearest->index != -1) {
    nearest->dist_sq = len_squared_v3v3(tmp_co, nearest->co);
  }
  else {
    nearest->dist_sq = FLT_MAX;
  }

  BLI_bvhtree_find_nearest(treeData->tree, tmp_co, nearest, treeData->nearest_callback, treeData);

  /* Found the nearest vertex */
  if (nearest->index != -1) {
    /* Adjusting the vertex weight,
     * so that after interpolating it keeps a certain distance from the nearest position */
    if (nearest->dist_sq > FLT_EPSILON) {
      const float dist = sqrtf(nearest->dist_sq);
      weight *= (dist - calc->keepDist) / dist;
    }

    /* Convert the coordinates back to mesh coordinates */
    copy_v3_v3(tmp_co, nearest->co);
    BLI_space_transform_invert(&calc->local2target, tmp_co);

    interp_v3_v3v3(co, co, tmp_co, weight); /* linear interpolation */
  }
}

static void shrinkwrap_calc_nearest_vertex(ShrinkwrapCalcData *calc)
{
  BVHTreeNearest nearest = NULL_BVHTreeNearest;

  /* Setup nearest */
  nearest.index = -1;
  nearest.dist_sq = FLT_MAX;

  ShrinkwrapCalcCBData data{};
  data.calc = calc;
  data.tree = calc->tree;
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (calc->numVerts > 10000);
  settings.userdata_chunk = &nearest;
  settings.userdata_chunk_size = sizeof(nearest);
  BLI_task_parallel_range(
      0, calc->numVerts, &data, shrinkwrap_calc_nearest_vertex_cb_ex, &settings);
}

bool BKE_shrinkwrap_project_normal(char options,
                                   const float vert[3],
                                   const float dir[3],
                                   const float ray_radius,
                                   const SpaceTransform *transf,
                                   ShrinkwrapTreeData *tree,
                                   BVHTreeRayHit *hit)
{
  /* don't use this because this dist value could be incompatible
   * this value used by the callback for comparing previous/new dist values.
   * also, at the moment there is no need to have a corrected 'dist' value */
  // #define USE_DIST_CORRECT

  float tmp_co[3], tmp_no[3];
  const float *co, *no;
  BVHTreeRayHit hit_tmp;

  /* Copy from hit (we need to convert hit rays from one space coordinates to the other */
  memcpy(&hit_tmp, hit, sizeof(hit_tmp));

  /* Apply space transform (TODO readjust dist) */
  if (transf) {
    copy_v3_v3(tmp_co, vert);
    BLI_space_transform_apply(transf, tmp_co);
    co = tmp_co;

    copy_v3_v3(tmp_no, dir);
    BLI_space_transform_apply_normal(transf, tmp_no);
    no = tmp_no;

#ifdef USE_DIST_CORRECT
    hit_tmp.dist *= mat4_to_scale(((SpaceTransform *)transf)->local2target);
#endif
  }
  else {
    co = vert;
    no = dir;
  }

  hit_tmp.index = -1;

  BLI_bvhtree_ray_cast(
      tree->bvh, co, no, ray_radius, &hit_tmp, tree->treeData.raycast_callback, &tree->treeData);

  if (hit_tmp.index != -1) {
    /* invert the normal first so face culling works on rotated objects */
    if (transf) {
      BLI_space_transform_invert_normal(transf, hit_tmp.no);
    }

    if (options & MOD_SHRINKWRAP_CULL_TARGET_MASK) {
      /* Apply back-face. */
      const float dot = dot_v3v3(dir, hit_tmp.no);
      if (((options & MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE) && dot <= 0.0f) ||
          ((options & MOD_SHRINKWRAP_CULL_TARGET_BACKFACE) && dot >= 0.0f))
      {
        return false; /* Ignore hit */
      }
    }

    if (transf) {
      /* Inverting space transform (TODO: make coherent with the initial dist readjust). */
      BLI_space_transform_invert(transf, hit_tmp.co);
#ifdef USE_DIST_CORRECT
      hit_tmp.dist = len_v3v3(vert, hit_tmp.co);
#endif
    }

    BLI_assert(hit_tmp.dist <= hit->dist);

    memcpy(hit, &hit_tmp, sizeof(hit_tmp));
    return true;
  }
  return false;
}

static void shrinkwrap_calc_normal_projection_cb_ex(void *__restrict userdata,
                                                    const int i,
                                                    const TaskParallelTLS *__restrict tls)
{
  ShrinkwrapCalcCBData *data = static_cast<ShrinkwrapCalcCBData *>(userdata);

  ShrinkwrapCalcData *calc = data->calc;
  ShrinkwrapTreeData *tree = data->tree;
  ShrinkwrapTreeData *aux_tree = data->aux_tree;

  float *proj_axis = data->proj_axis;
  SpaceTransform *local2aux = data->local2aux;

  BVHTreeRayHit *hit = static_cast<BVHTreeRayHit *>(tls->userdata_chunk);

  const float proj_limit_squared = calc->smd->projLimit * calc->smd->projLimit;
  float *co = calc->vertexCos[i];
  const float *tmp_co, *tmp_no;
  float weight = BKE_defvert_array_find_weight_safe(
      calc->dvert, i, calc->vgroup, calc->invert_vgroup);

  if (weight == 0.0f) {
    return;
  }

  if (calc->vert_positions != nullptr && calc->smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL)
  {
    /* calc->vert_positions contains verts from evaluated mesh. */
    /* These coordinates are deformed by vertexCos only for normal projection
     * (to get correct normals) for other cases calc->verts contains undeformed coordinates and
     * vertexCos should be used */
    tmp_co = calc->vert_positions[i];
    tmp_no = calc->vert_normals[i];
  }
  else {
    tmp_co = co;
    tmp_no = proj_axis;
  }

  hit->index = -1;

  /* TODO: we should use FLT_MAX here, but sweep-sphere code isn't prepared for that. */
  hit->dist = BVH_RAYCAST_DIST_MAX;

  bool is_aux = false;

  /* Project over positive direction of axis. */
  if (calc->smd->shrinkOpts & MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR) {
    if (aux_tree) {
      if (BKE_shrinkwrap_project_normal(0, tmp_co, tmp_no, 0.0, local2aux, aux_tree, hit)) {
        is_aux = true;
      }
    }

    if (BKE_shrinkwrap_project_normal(
            calc->smd->shrinkOpts, tmp_co, tmp_no, 0.0, &calc->local2target, tree, hit))
    {
      is_aux = false;
    }
  }

  /* Project over negative direction of axis */
  if (calc->smd->shrinkOpts & MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR) {
    float inv_no[3];
    negate_v3_v3(inv_no, tmp_no);

    char options = calc->smd->shrinkOpts;

    if ((options & MOD_SHRINKWRAP_INVERT_CULL_TARGET) &&
        (options & MOD_SHRINKWRAP_CULL_TARGET_MASK))
    {
      options ^= MOD_SHRINKWRAP_CULL_TARGET_MASK;
    }

    if (aux_tree) {
      if (BKE_shrinkwrap_project_normal(0, tmp_co, inv_no, 0.0, local2aux, aux_tree, hit)) {
        is_aux = true;
      }
    }

    if (BKE_shrinkwrap_project_normal(
            options, tmp_co, inv_no, 0.0, &calc->local2target, tree, hit))
    {
      is_aux = false;
    }
  }

  /* don't set the initial dist (which is more efficient),
   * because its calculated in the targets space, we want the dist in our own space */
  if (proj_limit_squared != 0.0f) {
    if (hit->index != -1 && len_squared_v3v3(hit->co, co) > proj_limit_squared) {
      hit->index = -1;
    }
  }

  if (hit->index != -1) {
    if (is_aux) {
      BKE_shrinkwrap_snap_point_to_surface(aux_tree,
                                           local2aux,
                                           calc->smd->shrinkMode,
                                           hit->index,
                                           hit->co,
                                           hit->no,
                                           calc->keepDist,
                                           tmp_co,
                                           hit->co);
    }
    else {
      BKE_shrinkwrap_snap_point_to_surface(tree,
                                           &calc->local2target,
                                           calc->smd->shrinkMode,
                                           hit->index,
                                           hit->co,
                                           hit->no,
                                           calc->keepDist,
                                           tmp_co,
                                           hit->co);
    }

    interp_v3_v3v3(co, co, hit->co, weight);
  }
}

static void shrinkwrap_calc_normal_projection(ShrinkwrapCalcData *calc)
{
  /* Options about projection direction */
  float proj_axis[3] = {0.0f, 0.0f, 0.0f};

  /* Ray-cast and tree stuff. */

  /** \note 'hit.dist' is kept in the targets space, this is only used
   * for finding the best hit, to get the real dist,
   * measure the len_v3v3() from the input coord to hit.co */
  BVHTreeRayHit hit;

  /* auxiliary target */
  Mesh *auxMesh = nullptr;
  ShrinkwrapTreeData *aux_tree = nullptr;
  ShrinkwrapTreeData aux_tree_stack;
  SpaceTransform local2aux;

  /* If the user doesn't allows to project in any direction of projection axis
   * then there's nothing todo. */
  if ((calc->smd->shrinkOpts &
       (MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR | MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR)) == 0)
  {
    return;
  }

  /* Prepare data to retrieve the direction in which we should project each vertex */
  if (calc->smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL) {
    if (calc->vert_positions == nullptr) {
      return;
    }
  }
  else {
    /* The code supports any axis that is a combination of X,Y,Z
     * although currently UI only allows to set the 3 different axis */
    if (calc->smd->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS) {
      proj_axis[0] = 1.0f;
    }
    if (calc->smd->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS) {
      proj_axis[1] = 1.0f;
    }
    if (calc->smd->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS) {
      proj_axis[2] = 1.0f;
    }

    normalize_v3(proj_axis);

    /* Invalid projection direction */
    if (len_squared_v3(proj_axis) < FLT_EPSILON) {
      return;
    }
  }

  if (calc->aux_target) {
    auxMesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(calc->aux_target);
    if (!auxMesh) {
      return;
    }
    BLI_SPACE_TRANSFORM_SETUP(&local2aux, calc->ob, calc->aux_target);
  }

  if (BKE_shrinkwrap_init_tree(
          &aux_tree_stack, auxMesh, calc->smd->shrinkType, calc->smd->shrinkMode, false))
  {
    aux_tree = &aux_tree_stack;
  }

  /* After successfully build the trees, start projection vertices. */
  ShrinkwrapCalcCBData data{};
  data.calc = calc;
  data.tree = calc->tree;
  data.aux_tree = aux_tree;
  data.proj_axis = proj_axis;
  data.local2aux = &local2aux;
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (calc->numVerts > 10000);
  settings.userdata_chunk = &hit;
  settings.userdata_chunk_size = sizeof(hit);
  BLI_task_parallel_range(
      0, calc->numVerts, &data, shrinkwrap_calc_normal_projection_cb_ex, &settings);

  /* free data structures */
  if (aux_tree) {
    BKE_shrinkwrap_free_tree(aux_tree);
  }
}

/*
 * Shrinkwrap Target Surface Project mode
 *
 * It uses Newton's method to find a surface location with its
 * smooth normal pointing at the original point.
 *
 * The equation system on barycentric weights and normal multiplier:
 *
 *   (w0*V0 + w1*V1 + w2*V2) + l * (w0*N0 + w1*N1 + w2*N2) - CO = 0
 *   w0 + w1 + w2 = 1
 *
 * The actual solution vector is [ w0, w1, l ], with w2 eliminated.
 */

// #define TRACE_TARGET_PROJECT

struct TargetProjectTriData {
  const float **vtri_co;
  const float (*vtri_no)[3];
  const float *point_co;

  float n0_minus_n2[3], n1_minus_n2[3];
  float c0_minus_c2[3], c1_minus_c2[3];

  /* Current interpolated position and normal. */
  float co_interp[3], no_interp[3];
};

/* Computes the deviation of the equation system from goal. */
static void target_project_tri_deviation(void *userdata, const float x[3], float r_delta[3])
{
  TargetProjectTriData *data = static_cast<TargetProjectTriData *>(userdata);

  const float w[3] = {x[0], x[1], 1.0f - x[0] - x[1]};
  interp_v3_v3v3v3(data->co_interp, data->vtri_co[0], data->vtri_co[1], data->vtri_co[2], w);
  interp_v3_v3v3v3(data->no_interp, data->vtri_no[0], data->vtri_no[1], data->vtri_no[2], w);

  madd_v3_v3v3fl(r_delta, data->co_interp, data->no_interp, x[2]);
  sub_v3_v3(r_delta, data->point_co);
}

/* Computes the Jacobian matrix of the equation system. */
static void target_project_tri_jacobian(void *userdata, const float x[3], float r_jacobian[3][3])
{
  TargetProjectTriData *data = static_cast<TargetProjectTriData *>(userdata);

  madd_v3_v3v3fl(r_jacobian[0], data->c0_minus_c2, data->n0_minus_n2, x[2]);
  madd_v3_v3v3fl(r_jacobian[1], data->c1_minus_c2, data->n1_minus_n2, x[2]);

  copy_v3_v3(r_jacobian[2], data->vtri_no[2]);
  madd_v3_v3fl(r_jacobian[2], data->n0_minus_n2, x[0]);
  madd_v3_v3fl(r_jacobian[2], data->n1_minus_n2, x[1]);
}

/* Clamp barycentric weights to the triangle. */
static void target_project_tri_clamp(float x[3])
{
  x[0] = std::max(x[0], 0.0f);
  x[1] = std::max(x[1], 0.0f);
  if (x[0] + x[1] > 1.0f) {
    x[0] = x[0] / (x[0] + x[1]);
    x[1] = 1.0f - x[0];
  }
}

/* Correct the Newton's method step to keep the coordinates within the triangle. */
static bool target_project_tri_correct(void * /*userdata*/,
                                       const float x[3],
                                       float step[3],
                                       float x_next[3])
{
  /* Insignificant correction threshold */
  const float epsilon = 1e-5f;
  /* Dot product threshold for checking if step is 'clearly' pointing outside. */
  const float dir_epsilon = 0.5f;
  bool fixed = false, locked = false;

  /* The barycentric coordinate domain is a triangle bounded by
   * the X and Y axes, plus the x+y=1 diagonal. First, clamp the
   * movement against the diagonal. Note that step is subtracted. */
  float sum = x[0] + x[1];
  float sstep = -(step[0] + step[1]);

  if (sum + sstep > 1.0f) {
    float ldist = 1.0f - sum;

    /* If already at the boundary, slide along it. */
    if (ldist < epsilon * float(M_SQRT2)) {
      float step_len = len_v2(step);

      /* Abort if the solution is clearly outside the domain. */
      if (step_len > epsilon && sstep > step_len * dir_epsilon * float(M_SQRT2)) {
        return false;
      }

      /* Project the new position onto the diagonal. */
      add_v2_fl(step, (sum + sstep - 1.0f) * 0.5f);
      fixed = locked = true;
    }
    else {
      /* Scale a significant step down to arrive at the boundary. */
      mul_v3_fl(step, ldist / sstep);
      fixed = true;
    }
  }

  /* Weight 0 and 1 boundary checks - along axis. */
  for (int i = 0; i < 2; i++) {
    if (step[i] > x[i]) {
      /* If already at the boundary, slide along it. */
      if (x[i] < epsilon) {
        float step_len = len_v2(step);

        /* Abort if the solution is clearly outside the domain. */
        if (step_len > epsilon && (locked || step[i] > step_len * dir_epsilon)) {
          return false;
        }

        /* Reset precision errors to stay at the boundary. */
        step[i] = x[i];
        fixed = true;
      }
      else {
        /* Scale a significant step down to arrive at the boundary. */
        mul_v3_fl(step, x[i] / step[i]);
        fixed = true;
      }
    }
  }

  /* Recompute and clamp the new coordinates after step correction. */
  if (fixed) {
    sub_v3_v3v3(x_next, x, step);
    target_project_tri_clamp(x_next);
  }

  return true;
}

static bool target_project_solve_point_tri(const float *vtri_co[3],
                                           const float vtri_no[3][3],
                                           const float point_co[3],
                                           const float hit_co[3],
                                           float hit_dist_sq,
                                           float r_hit_co[3],
                                           float r_hit_no[3])
{
  float x[3], tmp[3];
  float dist = sqrtf(hit_dist_sq);
  float magnitude_estimate = dist + len_manhattan_v3(vtri_co[0]) + len_manhattan_v3(vtri_co[1]) +
                             len_manhattan_v3(vtri_co[2]);
  float epsilon = magnitude_estimate * 1.0e-6f;

  /* Initial solution vector: barycentric weights plus distance along normal. */
  interp_weights_tri_v3(x, UNPACK3(vtri_co), hit_co);

  interp_v3_v3v3v3(r_hit_no, UNPACK3(vtri_no), x);
  sub_v3_v3v3(tmp, point_co, hit_co);

  x[2] = (dot_v3v3(tmp, r_hit_no) < 0) ? -dist : dist;

  /* Solve the equations iteratively. */
  TargetProjectTriData tri_data{};
  tri_data.vtri_co = vtri_co;
  tri_data.vtri_no = vtri_no;
  tri_data.point_co = point_co;

  sub_v3_v3v3(tri_data.n0_minus_n2, vtri_no[0], vtri_no[2]);
  sub_v3_v3v3(tri_data.n1_minus_n2, vtri_no[1], vtri_no[2]);
  sub_v3_v3v3(tri_data.c0_minus_c2, vtri_co[0], vtri_co[2]);
  sub_v3_v3v3(tri_data.c1_minus_c2, vtri_co[1], vtri_co[2]);

  target_project_tri_clamp(x);

#ifdef TRACE_TARGET_PROJECT
  const bool trace = true;
#else
  const bool trace = false;
#endif

  bool ok = BLI_newton3d_solve(target_project_tri_deviation,
                               target_project_tri_jacobian,
                               target_project_tri_correct,
                               &tri_data,
                               epsilon,
                               20,
                               trace,
                               x,
                               x);

  if (ok) {
    copy_v3_v3(r_hit_co, tri_data.co_interp);
    copy_v3_v3(r_hit_no, tri_data.no_interp);

    return true;
  }

  return false;
}

static bool update_hit(BVHTreeNearest *nearest,
                       int index,
                       const float co[3],
                       const float hit_co[3],
                       const float hit_no[3])
{
  float dist_sq = len_squared_v3v3(hit_co, co);

  if (dist_sq < nearest->dist_sq) {
#ifdef TRACE_TARGET_PROJECT
    printf(
        "#=#=#> %d (%.3f,%.3f,%.3f) %g < %g\n", index, UNPACK3(hit_co), dist_sq, nearest->dist_sq);
#endif
    nearest->index = index;
    nearest->dist_sq = dist_sq;
    copy_v3_v3(nearest->co, hit_co);
    normalize_v3_v3(nearest->no, hit_no);
    return true;
  }

  return false;
}

/* Target projection on a non-manifold boundary edge -
 * treats it like an infinitely thin cylinder. */
static void target_project_edge(const ShrinkwrapTreeData *tree,
                                int index,
                                const float co[3],
                                BVHTreeNearest *nearest,
                                int eidx)
{
  const blender::bke::BVHTreeFromMesh *data = &tree->treeData;
  const blender::int2 &edge = tree->edges[eidx];
  const float *vedge_co[2] = {data->vert_positions[edge[0]], data->vert_positions[edge[1]]};

#ifdef TRACE_TARGET_PROJECT
  printf("EDGE %d (%.3f,%.3f,%.3f) (%.3f,%.3f,%.3f)\n",
         eidx,
         UNPACK3(vedge_co[0]),
         UNPACK3(vedge_co[1]));
#endif

  /* Retrieve boundary vertex IDs */
  const int *vert_boundary_id = tree->boundary->vert_boundary_id.data();
  const int bid1 = vert_boundary_id[edge[0]], bid2 = vert_boundary_id[edge[1]];

  if (bid1 < 0 || bid2 < 0) {
    return;
  }

  /* Retrieve boundary vertex normals and align them to direction. */
  const ShrinkwrapBoundaryVertData *boundary_verts = tree->boundary->boundary_verts.data();
  float vedge_dir[2][3], dir[3];

  copy_v3_v3(vedge_dir[0], boundary_verts[bid1].normal_plane);
  copy_v3_v3(vedge_dir[1], boundary_verts[bid2].normal_plane);

  sub_v3_v3v3(dir, vedge_co[1], vedge_co[0]);

  if (dot_v3v3(boundary_verts[bid1].direction, dir) < 0) {
    negate_v3(vedge_dir[0]);
  }
  if (dot_v3v3(boundary_verts[bid2].direction, dir) < 0) {
    negate_v3(vedge_dir[1]);
  }

  /* Solve a quadratic equation: `lerp(d0,d1,x) * (co - lerp(v0,v1,x)) = 0`. */
  const float d0v0 = dot_v3v3(vedge_dir[0], vedge_co[0]),
              d0v1 = dot_v3v3(vedge_dir[0], vedge_co[1]);
  const float d1v0 = dot_v3v3(vedge_dir[1], vedge_co[0]),
              d1v1 = dot_v3v3(vedge_dir[1], vedge_co[1]);
  const float d0co = dot_v3v3(vedge_dir[0], co);

  /* Checked for non-zero prevent divide by zero when calculating `x`. */
  const float a = d0v1 - d0v0 + d1v0 - d1v1;
  if (a != 0.0f) {
    const float b = 2 * d0v0 - d0v1 - d0co - d1v0 + dot_v3v3(vedge_dir[1], co);
    const float c = d0co - d0v0;
    const float det = b * b - 4 * a * c;

    if (det >= 0) {
      const float epsilon = 1e-6f;
      const float sdet = sqrtf(det);
      float hit_co[3], hit_no[3];

      for (int i = (det > 0 ? 2 : 0); i >= 0; i -= 2) {
        float x = (-b + float(i - 1) * sdet) / (2.0f * a);

        if (x >= -epsilon && x <= 1.0f + epsilon) {
          CLAMP(x, 0.0f, 1.0f);

          float vedge_no[2][3];
          copy_v3_v3(vedge_no[0], tree->vert_normals[edge[0]]);
          copy_v3_v3(vedge_no[1], tree->vert_normals[edge[1]]);

          interp_v3_v3v3(hit_co, vedge_co[0], vedge_co[1], x);
          interp_v3_v3v3(hit_no, vedge_no[0], vedge_no[1], x);

          update_hit(nearest, index, co, hit_co, hit_no);
        }
      }
    }
  }
}

/* Target normal projection BVH callback - based on mesh_corner_tri_nearest_point. */
static void mesh_corner_tris_target_project(void *userdata,
                                            int index,
                                            const float co[3],
                                            BVHTreeNearest *nearest)
{
  using namespace blender;
  const ShrinkwrapTreeData *tree = (ShrinkwrapTreeData *)userdata;
  const blender::bke::BVHTreeFromMesh *data = &tree->treeData;
  const int3 &tri = data->corner_tris[index];
  const int tri_verts[3] = {
      data->corner_verts[tri[0]],
      data->corner_verts[tri[1]],
      data->corner_verts[tri[2]],
  };
  const float *vtri_co[3] = {
      data->vert_positions[tri_verts[0]],
      data->vert_positions[tri_verts[1]],
      data->vert_positions[tri_verts[2]],
  };
  float raw_hit_co[3], hit_co[3], hit_no[3], dist_sq, vtri_no[3][3];

  /* First find the closest point and bail out if it's worse than the current solution. */
  closest_on_tri_to_point_v3(raw_hit_co, co, UNPACK3(vtri_co));
  dist_sq = len_squared_v3v3(co, raw_hit_co);

#ifdef TRACE_TARGET_PROJECT
  printf("TRIANGLE %d (%.3f,%.3f,%.3f) (%.3f,%.3f,%.3f) (%.3f,%.3f,%.3f) %g %g\n",
         index,
         UNPACK3(vtri_co[0]),
         UNPACK3(vtri_co[1]),
         UNPACK3(vtri_co[2]),
         dist_sq,
         nearest->dist_sq);
#endif

  if (dist_sq >= nearest->dist_sq) {
    return;
  }

  /* Decode normals */
  copy_v3_v3(vtri_no[0], tree->vert_normals[tri_verts[0]]);
  copy_v3_v3(vtri_no[1], tree->vert_normals[tri_verts[1]]);
  copy_v3_v3(vtri_no[2], tree->vert_normals[tri_verts[2]]);

  /* Solve the equations for the triangle */
  if (target_project_solve_point_tri(vtri_co, vtri_no, co, raw_hit_co, dist_sq, hit_co, hit_no)) {
    update_hit(nearest, index, co, hit_co, hit_no);
  }
  /* Boundary edges */
  else if (tree->boundary && tree->boundary->has_boundary() &&
           tree->boundary->tri_has_boundary[index])
  {
    const BitSpan is_boundary = tree->boundary->edge_is_boundary;
    const int3 edges = bke::mesh::corner_tri_get_real_edges(
        tree->edges, data->corner_verts, tree->corner_edges, tri);

    for (int i = 0; i < 3; i++) {
      if (edges[i] >= 0 && is_boundary[edges[i]]) {
        target_project_edge(tree, index, co, nearest, edges[i]);
      }
    }
  }
}

void BKE_shrinkwrap_find_nearest_surface(ShrinkwrapTreeData *tree,
                                         BVHTreeNearest *nearest,
                                         float co[3],
                                         int type)
{
  blender::bke::BVHTreeFromMesh *treeData = &tree->treeData;

  if (type == MOD_SHRINKWRAP_TARGET_PROJECT) {
#ifdef TRACE_TARGET_PROJECT
    printf("\n====== TARGET PROJECT START ======\n");
#endif

    BLI_bvhtree_find_nearest_ex(
        tree->bvh, co, nearest, mesh_corner_tris_target_project, tree, BVH_NEAREST_OPTIMAL_ORDER);

#ifdef TRACE_TARGET_PROJECT
    printf("====== TARGET PROJECT END: %d %g ======\n\n", nearest->index, nearest->dist_sq);
#endif

    if (nearest->index < 0) {
      /* fall back to simple nearest */
      BLI_bvhtree_find_nearest(tree->bvh, co, nearest, treeData->nearest_callback, treeData);
    }
  }
  else {
    BLI_bvhtree_find_nearest(tree->bvh, co, nearest, treeData->nearest_callback, treeData);
  }
}

/**
 * Shrink-wrap moving vertices to the nearest surface point on the target.
 *
 * It builds a #BVHTree from the target mesh and then performs a
 * NN matches for each vertex
 */
static void shrinkwrap_calc_nearest_surface_point_cb_ex(void *__restrict userdata,
                                                        const int i,
                                                        const TaskParallelTLS *__restrict tls)
{
  ShrinkwrapCalcCBData *data = static_cast<ShrinkwrapCalcCBData *>(userdata);

  ShrinkwrapCalcData *calc = data->calc;
  BVHTreeNearest *nearest = static_cast<BVHTreeNearest *>(tls->userdata_chunk);

  float *co = calc->vertexCos[i];
  float tmp_co[3];
  float weight = BKE_defvert_array_find_weight_safe(
      calc->dvert, i, calc->vgroup, calc->invert_vgroup);

  if (weight == 0.0f) {
    return;
  }

  /* Convert the vertex to tree coordinates */
  if (calc->vert_positions) {
    copy_v3_v3(tmp_co, calc->vert_positions[i]);
  }
  else {
    copy_v3_v3(tmp_co, co);
  }
  BLI_space_transform_apply(&calc->local2target, tmp_co);

  /* Use local proximity heuristics (to reduce the nearest search)
   *
   * If we already had an hit before.. we assume this vertex is going to have a close hit to that
   * other vertex so we can initiate the "nearest.dist" with the expected value to that last hit.
   * This will lead in pruning of the search tree. */
  if (nearest->index != -1) {
    if (calc->smd->shrinkType == MOD_SHRINKWRAP_TARGET_PROJECT) {
      /* Heuristic doesn't work because of additional restrictions. */
      nearest->index = -1;
      nearest->dist_sq = FLT_MAX;
    }
    else {
      nearest->dist_sq = len_squared_v3v3(tmp_co, nearest->co);
    }
  }
  else {
    nearest->dist_sq = FLT_MAX;
  }

  BKE_shrinkwrap_find_nearest_surface(data->tree, nearest, tmp_co, calc->smd->shrinkType);

  /* Found the nearest vertex */
  if (nearest->index != -1) {
    BKE_shrinkwrap_snap_point_to_surface(data->tree,
                                         nullptr,
                                         calc->smd->shrinkMode,
                                         nearest->index,
                                         nearest->co,
                                         nearest->no,
                                         calc->keepDist,
                                         tmp_co,
                                         tmp_co);

    /* Convert the coordinates back to mesh coordinates */
    BLI_space_transform_invert(&calc->local2target, tmp_co);
    interp_v3_v3v3(co, co, tmp_co, weight); /* linear interpolation */
  }
}

void BKE_shrinkwrap_compute_smooth_normal(const ShrinkwrapTreeData *tree,
                                          const SpaceTransform *transform,
                                          int corner_tri_idx,
                                          const float hit_co[3],
                                          const float hit_no[3],
                                          float r_no[3])
{
  using namespace blender;
  const blender::bke::BVHTreeFromMesh *treeData = &tree->treeData;
  const int3 &tri = treeData->corner_tris[corner_tri_idx];
  const int face_i = tree->mesh->corner_tri_faces()[corner_tri_idx];

  /* Interpolate smooth normals if enabled. */
  if (tree->sharp_faces.is_empty() || tree->sharp_faces[face_i]) {
    const int vert_indices[3] = {treeData->corner_verts[tri[0]],
                                 treeData->corner_verts[tri[1]],
                                 treeData->corner_verts[tri[2]]};
    float w[3], no[3][3], tmp_co[3];

    /* Custom normals. */
    if (!tree->corner_normals.is_empty()) {
      copy_v3_v3(no[0], tree->corner_normals[tri[0]]);
      copy_v3_v3(no[1], tree->corner_normals[tri[1]]);
      copy_v3_v3(no[2], tree->corner_normals[tri[2]]);
    }
    /* Ordinary vertex normals. */
    else {
      copy_v3_v3(no[0], tree->vert_normals[vert_indices[0]]);
      copy_v3_v3(no[1], tree->vert_normals[vert_indices[1]]);
      copy_v3_v3(no[2], tree->vert_normals[vert_indices[2]]);
    }

    /* Barycentric weights from hit point. */
    copy_v3_v3(tmp_co, hit_co);

    if (transform) {
      BLI_space_transform_apply(transform, tmp_co);
    }

    interp_weights_tri_v3(w,
                          treeData->vert_positions[vert_indices[0]],
                          treeData->vert_positions[vert_indices[1]],
                          treeData->vert_positions[vert_indices[2]],
                          tmp_co);

    /* Interpolate using weights. */
    interp_v3_v3v3v3(r_no, no[0], no[1], no[2], w);

    if (transform) {
      BLI_space_transform_invert_normal(transform, r_no);
    }
    else {
      normalize_v3(r_no);
    }
  }
  /* Use the face normal if flat. */
  else if (!tree->face_normals.is_empty()) {
    copy_v3_v3(r_no, tree->face_normals[face_i]);
    if (transform) {
      BLI_space_transform_invert_normal(transform, r_no);
    }
  }
  /* Finally fall back to the corner_tris normal. */
  else {
    copy_v3_v3(r_no, hit_no);
  }
}

/* Helper for MOD_SHRINKWRAP_INSIDE, MOD_SHRINKWRAP_OUTSIDE and MOD_SHRINKWRAP_OUTSIDE_SURFACE. */
static void shrinkwrap_snap_with_side(float r_point_co[3],
                                      const float point_co[3],
                                      const float hit_co[3],
                                      const float hit_no[3],
                                      float goal_dist,
                                      float forcesign,
                                      bool forcesnap)
{
  float delta[3];
  sub_v3_v3v3(delta, point_co, hit_co);

  float dist = len_v3(delta);

  /* If exactly on the surface, push out along normal */
  if (dist < FLT_EPSILON) {
    if (forcesnap || goal_dist > 0) {
      madd_v3_v3v3fl(r_point_co, hit_co, hit_no, goal_dist * forcesign);
    }
    else {
      copy_v3_v3(r_point_co, hit_co);
    }
  }
  /* Move to the correct side if needed */
  else {
    float dsign = signf(dot_v3v3(delta, hit_no));

    if (forcesign == 0.0f) {
      forcesign = dsign;
    }

    /* If on the wrong side or too close, move to correct */
    if (forcesnap || dsign * dist * forcesign < goal_dist) {
      mul_v3_fl(delta, dsign / dist);

      /* At very small distance, blend in the hit normal to stabilize math. */
      float dist_epsilon = (fabsf(goal_dist) + len_manhattan_v3(hit_co)) * 1e-4f;

      if (dist < dist_epsilon) {
#ifdef TRACE_TARGET_PROJECT
        printf("zero_factor %g = %g / %g\n", dist / dist_epsilon, dist, dist_epsilon);
#endif

        interp_v3_v3v3(delta, hit_no, delta, dist / dist_epsilon);
      }

      madd_v3_v3v3fl(r_point_co, hit_co, delta, goal_dist * forcesign);
    }
    else {
      copy_v3_v3(r_point_co, point_co);
    }
  }
}

void BKE_shrinkwrap_snap_point_to_surface(const ShrinkwrapTreeData *tree,
                                          const SpaceTransform *transform,
                                          int mode,
                                          int hit_idx,
                                          const float hit_co[3],
                                          const float hit_no[3],
                                          float goal_dist,
                                          const float point_co[3],
                                          float r_point_co[3])
{
  float tmp[3];

  switch (mode) {
    /* Offsets along the line between point_co and hit_co. */
    case MOD_SHRINKWRAP_ON_SURFACE:
      if (goal_dist != 0) {
        shrinkwrap_snap_with_side(r_point_co, point_co, hit_co, hit_no, goal_dist, 0, true);
      }
      else {
        copy_v3_v3(r_point_co, hit_co);
      }
      break;

    case MOD_SHRINKWRAP_INSIDE:
      shrinkwrap_snap_with_side(r_point_co, point_co, hit_co, hit_no, goal_dist, -1, false);
      break;

    case MOD_SHRINKWRAP_OUTSIDE:
      shrinkwrap_snap_with_side(r_point_co, point_co, hit_co, hit_no, goal_dist, +1, false);
      break;

    case MOD_SHRINKWRAP_OUTSIDE_SURFACE:
      if (goal_dist != 0) {
        shrinkwrap_snap_with_side(r_point_co, point_co, hit_co, hit_no, goal_dist, +1, true);
      }
      else {
        copy_v3_v3(r_point_co, hit_co);
      }
      break;

    /* Offsets along the normal */
    case MOD_SHRINKWRAP_ABOVE_SURFACE:
      if (goal_dist != 0) {
        BKE_shrinkwrap_compute_smooth_normal(tree, transform, hit_idx, hit_co, hit_no, tmp);
        madd_v3_v3v3fl(r_point_co, hit_co, tmp, goal_dist);
      }
      else {
        copy_v3_v3(r_point_co, hit_co);
      }
      break;

    default:
      printf("Unknown Shrinkwrap surface snap mode: %d\n", mode);
      copy_v3_v3(r_point_co, hit_co);
  }
}

static void shrinkwrap_calc_nearest_surface_point(ShrinkwrapCalcData *calc)
{
  BVHTreeNearest nearest = NULL_BVHTreeNearest;

  /* Setup nearest */
  nearest.index = -1;
  nearest.dist_sq = FLT_MAX;

  /* Find the nearest vertex */
  ShrinkwrapCalcCBData data{};
  data.calc = calc;
  data.tree = calc->tree;
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (calc->numVerts > 10000);
  settings.userdata_chunk = &nearest;
  settings.userdata_chunk_size = sizeof(nearest);
  BLI_task_parallel_range(
      0, calc->numVerts, &data, shrinkwrap_calc_nearest_surface_point_cb_ex, &settings);
}

static blender::Array<blender::float3> shrinkwrap_calc_subdivided_positions(
    Mesh *mesh, const int subdivision_level)
{
  using namespace blender;
  using namespace blender::bke;

  Array<float3> positions = mesh->vert_positions();

  subdiv::Settings settings{};
  settings.is_simple = false;
  settings.is_adaptive = false;
  settings.level = subdivision_level;
  settings.use_creases = true;

  /* Default subdivision surface modifier settings:
   * - UV Smooth:Keep Corners.
   * - BoundarySmooth: All. */
  settings.vtx_boundary_interpolation = subdiv::SUBDIV_VTX_BOUNDARY_EDGE_ONLY;
  settings.fvar_linear_interpolation =
      subdiv::SUBDIV_FVAR_LINEAR_INTERPOLATION_CORNERS_AND_JUNCTIONS;

  subdiv::Subdiv *subdiv = subdiv::update_from_mesh(nullptr, &settings, mesh);
  if (subdiv) {
    subdiv::deform_coarse_vertices(subdiv, mesh, positions);
    subdiv::free(subdiv);
  }

  return positions;
}

void shrinkwrapModifier_deform(ShrinkwrapModifierData *smd,
                               const ModifierEvalContext *ctx,
                               Scene * /*scene*/,
                               Object *ob,
                               Mesh *mesh,
                               const MDeformVert *dvert,
                               const int defgrp_index,
                               float (*vertexCos)[3],
                               int numVerts)
{
  ShrinkwrapCalcData calc = NULL_ShrinkwrapCalcData;
  blender::Array<blender::float3> subdivided_positions;

  /* remove loop dependencies on derived meshes (TODO should this be done elsewhere?) */
  if (smd->target == ob) {
    smd->target = nullptr;
  }
  if (smd->auxTarget == ob) {
    smd->auxTarget = nullptr;
  }

  /* Configure Shrinkwrap calc data */
  calc.smd = smd;
  calc.ob = ob;
  calc.numVerts = numVerts;
  calc.vertexCos = vertexCos;
  calc.dvert = dvert;
  calc.vgroup = defgrp_index;
  calc.invert_vgroup = (smd->shrinkOpts & MOD_SHRINKWRAP_INVERT_VGROUP) != 0;

  if (smd->target != nullptr) {
    Object *ob_target = DEG_get_evaluated(ctx->depsgraph, smd->target);
    calc.target = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob_target);

    /* TODO: there might be several "bugs" with non-uniform scales matrices
     * because it will no longer be nearest surface, not sphere projection
     * because space has been deformed */
    BLI_SPACE_TRANSFORM_SETUP(&calc.local2target, ob, ob_target);

    /* TODO: smd->keepDist is in global units.. must change to local */
    calc.keepDist = smd->keepDist;
  }
  calc.aux_target = DEG_get_evaluated(ctx->depsgraph, smd->auxTarget);

  if (mesh != nullptr && smd->shrinkType == MOD_SHRINKWRAP_PROJECT) {
    /* Setup arrays to get vertex positions, normals and deform weights */
    calc.vert_positions = reinterpret_cast<float (*)[3]>(mesh->vert_positions_for_write().data());
    calc.vert_normals = mesh->vert_normals();

    /* Using vertices positions/normals as if a subsurface was applied */
    if (smd->subsurfLevels) {
      subdivided_positions = shrinkwrap_calc_subdivided_positions(mesh, smd->subsurfLevels);
      calc.vert_positions = reinterpret_cast<float (*)[3]>(subdivided_positions.data());
    }
  }

  /* Projecting target defined - lets work! */
  ShrinkwrapTreeData tree;

  if (BKE_shrinkwrap_init_tree(&tree, calc.target, smd->shrinkType, smd->shrinkMode, false)) {
    calc.tree = &tree;

    switch (smd->shrinkType) {
      case MOD_SHRINKWRAP_NEAREST_SURFACE:
      case MOD_SHRINKWRAP_TARGET_PROJECT:
        TIMEIT_BENCH(shrinkwrap_calc_nearest_surface_point(&calc), deform_surface);
        break;

      case MOD_SHRINKWRAP_PROJECT:
        TIMEIT_BENCH(shrinkwrap_calc_normal_projection(&calc), deform_project);
        break;

      case MOD_SHRINKWRAP_NEAREST_VERTEX:
        TIMEIT_BENCH(shrinkwrap_calc_nearest_vertex(&calc), deform_vertex);
        break;
    }

    BKE_shrinkwrap_free_tree(&tree);
  }
}

void shrinkwrapParams_deform(const ShrinkwrapParams &params,
                             Object &object,
                             ShrinkwrapTreeData &tree,
                             const blender::Span<MDeformVert> dvert,
                             const int defgrp_index,
                             const blender::MutableSpan<blender::float3> positions)
{
  using namespace blender::bke;

  ShrinkwrapCalcData calc = NULL_ShrinkwrapCalcData;
  /* Convert params struct to use the same struct and function used with meshes. */
  ShrinkwrapModifierData smd;
  smd.target = params.target;
  smd.auxTarget = params.aux_target;
  smd.keepDist = params.keep_distance;
  smd.shrinkType = params.shrink_type;
  smd.shrinkOpts = params.shrink_options;
  smd.shrinkMode = params.shrink_mode;
  smd.projLimit = params.projection_limit;
  smd.projAxis = params.projection_axis;

  /* Configure Shrinkwrap calc data. */
  calc.smd = &smd;
  calc.ob = &object;
  calc.numVerts = int(positions.size());
  calc.vertexCos = reinterpret_cast<float (*)[3]>(positions.data());
  calc.dvert = dvert.is_empty() ? nullptr : dvert.data();
  calc.vgroup = defgrp_index;
  calc.invert_vgroup = params.invert_vertex_weights;

  BLI_SPACE_TRANSFORM_SETUP(&calc.local2target, &object, params.target);
  calc.keepDist = params.keep_distance;
  calc.tree = &tree;

  switch (params.shrink_type) {
    case MOD_SHRINKWRAP_NEAREST_SURFACE:
    case MOD_SHRINKWRAP_TARGET_PROJECT:
      TIMEIT_BENCH(shrinkwrap_calc_nearest_surface_point(&calc), gpdeform_surface);
      break;

    case MOD_SHRINKWRAP_PROJECT:
      TIMEIT_BENCH(shrinkwrap_calc_normal_projection(&calc), gpdeform_project);
      break;

    case MOD_SHRINKWRAP_NEAREST_VERTEX:
      TIMEIT_BENCH(shrinkwrap_calc_nearest_vertex(&calc), gpdeform_vertex);
      break;
  }
}

void BKE_shrinkwrap_mesh_nearest_surface_deform(Depsgraph *depsgraph,
                                                Scene *scene,
                                                Object *ob_source,
                                                Object *ob_target)
{
  ShrinkwrapModifierData ssmd = {{nullptr}};
  ModifierEvalContext ctx = {depsgraph, ob_source, ModifierApplyFlag(0)};

  ssmd.target = ob_target;
  ssmd.shrinkType = MOD_SHRINKWRAP_NEAREST_SURFACE;
  ssmd.shrinkMode = MOD_SHRINKWRAP_ON_SURFACE;
  ssmd.keepDist = 0.0f;

  Mesh *src_me = static_cast<Mesh *>(ob_source->data);

  shrinkwrapModifier_deform(
      &ssmd,
      &ctx,
      scene,
      ob_source,
      src_me,
      nullptr,
      -1,
      reinterpret_cast<float (*)[3]>(src_me->vert_positions_for_write().data()),
      src_me->verts_num);
  src_me->tag_positions_changed();
}

void BKE_shrinkwrap_remesh_target_project(Mesh *src_me, Mesh *target_me, Object *ob_target)
{
  ShrinkwrapModifierData ssmd = {{nullptr}};

  ssmd.target = ob_target;
  ssmd.shrinkType = MOD_SHRINKWRAP_PROJECT;
  ssmd.shrinkMode = MOD_SHRINKWRAP_ON_SURFACE;
  ssmd.shrinkOpts = MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR | MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR;
  ssmd.keepDist = 0.0f;

  /* Tolerance value to prevent artifacts on sharp edges of a mesh.
   * This constant and based on experimenting with different values. */
  const float projLimitTolerance = 5.0f;
  ssmd.projLimit = target_me->remesh_voxel_size * projLimitTolerance;

  ShrinkwrapCalcData calc = NULL_ShrinkwrapCalcData;

  calc.smd = &ssmd;
  calc.numVerts = src_me->verts_num;
  calc.vertexCos = reinterpret_cast<float (*)[3]>(src_me->vert_positions_for_write().data());
  calc.vert_normals = src_me->vert_normals();
  calc.vgroup = -1;
  calc.target = target_me;
  calc.keepDist = ssmd.keepDist;
  calc.vert_positions = reinterpret_cast<float (*)[3]>(src_me->vert_positions_for_write().data());
  BLI_SPACE_TRANSFORM_SETUP(&calc.local2target, ob_target, ob_target);

  ShrinkwrapTreeData tree;
  if (BKE_shrinkwrap_init_tree(&tree, calc.target, ssmd.shrinkType, ssmd.shrinkMode, false)) {
    calc.tree = &tree;
    TIMEIT_BENCH(shrinkwrap_calc_normal_projection(&calc), deform_project);
    BKE_shrinkwrap_free_tree(&tree);
  }

  src_me->tag_positions_changed();
}
