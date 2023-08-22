/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <cmath>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_linklist_stack.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.hh"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.hh"

#include "DEG_depsgraph.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_screen.hh"
#include "ED_sculpt.hh"
#include "paint_intern.hh"
#include "sculpt_intern.hh"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"

#include "bmesh.h"

/* Sculpt Expand. */
/* Operator for creating selections and patterns in Sculpt Mode. Expand can create masks, face sets
 * and fill vertex colors. */
/* The main functionality of the operator
 * - The operator initializes a value per vertex, called "falloff". There are multiple algorithms
 * to generate these falloff values which will create different patterns in the result when using
 * the operator. These falloff values require algorithms that rely on mesh connectivity, so they
 * are only valid on parts of the mesh that are in the same connected component as the given
 * initial verts. If needed, these falloff values are propagated from vertex or grids into the
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
 * some situations, verts which have the same falloff value as max_falloff will start a new
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
  SCULPT_EXPAND_MODAL_SNAP_ENABLE,
  SCULPT_EXPAND_MODAL_SNAP_DISABLE,
  SCULPT_EXPAND_MODAL_LOOP_COUNT_INCREASE,
  SCULPT_EXPAND_MODAL_LOOP_COUNT_DECREASE,
  SCULPT_EXPAND_MODAL_BRUSH_GRADIENT_TOGGLE,
  SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_INCREASE,
  SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_DECREASE,
};

/* Functions for getting the state of mesh elements (verts and base mesh faces). When the main
 * functions for getting the state of an element return true it means that data associated to that
 * element will be modified by expand. */

/**
 * Returns true if the vertex is in a connected component with correctly initialized falloff
 * values.
 */
static bool sculpt_expand_is_vert_in_active_component(SculptSession *ss,
                                                      ExpandCache *expand_cache,
                                                      const PBVHVertRef v)
{
  for (int i = 0; i < EXPAND_SYMM_AREAS; i++) {
    if (SCULPT_vertex_island_get(ss, v) == expand_cache->active_connected_islands[i]) {
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
                                                      const PBVHFaceRef f)
{
  PBVHVertRef vertex;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      vertex.i = ss->corner_verts[ss->faces[f.i].start()];
      break;
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      vertex.i = ss->faces[f.i].start() * key->grid_area;
      break;
    }
    case PBVH_BMESH: {
      BMFace *face = reinterpret_cast<BMFace *>(f.i);
      vertex.i = reinterpret_cast<intptr_t>(face->l_first->v);
      break;
    }
  }
  return sculpt_expand_is_vert_in_active_component(ss, expand_cache, vertex);
}

/**
 * Returns the falloff value of a vertex. This function includes texture distortion, which is not
 * precomputed into the initial falloff values.
 */
static float sculpt_expand_falloff_value_vertex_get(SculptSession *ss,
                                                    ExpandCache *expand_cache,
                                                    const PBVHVertRef v)
{
  const int v_i = BKE_pbvh_vertex_to_index(ss->pbvh, v);

  if (expand_cache->texture_distortion_strength == 0.0f) {
    return expand_cache->vert_falloff[v_i];
  }
  const Brush *brush = expand_cache->brush;
  const MTex *mtex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);
  if (!mtex->tex) {
    return expand_cache->vert_falloff[v_i];
  }

  float rgba[4];
  const float *vertex_co = SCULPT_vertex_co_get(ss, v);
  const float avg = BKE_brush_sample_tex_3d(
      expand_cache->scene, brush, mtex, vertex_co, rgba, 0, ss->tex_pool);

  const float distortion = (avg - 0.5f) * expand_cache->texture_distortion_strength *
                           expand_cache->max_vert_falloff;
  return expand_cache->vert_falloff[v_i] + distortion;
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

  const MTex *mask_tex = BKE_brush_mask_texture_get(expand_cache->brush, OB_MODE_SCULPT);
  if (!mask_tex->tex) {
    return expand_cache->max_vert_falloff;
  }

  return expand_cache->max_vert_falloff +
         (0.5f * expand_cache->texture_distortion_strength * expand_cache->max_vert_falloff);
}

/**
 * Main function to get the state of a vertex for the current state and settings of a #ExpandCache.
 * Returns true when the target data should be modified by expand.
 */
static bool sculpt_expand_state_get(SculptSession *ss,
                                    ExpandCache *expand_cache,
                                    const PBVHVertRef v)
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
static bool sculpt_expand_face_state_get(SculptSession *ss,
                                         ExpandCache *expand_cache,
                                         const PBVHFaceRef f)
{
  int f_i = BKE_pbvh_face_to_index(ss->pbvh, f);

  if (ss->hide_poly && ss->hide_poly[f_i]) {
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
    const int face_set = expand_cache->original_face_sets[f_i];
    enabled = BLI_gset_haskey(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(face_set));
  }
  else {
    const float loop_len = (expand_cache->max_face_falloff / expand_cache->loop_count) +
                           SCULPT_EXPAND_LOOP_THRESHOLD;

    const float active_factor = fmod(expand_cache->active_falloff, loop_len);
    const float falloff_factor = fmod(expand_cache->face_falloff[f_i], loop_len);
    enabled = falloff_factor <= active_factor;
  }

  if (expand_cache->falloff_type == SCULPT_EXPAND_FALLOFF_ACTIVE_FACE_SET) {
    if (SCULPT_face_set_get(ss, f) == expand_cache->initial_active_face_set) {
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
                                              const PBVHVertRef v)
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

/* Utility functions for getting all verts state during expand. */

/**
 * Returns a bitmap indexed by vertex index which contains if the vertex was enabled or not for a
 * give expand_cache state.
 */
static BLI_bitmap *sculpt_expand_bitmap_from_enabled(SculptSession *ss, ExpandCache *expand_cache)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  BLI_bitmap *enabled_verts = BLI_BITMAP_NEW(totvert, "enabled verts");
  for (int i = 0; i < totvert; i++) {
    const bool enabled = sculpt_expand_state_get(
        ss, expand_cache, BKE_pbvh_index_to_vertex(ss->pbvh, i));
    BLI_BITMAP_SET(enabled_verts, i, enabled);
  }
  return enabled_verts;
}

/**
 * Returns a bitmap indexed by vertex index which contains if the vertex is in the boundary of the
 * enabled verts. This is defined as verts that are enabled and at least have one connected
 * vertex that is not enabled.
 */
static BLI_bitmap *sculpt_expand_boundary_from_enabled(SculptSession *ss,
                                                       const BLI_bitmap *enabled_verts,
                                                       const bool use_mesh_boundary)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  BLI_bitmap *boundary_verts = BLI_BITMAP_NEW(totvert, "boundary verts");
  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);
    if (!BLI_BITMAP_TEST(enabled_verts, i)) {
      continue;
    }

    bool is_expand_boundary = false;
    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
      if (!BLI_BITMAP_TEST(enabled_verts, ni.index)) {
        is_expand_boundary = true;
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    if (use_mesh_boundary && SCULPT_vertex_is_boundary(ss, vertex, SCULPT_BOUNDARY_MESH)) {
      is_expand_boundary = true;
    }

    BLI_BITMAP_SET(boundary_verts, i, is_expand_boundary);
  }

  return boundary_verts;
}

static void sculpt_expand_check_topology_islands(Object *ob, eSculptExpandFalloffType falloff_type)
{
  SculptSession *ss = ob->sculpt;

  ss->expand_cache->check_islands = ELEM(falloff_type,
                                         SCULPT_EXPAND_FALLOFF_GEODESIC,
                                         SCULPT_EXPAND_FALLOFF_TOPOLOGY,
                                         SCULPT_EXPAND_FALLOFF_TOPOLOGY_DIAGONALS,
                                         SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY,
                                         SCULPT_EXPAND_FALLOFF_NORMALS);

  if (ss->expand_cache->check_islands) {
    SCULPT_topology_islands_ensure(ob);
  }
}

/* Functions implementing different algorithms for initializing falloff values. */

/**
 * Utility function to get the closet vertex after flipping an original vertex position based on
 * an symmetry pass iteration index.
 */
static PBVHVertRef sculpt_expand_get_vertex_index_for_symmetry_pass(
    Object *ob, const char symm_it, const PBVHVertRef original_vertex)
{
  SculptSession *ss = ob->sculpt;
  PBVHVertRef symm_vertex = {SCULPT_EXPAND_VERTEX_NONE};

  if (symm_it == 0) {
    symm_vertex = original_vertex;
  }
  else {
    float location[3];
    flip_v3_v3(location, SCULPT_vertex_co_get(ss, original_vertex), ePaintSymmetryFlags(symm_it));
    symm_vertex = SCULPT_nearest_vertex_get(nullptr, ob, location, FLT_MAX, false);
  }
  return symm_vertex;
}

/**
 * Geodesic: Initializes the falloff with geodesic distances from the given active vertex, taking
 * symmetry into account.
 */
static float *sculpt_expand_geodesic_falloff_create(Sculpt *sd, Object *ob, const PBVHVertRef v)
{
  return SCULPT_geodesic_from_vertex_and_symm(sd, ob, v, FLT_MAX);
}

/**
 * Topology: Initializes the falloff using a flood-fill operation,
 * increasing the falloff value by 1 when visiting a new vertex.
 */
struct ExpandFloodFillData {
  float original_normal[3];
  float edge_sensitivity;
  float *dists;
  float *edge_factor;
};

static bool expand_topology_floodfill_cb(
    SculptSession *ss, PBVHVertRef from_v, PBVHVertRef to_v, bool is_duplicate, void *userdata)
{
  int from_v_i = BKE_pbvh_vertex_to_index(ss->pbvh, from_v);
  int to_v_i = BKE_pbvh_vertex_to_index(ss->pbvh, to_v);

  ExpandFloodFillData *data = static_cast<ExpandFloodFillData *>(userdata);
  if (!is_duplicate) {
    const float to_it = data->dists[from_v_i] + 1.0f;
    data->dists[to_v_i] = to_it;
  }
  else {
    data->dists[to_v_i] = data->dists[from_v_i];
  }
  return true;
}

static float *sculpt_expand_topology_falloff_create(Sculpt *sd, Object *ob, const PBVHVertRef v)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);
  float *dists = static_cast<float *>(MEM_calloc_arrayN(totvert, sizeof(float), __func__));

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
    SculptSession *ss, PBVHVertRef from_v, PBVHVertRef to_v, bool is_duplicate, void *userdata)
{
  int from_v_i = BKE_pbvh_vertex_to_index(ss->pbvh, from_v);
  int to_v_i = BKE_pbvh_vertex_to_index(ss->pbvh, to_v);

  ExpandFloodFillData *data = static_cast<ExpandFloodFillData *>(userdata);
  if (!is_duplicate) {
    float current_normal[3], prev_normal[3];
    SCULPT_vertex_normal_get(ss, to_v, current_normal);
    SCULPT_vertex_normal_get(ss, from_v, prev_normal);
    const float from_edge_factor = data->edge_factor[from_v_i];
    data->edge_factor[to_v_i] = dot_v3v3(current_normal, prev_normal) * from_edge_factor;
    data->dists[to_v_i] = dot_v3v3(data->original_normal, current_normal) *
                          powf(from_edge_factor, data->edge_sensitivity);
    CLAMP(data->dists[to_v_i], 0.0f, 1.0f);
  }
  else {
    /* PBVH_GRIDS duplicate handling. */
    data->edge_factor[to_v_i] = data->edge_factor[from_v_i];
    data->dists[to_v_i] = data->dists[from_v_i];
  }

  return true;
}

static float *sculpt_expand_normal_falloff_create(Sculpt *sd,
                                                  Object *ob,
                                                  const PBVHVertRef v,
                                                  const float edge_sensitivity,
                                                  const int blur_steps)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);
  float *dists = static_cast<float *>(MEM_calloc_arrayN(totvert, sizeof(float), __func__));
  float *edge_factor = static_cast<float *>(MEM_callocN(sizeof(float) * totvert, __func__));
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

  for (int repeat = 0; repeat < blur_steps; repeat++) {
    for (int i = 0; i < totvert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);
      float avg = 0.0f;

      SculptVertexNeighborIter ni;
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
        avg += dists[ni.index];
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

      if (ni.size > 0.0f) {
        dists[i] = avg / ni.size;
      }
    }
  }

  for (int i = 0; i < totvert; i++) {
    dists[i] = 1.0 - dists[i];
  }

  MEM_SAFE_FREE(edge_factor);

  return dists;
}

/**
 * Spherical: Initializes the falloff based on the distance from a vertex, taking symmetry into
 * account.
 */
static float *sculpt_expand_spherical_falloff_create(Object *ob, const PBVHVertRef v)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  float *dists = static_cast<float *>(MEM_malloc_arrayN(totvert, sizeof(float), __func__));
  for (int i = 0; i < totvert; i++) {
    dists[i] = FLT_MAX;
  }
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);

  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }
    const PBVHVertRef symm_vertex = sculpt_expand_get_vertex_index_for_symmetry_pass(
        ob, symm_it, v);
    if (symm_vertex.i != -1) {
      const float *co = SCULPT_vertex_co_get(ss, symm_vertex);
      for (int i = 0; i < totvert; i++) {
        dists[i] = min_ff(
            dists[i],
            len_v3v3(co, SCULPT_vertex_co_get(ss, BKE_pbvh_index_to_vertex(ss->pbvh, i))));
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
static float *sculpt_expand_boundary_topology_falloff_create(Sculpt *sd,
                                                             Object *ob,
                                                             const PBVHVertRef v)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);
  float *dists = static_cast<float *>(MEM_calloc_arrayN(totvert, sizeof(float), __func__));
  BLI_bitmap *visited_verts = BLI_BITMAP_NEW(totvert, "visited verts");
  GSQueue *queue = BLI_gsqueue_new(sizeof(PBVHVertRef));

  /* Search and initialize a boundary per symmetry pass, then mark those verts as visited. */
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }

    const PBVHVertRef symm_vertex = sculpt_expand_get_vertex_index_for_symmetry_pass(
        ob, symm_it, v);

    SculptBoundary *boundary = SCULPT_boundary_data_init(sd, ob, nullptr, symm_vertex, FLT_MAX);
    if (!boundary) {
      continue;
    }

    for (int i = 0; i < boundary->verts_num; i++) {
      BLI_gsqueue_push(queue, &boundary->verts[i]);
      BLI_BITMAP_ENABLE(visited_verts, BKE_pbvh_vertex_to_index(ss->pbvh, boundary->verts[i]));
    }
    SCULPT_boundary_data_free(boundary);
  }

  /* If there are no boundaries, return a falloff with all values set to 0. */
  if (BLI_gsqueue_is_empty(queue)) {
    return dists;
  }

  /* Propagate the values from the boundaries to the rest of the mesh. */
  while (!BLI_gsqueue_is_empty(queue)) {
    PBVHVertRef v_next;

    BLI_gsqueue_pop(queue, &v_next);

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, v_next, ni) {
      if (BLI_BITMAP_TEST(visited_verts, ni.index)) {
        continue;
      }

      const int v_next_i = BKE_pbvh_vertex_to_index(ss->pbvh, v_next);

      dists[ni.index] = dists[v_next_i] + 1.0f;
      BLI_BITMAP_ENABLE(visited_verts, ni.index);
      BLI_gsqueue_push(queue, &ni.vertex);
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  }

  BLI_gsqueue_free(queue);
  MEM_freeN(visited_verts);
  return dists;
}

/**
 * Topology diagonals. This falloff is similar to topology, but it also considers the diagonals of
 * the base mesh faces when checking a vertex neighbor. For this reason, this is not implement
 * using the general flood-fill and sculpt neighbors accessors.
 */
static float *sculpt_expand_diagonals_falloff_create(Object *ob, const PBVHVertRef v)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);
  float *dists = static_cast<float *>(MEM_calloc_arrayN(totvert, sizeof(float), __func__));

  /* This algorithm uses mesh data (faces and loops), so this falloff type can't be initialized for
   * Multires. Also supports non-tri PBVH_BMESH, though untested until we implement that properly*/
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES || (ss->bm && ss->bm->totloop != ss->bm->totvert * 3))
  {
    return dists;
  }

  if (ss->bm) {
    BM_mesh_elem_index_ensure(ss->bm, BM_VERT);
    BM_mesh_elem_table_ensure(ss->bm, BM_VERT);
  }

  /* Search and mask as visited the initial verts using the enabled symmetry passes. */
  BLI_bitmap *visited_verts = BLI_BITMAP_NEW(totvert, "visited verts");
  GSQueue *queue = BLI_gsqueue_new(sizeof(PBVHVertRef));
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }

    const PBVHVertRef symm_vertex = sculpt_expand_get_vertex_index_for_symmetry_pass(
        ob, symm_it, v);
    int symm_vertex_i = BKE_pbvh_vertex_to_index(ss->pbvh, symm_vertex);

    BLI_gsqueue_push(queue, &symm_vertex);
    BLI_BITMAP_ENABLE(visited_verts, symm_vertex_i);
  }

  if (BLI_gsqueue_is_empty(queue)) {
    return dists;
  }

  /* Propagate the falloff increasing the value by 1 each time a new vertex is visited. */
  while (!BLI_gsqueue_is_empty(queue)) {
    PBVHVertRef v_next;
    BLI_gsqueue_pop(queue, &v_next);

    int v_next_i = BKE_pbvh_vertex_to_index(ss->pbvh, v_next);

    if (ss->bm) {
      BMIter iter;
      BMFace *f;
      BMVert *v = (BMVert *)v_next.i;

      BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
        BMLoop *l = f->l_first;

        do {
          BMVert *neighbor_v = l->next->v;
          const int neighbor_v_i = BM_elem_index_get(neighbor_v);

          if (BLI_BITMAP_TEST(visited_verts, neighbor_v_i)) {
            l = l->next;
            continue;
          }

          dists[neighbor_v_i] = dists[v_next_i] + 1.0f;
          BLI_BITMAP_ENABLE(visited_verts, neighbor_v_i);
          BLI_gsqueue_push(queue, &neighbor_v);

          l = l->next;
        } while (l != f->l_first);
      }
    }
    else {
      Span<int> map = ss->pmap[v_next_i];

      for (int j : map.index_range()) {
        for (const int vert : ss->corner_verts.slice(ss->faces[map[j]])) {
          const PBVHVertRef neighbor_v = BKE_pbvh_make_vref(vert);
          if (BLI_BITMAP_TEST(visited_verts, neighbor_v.i)) {
            continue;
          }

          dists[neighbor_v.i] = dists[v_next_i] + 1.0f;
          BLI_BITMAP_ENABLE(visited_verts, neighbor_v.i);
          BLI_gsqueue_push(queue, &neighbor_v);
        }
      }
    }
  }

  BLI_gsqueue_free(queue);
  MEM_freeN(visited_verts);
  return dists;
}

/* Functions to update the max_falloff value in the #ExpandCache. These functions are called after
 * initializing a new falloff to make sure that this value is always updated. */

/**
 * Updates the max_falloff value for verts in a #ExpandCache based on the current values of the
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

    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    if (!sculpt_expand_is_vert_in_active_component(ss, expand_cache, vertex)) {
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
    PBVHFaceRef f = BKE_pbvh_index_to_face(ss->pbvh, i);

    if (expand_cache->face_falloff[i] == FLT_MAX) {
      continue;
    }

    if (!sculpt_expand_is_face_in_active_component(ss, expand_cache, f)) {
      continue;
    }

    expand_cache->max_face_falloff = max_ff(expand_cache->max_face_falloff,
                                            expand_cache->face_falloff[i]);
  }
}

/**
 * Functions to get falloff values for faces from the values from the verts. This is used for
 * expanding Face Sets. Depending on the data type of the #SculptSession, this needs to get the per
 * face falloff value from the connected verts of each face or from the grids stored per loops
 * for each face.
 */
static void sculpt_expand_grids_to_faces_falloff(SculptSession *ss,
                                                 Mesh *mesh,
                                                 ExpandCache *expand_cache)
{
  const blender::OffsetIndices faces = mesh->faces();
  const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);

  for (const int i : faces.index_range()) {
    float accum = 0.0f;
    for (const int corner : faces[i]) {
      const int grid_loop_index = corner * key->grid_area;
      for (int g = 0; g < key->grid_area; g++) {
        accum += expand_cache->vert_falloff[grid_loop_index + g];
      }
    }
    expand_cache->face_falloff[i] = accum / (faces[i].size() * key->grid_area);
  }
}

static void sculpt_expand_vertex_to_faces_falloff(Mesh *mesh, ExpandCache *expand_cache)
{
  const blender::OffsetIndices faces = mesh->faces();
  const blender::Span<int> corner_verts = mesh->corner_verts();

  for (const int i : faces.index_range()) {
    float accum = 0.0f;
    for (const int vert : corner_verts.slice(faces[i])) {
      accum += expand_cache->vert_falloff[vert];
    }
    expand_cache->face_falloff[i] = accum / faces[i].size();
  }
}

static void sculpt_expand_vertex_to_faces_falloff_bmesh(BMesh *bm, ExpandCache *expand_cache)
{
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMLoop *l = f->l_first;

    float accum = 0.0f;

    do {
      accum += expand_cache->vert_falloff[BM_elem_index_get(l->v)];
      l = l->next;
    } while (l != f->l_first);

    expand_cache->face_falloff[BM_elem_index_get(f)] = accum / f->len;
  }
}
/**
 * Main function to update the faces falloff from a already calculated vertex falloff.
 */
static void sculpt_expand_mesh_face_falloff_from_vertex_falloff(SculptSession *ss,
                                                                Mesh *mesh,
                                                                ExpandCache *expand_cache)
{
  BLI_assert(expand_cache->vert_falloff != nullptr);

  if (!expand_cache->face_falloff) {
    expand_cache->face_falloff = static_cast<float *>(
        MEM_malloc_arrayN(mesh->faces_num, sizeof(float), __func__));
  }

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      sculpt_expand_vertex_to_faces_falloff(mesh, expand_cache);
      break;
    case PBVH_GRIDS:
      sculpt_expand_grids_to_faces_falloff(ss, mesh, expand_cache);
      break;
    case PBVH_BMESH:
      sculpt_expand_vertex_to_faces_falloff_bmesh(ss->bm, expand_cache);
      break;
  }
}

/* Recursions. These functions will generate new falloff values based on the state of the verts
 * from the current ExpandCache options and falloff values. */

/**
 * Geodesic recursion: Initializes falloff values using geodesic distances from the boundary of the
 * current verts state.
 */
static void sculpt_expand_geodesics_from_state_boundary(Object *ob,
                                                        ExpandCache *expand_cache,
                                                        BLI_bitmap *enabled_verts)
{
  SculptSession *ss = ob->sculpt;
  BLI_assert(ELEM(BKE_pbvh_type(ss->pbvh), PBVH_GRIDS, PBVH_FACES, PBVH_BMESH));

  GSet *initial_verts = BLI_gset_int_new("initial_verts");
  BLI_bitmap *boundary_verts = sculpt_expand_boundary_from_enabled(ss, enabled_verts, false);
  const int totvert = SCULPT_vertex_count_get(ss);
  for (int i = 0; i < totvert; i++) {
    if (!BLI_BITMAP_TEST(boundary_verts, i)) {
      continue;
    }
    BLI_gset_add(initial_verts, POINTER_FROM_INT(i));
  }
  MEM_freeN(boundary_verts);

  MEM_SAFE_FREE(expand_cache->vert_falloff);
  MEM_SAFE_FREE(expand_cache->face_falloff);

  expand_cache->vert_falloff = SCULPT_geodesic_distances_create(
      ob, initial_verts, FLT_MAX, nullptr, nullptr);
  BLI_gset_free(initial_verts, nullptr);
}

/**
 * Topology recursion: Initializes falloff values using topology steps from the boundary of the
 * current verts state, increasing the value by 1 each time a new vertex is visited.
 */
static void sculpt_expand_topology_from_state_boundary(Object *ob,
                                                       ExpandCache *expand_cache,
                                                       BLI_bitmap *enabled_verts)
{
  MEM_SAFE_FREE(expand_cache->vert_falloff);
  MEM_SAFE_FREE(expand_cache->face_falloff);

  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  float *dists = static_cast<float *>(MEM_calloc_arrayN(totvert, sizeof(float), __func__));
  BLI_bitmap *boundary_verts = sculpt_expand_boundary_from_enabled(ss, enabled_verts, false);

  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  for (int i = 0; i < totvert; i++) {
    if (!BLI_BITMAP_TEST(boundary_verts, i)) {
      continue;
    }

    SCULPT_floodfill_add_and_skip_initial(ss, &flood, BKE_pbvh_index_to_vertex(ss->pbvh, i));
  }
  MEM_freeN(boundary_verts);

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

  BLI_bitmap *enabled_verts = sculpt_expand_bitmap_from_enabled(ss, expand_cache);

  /* Each time a new recursion step is created, reset the distortion strength. This is the expected
   * result from the recursion, as otherwise the new falloff will render with undesired distortion
   * from the beginning. */
  expand_cache->texture_distortion_strength = 0.0f;

  switch (recursion_type) {
    case SCULPT_EXPAND_RECURSION_GEODESICS:
      sculpt_expand_geodesics_from_state_boundary(ob, expand_cache, enabled_verts);
      break;
    case SCULPT_EXPAND_RECURSION_TOPOLOGY:
      sculpt_expand_topology_from_state_boundary(ob, expand_cache, enabled_verts);
      break;
  }

  sculpt_expand_update_max_vert_falloff_value(ss, expand_cache);
  if (expand_cache->target == SCULPT_EXPAND_TARGET_FACE_SETS) {
    sculpt_expand_mesh_face_falloff_from_vertex_falloff(
        ss, static_cast<Mesh *>(ob->data), expand_cache);
    sculpt_expand_update_max_face_falloff_factor(ss, expand_cache);
  }

  MEM_freeN(enabled_verts);
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

  BLI_bitmap *enabled_verts = BLI_BITMAP_NEW(totvert, "enabled verts");
  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vref = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    if (!SCULPT_vertex_has_unique_face_set(ss, vref)) {
      continue;
    }
    if (!SCULPT_vertex_has_face_set(ss, vref, active_face_set)) {
      continue;
    }
    BLI_BITMAP_ENABLE(enabled_verts, i);
  }

  /* TODO: Default to topology. */
  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
    sculpt_expand_geodesics_from_state_boundary(ob, expand_cache, enabled_verts);
  }
  else {
    sculpt_expand_topology_from_state_boundary(ob, expand_cache, enabled_verts);
  }

  MEM_freeN(enabled_verts);

  if (internal_falloff) {
    for (int i = 0; i < totvert; i++) {
      PBVHVertRef vref = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      if (!(SCULPT_vertex_has_face_set(ss, vref, active_face_set) &&
            SCULPT_vertex_has_unique_face_set(ss, vref)))
      {
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
      PBVHVertRef vref = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      if (!SCULPT_vertex_has_face_set(ss, vref, active_face_set)) {
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
    const PBVHVertRef v,
    eSculptExpandFalloffType falloff_type)
{
  MEM_SAFE_FREE(expand_cache->vert_falloff);
  expand_cache->falloff_type = falloff_type;

  SculptSession *ss = ob->sculpt;
  const bool has_topology_info = ELEM(BKE_pbvh_type(ss->pbvh), PBVH_FACES, PBVH_BMESH);

  switch (falloff_type) {
    case SCULPT_EXPAND_FALLOFF_GEODESIC:
      expand_cache->vert_falloff = sculpt_expand_geodesic_falloff_create(sd, ob, v);
      /*
      expand_cache->vert_falloff = has_topology_info ?
                                       sculpt_expand_geodesic_falloff_create(sd, ob, v) :
                                       sculpt_expand_spherical_falloff_create(ob, v);
                                       */
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
          sd,
          ob,
          v,
          SCULPT_EXPAND_NORMALS_FALLOFF_EDGE_SENSITIVITY,
          expand_cache->normal_falloff_blur_steps);
      break;
    case SCULPT_EXPAND_FALLOFF_SPHERICAL:
      expand_cache->vert_falloff = sculpt_expand_spherical_falloff_create(ob, v);
      break;
    case SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY:
      expand_cache->vert_falloff = sculpt_expand_boundary_topology_falloff_create(sd, ob, v);
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
    sculpt_expand_mesh_face_falloff_from_vertex_falloff(
        ss, static_cast<Mesh *>(ob->data), expand_cache);
    sculpt_expand_update_max_face_falloff_factor(ss, expand_cache);
  }
}

/**
 * Adds to the snapping Face Set `gset` all Face Sets which contain all enabled verts for the
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

  BLI_bitmap *enabled_verts = sculpt_expand_bitmap_from_enabled(ss, expand_cache);

  const int totface = ss->totfaces;
  for (int i = 0; i < totface; i++) {
    const int face_set = expand_cache->original_face_sets[i];
    BLI_gset_add(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(face_set));
  }

  for (const int i : ss->faces.index_range()) {
    bool any_disabled = false;
    for (const int vert : ss->corner_verts.slice(ss->faces[i])) {
      if (!BLI_BITMAP_TEST(enabled_verts, vert)) {
        any_disabled = true;
        break;
      }
    }
    if (any_disabled) {
      const int face_set = expand_cache->original_face_sets[i];
      BLI_gset_remove(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(face_set), nullptr);
    }
  }

  MEM_freeN(enabled_verts);
  expand_cache->snap = prev_snap_state;
  expand_cache->invert = prev_invert_state;
}

/**
 * Functions to free a #ExpandCache.
 */
static void sculpt_expand_cache_data_free(ExpandCache *expand_cache)
{
  if (expand_cache->snap_enabled_face_sets) {
    BLI_gset_free(expand_cache->snap_enabled_face_sets, nullptr);
  }
  MEM_SAFE_FREE(expand_cache->vert_falloff);
  MEM_SAFE_FREE(expand_cache->face_falloff);
  MEM_SAFE_FREE(expand_cache->original_mask);
  MEM_SAFE_FREE(expand_cache->original_face_sets);
  MEM_SAFE_FREE(expand_cache->initial_face_sets);
  MEM_SAFE_FREE(expand_cache->original_colors);
  MEM_delete<ExpandCache>(expand_cache);
}

static void sculpt_expand_cache_free(SculptSession *ss)
{
  sculpt_expand_cache_data_free(ss->expand_cache);
  /* Needs to be set to nullptr as the paint cursor relies on checking this pointer detecting if an
   * expand operation is running. */
  ss->expand_cache = nullptr;
}

/**
 * Functions to restore the original state from the #ExpandCache when canceling the operator.
 */
static void sculpt_expand_restore_face_set_data(SculptSession *ss, ExpandCache *expand_cache)
{
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);
  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_redraw(node);
  }

  for (int i = 0; i < ss->totfaces; i++) {
    PBVHFaceRef f = BKE_pbvh_index_to_face(ss->pbvh, i);

    SCULPT_face_set_set(ss, f, expand_cache->original_face_sets[i]);
  }
}

static void sculpt_expand_restore_color_data(SculptSession *ss, ExpandCache *expand_cache)
{
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

  for (PBVHNode *node : nodes) {
    PBVHVertexIter vd;
    BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
      SCULPT_vertex_color_set(ss, vd.vertex, expand_cache->original_colors[vd.index]);
    }
    BKE_pbvh_vertex_iter_end;
    BKE_pbvh_node_mark_redraw(node);
  }
}

static void sculpt_expand_restore_mask_data(SculptSession *ss, ExpandCache *expand_cache)
{
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);
  for (PBVHNode *node : nodes) {
    PBVHVertexIter vd;
    BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
      *vd.mask = expand_cache->original_mask[vd.index];
    }
    BKE_pbvh_vertex_iter_end;
    BKE_pbvh_node_mark_redraw(node);
  }
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
static void sculpt_expand_cancel(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  sculpt_expand_restore_original_state(C, ob, ss->expand_cache);

  SCULPT_undo_push_end(ob);
  sculpt_expand_cache_free(ss);
}

/* Functions to update the sculpt mesh data. */

/**
 * Callback to update mask data per PBVH node.
 */
static void sculpt_expand_mask_update_task_cb(void *__restrict userdata,
                                              const int i,
                                              const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  ExpandCache *expand_cache = ss->expand_cache;

  bool any_changed = false;

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_ALL) {
    const float initial_mask = *vd.mask;
    const bool enabled = sculpt_expand_state_get(ss, expand_cache, vd.vertex);

    if (expand_cache->check_islands &&
        !sculpt_expand_is_vert_in_active_component(ss, expand_cache, vd.vertex))
    {
      continue;
    }

    float new_mask;

    if (enabled) {
      new_mask = sculpt_expand_gradient_value_get(ss, expand_cache, vd.vertex);
    }
    else {
      new_mask = 0.0f;
    }

    if (expand_cache->preserve) {
      if (expand_cache->invert) {
        new_mask = min_ff(new_mask, expand_cache->original_mask[vd.index]);
      }
      else {
        new_mask = max_ff(new_mask, expand_cache->original_mask[vd.index]);
      }
    }

    if (new_mask == initial_mask) {
      continue;
    }

    *vd.mask = clamp_f(new_mask, 0.0f, 1.0f);
    any_changed = true;
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

  for (int f_i = 0; f_i < totface; f_i++) {
    PBVHFaceRef f = BKE_pbvh_index_to_face(ss->pbvh, f_i);
    int fset = SCULPT_face_set_get(ss, f);

    const bool enabled = sculpt_expand_face_state_get(ss, expand_cache, f);
    if (!enabled) {
      continue;
    }
    if (expand_cache->preserve) {
      SCULPT_face_set_set(ss, f, fset + expand_cache->next_face_set);
    }
    else {
      SCULPT_face_set_set(ss, f, expand_cache->next_face_set);
    }
  }

  for (PBVHNode *node : ss->expand_cache->nodes) {
    BKE_pbvh_node_mark_redraw(node);
  }
}

/**
 * Callback to update vertex colors per PBVH node.
 */
static void sculpt_expand_colors_update_task_cb(void *__restrict userdata,
                                                const int i,
                                                const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  ExpandCache *expand_cache = ss->expand_cache;

  bool any_changed = false;

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_ALL) {
    float initial_color[4];
    SCULPT_vertex_color_get(ss, vd.vertex, initial_color);

    const bool enabled = sculpt_expand_state_get(ss, expand_cache, vd.vertex);
    float fade;

    if (enabled) {
      fade = sculpt_expand_gradient_value_get(ss, expand_cache, vd.vertex);
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
                          IMB_BlendMode(expand_cache->blend_mode));

    if (equals_v4v4(initial_color, final_color)) {
      continue;
    }

    SCULPT_vertex_color_set(ss, vd.vertex, final_color);

    any_changed = true;
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

  SCULPT_vertex_random_access_ensure(ss);

  /* Face Sets are always stored as they are needed for snapping. */
  expand_cache->initial_face_sets = static_cast<int *>(
      MEM_malloc_arrayN(totface, sizeof(int), "initial face set"));
  expand_cache->original_face_sets = static_cast<int *>(
      MEM_malloc_arrayN(totface, sizeof(int), "original face set"));
  if (ss->face_sets) {
    for (int i = 0; i < totface; i++) {
      const PBVHFaceRef fref = BKE_pbvh_index_to_face(ss->pbvh, i);
      const int fset = SCULPT_face_set_get(ss, fref);

      expand_cache->initial_face_sets[i] = fset;
      expand_cache->original_face_sets[i] = fset;
    }
  }
  else {
    memset(expand_cache->initial_face_sets, SCULPT_FACE_SET_NONE, sizeof(int) * totface);
    memset(expand_cache->original_face_sets, SCULPT_FACE_SET_NONE, sizeof(int) * totface);
  }

  if (expand_cache->target == SCULPT_EXPAND_TARGET_MASK) {
    expand_cache->original_mask = static_cast<float *>(
        MEM_malloc_arrayN(totvert, sizeof(float), "initial mask"));
    for (int i = 0; i < totvert; i++) {
      expand_cache->original_mask[i] = SCULPT_vertex_mask_get(
          ss, BKE_pbvh_index_to_vertex(ss->pbvh, i));
    }
  }

  if (expand_cache->target == SCULPT_EXPAND_TARGET_COLORS) {
    expand_cache->original_colors = static_cast<float(*)[4]>(
        MEM_malloc_arrayN(totvert, sizeof(float[4]), "initial colors"));
    for (int i = 0; i < totvert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      SCULPT_vertex_color_get(ss, vertex, expand_cache->original_colors[i]);
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
    PBVHFaceRef fref = BKE_pbvh_index_to_face(ss->pbvh, i);

    if (expand_cache->original_face_sets[i] <= 0) {
      /* Do not modify hidden Face Sets, even when restoring the IDs state. */
      continue;
    }
    if (!sculpt_expand_is_face_in_active_component(ss, expand_cache, fref)) {
      continue;
    }

    SCULPT_face_set_set(ss, fref, expand_cache->initial_face_sets[i]);
  }
}

static void sculpt_expand_update_for_vertex(bContext *C, Object *ob, const PBVHVertRef vertex)
{
  SculptSession *ss = ob->sculpt;
  const int vertex_i = BKE_pbvh_vertex_to_index(ss->pbvh, vertex);

  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  ExpandCache *expand_cache = ss->expand_cache;

  /* Update the active factor in the cache. */
  if (vertex.i == SCULPT_EXPAND_VERTEX_NONE) {
    /* This means that the cursor is not over the mesh, so a valid active falloff can't be
     * determined. In this situations, don't evaluate enabled states and default all verts in
     * connected components to enabled. */
    expand_cache->active_falloff = expand_cache->max_vert_falloff;
    expand_cache->all_enabled = true;
  }
  else {
    expand_cache->active_falloff = expand_cache->vert_falloff[vertex_i];
    expand_cache->all_enabled = false;
  }

  if (expand_cache->target == SCULPT_EXPAND_TARGET_FACE_SETS) {
    /* Face sets needs to be restored their initial state on each iteration as the overwrite
     * existing data. */
    sculpt_expand_face_sets_restore(ss, expand_cache);
  }

  /* Update the mesh sculpt data. */
  SculptThreadedTaskData data{};
  data.sd = sd;
  data.ob = ob;
  data.nodes = expand_cache->nodes;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, expand_cache->nodes.size());

  switch (expand_cache->target) {
    case SCULPT_EXPAND_TARGET_MASK:
      BLI_task_parallel_range(
          0, expand_cache->nodes.size(), &data, sculpt_expand_mask_update_task_cb, &settings);
      break;
    case SCULPT_EXPAND_TARGET_FACE_SETS:
      sculpt_expand_face_sets_update(ss, expand_cache);
      break;
    case SCULPT_EXPAND_TARGET_COLORS:
      BLI_task_parallel_range(
          0, expand_cache->nodes.size(), &data, sculpt_expand_colors_update_task_cb, &settings);
      break;
  }

  sculpt_expand_flush_updates(C);
}

/**
 * Updates the #SculptSession cursor data and gets the active vertex
 * if the cursor is over the mesh.
 */
static PBVHVertRef sculpt_expand_target_vertex_update_and_get(bContext *C,
                                                              Object *ob,
                                                              const float mval[2])
{
  SculptSession *ss = ob->sculpt;
  SculptCursorGeometryInfo sgi;
  if (SCULPT_cursor_geometry_info_update(C, &sgi, mval, false, false)) {
    return SCULPT_active_vertex_get(ss);
  }

  PBVHVertRef ret = {SCULPT_EXPAND_VERTEX_NONE};
  return ret;
}

/**
 * Moves the sculpt pivot to the average point of the boundary enabled verts of the current
 * expand state. Take symmetry and active components into account.
 */
static void sculpt_expand_reposition_pivot(bContext *C, Object *ob, ExpandCache *expand_cache)
{
  SculptSession *ss = ob->sculpt;
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  const int totvert = SCULPT_vertex_count_get(ss);

  const bool initial_invert_state = expand_cache->invert;
  expand_cache->invert = false;
  BLI_bitmap *enabled_verts = sculpt_expand_bitmap_from_enabled(ss, expand_cache);

  /* For boundary topology, position the pivot using only the boundary of the enabled verts,
   * without taking mesh boundary into account. This allows to create deformations like bending the
   * mesh from the boundary of the mask that was just created. */
  const bool use_mesh_boundary = expand_cache->falloff_type !=
                                 SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY;

  BLI_bitmap *boundary_verts = sculpt_expand_boundary_from_enabled(
      ss, enabled_verts, use_mesh_boundary);

  /* Ignore invert state, as this is the expected behavior in most cases and mask are created in
   * inverted state by default. */
  expand_cache->invert = initial_invert_state;

  int total = 0;
  float avg[3] = {0.0f};

  const float *expand_init_co = SCULPT_vertex_co_get(ss, expand_cache->initial_active_vertex);

  for (int i = 0; i < totvert; i++) {
    if (!BLI_BITMAP_TEST(boundary_verts, i)) {
      continue;
    }

    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    if (!sculpt_expand_is_vert_in_active_component(ss, expand_cache, vertex)) {
      continue;
    }

    const float *vertex_co = SCULPT_vertex_co_get(ss, vertex);

    if (!SCULPT_check_vertex_pivot_symmetry(vertex_co, expand_init_co, symm)) {
      continue;
    }

    add_v3_v3(avg, vertex_co);
    total++;
  }

  MEM_freeN(enabled_verts);
  MEM_freeN(boundary_verts);

  if (total > 0) {
    mul_v3_v3fl(ss->pivot_pos, avg, 1.0f / total);
  }

  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
}

static void sculpt_expand_finish(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  SCULPT_undo_push_end(ob);

  /* Tag all nodes to redraw to avoid artifacts after the fast partial updates. */
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);
  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_update_mask(node);
  }

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
  ED_workspace_status_text(C, nullptr);
}

/**
 * Finds and stores in the #ExpandCache the sculpt connected component index for each symmetry pass
 * needed for expand.
 */
static void sculpt_expand_find_active_connected_components_from_vert(
    Object *ob, ExpandCache *expand_cache, const PBVHVertRef initial_vertex)
{
  SculptSession *ss = ob->sculpt;
  for (int i = 0; i < EXPAND_SYMM_AREAS; i++) {
    expand_cache->active_connected_islands[i] = EXPAND_ACTIVE_COMPONENT_NONE;
  }

  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }

    const PBVHVertRef symm_vertex = sculpt_expand_get_vertex_index_for_symmetry_pass(
        ob, symm_it, initial_vertex);

    expand_cache->active_connected_islands[int(symm_it)] = SCULPT_vertex_island_get(ss,
                                                                                    symm_vertex);
  }
}

/**
 * Stores the active vertex, Face Set and mouse coordinates in the #ExpandCache based on the
 * current cursor position.
 */
static void sculpt_expand_set_initial_components_for_mouse(bContext *C,
                                                           Object *ob,
                                                           ExpandCache *expand_cache,
                                                           const float mval[2])
{
  SculptSession *ss = ob->sculpt;

  PBVHVertRef initial_vertex = sculpt_expand_target_vertex_update_and_get(C, ob, mval);

  if (initial_vertex.i == SCULPT_EXPAND_VERTEX_NONE) {
    /* Cursor not over the mesh, for creating valid initial falloffs, fallback to the last active
     * vertex in the sculpt session. */
    initial_vertex = SCULPT_active_vertex_get(ss);
  }

  int initial_vertex_i = BKE_pbvh_vertex_to_index(ss->pbvh, initial_vertex);

  copy_v2_v2(ss->expand_cache->initial_mouse, mval);
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
      expand_cache->next_face_set = ED_sculpt_face_sets_find_next_available_id(
          static_cast<Mesh *>(ob->data));
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

  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  float move_disp[2];
  sub_v2_v2v2(move_disp, mval_fl, expand_cache->initial_mouse_move);

  float new_mval[2];
  add_v2_v2v2(new_mval, move_disp, expand_cache->original_mouse_move);

  sculpt_expand_set_initial_components_for_mouse(C, ob, expand_cache, new_mval);
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
  SCULPT_topology_islands_ensure(ob);
  SCULPT_vertex_random_access_ensure(ss);
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
    case PBVH_BMESH:
    case PBVH_FACES:
      return expand_cache->original_face_sets[BKE_pbvh_face_to_index(ss->pbvh, ss->active_face)];
    case PBVH_GRIDS: {
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg,
                                                               ss->active_grid_index);
      return expand_cache->original_face_sets[face_index];
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
  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  const PBVHVertRef target_expand_vertex = sculpt_expand_target_vertex_update_and_get(
      C, ob, mval_fl);

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
      case SCULPT_EXPAND_MODAL_SNAP_ENABLE: {
        expand_cache->snap = true;
        if (!expand_cache->snap_enabled_face_sets) {
          expand_cache->snap_enabled_face_sets = BLI_gset_int_new("snap face sets");
        }
        sculpt_expand_snap_initialize_from_enabled(ss, expand_cache);
      } break;
      case SCULPT_EXPAND_MODAL_SNAP_DISABLE: {
        expand_cache->snap = false;
        if (expand_cache->snap_enabled_face_sets) {
          BLI_gset_free(expand_cache->snap_enabled_face_sets, NULL);
          expand_cache->snap_enabled_face_sets = NULL;
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
        expand_cache->move = true;
        expand_cache->move_original_falloff_type = expand_cache->falloff_type;
        copy_v2_v2(expand_cache->initial_mouse_move, mval_fl);
        copy_v2_v2(expand_cache->original_mouse_move, expand_cache->initial_mouse);
        if (expand_cache->falloff_type == SCULPT_EXPAND_FALLOFF_GEODESIC &&
            SCULPT_vertex_count_get(ss) > expand_cache->max_geodesic_move_preview)
        {
          /* Set to spherical falloff for preview in high poly meshes as it is the fastest one.
           * In most cases it should match closely the preview from geodesic. */
          expand_cache->move_preview_falloff_type = SCULPT_EXPAND_FALLOFF_SPHERICAL;
        }
        else {
          expand_cache->move_preview_falloff_type = expand_cache->falloff_type;
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
        sculpt_expand_check_topology_islands(ob, SCULPT_EXPAND_FALLOFF_GEODESIC);

        sculpt_expand_falloff_factors_from_vertex_and_symm_create(
            expand_cache,
            sd,
            ob,
            expand_cache->initial_active_vertex,
            SCULPT_EXPAND_FALLOFF_GEODESIC);
        break;
      }
      case SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY: {
        sculpt_expand_check_topology_islands(ob, SCULPT_EXPAND_FALLOFF_TOPOLOGY);

        sculpt_expand_falloff_factors_from_vertex_and_symm_create(
            expand_cache,
            sd,
            ob,
            expand_cache->initial_active_vertex,
            SCULPT_EXPAND_FALLOFF_TOPOLOGY);
        break;
      }
      case SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY_DIAGONALS: {
        sculpt_expand_check_topology_islands(ob, SCULPT_EXPAND_FALLOFF_TOPOLOGY_DIAGONALS);

        sculpt_expand_falloff_factors_from_vertex_and_symm_create(
            expand_cache,
            sd,
            ob,
            expand_cache->initial_active_vertex,
            SCULPT_EXPAND_FALLOFF_TOPOLOGY_DIAGONALS);
        break;
      }
      case SCULPT_EXPAND_MODAL_FALLOFF_SPHERICAL: {
        expand_cache->check_islands = false;
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
          const MTex *mask_tex = BKE_brush_mask_texture_get(expand_cache->brush, OB_MODE_SCULPT);
          if (mask_tex->tex == nullptr) {
            BKE_report(op->reports,
                       RPT_WARNING,
                       "Active brush does not contain any texture to distort the expand boundary");
            break;
          }
          if (mask_tex->brush_map_mode != MTEX_MAP_MODE_3D) {
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
    /* The key may exist, in that case this does nothing. */
    BLI_gset_add(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(active_face_set_id));
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
static void sculpt_expand_delete_face_set_id_bmesh(int *r_face_sets,
                                                   SculptSession *ss,
                                                   ExpandCache *expand_cache,
                                                   const int delete_id)
{
  BMIter iter;
  BMFace *f;
  int i = 0;
  const int totface = ss->bm->totface;

  /* Check that all the face sets IDs in the mesh are not equal to `delete_id`
   * before attempting to delete it. */
  bool all_same_id = true;

  BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
    PBVHFaceRef fref = {(intptr_t)f};
    i++;

    if (!sculpt_expand_is_face_in_active_component(ss, expand_cache, fref)) {
      continue;
    }

    if (r_face_sets[i] != delete_id) {
      all_same_id = false;
      break;
    }
  }

  if (all_same_id) {
    return;
  }

  BLI_LINKSTACK_DECLARE(queue, BMFace *);
  BLI_LINKSTACK_DECLARE(queue_next, BMFace *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  for (int i = 0; i < totface; i++) {
    PBVHFaceRef fref = BKE_pbvh_index_to_face(ss->pbvh, i);

    if (r_face_sets[i] == delete_id) {
      BLI_LINKSTACK_PUSH(queue, (BMFace *)(fref.i));
    }
  }

  while (BLI_LINKSTACK_SIZE(queue)) {
    bool any_updated = false;

    while (BLI_LINKSTACK_SIZE(queue)) {
      const PBVHFaceRef f = {(intptr_t)(BLI_LINKSTACK_POP(queue))};
      BMFace *bf = (BMFace *)f.i;
      const int f_index = BM_elem_index_get(bf);

      int other_id = delete_id;
      BMLoop *l = bf->l_first;
      do {
        BMLoop *l2 = l->radial_next;
        do {
          const int neighbor_face_index = BM_elem_index_get(l2->f);

          if (expand_cache->original_face_sets[neighbor_face_index] <= 0) {
            /* Skip picking IDs from hidden Face Sets. */
            continue;
          }

          if (r_face_sets[neighbor_face_index] != delete_id) {
            other_id = r_face_sets[neighbor_face_index];
          }

          l2 = l2->radial_next;
        } while (l2 != l);

        l = l->next;
      } while (l != bf->l_first);

      if (other_id != delete_id) {
        any_updated = true;
        r_face_sets[f_index] = other_id;
      }
      else {
        BLI_LINKSTACK_PUSH(queue_next, bf);
      }
    }

    if (!any_updated) {
      /* No Face Sets where updated in this iteration, which means that no more content to keep
       * filling the faces of the deleted Face Set was found. Break to avoid entering an infinite
       * loop trying to search for those faces again. */
      break;
    }

    BLI_LINKSTACK_SWAP(queue, queue_next);
  }

  BLI_LINKSTACK_FREE(queue);
  BLI_LINKSTACK_FREE(queue_next);

  /* Ensure that the visibility state of the modified Face Sets is the same as the original ones.
   */
  for (int i = 0; i < totface; i++) {
    if (expand_cache->original_face_sets[i] >= 0) {
      r_face_sets[i] = abs(r_face_sets[i]);
    }
    else {
      r_face_sets[i] = -abs(r_face_sets[i]);
    }
  }
}

/**
 * Deletes the `delete_id` Face Set ID from the mesh Face Sets
 * and stores the result in `r_face_set`.
 * The faces that were using the `delete_id` Face Set are filled
 * using the content from their neighbors.
 */
static void sculpt_expand_delete_face_set_id(int *r_face_sets,
                                             SculptSession *ss,
                                             ExpandCache *expand_cache,
                                             Mesh *mesh,
                                             const int delete_id)
{
  if (ss->bm) {
    sculpt_expand_delete_face_set_id_bmesh(r_face_sets, ss, expand_cache, delete_id);
    return;
  }

  const int totface = ss->totfaces;
  const blender::GroupedSpan<int> pmap = ss->pmap;
  const blender::OffsetIndices<int> faces = mesh->faces();
  const blender::Span<int> corner_verts = mesh->corner_verts();

  /* Check that all the face sets IDs in the mesh are not equal to `delete_id`
   * before attempting to delete it. */
  bool all_same_id = true;
  for (int i = 0; i < totface; i++) {
    if (!sculpt_expand_is_face_in_active_component(
            ss, expand_cache, BKE_pbvh_index_to_face(ss->pbvh, i)))
    {
      continue;
    }
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
    bool any_updated = false;
    while (BLI_LINKSTACK_SIZE(queue)) {
      const int f_index = POINTER_AS_INT(BLI_LINKSTACK_POP(queue));
      int other_id = delete_id;
      for (const int vert : corner_verts.slice(faces[f_index])) {
        for (const int neighbor_face_index : pmap[vert]) {
          if (expand_cache->original_face_sets[neighbor_face_index] <= 0) {
            /* Skip picking IDs from hidden Face Sets. */
            continue;
          }
          if (r_face_sets[neighbor_face_index] != delete_id) {
            other_id = r_face_sets[neighbor_face_index];
          }
        }
      }

      if (other_id != delete_id) {
        any_updated = true;
        r_face_sets[f_index] = other_id;
      }
      else {
        BLI_LINKSTACK_PUSH(queue_next, POINTER_FROM_INT(f_index));
      }
    }
    if (!any_updated) {
      /* No Face Sets where updated in this iteration, which means that no more content to keep
       * filling the faces of the deleted Face Set was found. Break to avoid entering an infinite
       * loop trying to search for those faces again. */
      break;
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
  expand_cache->normal_falloff_blur_steps = RNA_int_get(op->ptr, "normal_falloff_smooth");
  expand_cache->invert = RNA_boolean_get(op->ptr, "invert");
  expand_cache->preserve_flip_inverse = RNA_boolean_get(op->ptr, "use_preserve_flip_inverse");
  expand_cache->preserve = RNA_boolean_get(op->ptr, "use_mask_preserve");
  expand_cache->auto_mask = RNA_boolean_get(op->ptr, "use_auto_mask");
  expand_cache->falloff_gradient = RNA_boolean_get(op->ptr, "use_falloff_gradient");
  expand_cache->target = eSculptExpandTargetType(RNA_enum_get(op->ptr, "target"));
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
  IMB_colormanagement_srgb_to_scene_linear_v3(expand_cache->fill_color, expand_cache->fill_color);

  expand_cache->scene = CTX_data_scene(C);
  expand_cache->texture_distortion_strength = 0.0f;
  expand_cache->blend_mode = expand_cache->brush->blend;
}

/**
 * Does the undo sculpt push for the affected target data of the #ExpandCache.
 */
static void sculpt_expand_undo_push(Object *ob, ExpandCache *expand_cache)
{
  SculptSession *ss = ob->sculpt;
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

  switch (expand_cache->target) {
    case SCULPT_EXPAND_TARGET_MASK:
      for (PBVHNode *node : nodes) {
        SCULPT_undo_push_node(ob, node, SCULPT_UNDO_MASK);
      }
      break;
    case SCULPT_EXPAND_TARGET_FACE_SETS:
      for (PBVHNode *node : nodes) {
        SCULPT_undo_push_node(ob, node, SCULPT_UNDO_FACE_SETS);
      }
      break;
    case SCULPT_EXPAND_TARGET_COLORS:
      for (PBVHNode *node : nodes) {
        SCULPT_undo_push_node(ob, node, SCULPT_UNDO_COLOR);
      }
      break;
  }
}

static int sculpt_expand_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_face_random_access_ensure(ss);
  SCULPT_stroke_id_next(ob);

  /* Create and configure the Expand Cache. */
  ss->expand_cache = MEM_new<ExpandCache>(__func__);
  sculpt_expand_cache_initial_config_set(C, op, ss->expand_cache);

  /* Update object. */
  const bool needs_colors = ss->expand_cache->target == SCULPT_EXPAND_TARGET_COLORS;

  if (needs_colors) {
    /* CTX_data_ensure_evaluated_depsgraph should be used at the end to include the updates of
     * earlier steps modifying the data. */
    BKE_sculpt_color_layer_create_if_needed(ob);
    depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  }

  if (ss->expand_cache->target == SCULPT_EXPAND_TARGET_MASK) {
    MultiresModifierData *mmd = BKE_sculpt_multires_active(ss->scene, ob);
    BKE_sculpt_mask_layers_ensure(depsgraph, CTX_data_main(C), ob, mmd);

    if (RNA_boolean_get(op->ptr, "use_auto_mask")) {
      int verts_num = SCULPT_vertex_count_get(ss);
      bool ok = true;

      for (int i = 0; i < verts_num; i++) {
        PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

        if (SCULPT_vertex_mask_get(ss, vertex) != 0.0f) {
          ok = false;
          break;
        }
      }

      if (ok) {
        /* TODO: implement SCULPT_vertex_mask_set and use it here. */
        Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);
        for (PBVHNode *node : nodes) {
          PBVHVertexIter vd;

          BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
            *vd.mask = 1.0f;
          }
          BKE_pbvh_vertex_iter_end;

          BKE_pbvh_node_mark_update_mask(node);
        }
      }
    }
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, needs_colors);

  /* Do nothing when the mesh has 0 verts. */
  const int totvert = SCULPT_vertex_count_get(ss);
  if (totvert == 0) {
    sculpt_expand_cache_free(ss);
    return OPERATOR_CANCELLED;
  }

  Mesh *mesh = static_cast<Mesh *>(ob->data);
  if (ss->expand_cache->target == SCULPT_EXPAND_TARGET_FACE_SETS) {
    ss->face_sets = BKE_sculpt_face_sets_ensure(ob);
  }

  if (ss->expand_cache->target == SCULPT_EXPAND_TARGET_MASK) {
    MultiresModifierData *mmd = BKE_sculpt_multires_active(ss->scene, ob);
    BKE_sculpt_mask_layers_ensure(depsgraph, CTX_data_main(C), ob, mmd);
  }

  sculpt_expand_ensure_sculptsession_data(ob);

  /* Initialize undo. */
  SCULPT_undo_push_begin(ob, op);
  sculpt_expand_undo_push(ob, ss->expand_cache);

  /* Set the initial element for expand from the event position. */
  const float mouse[2] = {float(event->mval[0]), float(event->mval[1])};
  sculpt_expand_set_initial_components_for_mouse(C, ob, ss->expand_cache, mouse);

  /* Cache PBVH nodes. */
  ss->expand_cache->nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

  /* Store initial state. */
  sculpt_expand_original_state_store(ob, ss->expand_cache);

  if (ss->expand_cache->modify_active_face_set) {
    sculpt_expand_delete_face_set_id(ss->expand_cache->initial_face_sets,
                                     ss,
                                     ss->expand_cache,
                                     mesh,
                                     ss->expand_cache->next_face_set);
  }

  /* Initialize the falloff. */
  eSculptExpandFalloffType falloff_type = eSculptExpandFalloffType(
      RNA_enum_get(op->ptr, "falloff_type"));

  /* When starting from a boundary vertex, set the initial falloff to boundary. */
  if (SCULPT_vertex_is_boundary(ss, ss->expand_cache->initial_active_vertex, SCULPT_BOUNDARY_MESH))
  {
    falloff_type = SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY;
  }

  sculpt_expand_falloff_factors_from_vertex_and_symm_create(
      ss->expand_cache, sd, ob, ss->expand_cache->initial_active_vertex, falloff_type);

  sculpt_expand_check_topology_islands(ob, falloff_type);

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
      {SCULPT_EXPAND_MODAL_SNAP_ENABLE, "SNAP_ENABLE", 0, "Snap expand to Face Sets", ""},
      {SCULPT_EXPAND_MODAL_SNAP_DISABLE,
       "SNAP_DISABLE",
       0,
       "Disable Snap expand to Face Sets",
       ""},
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const char *name = "Sculpt Expand Modal";
  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, name);

  /* This function is called for each space-type, only needs to add map once. */
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

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

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
      {0, nullptr, 0, nullptr, nullptr},
  };

  static EnumPropertyItem prop_sculpt_expand_target_type_items[] = {
      {SCULPT_EXPAND_TARGET_MASK, "MASK", 0, "Mask", ""},
      {SCULPT_EXPAND_TARGET_FACE_SETS, "FACE_SETS", 0, "Face Sets", ""},
      {SCULPT_EXPAND_TARGET_COLORS, "COLOR", 0, "Color", ""},
      {0, nullptr, 0, nullptr, nullptr},
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

  ot->prop = RNA_def_boolean(ot->srna,
                             "use_preserve_flip_inverse",
                             false,
                             "Preserve Inverted",
                             "Flip preserve mode in inverse mode");

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
                         "Maximum number of verts in the mesh for using geodesic falloff when "
                         "moving the origin of expand. If the total number of verts is greater "
                         "than this value, the falloff will be set to spherical when moving",
                         0,
                         1000000);
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_auto_mask",
                             false,
                             "Auto Create",
                             "Fill in mask if nothing is already masked");
  ot->prop = RNA_def_int(ot->srna,
                         "normal_falloff_smooth",
                         2,
                         0,
                         10,
                         "Normal Smooth",
                         "Blurring steps for normal falloff",
                         0,
                         10);
}
