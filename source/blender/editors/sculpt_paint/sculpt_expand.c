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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
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
#include "BKE_scene.h"

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

#define SCULPT_EXPAND_VERTEX_NONE -1

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
  SCULPT_EXPAND_MODAL_TEXTURE_DISTORSION_INCREASE,
  SCULPT_EXPAND_MODAL_TEXTURE_DISTORSION_DECREASE,
};

static EnumPropertyItem prop_sculpt_expand_falloff_type_items[] = {
    {SCULPT_EXPAND_FALLOFF_GEODESIC, "GEODESIC", 0, "Geodesic", ""},
    {SCULPT_EXPAND_FALLOFF_TOPOLOGY, "TOPOLOGY", 0, "Topology", ""},
    {SCULPT_EXPAND_FALLOFF_TOPOLOGY_DIAGONALS, "TOPOLOGY_DIAGONALS", 0, "Topology Diagonals", ""},
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

#define SCULPT_EXPAND_TEXTURE_DISTORSION_STEP 0.01f
#define SCULPT_EXPAND_LOOP_THRESHOLD 0.00001f

static bool sculpt_expand_is_vert_in_active_compoment(SculptSession *ss,
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

static bool sculpt_expand_is_face_in_active_component(SculptSession *ss,
                                                      ExpandCache *expand_cache,
                                                      const int f)
{
  const MLoop *loop = &ss->mloop[ss->mpoly[f].loopstart];
  return sculpt_expand_is_vert_in_active_compoment(ss, expand_cache, loop->v);
}

static float sculpt_expand_falloff_value_vertex_get(SculptSession *ss,
                                                    ExpandCache *expand_cache,
                                                    const int i)
{
  if (expand_cache->texture_distorsion_strength == 0.0f) {
    return expand_cache->falloff_factor[i];
  }

  if (!expand_cache->brush->mtex.tex) {
    return expand_cache->falloff_factor[i];
  }

  float rgba[4];
  const float *vertex_co = SCULPT_vertex_co_get(ss, i);
  const float avg = BKE_brush_sample_tex_3d(
      expand_cache->scene, expand_cache->brush, vertex_co, rgba, 0, ss->tex_pool);

  const float distorsion = (avg - 0.5f) * expand_cache->texture_distorsion_strength *
                           expand_cache->max_falloff_factor;
  return expand_cache->falloff_factor[i] + distorsion;
}

static float sculpt_expand_max_vertex_falloff_factor_get(ExpandCache *expand_cache)
{
  if (expand_cache->texture_distorsion_strength == 0.0f) {
    return expand_cache->max_falloff_factor;
  }

  if (!expand_cache->brush->mtex.tex) {
    return expand_cache->max_falloff_factor;
  }

  return expand_cache->max_falloff_factor +
         (0.5f * expand_cache->texture_distorsion_strength * expand_cache->max_falloff_factor);
}

static bool sculpt_expand_state_get(SculptSession *ss, ExpandCache *expand_cache, const int i)
{
  if (!SCULPT_vertex_visible_get(ss, i)) {
    return false;
  }

  if (!sculpt_expand_is_vert_in_active_compoment(ss, expand_cache, i)) {
    return false;
  }

  if (expand_cache->all_enabled) {
    return true;
  }

  bool enabled = false;

  if (expand_cache->snap) {
    const int face_set = SCULPT_vertex_face_set_get(ss, i);
    enabled = BLI_gset_haskey(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(face_set));
  }
  else {
    const float max_falloff_factor = sculpt_expand_max_vertex_falloff_factor_get(expand_cache);
    const float loop_len = (max_falloff_factor / expand_cache->loop_count) +
                           SCULPT_EXPAND_LOOP_THRESHOLD;

    const float vertex_falloff_factor = sculpt_expand_falloff_value_vertex_get(
        ss, expand_cache, i);
    const float active_factor = fmod(expand_cache->active_factor, loop_len);
    const float falloff_factor = fmod(vertex_falloff_factor, loop_len);

    enabled = falloff_factor < active_factor;
  }

  if (expand_cache->invert) {
    enabled = !enabled;
  }
  return enabled;
}

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
    const int face_set = ss->face_sets[f];
    enabled = BLI_gset_haskey(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(face_set));
  }
  else {
    const float loop_len = (expand_cache->max_face_falloff_factor / expand_cache->loop_count) +
                           SCULPT_EXPAND_LOOP_THRESHOLD;

    const float active_factor = fmod(expand_cache->active_factor, loop_len);
    const float falloff_factor = fmod(expand_cache->face_falloff_factor[f], loop_len);
    enabled = falloff_factor < active_factor;
  }

  if (expand_cache->falloff_factor_type == SCULPT_EXPAND_FALLOFF_ACTIVE_FACE_SET) {
    if (ss->face_sets[f] == expand_cache->initial_active_face_set) {
      enabled = false;
    }
  }

  if (expand_cache->invert) {
    enabled = !enabled;
  }

  return enabled;
}

static float sculpt_expand_gradient_falloff_get(SculptSession *ss,
                                                ExpandCache *expand_cache,
                                                const int i)
{
  if (!expand_cache->falloff_gradient) {
    return 1.0f;
  }

  const float max_falloff_factor = sculpt_expand_max_vertex_falloff_factor_get(expand_cache);
  const float loop_len = (max_falloff_factor / expand_cache->loop_count) +
                         SCULPT_EXPAND_LOOP_THRESHOLD;

  const float vertex_falloff_factor = sculpt_expand_falloff_value_vertex_get(ss, expand_cache, i);
  const float active_factor = fmod(expand_cache->active_factor, loop_len);
  const float falloff_factor = fmod(vertex_falloff_factor, loop_len);

  float linear_falloff;

  if (expand_cache->invert) {
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

static float *sculpt_expand_geodesic_falloff_create(Sculpt *sd, Object *ob, const int vertex)
{
  return SCULPT_geodesic_from_vertex_and_symm(sd, ob, vertex, FLT_MAX);
}

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

static float *sculpt_expand_topology_falloff_create(Sculpt *sd, Object *ob, const int vertex)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);
  float *dists = MEM_calloc_arrayN(sizeof(float), totvert, "topology dist");

  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  SCULPT_floodfill_add_initial_with_symmetry(sd, ob, ss, &flood, vertex, FLT_MAX);

  ExpandFloodFillData fdata;
  fdata.dists = dists;

  SCULPT_floodfill_execute(ss, &flood, expand_topology_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);

  return dists;
}

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
                                                  const int vertex,
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
  SCULPT_floodfill_add_initial_with_symmetry(sd, ob, ss, &flood, vertex, FLT_MAX);

  ExpandFloodFillData fdata;
  fdata.dists = dists;
  fdata.edge_factor = edge_factor;
  fdata.edge_sensitivity = edge_sensitivity;
  SCULPT_vertex_normal_get(ss, vertex, fdata.original_normal);

  SCULPT_floodfill_execute(ss, &flood, mask_expand_normal_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);

  for (int i = 0; i < totvert; i++) {
    dists[i] = FLT_MAX;
  }

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

static float *sculpt_expand_spherical_falloff_create(Sculpt *sd, Object *ob, const int vertex)
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
    int v = SCULPT_EXPAND_VERTEX_NONE;
    if (symm_it == 0) {
      v = vertex;
    }
    else {
      float location[3];
      flip_v3_v3(location, SCULPT_vertex_co_get(ss, vertex), symm_it);
      v = SCULPT_nearest_vertex_get(sd, ob, location, FLT_MAX, false);
    }
    if (v != -1) {
      const float *co = SCULPT_vertex_co_get(ss, v);
      for (int i = 0; i < totvert; i++) {
        dists[i] = min_ff(dists[i], len_v3v3(co, SCULPT_vertex_co_get(ss, i)));
      }
    }
  }

  return dists;
}

static float *sculpt_expand_boundary_topology_falloff_create(Sculpt *sd,
                                                             Object *ob,
                                                             const int vertex)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);
  float *dists = MEM_calloc_arrayN(sizeof(float), totvert, "spherical dist");
  BLI_bitmap *visited_vertices = BLI_BITMAP_NEW(totvert, "visited vertices");
  GSQueue *queue = BLI_gsqueue_new(sizeof(int));
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }
    int v = SCULPT_EXPAND_VERTEX_NONE;
    if (symm_it == 0) {
      v = vertex;
    }
    else {
      float location[3];
      flip_v3_v3(location, SCULPT_vertex_co_get(ss, vertex), symm_it);
      v = SCULPT_nearest_vertex_get(sd, ob, location, FLT_MAX, false);
    }

    SculptBoundary *boundary = SCULPT_boundary_data_init(ob, NULL, v, FLT_MAX);
    for (int i = 0; i < boundary->num_vertices; i++) {
      BLI_gsqueue_push(queue, &boundary->vertices[i]);
      BLI_BITMAP_ENABLE(visited_vertices, boundary->vertices[i]);
    }
    SCULPT_boundary_data_free(boundary);
  }

  if (BLI_gsqueue_is_empty(queue)) {
    return dists;
  }

  while (!BLI_gsqueue_is_empty(queue)) {
    int v;
    BLI_gsqueue_pop(queue, &v);

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, v, ni) {
      if (BLI_BITMAP_TEST(visited_vertices, ni.index)) {
        continue;
      }
      dists[ni.index] = dists[v] + 1.0f;
      BLI_BITMAP_ENABLE(visited_vertices, ni.index);
      BLI_gsqueue_push(queue, &ni.index);
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  }

  for (int i = 0; i < totvert; i++) {
    if (BLI_BITMAP_TEST(visited_vertices, i)) {
      continue;
    }
    dists[i] = FLT_MAX;
  }

  BLI_gsqueue_free(queue);
  MEM_freeN(visited_vertices);
  return dists;
}

static float *sculpt_expand_diagonals_falloff_create(Sculpt *sd, Object *ob, const int vertex)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);
  float *dists = MEM_calloc_arrayN(sizeof(float), totvert, "spherical dist");

  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    return dists;
  }

  BLI_bitmap *visited_vertices = BLI_BITMAP_NEW(totvert, "visited vertices");
  GSQueue *queue = BLI_gsqueue_new(sizeof(int));
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }
    int v = SCULPT_EXPAND_VERTEX_NONE;
    if (symm_it == 0) {
      v = vertex;
    }
    else {
      float location[3];
      flip_v3_v3(location, SCULPT_vertex_co_get(ss, vertex), symm_it);
      v = SCULPT_nearest_vertex_get(sd, ob, location, FLT_MAX, false);
    }

    BLI_gsqueue_push(queue, &v);
    BLI_BITMAP_ENABLE(visited_vertices, v);
  }

  if (BLI_gsqueue_is_empty(queue)) {
    return dists;
  }

  Mesh *mesh = ob->data;
  while (!BLI_gsqueue_is_empty(queue)) {
    int v;
    BLI_gsqueue_pop(queue, &v);
    for (int j = 0; j < ss->pmap[v].count; j++) {
      MPoly *p = &ss->mpoly[ss->pmap[v].indices[j]];
      for (int l = 0; l < p->totloop; l++) {
        const int neighbor_v = mesh->mloop[p->loopstart + l].v;
        if (BLI_BITMAP_TEST(visited_vertices, neighbor_v)) {
          continue;
        }
        dists[neighbor_v] = dists[v] + 1.0f;
        BLI_BITMAP_ENABLE(visited_vertices, neighbor_v);
        BLI_gsqueue_push(queue, &neighbor_v);
      }
    }
  }

  for (int i = 0; i < totvert; i++) {
    if (BLI_BITMAP_TEST(visited_vertices, i)) {
      continue;
    }
    dists[i] = FLT_MAX;
  }

  BLI_gsqueue_free(queue);
  MEM_freeN(visited_vertices);
  return dists;
}

static void sculpt_expand_update_max_falloff_factor(SculptSession *ss, ExpandCache *expand_cache)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  expand_cache->max_falloff_factor = -FLT_MAX;
  for (int i = 0; i < totvert; i++) {
    if (expand_cache->falloff_factor[i] == FLT_MAX) {
      continue;
    }
    expand_cache->max_falloff_factor = max_ff(expand_cache->max_falloff_factor,
                                              expand_cache->falloff_factor[i]);
  }
}

static void sculpt_expand_update_max_face_falloff_factor(SculptSession *ss,
                                                         ExpandCache *expand_cache)
{
  const int totface = ss->totfaces;
  expand_cache->max_face_falloff_factor = -FLT_MAX;
  for (int i = 0; i < totface; i++) {
    if (expand_cache->face_falloff_factor[i] == FLT_MAX) {
      continue;
    }

    if (!sculpt_expand_is_face_in_active_component(ss, expand_cache, i)) {
      continue;
    }

    expand_cache->max_face_falloff_factor = max_ff(expand_cache->max_face_falloff_factor,
                                                   expand_cache->face_falloff_factor[i]);
  }
}

static void sculpt_expand_mesh_face_falloff_from_grids_falloff(SculptSession *ss,
                                                               Mesh *mesh,
                                                               ExpandCache *expand_cache)
{
  if (expand_cache->face_falloff_factor) {
    MEM_freeN(expand_cache->face_falloff_factor);
  }
  expand_cache->face_falloff_factor = MEM_malloc_arrayN(
      mesh->totpoly, sizeof(float), "face falloff factors");

  const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);

  for (int p = 0; p < mesh->totpoly; p++) {
    MPoly *poly = &mesh->mpoly[p];
    float accum = 0.0f;
    for (int l = 0; l < poly->totloop; l++) {
      const int grid_loop_index = (poly->loopstart + l) * key->grid_area;
      for (int g = 0; g < key->grid_area; g++) {
        accum += expand_cache->falloff_factor[grid_loop_index + g];
      }
    }
    expand_cache->face_falloff_factor[p] = accum / (poly->totloop * key->grid_area);
  }
}

static void sculpt_expand_mesh_face_falloff_from_vertex_falloff(Mesh *mesh,
                                                                ExpandCache *expand_cache)
{
  if (expand_cache->face_falloff_factor) {
    MEM_freeN(expand_cache->face_falloff_factor);
  }
  expand_cache->face_falloff_factor = MEM_malloc_arrayN(
      mesh->totpoly, sizeof(float), "face falloff factors");

  for (int p = 0; p < mesh->totpoly; p++) {
    MPoly *poly = &mesh->mpoly[p];
    float accum = 0.0f;
    for (int l = 0; l < poly->totloop; l++) {
      MLoop *loop = &mesh->mloop[l + poly->loopstart];
      accum += expand_cache->falloff_factor[loop->v];
    }
    expand_cache->face_falloff_factor[p] = accum / poly->totloop;
  }
}

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

static void sculpt_expand_geodesics_from_state_boundary(Object *ob,
                                                        ExpandCache *expand_cache,
                                                        BLI_bitmap *enabled_vertices)
{
  SculptSession *ss = ob->sculpt;
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

  MEM_SAFE_FREE(expand_cache->falloff_factor);
  MEM_SAFE_FREE(expand_cache->face_falloff_factor);

  expand_cache->falloff_factor = SCULPT_geodesic_distances_create(ob, initial_vertices, FLT_MAX);
  BLI_gset_free(initial_vertices, NULL);
}

static void sculpt_expand_topology_from_state_boundary(Object *ob,
                                                       ExpandCache *expand_cache,
                                                       BLI_bitmap *enabled_vertices)
{
  MEM_SAFE_FREE(expand_cache->falloff_factor);
  MEM_SAFE_FREE(expand_cache->face_falloff_factor);

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

  expand_cache->falloff_factor = dists;
}

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

  sculpt_expand_geodesics_from_state_boundary(ob, expand_cache, enabled_vertices);

  MEM_freeN(enabled_vertices);

  if (internal_falloff) {
    for (int i = 0; i < totvert; i++) {
      if (!(SCULPT_vertex_has_face_set(ss, i, active_face_set) &&
            SCULPT_vertex_has_unique_face_set(ss, i))) {
        continue;
      }
      expand_cache->falloff_factor[i] *= -1.0f;
    }

    float min_factor = FLT_MAX;
    for (int i = 0; i < totvert; i++) {
      min_factor = min_ff(expand_cache->falloff_factor[i], min_factor);
    }

    const float increase_factor = fabsf(min_factor);
    for (int i = 0; i < totvert; i++) {
      expand_cache->falloff_factor[i] += increase_factor;
    }
  }
  else {
    for (int i = 0; i < totvert; i++) {
      if (!SCULPT_vertex_has_face_set(ss, i, active_face_set)) {
        continue;
      }
      expand_cache->falloff_factor[i] = 0.0f;
    }
  }
}

static void sculpt_expand_snap_initialize_from_enabled(SculptSession *ss,
                                                       ExpandCache *expand_cache)
{
  const bool prev_snap_state = expand_cache->snap;
  const bool prev_invert_state = expand_cache->invert;
  expand_cache->snap = false;
  expand_cache->invert = false;

  BLI_bitmap *enabled_vertices = sculpt_expand_bitmap_from_enabled(ss, expand_cache);

  const int totface = ss->totfaces;
  for (int i = 0; i < totface; i++) {
    const int face_set = expand_cache->initial_face_sets[i];
    BLI_gset_add(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(face_set));
  }

  for (int p = 0; p < totface; p++) {
    MPoly *poly = &ss->mpoly[p];
    bool any_disabled = false;
    for (int l = 0; l < poly->totloop; l++) {
      MLoop *loop = &ss->mloop[l + poly->loopstart];
      if (!BLI_BITMAP_TEST(enabled_vertices, loop->v)) {
        any_disabled = true;
      }
    }
    if (any_disabled) {
      const int face_set = expand_cache->initial_face_sets[p];
      if (BLI_gset_haskey(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(face_set))) {
        BLI_gset_remove(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(face_set), NULL);
      }
    }
  }

  MEM_freeN(enabled_vertices);
  expand_cache->snap = prev_snap_state;
  expand_cache->invert = prev_invert_state;
}

static void sculpt_expand_falloff_factors_from_vertex_and_symm_create(
    ExpandCache *expand_cache,
    Sculpt *sd,
    Object *ob,
    const int vertex,
    eSculptExpandFalloffType falloff_type)
{

  MEM_SAFE_FREE(expand_cache->falloff_factor);
  expand_cache->falloff_factor_type = falloff_type;

  SculptSession *ss = ob->sculpt;

  /* Handle limited support for Multires. */
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    expand_cache->falloff_factor = sculpt_expand_topology_falloff_create(sd, ob, vertex);
    sculpt_expand_update_max_falloff_factor(ss, expand_cache);
    if (expand_cache->target == SCULPT_EXPAND_TARGET_FACE_SETS) {
      sculpt_expand_mesh_face_falloff_from_grids_falloff(ss, ob->data, expand_cache);
      sculpt_expand_update_max_face_falloff_factor(ss, expand_cache);
    }
    return;
  }

  switch (falloff_type) {
    case SCULPT_EXPAND_FALLOFF_GEODESIC:
      expand_cache->falloff_factor = sculpt_expand_geodesic_falloff_create(sd, ob, vertex);
      break;
    case SCULPT_EXPAND_FALLOFF_TOPOLOGY:
      expand_cache->falloff_factor = sculpt_expand_topology_falloff_create(sd, ob, vertex);
      break;
    case SCULPT_EXPAND_FALLOFF_TOPOLOGY_DIAGONALS:
      expand_cache->falloff_factor = sculpt_expand_diagonals_falloff_create(sd, ob, vertex);
      break;
    case SCULPT_EXPAND_FALLOFF_NORMALS:
      expand_cache->falloff_factor = sculpt_expand_normal_falloff_create(sd, ob, vertex, 300.0f);
      break;
    case SCULPT_EXPAND_FALLOFF_SPHERICAL:
      expand_cache->falloff_factor = sculpt_expand_spherical_falloff_create(sd, ob, vertex);
      break;
    case SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY:
      expand_cache->falloff_factor = sculpt_expand_boundary_topology_falloff_create(
          sd, ob, vertex);
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

  sculpt_expand_update_max_falloff_factor(ss, expand_cache);
  if (expand_cache->target == SCULPT_EXPAND_TARGET_FACE_SETS) {
    sculpt_expand_mesh_face_falloff_from_vertex_falloff(ob->data, expand_cache);
    sculpt_expand_update_max_face_falloff_factor(ss, expand_cache);
  }
}

static void sculpt_expand_cache_data_free(ExpandCache *expand_cache)
{
  if (expand_cache->snap_enabled_face_sets) {
    BLI_gset_free(expand_cache->snap_enabled_face_sets, NULL);
  }
  MEM_SAFE_FREE(expand_cache->nodes);
  MEM_SAFE_FREE(expand_cache->falloff_factor);
  MEM_SAFE_FREE(expand_cache->face_falloff_factor);
  MEM_SAFE_FREE(expand_cache->initial_mask);
  MEM_SAFE_FREE(expand_cache->origin_face_sets);
  MEM_SAFE_FREE(expand_cache->initial_face_sets);
  MEM_SAFE_FREE(expand_cache->initial_color);
  MEM_SAFE_FREE(expand_cache);
}

static void sculpt_expand_cache_free(SculptSession *ss)
{
  sculpt_expand_cache_data_free(ss->expand_cache);
  ss->expand_cache = NULL;
}

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
    ss->face_sets[i] = expand_cache->origin_face_sets[i];
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
      copy_v4_v4(vd.col, expand_cache->initial_color[vd.index]);
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
      *vd.mask = expand_cache->initial_mask[vd.index];
    }
    BKE_pbvh_vertex_iter_end;
    BKE_pbvh_node_mark_redraw(node);
  }
  MEM_freeN(nodes);
}

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

static void sculpt_expand_cancel(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  sculpt_expand_restore_original_state(C, ob, ss->expand_cache);

  SCULPT_undo_push_end();
  sculpt_expand_cache_free(ss);
}

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
      new_mask = sculpt_expand_gradient_falloff_get(ss, expand_cache, vd.index);
    }
    else {
      new_mask = 0.0f;
    }

    if (expand_cache->preserve) {
      new_mask = max_ff(new_mask, expand_cache->initial_mask[vd.index]);
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
    BKE_pbvh_node_mark_update_mask(expand_cache->nodes[i]);
  }
}

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
      fade = sculpt_expand_gradient_falloff_get(ss, expand_cache, vd.index);
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
                          expand_cache->initial_color[vd.index],
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
  for (int i = 0; i < ss->expand_cache->totnode; i++) {
    BKE_pbvh_node_mark_redraw(ss->expand_cache->nodes[i]);
  }

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

static void sculpt_expand_initial_state_store(Object *ob, ExpandCache *expand_cache)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);
  const int totface = ss->totfaces;

  expand_cache->initial_mask = MEM_malloc_arrayN(totvert, sizeof(float), "initial mask");
  for (int i = 0; i < totvert; i++) {
    expand_cache->initial_mask[i] = SCULPT_vertex_mask_get(ss, i);
  }

  expand_cache->initial_face_sets = MEM_malloc_arrayN(totvert, sizeof(int), "initial face set");
  expand_cache->origin_face_sets = MEM_malloc_arrayN(totvert, sizeof(int), "initial face set");
  for (int i = 0; i < totface; i++) {
    expand_cache->initial_face_sets[i] = ss->face_sets[i];
    expand_cache->origin_face_sets[i] = ss->face_sets[i];
  }

  if (expand_cache->target == SCULPT_EXPAND_TARGET_COLORS) {
    expand_cache->initial_color = MEM_malloc_arrayN(totvert, sizeof(float[4]), "initial colors");
    for (int i = 0; i < totvert; i++) {
      copy_v4_v4(expand_cache->initial_color[i], SCULPT_vertex_color_get(ss, i));
    }
  }
}

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
    expand_cache->active_factor = expand_cache->max_falloff_factor;
    expand_cache->all_enabled = true;
  }
  else {
    expand_cache->active_factor = expand_cache->falloff_factor[vertex];
    expand_cache->all_enabled = false;
  }

  if (expand_cache->target == SCULPT_EXPAND_TARGET_FACE_SETS) {
    sculpt_expand_face_sets_restore(ss, expand_cache);
  }

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

static int sculpt_expand_target_vertex_update_and_get(bContext *C,
                                                      Object *ob,
                                                      const float mouse[2])
{
  SculptSession *ss = ob->sculpt;
  SculptCursorGeometryInfo sgi;
  if (SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false, false)) {
    return SCULPT_active_vertex_get(ss);
  }
  else {
    return SCULPT_EXPAND_VERTEX_NONE;
  }
}

static void sculpt_expand_reposition_pivot(bContext *C, Object *ob, ExpandCache *expand_cache)
{
  SculptSession *ss = ob->sculpt;
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  const int totvert = SCULPT_vertex_count_get(ss);

  const bool initial_invert_state = expand_cache->invert;
  expand_cache->invert = false;
  BLI_bitmap *enabled_vertices = sculpt_expand_bitmap_from_enabled(ss, expand_cache);

  const float use_mesh_boundary = expand_cache->falloff_factor_type !=
                                  SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY;

  BLI_bitmap *boundary_vertices = sculpt_expand_boundary_from_enabled(
      ss, enabled_vertices, use_mesh_boundary);
  expand_cache->invert = initial_invert_state;

  int total = 0;
  float avg[3] = {0.0f};

  const float *expand_init_co = SCULPT_vertex_co_get(ss, expand_cache->initial_active_vertex);

  for (int i = 0; i < totvert; i++) {
    if (!BLI_BITMAP_TEST(boundary_vertices, i)) {
      continue;
    }

    if (!sculpt_expand_is_vert_in_active_compoment(ss, expand_cache, i)) {
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

static void sculpt_expand_resursion_step_add(Object *ob,
                                             ExpandCache *expand_cache,
                                             const eSculptExpandRecursionType recursion_type)
{
  SculptSession *ss = ob->sculpt;
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    return;
  }

  BLI_bitmap *enabled_vertices = sculpt_expand_bitmap_from_enabled(ss, expand_cache);

  expand_cache->texture_distorsion_strength = 0.0f;

  switch (recursion_type) {
    case SCULPT_EXPAND_RECURSION_GEODESICS:
      sculpt_expand_geodesics_from_state_boundary(ob, expand_cache, enabled_vertices);
      break;
    case SCULPT_EXPAND_RECURSION_TOPOLOGY:
      sculpt_expand_topology_from_state_boundary(ob, expand_cache, enabled_vertices);
      break;
  }

  sculpt_expand_update_max_falloff_factor(ss, expand_cache);
  if (expand_cache->target == SCULPT_EXPAND_TARGET_FACE_SETS) {
    sculpt_expand_mesh_face_falloff_from_vertex_falloff(ob->data, expand_cache);
    sculpt_expand_update_max_face_falloff_factor(ss, expand_cache);
  }

  MEM_freeN(enabled_vertices);
}

static void sculpt_expand_find_active_connected_components_from_vert(Object *ob,
                                                                     ExpandCache *expand_cache,
                                                                     const int initial_vertex)
{
  SculptSession *ss = ob->sculpt;
  for (int i = 0; i < EXPAND_SYMM_AREAS; i++) {
    expand_cache->active_connected_components[i] = EXPAND_ACTIVE_COMPOMENT_NONE;
  }

  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }

    int v = SCULPT_EXPAND_VERTEX_NONE;
    if (symm_it == 0) {
      v = initial_vertex;
    }
    else {
      float location[3];
      flip_v3_v3(location, SCULPT_vertex_co_get(ss, initial_vertex), symm_it);
      v = SCULPT_nearest_vertex_get(NULL, ob, location, FLT_MAX, false);
    }
    expand_cache->active_connected_components[symm_it] = ss->vertex_info.connected_component[v];
  }
}

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
    if (expand_cache->modify_active) {
      expand_cache->next_face_set = SCULPT_active_face_set_get(ss);
    }
    else {
      expand_cache->next_face_set = ED_sculpt_face_sets_find_next_available_id(ob->data);
    }
  }
  sculpt_expand_find_active_connected_components_from_vert(ob, expand_cache, initial_vertex);
}

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
  sculpt_expand_falloff_factors_from_vertex_and_symm_create(expand_cache,
                                                            sd,
                                                            ob,
                                                            expand_cache->initial_active_vertex,
                                                            expand_cache->falloff_factor_type);
}

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

static int sculpt_expand_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  if (!ELEM(event->type, MOUSEMOVE, EVT_MODAL_MAP)) {
    return OPERATOR_RUNNING_MODAL;
  }

  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);
  sculpt_expand_ensure_sculptsession_data(ob);

  const float mouse[2] = {event->mval[0], event->mval[1]};
  const int target_expand_vertex = sculpt_expand_target_vertex_update_and_get(C, ob, mouse);

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
        }
        else {
          expand_cache->move = true;
          copy_v2_v2(expand_cache->initial_mouse_move, mouse);
          copy_v2_v2(expand_cache->original_mouse_move, expand_cache->initial_mouse);
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
      case SCULPT_EXPAND_MODAL_TEXTURE_DISTORSION_INCREASE: {
        expand_cache->texture_distorsion_strength += SCULPT_EXPAND_TEXTURE_DISTORSION_STEP;
        break;
      }
      case SCULPT_EXPAND_MODAL_TEXTURE_DISTORSION_DECREASE: {
        expand_cache->texture_distorsion_strength -= SCULPT_EXPAND_TEXTURE_DISTORSION_STEP;
        expand_cache->texture_distorsion_strength = max_ff(
            expand_cache->texture_distorsion_strength, 0.0f);
        break;
      }
    }
  }

  if (expand_cache->move) {
    sculpt_expand_move_propagation_origin(C, ob, event, expand_cache);
  }

  if (expand_cache->snap) {
    const int active_face_set_id = expand_cache->initial_face_sets[ss->active_face_index];
    if (!BLI_gset_haskey(expand_cache->snap_enabled_face_sets,
                         POINTER_FROM_INT(active_face_set_id))) {
      BLI_gset_add(expand_cache->snap_enabled_face_sets, POINTER_FROM_INT(active_face_set_id));
    }
  }

  sculpt_expand_update_for_vertex(C, ob, target_expand_vertex);

  return OPERATOR_RUNNING_MODAL;
}

static void sculpt_expand_delete_face_set_id(
    Mesh *mesh, MeshElemMap *pmap, int *face_sets, const int totface, const int delete_id)
{

  BLI_LINKSTACK_DECLARE(queue, int);
  BLI_LINKSTACK_DECLARE(queue_next, int);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  for (int i = 0; i < totface; i++) {
    if (face_sets[i] == delete_id) {
      BLI_LINKSTACK_PUSH(queue, i);
    }
  }

  while (BLI_LINKSTACK_SIZE(queue)) {
    int f_index;
    while (f_index = BLI_LINKSTACK_POP(queue)) {

      int other_id = delete_id;
      const MPoly *c_poly = &mesh->mpoly[f_index];
      for (int l = 0; l < c_poly->totloop; l++) {
        const MLoop *c_loop = &mesh->mloop[c_poly->loopstart + l];
        const MeshElemMap *vert_map = &pmap[c_loop->v];
        for (int i = 0; i < vert_map->count; i++) {

          const int neighbor_face_index = vert_map->indices[i];
          if (face_sets[neighbor_face_index] != delete_id) {
            other_id = face_sets[neighbor_face_index];
          }
        }
      }

      if (other_id != delete_id) {
        face_sets[f_index] = other_id;
      }
      else {
        BLI_LINKSTACK_PUSH(queue_next, f_index);
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

  expand_cache->invert = RNA_boolean_get(op->ptr, "invert");
  expand_cache->preserve = RNA_boolean_get(op->ptr, "use_mask_preserve");
  expand_cache->falloff_gradient = RNA_boolean_get(op->ptr, "use_falloff_gradient");
  expand_cache->target = RNA_enum_get(op->ptr, "target");
  expand_cache->modify_active = RNA_boolean_get(op->ptr, "use_modify_active");
  expand_cache->reposition_pivot = RNA_boolean_get(op->ptr, "use_reposition_pivot");

  /* TODO: Expose in RNA. */
  expand_cache->loop_count = 1;
  expand_cache->brush_gradient = false;

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
  expand_cache->texture_distorsion_strength = 0.0f;

  expand_cache->blend_mode = expand_cache->brush->blend;
}

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
    BKE_sculpt_color_layer_create_if_needed(ob);
    depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, needs_colors);
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
  sculpt_expand_initial_state_store(ob, ss->expand_cache);

  if (ss->expand_cache->modify_active) {
    sculpt_expand_delete_face_set_id(ob->data,
                                     ss->pmap,
                                     ss->expand_cache->initial_face_sets,
                                     ss->totfaces,
                                     ss->expand_cache->next_face_set);
  }

  /* Initialize the factors. */
  eSculptExpandFalloffType falloff_type = RNA_enum_get(op->ptr, "falloff_type");
  if (SCULPT_vertex_is_boundary(ss, ss->expand_cache->initial_active_vertex)) {
    falloff_type = SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY;
  }

  sculpt_expand_falloff_factors_from_vertex_and_symm_create(
      ss->expand_cache, sd, ob, ss->expand_cache->initial_active_vertex, falloff_type);

  /* Initial update. */
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
      {SCULPT_EXPAND_MODAL_TEXTURE_DISTORSION_INCREASE,
       "TEXTURE_DISTORSION_INCREASE",
       0,
       "Texture Distorsion Increase",
       ""},
      {SCULPT_EXPAND_MODAL_TEXTURE_DISTORSION_DECREASE,
       "TEXTURE_DISTORSION_DECREASE",
       0,
       "Texture Distorsion Decrease",
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
               "Fallof Type",
               "Initial falloff of the expand operation");

  ot->prop = RNA_def_boolean(
      ot->srna, "invert", false, "Invert", "Invert the expand active elements");
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_mask_preserve",
                             false,
                             "Preserve Previous",
                             "Preserve the previous state of the target data");
  ot->prop = RNA_def_boolean(
      ot->srna, "use_falloff_gradient", false, "Falloff Gradient", "Expand Using a Falloff");

  ot->prop = RNA_def_boolean(
      ot->srna, "use_modify_active", false, "Modify Active", "Modify Active");

  ot->prop = RNA_def_boolean(
      ot->srna, "use_reposition_pivot", true, "Reposition Pivot", "Reposition pivot");
}
