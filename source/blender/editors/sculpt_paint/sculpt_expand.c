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
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

/* Sculpt Expand. */
/* Operator for creating selections and patterns in Sculpt Mode. Expand can create masks, face sets
 * and fill vertex colors. */
/* The main functionality of the operator
 * - The operator initializes a value per vertex, called "falloff". There are multiple algorithms
 * to generate these falloff values which will create different patterns in the result when using
 * the operator. These falloff values require algorithms that rely on mesh connectivity, so they
 * are only valid on parts of the mesh that are in the same connected component as the given
 * initial vertices. If needed, these falloff values are propagated from vertex or grids into the
 * base mesh faces.
 *
 * - On each modal callback, the operator gets the active vertex and face and gets its falloff
 *   value from its precalculated falloff. This is now the active falloff value.
 * - Using the active falloff value and the settings of the expand operation (which can be modified
 *   during execution using the modal key-map), the operator loops over all elements in the mesh to
 *   check if they are enabled of not.
 * - Based on each element state after evaluating the settings, the desired mesh data (mask, face
 *   sets, colors...) is updated.
 */

/**
 * Used for defining an invalid vertex state (for example, when the cursor is not over the mesh).
 */
#define SCULPT_EXPAND_VERTEX_NONE -1

/** Used for defining an uninitialized active component index for an unused symmetry pass. */
#define EXPAND_ACTIVE_COMPONENT_NONE -1
/**
 * Defines how much each time the texture distortion is increased/decreased
 * when using the modal key-map.
 */
#define SCULPT_EXPAND_TEXTURE_DISTORTION_STEP 0.01f

/**
 * This threshold offsets the required falloff value to start a new loop. This is needed because in
 * some situations, vertices which have the same falloff value as max_falloff will start a new
 * loop, which is undesired.
 */
#define SCULPT_EXPAND_LOOP_THRESHOLD 0.00001f

/**
 * Defines how much changes in curvature in the mesh affect the falloff shape when using normal
 * falloff. This default was found experimentally and it works well in most cases, but can be
 * exposed for tweaking if needed.
 */
#define SCULPT_EXPAND_NORMALS_FALLOFF_EDGE_SENSITIVITY 300

/* Expand Modal Key-map. */
enum {
  SCULPT_EXPAND_MODAL_CONFIRM = 1,
  SCULPT_EXPAND_MODAL_CANCEL,
  SCULPT_EXPAND_MODAL_INVERT,
  SCULPT_EXPAND_MODAL_PRESERVE_TOGGLE,
  SCULPT_EXPAND_MODAL_GRADIENT_TOGGLE,
  SCULPT_EXPAND_MODAL_FALLOFF_CYCLE,
  SCULPT_EXPAND_MODAL_RECURSION_STEP_GEODESIC,
  SCULPT_EXPAND_MODAL_RECURSION_STEP_TOPOLOGY,
  SCULPT_EXPAND_MODAL_MOVE_TOGGLE,
  SCULPT_EXPAND_MODAL_FALLOFF_GEODESIC,
  SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY,
  SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY_DIAGONALS,
  SCULPT_EXPAND_MODAL_FALLOFF_SPHERICAL,
  SCULPT_EXPAND_MODAL_SNAP_TOGGLE,
  SCULPT_EXPAND_MODAL_LOOP_COUNT_INCREASE,
  SCULPT_EXPAND_MODAL_LOOP_COUNT_DECREASE,
  SCULPT_EXPAND_MODAL_BRUSH_GRADIENT_TOGGLE,
  SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_INCREASE,
  SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_DECREASE,
};

/* Functions for getting the state of mesh elements (vertices and base mesh faces). When the main
 * functions for getting the state of an element return true it means that data associated to that
 * element will be modified by expand. */

/**
 * Returns true if the vertex is in a connected component with correctly initialized falloff
 * values.
 */
static bool sculpt_expand_is_vert_in_active_component(SculptSession *ss,
                                                      ExpandCache *expand_cache,
                                                      const int v)
{
  for (int i = 0; i < EXPAND_SYMM_AREAS; i++) {
    if (ss->vertex_info.connected_component[v] == expand_cache->active_connected_components[i]) {
      return true;
    }
  }
  return false;
}

/**
 * Returns true if the face is in a connected component with correctly initialized falloff values.
 */
static bool sculpt_expand_is_face_in_active_component(SculptSession *ss,
                                                      ExpandCache *expand_cache,
                                                      const int f)
{
  const MLoop *loop = &ss->mloop[ss->mpoly[f].loopstart];
  return sculpt_expand_is_vert_in_active_component(ss, expand_cache, loop->v);
}

/**
 * Returns the falloff value of a vertex. This function includes texture distortion, which is not
 * precomputed into the initial falloff values.
 */
static float sculpt_expand_falloff_value_vertex_get(SculptSession *ss,
                                                    ExpandCache *expand_cache,
                                                    const int v)
{
  if (expand_cache->texture_distortion_strength == 0.0f) {
    return expand_cache->vert_falloff[v];
  }

  if (!expand_cache->brush->mtex.tex) {
    return expand_cache->vert_falloff[v];
  }

  float rgba[4];
  const float *vertex_co = SCULPT_vertex_co_get(ss, v);
  const float avg = BKE_brush_sample_tex_3d(
      expand_cache->scene, expand_cache->brush, vertex_co, rgba, 0, ss->tex_pool);

  const float distortion = (avg - 0.5f) * expand_cache->texture_distortion_strength *
                           expand_cache->max_vert_falloff;
  return expand_cache->vert_falloff[v] + distortion;
}

/**
 * Returns the maximum valid falloff value stored in the falloff array, taking the maximum possible
 * texture distortion into account.
 */
static float sculpt_expand_max_vertex_falloff_get(ExpandCache *expand_cache)
{
  if (expand_cache->texture_distortion_strength == 0.0f) {
    return expand_cache->max_vert_falloff;
  }

  if (!expand_cache->brush->mtex.tex) {
    return expand_cache->max_vert_falloff;
  }

  return expand_cache->max_vert_falloff +
         (0.5f * expand_cache->texture_distortion_strength * expand_cache->max_vert_falloff);
}

/**
 * Main function to get the state of a vertex for the current state and settings of a #ExpandCache.
 * Returns true when the target data should be modified by expand.
 */
static bool sculpt_expand_state_get(SculptSession *ss, ExpandCache *expand_cache, const int v)
{
  if (!SCULPT_vertex_visible_get(ss, v)) {
    return false;
  }

  if (!sculpt_expand_is_vert_in_active_component(ss, expand_cache, v)) {
    return false;
  }

  if (expand_cache->all_enabled) {
    return true;
  }

  bool enabled = false;

  if (expand_cache->snap) {
    /* Face Sets are not being modified when using this function, so it is ok to get this directly
     * from the Sculpt API instead of implementing a custom function to get them from
     * expand_cache->original_face_sets. */
    const int face_set = SCULPT_vertex_face_set_get(ss, v);
    enabled = BLI_gset_haskey(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(face_set));
  }
  else {
    const float max_falloff_factor = sculpt_expand_max_vertex_falloff_get(expand_cache);
    const float loop_len = (max_falloff_factor / expand_cache->loop_count) +
                           SCULPT_EXPAND_LOOP_THRESHOLD;

    const float vertex_falloff_factor = sculpt_expand_falloff_value_vertex_get(
        ss, expand_cache, v);
    const float active_factor = fmod(expand_cache->active_falloff, loop_len);
    const float falloff_factor = fmod(vertex_falloff_factor, loop_len);

    enabled = falloff_factor < active_factor;
  }

  if (expand_cache->invert) {
    enabled = !enabled;
  }
  return enabled;
}

/**
 * Main function to get the state of a face for the current state and settings of a #ExpandCache.
 * Returns true when the target data should be modified by expand.
 */
static bool sculpt_expand_face_state_get(SculptSession *ss, ExpandCache *expand_cache, const int f)
{
  if (ss->face_sets[f] <= 0) {
    return false;
  }

  if (!sculpt_expand_is_face_in_active_component(ss, expand_cache, f)) {
    return false;
  }

  if (expand_cache->all_enabled) {
    return true;
  }

  bool enabled = false;

  if (expand_cache->snap_enabled_face_sets) {
    const int face_set = expand_cache->original_face_sets[f];
    enabled = BLI_gset_haskey(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(face_set));
  }
  else {
    const float loop_len = (expand_cache->max_face_falloff / expand_cache->loop_count) +
                           SCULPT_EXPAND_LOOP_THRESHOLD;

    const float active_factor = fmod(expand_cache->active_falloff, loop_len);
    const float falloff_factor = fmod(expand_cache->face_falloff[f], loop_len);
    enabled = falloff_factor < active_factor;
  }

  if (expand_cache->falloff_type == SCULPT_EXPAND_FALLOFF_ACTIVE_FACE_SET) {
    if (ss->face_sets[f] == expand_cache->initial_active_face_set) {
      enabled = false;
    }
  }

  if (expand_cache->invert) {
    enabled = !enabled;
  }

  return enabled;
}

/**
 * For target modes that support gradients (such as sculpt masks or colors), this function returns
 * the corresponding gradient value for an enabled vertex.
 */
static float sculpt_expand_gradient_value_get(SculptSession *ss,
                                              ExpandCache *expand_cache,
                                              const int v)
{
  if (!expand_cache->falloff_gradient) {
    return 1.0f;
  }

  const float max_falloff_factor = sculpt_expand_max_vertex_falloff_get(expand_cache);
  const float loop_len = (max_falloff_factor / expand_cache->loop_count) +
                         SCULPT_EXPAND_LOOP_THRESHOLD;

  const float vertex_falloff_factor = sculpt_expand_falloff_value_vertex_get(ss, expand_cache, v);
  const float active_factor = fmod(expand_cache->active_falloff, loop_len);
  const float falloff_factor = fmod(vertex_falloff_factor, loop_len);

  float linear_falloff;

  if (expand_cache->invert) {
    /* Active factor is the result of a modulus operation using loop_len, so they will never be
     * equal and loop_len - active_factor should never be 0. */
    BLI_assert((loop_len - active_factor) != 0.0f);
    linear_falloff = (falloff_factor - active_factor) / (loop_len - active_factor);
  }
  else {
    linear_falloff = 1.0f - (falloff_factor / active_factor);
  }

  if (!expand_cache->brush_gradient) {
    return linear_falloff;
  }

  return BKE_brush_curve_strength(expand_cache->brush, linear_falloff, 1.0f);
}

/* Utility functions for getting all vertices state during expand. */

/**
 * Returns a bitmap indexed by vertex index which contains if the vertex was enabled or not for a
 * give expand_cache state.
 */
static BLI_bitmap *sculpt_expand_bitmap_from_enabled(SculptSession *ss, ExpandCache *expand_cache)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  BLI_bitmap *enabled_vertices = BLI_BITMAP_NEW(totvert, "enabled vertices");
  for (int i = 0; i < totvert; i++) {
    const bool enabled = sculpt_expand_state_get(ss, expand_cache, i);
    BLI_BITMAP_SET(enabled_vertices, i, enabled);
  }
  return enabled_vertices;
}

/**
 * Returns a bitmap indexed by vertex index which contains if the vertex is in the boundary of the
 * enabled vertices. This is defined as vertices that are enabled and at least have one connected
 * vertex that is not enabled.
 */
static BLI_bitmap *sculpt_expand_boundary_from_enabled(SculptSession *ss,
                                                       BLI_bitmap *enabled_vertices,
                                                       const bool use_mesh_boundary)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  BLI_bitmap *boundary_vertices = BLI_BITMAP_NEW(totvert, "boundary vertices");
  for (int i = 0; i < totvert; i++) {
    if (!BLI_BITMAP_TEST(enabled_vertices, i)) {
      continue;
    }

    bool is_expand_boundary = false;
    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, i, ni) {
      if (!BLI_BITMAP_TEST(enabled_vertices, ni.index)) {
        is_expand_boundary = true;
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    if (use_mesh_boundary && SCULPT_vertex_is_boundary(ss, i)) {
      is_expand_boundary = true;
    }

    BLI_BITMAP_SET(boundary_vertices, i, is_expand_boundary);
  }

  return boundary_vertices;
}

/* Functions implementing different algorithms for initializing falloff values. */

/**
 * Utility function to get the closet vertex after flipping an original vertex position based on
 * an symmetry pass iteration index.
 */
static int sculpt_expand_get_vertex_index_for_symmetry_pass(Object *ob,
                                                            const char symm_it,
                                                            const int original_vertex)
{
  SculptSession *ss = ob->sculpt;
  int symm_vertex = SCULPT_EXPAND_VERTEX_NONE;
  if (symm_it == 0) {
    symm_vertex = original_vertex;
  }
  else {
    float location[3];
    flip_v3_v3(location, SCULPT_vertex_co_get(ss, original_vertex), symm_it);
    symm_vertex = SCULPT_nearest_vertex_get(NULL, ob, location, FLT_MAX, false);
  }
  return symm_vertex;
}

/**
 * Geodesic: Initializes the falloff with geodesic distances from the given active vertex, taking
 * symmetry into account.
 */
static float *sculpt_expand_geodesic_falloff_create(Sculpt *sd, Object *ob, const int v)
{
  return SCULPT_geodesic_from_vertex_and_symm(sd, ob, v, FLT_MAX);
}

/**
 * Topology: Initializes the falloff using a flood-fill operation,
 * increasing the falloff value by 1 when visiting a new vertex.
 */
typedef struct ExpandFloodFillData {
  float original_normal[3];
  float edge_sensitivity;
  float *dists;
  float *edge_factor;
} ExpandFloodFillData;

static bool expand_topology_floodfill_cb(
    SculptSession *UNUSED(ss), int from_v, int to_v, bool is_duplicate, void *userdata)
{
  ExpandFloodFillData *data = userdata;
  if (!is_duplicate) {
    const float to_it = data->dists[from_v] + 1.0f;
    data->dists[to_v] = to_it;
  }
  else {
    data->dists[to_v] = data->dists[from_v];
  }
  return true;
}

static float *sculpt_expand_topology_falloff_create(Sculpt *sd, Object *ob, const int v)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);
  float *dists = MEM_calloc_arrayN(sizeof(float), totvert, "topology dist");

  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  SCULPT_floodfill_add_initial_with_symmetry(sd, ob, ss, &flood, v, FLT_MAX);

  ExpandFloodFillData fdata;
  fdata.dists = dists;

  SCULPT_floodfill_execute(ss, &flood, expand_topology_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);

  return dists;
}

/**
 * Normals: Flood-fills the mesh and reduces the falloff depending on the normal difference between
 * each vertex and the previous one.
 * This creates falloff patterns that follow and snap to the hard edges of the object.
 */
static bool mask_expand_normal_floodfill_cb(
    SculptSession *ss, int from_v, int to_v, bool is_duplicate, void *userdata)
{
  ExpandFloodFillData *data = userdata;
  if (!is_duplicate) {
    float current_normal[3], prev_normal[3];
    SCULPT_vertex_normal_get(ss, to_v, current_normal);
    SCULPT_vertex_normal_get(ss, from_v, prev_normal);
    const float from_edge_factor = data->edge_factor[from_v];
    data->edge_factor[to_v] = dot_v3v3(current_normal, prev_normal) * from_edge_factor;
    data->dists[to_v] = dot_v3v3(data->original_normal, current_normal) *
                        powf(from_edge_factor, data->edge_sensitivity);
    CLAMP(data->dists[to_v], 0.0f, 1.0f);
  }
  else {
    /* PBVH_GRIDS duplicate handling. */
    data->edge_factor[to_v] = data->edge_factor[from_v];
    data->dists[to_v] = data->dists[from_v];
  }

  return true;
}

static float *sculpt_expand_normal_falloff_create(Sculpt *sd,
                                                  Object *ob,
                                                  const int v,
                                                  const float edge_sensitivity)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);
  float *dists = MEM_malloc_arrayN(sizeof(float), totvert, "normal dist");
  float *edge_factor = MEM_callocN(sizeof(float) * totvert, "mask edge factor");
  for (int i = 0; i < totvert; i++) {
    edge_factor[i] = 1.0f;
  }

  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  SCULPT_floodfill_add_initial_with_symmetry(sd, ob, ss, &flood, v, FLT_MAX);

  ExpandFloodFillData fdata;
  fdata.dists = dists;
  fdata.edge_factor = edge_factor;
  fdata.edge_sensitivity = edge_sensitivity;
  SCULPT_vertex_normal_get(ss, v, fdata.original_normal);

  SCULPT_floodfill_execute(ss, &flood, mask_expand_normal_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);

  for (int repeat = 0; repeat < 2; repeat++) {
    for (int i = 0; i < totvert; i++) {
      float avg = 0.0f;
      SculptVertexNeighborIter ni;
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, i, ni) {
        avg += dists[ni.index];
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
      dists[i] = avg / ni.size;
    }
  }

  MEM_SAFE_FREE(edge_factor);

  return dists;
}

/**
 * Spherical: Initializes the falloff based on the distance from a vertex, taking symmetry into
 * account.
 */
static float *sculpt_expand_spherical_falloff_create(Object *ob, const int v)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  float *dists = MEM_malloc_arrayN(sizeof(float), totvert, "spherical dist");
  for (int i = 0; i < totvert; i++) {
    dists[i] = FLT_MAX;
  }
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);

  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }
    const int symm_vertex = sculpt_expand_get_vertex_index_for_symmetry_pass(ob, symm_it, v);
    if (symm_vertex != -1) {
      const float *co = SCULPT_vertex_co_get(ss, symm_vertex);
      for (int i = 0; i < totvert; i++) {
        dists[i] = min_ff(dists[i], len_v3v3(co, SCULPT_vertex_co_get(ss, i)));
      }
    }
  }

  return dists;
}

/**
 * Boundary: This falloff mode uses the code from sculpt_boundary to initialize the closest mesh
 * boundary to a falloff value of 0. Then, it propagates that falloff to the rest of the mesh so it
 * stays parallel to the boundary, increasing the falloff value by 1 on each step.
 */
static float *sculpt_expand_boundary_topology_falloff_create(Object *ob, const int v)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);
  float *dists = MEM_calloc_arrayN(sizeof(float), totvert, "spherical dist");
  BLI_bitmap *visited_vertices = BLI_BITMAP_NEW(totvert, "visited vertices");
  GSQueue *queue = BLI_gsqueue_new(sizeof(int));

  /* Search and initialize a boundary per symmetry pass, then mark those vertices as visited. */
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }

    const int symm_vertex = sculpt_expand_get_vertex_index_for_symmetry_pass(ob, symm_it, v);

    SculptBoundary *boundary = SCULPT_boundary_data_init(ob, NULL, symm_vertex, FLT_MAX);
    if (!boundary) {
      continue;
    }

    for (int i = 0; i < boundary->num_vertices; i++) {
      BLI_gsqueue_push(queue, &boundary->vertices[i]);
      BLI_BITMAP_ENABLE(visited_vertices, boundary->vertices[i]);
    }
    SCULPT_boundary_data_free(boundary);
  }

  /* If there are no boundaries, return a falloff with all values set to 0. */
  if (BLI_gsqueue_is_empty(queue)) {
    return dists;
  }

  /* Propagate the values from the boundaries to the rest of the mesh. */
  while (!BLI_gsqueue_is_empty(queue)) {
    int v_next;
    BLI_gsqueue_pop(queue, &v_next);

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, v_next, ni) {
      if (BLI_BITMAP_TEST(visited_vertices, ni.index)) {
        continue;
      }
      dists[ni.index] = dists[v_next] + 1.0f;
      BLI_BITMAP_ENABLE(visited_vertices, ni.index);
      BLI_gsqueue_push(queue, &ni.index);
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  }

  BLI_gsqueue_free(queue);
  MEM_freeN(visited_vertices);
  return dists;
}

/**
 * Topology diagonals. This falloff is similar to topology, but it also considers the diagonals of
 * the base mesh faces when checking a vertex neighbor. For this reason, this is not implement
 * using the general flood-fill and sculpt neighbors accessors.
 */
static float *sculpt_expand_diagonals_falloff_create(Object *ob, const int v)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);
  float *dists = MEM_calloc_arrayN(sizeof(float), totvert, "spherical dist");

  /* This algorithm uses mesh data (polys and loops), so this falloff type can't be initialized for
   * Multires. It also does not make sense to implement it for dyntopo as the result will be the
   * same as Topology falloff. */
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    return dists;
  }

  /* Search and mask as visited the initial vertices using the enabled symmetry passes. */
  BLI_bitmap *visited_vertices = BLI_BITMAP_NEW(totvert, "visited vertices");
  GSQueue *queue = BLI_gsqueue_new(sizeof(int));
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }

    const int symm_vertex = sculpt_expand_get_vertex_index_for_symmetry_pass(ob, symm_it, v);

    BLI_gsqueue_push(queue, &symm_vertex);
    BLI_BITMAP_ENABLE(visited_vertices, symm_vertex);
  }

  if (BLI_gsqueue_is_empty(queue)) {
    return dists;
  }

  /* Propagate the falloff increasing the value by 1 each time a new vertex is visited. */
  Mesh *mesh = ob->data;
  while (!BLI_gsqueue_is_empty(queue)) {
    int v_next;
    BLI_gsqueue_pop(queue, &v_next);
    for (int j = 0; j < ss->pmap[v_next].count; j++) {
      MPoly *p = &ss->mpoly[ss->pmap[v_next].indices[j]];
      for (int l = 0; l < p->totloop; l++) {
        const int neighbor_v = mesh->mloop[p->loopstart + l].v;
        if (BLI_BITMAP_TEST(visited_vertices, neighbor_v)) {
          continue;
        }
        dists[neighbor_v] = dists[v_next] + 1.0f;
        BLI_BITMAP_ENABLE(visited_vertices, neighbor_v);
        BLI_gsqueue_push(queue, &neighbor_v);
      }
    }
  }

  BLI_gsqueue_free(queue);
  MEM_freeN(visited_vertices);
  return dists;
}

/* Functions to update the max_falloff value in the #ExpandCache. These functions are called after
 * initializing a new falloff to make sure that this value is always updated. */

/**
 * Updates the max_falloff value for vertices in a #ExpandCache based on the current values of the
 * falloff, skipping any invalid values initialized to FLT_MAX and not initialized components.
 */
static void sculpt_expand_update_max_vert_falloff_value(SculptSession *ss,
                                                        ExpandCache *expand_cache)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  expand_cache->max_vert_falloff = -FLT_MAX;
  for (int i = 0; i < totvert; i++) {
    if (expand_cache->vert_falloff[i] == FLT_MAX) {
      continue;
    }

    if (!sculpt_expand_is_vert_in_active_component(ss, expand_cache, i)) {
      continue;
    }

    expand_cache->max_vert_falloff = max_ff(expand_cache->max_vert_falloff,
                                            expand_cache->vert_falloff[i]);
  }
}

/**
 * Updates the max_falloff value for faces in a ExpandCache based on the current values of the
 * falloff, skipping any invalid values initialized to FLT_MAX and not initialized components.
 */
static void sculpt_expand_update_max_face_falloff_factor(SculptSession *ss,
                                                         ExpandCache *expand_cache)
{
  const int totface = ss->totfaces;
  expand_cache->max_face_falloff = -FLT_MAX;
  for (int i = 0; i < totface; i++) {
    if (expand_cache->face_falloff[i] == FLT_MAX) {
      continue;
    }

    if (!sculpt_expand_is_face_in_active_component(ss, expand_cache, i)) {
      continue;
    }

    expand_cache->max_face_falloff = max_ff(expand_cache->max_face_falloff,
                                            expand_cache->face_falloff[i]);
  }
}

/**
 * Functions to get falloff values for faces from the values from the vertices. This is used for
 * expanding Face Sets. Depending on the data type of the #SculptSession, this needs to get the per
 * face falloff value from the connected vertices of each face or from the grids stored per loops
 * for each face.
 */
static void sculpt_expand_grids_to_faces_falloff(SculptSession *ss,
                                                 Mesh *mesh,
                                                 ExpandCache *expand_cache)
{

  const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);

  for (int p = 0; p < mesh->totpoly; p++) {
    MPoly *poly = &mesh->mpoly[p];
    float accum = 0.0f;
    for (int l = 0; l < poly->totloop; l++) {
      const int grid_loop_index = (poly->loopstart + l) * key->grid_area;
      for (int g = 0; g < key->grid_area; g++) {
        accum += expand_cache->vert_falloff[grid_loop_index + g];
      }
    }
    expand_cache->face_falloff[p] = accum / (poly->totloop * key->grid_area);
  }
}

static void sculpt_expand_vertex_to_faces_falloff(Mesh *mesh, ExpandCache *expand_cache)
{
  for (int p = 0; p < mesh->totpoly; p++) {
    MPoly *poly = &mesh->mpoly[p];
    float accum = 0.0f;
    for (int l = 0; l < poly->totloop; l++) {
      MLoop *loop = &mesh->mloop[l + poly->loopstart];
      accum += expand_cache->vert_falloff[loop->v];
    }
    expand_cache->face_falloff[p] = accum / poly->totloop;
  }
}

/**
 * Main function to update the faces falloff from a already calculated vertex falloff.
 */
static void sculpt_expand_mesh_face_falloff_from_vertex_falloff(SculptSession *ss,
                                                                Mesh *mesh,
                                                                ExpandCache *expand_cache)
{
  BLI_assert(expand_cache->vert_falloff != NULL);

  if (!expand_cache->face_falloff) {
    expand_cache->face_falloff = MEM_malloc_arrayN(
        mesh->totpoly, sizeof(float), "face falloff factors");
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
    sculpt_expand_vertex_to_faces_falloff(mesh, expand_cache);
  }
  else if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
    sculpt_expand_grids_to_faces_falloff(ss, mesh, expand_cache);
  }
  else {
    BLI_assert(false);
  }
}

/* Recursions. These functions will generate new falloff values based on the state of the vertices
 * from the current ExpandCache options and falloff values. */

/**
 * Geodesic recursion: Initializes falloff values using geodesic distances from the boundary of the
 * current vertices state.
 */
static void sculpt_expand_geodesics_from_state_boundary(Object *ob,
                                                        ExpandCache *expand_cache,
                                                        BLI_bitmap *enabled_vertices)
{
  SculptSession *ss = ob->sculpt;
  BLI_assert(BKE_pbvh_type(ss->pbvh) == PBVH_FACES);

  GSet *initial_vertices = BLI_gset_int_new("initial_vertices");
  BLI_bitmap *boundary_vertices = sculpt_expand_boundary_from_enabled(ss, enabled_vertices, false);
  const int totvert = SCULPT_vertex_count_get(ss);
  for (int i = 0; i < totvert; i++) {
    if (!BLI_BITMAP_TEST(boundary_vertices, i)) {
      continue;
    }
    BLI_gset_add(initial_vertices, POINTER_FROM_INT(i));
  }
  MEM_freeN(boundary_vertices);

  MEM_SAFE_FREE(expand_cache->vert_falloff);
  MEM_SAFE_FREE(expand_cache->face_falloff);

  expand_cache->vert_falloff = SCULPT_geodesic_distances_create(ob, initial_vertices, FLT_MAX);
  BLI_gset_free(initial_vertices, NULL);
}

/**
 * Topology recursion: Initializes falloff values using topology steps from the boundary of the
 * current vertices state, increasing the value by 1 each time a new vertex is visited.
 */
static void sculpt_expand_topology_from_state_boundary(Object *ob,
                                                       ExpandCache *expand_cache,
                                                       BLI_bitmap *enabled_vertices)
{
  MEM_SAFE_FREE(expand_cache->vert_falloff);
  MEM_SAFE_FREE(expand_cache->face_falloff);

  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  float *dists = MEM_calloc_arrayN(sizeof(float), totvert, "topology dist");
  BLI_bitmap *boundary_vertices = sculpt_expand_boundary_from_enabled(ss, enabled_vertices, false);

  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  for (int i = 0; i < totvert; i++) {
    if (!BLI_BITMAP_TEST(boundary_vertices, i)) {
      continue;
    }
    SCULPT_floodfill_add_and_skip_initial(&flood, i);
  }
  MEM_freeN(boundary_vertices);

  ExpandFloodFillData fdata;
  fdata.dists = dists;
  SCULPT_floodfill_execute(ss, &flood, expand_topology_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);

  expand_cache->vert_falloff = dists;
}

/**
 * Main function to create a recursion step from the current #ExpandCache state.
 */
static void sculpt_expand_resursion_step_add(Object *ob,
                                             ExpandCache *expand_cache,
                                             const eSculptExpandRecursionType recursion_type)
{
  SculptSession *ss = ob->sculpt;
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    return;
  }

  BLI_bitmap *enabled_vertices = sculpt_expand_bitmap_from_enabled(ss, expand_cache);

  /* Each time a new recursion step is created, reset the distortion strength. This is the expected
   * result from the recursion, as otherwise the new falloff will render with undesired distortion
   * from the beginning. */
  expand_cache->texture_distortion_strength = 0.0f;

  switch (recursion_type) {
    case SCULPT_EXPAND_RECURSION_GEODESICS:
      sculpt_expand_geodesics_from_state_boundary(ob, expand_cache, enabled_vertices);
      break;
    case SCULPT_EXPAND_RECURSION_TOPOLOGY:
      sculpt_expand_topology_from_state_boundary(ob, expand_cache, enabled_vertices);
      break;
  }

  sculpt_expand_update_max_vert_falloff_value(ss, expand_cache);
  if (expand_cache->target == SCULPT_EXPAND_TARGET_FACE_SETS) {
    sculpt_expand_mesh_face_falloff_from_vertex_falloff(ss, ob->data, expand_cache);
    sculpt_expand_update_max_face_falloff_factor(ss, expand_cache);
  }

  MEM_freeN(enabled_vertices);
}

/* Face Set Boundary falloff. */

/**
 * When internal falloff is set to true, the falloff will fill the active Face Set with a gradient,
 * otherwise the active Face Set will be filled with a constant falloff of 0.0f.
 */
static void sculpt_expand_initialize_from_face_set_boundary(Object *ob,
                                                            ExpandCache *expand_cache,
                                                            const int active_face_set,
                                                            const bool internal_falloff)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  BLI_bitmap *enabled_vertices = BLI_BITMAP_NEW(totvert, "enabled vertices");
  for (int i = 0; i < totvert; i++) {
    if (!SCULPT_vertex_has_unique_face_set(ss, i)) {
      continue;
    }
    if (!SCULPT_vertex_has_face_set(ss, i, active_face_set)) {
      continue;
    }
    BLI_BITMAP_ENABLE(enabled_vertices, i);
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
    sculpt_expand_geodesics_from_state_boundary(ob, expand_cache, enabled_vertices);
  }
  else {
    sculpt_expand_topology_from_state_boundary(ob, expand_cache, enabled_vertices);
  }

  MEM_freeN(enabled_vertices);

  if (internal_falloff) {
    for (int i = 0; i < totvert; i++) {
      if (!(SCULPT_vertex_has_face_set(ss, i, active_face_set) &&
            SCULPT_vertex_has_unique_face_set(ss, i))) {
        continue;
      }
      expand_cache->vert_falloff[i] *= -1.0f;
    }

    float min_factor = FLT_MAX;
    for (int i = 0; i < totvert; i++) {
      min_factor = min_ff(expand_cache->vert_falloff[i], min_factor);
    }

    const float additional_falloff = fabsf(min_factor);
    for (int i = 0; i < totvert; i++) {
      expand_cache->vert_falloff[i] += additional_falloff;
    }
  }
  else {
    for (int i = 0; i < totvert; i++) {
      if (!SCULPT_vertex_has_face_set(ss, i, active_face_set)) {
        continue;
      }
      expand_cache->vert_falloff[i] = 0.0f;
    }
  }
}

/**
 * Main function to initialize new falloff values in a #ExpandCache given an initial vertex and a
 * falloff type.
 */
static void sculpt_expand_falloff_factors_from_vertex_and_symm_create(
    ExpandCache *expand_cache,
    Sculpt *sd,
    Object *ob,
    const int v,
    eSculptExpandFalloffType falloff_type)
{
  MEM_SAFE_FREE(expand_cache->vert_falloff);
  expand_cache->falloff_type = falloff_type;

  SculptSession *ss = ob->sculpt;
  const bool has_topology_info = BKE_pbvh_type(ss->pbvh) == PBVH_FACES;

  switch (falloff_type) {
    case SCULPT_EXPAND_FALLOFF_GEODESIC:
      expand_cache->vert_falloff = has_topology_info ?
                                       sculpt_expand_geodesic_falloff_create(sd, ob, v) :
                                       sculpt_expand_spherical_falloff_create(ob, v);
      break;
    case SCULPT_EXPAND_FALLOFF_TOPOLOGY:
      expand_cache->vert_falloff = sculpt_expand_topology_falloff_create(sd, ob, v);
      break;
    case SCULPT_EXPAND_FALLOFF_TOPOLOGY_DIAGONALS:
      expand_cache->vert_falloff = has_topology_info ?
                                       sculpt_expand_diagonals_falloff_create(ob, v) :
                                       sculpt_expand_topology_falloff_create(sd, ob, v);
      break;
    case SCULPT_EXPAND_FALLOFF_NORMALS:
      expand_cache->vert_falloff = sculpt_expand_normal_falloff_create(
          sd, ob, v, SCULPT_EXPAND_NORMALS_FALLOFF_EDGE_SENSITIVITY);
      break;
    case SCULPT_EXPAND_FALLOFF_SPHERICAL:
      expand_cache->vert_falloff = sculpt_expand_spherical_falloff_create(ob, v);
      break;
    case SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY:
      expand_cache->vert_falloff = sculpt_expand_boundary_topology_falloff_create(ob, v);
      break;
    case SCULPT_EXPAND_FALLOFF_BOUNDARY_FACE_SET:
      sculpt_expand_initialize_from_face_set_boundary(
          ob, expand_cache, expand_cache->initial_active_face_set, true);
      break;
    case SCULPT_EXPAND_FALLOFF_ACTIVE_FACE_SET:
      sculpt_expand_initialize_from_face_set_boundary(
          ob, expand_cache, expand_cache->initial_active_face_set, false);
      break;
  }

  /* Update max falloff values and propagate to base mesh faces if needed. */
  sculpt_expand_update_max_vert_falloff_value(ss, expand_cache);
  if (expand_cache->target == SCULPT_EXPAND_TARGET_FACE_SETS) {
    sculpt_expand_mesh_face_falloff_from_vertex_falloff(ss, ob->data, expand_cache);
    sculpt_expand_update_max_face_falloff_factor(ss, expand_cache);
  }
}

/**
 * Adds to the snapping Face Set `gset` all Face Sets which contain all enabled vertices for the
 * current #ExpandCache state. This improves the usability of snapping, as already enabled elements
 * won't switch their state when toggling snapping with the modal key-map.
 */
static void sculpt_expand_snap_initialize_from_enabled(SculptSession *ss,
                                                       ExpandCache *expand_cache)
{
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    return;
  }

  /* Make sure this code runs with snapping and invert disabled. This simplifies the code and
   * prevents using this function with snapping already enabled. */
  const bool prev_snap_state = expand_cache->snap;
  const bool prev_invert_state = expand_cache->invert;
  expand_cache->snap = false;
  expand_cache->invert = false;

  BLI_bitmap *enabled_vertices = sculpt_expand_bitmap_from_enabled(ss, expand_cache);

  const int totface = ss->totfaces;
  for (int i = 0; i < totface; i++) {
    const int face_set = expand_cache->original_face_sets[i];
    BLI_gset_add(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(face_set));
  }

  for (int p = 0; p < totface; p++) {
    MPoly *poly = &ss->mpoly[p];
    bool any_disabled = false;
    for (int l = 0; l < poly->totloop; l++) {
      MLoop *loop = &ss->mloop[l + poly->loopstart];
      if (!BLI_BITMAP_TEST(enabled_vertices, loop->v)) {
        any_disabled = true;
        break;
      }
    }
    if (any_disabled) {
      const int face_set = expand_cache->original_face_sets[p];
      BLI_gset_remove(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(face_set), NULL);
    }
  }

  MEM_freeN(enabled_vertices);
  expand_cache->snap = prev_snap_state;
  expand_cache->invert = prev_invert_state;
}

/**
 * Functions to free a #ExpandCache.
 */
static void sculpt_expand_cache_data_free(ExpandCache *expand_cache)
{
  if (expand_cache->snap_enabled_face_sets) {
    BLI_gset_free(expand_cache->snap_enabled_face_sets, NULL);
  }
  MEM_SAFE_FREE(expand_cache->nodes);
  MEM_SAFE_FREE(expand_cache->vert_falloff);
  MEM_SAFE_FREE(expand_cache->face_falloff);
  MEM_SAFE_FREE(expand_cache->original_mask);
  MEM_SAFE_FREE(expand_cache->original_face_sets);
  MEM_SAFE_FREE(expand_cache->initial_face_sets);
  MEM_SAFE_FREE(expand_cache->original_colors);
  MEM_SAFE_FREE(expand_cache);
}

static void sculpt_expand_cache_free(SculptSession *ss)
{
  sculpt_expand_cache_data_free(ss->expand_cache);
  /* Needs to be set to NULL as the paint cursor relies on checking this pointer detecting if an
   * expand operation is running. */
  ss->expand_cache = NULL;
}

/**
 * Functions to restore the original state from the #ExpandCache when canceling the operator.
 */
static void sculpt_expand_restore_face_set_data(SculptSession *ss, ExpandCache *expand_cache)
{
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);
  for (int n = 0; n < totnode; n++) {
    PBVHNode *node = nodes[n];
    BKE_pbvh_node_mark_redraw(node);
  }
  MEM_freeN(nodes);
  for (int i = 0; i < ss->totfaces; i++) {
    ss->face_sets[i] = expand_cache->original_face_sets[i];
  }
}

static void sculpt_expand_restore_color_data(SculptSession *ss, ExpandCache *expand_cache)
{
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);
  for (int n = 0; n < totnode; n++) {
    PBVHNode *node = nodes[n];
    PBVHVertexIter vd;
    BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
    {
      copy_v4_v4(vd.col, expand_cache->original_colors[vd.index]);
    }
    BKE_pbvh_vertex_iter_end;
    BKE_pbvh_node_mark_redraw(node);
  }
  MEM_freeN(nodes);
}

static void sculpt_expand_restore_mask_data(SculptSession *ss, ExpandCache *expand_cache)
{
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);
  for (int n = 0; n < totnode; n++) {
    PBVHNode *node = nodes[n];
    PBVHVertexIter vd;
    BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
    {
      *vd.mask = expand_cache->original_mask[vd.index];
    }
    BKE_pbvh_vertex_iter_end;
    BKE_pbvh_node_mark_redraw(node);
  }
  MEM_freeN(nodes);
}

/* Main function to restore the original state of the data to how it was before starting the expand
 * operation. */
static void sculpt_expand_restore_original_state(bContext *C,
                                                 Object *ob,
                                                 ExpandCache *expand_cache)
{

  SculptSession *ss = ob->sculpt;
  switch (expand_cache->target) {
    case SCULPT_EXPAND_TARGET_MASK:
      sculpt_expand_restore_mask_data(ss, expand_cache);
      SCULPT_flush_update_step(C, SCULPT_UPDATE_MASK);
      SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
      SCULPT_tag_update_overlays(C);
      break;
    case SCULPT_EXPAND_TARGET_FACE_SETS:
      sculpt_expand_restore_face_set_data(ss, expand_cache);
      SCULPT_flush_update_step(C, SCULPT_UPDATE_MASK);
      SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
      SCULPT_tag_update_overlays(C);
      break;
    case SCULPT_EXPAND_TARGET_COLORS:
      sculpt_expand_restore_color_data(ss, expand_cache);
      SCULPT_flush_update_step(C, SCULPT_UPDATE_COLOR);
      SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COLOR);
      break;
  }
}

/**
 * Cancel operator callback.
 */
static void sculpt_expand_cancel(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  sculpt_expand_restore_original_state(C, ob, ss->expand_cache);

  SCULPT_undo_push_end();
  sculpt_expand_cache_free(ss);
}

/* Functions to update the sculpt mesh data. */

/**
 * Callback to update mask data per PBVH node.
 */
static void sculpt_expand_mask_update_task_cb(void *__restrict userdata,
                                              const int i,
                                              const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  ExpandCache *expand_cache = ss->expand_cache;

  bool any_changed = false;

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_ALL)
  {
    const float initial_mask = *vd.mask;
    const bool enabled = sculpt_expand_state_get(ss, expand_cache, vd.index);

    float new_mask;

    if (enabled) {
      new_mask = sculpt_expand_gradient_value_get(ss, expand_cache, vd.index);
    }
    else {
      new_mask = 0.0f;
    }

    if (expand_cache->preserve) {
      new_mask = max_ff(new_mask, expand_cache->original_mask[vd.index]);
    }

    if (new_mask == initial_mask) {
      continue;
    }

    *vd.mask = clamp_f(new_mask, 0.0f, 1.0f);
    any_changed = true;
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
  if (any_changed) {
    BKE_pbvh_node_mark_update_mask(node);
  }
}

/**
 * Update Face Set data. Not multi-threaded per node as nodes don't contain face arrays.
 */
static void sculpt_expand_face_sets_update(SculptSession *ss, ExpandCache *expand_cache)
{
  const int totface = ss->totfaces;
  for (int f = 0; f < totface; f++) {
    const bool enabled = sculpt_expand_face_state_get(ss, expand_cache, f);
    if (!enabled) {
      continue;
    }
    if (expand_cache->preserve) {
      ss->face_sets[f] += expand_cache->next_face_set;
    }
    else {
      ss->face_sets[f] = expand_cache->next_face_set;
    }
  }

  for (int i = 0; i < expand_cache->totnode; i++) {
    BKE_pbvh_node_mark_redraw(ss->expand_cache->nodes[i]);
  }
}

/**
 * Callback to update vertex colors per PBVH node.
 */
static void sculpt_expand_colors_update_task_cb(void *__restrict userdata,
                                                const int i,
                                                const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  ExpandCache *expand_cache = ss->expand_cache;

  bool any_changed = false;

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_ALL)
  {
    float initial_color[4];
    copy_v4_v4(initial_color, vd.col);

    const bool enabled = sculpt_expand_state_get(ss, expand_cache, vd.index);
    float fade;

    if (enabled) {
      fade = sculpt_expand_gradient_value_get(ss, expand_cache, vd.index);
    }
    else {
      fade = 0.0f;
    }

    fade *= 1.0f - *vd.mask;
    fade = clamp_f(fade, 0.0f, 1.0f);

    float final_color[4];
    float final_fill_color[4];
    mul_v4_v4fl(final_fill_color, expand_cache->fill_color, fade);
    IMB_blend_color_float(final_color,
                          expand_cache->original_colors[vd.index],
                          final_fill_color,
                          expand_cache->blend_mode);

    if (equals_v4v4(initial_color, final_color)) {
      continue;
    }

    copy_v4_v4(vd.col, final_color);
    any_changed = true;
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
  if (any_changed) {
    BKE_pbvh_node_mark_update_color(node);
  }
}

static void sculpt_expand_flush_updates(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  switch (ss->expand_cache->target) {
    case SCULPT_EXPAND_TARGET_MASK:
      SCULPT_flush_update_step(C, SCULPT_UPDATE_MASK);
      break;
    case SCULPT_EXPAND_TARGET_FACE_SETS:
      SCULPT_flush_update_step(C, SCULPT_UPDATE_MASK);
      break;
    case SCULPT_EXPAND_TARGET_COLORS:
      SCULPT_flush_update_step(C, SCULPT_UPDATE_COLOR);
      break;
    default:
      break;
  }
}

/* Store the original mesh data state in the expand cache. */
static void sculpt_expand_original_state_store(Object *ob, ExpandCache *expand_cache)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);
  const int totface = ss->totfaces;

  /* Face Sets are always stored as they are needed for snapping. */
  expand_cache->initial_face_sets = MEM_malloc_arrayN(totface, sizeof(int), "initial face set");
  expand_cache->original_face_sets = MEM_malloc_arrayN(totface, sizeof(int), "original face set");
  for (int i = 0; i < totface; i++) {
    expand_cache->initial_face_sets[i] = ss->face_sets[i];
    expand_cache->original_face_sets[i] = ss->face_sets[i];
  }

  if (expand_cache->target == SCULPT_EXPAND_TARGET_MASK) {
    expand_cache->original_mask = MEM_malloc_arrayN(totvert, sizeof(float), "initial mask");
    for (int i = 0; i < totvert; i++) {
      expand_cache->original_mask[i] = SCULPT_vertex_mask_get(ss, i);
    }
  }

  if (expand_cache->target == SCULPT_EXPAND_TARGET_COLORS) {
    expand_cache->original_colors = MEM_malloc_arrayN(totvert, sizeof(float[4]), "initial colors");
    for (int i = 0; i < totvert; i++) {
      copy_v4_v4(expand_cache->original_colors[i], SCULPT_vertex_color_get(ss, i));
    }
  }
}

/**
 * Restore the state of the Face Sets before a new update.
 */
static void sculpt_expand_face_sets_restore(SculptSession *ss, ExpandCache *expand_cache)
{
  const int totfaces = ss->totfaces;
  for (int i = 0; i < totfaces; i++) {
    ss->face_sets[i] = expand_cache->initial_face_sets[i];
  }
}

static void sculpt_expand_update_for_vertex(bContext *C, Object *ob, const int vertex)
{
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  ExpandCache *expand_cache = ss->expand_cache;

  /* Update the active factor in the cache. */
  if (vertex == SCULPT_EXPAND_VERTEX_NONE) {
    /* This means that the cursor is not over the mesh, so a valid active falloff can't be
     * determined. In this situations, don't evaluate enabled states and default all vertices in
     * connected components to enabled. */
    expand_cache->active_falloff = expand_cache->max_vert_falloff;
    expand_cache->all_enabled = true;
  }
  else {
    expand_cache->active_falloff = expand_cache->vert_falloff[vertex];
    expand_cache->all_enabled = false;
  }

  if (expand_cache->target == SCULPT_EXPAND_TARGET_FACE_SETS) {
    /* Face sets needs to be restored their initial state on each iteration as the overwrite
     * existing data. */
    sculpt_expand_face_sets_restore(ss, expand_cache);
  }

  /* Update the mesh sculpt data. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = expand_cache->nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, expand_cache->totnode);

  switch (expand_cache->target) {
    case SCULPT_EXPAND_TARGET_MASK:
      BLI_task_parallel_range(
          0, expand_cache->totnode, &data, sculpt_expand_mask_update_task_cb, &settings);
      break;
    case SCULPT_EXPAND_TARGET_FACE_SETS:
      sculpt_expand_face_sets_update(ss, expand_cache);
      break;
    case SCULPT_EXPAND_TARGET_COLORS:
      BLI_task_parallel_range(
          0, expand_cache->totnode, &data, sculpt_expand_colors_update_task_cb, &settings);
      break;
  }

  sculpt_expand_flush_updates(C);
}

/**
 * Updates the #SculptSession cursor data and gets the active vertex
 * if the cursor is over the mesh.
 */
static int sculpt_expand_target_vertex_update_and_get(bContext *C,
                                                      Object *ob,
                                                      const float mouse[2])
{
  SculptSession *ss = ob->sculpt;
  SculptCursorGeometryInfo sgi;
  if (SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false)) {
    return SCULPT_active_vertex_get(ss);
  }
  else {
    return SCULPT_EXPAND_VERTEX_NONE;
  }
}

/**
 * Moves the sculpt pivot to the average point of the boundary enabled vertices of the current
 * expand state. Take symmetry and active components into account.
 */
static void sculpt_expand_reposition_pivot(bContext *C, Object *ob, ExpandCache *expand_cache)
{
  SculptSession *ss = ob->sculpt;
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  const int totvert = SCULPT_vertex_count_get(ss);

  const bool initial_invert_state = expand_cache->invert;
  expand_cache->invert = false;
  BLI_bitmap *enabled_vertices = sculpt_expand_bitmap_from_enabled(ss, expand_cache);

  /* For boundary topology, position the pivot using only the boundary of the enabled vertices,
   * without taking mesh boundary into account. This allows to create deformations like bending the
   * mesh from the boundary of the mask that was just created. */
  const float use_mesh_boundary = expand_cache->falloff_type !=
                                  SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY;

  BLI_bitmap *boundary_vertices = sculpt_expand_boundary_from_enabled(
      ss, enabled_vertices, use_mesh_boundary);

  /* Ignore invert state, as this is the expected behavior in most cases and mask are created in
   * inverted state by default. */
  expand_cache->invert = initial_invert_state;

  int total = 0;
  float avg[3] = {0.0f};

  const float *expand_init_co = SCULPT_vertex_co_get(ss, expand_cache->initial_active_vertex);

  for (int i = 0; i < totvert; i++) {
    if (!BLI_BITMAP_TEST(boundary_vertices, i)) {
      continue;
    }

    if (!sculpt_expand_is_vert_in_active_component(ss, expand_cache, i)) {
      continue;
    }

    const float *vertex_co = SCULPT_vertex_co_get(ss, i);

    if (!SCULPT_check_vertex_pivot_symmetry(vertex_co, expand_init_co, symm)) {
      continue;
    }

    add_v3_v3(avg, vertex_co);
    total++;
  }

  MEM_freeN(enabled_vertices);
  MEM_freeN(boundary_vertices);

  if (total > 0) {
    mul_v3_v3fl(ss->pivot_pos, avg, 1.0f / total);
  }

  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
}

static void sculpt_expand_finish(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  SCULPT_undo_push_end();

  /* Tag all nodes to redraw to avoid artifacts after the fast partial updates. */
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);
  for (int n = 0; n < totnode; n++) {
    BKE_pbvh_node_mark_update_mask(nodes[n]);
  }
  MEM_freeN(nodes);

  switch (ss->expand_cache->target) {
    case SCULPT_EXPAND_TARGET_MASK:
      SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
      break;
    case SCULPT_EXPAND_TARGET_FACE_SETS:
      SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
      break;
    case SCULPT_EXPAND_TARGET_COLORS:
      SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COLOR);
      break;
  }

  sculpt_expand_cache_free(ss);
  ED_workspace_status_text(C, NULL);
}

/**
 * Finds and stores in the #ExpandCache the sculpt connected component index for each symmetry pass
 * needed for expand.
 */
static void sculpt_expand_find_active_connected_components_from_vert(Object *ob,
                                                                     ExpandCache *expand_cache,
                                                                     const int initial_vertex)
{
  SculptSession *ss = ob->sculpt;
  for (int i = 0; i < EXPAND_SYMM_AREAS; i++) {
    expand_cache->active_connected_components[i] = EXPAND_ACTIVE_COMPONENT_NONE;
  }

  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }

    const int symm_vertex = sculpt_expand_get_vertex_index_for_symmetry_pass(
        ob, symm_it, initial_vertex);

    expand_cache->active_connected_components[(int)symm_it] =
        ss->vertex_info.connected_component[symm_vertex];
  }
}

/**
 * Stores the active vertex, Face Set and mouse coordinates in the #ExpandCache based on the
 * current cursor position.
 */
static void sculpt_expand_set_initial_components_for_mouse(bContext *C,
                                                           Object *ob,
                                                           ExpandCache *expand_cache,
                                                           const float mouse[2])
{
  SculptSession *ss = ob->sculpt;
  int initial_vertex = sculpt_expand_target_vertex_update_and_get(C, ob, mouse);
  if (initial_vertex == SCULPT_EXPAND_VERTEX_NONE) {
    /* Cursor not over the mesh, for creating valid initial falloffs, fallback to the last active
     * vertex in the sculpt session. */
    initial_vertex = SCULPT_active_vertex_get(ss);
  }
  copy_v2_v2(ss->expand_cache->initial_mouse, mouse);
  expand_cache->initial_active_vertex = initial_vertex;
  expand_cache->initial_active_face_set = SCULPT_active_face_set_get(ss);

  if (expand_cache->next_face_set == SCULPT_FACE_SET_NONE) {
    /* Only set the next face set once, otherwise this ID will constantly update to a new one each
     * time this function is called for using a new initial vertex from a different cursor
     * position. */
    if (expand_cache->modify_active_face_set) {
      expand_cache->next_face_set = SCULPT_active_face_set_get(ss);
    }
    else {
      expand_cache->next_face_set = ED_sculpt_face_sets_find_next_available_id(ob->data);
    }
  }

  /* The new mouse position can be over a different connected component, so this needs to be
   * updated. */
  sculpt_expand_find_active_connected_components_from_vert(ob, expand_cache, initial_vertex);
}

/**
 * Displaces the initial mouse coordinates using the new mouse position to get a new active vertex.
 * After that, initializes a new falloff of the same type with the new active vertex.
 */
static void sculpt_expand_move_propagation_origin(bContext *C,
                                                  Object *ob,
                                                  const wmEvent *event,
                                                  ExpandCache *expand_cache)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  const float mouse[2] = {event->mval[0], event->mval[1]};
  float move_disp[2];
  sub_v2_v2v2(move_disp, mouse, expand_cache->initial_mouse_move);

  float new_mouse[2];
  add_v2_v2v2(new_mouse, move_disp, expand_cache->original_mouse_move);

  sculpt_expand_set_initial_components_for_mouse(C, ob, expand_cache, new_mouse);
  sculpt_expand_falloff_factors_from_vertex_and_symm_create(
      expand_cache,
      sd,
      ob,
      expand_cache->initial_active_vertex,
      expand_cache->move_preview_falloff_type);
}

/**
 * Ensures that the #SculptSession contains the required data needed for Expand.
 */
static void sculpt_expand_ensure_sculptsession_data(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_connected_components_ensure(ob);
  SCULPT_boundary_info_ensure(ob);
  if (!ss->tex_pool) {
    ss->tex_pool = BKE_image_pool_new();
  }
}

/**
 * Returns the active Face Sets ID from the enabled face or grid in the #SculptSession.
 */
static int sculpt_expand_active_face_set_id_get(SculptSession *ss, ExpandCache *expand_cache)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return expand_cache->original_face_sets[ss->active_face_index];
    case PBVH_GRIDS: {
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg,
                                                               ss->active_grid_index);
      return expand_cache->original_face_sets[face_index];
    }
    case PBVH_BMESH: {
      /* Dyntopo does not support Face Set functionality. */
      BLI_assert(false);
    }
  }
  return SCULPT_FACE_SET_NONE;
}

static int sculpt_expand_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  /* Skips INBETWEEN_MOUSEMOVE events and other events that may cause unnecessary updates. */
  if (!ELEM(event->type, MOUSEMOVE, EVT_MODAL_MAP)) {
    return OPERATOR_RUNNING_MODAL;
  }

  /* Update SculptSession data. */
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);
  sculpt_expand_ensure_sculptsession_data(ob);

  /* Update and get the active vertex (and face) from the cursor. */
  const float mouse[2] = {event->mval[0], event->mval[1]};
  const int target_expand_vertex = sculpt_expand_target_vertex_update_and_get(C, ob, mouse);

  /* Handle the modal keymap state changes. */
  ExpandCache *expand_cache = ss->expand_cache;
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case SCULPT_EXPAND_MODAL_CANCEL: {
        sculpt_expand_cancel(C, op);
        return OPERATOR_FINISHED;
      }
      case SCULPT_EXPAND_MODAL_INVERT: {
        expand_cache->invert = !expand_cache->invert;
        break;
      }
      case SCULPT_EXPAND_MODAL_PRESERVE_TOGGLE: {
        expand_cache->preserve = !expand_cache->preserve;
        break;
      }
      case SCULPT_EXPAND_MODAL_GRADIENT_TOGGLE: {
        expand_cache->falloff_gradient = !expand_cache->falloff_gradient;
        break;
      }
      case SCULPT_EXPAND_MODAL_BRUSH_GRADIENT_TOGGLE: {
        expand_cache->brush_gradient = !expand_cache->brush_gradient;
        if (expand_cache->brush_gradient) {
          expand_cache->falloff_gradient = true;
        }
        break;
      }
      case SCULPT_EXPAND_MODAL_SNAP_TOGGLE: {
        if (expand_cache->snap) {
          expand_cache->snap = false;
          if (expand_cache->snap_enabled_face_sets) {
            BLI_gset_free(expand_cache->snap_enabled_face_sets, NULL);
            expand_cache->snap_enabled_face_sets = NULL;
          }
        }
        else {
          expand_cache->snap = true;
          if (!expand_cache->snap_enabled_face_sets) {
            expand_cache->snap_enabled_face_sets = BLI_gset_int_new("snap face sets");
          }
          sculpt_expand_snap_initialize_from_enabled(ss, expand_cache);
        }
      } break;
      case SCULPT_EXPAND_MODAL_MOVE_TOGGLE: {
        if (expand_cache->move) {
          expand_cache->move = false;
          sculpt_expand_falloff_factors_from_vertex_and_symm_create(
              expand_cache,
              sd,
              ob,
              expand_cache->initial_active_vertex,
              expand_cache->move_original_falloff_type);
          break;
        }
        else {
          expand_cache->move = true;
          expand_cache->move_original_falloff_type = expand_cache->falloff_type;
          copy_v2_v2(expand_cache->initial_mouse_move, mouse);
          copy_v2_v2(expand_cache->original_mouse_move, expand_cache->initial_mouse);
          if (expand_cache->falloff_type == SCULPT_EXPAND_FALLOFF_GEODESIC &&
              SCULPT_vertex_count_get(ss) > expand_cache->max_geodesic_move_preview) {
            /* Set to spherical falloff for preview in high poly meshes as it is the fastest one.
             * In most cases it should match closely the preview from geodesic. */
            expand_cache->move_preview_falloff_type = SCULPT_EXPAND_FALLOFF_SPHERICAL;
          }
          else {
            expand_cache->move_preview_falloff_type = expand_cache->falloff_type;
          }
        }
        break;
      }
      case SCULPT_EXPAND_MODAL_RECURSION_STEP_GEODESIC: {
        sculpt_expand_resursion_step_add(ob, expand_cache, SCULPT_EXPAND_RECURSION_GEODESICS);
        break;
      }
      case SCULPT_EXPAND_MODAL_RECURSION_STEP_TOPOLOGY: {
        sculpt_expand_resursion_step_add(ob, expand_cache, SCULPT_EXPAND_RECURSION_TOPOLOGY);
        break;
      }
      case SCULPT_EXPAND_MODAL_CONFIRM: {
        sculpt_expand_update_for_vertex(C, ob, target_expand_vertex);

        if (expand_cache->reposition_pivot) {
          sculpt_expand_reposition_pivot(C, ob, expand_cache);
        }

        sculpt_expand_finish(C);
        return OPERATOR_FINISHED;
      }
      case SCULPT_EXPAND_MODAL_FALLOFF_GEODESIC: {
        sculpt_expand_falloff_factors_from_vertex_and_symm_create(
            expand_cache,
            sd,
            ob,
            expand_cache->initial_active_vertex,
            SCULPT_EXPAND_FALLOFF_GEODESIC);
        break;
      }
      case SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY: {
        sculpt_expand_falloff_factors_from_vertex_and_symm_create(
            expand_cache,
            sd,
            ob,
            expand_cache->initial_active_vertex,
            SCULPT_EXPAND_FALLOFF_TOPOLOGY);
        break;
      }
      case SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY_DIAGONALS: {
        sculpt_expand_falloff_factors_from_vertex_and_symm_create(
            expand_cache,
            sd,
            ob,
            expand_cache->initial_active_vertex,
            SCULPT_EXPAND_FALLOFF_TOPOLOGY_DIAGONALS);
        break;
      }
      case SCULPT_EXPAND_MODAL_FALLOFF_SPHERICAL: {
        sculpt_expand_falloff_factors_from_vertex_and_symm_create(
            expand_cache,
            sd,
            ob,
            expand_cache->initial_active_vertex,
            SCULPT_EXPAND_FALLOFF_SPHERICAL);
        break;
      }
      case SCULPT_EXPAND_MODAL_LOOP_COUNT_INCREASE: {
        expand_cache->loop_count += 1;
        break;
      }
      case SCULPT_EXPAND_MODAL_LOOP_COUNT_DECREASE: {
        expand_cache->loop_count -= 1;
        expand_cache->loop_count = max_ii(expand_cache->loop_count, 1);
        break;
      }
      case SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_INCREASE: {
        if (expand_cache->texture_distortion_strength == 0.0f) {
          if (expand_cache->brush->mtex.tex == NULL) {
            BKE_report(op->reports,
                       RPT_WARNING,
                       "Active brush does not contain any texture to distort the expand boundary");
            break;
          }
          if (expand_cache->brush->mtex.brush_map_mode != MTEX_MAP_MODE_3D) {
            BKE_report(op->reports,
                       RPT_WARNING,
                       "Texture mapping not set to 3D, results may be unpredictable");
          }
        }
        expand_cache->texture_distortion_strength += SCULPT_EXPAND_TEXTURE_DISTORTION_STEP;
        break;
      }
      case SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_DECREASE: {
        expand_cache->texture_distortion_strength -= SCULPT_EXPAND_TEXTURE_DISTORTION_STEP;
        expand_cache->texture_distortion_strength = max_ff(
            expand_cache->texture_distortion_strength, 0.0f);
        break;
      }
    }
  }

  /* Handle expand origin movement if enabled. */
  if (expand_cache->move) {
    sculpt_expand_move_propagation_origin(C, ob, event, expand_cache);
  }

  /* Add new Face Sets IDs to the snapping gset if enabled. */
  if (expand_cache->snap) {
    const int active_face_set_id = sculpt_expand_active_face_set_id_get(ss, expand_cache);
    if (!BLI_gset_haskey(expand_cache->snap_enabled_face_sets,
                         POINTER_FROM_INT(active_face_set_id))) {
      BLI_gset_add(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(active_face_set_id));
    }
  }

  /* Update the sculpt data with the current state of the #ExpandCache. */
  sculpt_expand_update_for_vertex(C, ob, target_expand_vertex);

  return OPERATOR_RUNNING_MODAL;
}

/**
 * Deletes the `delete_id` Face Set ID from the mesh Face Sets
 * and stores the result in `r_face_set`.
 * The faces that were using the `delete_id` Face Set are filled
 * using the content from their neighbors.
 */
static void sculpt_expand_delete_face_set_id(
    int *r_face_sets, Mesh *mesh, MeshElemMap *pmap, const int totface, const int delete_id)
{
  /* Check that all the face sets IDs in the mesh are not equal to `delete_id`
   * before attempting to delete it. */
  bool all_same_id = true;
  for (int i = 0; i < totface; i++) {
    if (r_face_sets[i] != delete_id) {
      all_same_id = false;
      break;
    }
  }
  if (all_same_id) {
    return;
  }

  BLI_LINKSTACK_DECLARE(queue, void *);
  BLI_LINKSTACK_DECLARE(queue_next, void *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  for (int i = 0; i < totface; i++) {
    if (r_face_sets[i] == delete_id) {
      BLI_LINKSTACK_PUSH(queue, POINTER_FROM_INT(i));
    }
  }

  while (BLI_LINKSTACK_SIZE(queue)) {
    while (BLI_LINKSTACK_SIZE(queue)) {
      const int f_index = POINTER_AS_INT(BLI_LINKSTACK_POP(queue));
      int other_id = delete_id;
      const MPoly *c_poly = &mesh->mpoly[f_index];
      for (int l = 0; l < c_poly->totloop; l++) {
        const MLoop *c_loop = &mesh->mloop[c_poly->loopstart + l];
        const MeshElemMap *vert_map = &pmap[c_loop->v];
        for (int i = 0; i < vert_map->count; i++) {

          const int neighbor_face_index = vert_map->indices[i];
          if (r_face_sets[neighbor_face_index] != delete_id) {
            other_id = r_face_sets[neighbor_face_index];
          }
        }
      }

      if (other_id != delete_id) {
        r_face_sets[f_index] = other_id;
      }
      else {
        BLI_LINKSTACK_PUSH(queue_next, POINTER_FROM_INT(f_index));
      }
    }

    BLI_LINKSTACK_SWAP(queue, queue_next);
  }

  BLI_LINKSTACK_FREE(queue);
  BLI_LINKSTACK_FREE(queue_next);
}

static void sculpt_expand_cache_initial_config_set(bContext *C,
                                                   wmOperator *op,
                                                   ExpandCache *expand_cache)
{
  /* RNA properties. */
  expand_cache->invert = RNA_boolean_get(op->ptr, "invert");
  expand_cache->preserve = RNA_boolean_get(op->ptr, "use_mask_preserve");
  expand_cache->falloff_gradient = RNA_boolean_get(op->ptr, "use_falloff_gradient");
  expand_cache->target = RNA_enum_get(op->ptr, "target");
  expand_cache->modify_active_face_set = RNA_boolean_get(op->ptr, "use_modify_active");
  expand_cache->reposition_pivot = RNA_boolean_get(op->ptr, "use_reposition_pivot");
  expand_cache->max_geodesic_move_preview = RNA_int_get(op->ptr, "max_geodesic_move_preview");

  /* These can be exposed in RNA if needed. */
  expand_cache->loop_count = 1;
  expand_cache->brush_gradient = false;

  /* Texture and color data from the active Brush. */
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;
  expand_cache->brush = BKE_paint_brush(&sd->paint);
  BKE_curvemapping_init(expand_cache->brush->curve);
  copy_v4_fl(expand_cache->fill_color, 1.0f);
  copy_v3_v3(expand_cache->fill_color, BKE_brush_color_get(ss->scene, expand_cache->brush));
  IMB_colormanagement_srgb_to_scene_linear_v3(expand_cache->fill_color);

  expand_cache->scene = CTX_data_scene(C);
  expand_cache->mtex = &expand_cache->brush->mtex;
  expand_cache->texture_distortion_strength = 0.0f;
  expand_cache->blend_mode = expand_cache->brush->blend;
}

/**
 * Does the undo sculpt push for the affected target data of the #ExpandCache.
 */
static void sculpt_expand_undo_push(Object *ob, ExpandCache *expand_cache)
{
  SculptSession *ss = ob->sculpt;
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

  switch (expand_cache->target) {
    case SCULPT_EXPAND_TARGET_MASK:
      for (int i = 0; i < totnode; i++) {
        SCULPT_undo_push_node(ob, nodes[i], SCULPT_UNDO_MASK);
      }
      break;
    case SCULPT_EXPAND_TARGET_FACE_SETS:
      SCULPT_undo_push_node(ob, nodes[0], SCULPT_UNDO_FACE_SETS);
      break;
    case SCULPT_EXPAND_TARGET_COLORS:
      for (int i = 0; i < totnode; i++) {
        SCULPT_undo_push_node(ob, nodes[i], SCULPT_UNDO_COLOR);
      }
      break;
  }

  MEM_freeN(nodes);
}

static int sculpt_expand_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  /* Create and configure the Expand Cache. */
  ss->expand_cache = MEM_callocN(sizeof(ExpandCache), "expand cache");
  sculpt_expand_cache_initial_config_set(C, op, ss->expand_cache);

  /* Update object. */
  const bool needs_colors = ss->expand_cache->target == SCULPT_EXPAND_TARGET_COLORS;

  if (needs_colors) {
    /* CTX_data_ensure_evaluated_depsgraph should be used at the end to include the updates of
     * earlier steps modifying the data. */
    BKE_sculpt_color_layer_create_if_needed(ob);
    depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, needs_colors);

  /* Do nothing when the mesh has 0 vertices. */
  const int totvert = SCULPT_vertex_count_get(ss);
  if (totvert == 0) {
    sculpt_expand_cache_free(ss);
    return OPERATOR_CANCELLED;
  }

  /* Face Set operations are not supported in dyntopo. */
  if (ss->expand_cache->target == SCULPT_EXPAND_TARGET_FACE_SETS &&
      BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    sculpt_expand_cache_free(ss);
    return OPERATOR_CANCELLED;
  }

  sculpt_expand_ensure_sculptsession_data(ob);

  /* Initialize undo. */
  SCULPT_undo_push_begin(ob, "expand");
  sculpt_expand_undo_push(ob, ss->expand_cache);

  /* Set the initial element for expand from the event position. */
  const float mouse[2] = {event->mval[0], event->mval[1]};
  sculpt_expand_set_initial_components_for_mouse(C, ob, ss->expand_cache, mouse);

  /* Cache PBVH nodes. */
  BKE_pbvh_search_gather(
      ss->pbvh, NULL, NULL, &ss->expand_cache->nodes, &ss->expand_cache->totnode);

  /* Store initial state. */
  sculpt_expand_original_state_store(ob, ss->expand_cache);

  if (ss->expand_cache->modify_active_face_set) {
    sculpt_expand_delete_face_set_id(ss->expand_cache->initial_face_sets,
                                     ob->data,
                                     ss->pmap,
                                     ss->totfaces,
                                     ss->expand_cache->next_face_set);
  }

  /* Initialize the falloff. */
  eSculptExpandFalloffType falloff_type = RNA_enum_get(op->ptr, "falloff_type");

  /* When starting from a boundary vertex, set the initial falloff to boundary. */
  if (SCULPT_vertex_is_boundary(ss, ss->expand_cache->initial_active_vertex)) {
    falloff_type = SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY;
  }

  sculpt_expand_falloff_factors_from_vertex_and_symm_create(
      ss->expand_cache, sd, ob, ss->expand_cache->initial_active_vertex, falloff_type);

  /* Initial mesh data update, resets all target data in the sculpt mesh. */
  sculpt_expand_update_for_vertex(C, ob, ss->expand_cache->initial_active_vertex);

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void sculpt_expand_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {SCULPT_EXPAND_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {SCULPT_EXPAND_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {SCULPT_EXPAND_MODAL_INVERT, "INVERT", 0, "Invert", ""},
      {SCULPT_EXPAND_MODAL_PRESERVE_TOGGLE, "PRESERVE", 0, "Toggle Preserve State", ""},
      {SCULPT_EXPAND_MODAL_GRADIENT_TOGGLE, "GRADIENT", 0, "Toggle Gradient", ""},
      {SCULPT_EXPAND_MODAL_RECURSION_STEP_GEODESIC,
       "RECURSION_STEP_GEODESIC",
       0,
       "Geodesic recursion step",
       ""},
      {SCULPT_EXPAND_MODAL_RECURSION_STEP_TOPOLOGY,
       "RECURSION_STEP_TOPOLOGY",
       0,
       "Topology recursion Step",
       ""},
      {SCULPT_EXPAND_MODAL_MOVE_TOGGLE, "MOVE_TOGGLE", 0, "Move Origin", ""},
      {SCULPT_EXPAND_MODAL_FALLOFF_GEODESIC, "FALLOFF_GEODESICS", 0, "Geodesic Falloff", ""},
      {SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY, "FALLOFF_TOPOLOGY", 0, "Topology Falloff", ""},
      {SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY_DIAGONALS,
       "FALLOFF_TOPOLOGY_DIAGONALS",
       0,
       "Diagonals Falloff",
       ""},
      {SCULPT_EXPAND_MODAL_FALLOFF_SPHERICAL, "FALLOFF_SPHERICAL", 0, "Spherical Falloff", ""},
      {SCULPT_EXPAND_MODAL_SNAP_TOGGLE, "SNAP_TOGGLE", 0, "Snap expand to Face Sets", ""},
      {SCULPT_EXPAND_MODAL_LOOP_COUNT_INCREASE,
       "LOOP_COUNT_INCREASE",
       0,
       "Loop Count Increase",
       ""},
      {SCULPT_EXPAND_MODAL_LOOP_COUNT_DECREASE,
       "LOOP_COUNT_DECREASE",
       0,
       "Loop Count Decrease",
       ""},
      {SCULPT_EXPAND_MODAL_BRUSH_GRADIENT_TOGGLE,
       "BRUSH_GRADIENT_TOGGLE",
       0,
       "Toggle Brush Gradient",
       ""},
      {SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_INCREASE,
       "TEXTURE_DISTORTION_INCREASE",
       0,
       "Texture Distortion Increase",
       ""},
      {SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_DECREASE,
       "TEXTURE_DISTORTION_DECREASE",
       0,
       "Texture Distortion Decrease",
       ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const char *name = "Sculpt Expand Modal";
  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, name);

  /* This function is called for each spacetype, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, name, modal_items);
  WM_modalkeymap_assign(keymap, "SCULPT_OT_expand");
}

void SCULPT_OT_expand(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Expand";
  ot->idname = "SCULPT_OT_expand";
  ot->description = "Generic sculpt expand operator";

  /* API callbacks. */
  ot->invoke = sculpt_expand_invoke;
  ot->modal = sculpt_expand_modal;
  ot->cancel = sculpt_expand_cancel;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  static EnumPropertyItem prop_sculpt_expand_falloff_type_items[] = {
      {SCULPT_EXPAND_FALLOFF_GEODESIC, "GEODESIC", 0, "Geodesic", ""},
      {SCULPT_EXPAND_FALLOFF_TOPOLOGY, "TOPOLOGY", 0, "Topology", ""},
      {SCULPT_EXPAND_FALLOFF_TOPOLOGY_DIAGONALS,
       "TOPOLOGY_DIAGONALS",
       0,
       "Topology Diagonals",
       ""},
      {SCULPT_EXPAND_FALLOFF_NORMALS, "NORMALS", 0, "Normals", ""},
      {SCULPT_EXPAND_FALLOFF_SPHERICAL, "SPHERICAL", 0, "Spherical", ""},
      {SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY, "BOUNDARY_TOPOLOGY", 0, "Boundary Topology", ""},
      {SCULPT_EXPAND_FALLOFF_BOUNDARY_FACE_SET, "BOUNDARY_FACE_SET", 0, "Boundary Face Set", ""},
      {SCULPT_EXPAND_FALLOFF_ACTIVE_FACE_SET, "ACTIVE_FACE_SET", 0, "Active Face Set", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static EnumPropertyItem prop_sculpt_expand_target_type_items[] = {
      {SCULPT_EXPAND_TARGET_MASK, "MASK", 0, "Mask", ""},
      {SCULPT_EXPAND_TARGET_FACE_SETS, "FACE_SETS", 0, "Face Sets", ""},
      {SCULPT_EXPAND_TARGET_COLORS, "COLOR", 0, "Color", ""},
      {0, NULL, 0, NULL, NULL},
  };

  RNA_def_enum(ot->srna,
               "target",
               prop_sculpt_expand_target_type_items,
               SCULPT_EXPAND_TARGET_MASK,
               "Data Target",
               "Data that is going to be modified in the expand operation");

  RNA_def_enum(ot->srna,
               "falloff_type",
               prop_sculpt_expand_falloff_type_items,
               SCULPT_EXPAND_FALLOFF_GEODESIC,
               "Falloff Type",
               "Initial falloff of the expand operation");

  ot->prop = RNA_def_boolean(
      ot->srna, "invert", false, "Invert", "Invert the expand active elements");
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_mask_preserve",
                             false,
                             "Preserve Previous",
                             "Preserve the previous state of the target data");
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_falloff_gradient",
                             false,
                             "Falloff Gradient",
                             "Expand Using a linear falloff");

  ot->prop = RNA_def_boolean(ot->srna,
                             "use_modify_active",
                             false,
                             "Modify Active",
                             "Modify the active Face Set instead of creating a new one");

  ot->prop = RNA_def_boolean(
      ot->srna,
      "use_reposition_pivot",
      true,
      "Reposition Pivot",
      "Reposition the sculpt transform pivot to the boundary of the expand active area");

  ot->prop = RNA_def_int(ot->srna,
                         "max_geodesic_move_preview",
                         10000,
                         0,
                         INT_MAX,
                         "Max Vertex Count for Geodesic Move Preview",
                         "Maximum number of vertices in the mesh for using geodesic falloff when "
                         "moving the origin of expand. If the total number of vertices is greater "
                         "than this value, the falloff will be set to spherical when moving",
                         0,
                         1000000);
}
