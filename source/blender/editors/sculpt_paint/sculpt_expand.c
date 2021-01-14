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
#include "BKE_mesh.h"
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

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

#define SCULPT_EXPAND_VERTEX_NONE -1

static EnumPropertyItem prop_sculpt_expand_faloff_type_items[] = {
    {SCULPT_EXPAND_FALLOFF_GEODESICS, "GEODESICS", 0, "Surface", ""},
    {SCULPT_EXPAND_FALLOFF_TOPOLOGY, "TOPOLOGY", 0, "Topology", ""},
    {SCULPT_EXPAND_FALLOFF_NORMALS, "NORMALS", 0, "Normals", ""},
    {SCULPT_EXPAND_FALLOFF_SPHERICAL, "SPHERICAL", 0, "Spherical", ""},
    {SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY, "BOUNDARY_TOPOLOGY`", 0, "Boundary Topology", ""},
    {0, NULL, 0, NULL, NULL},
};

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

static bool mask_expand_topology_floodfill_cb(
    SculptSession *UNUSED(ss), int from_v, int to_v, bool is_duplicate, void *userdata)
{
  ExpandFloodFillData *data = userdata;
  if (!is_duplicate) {
    const int to_it = data->dists[from_v] + 1;
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
  float *dists = MEM_malloc_arrayN(sizeof(float), totvert, "spherical dist");

  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  SCULPT_floodfill_add_initial_with_symmetry(sd, ob, ss, &flood, vertex, FLT_MAX);

  ExpandFloodFillData fdata;
  fdata.dists = dists;

  SCULPT_floodfill_execute(ss, &flood, mask_expand_topology_floodfill_cb, &fdata);
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

  BLI_gsqueue_free(queue);
  MEM_freeN(visited_vertices);
  return dists;
}

static void sculpt_expand_update_max_falloff_factor(SculptSession *ss, ExpandCache *expand_cache)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  expand_cache->max_falloff_factor = -FLT_MAX;
  for (int i = 0; i < totvert; i++) {
    expand_cache->max_falloff_factor = max_ff(expand_cache->max_falloff_factor,
                                              expand_cache->falloff_factor[i]);
  }
}

static void sculpt_expand_falloff_factors_from_vertex_and_symm_create(
    ExpandCache *expand_cache,
    Sculpt *sd,
    Object *ob,
    const int vertex,
    eSculptExpandFalloffType falloff_type)
{
  if (expand_cache->falloff_factor && expand_cache->falloff_factor_type == falloff_type) {
    /* Falloffs are already initialize with the current falloff type, nothing to do. */
    return;
  }

  if (expand_cache->falloff_factor) {
    MEM_freeN(expand_cache->falloff_factor);
  }

  switch (falloff_type) {
    case SCULPT_EXPAND_FALLOFF_GEODESICS:
      expand_cache->falloff_factor = sculpt_expand_geodesic_falloff_create(sd, ob, vertex);
      break;
    case SCULPT_EXPAND_FALLOFF_TOPOLOGY:
      expand_cache->falloff_factor = sculpt_expand_topology_falloff_create(sd, ob, vertex);
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
  }

  expand_cache->falloff_factor_type = falloff_type;

  SculptSession *ss = ob->sculpt;
  sculpt_expand_update_max_falloff_factor(ss, expand_cache);
}

void sculpt_expand_cache_free(ExpandCache *expand_cache)
{
  MEM_SAFE_FREE(expand_cache->nodes);
  MEM_SAFE_FREE(expand_cache->falloff_factor);
  MEM_SAFE_FREE(expand_cache->initial_mask);
  MEM_SAFE_FREE(expand_cache->initial_face_sets);
  MEM_SAFE_FREE(expand_cache);
}

static void sculpt_mask_expand_cancel(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  const bool create_face_set = RNA_boolean_get(op->ptr, "create_face_set");

  MEM_freeN(op->customdata);

  for (int n = 0; n < ss->filter_cache->totnode; n++) {
    PBVHNode *node = ss->filter_cache->nodes[n];
    if (create_face_set) {
      for (int i = 0; i < ss->totfaces; i++) {
        ss->face_sets[i] = ss->filter_cache->prev_face_set[i];
      }
    }
    else {
      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
      {
        *vd.mask = ss->filter_cache->prev_mask[vd.index];
      }
      BKE_pbvh_vertex_iter_end;
    }

    BKE_pbvh_node_mark_redraw(node);
  }

  if (!create_face_set) {
    SCULPT_flush_update_step(C, SCULPT_UPDATE_MASK);
  }
  SCULPT_filter_cache_free(ss);
  SCULPT_undo_push_end();
  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
  ED_workspace_status_text(C, NULL);
}

static void sculpt_expand_cancel(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  sculpt_expand_cache_free(ss->expand_cache);
  ED_workspace_status_text(C, NULL);
}

static bool sculpt_expand_state_get(ExpandCache *expand_cache, int i)
{
  bool enabled = expand_cache->falloff_factor[i] <= expand_cache->active_factor;
  if (expand_cache->invert) {
    enabled = !enabled;
  }
  return enabled;
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
    const bool enabled = sculpt_expand_state_get(expand_cache, vd.index);

    if (enabled) {
      if (expand_cache->mask_preserve_previous) {
        *vd.mask = max_ff(initial_mask, expand_cache->initial_mask[vd.index]);
      }
      else {
        *vd.mask = 1.0f;
      }
    }
    else {
      *vd.mask = 0.0f;
    }

    if (*vd.mask != initial_mask) {
      any_changed = true;
      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
  if (any_changed) {
    BKE_pbvh_node_mark_update_mask(node);
  }
}

static void sculpt_expand_flush_updates(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  for (int i = 0; i < ss->expand_cache->totnode; i++) {
    BKE_pbvh_node_mark_redraw(ss->expand_cache->nodes[i]);
  }
  SCULPT_flush_update_step(C, SCULPT_UPDATE_MASK);
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
  for (int i = 0; i < totface; i++) {
    expand_cache->initial_face_sets[i] = ss->face_sets[i];
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
  }
  else {
    expand_cache->active_factor = expand_cache->falloff_factor[vertex];
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = expand_cache->nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, expand_cache->totnode);
  BLI_task_parallel_range(
      0, expand_cache->totnode, &data, sculpt_expand_mask_update_task_cb, &settings);

  sculpt_expand_flush_updates(C);
}

static int sculpt_expand_target_vertex_update_and_get(bContext *C,
                                                      Object *ob,
                                                      const wmEvent *event)
{
  SculptSession *ss = ob->sculpt;
  SculptCursorGeometryInfo sgi;
  float mouse[2] = {event->mval[0], event->mval[1]};
  if (SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false)) {
    return SCULPT_active_vertex_get(ss);
  }
  else {
    return SCULPT_EXPAND_VERTEX_NONE;
  }
}

static int sculpt_expand_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  const int target_expand_vertex = sculpt_expand_target_vertex_update_and_get(C, ob, event);
  sculpt_expand_update_for_vertex(C, ob, target_expand_vertex);

  if ((event->type == LEFTMOUSE && event->val == KM_RELEASE) ||
      (event->type == EVT_RETKEY && event->val == KM_PRESS) ||
      (event->type == EVT_PADENTER && event->val == KM_PRESS)) {
    sculpt_expand_cache_free(ss->expand_cache);
    SCULPT_undo_push_end();
    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
    ED_workspace_status_text(C, NULL);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_expand_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  /* Update object. */
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);
  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_undo_push_begin(ob, "expand");

  /* Create the Expand Cache. */
  ss->expand_cache = MEM_callocN(sizeof(ExpandCache), "expand cache");

  /* Set the initial element for expand. */
  int initial_vertex = sculpt_expand_target_vertex_update_and_get(C, ob, event);
  if (initial_vertex == SCULPT_EXPAND_VERTEX_NONE) {
    /* Cursor not over the mesh, for creating valid initial falloffs, fallback to the last active
     * vertex in the sculpt session. */
    initial_vertex = SCULPT_active_vertex_get(ss);
  }
  ss->expand_cache->initial_active_vertex = initial_vertex;

  /* Cache PBVH nodes. */
  BKE_pbvh_search_gather(
      ss->pbvh, NULL, NULL, &ss->expand_cache->nodes, &ss->expand_cache->totnode);

  /* Store initial state. */
  sculpt_expand_initial_state_store(ob, ss->expand_cache);

  /* Initialize the factors. */
  sculpt_expand_falloff_factors_from_vertex_and_symm_create(
      ss->expand_cache, sd, ob, initial_vertex, SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY);

  /* Initial update. */
  sculpt_expand_update_for_vertex(C, ob, initial_vertex);

  const char *status_str = TIP_(
      "Move the mouse to expand from the active vertex. LMB: confirm, ESC/RMB: "
      "cancel");
  ED_workspace_status_text(C, status_str);

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
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
}
