/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_blenlib.h"
#include "BLI_hash.h"
#include "BLI_index_range.hh"
#include "BLI_math.h"
#include "BLI_math_vec_types.hh"
#include "BLI_set.hh"
#include "BLI_task.h"
#include "BLI_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
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

#include <cmath>
#include <cstdlib>

using blender::float3;
using blender::IndexRange;
using blender::Set;
using blender::Vector;

AutomaskingCache *SCULPT_automasking_active_cache_get(SculptSession *ss)
{
  if (ss->cache) {
    return ss->cache->automasking;
  }
  if (ss->filter_cache) {
    return ss->filter_cache->automasking;
  }
  return nullptr;
}

bool SCULPT_is_automasking_mode_enabled(const Sculpt *sd,
                                        const Brush *br,
                                        const eAutomasking_flag mode)
{
  int automasking = sd->automasking_flags;

  if (br) {
    automasking |= br->automasking_flags;
  }

  return (eAutomasking_flag)automasking & mode;
}

bool SCULPT_is_automasking_enabled(const Sculpt *sd, const SculptSession *ss, const Brush *br)
{
  if (br && SCULPT_stroke_is_dynamic_topology(ss, br)) {
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
  if (SCULPT_is_automasking_mode_enabled(sd, br, BRUSH_AUTOMASKING_CAVITY_ALL)) {
    return true;
  }

  return false;
}

static int sculpt_automasking_mode_effective_bits(const Sculpt *sculpt, const Brush *brush)
{
  if (brush) {
    int flags = sculpt->automasking_flags | brush->automasking_flags;

    /* Check if we are using brush cavity settings. */
    if (brush->automasking_flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
      flags &= ~(BRUSH_AUTOMASKING_CAVITY_ALL | BRUSH_AUTOMASKING_CAVITY_USE_CURVE |
                 BRUSH_AUTOMASKING_CAVITY_NORMAL);
      flags |= brush->automasking_flags;
    }
    else if (sculpt->automasking_flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
      flags &= ~(BRUSH_AUTOMASKING_CAVITY_ALL | BRUSH_AUTOMASKING_CAVITY_USE_CURVE |
                 BRUSH_AUTOMASKING_CAVITY_NORMAL);
      flags |= sculpt->automasking_flags;
    }

    return flags;
  }
  return sculpt->automasking_flags;
}

static bool SCULPT_automasking_needs_factors_cache(const Sculpt *sd, const Brush *brush)
{

  const int automasking_flags = sculpt_automasking_mode_effective_bits(sd, brush);
  if (automasking_flags & BRUSH_AUTOMASKING_TOPOLOGY) {
    return true;
  }
  if (automasking_flags &
      (BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS | BRUSH_AUTOMASKING_BOUNDARY_EDGES)) {
    return brush && brush->automasking_boundary_edges_propagation_steps != 1;
  }
  return false;
}

static float sculpt_cavity_calc_factor(AutomaskingCache *automasking, float factor)
{
  float sign = signf(factor);

  factor = fabsf(factor) * automasking->settings.cavity_factor * 50.0f;

  factor = factor * sign * 0.5f + 0.5f;
  CLAMP(factor, 0.0f, 1.0f);

  return (automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_INVERTED) ? 1.0f - factor :
                                                                             factor;
}

struct CavityBlurVert {
  PBVHVertRef vertex;
  float dist;
  int depth;

  CavityBlurVert(PBVHVertRef vertex_, float dist_, int depth_)
      : vertex(vertex_), dist(dist_), depth(depth_)
  {
  }

  CavityBlurVert() = default;
};

static void sculpt_calc_blurred_cavity(SculptSession *ss,
                                       AutomaskingCache *automasking,
                                       int steps,
                                       PBVHVertRef vertex)
{
  float3 sno1(0.0f);
  float3 sno2(0.0f);
  float3 sco1(0.0f);
  float3 sco2(0.0f);
  float len1_sum = 0.0f, len2_sum = 0.0f;
  int sco1_len = 0, sco2_len = 0;

  /* Steps starts at 1, but API and user interface
   * are zero-based.
   */
  steps++;

  Vector<CavityBlurVert, 64> queue;
  Set<int64_t, 64> visit;

  int start = 0, end = 0;

  queue.resize(64);

  CavityBlurVert initial(vertex, 0.0f, 0);

  visit.add_new(vertex.i);
  queue[0] = initial;
  end = 1;

  const float *co1 = SCULPT_vertex_co_get(ss, vertex);

  while (start != end) {
    CavityBlurVert &blurvert = queue[start];
    PBVHVertRef v = blurvert.vertex;
    start = (start + 1) % queue.size();

    float3 no;

    const float *co = SCULPT_vertex_co_get(ss, v);
    SCULPT_vertex_normal_get(ss, v, no);

    float centdist = len_v3v3(co, co1);

    sco1 += co;
    sno1 += no;
    len1_sum += centdist;
    sco1_len++;

    if (blurvert.depth < steps) {
      sco2 += co;
      sno2 += no;
      len2_sum += centdist;
      sco2_len++;
    }

    if (blurvert.depth >= steps) {
      continue;
    }

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, v, ni) {
      PBVHVertRef v2 = ni.vertex;

      if (visit.contains(v2.i)) {
        continue;
      }

      float dist = len_v3v3(SCULPT_vertex_co_get(ss, v2), SCULPT_vertex_co_get(ss, v));

      visit.add_new(v2.i);
      CavityBlurVert blurvert2(v2, dist, blurvert.depth + 1);

      int nextend = (end + 1) % queue.size();

      if (nextend == start) {
        int oldsize = queue.size();

        queue.resize(queue.size() << 1);

        if (end < start) {
          int n = oldsize - start;

          for (int i = 0; i < n; i++) {
            queue[queue.size() - n + i] = queue[i + start];
          }

          start = queue.size() - n;
        }
      }

      queue[end] = blurvert2;
      end = (end + 1) % queue.size();
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  }

  BLI_assert(sco1_len != sco2_len);

  if (!sco1_len) {
    sco1 = SCULPT_vertex_co_get(ss, vertex);
  }
  else {
    sco1 /= (float)sco1_len;
    len1_sum /= sco1_len;
  }

  if (!sco2_len) {
    sco2 = SCULPT_vertex_co_get(ss, vertex);
  }
  else {
    sco2 /= (float)sco2_len;
    len2_sum /= sco2_len;
  }

  normalize_v3(sno1);
  if (dot_v3v3(sno1, sno1) == 0.0f) {
    SCULPT_vertex_normal_get(ss, vertex, sno1);
  }

  normalize_v3(sno2);
  if (dot_v3v3(sno2, sno2) == 0.0f) {
    SCULPT_vertex_normal_get(ss, vertex, sno2);
  }

  float3 vec = sco1 - sco2;
  float factor_sum = dot_v3v3(vec, sno2) / len1_sum;

  factor_sum = sculpt_cavity_calc_factor(automasking, factor_sum);

  *(float *)SCULPT_vertex_attr_get(vertex, ss->attrs.cavity) = factor_sum;
  *(uchar *)SCULPT_vertex_attr_get(vertex, ss->attrs.stroke_id) = automasking->cavity_stroke_id;
}

int SCULPT_automasking_settings_hash(Object *ob, AutomaskingCache *automasking)
{
  SculptSession *ss = ob->sculpt;

  int hash;
  int totvert = SCULPT_vertex_count_get(ss);

  hash = BLI_hash_int(automasking->settings.flags);
  hash = BLI_hash_int_2d(hash, totvert);

  if (automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
    hash = BLI_hash_int_2d(hash, automasking->settings.cavity_blur_steps);
    hash = BLI_hash_int_2d(hash, *reinterpret_cast<uint *>(&automasking->settings.cavity_factor));

    if (automasking->settings.cavity_curve) {
      CurveMap *cm = automasking->settings.cavity_curve->cm;

      for (int i = 0; i < cm->totpoint; i++) {
        hash = BLI_hash_int_2d(hash, *reinterpret_cast<uint *>(&cm->curve[i].x));
        hash = BLI_hash_int_2d(hash, *reinterpret_cast<uint *>(&cm->curve[i].y));
        hash = BLI_hash_int_2d(hash, (uint)cm->curve[i].flag);
        hash = BLI_hash_int_2d(hash, (uint)cm->curve[i].shorty);
      }
    }
  }

  if (automasking->settings.flags & BRUSH_AUTOMASKING_FACE_SETS) {
    hash = BLI_hash_int_2d(hash, automasking->settings.initial_face_set);
  }

  return hash;
}

static float sculpt_automasking_cavity_factor(AutomaskingCache *automasking,
                                              SculptSession *ss,
                                              PBVHVertRef vertex)
{
  uchar stroke_id = *(uchar *)SCULPT_vertex_attr_get(vertex, ss->attrs.stroke_id);

  if (stroke_id != automasking->cavity_stroke_id) {
    sculpt_calc_blurred_cavity(ss, automasking, automasking->settings.cavity_blur_steps, vertex);
  }

  float factor = *(float *)SCULPT_vertex_attr_get(vertex, ss->attrs.cavity);
  bool inverted = automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_INVERTED;

  if ((automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) &&
      (automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_USE_CURVE)) {
    factor = inverted ? 1.0f - factor : factor;
    factor = BKE_curvemapping_evaluateF(automasking->settings.cavity_curve, 0, factor);
    factor = inverted ? 1.0f - factor : factor;
  }

  return factor;
}

float SCULPT_automasking_factor_get(AutomaskingCache *automasking,
                                    SculptSession *ss,
                                    PBVHVertRef vert)
{
  if (!automasking) {
    return 1.0f;
  }

  /* If the cache is initialized with valid info, use the cache. This is used when the
   * automasking information can't be computed in real time per vertex and needs to be
   * initialized for the whole mesh when the stroke starts. */
  if (ss->attrs.automasking_factor) {
    float factor = *(float *)SCULPT_vertex_attr_get(vert, ss->attrs.automasking_factor);

    if (automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
      factor *= sculpt_automasking_cavity_factor(automasking, ss, vert);
    }

    return factor;
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

  if (automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
    return sculpt_automasking_cavity_factor(automasking, ss, vert);
  }

  return 1.0f;
}

void SCULPT_automasking_cache_free(AutomaskingCache *automasking)
{
  if (!automasking) {
    return;
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

struct AutomaskFloodFillData {
  float radius;
  bool use_radius;
  float location[3];
  char symm;
};

static bool automask_floodfill_cb(SculptSession *ss,
                                  PBVHVertRef from_v,
                                  PBVHVertRef to_v,
                                  bool UNUSED(is_duplicate),
                                  void *userdata)
{
  AutomaskFloodFillData *data = (AutomaskFloodFillData *)userdata;

  *(float *)SCULPT_vertex_attr_get(to_v, ss->attrs.automasking_factor) = 1.0f;
  *(float *)SCULPT_vertex_attr_get(from_v, ss->attrs.automasking_factor) = 1.0f;
  return (!data->use_radius ||
          SCULPT_is_vertex_inside_brush_radius_symm(
              SCULPT_vertex_co_get(ss, to_v), data->location, data->radius, data->symm));
}

static void SCULPT_topology_automasking_init(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && !ss->pmap) {
    BLI_assert_unreachable();
    return;
  }

  const int totvert = SCULPT_vertex_count_get(ss);
  for (int i : IndexRange(totvert)) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    (*(float *)SCULPT_vertex_attr_get(vertex, ss->attrs.automasking_factor)) = 0.0f;
  }

  /* Flood fill automask to connected vertices. Limited to vertices inside
   * the brush radius if the tool requires it. */
  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  const float radius = ss->cache ? ss->cache->radius : FLT_MAX;
  SCULPT_floodfill_add_active(sd, ob, ss, &flood, radius);

  AutomaskFloodFillData fdata = {0};

  fdata.radius = radius;
  fdata.use_radius = ss->cache && sculpt_automasking_is_constrained_by_radius(brush);
  fdata.symm = SCULPT_mesh_symmetry_xyz_get(ob);

  copy_v3_v3(fdata.location, SCULPT_active_vertex_co_get(ss));
  SCULPT_floodfill_execute(ss, &flood, automask_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);
}

static void sculpt_face_sets_automasking_init(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (!SCULPT_is_automasking_enabled(sd, ss, brush)) {
    return;
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && !ss->pmap) {
    BLI_assert_msg(0, "Face Sets automasking: pmap missing");
    return;
  }

  int tot_vert = SCULPT_vertex_count_get(ss);
  int active_face_set = SCULPT_active_face_set_get(ss);
  for (int i : IndexRange(tot_vert)) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    if (!SCULPT_vertex_has_face_set(ss, vertex, active_face_set)) {
      *(float *)SCULPT_vertex_attr_get(vertex, ss->attrs.automasking_factor) = 0.0f;
    }
  }
}

#define EDGE_DISTANCE_INF -1

static void SCULPT_boundary_automasking_init(Object *ob,
                                             eBoundaryAutomaskMode mode,
                                             int propagation_steps)
{
  SculptSession *ss = ob->sculpt;

  if (!ss->pmap) {
    BLI_assert_msg(0, "Boundary Edges masking: pmap missing");
    return;
  }

  const int totvert = SCULPT_vertex_count_get(ss);
  int *edge_distance = (int *)MEM_callocN(sizeof(int) * totvert, "automask_factor");

  for (int i : IndexRange(totvert)) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

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

  for (int propagation_it : IndexRange(propagation_steps)) {
    for (int i : IndexRange(totvert)) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      if (edge_distance[i] != EDGE_DISTANCE_INF) {
        continue;
      }
      SculptVertexNeighborIter ni;
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
        if (edge_distance[ni.index] == propagation_it) {
          edge_distance[i] = propagation_it + 1;
        }
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
    }
  }

  for (int i : IndexRange(totvert)) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    if (edge_distance[i] == EDGE_DISTANCE_INF) {
      continue;
    }
    const float p = 1.0f - (float(edge_distance[i]) / float(propagation_steps));
    const float edge_boundary_automask = pow2f(p);

    *(float *)SCULPT_vertex_attr_get(
        vertex, ss->attrs.automasking_factor) *= (1.0f - edge_boundary_automask);
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

  if (brush && (brush->automasking_flags & BRUSH_AUTOMASKING_CAVITY_ALL)) {
    automasking->settings.cavity_curve = brush->automasking_cavity_curve;
    automasking->settings.cavity_factor = brush->automasking_cavity_factor;
    automasking->settings.cavity_blur_steps = brush->automasking_cavity_blur_steps;
  }
  else {
    automasking->settings.cavity_curve = sd->automasking_cavity_curve;
    automasking->settings.cavity_factor = sd->automasking_cavity_factor;
    automasking->settings.cavity_blur_steps = sd->automasking_cavity_blur_steps;
  }
}

bool SCULPT_tool_can_reuse_cavity_mask(int sculpt_tool)
{
  return ELEM(sculpt_tool,
              SCULPT_TOOL_PAINT,
              SCULPT_TOOL_SMEAR,
              SCULPT_TOOL_MASK,
              SCULPT_TOOL_DRAW_FACE_SETS);
}

AutomaskingCache *SCULPT_automasking_cache_init(Sculpt *sd, Brush *brush, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  if (!SCULPT_is_automasking_enabled(sd, ss, brush)) {
    return nullptr;
  }

  AutomaskingCache *automasking = (AutomaskingCache *)MEM_callocN(sizeof(AutomaskingCache),
                                                                  "automasking cache");
  SCULPT_automasking_cache_settings_update(automasking, ss, sd, brush);
  SCULPT_boundary_info_ensure(ob);

  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_CAVITY_ALL)) {
    if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_CAVITY_USE_CURVE)) {
      BKE_curvemapping_init(brush->automasking_cavity_curve);
      BKE_curvemapping_init(sd->automasking_cavity_curve);
    }

    SCULPT_stroke_id_ensure(ob);
    automasking->cavity_stroke_id = ss->stroke_id;

    if (!ss->attrs.cavity) {
      SculptAttributeParams params = {0};
      ss->attrs.cavity = BKE_sculpt_attribute_ensure(
          ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT, SCULPT_ATTRIBUTE_NAME(cavity), &params);
    }
    /* Can we reuse the previous stroke's cavity mask? */
    else if (brush && SCULPT_tool_can_reuse_cavity_mask(brush->sculpt_tool)) {
      int hash = SCULPT_automasking_settings_hash(ob, automasking);

      if (hash == ss->last_automasking_settings_hash) {
        automasking->cavity_stroke_id = ss->last_automasking_settings_hash;
        automasking->can_reuse_cavity = true;
      }
    }

    if (!automasking->can_reuse_cavity) {
      ss->last_cavity_stroke_id = ss->stroke_id;
    }
  }

  if (!SCULPT_automasking_needs_factors_cache(sd, brush)) {
    return automasking;
  }

  SculptAttributeParams params = {0};
  params.stroke_only = true;

  ss->attrs.automasking_factor = BKE_sculpt_attribute_ensure(
      ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT, SCULPT_ATTRIBUTE_NAME(automasking_factor), &params);

  for (int i : IndexRange(totvert)) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    (*(float *)SCULPT_vertex_attr_get(vertex, ss->attrs.automasking_factor)) = 0.0f;
  }

  const int boundary_propagation_steps = brush ?
                                             brush->automasking_boundary_edges_propagation_steps :
                                             1;

  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_TOPOLOGY)) {
    SCULPT_vertex_random_access_ensure(ss);
    SCULPT_topology_automasking_init(sd, ob);
  }
  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_FACE_SETS)) {
    SCULPT_vertex_random_access_ensure(ss);
    sculpt_face_sets_automasking_init(sd, ob);
  }

  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_BOUNDARY_EDGES)) {
    SCULPT_vertex_random_access_ensure(ss);
    SCULPT_boundary_automasking_init(ob, AUTOMASK_INIT_BOUNDARY_EDGES, boundary_propagation_steps);
  }
  if (SCULPT_is_automasking_mode_enabled(sd, brush, BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS)) {
    SCULPT_vertex_random_access_ensure(ss);
    SCULPT_boundary_automasking_init(
        ob, AUTOMASK_INIT_BOUNDARY_FACE_SETS, boundary_propagation_steps);
  }

  return automasking;
}

bool SCULPT_automasking_needs_original(const Sculpt *sd, const Brush *brush)
{
  return sculpt_automasking_mode_effective_bits(sd, brush) & BRUSH_AUTOMASKING_CAVITY_ALL;
}
