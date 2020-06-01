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
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

bool SCULPT_is_automasking_mode_enabled(const Sculpt *sd,
                                        const Brush *br,
                                        const eAutomasking_flag mode)
{
  return br->automasking_flags & mode || sd->automasking_flags & mode;
}

bool SCULPT_is_automasking_enabled(const Sculpt *sd, const SculptSession *ss, const Brush *br)
{
  if (SCULPT_stroke_is_dynamic_topology(ss, br)) {
    return false;
  }
  if (SCULPT_is_automasking_mode_enabled(sd, br, BRUSH_AUTOMASKING_TOPOLOGY)) {
    return true;
  }
  if (SCULPT_is_automasking_mode_enabled(sd, br, BRUSH_AUTOMASKING_FACE_SETS)) {
    return true;
  }
  if (SCULPT_is_automasking_mode_enabled(sd, br, BRUSH_AUTOMASKING_BOUNDARY_EDGES)) {
    return true;
  }
  if (SCULPT_is_automasking_mode_enabled(sd, br, BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS)) {
    return true;
  }
  return false;
}

float SCULPT_automasking_factor_get(SculptSession *ss, int vert)
{
  if (ss->cache && ss->cache->automask) {
    return ss->cache->automask[vert];
  }
  else {
    return 1.0f;
  }
}

void SCULPT_automasking_end(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->cache && ss->cache->automask) {
    MEM_freeN(ss->cache->automask);
  }
}

static bool sculpt_automasking_is_constrained_by_radius(Brush *br)
{
  /* 2D falloff is not constrained by radius. */
  if (br->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    return false;
  }

  if (ELEM(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB, SCULPT_TOOL_ROTATE)) {
    return true;
  }
  return false;
}

typedef struct AutomaskFloodFillData {
  float *automask_factor;
  float radius;
  bool use_radius;
  float location[3];
  char symm;
} AutomaskFloodFillData;

static bool automask_floodfill_cb(
    SculptSession *ss, int UNUSED(from_v), int to_v, bool UNUSED(is_duplicate), void *userdata)
{
  AutomaskFloodFillData *data = userdata;

  data->automask_factor[to_v] = 1.0f;
  return (!data->use_radius ||
          SCULPT_is_vertex_inside_brush_radius_symm(
              SCULPT_vertex_co_get(ss, to_v), data->location, data->radius, data->symm));
}

static float *SCULPT_topology_automasking_init(Sculpt *sd, Object *ob, float *automask_factor)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (!SCULPT_is_automasking_enabled(sd, ss, brush)) {
    return NULL;
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && !ss->pmap) {
    BLI_assert(!"Topology masking: pmap missing");
    return NULL;
  }

  const int totvert = SCULPT_vertex_count_get(ss);
  for (int i = 0; i < totvert; i++) {
    ss->cache->automask[i] = 0.0f;
  }

  /* Flood fill automask to connected vertices. Limited to vertices inside
   * the brush radius if the tool requires it. */
  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  SCULPT_floodfill_add_active(sd, ob, ss, &flood, ss->cache->radius);

  AutomaskFloodFillData fdata = {
      .automask_factor = automask_factor,
      .radius = ss->cache->radius,
      .use_radius = sculpt_automasking_is_constrained_by_radius(brush),
      .symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL,
  };
  copy_v3_v3(fdata.location, SCULPT_active_vertex_co_get(ss));
  SCULPT_floodfill_execute(ss, &flood, automask_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);

  return automask_factor;
}

static float *sculpt_face_sets_automasking_init(Sculpt *sd, Object *ob, float *automask_factor)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (!SCULPT_is_automasking_enabled(sd, ss, brush)) {
    return NULL;
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && !ss->pmap) {
    BLI_assert(!"Face Sets automasking: pmap missing");
    return NULL;
  }

  int tot_vert = SCULPT_vertex_count_get(ss);
  int active_face_set = SCULPT_active_face_set_get(ss);
  for (int i = 0; i < tot_vert; i++) {
    if (!SCULPT_vertex_has_face_set(ss, i, active_face_set)) {
      automask_factor[i] *= 0.0f;
    }
  }

  return automask_factor;
}

#define EDGE_DISTANCE_INF -1

float *SCULPT_boundary_automasking_init(Object *ob,
                                        eBoundaryAutomaskMode mode,
                                        int propagation_steps,
                                        float *automask_factor)
{
  SculptSession *ss = ob->sculpt;

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && !ss->pmap) {
    BLI_assert(!"Boundary Edges masking: pmap missing");
    return NULL;
  }

  const int totvert = SCULPT_vertex_count_get(ss);
  int *edge_distance = MEM_callocN(sizeof(int) * totvert, "automask_factor");

  for (int i = 0; i < totvert; i++) {
    edge_distance[i] = EDGE_DISTANCE_INF;
    switch (mode) {
      case AUTOMASK_INIT_BOUNDARY_EDGES:
        if (!SCULPT_vertex_is_boundary(ss, i)) {
          edge_distance[i] = 0;
        }
        break;
      case AUTOMASK_INIT_BOUNDARY_FACE_SETS:
        if (!SCULPT_vertex_has_unique_face_set(ss, i)) {
          edge_distance[i] = 0;
        }
        break;
    }
  }

  for (int propagation_it = 0; propagation_it < propagation_steps; propagation_it++) {
    for (int i = 0; i < totvert; i++) {
      if (edge_distance[i] == EDGE_DISTANCE_INF) {
        SculptVertexNeighborIter ni;
        SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, i, ni) {
          if (edge_distance[ni.index] == propagation_it) {
            edge_distance[i] = propagation_it + 1;
          }
        }
        SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
      }
    }
  }

  for (int i = 0; i < totvert; i++) {
    if (edge_distance[i] != EDGE_DISTANCE_INF) {
      const float p = 1.0f - ((float)edge_distance[i] / (float)propagation_steps);
      const float edge_boundary_automask = pow2f(p);
      automask_factor[i] *= (1.0f - edge_boundary_automask);
    }
  }

  MEM_SAFE_FREE(edge_distance);
  return automask_factor;
}

void SCULPT_automasking_init(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  const int totvert = SCULPT_vertex_count_get(ss);

  if (!SCULPT_is_automasking_enabled(sd, ss, brush)) {
    return;
  }

  ss->cache->automask = MEM_callocN(sizeof(float) * SCULPT_vertex_count_get(ss),
                                    "automask_factor");

  for (int i = 0; i < totvert; i++) {
    ss->cache->automask[i] = 1.0f;
  }

  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_TOPOLOGY)) {
    SCULPT_vertex_random_access_init(ss);
    SCULPT_topology_automasking_init(sd, ob, ss->cache->automask);
  }
  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_FACE_SETS)) {
    SCULPT_vertex_random_access_init(ss);
    sculpt_face_sets_automasking_init(sd, ob, ss->cache->automask);
  }

  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_BOUNDARY_EDGES)) {
    SCULPT_vertex_random_access_init(ss);
    SCULPT_boundary_automasking_init(ob,
                                     AUTOMASK_INIT_BOUNDARY_EDGES,
                                     brush->automasking_boundary_edges_propagation_steps,
                                     ss->cache->automask);
  }
  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS)) {
    SCULPT_vertex_random_access_init(ss);
    SCULPT_boundary_automasking_init(ob,
                                     AUTOMASK_INIT_BOUNDARY_FACE_SETS,
                                     brush->automasking_boundary_edges_propagation_steps,
                                     ss->cache->automask);
  }
}
