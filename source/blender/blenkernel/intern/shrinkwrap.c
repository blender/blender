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
 *
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <string.h>
#include <float.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_task.h"
#include "BLI_math_solvers.h"

#include "BKE_shrinkwrap.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_modifier.h"

#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.h" /* for OMP limits. */
#include "BKE_subsurf.h"
#include "BKE_mesh_runtime.h"

#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "BLI_strict_flags.h"

/* for timing... */
#if 0
#  include "PIL_time_utildefines.h"
#else
#  define TIMEIT_BENCH(expr, id) (expr)
#endif

/* Util macros */
#define OUT_OF_MEMORY() ((void)printf("Shrinkwrap: Out of memory\n"))

typedef struct ShrinkwrapCalcData {
  ShrinkwrapModifierData *smd;  // shrinkwrap modifier data

  struct Object *ob;  // object we are applying shrinkwrap to

  struct MVert *vert;     // Array of verts being projected (to fetch normals or other data)
  float (*vertexCos)[3];  // vertexs being shrinkwraped
  int numVerts;

  struct MDeformVert *dvert;  // Pointer to mdeform array
  int vgroup;                 // Vertex group num
  bool invert_vgroup;         /* invert vertex group influence */

  struct Mesh *target;                 // mesh we are shrinking to
  struct SpaceTransform local2target;  // transform to move between local and target space
  struct ShrinkwrapTreeData *tree;     // mesh BVH tree data

  struct Object *aux_target;

  float keepDist;  // Distance to keep above target surface (units are in local space)
} ShrinkwrapCalcData;

typedef struct ShrinkwrapCalcCBData {
  ShrinkwrapCalcData *calc;

  ShrinkwrapTreeData *tree;
  ShrinkwrapTreeData *aux_tree;

  float *proj_axis;
  SpaceTransform *local2aux;
} ShrinkwrapCalcCBData;

/* Checks if the modifier needs target normals with these settings. */
bool BKE_shrinkwrap_needs_normals(int shrinkType, int shrinkMode)
{
  return (shrinkType == MOD_SHRINKWRAP_TARGET_PROJECT) ||
         (shrinkType != MOD_SHRINKWRAP_NEAREST_VERTEX &&
          shrinkMode == MOD_SHRINKWRAP_ABOVE_SURFACE);
}

/* Initializes the mesh data structure from the given mesh and settings. */
bool BKE_shrinkwrap_init_tree(
    ShrinkwrapTreeData *data, Mesh *mesh, int shrinkType, int shrinkMode, bool force_normals)
{
  memset(data, 0, sizeof(*data));

  if (!mesh || mesh->totvert <= 0) {
    return false;
  }

  data->mesh = mesh;

  if (shrinkType == MOD_SHRINKWRAP_NEAREST_VERTEX) {
    data->bvh = BKE_bvhtree_from_mesh_get(&data->treeData, mesh, BVHTREE_FROM_VERTS, 2);

    return data->bvh != NULL;
  }
  else {
    if (mesh->totpoly <= 0) {
      return false;
    }

    data->bvh = BKE_bvhtree_from_mesh_get(&data->treeData, mesh, BVHTREE_FROM_LOOPTRI, 4);

    if (data->bvh == NULL) {
      return false;
    }

    if (force_normals || BKE_shrinkwrap_needs_normals(shrinkType, shrinkMode)) {
      data->pnors = CustomData_get_layer(&mesh->pdata, CD_NORMAL);
      if ((mesh->flag & ME_AUTOSMOOTH) != 0) {
        data->clnors = CustomData_get_layer(&mesh->ldata, CD_NORMAL);
      }
    }

    if (shrinkType == MOD_SHRINKWRAP_TARGET_PROJECT) {
      data->boundary = mesh->runtime.shrinkwrap_data;
    }

    return true;
  }
}

/* Frees the tree data if necessary. */
void BKE_shrinkwrap_free_tree(ShrinkwrapTreeData *data)
{
  free_bvhtree_from_mesh(&data->treeData);
}

/* Free boundary data for target project */
void BKE_shrinkwrap_discard_boundary_data(struct Mesh *mesh)
{
  struct ShrinkwrapBoundaryData *data = mesh->runtime.shrinkwrap_data;

  if (data != NULL) {
    MEM_freeN((void *)data->edge_is_boundary);
    MEM_freeN((void *)data->looptri_has_boundary);
    MEM_freeN((void *)data->vert_boundary_id);
    MEM_freeN((void *)data->boundary_verts);

    MEM_freeN(data);
  }

  mesh->runtime.shrinkwrap_data = NULL;
}

/* Accumulate edge for average boundary edge direction. */
static void merge_vert_dir(ShrinkwrapBoundaryVertData *vdata,
                           signed char *status,
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

static ShrinkwrapBoundaryData *shrinkwrap_build_boundary_data(struct Mesh *mesh)
{
  const MLoop *mloop = mesh->mloop;
  const MEdge *medge = mesh->medge;
  const MVert *mvert = mesh->mvert;

  /* Count faces per edge (up to 2). */
  char *edge_mode = MEM_calloc_arrayN((size_t)mesh->totedge, sizeof(char), __func__);

  for (int i = 0; i < mesh->totloop; i++) {
    unsigned int eidx = mloop[i].e;

    if (edge_mode[eidx] < 2) {
      edge_mode[eidx]++;
    }
  }

  /* Build the boundary edge bitmask. */
  BLI_bitmap *edge_is_boundary = BLI_BITMAP_NEW(mesh->totedge,
                                                "ShrinkwrapBoundaryData::edge_is_boundary");
  unsigned int num_boundary_edges = 0;

  for (int i = 0; i < mesh->totedge; i++) {
    edge_mode[i] = (edge_mode[i] == 1);

    if (edge_mode[i]) {
      BLI_BITMAP_ENABLE(edge_is_boundary, i);
      num_boundary_edges++;
    }
  }

  /* If no boundary, return NULL. */
  if (num_boundary_edges == 0) {
    MEM_freeN(edge_is_boundary);
    MEM_freeN(edge_mode);
    return NULL;
  }

  /* Allocate the data object. */
  ShrinkwrapBoundaryData *data = MEM_callocN(sizeof(ShrinkwrapBoundaryData),
                                             "ShrinkwrapBoundaryData");

  data->edge_is_boundary = edge_is_boundary;

  /* Build the boundary looptri bitmask. */
  const MLoopTri *mlooptri = BKE_mesh_runtime_looptri_ensure(mesh);
  int totlooptri = BKE_mesh_runtime_looptri_len(mesh);

  BLI_bitmap *looptri_has_boundary = BLI_BITMAP_NEW(totlooptri,
                                                    "ShrinkwrapBoundaryData::looptri_is_boundary");

  for (int i = 0; i < totlooptri; i++) {
    int edges[3];
    BKE_mesh_looptri_get_real_edges(mesh, &mlooptri[i], edges);

    for (int j = 0; j < 3; j++) {
      if (edges[j] >= 0 && edge_mode[edges[j]]) {
        BLI_BITMAP_ENABLE(looptri_has_boundary, i);
        break;
      }
    }
  }

  data->looptri_has_boundary = looptri_has_boundary;

  /* Find boundary vertices and build a mapping table for compact storage of data. */
  int *vert_boundary_id = MEM_calloc_arrayN(
      (size_t)mesh->totvert, sizeof(int), "ShrinkwrapBoundaryData::vert_boundary_id");

  for (int i = 0; i < mesh->totedge; i++) {
    if (edge_mode[i]) {
      const MEdge *edge = &medge[i];

      vert_boundary_id[edge->v1] = 1;
      vert_boundary_id[edge->v2] = 1;
    }
  }

  unsigned int num_boundary_verts = 0;

  for (int i = 0; i < mesh->totvert; i++) {
    vert_boundary_id[i] = (vert_boundary_id[i] != 0) ? (int)num_boundary_verts++ : -1;
  }

  data->vert_boundary_id = vert_boundary_id;
  data->num_boundary_verts = num_boundary_verts;

  /* Compute average directions. */
  ShrinkwrapBoundaryVertData *boundary_verts = MEM_calloc_arrayN(
      num_boundary_verts, sizeof(*boundary_verts), "ShrinkwrapBoundaryData::boundary_verts");

  signed char *vert_status = MEM_calloc_arrayN(num_boundary_verts, sizeof(char), __func__);

  for (int i = 0; i < mesh->totedge; i++) {
    if (edge_mode[i]) {
      const MEdge *edge = &medge[i];

      float dir[3];
      sub_v3_v3v3(dir, mvert[edge->v2].co, mvert[edge->v1].co);
      normalize_v3(dir);

      merge_vert_dir(boundary_verts, vert_status, vert_boundary_id[edge->v1], dir, 1);
      merge_vert_dir(boundary_verts, vert_status, vert_boundary_id[edge->v2], dir, 2);
    }
  }

  MEM_freeN(vert_status);

  /* Finalize average direction and compute normal. */
  for (int i = 0; i < mesh->totvert; i++) {
    int bidx = vert_boundary_id[i];

    if (bidx >= 0) {
      ShrinkwrapBoundaryVertData *vdata = &boundary_verts[bidx];
      float no[3], tmp[3];

      normalize_v3(vdata->direction);

      normal_short_to_float_v3(no, mesh->mvert[i].no);
      cross_v3_v3v3(tmp, no, vdata->direction);
      cross_v3_v3v3(vdata->normal_plane, tmp, no);
      normalize_v3(vdata->normal_plane);
    }
  }

  data->boundary_verts = boundary_verts;

  MEM_freeN(edge_mode);
  return data;
}

void BKE_shrinkwrap_compute_boundary_data(struct Mesh *mesh)
{
  BKE_shrinkwrap_discard_boundary_data(mesh);

  mesh->runtime.shrinkwrap_data = shrinkwrap_build_boundary_data(mesh);
}

/*
 * Shrinkwrap to the nearest vertex
 *
 * it builds a kdtree of vertexs we can attach to and then
 * for each vertex performs a nearest vertex search on the tree
 */
static void shrinkwrap_calc_nearest_vertex_cb_ex(void *__restrict userdata,
                                                 const int i,
                                                 const TaskParallelTLS *__restrict tls)
{
  ShrinkwrapCalcCBData *data = userdata;

  ShrinkwrapCalcData *calc = data->calc;
  BVHTreeFromMesh *treeData = &data->tree->treeData;
  BVHTreeNearest *nearest = tls->userdata_chunk;

  float *co = calc->vertexCos[i];
  float tmp_co[3];
  float weight = defvert_array_find_weight_safe(calc->dvert, i, calc->vgroup);

  if (calc->invert_vgroup) {
    weight = 1.0f - weight;
  }

  if (weight == 0.0f) {
    return;
  }

  /* Convert the vertex to tree coordinates */
  if (calc->vert) {
    copy_v3_v3(tmp_co, calc->vert[i].co);
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

  ShrinkwrapCalcCBData data = {
      .calc = calc,
      .tree = calc->tree,
  };
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (calc->numVerts > BKE_MESH_OMP_LIMIT);
  settings.userdata_chunk = &nearest;
  settings.userdata_chunk_size = sizeof(nearest);
  BLI_task_parallel_range(
      0, calc->numVerts, &data, shrinkwrap_calc_nearest_vertex_cb_ex, &settings);
}

/*
 * This function raycast a single vertex and updates the hit if the "hit" is considered valid.
 * Returns true if "hit" was updated.
 * Opts control whether an hit is valid or not
 * Supported options are:
 * - MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE (front faces hits are ignored)
 * - MOD_SHRINKWRAP_CULL_TARGET_BACKFACE (back faces hits are ignored)
 */
bool BKE_shrinkwrap_project_normal(char options,
                                   const float vert[3],
                                   const float dir[3],
                                   const float ray_radius,
                                   const SpaceTransform *transf,
                                   ShrinkwrapTreeData *tree,
                                   BVHTreeRayHit *hit)
{
  /* don't use this because this dist value could be incompatible
   * this value used by the callback for comparing prev/new dist values.
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
      /* apply backface */
      const float dot = dot_v3v3(dir, hit_tmp.no);
      if (((options & MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE) && dot <= 0.0f) ||
          ((options & MOD_SHRINKWRAP_CULL_TARGET_BACKFACE) && dot >= 0.0f)) {
        return false; /* Ignore hit */
      }
    }

    if (transf) {
      /* Inverting space transform (TODO make coeherent with the initial dist readjust) */
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
  ShrinkwrapCalcCBData *data = userdata;

  ShrinkwrapCalcData *calc = data->calc;
  ShrinkwrapTreeData *tree = data->tree;
  ShrinkwrapTreeData *aux_tree = data->aux_tree;

  float *proj_axis = data->proj_axis;
  SpaceTransform *local2aux = data->local2aux;

  BVHTreeRayHit *hit = tls->userdata_chunk;

  const float proj_limit_squared = calc->smd->projLimit * calc->smd->projLimit;
  float *co = calc->vertexCos[i];
  float tmp_co[3], tmp_no[3];
  float weight = defvert_array_find_weight_safe(calc->dvert, i, calc->vgroup);

  if (calc->invert_vgroup) {
    weight = 1.0f - weight;
  }

  if (weight == 0.0f) {
    return;
  }

  if (calc->vert != NULL && calc->smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL) {
    /* calc->vert contains verts from evaluated mesh.  */
    /* These coordinates are deformed by vertexCos only for normal projection
     * (to get correct normals) for other cases calc->verts contains undeformed coordinates and
     * vertexCos should be used */
    copy_v3_v3(tmp_co, calc->vert[i].co);
    normal_short_to_float_v3(tmp_no, calc->vert[i].no);
  }
  else {
    copy_v3_v3(tmp_co, co);
    copy_v3_v3(tmp_no, proj_axis);
  }

  hit->index = -1;

  /* TODO: we should use FLT_MAX here, but sweepsphere code isn't prepared for that */
  hit->dist = BVH_RAYCAST_DIST_MAX;

  bool is_aux = false;

  /* Project over positive direction of axis */
  if (calc->smd->shrinkOpts & MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR) {
    if (aux_tree) {
      if (BKE_shrinkwrap_project_normal(0, tmp_co, tmp_no, 0.0, local2aux, aux_tree, hit)) {
        is_aux = true;
      }
    }

    if (BKE_shrinkwrap_project_normal(
            calc->smd->shrinkOpts, tmp_co, tmp_no, 0.0, &calc->local2target, tree, hit)) {
      is_aux = false;
    }
  }

  /* Project over negative direction of axis */
  if (calc->smd->shrinkOpts & MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR) {
    float inv_no[3];
    negate_v3_v3(inv_no, tmp_no);

    char options = calc->smd->shrinkOpts;

    if ((options & MOD_SHRINKWRAP_INVERT_CULL_TARGET) &&
        (options & MOD_SHRINKWRAP_CULL_TARGET_MASK)) {
      options ^= MOD_SHRINKWRAP_CULL_TARGET_MASK;
    }

    if (aux_tree) {
      if (BKE_shrinkwrap_project_normal(0, tmp_co, inv_no, 0.0, local2aux, aux_tree, hit)) {
        is_aux = true;
      }
    }

    if (BKE_shrinkwrap_project_normal(
            options, tmp_co, inv_no, 0.0, &calc->local2target, tree, hit)) {
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

  /* Raycast and tree stuff */

  /** \note 'hit.dist' is kept in the targets space, this is only used
   * for finding the best hit, to get the real dist,
   * measure the len_v3v3() from the input coord to hit.co */
  BVHTreeRayHit hit;

  /* auxiliary target */
  Mesh *auxMesh = NULL;
  ShrinkwrapTreeData *aux_tree = NULL;
  ShrinkwrapTreeData aux_tree_stack;
  SpaceTransform local2aux;

  /* If the user doesn't allows to project in any direction of projection axis
   * then there's nothing todo. */
  if ((calc->smd->shrinkOpts &
       (MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR | MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR)) == 0) {
    return;
  }

  /* Prepare data to retrieve the direction in which we should project each vertex */
  if (calc->smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL) {
    if (calc->vert == NULL) {
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
    auxMesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(calc->aux_target, false);
    if (!auxMesh) {
      return;
    }
    BLI_SPACE_TRANSFORM_SETUP(&local2aux, calc->ob, calc->aux_target);
  }

  if (BKE_shrinkwrap_init_tree(
          &aux_tree_stack, auxMesh, calc->smd->shrinkType, calc->smd->shrinkMode, false)) {
    aux_tree = &aux_tree_stack;
  }

  /* After successfully build the trees, start projection vertices. */
  ShrinkwrapCalcCBData data = {
      .calc = calc,
      .tree = calc->tree,
      .aux_tree = aux_tree,
      .proj_axis = proj_axis,
      .local2aux = &local2aux,
  };
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (calc->numVerts > BKE_MESH_OMP_LIMIT);
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

//#define TRACE_TARGET_PROJECT

typedef struct TargetProjectTriData {
  const float **vtri_co;
  const float (*vtri_no)[3];
  const float *point_co;

  float n0_minus_n2[3], n1_minus_n2[3];
  float c0_minus_c2[3], c1_minus_c2[3];

  /* Current interpolated position and normal. */
  float co_interp[3], no_interp[3];
} TargetProjectTriData;

/* Computes the deviation of the equation system from goal. */
static void target_project_tri_deviation(void *userdata, const float x[3], float r_delta[3])
{
  TargetProjectTriData *data = userdata;

  float w[3] = {x[0], x[1], 1.0f - x[0] - x[1]};
  interp_v3_v3v3v3(data->co_interp, data->vtri_co[0], data->vtri_co[1], data->vtri_co[2], w);
  interp_v3_v3v3v3(data->no_interp, data->vtri_no[0], data->vtri_no[1], data->vtri_no[2], w);

  madd_v3_v3v3fl(r_delta, data->co_interp, data->no_interp, x[2]);
  sub_v3_v3(r_delta, data->point_co);
}

/* Computes the Jacobian matrix of the equation system. */
static void target_project_tri_jacobian(void *userdata, const float x[3], float r_jacobian[3][3])
{
  TargetProjectTriData *data = userdata;

  madd_v3_v3v3fl(r_jacobian[0], data->c0_minus_c2, data->n0_minus_n2, x[2]);
  madd_v3_v3v3fl(r_jacobian[1], data->c1_minus_c2, data->n1_minus_n2, x[2]);

  copy_v3_v3(r_jacobian[2], data->vtri_no[2]);
  madd_v3_v3fl(r_jacobian[2], data->n0_minus_n2, x[0]);
  madd_v3_v3fl(r_jacobian[2], data->n1_minus_n2, x[1]);
}

/* Clamp barycentric weights to the triangle. */
static void target_project_tri_clamp(float x[3])
{
  if (x[0] < 0.0f) {
    x[0] = 0.0f;
  }
  if (x[1] < 0.0f) {
    x[1] = 0.0f;
  }
  if (x[0] + x[1] > 1.0f) {
    x[0] = x[0] / (x[0] + x[1]);
    x[1] = 1.0f - x[0];
  }
}

/* Correct the Newton's method step to keep the coordinates within the triangle. */
static bool target_project_tri_correct(void *UNUSED(userdata),
                                       const float x[3],
                                       float step[3],
                                       float x_next[3])
{
  /* Insignificant correction threshold */
  const float epsilon = 1e-6f;
  const float dir_epsilon = 0.05f;
  bool fixed = false, locked = false;

  /* Weight 0 and 1 boundary check. */
  for (int i = 0; i < 2; i++) {
    if (step[i] > x[i]) {
      if (step[i] > dir_epsilon * fabsf(step[1 - i])) {
        /* Abort if the solution is clearly outside the domain. */
        if (x[i] < epsilon) {
          return false;
        }

        /* Scale a significant step down to arrive at the boundary. */
        mul_v3_fl(step, x[i] / step[i]);
        fixed = true;
      }
      else {
        /* Reset precision errors to stay at the boundary. */
        step[i] = x[i];
        fixed = locked = true;
      }
    }
  }

  /* Weight 2 boundary check. */
  float sum = x[0] + x[1];
  float sstep = step[0] + step[1];

  if (sum - sstep > 1.0f) {
    if (sstep < -dir_epsilon * (fabsf(step[0]) + fabsf(step[1]))) {
      /* Abort if the solution is clearly outside the domain. */
      if (sum > 1.0f - epsilon) {
        return false;
      }

      /* Scale a significant step down to arrive at the boundary. */
      mul_v3_fl(step, (1.0f - sum) / -sstep);
      fixed = true;
    }
    else {
      /* Reset precision errors to stay at the boundary. */
      if (locked) {
        step[0] = step[1] = 0.0f;
      }
      else {
        step[0] -= 0.5f * sstep;
        step[1] = -step[0];
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
  float epsilon = dist * 1.0e-5f;

  CLAMP_MIN(epsilon, 1.0e-5f);

  /* Initial solution vector: barycentric weights plus distance along normal. */
  interp_weights_tri_v3(x, UNPACK3(vtri_co), hit_co);

  interp_v3_v3v3v3(r_hit_no, UNPACK3(vtri_no), x);
  sub_v3_v3v3(tmp, point_co, hit_co);

  x[2] = (dot_v3v3(tmp, r_hit_no) < 0) ? -dist : dist;

  /* Solve the equations iteratively. */
  TargetProjectTriData tri_data = {
      .vtri_co = vtri_co,
      .vtri_no = vtri_no,
      .point_co = point_co,
  };

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
        "===> %d (%.3f,%.3f,%.3f) %g < %g\n", index, UNPACK3(hit_co), dist_sq, nearest->dist_sq);
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
  const BVHTreeFromMesh *data = &tree->treeData;
  const MEdge *edge = &tree->mesh->medge[eidx];
  const float *vedge_co[2] = {data->vert[edge->v1].co, data->vert[edge->v2].co};

#ifdef TRACE_TARGET_PROJECT
  printf("EDGE %d (%.3f,%.3f,%.3f) (%.3f,%.3f,%.3f)\n",
         eidx,
         UNPACK3(vedge_co[0]),
         UNPACK3(vedge_co[1]));
#endif

  /* Retrieve boundary vertex IDs */
  const int *vert_boundary_id = tree->boundary->vert_boundary_id;
  int bid1 = vert_boundary_id[edge->v1], bid2 = vert_boundary_id[edge->v2];

  if (bid1 < 0 || bid2 < 0) {
    return;
  }

  /* Retrieve boundary vertex normals and align them to direction. */
  const ShrinkwrapBoundaryVertData *boundary_verts = tree->boundary->boundary_verts;
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

  /* Solve a quadratic equation: lerp(d0,d1,x) * (co - lerp(v0,v1,x)) = 0 */
  float d0v0 = dot_v3v3(vedge_dir[0], vedge_co[0]), d0v1 = dot_v3v3(vedge_dir[0], vedge_co[1]);
  float d1v0 = dot_v3v3(vedge_dir[1], vedge_co[0]), d1v1 = dot_v3v3(vedge_dir[1], vedge_co[1]);
  float d0co = dot_v3v3(vedge_dir[0], co);

  float a = d0v1 - d0v0 + d1v0 - d1v1;
  float b = 2 * d0v0 - d0v1 - d0co - d1v0 + dot_v3v3(vedge_dir[1], co);
  float c = d0co - d0v0;
  float det = b * b - 4 * a * c;

  if (det >= 0) {
    const float epsilon = 1e-6f;
    float sdet = sqrtf(det);
    float hit_co[3], hit_no[3];

    for (int i = (det > 0 ? 2 : 0); i >= 0; i -= 2) {
      float x = (-b + ((float)i - 1) * sdet) / (2 * a);

      if (x >= -epsilon && x <= 1.0f + epsilon) {
        CLAMP(x, 0, 1);

        float vedge_no[2][3];
        normal_short_to_float_v3(vedge_no[0], data->vert[edge->v1].no);
        normal_short_to_float_v3(vedge_no[1], data->vert[edge->v2].no);

        interp_v3_v3v3(hit_co, vedge_co[0], vedge_co[1], x);
        interp_v3_v3v3(hit_no, vedge_no[0], vedge_no[1], x);

        update_hit(nearest, index, co, hit_co, hit_no);
      }
    }
  }
}

/* Target normal projection BVH callback - based on mesh_looptri_nearest_point. */
static void mesh_looptri_target_project(void *userdata,
                                        int index,
                                        const float co[3],
                                        BVHTreeNearest *nearest)
{
  const ShrinkwrapTreeData *tree = (ShrinkwrapTreeData *)userdata;
  const BVHTreeFromMesh *data = &tree->treeData;
  const MLoopTri *lt = &data->looptri[index];
  const MLoop *loop[3] = {
      &data->loop[lt->tri[0]], &data->loop[lt->tri[1]], &data->loop[lt->tri[2]]};
  const MVert *vtri[3] = {
      &data->vert[loop[0]->v], &data->vert[loop[1]->v], &data->vert[loop[2]->v]};
  const float *vtri_co[3] = {vtri[0]->co, vtri[1]->co, vtri[2]->co};
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
  normal_short_to_float_v3(vtri_no[0], vtri[0]->no);
  normal_short_to_float_v3(vtri_no[1], vtri[1]->no);
  normal_short_to_float_v3(vtri_no[2], vtri[2]->no);

  /* Solve the equations for the triangle */
  if (target_project_solve_point_tri(vtri_co, vtri_no, co, raw_hit_co, dist_sq, hit_co, hit_no)) {
    update_hit(nearest, index, co, hit_co, hit_no);
  }
  /* Boundary edges */
  else if (tree->boundary && BLI_BITMAP_TEST(tree->boundary->looptri_has_boundary, index)) {
    const BLI_bitmap *is_boundary = tree->boundary->edge_is_boundary;
    int edges[3];

    BKE_mesh_looptri_get_real_edges(tree->mesh, lt, edges);

    for (int i = 0; i < 3; i++) {
      if (edges[i] >= 0 && BLI_BITMAP_TEST(is_boundary, edges[i])) {
        target_project_edge(tree, index, co, nearest, edges[i]);
      }
    }
  }
}

/*
 * Maps the point to the nearest surface, either by simple nearest, or by target normal projection.
 */
void BKE_shrinkwrap_find_nearest_surface(struct ShrinkwrapTreeData *tree,
                                         BVHTreeNearest *nearest,
                                         float co[3],
                                         int type)
{
  BVHTreeFromMesh *treeData = &tree->treeData;

  if (type == MOD_SHRINKWRAP_TARGET_PROJECT) {
#ifdef TRACE_TARGET_PROJECT
    printf("====== TARGET PROJECT START ======\n");
#endif

    BLI_bvhtree_find_nearest_ex(
        tree->bvh, co, nearest, mesh_looptri_target_project, tree, BVH_NEAREST_OPTIMAL_ORDER);

#ifdef TRACE_TARGET_PROJECT
    printf("====== TARGET PROJECT END: %d %g ======\n", nearest->index, nearest->dist_sq);
#endif

    if (nearest->index < 0) {
      /* fallback to simple nearest */
      BLI_bvhtree_find_nearest(tree->bvh, co, nearest, treeData->nearest_callback, treeData);
    }
  }
  else {
    BLI_bvhtree_find_nearest(tree->bvh, co, nearest, treeData->nearest_callback, treeData);
  }
}

/*
 * Shrinkwrap moving vertexs to the nearest surface point on the target
 *
 * it builds a BVHTree from the target mesh and then performs a
 * NN matches for each vertex
 */
static void shrinkwrap_calc_nearest_surface_point_cb_ex(void *__restrict userdata,
                                                        const int i,
                                                        const TaskParallelTLS *__restrict tls)
{
  ShrinkwrapCalcCBData *data = userdata;

  ShrinkwrapCalcData *calc = data->calc;
  BVHTreeNearest *nearest = tls->userdata_chunk;

  float *co = calc->vertexCos[i];
  float tmp_co[3];
  float weight = defvert_array_find_weight_safe(calc->dvert, i, calc->vgroup);

  if (calc->invert_vgroup) {
    weight = 1.0f - weight;
  }

  if (weight == 0.0f) {
    return;
  }

  /* Convert the vertex to tree coordinates */
  if (calc->vert) {
    copy_v3_v3(tmp_co, calc->vert[i].co);
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
                                         NULL,
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

/**
 * Compute a smooth normal of the target (if applicable) at the hit location.
 *
 * \param tree: information about the mesh
 * \param transform: transform from the hit coordinate space to the object space; may be null
 * \param r_no: output in hit coordinate space; may be shared with inputs
 */
void BKE_shrinkwrap_compute_smooth_normal(const struct ShrinkwrapTreeData *tree,
                                          const struct SpaceTransform *transform,
                                          int looptri_idx,
                                          const float hit_co[3],
                                          const float hit_no[3],
                                          float r_no[3])
{
  const BVHTreeFromMesh *treeData = &tree->treeData;
  const MLoopTri *tri = &treeData->looptri[looptri_idx];

  /* Interpolate smooth normals if enabled. */
  if ((tree->mesh->mpoly[tri->poly].flag & ME_SMOOTH) != 0) {
    const MVert *verts[] = {
        &treeData->vert[treeData->loop[tri->tri[0]].v],
        &treeData->vert[treeData->loop[tri->tri[1]].v],
        &treeData->vert[treeData->loop[tri->tri[2]].v],
    };
    float w[3], no[3][3], tmp_co[3];

    /* Custom and auto smooth split normals. */
    if (tree->clnors) {
      copy_v3_v3(no[0], tree->clnors[tri->tri[0]]);
      copy_v3_v3(no[1], tree->clnors[tri->tri[1]]);
      copy_v3_v3(no[2], tree->clnors[tri->tri[2]]);
    }
    /* Ordinary vertex normals. */
    else {
      normal_short_to_float_v3(no[0], verts[0]->no);
      normal_short_to_float_v3(no[1], verts[1]->no);
      normal_short_to_float_v3(no[2], verts[2]->no);
    }

    /* Barycentric weights from hit point. */
    copy_v3_v3(tmp_co, hit_co);

    if (transform) {
      BLI_space_transform_apply(transform, tmp_co);
    }

    interp_weights_tri_v3(w, verts[0]->co, verts[1]->co, verts[2]->co, tmp_co);

    /* Interpolate using weights. */
    interp_v3_v3v3v3(r_no, no[0], no[1], no[2], w);

    if (transform) {
      BLI_space_transform_invert_normal(transform, r_no);
    }
    else {
      normalize_v3(r_no);
    }
  }
  /* Use the polygon normal if flat. */
  else if (tree->pnors != NULL) {
    copy_v3_v3(r_no, tree->pnors[tri->poly]);
  }
  /* Finally fallback to the looptri normal. */
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
  float dist = len_v3v3(point_co, hit_co);

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
    float delta[3];
    sub_v3_v3v3(delta, point_co, hit_co);
    float dsign = signf(dot_v3v3(delta, hit_no) * forcesign);

    /* If on the wrong side or too close, move to correct */
    if (forcesnap || dsign * dist < goal_dist) {
      interp_v3_v3v3(r_point_co, point_co, hit_co, (dist - goal_dist * dsign) / dist);
    }
    else {
      copy_v3_v3(r_point_co, point_co);
    }
  }
}

/**
 * Apply the shrink to surface modes to the given original coordinates and nearest point.
 *
 * \param tree: mesh data for smooth normals
 * \param transform: transform from the hit coordinate space to the object space; may be null
 * \param r_point_co: may be the same memory location as point_co, hit_co, or hit_no.
 */
void BKE_shrinkwrap_snap_point_to_surface(const struct ShrinkwrapTreeData *tree,
                                          const struct SpaceTransform *transform,
                                          int mode,
                                          int hit_idx,
                                          const float hit_co[3],
                                          const float hit_no[3],
                                          float goal_dist,
                                          const float point_co[3],
                                          float r_point_co[3])
{
  float dist, tmp[3];

  switch (mode) {
    /* Offsets along the line between point_co and hit_co. */
    case MOD_SHRINKWRAP_ON_SURFACE:
      if (goal_dist != 0 && (dist = len_v3v3(point_co, hit_co)) > FLT_EPSILON) {
        interp_v3_v3v3(r_point_co, point_co, hit_co, (dist - goal_dist) / dist);
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
  ShrinkwrapCalcCBData data = {
      .calc = calc,
      .tree = calc->tree,
  };
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (calc->numVerts > BKE_MESH_OMP_LIMIT);
  settings.userdata_chunk = &nearest;
  settings.userdata_chunk_size = sizeof(nearest);
  BLI_task_parallel_range(
      0, calc->numVerts, &data, shrinkwrap_calc_nearest_surface_point_cb_ex, &settings);
}

/* Main shrinkwrap function */
void shrinkwrapModifier_deform(ShrinkwrapModifierData *smd,
                               const ModifierEvalContext *ctx,
                               struct Scene *scene,
                               Object *ob,
                               Mesh *mesh,
                               MDeformVert *dvert,
                               const int defgrp_index,
                               float (*vertexCos)[3],
                               int numVerts)
{

  DerivedMesh *ss_mesh = NULL;
  ShrinkwrapCalcData calc = NULL_ShrinkwrapCalcData;

  /* remove loop dependencies on derived meshes (TODO should this be done elsewhere?) */
  if (smd->target == ob) {
    smd->target = NULL;
  }
  if (smd->auxTarget == ob) {
    smd->auxTarget = NULL;
  }

  /* Configure Shrinkwrap calc data */
  calc.smd = smd;
  calc.ob = ob;
  calc.numVerts = numVerts;
  calc.vertexCos = vertexCos;
  calc.dvert = dvert;
  calc.vgroup = defgrp_index;
  calc.invert_vgroup = (smd->shrinkOpts & MOD_SHRINKWRAP_INVERT_VGROUP) != 0;

  if (smd->target != NULL) {
    Object *ob_target = DEG_get_evaluated_object(ctx->depsgraph, smd->target);
    calc.target = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob_target, false);

    /* TODO there might be several "bugs" on non-uniform scales matrixs
     * because it will no longer be nearest surface, not sphere projection
     * because space has been deformed */
    BLI_SPACE_TRANSFORM_SETUP(&calc.local2target, ob, ob_target);

    /* TODO: smd->keepDist is in global units.. must change to local */
    calc.keepDist = smd->keepDist;
  }
  calc.aux_target = DEG_get_evaluated_object(ctx->depsgraph, smd->auxTarget);

  if (mesh != NULL && smd->shrinkType == MOD_SHRINKWRAP_PROJECT) {
    /* Setup arrays to get vertexs positions, normals and deform weights */
    calc.vert = mesh->mvert;

    /* Using vertexs positions/normals as if a subsurface was applied */
    if (smd->subsurfLevels) {
      SubsurfModifierData ssmd = {{NULL}};
      ssmd.subdivType = ME_CC_SUBSURF;  /* catmull clark */
      ssmd.levels = smd->subsurfLevels; /* levels */

      /* TODO to be moved to Mesh once we are done with changes in subsurf code. */
      DerivedMesh *dm = CDDM_from_mesh(mesh);

      ss_mesh = subsurf_make_derived_from_derived(
          dm, &ssmd, scene, NULL, (ob->mode & OB_MODE_EDIT) ? SUBSURF_IN_EDIT_MODE : 0);

      if (ss_mesh) {
        calc.vert = ss_mesh->getVertDataArray(ss_mesh, CD_MVERT);
        if (calc.vert) {
          /* TRICKY: this code assumes subsurface will have the transformed original vertices
           * in their original order at the end of the vert array. */
          calc.vert = calc.vert + ss_mesh->getNumVerts(ss_mesh) - dm->getNumVerts(dm);
        }
      }

      /* Just to make sure we are not leaving any memory behind */
      BLI_assert(ssmd.emCache == NULL);
      BLI_assert(ssmd.mCache == NULL);

      dm->release(dm);
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

  /* free memory */
  if (ss_mesh) {
    ss_mesh->release(ss_mesh);
  }
}
