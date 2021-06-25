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

AutomaskingCache *SCULPT_automasking_active_cache_get(SculptSession *ss)
{
  if (ss->cache) {
    return ss->cache->automasking;
  }
  if (ss->filter_cache) {
    return ss->filter_cache->automasking;
  }
  return NULL;
}

bool SCULPT_is_automasking_mode_enabled(const Sculpt *sd,
                                        const Brush *br,
                                        const eAutomasking_flag mode)
{
  if (br) {
    return br->automasking_flags & mode || sd->automasking_flags & mode;
  }
  return sd->automasking_flags & mode;
}

bool SCULPT_is_automasking_enabled(const Sculpt *sd, const SculptSession *ss, const Brush *br)
{
  /*
  if (br && SCULPT_stroke_is_dynamic_topology(ss, br)) {
    return false;
  }*/

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
  if (SCULPT_is_automasking_mode_enabled(sd, br, BRUSH_AUTOMASKING_CONCAVITY)) {
    return true;
  }
  if (br && br->concave_mask_factor > 0.0f) {
    return true;
  }

  return false;
}

static int sculpt_automasking_mode_effective_bits(const Sculpt *sculpt, const Brush *brush)
{
  if (brush) {
    return sculpt->automasking_flags | brush->automasking_flags;
  }
  return sculpt->automasking_flags;
}

static float sculpt_concavity_factor(AutomaskingCache *automasking, float fac)
{
  if (automasking->settings.flags & BRUSH_AUTOMASKING_INVERT_CONCAVITY) {
    fac = 1.0 - fac;
  }

  fac = pow(fac * 1.5f, (0.5f + automasking->settings.concave_factor) * 8.0);
  CLAMP(fac, 0.0f, 1.0f);

  return fac;
}

static bool SCULPT_automasking_needs_factors_cache(const Sculpt *sd, const Brush *brush)
{

  const int automasking_flags = sculpt_automasking_mode_effective_bits(sd, brush);
  if (automasking_flags & BRUSH_AUTOMASKING_TOPOLOGY) {
    return true;
  }
  if (automasking_flags & BRUSH_AUTOMASKING_BOUNDARY_EDGES) {
    return brush && brush->automasking_boundary_edges_propagation_steps != 1;
  }
  if (automasking_flags & BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS) {
    return brush && brush->automasking_boundary_edges_propagation_steps != 1;
  }
  return false;
}

ATTR_NO_OPT float SCULPT_automasking_factor_get(AutomaskingCache *automasking,
                                                SculptSession *ss,
                                                SculptVertRef vert)
{
  float mask = 1.0f;
  bool do_concave;

  if (!automasking) {
    return mask;
  }

  do_concave = ss->cache && ss->cache->brush && ss->cache->brush->concave_mask_factor > 0.0f &&
               (automasking->settings.flags & BRUSH_AUTOMASKING_CONCAVITY);

  /* If the cache is initialized with valid info, use the cache. This is used when the
   * automasking information can't be computed in real time per vertex and needs to be
   * initialized for the whole mesh when the stroke starts. */
  if (automasking->factorlayer) {
    mask = *(float *)SCULPT_temp_cdata_get(vert, automasking->factorlayer);
  }

  // don't used cached automasking factors for facesets or concave in
  // dyntopo
  if (automasking->factorlayer && !ss->bm) {
    return mask;
  }

  if (do_concave) {
    float fac = SCULPT_calc_concavity(ss, vert);

    mask *= sculpt_concavity_factor(automasking, fac);
  }

  if (automasking->settings.flags & BRUSH_AUTOMASKING_FACE_SETS) {
    if (!SCULPT_vertex_has_face_set(ss, vert, automasking->settings.initial_face_set)) {
      return 0.0f;
    }
  }

  if (automasking->settings.flags & BRUSH_AUTOMASKING_BOUNDARY_EDGES) {
    if (SCULPT_vertex_is_boundary(ss, vert)) {
      return 0.0f;
    }
  }

  if (automasking->settings.flags & BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS) {
    if (!SCULPT_vertex_has_unique_face_set(ss, vert)) {
      return 0.0f;
    }
  }

  return mask;
}

void SCULPT_automasking_cache_free(AutomaskingCache *automasking)
{
  if (!automasking) {
    return;
  }

  if (automasking->factorlayer) {
    MEM_SAFE_FREE(automasking->factorlayer);
  }

  MEM_SAFE_FREE(automasking);
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
  SculptCustomLayer *factorlayer;
  float radius;
  bool use_radius;
  float location[3];
  char symm;
} AutomaskFloodFillData;

static bool automask_floodfill_cb(SculptSession *ss,
                                  SculptVertRef from_vref,
                                  SculptVertRef to_vref,
                                  bool UNUSED(is_duplicate),
                                  void *userdata)
{
  AutomaskFloodFillData *data = userdata;

  *(float *)SCULPT_temp_cdata_get(to_vref, data->factorlayer) = 1.0f;
  *(float *)SCULPT_temp_cdata_get(from_vref, data->factorlayer) = 1.0f;

  return (!data->use_radius ||
          SCULPT_is_vertex_inside_brush_radius_symm(
              SCULPT_vertex_co_get(ss, to_vref), data->location, data->radius, data->symm));
}

static void SCULPT_topology_automasking_init(Sculpt *sd,
                                             Object *ob,
                                             SculptCustomLayer *factorlayer)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && !ss->pmap) {
    BLI_assert(!"Topology masking: pmap missing");
    return;
  }

  const int totvert = SCULPT_vertex_count_get(ss);
  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    float *fac = SCULPT_temp_cdata_get(vertex, factorlayer);
    *fac = 0.0f;
  }

  /* Flood fill automask to connected vertices. Limited to vertices inside
   * the brush radius if the tool requires it. */
  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  const float radius = ss->cache ? ss->cache->radius : FLT_MAX;
  SCULPT_floodfill_add_active(sd, ob, ss, &flood, radius);

  AutomaskFloodFillData fdata = {
      .factorlayer = factorlayer,
      .radius = radius,
      .use_radius = ss->cache && sculpt_automasking_is_constrained_by_radius(brush),
      .symm = SCULPT_mesh_symmetry_xyz_get(ob),
  };
  copy_v3_v3(fdata.location, SCULPT_active_vertex_co_get(ss));
  SCULPT_floodfill_execute(ss, &flood, automask_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);
}

static void sculpt_face_sets_automasking_init(Sculpt *sd,
                                              Object *ob,
                                              SculptCustomLayer *factorlayer)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (!SCULPT_is_automasking_enabled(sd, ss, brush)) {
    return;
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && !ss->pmap) {
    BLI_assert(!"Face Sets automasking: pmap missing");
    return;
  }

  int tot_vert = SCULPT_vertex_count_get(ss);
  int active_face_set = SCULPT_active_face_set_get(ss);
  for (int i = 0; i < tot_vert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    if (!SCULPT_vertex_has_face_set(ss, vertex, active_face_set)) {
      *(float *)SCULPT_temp_cdata_get(vertex, factorlayer) = 0.0f;
    }
  }

  return;
}

#define EDGE_DISTANCE_INF -1

void SCULPT_boundary_automasking_init(Object *ob,
                                      eBoundaryAutomaskMode mode,
                                      int propagation_steps,
                                      SculptCustomLayer *factorlayer)
{
  SculptSession *ss = ob->sculpt;

  if (!ss->pmap) {
    BLI_assert(!"Boundary Edges masking: pmap missing");
    return;
  }

  const int totvert = SCULPT_vertex_count_get(ss);
  int *edge_distance = MEM_callocN(sizeof(int) * totvert, "automask_factor");

  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    edge_distance[i] = EDGE_DISTANCE_INF;
    switch (mode) {
      case AUTOMASK_INIT_BOUNDARY_EDGES:
        if (SCULPT_vertex_is_boundary(ss, vertex)) {
          edge_distance[i] = 0;
        }
        break;
      case AUTOMASK_INIT_BOUNDARY_FACE_SETS:
        if (!SCULPT_vertex_has_unique_face_set(ss, vertex)) {
          edge_distance[i] = 0;
        }
        break;
    }
  }

  for (int propagation_it = 0; propagation_it < propagation_steps; propagation_it++) {
    for (int i = 0; i < totvert; i++) {
      SculptVertRef vref = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

      if (edge_distance[i] != EDGE_DISTANCE_INF) {
        continue;
      }
      SculptVertexNeighborIter ni;
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vref, ni) {
        if (edge_distance[ni.index] == propagation_it) {
          edge_distance[i] = propagation_it + 1;
        }
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
    }
  }

  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    if (edge_distance[i] == EDGE_DISTANCE_INF) {
      continue;
    }
    const float p = 1.0f - ((float)edge_distance[i] / (float)propagation_steps);
    const float edge_boundary_automask = pow2f(p);

    *(float *)SCULPT_temp_cdata_get(vertex, factorlayer) *= (1.0f - edge_boundary_automask);
  }

  MEM_SAFE_FREE(edge_distance);
}

static void SCULPT_automasking_cache_settings_update(AutomaskingCache *automasking,
                                                     SculptSession *ss,
                                                     Sculpt *sd,
                                                     Brush *brush)
{
  automasking->settings.flags = sculpt_automasking_mode_effective_bits(sd, brush);

  automasking->settings.initial_face_set = SCULPT_active_face_set_get(ss);
  automasking->settings.concave_factor = brush ? brush->concave_mask_factor : 0.0f;
}

float SCULPT_calc_concavity(SculptSession *ss, SculptVertRef vref)
{
  SculptVertexNeighborIter ni;
  float co[3], tot = 0.0, elen = 0.0;
  const float *vco = SCULPT_vertex_co_get(ss, vref);

  zero_v3(co);

  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vref, ni) {
    const float *vco2 = SCULPT_vertex_co_get(ss, ni.vertex);

    elen += len_v3v3(vco, vco2);
    add_v3_v3(co, vco2);
    tot += 1.0f;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (!tot) {
    return 0.5f;
  }

  elen /= tot;
  mul_v3_fl(co, 1.0 / tot);
  sub_v3_v3(co, vco);
  mul_v3_fl(co, -1.0 / elen);

  float no[3];
  SCULPT_vertex_normal_get(ss, vref, no);

  float f = dot_v3v3(co, no) * 0.5 + 0.5;
  return 1.0 - f;
}

static void SCULPT_concavity_automasking_init(Object *ob,
                                              Brush *brush,
                                              AutomaskingCache *automasking,
                                              SculptCustomLayer *factorlayer)
{
  SculptSession *ss = ob->sculpt;

  if (!ss) {
    return;
  }

  const int totvert = SCULPT_vertex_count_get(ss);

  for (int i = 0; i < totvert; i++) {
    SculptVertRef vref = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);
    float f = SCULPT_calc_concavity(ss, vref);
    f = sculpt_concavity_factor(automasking, f);

    *(float *)SCULPT_temp_cdata_get(vref, factorlayer) *= f;
  }
  // BKE_pbvh_vertex_iter_begin
}

ATTR_NO_OPT AutomaskingCache *SCULPT_automasking_cache_init(Sculpt *sd, Brush *brush, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  if (!SCULPT_is_automasking_enabled(sd, ss, brush)) {
    return NULL;
  }

  AutomaskingCache *automasking = MEM_callocN(sizeof(AutomaskingCache), "automasking cache");
  SCULPT_automasking_cache_settings_update(automasking, ss, sd, brush);
  SCULPT_boundary_info_ensure(ob);

  if (!SCULPT_automasking_needs_factors_cache(sd, brush)) {
    return automasking;
  }

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_face_random_access_ensure(ss);

  automasking->factorlayer = MEM_callocN(sizeof(*automasking->factorlayer),
                                         "automasking->factorlayer");

  if (!SCULPT_temp_customlayer_get(ss,
                                   ATTR_DOMAIN_POINT,
                                   CD_PROP_FLOAT,
                                   "__sculpt_mask_factor",
                                   automasking->factorlayer)) {
    // failed
    MEM_freeN(automasking->factorlayer);
    return automasking;
  }

  // automasking->factorlayer = SCULPT_temp_customlayer_ensure()
  // automasking->factor = MEM_malloc_arrayN(totvert, sizeof(float), "automask_factor");
  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);
    float *f = SCULPT_temp_cdata_get(vertex, automasking->factorlayer);

    *f = 1.0f;
  }

  const int boundary_propagation_steps = brush ?
                                             brush->automasking_boundary_edges_propagation_steps :
                                             1;

  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_TOPOLOGY)) {
    SCULPT_vertex_random_access_ensure(ss);
    SCULPT_topology_automasking_init(sd, ob, automasking->factorlayer);
  }

  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS)) {
    SCULPT_vertex_random_access_ensure(ss);
    SCULPT_boundary_automasking_init(ob,
                                     AUTOMASK_INIT_BOUNDARY_FACE_SETS,
                                     boundary_propagation_steps,
                                     automasking->factorlayer);
  }

  // for dyntopo, only topology and fset boundary area initialized here
  if (ss->bm) {
    return automasking;
  }

  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_FACE_SETS)) {
    SCULPT_vertex_random_access_ensure(ss);
    sculpt_face_sets_automasking_init(sd, ob, automasking->factorlayer);
  }

  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_BOUNDARY_EDGES)) {
    SCULPT_vertex_random_access_ensure(ss);
    SCULPT_boundary_automasking_init(
        ob, AUTOMASK_INIT_BOUNDARY_EDGES, boundary_propagation_steps, automasking->factorlayer);
  }
  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_CONCAVITY)) {
    SCULPT_vertex_random_access_ensure(ss);
    SCULPT_concavity_automasking_init(ob, brush, automasking, automasking->factorlayer);
  }

  return automasking;
}
