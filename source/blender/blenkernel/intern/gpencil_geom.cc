/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation. */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_heap.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_polyfill_2d.h"
#include "BLI_span.hh"

#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLT_translation.h"

#include "BKE_attribute.hh"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_curve.h"
#include "BKE_gpencil_geom.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

#include "DEG_depsgraph_query.h"

using blender::float3;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name Grease Pencil Object: Bound-box Support
 * \{ */

bool BKE_gpencil_stroke_minmax(const bGPDstroke *gps,
                               const bool use_select,
                               float r_min[3],
                               float r_max[3])
{
  if (gps == nullptr) {
    return false;
  }

  bool changed = false;
  if (use_select) {
    for (const bGPDspoint &pt : Span(gps->points, gps->totpoints)) {
      if (pt.flag & GP_SPOINT_SELECT) {
        minmax_v3v3_v3(r_min, r_max, &pt.x);
        changed = true;
      }
    }
  }
  else {
    for (const bGPDspoint &pt : Span(gps->points, gps->totpoints)) {
      minmax_v3v3_v3(r_min, r_max, &pt.x);
      changed = true;
    }
  }

  return changed;
}

bool BKE_gpencil_data_minmax(const bGPdata *gpd, float r_min[3], float r_max[3])
{
  bool changed = false;

  INIT_MINMAX(r_min, r_max);

  if (gpd == nullptr) {
    return changed;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = gpl->actframe;

    if (gpf != nullptr) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        changed |= BKE_gpencil_stroke_minmax(gps, false, r_min, r_max);
      }
    }
  }

  return changed;
}

void BKE_gpencil_centroid_3d(bGPdata *gpd, float r_centroid[3])
{
  float3 min;
  float3 max;
  BKE_gpencil_data_minmax(gpd, min, max);

  const float3 tot = min + max;
  mul_v3_v3fl(r_centroid, tot, 0.5f);
}

void BKE_gpencil_stroke_boundingbox_calc(bGPDstroke *gps)
{
  INIT_MINMAX(gps->boundbox_min, gps->boundbox_max);
  BKE_gpencil_stroke_minmax(gps, false, gps->boundbox_min, gps->boundbox_max);
}

/**
 * Create bounding box values.
 * \param ob: Grease pencil object
 */
static void boundbox_gpencil(Object *ob)
{
  if (ob->runtime.bb == nullptr) {
    ob->runtime.bb = MEM_cnew<BoundBox>("GPencil boundbox");
  }

  BoundBox *bb = ob->runtime.bb;
  bGPdata *gpd = (bGPdata *)ob->data;

  float3 min;
  float3 max;
  if (!BKE_gpencil_data_minmax(gpd, min, max)) {
    min = float3(-1);
    max = float3(1);
  }

  BKE_boundbox_init_from_minmax(bb, min, max);

  bb->flag &= ~BOUNDBOX_DIRTY;
}

BoundBox *BKE_gpencil_boundbox_get(Object *ob)
{
  if (ELEM(nullptr, ob, ob->data)) {
    return nullptr;
  }

  bGPdata *gpd = (bGPdata *)ob->data;
  if ((ob->runtime.bb) && ((gpd->flag & GP_DATA_CACHE_IS_DIRTY) == 0)) {
    return ob->runtime.bb;
  }

  boundbox_gpencil(ob);

  Object *ob_orig = (Object *)DEG_get_original_id(&ob->id);
  /* Update orig object's boundbox with re-computed evaluated values. This function can be
   * called with the evaluated object and need update the original object bound box data
   * to keep both values synchronized. */
  if (!ELEM(ob_orig, nullptr, ob)) {
    if (ob_orig->runtime.bb == nullptr) {
      ob_orig->runtime.bb = MEM_cnew<BoundBox>("GPencil boundbox");
    }
    for (int i = 0; i < 8; i++) {
      copy_v3_v3(ob_orig->runtime.bb->vec[i], ob->runtime.bb->vec[i]);
    }
  }

  return ob->runtime.bb;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Sample
 * \{ */

static int stroke_march_next_point(const bGPDstroke *gps,
                                   const int index_next_pt,
                                   const float *current,
                                   const float dist,
                                   float *result,
                                   float *pressure,
                                   float *strength,
                                   float *vert_color,
                                   float *uv_fac,
                                   float *uv_fill,
                                   float *uv_rot,
                                   float *ratio_result,
                                   int *index_from,
                                   int *index_to)
{
  float remaining_till_next = 0.0f;
  float remaining_march = dist;
  float step_start[3];
  float point[3];
  int next_point_index = index_next_pt;
  bGPDspoint *pt = nullptr;

  if (next_point_index == gps->totpoints) {
    next_point_index = 0;
  }

  copy_v3_v3(step_start, current);
  pt = &gps->points[next_point_index];
  copy_v3_v3(point, &pt->x);
  remaining_till_next = len_v3v3(point, step_start);

  while (remaining_till_next < remaining_march && next_point_index) {
    remaining_march -= remaining_till_next;
    pt = &gps->points[next_point_index];
    if (pt->flag & GP_SPOINT_TEMP_TAG) {
      pt = &gps->points[next_point_index];
      copy_v3_v3(result, &pt->x);
      *pressure = gps->points[next_point_index].pressure;
      *strength = gps->points[next_point_index].strength;
      memcpy(vert_color, gps->points[next_point_index].vert_color, sizeof(float[4]));

      *index_from = next_point_index == 0 ? (gps->totpoints - 1) : (next_point_index - 1);
      *index_to = next_point_index;
      *ratio_result = 1.0f;
      next_point_index++;
      return next_point_index == 0 ? gps->totpoints : next_point_index;
    }
    next_point_index++;
    copy_v3_v3(point, &pt->x);
    copy_v3_v3(step_start, point);
    if (!(next_point_index < gps->totpoints)) {
      if (gps->flag & GP_STROKE_CYCLIC) {
        next_point_index = 0;
      }
      else {
        next_point_index = gps->totpoints - 1;
        remaining_till_next = 0;
        break;
      }
    }
    pt = &gps->points[next_point_index];
    copy_v3_v3(point, &pt->x);
    remaining_till_next = len_v3v3(point, step_start);
  }
  if (remaining_till_next < remaining_march) {
    pt = &gps->points[next_point_index];
    copy_v3_v3(result, &pt->x);
    *pressure = gps->points[next_point_index].pressure;
    *strength = gps->points[next_point_index].strength;
    memcpy(vert_color, gps->points[next_point_index].vert_color, sizeof(float[4]));

    *index_from = next_point_index == 0 ? (gps->totpoints - 1) : (next_point_index - 1);
    *index_to = next_point_index;
    *ratio_result = 1.0f;

    return 0;
  }

  *index_from = next_point_index == 0 ? (gps->totpoints - 1) : (next_point_index - 1);
  *index_to = next_point_index;

  float ratio = remaining_march / remaining_till_next;
  interp_v3_v3v3(result, step_start, point, ratio);
  *ratio_result = ratio;
  float d1 = len_v3v3(result, &gps->points[*index_from].x);
  float d2 = len_v3v3(result, &gps->points[next_point_index].x);
  float vratio = d1 / (d1 + d2);

  *pressure = interpf(
      gps->points[next_point_index].pressure, gps->points[*index_from].pressure, vratio);
  *strength = interpf(
      gps->points[next_point_index].strength, gps->points[*index_from].strength, vratio);
  *uv_fac = interpf(gps->points[next_point_index].uv_fac, gps->points[*index_from].uv_fac, vratio);
  *uv_rot = interpf(gps->points[next_point_index].uv_rot, gps->points[*index_from].uv_rot, vratio);
  interp_v2_v2v2(
      uv_fill, gps->points[*index_from].uv_fill, gps->points[next_point_index].uv_fill, vratio);
  interp_v4_v4v4(vert_color,
                 gps->points[*index_from].vert_color,
                 gps->points[next_point_index].vert_color,
                 vratio);

  return next_point_index == 0 ? gps->totpoints : next_point_index;
}

static int stroke_march_next_point_no_interp(const bGPDstroke *gps,
                                             const int index_next_pt,
                                             const float *current,
                                             const float dist,
                                             const float sharp_threshold,
                                             float *result)
{
  float remaining_till_next = 0.0f;
  float remaining_march = dist;
  float step_start[3];
  float point[3];
  int next_point_index = index_next_pt;
  bGPDspoint *pt = nullptr;

  if (next_point_index == gps->totpoints) {
    next_point_index = 0;
  }

  copy_v3_v3(step_start, current);
  pt = &gps->points[next_point_index];
  copy_v3_v3(point, &pt->x);
  remaining_till_next = len_v3v3(point, step_start);

  while (remaining_till_next < remaining_march && next_point_index) {
    remaining_march -= remaining_till_next;
    pt = &gps->points[next_point_index];
    if (next_point_index < gps->totpoints - 1 &&
        angle_v3v3v3(&gps->points[next_point_index - 1].x,
                     &gps->points[next_point_index].x,
                     &gps->points[next_point_index + 1].x) < sharp_threshold) {
      copy_v3_v3(result, &pt->x);
      pt->flag |= GP_SPOINT_TEMP_TAG;
      next_point_index++;
      return next_point_index == 0 ? gps->totpoints : next_point_index;
    }
    next_point_index++;
    copy_v3_v3(point, &pt->x);
    copy_v3_v3(step_start, point);
    if (!(next_point_index < gps->totpoints)) {
      if (gps->flag & GP_STROKE_CYCLIC) {
        next_point_index = 0;
      }
      else {
        next_point_index = gps->totpoints - 1;
        remaining_till_next = 0;
        break;
      }
    }
    pt = &gps->points[next_point_index];
    copy_v3_v3(point, &pt->x);
    remaining_till_next = len_v3v3(point, step_start);
  }
  if (remaining_till_next < remaining_march) {
    pt = &gps->points[next_point_index];
    copy_v3_v3(result, &pt->x);
    /* Stroke marching only terminates here. */
    return 0;
  }

  float ratio = remaining_march / remaining_till_next;
  interp_v3_v3v3(result, step_start, point, ratio);
  return next_point_index == 0 ? gps->totpoints : next_point_index;
}

static int stroke_march_count(const bGPDstroke *gps, const float dist, const float sharp_threshold)
{
  int point_count = 0;
  float point[3];
  int next_point_index = 1;
  bGPDspoint *pt = nullptr;

  pt = &gps->points[0];
  copy_v3_v3(point, &pt->x);
  point_count++;

  /* Sharp points will be tagged by the stroke_march_next_point_no_interp() call below. */
  for (int i = 0; i < gps->totpoints; i++) {
    gps->points[i].flag &= (~GP_SPOINT_TEMP_TAG);
  }

  while ((next_point_index = stroke_march_next_point_no_interp(
              gps, next_point_index, point, dist, sharp_threshold, point)) > -1) {
    point_count++;
    if (next_point_index == 0) {
      break; /* last point finished */
    }
  }
  return point_count;
}

static void stroke_defvert_create_nr_list(MDeformVert *dv_list,
                                          int count,
                                          ListBase *result,
                                          int *totweight)
{
  LinkData *ld;
  MDeformVert *dv;
  MDeformWeight *dw;
  int i, j;
  int tw = 0;
  for (i = 0; i < count; i++) {
    dv = &dv_list[i];

    /* find def_nr in list, if not exist, then create one */
    for (j = 0; j < dv->totweight; j++) {
      bool found = false;
      dw = &dv->dw[j];
      for (ld = (LinkData *)result->first; ld; ld = ld->next) {
        if (ld->data == POINTER_FROM_INT(dw->def_nr)) {
          found = true;
          break;
        }
      }
      if (!found) {
        ld = MEM_cnew<LinkData>("def_nr_item");
        ld->data = POINTER_FROM_INT(dw->def_nr);
        BLI_addtail(result, ld);
        tw++;
      }
    }
  }

  *totweight = tw;
}

static MDeformVert *stroke_defvert_new_count(int count, int totweight, ListBase *def_nr_list)
{
  int i, j;
  LinkData *ld;
  MDeformVert *dst = (MDeformVert *)MEM_mallocN(count * sizeof(MDeformVert), "new_deformVert");

  for (i = 0; i < count; i++) {
    dst[i].dw = (MDeformWeight *)MEM_mallocN(sizeof(MDeformWeight) * totweight,
                                             "new_deformWeight");
    dst[i].totweight = totweight;
    j = 0;
    /* re-assign deform groups */
    for (ld = (LinkData *)def_nr_list->first; ld; ld = ld->next) {
      dst[i].dw[j].def_nr = POINTER_AS_INT(ld->data);
      j++;
    }
  }

  return dst;
}

static void stroke_interpolate_deform_weights(
    bGPDstroke *gps, int index_from, int index_to, float ratio, MDeformVert *vert)
{
  const MDeformVert *vl = &gps->dvert[index_from];
  const MDeformVert *vr = &gps->dvert[index_to];

  for (int i = 0; i < vert->totweight; i++) {
    float wl = BKE_defvert_find_weight(vl, vert->dw[i].def_nr);
    float wr = BKE_defvert_find_weight(vr, vert->dw[i].def_nr);
    vert->dw[i].weight = interpf(wr, wl, ratio);
  }
}

bool BKE_gpencil_stroke_sample(bGPdata *gpd,
                               bGPDstroke *gps,
                               const float dist,
                               const bool select,
                               const float sharp_threshold)
{
  bGPDspoint *pt = gps->points;
  bGPDspoint *pt1 = nullptr;
  bGPDspoint *pt2 = nullptr;
  LinkData *ld;
  ListBase def_nr_list = {nullptr};

  if (gps->totpoints < 2 || dist < FLT_EPSILON) {
    return false;
  }
  /* TODO: Implement feature point preservation. */
  int count = stroke_march_count(gps, dist, sharp_threshold);
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;
  if (is_cyclic) {
    count--;
  }

  bGPDspoint *new_pt = (bGPDspoint *)MEM_callocN(sizeof(bGPDspoint) * count,
                                                 "gp_stroke_points_sampled");
  MDeformVert *new_dv = nullptr;

  int result_totweight;

  if (gps->dvert != nullptr) {
    stroke_defvert_create_nr_list(gps->dvert, gps->totpoints, &def_nr_list, &result_totweight);
    new_dv = stroke_defvert_new_count(count, result_totweight, &def_nr_list);
  }

  int next_point_index = 1;
  int i = 0;
  float pressure, strength, ratio_result;
  float uv_fac, uv_rot, uv_fill[2];
  float vert_color[4];
  int index_from, index_to;
  float last_coord[3];

  /*  1st point is always at the start */
  pt1 = &gps->points[0];
  copy_v3_v3(last_coord, &pt1->x);
  pt2 = &new_pt[i];
  copy_v3_v3(&pt2->x, last_coord);
  new_pt[i].pressure = pt[0].pressure;
  new_pt[i].strength = pt[0].strength;
  memcpy(new_pt[i].vert_color, pt[0].vert_color, sizeof(float[4]));
  if (select) {
    new_pt[i].flag |= GP_SPOINT_SELECT;
  }
  i++;

  if (new_dv) {
    stroke_interpolate_deform_weights(gps, 0, 0, 0, &new_dv[0]);
  }

  /* The rest. */
  while ((next_point_index = stroke_march_next_point(gps,
                                                     next_point_index,
                                                     last_coord,
                                                     dist,
                                                     last_coord,
                                                     &pressure,
                                                     &strength,
                                                     vert_color,
                                                     &uv_fac,
                                                     uv_fill,
                                                     &uv_rot,
                                                     &ratio_result,
                                                     &index_from,
                                                     &index_to)) > -1) {
    if (is_cyclic && next_point_index == 0) {
      break; /* last point finished */
    }
    pt2 = &new_pt[i];
    copy_v3_v3(&pt2->x, last_coord);
    new_pt[i].pressure = pressure;
    new_pt[i].strength = strength;
    new_pt[i].uv_fac = uv_fac;
    new_pt[i].uv_rot = uv_rot;
    copy_v2_v2(new_pt[i].uv_fill, uv_fill);

    memcpy(new_pt[i].vert_color, vert_color, sizeof(float[4]));
    if (select) {
      new_pt[i].flag |= GP_SPOINT_SELECT;
    }

    if (new_dv) {
      stroke_interpolate_deform_weights(gps, index_from, index_to, ratio_result, &new_dv[i]);
    }

    i++;
    if (next_point_index == 0) {
      break; /* last point finished */
    }
  }

  gps->points = new_pt;
  /* Free original vertex list. */
  MEM_freeN(pt);

  if (new_dv) {
    /* Free original weight data. */
    BKE_gpencil_free_stroke_weights(gps);
    MEM_freeN(gps->dvert);
    while ((ld = (LinkData *)BLI_pophead(&def_nr_list))) {
      MEM_freeN(ld);
    }

    gps->dvert = new_dv;
  }

  BLI_assert(i == count);
  gps->totpoints = i;

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);

  return true;
}

/**
 * Give extra stroke points before and after the original tip points.
 * \param gps: Target stroke
 * \param count_before: how many extra points to be added before a stroke
 * \param count_after: how many extra points to be added after a stroke
 */
static bool BKE_gpencil_stroke_extra_points(bGPDstroke *gps,
                                            const int count_before,
                                            const int count_after)
{
  bGPDspoint *pts = gps->points;

  BLI_assert(count_before >= 0);
  BLI_assert(count_after >= 0);
  if (!count_before && !count_after) {
    return false;
  }

  const int new_count = count_before + count_after + gps->totpoints;

  bGPDspoint *new_pts = (bGPDspoint *)MEM_mallocN(sizeof(bGPDspoint) * new_count, __func__);

  for (int i = 0; i < count_before; i++) {
    new_pts[i] = blender::dna::shallow_copy(pts[0]);
  }
  memcpy(static_cast<void *>(&new_pts[count_before]), pts, sizeof(bGPDspoint) * gps->totpoints);
  for (int i = new_count - count_after; i < new_count; i++) {
    new_pts[i] = blender::dna::shallow_copy(pts[gps->totpoints - 1]);
  }

  if (gps->dvert) {
    MDeformVert *new_dv = (MDeformVert *)MEM_mallocN(sizeof(MDeformVert) * new_count, __func__);

    for (int i = 0; i < new_count; i++) {
      MDeformVert *dv = &gps->dvert[CLAMPIS(i - count_before, 0, gps->totpoints - 1)];
      int inew = i;
      new_dv[inew].flag = dv->flag;
      new_dv[inew].totweight = dv->totweight;
      new_dv[inew].dw = (MDeformWeight *)MEM_mallocN(sizeof(MDeformWeight) * dv->totweight,
                                                     __func__);
      memcpy(new_dv[inew].dw, dv->dw, sizeof(MDeformWeight) * dv->totweight);
    }
    BKE_gpencil_free_stroke_weights(gps);
    MEM_freeN(gps->dvert);
    gps->dvert = new_dv;
  }

  MEM_freeN(gps->points);
  gps->points = new_pts;
  gps->totpoints = new_count;

  return true;
}

bool BKE_gpencil_stroke_stretch(bGPDstroke *gps,
                                const float dist,
                                const float overshoot_fac,
                                const short mode,
                                const bool follow_curvature,
                                const int extra_point_count,
                                const float segment_influence,
                                const float max_angle,
                                const bool invert_curvature)
{
#define BOTH 0
#define START 1
#define END 2

  const bool do_start = ELEM(mode, BOTH, START);
  const bool do_end = ELEM(mode, BOTH, END);
  float used_percent_length = overshoot_fac;
  CLAMP(used_percent_length, 1e-4f, 1.0f);
  if (!isfinite(used_percent_length)) {
    /* #used_percent_length must always be finite, otherwise a segfault occurs.
     * Since this function should never segfault, set #used_percent_length to a safe fallback. */
    /* NOTE: This fallback is used if gps->totpoints == 2, see MOD_gpencillength.c */
    used_percent_length = 0.1f;
  }

  if (gps->totpoints <= 1 || dist < FLT_EPSILON || extra_point_count <= 0) {
    return false;
  }

  /* NOTE: When it's just a straight line, we don't need to do the curvature stuff. */
  if (!follow_curvature || gps->totpoints <= 2) {
    /* Not following curvature, just straight line. */
    /* NOTE: #overshoot_point_param can not be zero. */
    float overshoot_point_param = used_percent_length * (gps->totpoints - 1);
    float result[3];

    if (do_start) {
      int index1 = floor(overshoot_point_param);
      int index2 = ceil(overshoot_point_param);
      interp_v3_v3v3(result,
                     &gps->points[index1].x,
                     &gps->points[index2].x,
                     fmodf(overshoot_point_param, 1.0f));
      sub_v3_v3(result, &gps->points[0].x);
      if (UNLIKELY(is_zero_v3(result))) {
        sub_v3_v3v3(result, &gps->points[1].x, &gps->points[0].x);
      }
      madd_v3_v3fl(&gps->points[0].x, result, -dist / len_v3(result));
    }

    if (do_end) {
      int index1 = gps->totpoints - 1 - floor(overshoot_point_param);
      int index2 = gps->totpoints - 1 - ceil(overshoot_point_param);
      interp_v3_v3v3(result,
                     &gps->points[index1].x,
                     &gps->points[index2].x,
                     fmodf(overshoot_point_param, 1.0f));
      sub_v3_v3(result, &gps->points[gps->totpoints - 1].x);
      if (UNLIKELY(is_zero_v3(result))) {
        sub_v3_v3v3(
            result, &gps->points[gps->totpoints - 2].x, &gps->points[gps->totpoints - 1].x);
      }
      madd_v3_v3fl(&gps->points[gps->totpoints - 1].x, result, -dist / len_v3(result));
    }
    return true;
  }

  /* Curvature calculation. */

  /* First allocate the new stroke size. */
  const int first_old_index = do_start ? extra_point_count : 0;
  const int last_old_index = gps->totpoints - 1 + first_old_index;
  const int orig_totpoints = gps->totpoints;
  BKE_gpencil_stroke_extra_points(gps, first_old_index, do_end ? extra_point_count : 0);

  /* The fractional amount of points to query when calculating the average curvature of the
   * strokes. */
  const float overshoot_parameter = used_percent_length * (orig_totpoints - 2);
  int overshoot_pointcount = ceil(overshoot_parameter);
  CLAMP(overshoot_pointcount, 1, orig_totpoints - 2);

  /* Do for both sides without code duplication. */
  float no[3], vec1[3], vec2[3], total_angle[3];
  for (int k = 0; k < 2; k++) {
    if ((k == 0 && !do_start) || (k == 1 && !do_end)) {
      continue;
    }

    const int start_i = k == 0 ? first_old_index :
                                 last_old_index;  // first_old_index, last_old_index
    const int dir_i = 1 - k * 2;                  // 1, -1

    sub_v3_v3v3(vec1, &gps->points[start_i + dir_i].x, &gps->points[start_i].x);
    zero_v3(total_angle);
    float segment_length = normalize_v3(vec1);
    float overshoot_length = 0.0f;

    /* Accumulate rotation angle and length. */
    int j = 0;
    for (int i = start_i; j < overshoot_pointcount; i += dir_i, j++) {
      /* Don't fully add last segment to get continuity in overshoot_fac. */
      float fac = fmin(overshoot_parameter - j, 1.0f);

      /* Read segments. */
      copy_v3_v3(vec2, vec1);
      sub_v3_v3v3(vec1, &gps->points[i + dir_i * 2].x, &gps->points[i + dir_i].x);
      const float len = normalize_v3(vec1);
      float angle = angle_normalized_v3v3(vec1, vec2) * fac;

      /* Add half of both adjacent legs of the current angle. */
      const float added_len = (segment_length + len) * 0.5f * fac;
      overshoot_length += added_len;
      segment_length = len;

      if (angle > max_angle) {
        continue;
      }
      if (angle > M_PI * 0.995f) {
        continue;
      }

      angle *= powf(added_len, segment_influence);

      cross_v3_v3v3(no, vec1, vec2);
      normalize_v3_length(no, angle);
      add_v3_v3(total_angle, no);
    }

    if (UNLIKELY(overshoot_length == 0.0f)) {
      /* Don't do a proper extension if the used points are all in the same position. */
      continue;
    }

    sub_v3_v3v3(vec1, &gps->points[start_i].x, &gps->points[start_i + dir_i].x);
    /* In general curvature = 1/radius. For the case without the
     * weights introduced by #segment_influence, the calculation is:
     * `curvature = delta angle/delta arclength = len_v3(total_angle) / overshoot_length` */
    float curvature = normalize_v3(total_angle) / overshoot_length;
    /* Compensate for the weights powf(added_len, segment_influence). */
    curvature /= powf(overshoot_length / fminf(overshoot_parameter, float(j)), segment_influence);
    if (invert_curvature) {
      curvature = -curvature;
    }
    const float angle_step = curvature * dist / extra_point_count;
    float step_length = dist / extra_point_count;
    if (fabsf(angle_step) > FLT_EPSILON) {
      /* Make a direct step length from the assigned arc step length. */
      step_length *= sin(angle_step * 0.5f) / (angle_step * 0.5f);
    }
    else {
      zero_v3(total_angle);
    }
    const float prev_length = normalize_v3_length(vec1, step_length);

    /* Build rotation matrix here to get best performance. */
    float rot[3][3];
    float q[4];
    axis_angle_to_quat(q, total_angle, angle_step);
    quat_to_mat3(rot, q);

    /* Rotate the starting direction to account for change in edge lengths. */
    axis_angle_to_quat(q,
                       total_angle,
                       fmaxf(0.0f, 1.0f - fabs(segment_influence)) *
                           (curvature * prev_length - angle_step) / 2.0f);
    mul_qt_v3(q, vec1);

    /* Now iteratively accumulate the segments with a rotating added direction. */
    for (int i = start_i - dir_i, j = 0; j < extra_point_count; i -= dir_i, j++) {
      mul_v3_m3v3(vec1, rot, vec1);
      add_v3_v3v3(&gps->points[i].x, vec1, &gps->points[i + dir_i].x);
    }
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Trim
 * \{ */

bool BKE_gpencil_stroke_trim_points(bGPDstroke *gps,
                                    const int index_from,
                                    const int index_to,
                                    const bool keep_point)
{
  bGPDspoint *pt = gps->points, *new_pt;
  MDeformVert *dv, *new_dv;

  const int new_count = index_to - index_from + 1;

  if (new_count >= gps->totpoints) {
    return false;
  }

  if ((!keep_point) && (new_count == 1)) {
    if (gps->dvert) {
      BKE_gpencil_free_stroke_weights(gps);
      MEM_freeN(gps->dvert);
    }
    MEM_freeN(gps->points);
    gps->points = nullptr;
    gps->dvert = nullptr;
    gps->totpoints = 0;
    return false;
  }

  new_pt = (bGPDspoint *)MEM_mallocN(sizeof(bGPDspoint) * new_count, "gp_stroke_points_trimmed");
  memcpy(static_cast<void *>(new_pt), &pt[index_from], sizeof(bGPDspoint) * new_count);

  if (gps->dvert) {
    new_dv = (MDeformVert *)MEM_mallocN(sizeof(MDeformVert) * new_count,
                                        "gp_stroke_dverts_trimmed");
    for (int i = 0; i < new_count; i++) {
      dv = &gps->dvert[i + index_from];
      new_dv[i].flag = dv->flag;
      new_dv[i].totweight = dv->totweight;
      new_dv[i].dw = (MDeformWeight *)MEM_mallocN(sizeof(MDeformWeight) * dv->totweight,
                                                  "gp_stroke_dverts_dw_trimmed");
      for (int j = 0; j < dv->totweight; j++) {
        new_dv[i].dw[j].weight = dv->dw[j].weight;
        new_dv[i].dw[j].def_nr = dv->dw[j].def_nr;
      }
    }
    BKE_gpencil_free_stroke_weights(gps);
    MEM_freeN(gps->dvert);
    gps->dvert = new_dv;
  }

  MEM_freeN(gps->points);
  gps->points = new_pt;
  gps->totpoints = new_count;

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Split
 * \{ */

bool BKE_gpencil_stroke_split(bGPdata *gpd,
                              bGPDframe *gpf,
                              bGPDstroke *gps,
                              const int before_index,
                              bGPDstroke **remaining_gps)
{
  bGPDstroke *new_gps;
  bGPDspoint *pt = gps->points, *new_pt;
  MDeformVert *dv, *new_dv;

  if (before_index >= gps->totpoints || before_index == 0) {
    return false;
  }

  const int new_count = gps->totpoints - before_index;
  const int old_count = before_index;

  /* Handle remaining segments first. */

  new_gps = BKE_gpencil_stroke_add_existing_style(
      gpf, gps, gps->mat_nr, new_count, gps->thickness);

  new_pt = new_gps->points; /* Allocated from above. */
  memcpy(static_cast<void *>(new_pt), &pt[before_index], sizeof(bGPDspoint) * new_count);

  if (gps->dvert) {
    new_dv = (MDeformVert *)MEM_mallocN(sizeof(MDeformVert) * new_count,
                                        "gp_stroke_dverts_remaining(MDeformVert)");
    for (int i = 0; i < new_count; i++) {
      dv = &gps->dvert[i + before_index];
      new_dv[i].flag = dv->flag;
      new_dv[i].totweight = dv->totweight;
      new_dv[i].dw = (MDeformWeight *)MEM_mallocN(sizeof(MDeformWeight) * dv->totweight,
                                                  "gp_stroke_dverts_dw_remaining(MDeformWeight)");
      for (int j = 0; j < dv->totweight; j++) {
        new_dv[i].dw[j].weight = dv->dw[j].weight;
        new_dv[i].dw[j].def_nr = dv->dw[j].def_nr;
      }
    }
    new_gps->dvert = new_dv;
  }

  (*remaining_gps) = new_gps;

  /* Trim the original stroke into a shorter one.
   * Keep the end point. */

  BKE_gpencil_stroke_trim_points(gps, 0, old_count, false);
  BKE_gpencil_stroke_geometry_update(gpd, gps);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Shrink
 * \{ */

bool BKE_gpencil_stroke_shrink(bGPDstroke *gps, const float dist, const short mode)
{
#define START 1
#define END 2

  bGPDspoint *pt = gps->points, *second_last;
  int i;

  if (gps->totpoints < 2) {
    if (gps->totpoints == 1) {
      second_last = &pt[1];
      if (len_v3v3(&second_last->x, &pt->x) < dist) {
        BKE_gpencil_stroke_trim_points(gps, 0, 0, false);
        return true;
      }
    }

    return false;
  }

  second_last = &pt[gps->totpoints - 2];

  float len;
  float len1, cut_len1;
  float len2, cut_len2;
  len1 = len2 = cut_len1 = cut_len2 = 0.0f;

  int index_start = 0;
  int index_end = 0;
  if (mode == START) {
    i = 0;
    index_end = gps->totpoints - 1;
    while (len1 < dist && gps->totpoints > i + 1) {
      len = len_v3v3(&pt[i].x, &pt[i + 1].x);
      len1 += len;
      cut_len1 = len1 - dist;
      i++;
    }
    index_start = i - 1;
    interp_v3_v3v3(&pt[index_start].x, &pt[index_start + 1].x, &pt[index_start].x, cut_len1 / len);
  }

  if (mode == END) {
    index_start = 0;
    i = 2;
    while (len2 < dist && gps->totpoints >= i) {
      second_last = &pt[gps->totpoints - i];
      len = len_v3v3(&second_last[1].x, &second_last->x);
      len2 += len;
      cut_len2 = len2 - dist;
      i++;
    }
    index_end = gps->totpoints - i + 2;
    interp_v3_v3v3(&pt[index_end].x, &pt[index_end - 1].x, &pt[index_end].x, cut_len2 / len);
  }

  if (index_end <= index_start) {
    index_start = index_end = 0; /* empty stroke */
  }

  if ((index_end == index_start + 1) && (cut_len1 + cut_len2 < 0)) {
    index_start = index_end = 0; /* no length left to cut */
  }

  BKE_gpencil_stroke_trim_points(gps, index_start, index_end, false);

  if (gps->totpoints == 0) {
    return false;
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Smooth Positions
 * \{ */

bool BKE_gpencil_stroke_smooth_point(bGPDstroke *gps,
                                     int point_index,
                                     float influence,
                                     int iterations,
                                     const bool smooth_caps,
                                     const bool keep_shape,
                                     bGPDstroke *r_gps)
{
  /* If nothing to do, return early */
  if (gps->totpoints <= 2 || iterations <= 0) {
    return false;
  }

  /* Overview of the algorithm here and in the following smooth functions:
   *  The smooth functions return the new attribute in question for a single point.
   *  The result is stored in r_gps->points[point_index], while the data is read from gps.
   *  To get a correct result, duplicate the stroke point data and read from the copy,
   *  while writing to the real stroke. Not doing that will result in acceptable, but
   *  asymmetric results.
   * This algorithm works as long as all points are being smoothed. If there is
   * points that should not get smoothed, use the old repeat smooth pattern with
   * the parameter "iterations" set to 1 or 2. (2 matches the old algorithm).
   */

  const bGPDspoint *pt = &gps->points[point_index];
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;
  /* If smooth_caps is false, the caps will not be translated by smoothing. */
  if (!smooth_caps && !is_cyclic && ELEM(point_index, 0, gps->totpoints - 1)) {
    copy_v3_v3(&r_gps->points[point_index].x, &pt->x);
    return true;
  }

  /* This function uses a binomial kernel, which is the discrete version of gaussian blur.
   * The weight for a vertex at the relative index point_index is
   * w = nCr(n, j + n/2) / 2^n = (n/1 * (n-1)/2 * ... * (n-j-n/2)/(j+n/2)) / 2^n
   * All weights together sum up to 1
   * This is equivalent to doing multiple iterations of averaging neighbors,
   * where n = iterations * 2 and -n/2 <= j <= n/2
   *
   * Now the problem is that nCr(n, j + n/2) is very hard to compute for n > 500, since even
   * double precision isn't sufficient. A very good robust approximation for n > 20 is
   * nCr(n, j + n/2) / 2^n = sqrt(2/(pi*n)) * exp(-2*j*j/n)
   *
   * There is one more problem left: The old smooth algorithm was doing a more aggressive
   * smooth. To solve that problem, choose a different n/2, which does not match the range and
   * normalize the weights on finish. This may cause some artifacts at low values.
   *
   * keep_shape is a new option to stop the stroke from severely deforming.
   * It uses different partially negative weights.
   * w = 2 * (nCr(n, j + n/2) / 2^n) - (nCr(3*n, j + n) / 2^(3*n))
   *   ~ 2 * sqrt(2/(pi*n)) * exp(-2*j*j/n) - sqrt(2/(pi*3*n)) * exp(-2*j*j/(3*n))
   * All weights still sum up to 1.
   * Note these weights only work because the averaging is done in relative coordinates.
   */
  float sco[3] = {0.0f, 0.0f, 0.0f};
  float tmp[3];
  const int n_half = keep_shape ? (iterations * iterations) / 8 + iterations :
                                  (iterations * iterations) / 4 + 2 * iterations + 12;
  double w = keep_shape ? 2.0 : 1.0;
  double w2 = keep_shape ?
                  (1.0 / M_SQRT3) * exp((2 * iterations * iterations) / double(n_half * 3)) :
                  0.0;
  double total_w = 0.0;
  for (int step = iterations; step > 0; step--) {
    int before = point_index - step;
    int after = point_index + step;
    float w_before = float(w - w2);
    float w_after = float(w - w2);

    if (is_cyclic) {
      before = (before % gps->totpoints + gps->totpoints) % gps->totpoints;
      after = after % gps->totpoints;
    }
    else {
      if (before < 0) {
        if (!smooth_caps) {
          w_before *= -before / float(point_index);
        }
        before = 0;
      }
      if (after > gps->totpoints - 1) {
        if (!smooth_caps) {
          w_after *= (after - (gps->totpoints - 1)) / float(gps->totpoints - 1 - point_index);
        }
        after = gps->totpoints - 1;
      }
    }

    /* Add both these points in relative coordinates to the weighted average sum. */
    sub_v3_v3v3(tmp, &gps->points[before].x, &pt->x);
    madd_v3_v3fl(sco, tmp, w_before);
    sub_v3_v3v3(tmp, &gps->points[after].x, &pt->x);
    madd_v3_v3fl(sco, tmp, w_after);

    total_w += w_before;
    total_w += w_after;

    w *= (n_half + step) / double(n_half + 1 - step);
    w2 *= (n_half * 3 + step) / double(n_half * 3 + 1 - step);
  }
  total_w += w - w2;
  /* The accumulated weight total_w should be
   * ~sqrt(M_PI * n_half) * exp((iterations * iterations) / n_half) < 100
   * here, but sometimes not quite. */
  mul_v3_fl(sco, float(1.0 / total_w));
  /* Shift back to global coordinates. */
  add_v3_v3(sco, &pt->x);

  /* Based on influence factor, blend between original and optimal smoothed coordinate. */
  interp_v3_v3v3(&r_gps->points[point_index].x, &pt->x, sco, influence);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Smooth Strength
 * \{ */

bool BKE_gpencil_stroke_smooth_strength(
    bGPDstroke *gps, int point_index, float influence, int iterations, bGPDstroke *r_gps)
{
  /* If nothing to do, return early */
  if (gps->totpoints <= 2 || iterations <= 0) {
    return false;
  }

  /* See BKE_gpencil_stroke_smooth_point for details on the algorithm. */

  const bGPDspoint *pt = &gps->points[point_index];
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;
  float strength = 0.0f;
  const int n_half = (iterations * iterations) / 4 + iterations;
  double w = 1.0;
  double total_w = 0.0;
  for (int step = iterations; step > 0; step--) {
    int before = point_index - step;
    int after = point_index + step;
    float w_before = float(w);
    float w_after = float(w);

    if (is_cyclic) {
      before = (before % gps->totpoints + gps->totpoints) % gps->totpoints;
      after = after % gps->totpoints;
    }
    else {
      CLAMP_MIN(before, 0);
      CLAMP_MAX(after, gps->totpoints - 1);
    }

    /* Add both these points in relative coordinates to the weighted average sum. */
    strength += w_before * (gps->points[before].strength - pt->strength);
    strength += w_after * (gps->points[after].strength - pt->strength);

    total_w += w_before;
    total_w += w_after;

    w *= (n_half + step) / double(n_half + 1 - step);
  }
  total_w += w;
  /* The accumulated weight total_w should be
   * ~sqrt(M_PI * n_half) * exp((iterations * iterations) / n_half) < 100
   * here, but sometimes not quite. */
  strength /= total_w;

  /* Based on influence factor, blend between original and optimal smoothed value. */
  r_gps->points[point_index].strength = pt->strength + strength * influence;

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Smooth Thickness
 * \{ */

bool BKE_gpencil_stroke_smooth_thickness(
    bGPDstroke *gps, int point_index, float influence, int iterations, bGPDstroke *r_gps)
{
  /* If nothing to do, return early */
  if (gps->totpoints <= 2 || iterations <= 0) {
    return false;
  }

  /* See BKE_gpencil_stroke_smooth_point for details on the algorithm. */

  const bGPDspoint *pt = &gps->points[point_index];
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;
  float pressure = 0.0f;
  const int n_half = (iterations * iterations) / 4 + iterations;
  double w = 1.0;
  double total_w = 0.0;
  for (int step = iterations; step > 0; step--) {
    int before = point_index - step;
    int after = point_index + step;
    float w_before = float(w);
    float w_after = float(w);

    if (is_cyclic) {
      before = (before % gps->totpoints + gps->totpoints) % gps->totpoints;
      after = after % gps->totpoints;
    }
    else {
      CLAMP_MIN(before, 0);
      CLAMP_MAX(after, gps->totpoints - 1);
    }

    /* Add both these points in relative coordinates to the weighted average sum. */
    pressure += w_before * (gps->points[before].pressure - pt->pressure);
    pressure += w_after * (gps->points[after].pressure - pt->pressure);

    total_w += w_before;
    total_w += w_after;

    w *= (n_half + step) / double(n_half + 1 - step);
  }
  total_w += w;
  /* The accumulated weight total_w should be
   * ~sqrt(M_PI * n_half) * exp((iterations * iterations) / n_half) < 100
   * here, but sometimes not quite. */
  pressure /= total_w;

  /* Based on influence factor, blend between original and optimal smoothed value. */
  r_gps->points[point_index].pressure = pt->pressure + pressure * influence;

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Smooth UV
 * \{ */

bool BKE_gpencil_stroke_smooth_uv(struct bGPDstroke *gps,
                                  int point_index,
                                  float influence,
                                  int iterations,
                                  struct bGPDstroke *r_gps)
{
  /* If nothing to do, return early */
  if (gps->totpoints <= 2 || iterations <= 0) {
    return false;
  }

  /* See BKE_gpencil_stroke_smooth_point for details on the algorithm. */

  const bGPDspoint *pt = &gps->points[point_index];
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;

  /* If don't change the caps. */
  if (!is_cyclic && ELEM(point_index, 0, gps->totpoints - 1)) {
    r_gps->points[point_index].uv_rot = pt->uv_rot;
    r_gps->points[point_index].uv_fac = pt->uv_fac;
    return true;
  }

  float uv_rot = 0.0f;
  float uv_fac = 0.0f;
  const int n_half = iterations * iterations + iterations;
  double w = 1.0;
  double total_w = 0.0;
  for (int step = iterations; step > 0; step--) {
    int before = point_index - step;
    int after = point_index + step;
    float w_before = float(w);
    float w_after = float(w);

    if (is_cyclic) {
      before = (before % gps->totpoints + gps->totpoints) % gps->totpoints;
      after = after % gps->totpoints;
    }
    else {
      if (before < 0) {
        w_before *= -before / float(point_index);
        before = 0;
      }
      if (after > gps->totpoints - 1) {
        w_after *= (after - (gps->totpoints - 1)) / float(gps->totpoints - 1 - point_index);
        after = gps->totpoints - 1;
      }
    }

    /* Add both these points in relative coordinates to the weighted average sum. */
    uv_rot += w_before * (gps->points[before].uv_rot - pt->uv_rot);
    uv_rot += w_after * (gps->points[after].uv_rot - pt->uv_rot);
    uv_fac += w_before * (gps->points[before].uv_fac - pt->uv_fac);
    uv_fac += w_after * (gps->points[after].uv_fac - pt->uv_fac);

    total_w += w_before;
    total_w += w_after;

    w *= (n_half + step) / double(n_half + 1 - step);
  }
  total_w += w;
  /* The accumulated weight total_w should be
   * ~sqrt(M_PI * n_half) * exp((iterations * iterations) / n_half) < 100
   * here, but sometimes not quite. */
  uv_rot /= total_w;
  uv_fac /= total_w;

  /* Based on influence factor, blend between original and optimal smoothed value. */
  r_gps->points[point_index].uv_rot = pt->uv_rot + uv_rot * influence;
  r_gps->points[point_index].uv_fac = pt->uv_fac + uv_fac * influence;

  return true;
}

void BKE_gpencil_stroke_smooth(bGPDstroke *gps,
                               const float influence,
                               const int iterations,
                               const bool smooth_position,
                               const bool smooth_strength,
                               const bool smooth_thickness,
                               const bool smooth_uv,
                               const bool keep_shape,
                               const float *weights)
{
  if (influence <= 0 || iterations <= 0) {
    return;
  }

  /* Make a copy of the point data to avoid directionality of the smooth operation. */
  bGPDstroke gps_old = blender::dna::shallow_copy(*gps);
  gps_old.points = (bGPDspoint *)MEM_dupallocN(gps->points);

  /* Smooth stroke. */
  for (int i = 0; i < gps->totpoints; i++) {
    float val = influence;
    if (weights != nullptr) {
      val *= weights[i];
      if (val <= 0.0f) {
        continue;
      }
    }

    /* TODO: Currently the weights only control the influence, but is would be much better if they
     * would control the distribution used in smooth, similar to how the ends are handled. */

    /* Perform smoothing. */
    if (smooth_position) {
      BKE_gpencil_stroke_smooth_point(&gps_old, i, val, iterations, false, keep_shape, gps);
    }
    if (smooth_strength) {
      BKE_gpencil_stroke_smooth_strength(&gps_old, i, val, iterations, gps);
    }
    if (smooth_thickness) {
      BKE_gpencil_stroke_smooth_thickness(&gps_old, i, val, iterations, gps);
    }
    if (smooth_uv) {
      BKE_gpencil_stroke_smooth_uv(&gps_old, i, val, iterations, gps);
    }
  }

  /* Free the copied points array. */
  MEM_freeN(gps_old.points);
}

void BKE_gpencil_stroke_2d_flat(const bGPDspoint *points,
                                int totpoints,
                                float (*points2d)[2],
                                int *r_direction)
{
  BLI_assert(totpoints >= 2);

  const bGPDspoint *pt0 = &points[0];
  const bGPDspoint *pt1 = &points[1];
  const bGPDspoint *pt3 = &points[int(totpoints * 0.75)];

  float locx[3];
  float locy[3];
  float loc3[3];
  float normal[3];

  /* local X axis (p0 -> p1) */
  sub_v3_v3v3(locx, &pt1->x, &pt0->x);

  /* point vector at 3/4 */
  float v3[3];
  if (totpoints == 2) {
    mul_v3_v3fl(v3, &pt3->x, 0.001f);
  }
  else {
    copy_v3_v3(v3, &pt3->x);
  }

  sub_v3_v3v3(loc3, v3, &pt0->x);

  /* vector orthogonal to polygon plane */
  cross_v3_v3v3(normal, locx, loc3);

  /* local Y axis (cross to normal/x axis) */
  cross_v3_v3v3(locy, normal, locx);

  /* Normalize vectors */
  normalize_v3(locx);
  normalize_v3(locy);

  /* Calculate last point first. */
  const bGPDspoint *pt_last = &points[totpoints - 1];
  float tmp[3];
  sub_v3_v3v3(tmp, &pt_last->x, &pt0->x);

  points2d[totpoints - 1][0] = dot_v3v3(tmp, locx);
  points2d[totpoints - 1][1] = dot_v3v3(tmp, locy);

  /* Calculate the scalar cross product of the 2d points. */
  float cross = 0.0f;
  float *co_curr;
  float *co_prev = (float *)&points2d[totpoints - 1];

  /* Get all points in local space */
  for (int i = 0; i < totpoints - 1; i++) {
    const bGPDspoint *pt = &points[i];
    float loc[3];

    /* Get local space using first point as origin */
    sub_v3_v3v3(loc, &pt->x, &pt0->x);

    points2d[i][0] = dot_v3v3(loc, locx);
    points2d[i][1] = dot_v3v3(loc, locy);

    /* Calculate cross product. */
    co_curr = (float *)&points2d[i][0];
    cross += (co_curr[0] - co_prev[0]) * (co_curr[1] + co_prev[1]);
    co_prev = (float *)&points2d[i][0];
  }

  /* Concave (-1), Convex (1) */
  *r_direction = (cross >= 0.0f) ? 1 : -1;
}

void BKE_gpencil_stroke_2d_flat_ref(const bGPDspoint *ref_points,
                                    int ref_totpoints,
                                    const bGPDspoint *points,
                                    int totpoints,
                                    float (*points2d)[2],
                                    const float scale,
                                    int *r_direction)
{
  BLI_assert(totpoints >= 2);

  const bGPDspoint *pt0 = &ref_points[0];
  const bGPDspoint *pt1 = &ref_points[1];
  const bGPDspoint *pt3 = &ref_points[int(ref_totpoints * 0.75)];

  float locx[3];
  float locy[3];
  float loc3[3];
  float normal[3];

  /* local X axis (p0 -> p1) */
  sub_v3_v3v3(locx, &pt1->x, &pt0->x);

  /* point vector at 3/4 */
  float v3[3];
  if (totpoints == 2) {
    mul_v3_v3fl(v3, &pt3->x, 0.001f);
  }
  else {
    copy_v3_v3(v3, &pt3->x);
  }

  sub_v3_v3v3(loc3, v3, &pt0->x);

  /* vector orthogonal to polygon plane */
  cross_v3_v3v3(normal, locx, loc3);

  /* local Y axis (cross to normal/x axis) */
  cross_v3_v3v3(locy, normal, locx);

  /* Normalize vectors */
  normalize_v3(locx);
  normalize_v3(locy);

  /* Get all points in local space */
  for (int i = 0; i < totpoints; i++) {
    const bGPDspoint *pt = &points[i];
    float loc[3];
    float v1[3];
    float vn[3] = {0.0f, 0.0f, 0.0f};

    /* apply scale to extremes of the stroke to get better collision detection
     * the scale is divided to get more control in the UI parameter
     */
    /* first point */
    if (i == 0) {
      const bGPDspoint *pt_next = &points[i + 1];
      sub_v3_v3v3(vn, &pt->x, &pt_next->x);
      normalize_v3(vn);
      mul_v3_fl(vn, scale / 10.0f);
      add_v3_v3v3(v1, &pt->x, vn);
    }
    /* last point */
    else if (i == totpoints - 1) {
      const bGPDspoint *pt_prev = &points[i - 1];
      sub_v3_v3v3(vn, &pt->x, &pt_prev->x);
      normalize_v3(vn);
      mul_v3_fl(vn, scale / 10.0f);
      add_v3_v3v3(v1, &pt->x, vn);
    }
    else {
      copy_v3_v3(v1, &pt->x);
    }

    /* Get local space using first point as origin (ref stroke) */
    sub_v3_v3v3(loc, v1, &pt0->x);

    points2d[i][0] = dot_v3v3(loc, locx);
    points2d[i][1] = dot_v3v3(loc, locy);
  }

  /* Concave (-1), Convex (1), or Auto-detect (0)? */
  *r_direction = int(locy[2]);
}

/* Calc texture coordinates using flat projected points. */
static void gpencil_calc_stroke_fill_uv(const float (*points2d)[2],
                                        bGPDstroke *gps,
                                        const float minv[2],
                                        const float maxv[2],
                                        float (*r_uv)[2])
{
  const float s = sin(gps->uv_rotation);
  const float c = cos(gps->uv_rotation);

  /* Calc center for rotation. */
  float center[2] = {0.5f, 0.5f};
  float d[2];
  d[0] = maxv[0] - minv[0];
  d[1] = maxv[1] - minv[1];
  for (int i = 0; i < gps->totpoints; i++) {
    r_uv[i][0] = (points2d[i][0] - minv[0]) / d[0];
    r_uv[i][1] = (points2d[i][1] - minv[1]) / d[1];

    /* Apply translation. */
    add_v2_v2(r_uv[i], gps->uv_translation);

    /* Apply Rotation. */
    r_uv[i][0] -= center[0];
    r_uv[i][1] -= center[1];

    float x = r_uv[i][0] * c - r_uv[i][1] * s;
    float y = r_uv[i][0] * s + r_uv[i][1] * c;

    r_uv[i][0] = x + center[0];
    r_uv[i][1] = y + center[1];

    /* Apply scale. */
    if (gps->uv_scale != 0.0f) {
      mul_v2_fl(r_uv[i], 1.0f / gps->uv_scale);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Fill Triangulate
 * \{ */

void BKE_gpencil_stroke_fill_triangulate(bGPDstroke *gps)
{
  BLI_assert(gps->totpoints >= 3);

  /* allocate memory for temporary areas */
  gps->tot_triangles = gps->totpoints - 2;
  uint(*tmp_triangles)[3] = (uint(*)[3])MEM_mallocN(sizeof(*tmp_triangles) * gps->tot_triangles,
                                                    "GP Stroke temp triangulation");
  float(*points2d)[2] = (float(*)[2])MEM_mallocN(sizeof(*points2d) * gps->totpoints,
                                                 "GP Stroke temp 2d points");
  float(*uv)[2] = (float(*)[2])MEM_mallocN(sizeof(*uv) * gps->totpoints,
                                           "GP Stroke temp 2d uv data");

  int direction = 0;

  /* convert to 2d and triangulate */
  BKE_gpencil_stroke_2d_flat(gps->points, gps->totpoints, points2d, &direction);
  BLI_polyfill_calc(points2d, uint(gps->totpoints), direction, tmp_triangles);

  /* calc texture coordinates automatically */
  float minv[2];
  float maxv[2];
  /* first needs bounding box data */
  ARRAY_SET_ITEMS(minv, -1.0f, -1.0f);
  ARRAY_SET_ITEMS(maxv, 1.0f, 1.0f);

  /* calc uv data */
  gpencil_calc_stroke_fill_uv(points2d, gps, minv, maxv, uv);

  /* Save triangulation data. */
  if (gps->tot_triangles > 0) {
    MEM_SAFE_FREE(gps->triangles);
    gps->triangles = (bGPDtriangle *)MEM_callocN(sizeof(*gps->triangles) * gps->tot_triangles,
                                                 "GP Stroke triangulation");

    for (int i = 0; i < gps->tot_triangles; i++) {
      memcpy(gps->triangles[i].verts, tmp_triangles[i], sizeof(uint[3]));
    }

    /* Copy UVs to bGPDspoint. */
    for (int i = 0; i < gps->totpoints; i++) {
      copy_v2_v2(gps->points[i].uv_fill, uv[i]);
    }
  }
  else {
    /* No triangles needed - Free anything allocated previously */
    if (gps->triangles) {
      MEM_freeN(gps->triangles);
    }

    gps->triangles = nullptr;
  }

  /* clear memory */
  MEM_SAFE_FREE(tmp_triangles);
  MEM_SAFE_FREE(points2d);
  MEM_SAFE_FREE(uv);
}

void BKE_gpencil_stroke_uv_update(bGPDstroke *gps)
{
  if (gps == nullptr || gps->totpoints == 0) {
    return;
  }

  bGPDspoint *pt = gps->points;
  float totlen = 0.0f;
  pt[0].uv_fac = totlen;
  for (int i = 1; i < gps->totpoints; i++) {
    totlen += len_v3v3(&pt[i - 1].x, &pt[i].x);
    pt[i].uv_fac = totlen;
  }
}

void BKE_gpencil_stroke_geometry_update(bGPdata *gpd, bGPDstroke *gps)
{
  if (gps == nullptr) {
    return;
  }

  if (gps->editcurve != nullptr) {
    if (GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd)) {
      /* curve geometry was updated: stroke needs recalculation */
      if (gps->flag & GP_STROKE_NEEDS_CURVE_UPDATE) {
        bool is_adaptive = gpd->flag & GP_DATA_CURVE_ADAPTIVE_RESOLUTION;
        BKE_gpencil_stroke_update_geometry_from_editcurve(
            gps, gpd->curve_edit_resolution, is_adaptive);
        gps->flag &= ~GP_STROKE_NEEDS_CURVE_UPDATE;
      }
    }
    else {
      /* stroke geometry was updated: editcurve needs recalculation */
      gps->editcurve->flag |= GP_CURVE_NEEDS_STROKE_UPDATE;
    }
  }

  if (gps->totpoints > 2) {
    BKE_gpencil_stroke_fill_triangulate(gps);
  }
  else {
    gps->tot_triangles = 0;
    MEM_SAFE_FREE(gps->triangles);
  }

  /* calc uv data along the stroke */
  BKE_gpencil_stroke_uv_update(gps);

  /* Calc stroke bounding box. */
  BKE_gpencil_stroke_boundingbox_calc(gps);
}

float BKE_gpencil_stroke_length(const bGPDstroke *gps, bool use_3d)
{
  if (!gps->points || gps->totpoints < 2) {
    return 0.0f;
  }
  float *last_pt = &gps->points[0].x;
  float total_length = 0.0f;
  for (int i = 1; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    if (use_3d) {
      total_length += len_v3v3(&pt->x, last_pt);
    }
    else {
      total_length += len_v2v2(&pt->x, last_pt);
    }
    last_pt = &pt->x;
  }
  return total_length;
}

float BKE_gpencil_stroke_segment_length(const struct bGPDstroke *gps,
                                        const int start_index,
                                        const int end_index,
                                        bool use_3d)
{
  if (!gps->points || gps->totpoints < 2 || end_index <= start_index) {
    return 0.0f;
  }

  int index = MAX2(start_index, 0) + 1;
  int last_index = MIN2(end_index, gps->totpoints - 1) + 1;

  float *last_pt = &gps->points[index - 1].x;
  float total_length = 0.0f;
  for (int i = index; i < last_index; i++) {
    bGPDspoint *pt = &gps->points[i];
    if (use_3d) {
      total_length += len_v3v3(&pt->x, last_pt);
    }
    else {
      total_length += len_v2v2(&pt->x, last_pt);
    }
    last_pt = &pt->x;
  }
  return total_length;
}

bool BKE_gpencil_stroke_trim(bGPdata *gpd, bGPDstroke *gps)
{
  if (gps->totpoints < 4) {
    return false;
  }
  bool intersect = false;
  int start = 0;
  int end = 0;
  float point[3];
  /* loop segments from start until we have an intersection */
  for (int i = 0; i < gps->totpoints - 2; i++) {
    start = i;
    bGPDspoint *a = &gps->points[start];
    bGPDspoint *b = &gps->points[start + 1];
    for (int j = start + 2; j < gps->totpoints - 1; j++) {
      end = j + 1;
      bGPDspoint *c = &gps->points[j];
      bGPDspoint *d = &gps->points[end];
      float pointb[3];
      /* get intersection */
      if (isect_line_line_v3(&a->x, &b->x, &c->x, &d->x, point, pointb)) {
        if (len_v3(point) > 0.0f) {
          float closest[3];
          /* check intersection is on both lines */
          float lambda = closest_to_line_v3(closest, point, &a->x, &b->x);
          if ((lambda <= 0.0f) || (lambda >= 1.0f)) {
            continue;
          }
          lambda = closest_to_line_v3(closest, point, &c->x, &d->x);
          if ((lambda <= 0.0f) || (lambda >= 1.0f)) {
            continue;
          }

          intersect = true;
          break;
        }
      }
    }
    if (intersect) {
      break;
    }
  }

  /* trim unwanted points */
  if (intersect) {

    /* save points */
    bGPDspoint *old_points = (bGPDspoint *)MEM_dupallocN(gps->points);
    MDeformVert *old_dvert = nullptr;
    MDeformVert *dvert_src = nullptr;

    if (gps->dvert != nullptr) {
      old_dvert = (MDeformVert *)MEM_dupallocN(gps->dvert);
    }

    /* resize gps */
    int newtot = end - start + 1;

    gps->points = (bGPDspoint *)MEM_recallocN(gps->points, sizeof(*gps->points) * newtot);
    if (gps->dvert != nullptr) {
      gps->dvert = (MDeformVert *)MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * newtot);
    }

    for (int i = 0; i < newtot; i++) {
      int idx = start + i;
      bGPDspoint *pt_src = &old_points[idx];
      bGPDspoint *pt_new = &gps->points[i];
      *pt_new = blender::dna::shallow_copy(*pt_src);
      if (gps->dvert != nullptr) {
        dvert_src = &old_dvert[idx];
        MDeformVert *dvert = &gps->dvert[i];
        memcpy(dvert, dvert_src, sizeof(MDeformVert));
        if (dvert_src->dw) {
          memcpy(dvert->dw, dvert_src->dw, sizeof(MDeformWeight));
        }
      }
      if (ELEM(idx, start, end)) {
        copy_v3_v3(&pt_new->x, point);
      }
    }

    gps->totpoints = newtot;

    MEM_SAFE_FREE(old_points);
    MEM_SAFE_FREE(old_dvert);
  }

  BKE_gpencil_stroke_geometry_update(gpd, gps);

  return intersect;
}

bool BKE_gpencil_stroke_close(bGPDstroke *gps)
{
  bGPDspoint *pt1 = nullptr;
  bGPDspoint *pt2 = nullptr;

  /* Only can close a stroke with 3 points or more. */
  if (gps->totpoints < 3) {
    return false;
  }

  /* Calc average distance between points to get same level of sampling. */
  float dist_tot = 0.0f;
  for (int i = 0; i < gps->totpoints - 1; i++) {
    pt1 = &gps->points[i];
    pt2 = &gps->points[i + 1];
    dist_tot += len_v3v3(&pt1->x, &pt2->x);
  }
  /* Calc the average distance. */
  float dist_avg = dist_tot / (gps->totpoints - 1);

  /* Calc distance between last and first point. */
  pt1 = &gps->points[gps->totpoints - 1];
  pt2 = &gps->points[0];
  float dist_close = len_v3v3(&pt1->x, &pt2->x);

  /* if the distance to close is very small, don't need add points and just enable cyclic. */
  if (dist_close <= dist_avg) {
    gps->flag |= GP_STROKE_CYCLIC;
    return true;
  }

  /* Calc number of points required using the average distance. */
  int tot_newpoints = MAX2(dist_close / dist_avg, 1);

  /* Resize stroke array. */
  int old_tot = gps->totpoints;
  gps->totpoints += tot_newpoints;
  gps->points = (bGPDspoint *)MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints);
  if (gps->dvert != nullptr) {
    gps->dvert = (MDeformVert *)MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints);
  }

  /* Generate new points */
  pt1 = &gps->points[old_tot - 1];
  pt2 = &gps->points[0];
  bGPDspoint *pt = &gps->points[old_tot];
  for (int i = 1; i < tot_newpoints + 1; i++, pt++) {
    float step = (tot_newpoints > 1) ? (float(i) / float(tot_newpoints)) : 0.99f;
    /* Clamp last point to be near, but not on top of first point. */
    if ((tot_newpoints > 1) && (i == tot_newpoints)) {
      step *= 0.99f;
    }

    /* Average point. */
    interp_v3_v3v3(&pt->x, &pt1->x, &pt2->x, step);
    pt->pressure = interpf(pt2->pressure, pt1->pressure, step);
    pt->strength = interpf(pt2->strength, pt1->strength, step);
    pt->flag = 0;
    interp_v4_v4v4(pt->vert_color, pt1->vert_color, pt2->vert_color, step);
    /* Set point as selected. */
    if (gps->flag & GP_STROKE_SELECT) {
      pt->flag |= GP_SPOINT_SELECT;
    }

    /* Set weights. */
    if (gps->dvert != nullptr) {
      MDeformVert *dvert1 = &gps->dvert[old_tot - 1];
      MDeformWeight *dw1 = BKE_defvert_ensure_index(dvert1, 0);
      float weight_1 = dw1 ? dw1->weight : 0.0f;

      MDeformVert *dvert2 = &gps->dvert[0];
      MDeformWeight *dw2 = BKE_defvert_ensure_index(dvert2, 0);
      float weight_2 = dw2 ? dw2->weight : 0.0f;

      MDeformVert *dvert_final = &gps->dvert[old_tot + i - 1];
      dvert_final->totweight = 0;
      MDeformWeight *dw = BKE_defvert_ensure_index(dvert_final, 0);
      if (dvert_final->dw) {
        dw->weight = interpf(weight_2, weight_1, step);
      }
    }
  }

  /* Enable cyclic flag. */
  gps->flag |= GP_STROKE_CYCLIC;

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dissolve Points
 * \{ */

void BKE_gpencil_dissolve_points(bGPdata *gpd, bGPDframe *gpf, bGPDstroke *gps, const short tag)
{
  bGPDspoint *pt;
  MDeformVert *dvert = nullptr;
  int i;

  int tot = gps->totpoints; /* number of points in new buffer */
  /* first pass: count points to remove */
  /* Count how many points are selected (i.e. how many to remove) */
  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    if (pt->flag & tag) {
      /* selected point - one of the points to remove */
      tot--;
    }
  }

  /* if no points are left, we simply delete the entire stroke */
  if (tot <= 0) {
    /* remove the entire stroke */
    if (gps->points) {
      MEM_freeN(gps->points);
    }
    if (gps->dvert) {
      BKE_gpencil_free_stroke_weights(gps);
      MEM_freeN(gps->dvert);
    }
    if (gps->triangles) {
      MEM_freeN(gps->triangles);
    }
    BLI_freelinkN(&gpf->strokes, gps);
  }
  else {
    /* just copy all points to keep into a smaller buffer */
    bGPDspoint *new_points = (bGPDspoint *)MEM_callocN(sizeof(bGPDspoint) * tot,
                                                       "new gp stroke points copy");
    bGPDspoint *npt = new_points;

    MDeformVert *new_dvert = nullptr;
    MDeformVert *ndvert = nullptr;

    if (gps->dvert != nullptr) {
      new_dvert = (MDeformVert *)MEM_callocN(sizeof(MDeformVert) * tot,
                                             "new gp stroke weights copy");
      ndvert = new_dvert;
    }

    (gps->dvert != nullptr) ? dvert = gps->dvert : nullptr;
    for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
      if ((pt->flag & tag) == 0) {
        *npt = blender::dna::shallow_copy(*pt);
        npt++;

        if (gps->dvert != nullptr) {
          *ndvert = *dvert;
          ndvert->dw = (MDeformWeight *)MEM_dupallocN(dvert->dw);
          ndvert++;
        }
      }
      if (gps->dvert != nullptr) {
        dvert++;
      }
    }

    /* free the old buffer */
    if (gps->points) {
      MEM_freeN(gps->points);
    }
    if (gps->dvert) {
      BKE_gpencil_free_stroke_weights(gps);
      MEM_freeN(gps->dvert);
    }

    /* save the new buffer */
    gps->points = new_points;
    gps->dvert = new_dvert;
    gps->totpoints = tot;

    /* triangles cache needs to be recalculated */
    BKE_gpencil_stroke_geometry_update(gpd, gps);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Normal Calculation
 * \{ */

void BKE_gpencil_stroke_normal(const bGPDstroke *gps, float r_normal[3])
{
  if (gps->totpoints < 3) {
    zero_v3(r_normal);
    return;
  }

  bGPDspoint *points = gps->points;
  int totpoints = gps->totpoints;

  const bGPDspoint *pt0 = &points[0];
  const bGPDspoint *pt1 = &points[1];
  const bGPDspoint *pt3 = &points[int(totpoints * 0.75)];

  float vec1[3];
  float vec2[3];

  /* initial vector (p0 -> p1) */
  sub_v3_v3v3(vec1, &pt1->x, &pt0->x);

  /* point vector at 3/4 */
  sub_v3_v3v3(vec2, &pt3->x, &pt0->x);

  /* vector orthogonal to polygon plane */
  cross_v3_v3v3(r_normal, vec1, vec2);

  /* Normalize vector */
  normalize_v3(r_normal);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Simplify
 * \{ */

void BKE_gpencil_stroke_simplify_adaptive(bGPdata *gpd, bGPDstroke *gps, float epsilon)
{
  bGPDspoint *old_points = (bGPDspoint *)MEM_dupallocN(gps->points);
  int totpoints = gps->totpoints;
  char *marked = nullptr;
  char work;

  int start = 0;
  int end = gps->totpoints - 1;

  marked = (char *)MEM_callocN(totpoints, "GP marked array");
  marked[start] = 1;
  marked[end] = 1;

  work = 1;
  int totmarked = 0;
  /* while still reducing */
  while (work) {
    int ls, le;
    work = 0;

    ls = start;
    le = start + 1;

    /* while not over interval */
    while (ls < end) {
      int max_i = 0;
      /* divided to get more control */
      float max_dist = epsilon / 10.0f;

      /* find the next marked point */
      while (marked[le] == 0) {
        le++;
      }

      for (int i = ls + 1; i < le; i++) {
        float point_on_line[3];
        float dist;

        closest_to_line_segment_v3(
            point_on_line, &old_points[i].x, &old_points[ls].x, &old_points[le].x);

        dist = len_v3v3(point_on_line, &old_points[i].x);

        if (dist > max_dist) {
          max_dist = dist;
          max_i = i;
        }
      }

      if (max_i != 0) {
        work = 1;
        marked[max_i] = 1;
        totmarked++;
      }

      ls = le;
      le = ls + 1;
    }
  }

  /* adding points marked */
  MDeformVert *old_dvert = nullptr;
  MDeformVert *dvert_src = nullptr;

  if (gps->dvert != nullptr) {
    old_dvert = (MDeformVert *)MEM_dupallocN(gps->dvert);
  }
  /* resize gps */
  int j = 0;
  for (int i = 0; i < totpoints; i++) {
    bGPDspoint *pt_src = &old_points[i];
    bGPDspoint *pt = &gps->points[j];

    if ((marked[i]) || (i == 0) || (i == totpoints - 1)) {
      *pt = blender::dna::shallow_copy(*pt_src);
      if (gps->dvert != nullptr) {
        dvert_src = &old_dvert[i];
        MDeformVert *dvert = &gps->dvert[j];
        memcpy(dvert, dvert_src, sizeof(MDeformVert));
        if (dvert_src->dw) {
          memcpy(dvert->dw, dvert_src->dw, sizeof(MDeformWeight));
        }
      }
      j++;
    }
    else {
      if (gps->dvert != nullptr) {
        dvert_src = &old_dvert[i];
        BKE_gpencil_free_point_weights(dvert_src);
      }
    }
  }

  gps->totpoints = j;

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);

  MEM_SAFE_FREE(old_points);
  MEM_SAFE_FREE(old_dvert);
  MEM_SAFE_FREE(marked);
}

void BKE_gpencil_stroke_simplify_fixed(bGPdata *gpd, bGPDstroke *gps)
{
  if (gps->totpoints < 4) {
    return;
  }

  /* save points */
  bGPDspoint *old_points = (bGPDspoint *)MEM_dupallocN(gps->points);
  MDeformVert *old_dvert = nullptr;
  MDeformVert *dvert_src = nullptr;

  if (gps->dvert != nullptr) {
    old_dvert = (MDeformVert *)MEM_dupallocN(gps->dvert);
  }

  /* resize gps */
  int newtot = (gps->totpoints - 2) / 2;
  if ((gps->totpoints % 2) != 0) {
    newtot++;
  }
  newtot += 2;

  gps->points = (bGPDspoint *)MEM_recallocN(gps->points, sizeof(*gps->points) * newtot);
  if (gps->dvert != nullptr) {
    gps->dvert = (MDeformVert *)MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * newtot);
  }

  int j = 0;
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt_src = &old_points[i];
    bGPDspoint *pt = &gps->points[j];

    if ((i == 0) || (i == gps->totpoints - 1) || ((i % 2) > 0.0)) {
      *pt = blender::dna::shallow_copy(*pt_src);
      if (gps->dvert != nullptr) {
        dvert_src = &old_dvert[i];
        MDeformVert *dvert = &gps->dvert[j];
        memcpy(dvert, dvert_src, sizeof(MDeformVert));
        if (dvert_src->dw) {
          memcpy(dvert->dw, dvert_src->dw, sizeof(MDeformWeight));
        }
      }
      j++;
    }
    else {
      if (gps->dvert != nullptr) {
        dvert_src = &old_dvert[i];
        BKE_gpencil_free_point_weights(dvert_src);
      }
    }
  }

  gps->totpoints = j;
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);

  MEM_SAFE_FREE(old_points);
  MEM_SAFE_FREE(old_dvert);
}

void BKE_gpencil_stroke_subdivide(bGPdata *gpd, bGPDstroke *gps, int level, int type)
{
  bGPDspoint *temp_points;
  MDeformVert *temp_dverts = nullptr;
  MDeformVert *dvert = nullptr;
  MDeformVert *dvert_final = nullptr;
  MDeformVert *dvert_next = nullptr;
  int totnewpoints, oldtotpoints;

  bool cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;

  for (int s = 0; s < level; s++) {
    totnewpoints = gps->totpoints;
    if (!cyclic) {
      totnewpoints--;
    }
    /* duplicate points in a temp area */
    temp_points = gps->points;
    oldtotpoints = gps->totpoints;

    /* resize the points arrays */
    gps->totpoints += totnewpoints;
    gps->points = (bGPDspoint *)MEM_malloc_arrayN(gps->totpoints, sizeof(*gps->points), __func__);
    if (gps->dvert != nullptr) {
      temp_dverts = gps->dvert;
      gps->dvert = (MDeformVert *)MEM_malloc_arrayN(gps->totpoints, sizeof(*gps->dvert), __func__);
    }

    /* move points from last to first to new place */
    for (int i = 0; i < oldtotpoints; i++) {
      bGPDspoint *pt = &temp_points[i];
      bGPDspoint *pt_final = &gps->points[i * 2];

      copy_v3_v3(&pt_final->x, &pt->x);
      pt_final->pressure = pt->pressure;
      pt_final->strength = pt->strength;
      pt_final->uv_rot = pt->uv_rot;
      pt_final->uv_fac = pt->uv_fac;
      pt_final->time = pt->time;
      pt_final->flag = pt->flag;
      pt_final->runtime.pt_orig = pt->runtime.pt_orig;
      pt_final->runtime.idx_orig = pt->runtime.idx_orig;
      copy_v4_v4(pt_final->vert_color, pt->vert_color);
      copy_v4_v4(pt_final->uv_fill, pt->uv_fill);

      if (gps->dvert != nullptr) {
        dvert = &temp_dverts[i];
        dvert_final = &gps->dvert[i * 2];
        dvert_final->totweight = dvert->totweight;
        dvert_final->dw = dvert->dw;
      }
    }
    /* interpolate mid points */
    for (int i = cyclic ? 0 : 1, j = cyclic ? oldtotpoints - 1 : 0; i < oldtotpoints; j = i, i++) {
      bGPDspoint *pt = &temp_points[j];
      bGPDspoint *next = &temp_points[i];
      bGPDspoint *pt_final = &gps->points[j * 2 + 1];

      /* add a half way point */
      interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
      pt_final->pressure = interpf(pt->pressure, next->pressure, 0.5f);
      pt_final->strength = interpf(pt->strength, next->strength, 0.5f);
      pt_final->uv_rot = interpf(pt->uv_rot, next->uv_rot, 0.5f);
      pt_final->uv_fac = interpf(pt->uv_fac, next->uv_fac, 0.5f);
      interp_v4_v4v4(pt_final->uv_fill, pt->uv_fill, next->uv_fill, 0.5f);
      CLAMP(pt_final->strength, GPENCIL_STRENGTH_MIN, 1.0f);
      pt_final->time = interpf(pt->time, next->time, 0.5f);
      pt_final->runtime.pt_orig = nullptr;
      pt_final->flag = 0;
      interp_v4_v4v4(pt_final->vert_color, pt->vert_color, next->vert_color, 0.5f);

      if (gps->dvert != nullptr) {
        dvert = &temp_dverts[j];
        dvert_next = &temp_dverts[i];
        dvert_final = &gps->dvert[j * 2 + 1];

        dvert_final->totweight = dvert->totweight;
        dvert_final->dw = (MDeformWeight *)MEM_dupallocN(dvert->dw);

        /* interpolate weight values */
        for (int d = 0; d < dvert->totweight; d++) {
          MDeformWeight *dw_a = &dvert->dw[d];
          if (dvert_next->totweight > d) {
            MDeformWeight *dw_b = &dvert_next->dw[d];
            MDeformWeight *dw_final = &dvert_final->dw[d];
            dw_final->weight = interpf(dw_a->weight, dw_b->weight, 0.5f);
          }
        }
      }
    }

    MEM_SAFE_FREE(temp_points);
    MEM_SAFE_FREE(temp_dverts);

    /* Move points to smooth stroke (not simple type). */
    if (type != GP_SUBDIV_SIMPLE) {
      float mid[3];
      /* extreme points are not changed */
      for (int i = cyclic ? 0 : 2, j = cyclic ? gps->totpoints - 2 : 0; i < gps->totpoints - 2;
           j = i, i += 2) {
        bGPDspoint *prev = &gps->points[j + 1];
        bGPDspoint *pt = &gps->points[i];
        bGPDspoint *next = &gps->points[i + 1];

        /* move point */
        interp_v3_v3v3(mid, &prev->x, &next->x, 0.5f);
        interp_v3_v3v3(&pt->x, mid, &pt->x, 0.5f);
      }
    }
  }

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Merge by Distance
 * \{ */

void BKE_gpencil_stroke_merge_distance(bGPdata *gpd,
                                       bGPDframe *gpf,
                                       bGPDstroke *gps,
                                       const float threshold,
                                       const bool use_unselected)
{
  bGPDspoint *pt = nullptr;
  bGPDspoint *pt_next = nullptr;
  float tagged = false;
  /* Use square distance to speed up loop */
  const float th_square = threshold * threshold;
  /* Need to have something to merge. */
  if (gps->totpoints < 2) {
    return;
  }
  int i = 0;
  int step = 1;
  while ((i < gps->totpoints - 1) && (i + step < gps->totpoints)) {
    pt = &gps->points[i];
    if (pt->flag & GP_SPOINT_TAG) {
      i++;
      step = 1;
      continue;
    }
    pt_next = &gps->points[i + step];
    /* Do not recalc tagged points. */
    if (pt_next->flag & GP_SPOINT_TAG) {
      step++;
      continue;
    }
    /* Check if contiguous points are selected. */
    if (!use_unselected) {
      if (((pt->flag & GP_SPOINT_SELECT) == 0) || ((pt_next->flag & GP_SPOINT_SELECT) == 0)) {
        i++;
        step = 1;
        continue;
      }
    }
    float len_square = len_squared_v3v3(&pt->x, &pt_next->x);
    if (len_square <= th_square) {
      tagged = true;
      if (i != gps->totpoints - 1) {
        /* Tag second point for delete. */
        pt_next->flag |= GP_SPOINT_TAG;
      }
      else {
        pt->flag |= GP_SPOINT_TAG;
      }
      /* Jump to next pair of points, keeping first point segment equals. */
      step++;
    }
    else {
      /* Analyze next point. */
      i++;
      step = 1;
    }
  }

  /* Always untag extremes. */
  pt = &gps->points[0];
  pt->flag &= ~GP_SPOINT_TAG;
  pt = &gps->points[gps->totpoints - 1];
  pt->flag &= ~GP_SPOINT_TAG;

  /* Dissolve tagged points */
  if (tagged) {
    BKE_gpencil_dissolve_points(gpd, gpf, gps, GP_SPOINT_TAG);
  }

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

struct GpEdge {
  uint v1, v2;
  /* Coordinates. */
  float v1_co[3], v2_co[3];
  /* Normals. */
  float n1[3], n2[3];
  /* Direction of the segment. */
  float vec[3];
  int flag;
};

static int gpencil_next_edge(
    GpEdge *gp_edges, int totedges, GpEdge *gped_init, const float threshold, const bool reverse)
{
  int edge = -1;
  float last_angle = 999999.0f;
  for (int i = 0; i < totedges; i++) {
    GpEdge *gped = &gp_edges[i];
    if (gped->flag != 0) {
      continue;
    }
    if (reverse) {
      if (gped_init->v1 != gped->v2) {
        continue;
      }
    }
    else {
      if (gped_init->v2 != gped->v1) {
        continue;
      }
    }
    /* Look for straight lines. */
    float angle = angle_v3v3(gped->vec, gped_init->vec);
    if ((angle < threshold) && (angle <= last_angle)) {
      edge = i;
      last_angle = angle;
    }
  }

  return edge;
}

static int gpencil_walk_edge(GHash *v_table,
                             GpEdge *gp_edges,
                             int totedges,
                             uint *stroke_array,
                             int init_idx,
                             const float angle,
                             const bool reverse)
{
  GpEdge *gped_init = &gp_edges[init_idx];
  int idx = 1;
  int edge = 0;
  while (edge > -1) {
    edge = gpencil_next_edge(gp_edges, totedges, gped_init, angle, reverse);
    if (edge > -1) {
      GpEdge *gped = &gp_edges[edge];
      stroke_array[idx] = edge;
      gped->flag = 1;
      gped_init = &gp_edges[edge];
      idx++;

      /* Avoid following already visited vertices. */
      if (reverse) {
        if (BLI_ghash_haskey(v_table, POINTER_FROM_INT(gped->v1))) {
          edge = -1;
        }
        else {
          BLI_ghash_insert(v_table, POINTER_FROM_INT(gped->v1), POINTER_FROM_INT(gped->v1));
        }
      }
      else {
        if (BLI_ghash_haskey(v_table, POINTER_FROM_INT(gped->v2))) {
          edge = -1;
        }
        else {
          BLI_ghash_insert(v_table, POINTER_FROM_INT(gped->v2), POINTER_FROM_INT(gped->v2));
        }
      }
    }
  }

  return idx;
}

static void gpencil_generate_edgeloops(Object *ob,
                                       bGPdata *gpd,
                                       bGPDframe *gpf_stroke,
                                       int stroke_mat_index,
                                       const float angle,
                                       const int thickness,
                                       const float offset,
                                       const float matrix[4][4],
                                       const bool use_seams,
                                       const bool use_vgroups)
{
  Mesh *me = (Mesh *)ob->data;
  if (me->totedge == 0) {
    return;
  }
  const Span<float3> vert_positions = me->vert_positions();
  const Span<MEdge> edges = me->edges();
  const Span<MDeformVert> dverts = me->deform_verts();
  const float(*vert_normals)[3] = BKE_mesh_vertex_normals_ensure(me);

  /* Arrays for all edge vertices (forward and backward) that form a edge loop.
   * This is reused for each edge-loop to create gpencil stroke. */
  uint *stroke = (uint *)MEM_mallocN(sizeof(uint) * me->totedge * 2, __func__);
  uint *stroke_fw = (uint *)MEM_mallocN(sizeof(uint) * me->totedge, __func__);
  uint *stroke_bw = (uint *)MEM_mallocN(sizeof(uint) * me->totedge, __func__);

  /* Create array with all edges. */
  GpEdge *gp_edges = (GpEdge *)MEM_callocN(sizeof(GpEdge) * me->totedge, __func__);
  GpEdge *gped = nullptr;
  for (int i = 0; i < me->totedge; i++) {
    const MEdge *ed = &edges[i];
    gped = &gp_edges[i];
    copy_v3_v3(gped->n1, vert_normals[ed->v1]);

    gped->v1 = ed->v1;
    copy_v3_v3(gped->v1_co, vert_positions[ed->v1]);

    copy_v3_v3(gped->n2, vert_normals[ed->v2]);
    gped->v2 = ed->v2;
    copy_v3_v3(gped->v2_co, vert_positions[ed->v2]);

    sub_v3_v3v3(gped->vec, vert_positions[ed->v1], vert_positions[ed->v2]);

    /* If use seams, mark as done if not a seam. */
    if ((use_seams) && ((ed->flag & ME_SEAM) == 0)) {
      gped->flag = 1;
    }
  }

  /* Loop edges to find edgeloops */
  bool pending = true;
  int e = 0;
  while (pending) {
    gped = &gp_edges[e];
    /* Look first unused edge. */
    if (gped->flag != 0) {
      e++;
      if (e == me->totedge) {
        pending = false;
      }
      continue;
    }
    /* Add current edge to arrays. */
    stroke_fw[0] = e;
    stroke_bw[0] = e;
    gped->flag = 1;

    /* Hash used to avoid loop over same vertices. */
    GHash *v_table = BLI_ghash_int_new(__func__);
    /* Look forward edges. */
    int totedges = gpencil_walk_edge(v_table, gp_edges, me->totedge, stroke_fw, e, angle, false);
    /* Look backward edges. */
    int totbw = gpencil_walk_edge(v_table, gp_edges, me->totedge, stroke_bw, e, angle, true);

    BLI_ghash_free(v_table, nullptr, nullptr);

    /* Join both arrays. */
    int array_len = 0;
    for (int i = totbw - 1; i > 0; i--) {
      stroke[array_len] = stroke_bw[i];
      array_len++;
    }
    for (int i = 0; i < totedges; i++) {
      stroke[array_len] = stroke_fw[i];
      array_len++;
    }

    /* Create Stroke. */
    bGPDstroke *gps_stroke = BKE_gpencil_stroke_add(
        gpf_stroke, MAX2(stroke_mat_index, 0), array_len + 1, thickness * thickness, false);

    /* Create dvert data. */
    if (use_vgroups && !dverts.is_empty()) {
      gps_stroke->dvert = (MDeformVert *)MEM_callocN(sizeof(MDeformVert) * (array_len + 1),
                                                     "gp_stroke_dverts");
    }

    /* Create first segment. */
    float fpt[3];
    for (int i = 0; i < array_len + 1; i++) {
      int vertex_index = i == 0 ? gp_edges[stroke[0]].v1 : gp_edges[stroke[i - 1]].v2;
      /* Add segment. */
      bGPDspoint *pt = &gps_stroke->points[i];
      copy_v3_v3(fpt, vert_normals[vertex_index]);
      mul_v3_v3fl(fpt, fpt, offset);
      add_v3_v3v3(&pt->x, vert_positions[vertex_index], fpt);
      mul_m4_v3(matrix, &pt->x);

      pt->pressure = 1.0f;
      pt->strength = 1.0f;

      /* Copy vertex groups from mesh. Assuming they already exist in the same order. */
      if (use_vgroups && !dverts.is_empty()) {
        MDeformVert *dv = &gps_stroke->dvert[i];
        const MDeformVert *src_dv = &dverts[vertex_index];
        dv->totweight = src_dv->totweight;
        dv->dw = (MDeformWeight *)MEM_callocN(sizeof(MDeformWeight) * dv->totweight,
                                              "gp_stroke_dverts_dw");
        for (int j = 0; j < dv->totweight; j++) {
          dv->dw[j].weight = src_dv->dw[j].weight;
          dv->dw[j].def_nr = src_dv->dw[j].def_nr;
        }
      }
    }

    BKE_gpencil_stroke_geometry_update(gpd, gps_stroke);
  }

  /* Free memory. */
  MEM_SAFE_FREE(stroke);
  MEM_SAFE_FREE(stroke_fw);
  MEM_SAFE_FREE(stroke_bw);
  MEM_SAFE_FREE(gp_edges);
}

/* Helper: Add gpencil material using material as base. */
static Material *gpencil_add_material(Main *bmain,
                                      Object *ob_gp,
                                      const char *name,
                                      const float color[4],
                                      const bool use_stroke,
                                      const bool use_fill,
                                      int *r_idx)
{
  Material *mat_gp = BKE_gpencil_object_material_new(bmain, ob_gp, name, r_idx);
  MaterialGPencilStyle *gp_style = mat_gp->gp_style;

  /* Stroke color. */
  if (use_stroke) {
    ARRAY_SET_ITEMS(gp_style->stroke_rgba, 0.0f, 0.0f, 0.0f, 1.0f);
    gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
  }
  else {
    copy_v4_v4(gp_style->stroke_rgba, color);
    gp_style->flag &= ~GP_MATERIAL_STROKE_SHOW;
  }

  /* Fill color. */
  copy_v4_v4(gp_style->fill_rgba, color);
  if (use_fill) {
    gp_style->flag |= GP_MATERIAL_FILL_SHOW;
  }

  /* Check at least one is enabled. */
  if (((gp_style->flag & GP_MATERIAL_STROKE_SHOW) == 0) &&
      ((gp_style->flag & GP_MATERIAL_FILL_SHOW) == 0)) {
    gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
  }

  return mat_gp;
}

static int gpencil_material_find_index_by_name(Object *ob, const char *name)
{
  for (int i = 0; i < ob->totcol; i++) {
    Material *ma = BKE_object_material_get(ob, i + 1);
    if ((ma != nullptr) && (ma->gp_style != nullptr) && STREQ(ma->id.name + 2, name)) {
      return i;
    }
  }

  return -1;
}

/**
 * Create the name with the object name and a suffix.
 */
static void make_element_name(const char *obname, const char *name, const int maxlen, char *r_name)
{
  char str[256];
  SNPRINTF(str, "%s_%s", obname, name);

  /* Replace any point by underscore. */
  BLI_str_replace_char(str, '.', '_');

  BLI_strncpy_utf8(r_name, str, maxlen);
}

bool BKE_gpencil_convert_mesh(Main *bmain,
                              Depsgraph *depsgraph,
                              Scene *scene,
                              Object *ob_gp,
                              Object *ob_mesh,
                              const float angle,
                              const int thickness,
                              const float offset,
                              const float matrix[4][4],
                              const int frame_offset,
                              const bool use_seams,
                              const bool use_faces,
                              const bool use_vgroups)
{
  using namespace blender;
  using namespace blender::bke;
  if (ELEM(nullptr, ob_gp, ob_mesh) || (ob_gp->type != OB_GPENCIL) || (ob_gp->data == nullptr)) {
    return false;
  }

  bGPdata *gpd = (bGPdata *)ob_gp->data;

  /* Use evaluated data to get mesh with all modifiers on top. */
  Object *ob_eval = (Object *)DEG_get_evaluated_object(depsgraph, ob_mesh);
  const Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);
  const Span<float3> positions = me_eval->vert_positions();
  const Span<MPoly> polys = me_eval->polys();
  const Span<MLoop> loops = me_eval->loops();
  int mpoly_len = me_eval->totpoly;
  char element_name[200];

  /* Need at least an edge. */
  if (me_eval->totedge < 1) {
    return false;
  }

  /* Create matching vertex groups. */
  BKE_defgroup_copy_list(&gpd->vertex_group_names, &me_eval->vertex_group_names);
  gpd->vertex_group_active_index = me_eval->vertex_group_active_index;

  const float default_colors[2][4] = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.7f, 0.7f, 0.7f, 1.0f}};
  /* Lookup existing stroke material on gp object. */
  make_element_name(ob_mesh->id.name + 2, "Stroke", 64, element_name);
  int stroke_mat_index = gpencil_material_find_index_by_name(ob_gp, element_name);

  if (stroke_mat_index == -1) {
    /* Create new default stroke material as there is no existing material. */
    gpencil_add_material(
        bmain, ob_gp, element_name, default_colors[0], true, false, &stroke_mat_index);
  }

  /* Export faces as filled strokes. */
  if (use_faces && mpoly_len > 0) {
    /* Read all polygons and create fill for each. */
    make_element_name(ob_mesh->id.name + 2, "Fills", 128, element_name);
    /* Create Layer and Frame. */
    bGPDlayer *gpl_fill = BKE_gpencil_layer_named_get(gpd, element_name);
    if (gpl_fill == nullptr) {
      gpl_fill = BKE_gpencil_layer_addnew(gpd, element_name, true, false);
    }
    bGPDframe *gpf_fill = BKE_gpencil_layer_frame_get(
        gpl_fill, scene->r.cfra + frame_offset, GP_GETFRAME_ADD_NEW);
    int i;

    const VArray<int> mesh_material_indices = me_eval->attributes().lookup_or_default<int>(
        "material_index", ATTR_DOMAIN_FACE, 0);
    for (i = 0; i < mpoly_len; i++) {
      const MPoly *mp = &polys[i];

      /* Find material. */
      int mat_idx = 0;
      Material *ma = BKE_object_material_get(ob_mesh, mesh_material_indices[i] + 1);
      make_element_name(
          ob_mesh->id.name + 2, (ma != nullptr) ? ma->id.name + 2 : "Fill", 64, element_name);
      mat_idx = BKE_gpencil_material_find_index_by_name_prefix(ob_gp, element_name);
      if (mat_idx == -1) {
        float color[4];
        if (ma != nullptr) {
          copy_v3_v3(color, &ma->r);
          color[3] = 1.0f;
        }
        else {
          copy_v4_v4(color, default_colors[1]);
        }
        gpencil_add_material(bmain, ob_gp, element_name, color, false, true, &mat_idx);
      }

      bGPDstroke *gps_fill = BKE_gpencil_stroke_add(gpf_fill, mat_idx, mp->totloop, 10, false);
      gps_fill->flag |= GP_STROKE_CYCLIC;

      /* Create dvert data. */
      const Span<MDeformVert> dverts = me_eval->deform_verts();
      if (use_vgroups && !dverts.is_empty()) {
        gps_fill->dvert = (MDeformVert *)MEM_callocN(sizeof(MDeformVert) * mp->totloop,
                                                     "gp_fill_dverts");
      }

      /* Add points to strokes. */
      for (int j = 0; j < mp->totloop; j++) {
        const MLoop *ml = &loops[mp->loopstart + j];

        bGPDspoint *pt = &gps_fill->points[j];
        copy_v3_v3(&pt->x, positions[ml->v]);
        mul_m4_v3(matrix, &pt->x);
        pt->pressure = 1.0f;
        pt->strength = 1.0f;

        /* Copy vertex groups from mesh. Assuming they already exist in the same order. */
        if (use_vgroups && !dverts.is_empty()) {
          MDeformVert *dv = &gps_fill->dvert[j];
          const MDeformVert *src_dv = &dverts[ml->v];
          dv->totweight = src_dv->totweight;
          dv->dw = (MDeformWeight *)MEM_callocN(sizeof(MDeformWeight) * dv->totweight,
                                                "gp_fill_dverts_dw");
          for (int k = 0; k < dv->totweight; k++) {
            dv->dw[k].weight = src_dv->dw[k].weight;
            dv->dw[k].def_nr = src_dv->dw[k].def_nr;
          }
        }
      }
      /* If has only 3 points subdivide. */
      if (mp->totloop == 3) {
        BKE_gpencil_stroke_subdivide(gpd, gps_fill, 1, GP_SUBDIV_SIMPLE);
      }

      BKE_gpencil_stroke_geometry_update(gpd, gps_fill);
    }
  }

  /* Create stroke from edges. */

  /* Create Layer and Frame. */
  make_element_name(ob_mesh->id.name + 2, "Lines", 128, element_name);
  bGPDlayer *gpl_stroke = BKE_gpencil_layer_named_get(gpd, element_name);
  if (gpl_stroke == nullptr) {
    gpl_stroke = BKE_gpencil_layer_addnew(gpd, element_name, true, false);
  }
  bGPDframe *gpf_stroke = BKE_gpencil_layer_frame_get(
      gpl_stroke, scene->r.cfra + frame_offset, GP_GETFRAME_ADD_NEW);

  gpencil_generate_edgeloops(ob_eval,
                             gpd,
                             gpf_stroke,
                             stroke_mat_index,
                             angle,
                             thickness,
                             offset,
                             matrix,
                             use_seams,
                             use_vgroups);

  /* Tag for recalculation */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);

  return true;
}

void BKE_gpencil_transform(bGPdata *gpd, const float mat[4][4])
{
  if (gpd == nullptr) {
    return;
  }

  const float scalef = mat4_to_scale(mat);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* FIXME: For now, we just skip parented layers.
     * Otherwise, we have to update each frame to find
     * the current parent position/effects.
     */
    if (gpl->parent) {
      continue;
    }

    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        bGPDspoint *pt;
        int i;

        for (pt = gps->points, i = 0; i < gps->totpoints; pt++, i++) {
          mul_m4_v3(mat, &pt->x);
          pt->pressure *= scalef;
        }

        /* Distortion may mean we need to re-triangulate. */
        BKE_gpencil_stroke_geometry_update(gpd, gps);
      }
    }
  }
}

int BKE_gpencil_stroke_point_count(const bGPdata *gpd)
{
  int total_points = 0;

  if (gpd == nullptr) {
    return 0;
  }

  LISTBASE_FOREACH (const bGPDlayer *, gpl, &gpd->layers) {
    /* FIXME: For now, we just skip parented layers.
     * Otherwise, we have to update each frame to find
     * the current parent position/effects.
     */
    if (gpl->parent) {
      continue;
    }

    LISTBASE_FOREACH (const bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        total_points += gps->totpoints;
      }
    }
  }
  return total_points;
}

void BKE_gpencil_point_coords_get(bGPdata *gpd, GPencilPointCoordinates *elem_data)
{
  if (gpd == nullptr) {
    return;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* FIXME: For now, we just skip parented layers.
     * Otherwise, we have to update each frame to find
     * the current parent position/effects.
     */
    if (gpl->parent) {
      continue;
    }

    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        bGPDspoint *pt;
        int i;

        for (pt = gps->points, i = 0; i < gps->totpoints; pt++, i++) {
          copy_v3_v3(elem_data->co, &pt->x);
          elem_data->pressure = pt->pressure;
          elem_data++;
        }
      }
    }
  }
}

void BKE_gpencil_point_coords_apply(bGPdata *gpd, const GPencilPointCoordinates *elem_data)
{
  if (gpd == nullptr) {
    return;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* FIXME: For now, we just skip parented layers.
     * Otherwise, we have to update each frame to find
     * the current parent position/effects.
     */
    if (gpl->parent) {
      continue;
    }

    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        bGPDspoint *pt;
        int i;

        for (pt = gps->points, i = 0; i < gps->totpoints; pt++, i++) {
          copy_v3_v3(&pt->x, elem_data->co);
          pt->pressure = elem_data->pressure;
          elem_data++;
        }

        /* Distortion may mean we need to re-triangulate. */
        BKE_gpencil_stroke_geometry_update(gpd, gps);
      }
    }
  }
}

void BKE_gpencil_point_coords_apply_with_mat4(bGPdata *gpd,
                                              const GPencilPointCoordinates *elem_data,
                                              const float mat[4][4])
{
  if (gpd == nullptr) {
    return;
  }

  const float scalef = mat4_to_scale(mat);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* FIXME: For now, we just skip parented layers.
     * Otherwise, we have to update each frame to find
     * the current parent position/effects.
     */
    if (gpl->parent) {
      continue;
    }

    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        bGPDspoint *pt;
        int i;

        for (pt = gps->points, i = 0; i < gps->totpoints; pt++, i++) {
          mul_v3_m4v3(&pt->x, mat, elem_data->co);
          pt->pressure = elem_data->pressure * scalef;
          elem_data++;
        }

        /* Distortion may mean we need to re-triangulate. */
        BKE_gpencil_stroke_geometry_update(gpd, gps);
      }
    }
  }
}

void BKE_gpencil_stroke_set_random_color(bGPDstroke *gps)
{
  BLI_assert(gps->totpoints > 0);

  float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  bGPDspoint *pt = &gps->points[0];
  color[0] *= BLI_hash_int_01(BLI_hash_int_2d(gps->totpoints / 5, pt->x + pt->z));
  color[1] *= BLI_hash_int_01(BLI_hash_int_2d(gps->totpoints + pt->x, pt->y * pt->z + pt->x));
  color[2] *= BLI_hash_int_01(BLI_hash_int_2d(gps->totpoints - pt->x, pt->z * pt->x + pt->y));
  for (int i = 0; i < gps->totpoints; i++) {
    pt = &gps->points[i];
    copy_v4_v4(pt->vert_color, color);
  }
}

void BKE_gpencil_stroke_flip(bGPDstroke *gps)
{
  /* Reverse points. */
  BLI_array_reverse(gps->points, gps->totpoints);

  /* Reverse vertex groups if available. */
  if (gps->dvert) {
    BLI_array_reverse(gps->dvert, gps->totpoints);
  }
}

/* Temp data for storing information about an "island" of points
 * that should be kept when splitting up a stroke. Used in:
 * gpencil_stroke_delete_tagged_points()
 */
struct tGPDeleteIsland {
  int start_idx;
  int end_idx;
};

static void gpencil_stroke_join_islands(bGPdata *gpd,
                                        bGPDframe *gpf,
                                        bGPDstroke *gps_first,
                                        bGPDstroke *gps_last)
{
  bGPDspoint *pt = nullptr;
  bGPDspoint *pt_final = nullptr;
  const int totpoints = gps_first->totpoints + gps_last->totpoints;

  /* create new stroke */
  bGPDstroke *join_stroke = BKE_gpencil_stroke_duplicate(gps_first, false, true);

  join_stroke->points = (bGPDspoint *)MEM_callocN(sizeof(bGPDspoint) * totpoints, __func__);
  join_stroke->totpoints = totpoints;
  join_stroke->flag &= ~GP_STROKE_CYCLIC;

  /* copy points (last before) */
  int e1 = 0;
  int e2 = 0;
  float delta = 0.0f;

  for (int i = 0; i < totpoints; i++) {
    pt_final = &join_stroke->points[i];
    if (i < gps_last->totpoints) {
      pt = &gps_last->points[e1];
      e1++;
    }
    else {
      pt = &gps_first->points[e2];
      e2++;
    }

    /* copy current point */
    copy_v3_v3(&pt_final->x, &pt->x);
    pt_final->pressure = pt->pressure;
    pt_final->strength = pt->strength;
    pt_final->time = delta;
    pt_final->flag = pt->flag;
    copy_v4_v4(pt_final->vert_color, pt->vert_color);

    /* retiming with fixed time interval (we cannot determine real time) */
    delta += 0.01f;
  }

  /* Copy over vertex weight data (if available) */
  if ((gps_first->dvert != nullptr) || (gps_last->dvert != nullptr)) {
    join_stroke->dvert = (MDeformVert *)MEM_callocN(sizeof(MDeformVert) * totpoints, __func__);
    MDeformVert *dvert_src = nullptr;
    MDeformVert *dvert_dst = nullptr;

    /* Copy weights (last before). */
    e1 = 0;
    e2 = 0;
    for (int i = 0; i < totpoints; i++) {
      dvert_dst = &join_stroke->dvert[i];
      dvert_src = nullptr;
      if (i < gps_last->totpoints) {
        if (gps_last->dvert) {
          dvert_src = &gps_last->dvert[e1];
          e1++;
        }
      }
      else {
        if (gps_first->dvert) {
          dvert_src = &gps_first->dvert[e2];
          e2++;
        }
      }

      if ((dvert_src) && (dvert_src->dw)) {
        dvert_dst->dw = (MDeformWeight *)MEM_dupallocN(dvert_src->dw);
      }
    }
  }

  /* add new stroke at head */
  BLI_addhead(&gpf->strokes, join_stroke);
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, join_stroke);

  /* remove first stroke */
  BLI_remlink(&gpf->strokes, gps_first);
  BKE_gpencil_free_stroke(gps_first);

  /* remove last stroke */
  BLI_remlink(&gpf->strokes, gps_last);
  BKE_gpencil_free_stroke(gps_last);
}

bGPDstroke *BKE_gpencil_stroke_delete_tagged_points(bGPdata *gpd,
                                                    bGPDframe *gpf,
                                                    bGPDstroke *gps,
                                                    bGPDstroke *next_stroke,
                                                    int tag_flags,
                                                    const bool select,
                                                    const bool flat_cap,
                                                    const int limit)
{
  /* The algorithm used here is as follows:
   * 1) We firstly identify the number of "islands" of non-tagged points
   *    which will all end up being in new strokes.
   *    - In the most extreme case (i.e. every other vert is a 1-vert island),
   *      we have at most `n / 2` islands
   *    - Once we start having larger islands than that, the number required
   *      becomes much less
   * 2) Each island gets converted to a new stroke
   * If the number of points is <= limit, the stroke is deleted. */

  tGPDeleteIsland *islands = (tGPDeleteIsland *)MEM_callocN(
      sizeof(tGPDeleteIsland) * (gps->totpoints + 1) / 2, "gp_point_islands");
  bool in_island = false;
  int num_islands = 0;

  bGPDstroke *new_stroke = nullptr;
  bGPDstroke *gps_first = nullptr;
  const bool is_cyclic = bool(gps->flag & GP_STROKE_CYCLIC);

  /* First Pass: Identify start/end of islands */
  bGPDspoint *pt = gps->points;
  for (int i = 0; i < gps->totpoints; i++, pt++) {
    if (pt->flag & tag_flags) {
      /* selected - stop accumulating to island */
      in_island = false;
    }
    else {
      /* unselected - start of a new island? */
      int idx;

      if (in_island) {
        /* extend existing island */
        idx = num_islands - 1;
        islands[idx].end_idx = i;
      }
      else {
        /* start of new island */
        in_island = true;
        num_islands++;

        idx = num_islands - 1;
        islands[idx].start_idx = islands[idx].end_idx = i;
      }
    }
  }

  /* Watch out for special case where No islands = All points selected = Delete Stroke only */
  if (num_islands) {
    /* There are islands, so create a series of new strokes,
     * adding them before the "next" stroke. */
    int idx;

    /* Create each new stroke... */
    for (idx = 0; idx < num_islands; idx++) {
      tGPDeleteIsland *island = &islands[idx];
      new_stroke = BKE_gpencil_stroke_duplicate(gps, false, true);
      if (flat_cap) {
        new_stroke->caps[1 - (idx % 2)] = GP_STROKE_CAP_FLAT;
      }

      /* if cyclic and first stroke, save to join later */
      if ((is_cyclic) && (gps_first == nullptr)) {
        gps_first = new_stroke;
      }

      new_stroke->flag &= ~GP_STROKE_CYCLIC;

      /* Compute new buffer size (+ 1 needed as the endpoint index is "inclusive") */
      new_stroke->totpoints = island->end_idx - island->start_idx + 1;

      /* Copy over the relevant point data */
      new_stroke->points = (bGPDspoint *)MEM_callocN(sizeof(bGPDspoint) * new_stroke->totpoints,
                                                     "gp delete stroke fragment");
      memcpy(static_cast<void *>(new_stroke->points),
             gps->points + island->start_idx,
             sizeof(bGPDspoint) * new_stroke->totpoints);

      /* Copy over vertex weight data (if available) */
      if (gps->dvert != nullptr) {
        /* Copy over the relevant vertex-weight points */
        new_stroke->dvert = (MDeformVert *)MEM_callocN(sizeof(MDeformVert) * new_stroke->totpoints,
                                                       "gp delete stroke fragment weight");
        memcpy(new_stroke->dvert,
               gps->dvert + island->start_idx,
               sizeof(MDeformVert) * new_stroke->totpoints);

        /* Copy weights */
        int e = island->start_idx;
        for (int i = 0; i < new_stroke->totpoints; i++) {
          MDeformVert *dvert_src = &gps->dvert[e];
          MDeformVert *dvert_dst = &new_stroke->dvert[i];
          if (dvert_src->dw) {
            dvert_dst->dw = (MDeformWeight *)MEM_dupallocN(dvert_src->dw);
          }
          e++;
        }
      }
      /* Each island corresponds to a new stroke.
       * We must adjust the timings of these new strokes:
       *
       * Each point's timing data is a delta from stroke's inittime, so as we erase some points
       * from the start of the stroke, we have to offset this inittime and all remaining points'
       * delta values. This way we get a new stroke with exactly the same timing as if user had
       * started drawing from the first non-removed point.
       */
      {
        bGPDspoint *pts;
        float delta = gps->points[island->start_idx].time;
        int j;

        new_stroke->inittime += double(delta);

        pts = new_stroke->points;
        for (j = 0; j < new_stroke->totpoints; j++, pts++) {
          /* Some points have time = 0, so check to not get negative time values.*/
          pts->time = max_ff(pts->time - delta, 0.0f);
          /* set flag for select again later */
          if (select == true) {
            pts->flag &= ~GP_SPOINT_SELECT;
            pts->flag |= GP_SPOINT_TAG;
          }
        }
      }

      /* Add new stroke to the frame or delete if below limit */
      if ((limit > 0) && (new_stroke->totpoints <= limit)) {
        if (gps_first == new_stroke) {
          gps_first = nullptr;
        }
        BKE_gpencil_free_stroke(new_stroke);
      }
      else {
        /* Calc geometry data. */
        BKE_gpencil_stroke_geometry_update(gpd, new_stroke);

        if (next_stroke) {
          BLI_insertlinkbefore(&gpf->strokes, next_stroke, new_stroke);
        }
        else {
          BLI_addtail(&gpf->strokes, new_stroke);
        }
      }
    }
    /* if cyclic, need to join last stroke with first stroke */
    if ((is_cyclic) && (gps_first != nullptr) && (gps_first != new_stroke)) {
      gpencil_stroke_join_islands(gpd, gpf, gps_first, new_stroke);
    }
  }

  /* free islands */
  MEM_freeN(islands);

  /* Delete the old stroke */
  BLI_remlink(&gpf->strokes, gps);
  BKE_gpencil_free_stroke(gps);

  return new_stroke;
}

void BKE_gpencil_curve_delete_tagged_points(bGPdata *gpd,
                                            bGPDframe *gpf,
                                            bGPDstroke *gps,
                                            bGPDstroke *next_stroke,
                                            bGPDcurve *gpc,
                                            int tag_flags)
{
  if (gpc == nullptr) {
    return;
  }
  const bool is_cyclic = gps->flag & GP_STROKE_CYCLIC;
  const int idx_last = gpc->tot_curve_points - 1;
  bGPDstroke *gps_first = nullptr;
  bGPDstroke *gps_last = nullptr;

  int idx_start = 0;
  int idx_end = 0;
  bool prev_selected = gpc->curve_points[0].flag & tag_flags;
  for (int i = 1; i < gpc->tot_curve_points; i++) {
    bool selected = gpc->curve_points[i].flag & tag_flags;
    if (prev_selected == true && selected == false) {
      idx_start = i;
    }
    /* Island ends if the current point is selected or if we reached the end of the stroke */
    if ((prev_selected == false && selected == true) || (selected == false && i == idx_last)) {

      idx_end = selected ? i - 1 : i;
      int island_length = idx_end - idx_start + 1;

      /* If an island has only a single curve point, there is no curve segment, so skip island */
      if (island_length == 1) {
        if (is_cyclic) {
          if (idx_start > 0 && idx_end < idx_last) {
            prev_selected = selected;
            continue;
          }
        }
        else {
          prev_selected = selected;
          continue;
        }
      }

      bGPDstroke *new_stroke = BKE_gpencil_stroke_duplicate(gps, false, false);
      new_stroke->points = nullptr;
      new_stroke->flag &= ~GP_STROKE_CYCLIC;
      new_stroke->editcurve = BKE_gpencil_stroke_editcurve_new(island_length);

      if (gps_first == nullptr) {
        gps_first = new_stroke;
      }

      bGPDcurve *new_gpc = new_stroke->editcurve;
      memcpy(new_gpc->curve_points,
             gpc->curve_points + idx_start,
             sizeof(bGPDcurve_point) * island_length);

      BKE_gpencil_editcurve_recalculate_handles(new_stroke);
      new_stroke->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;

      /* Calc geometry data. */
      BKE_gpencil_stroke_geometry_update(gpd, new_stroke);

      if (next_stroke) {
        BLI_insertlinkbefore(&gpf->strokes, next_stroke, new_stroke);
      }
      else {
        BLI_addtail(&gpf->strokes, new_stroke);
      }

      gps_last = new_stroke;
    }
    prev_selected = selected;
  }

  /* join first and last stroke if cyclic */
  if (is_cyclic && gps_first != nullptr && gps_last != nullptr && gps_first != gps_last) {
    bGPDcurve *gpc_first = gps_first->editcurve;
    bGPDcurve *gpc_last = gps_last->editcurve;
    int first_tot_points = gpc_first->tot_curve_points;
    int old_tot_points = gpc_last->tot_curve_points;

    gpc_last->tot_curve_points = first_tot_points + old_tot_points;
    gpc_last->curve_points = (bGPDcurve_point *)MEM_recallocN(
        gpc_last->curve_points, sizeof(bGPDcurve_point) * gpc_last->tot_curve_points);
    /* copy data from first to last */
    memcpy(gpc_last->curve_points + old_tot_points,
           gpc_first->curve_points,
           sizeof(bGPDcurve_point) * first_tot_points);

    BKE_gpencil_editcurve_recalculate_handles(gps_last);
    gps_last->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;

    /* Calc geometry data. */
    BKE_gpencil_stroke_geometry_update(gpd, gps_last);

    /* remove first one */
    BLI_remlink(&gpf->strokes, gps_first);
    BKE_gpencil_free_stroke(gps_first);
  }

  /* Delete the old stroke */
  BLI_remlink(&gpf->strokes, gps);
  BKE_gpencil_free_stroke(gps);
}

/* Helper: copy point between strokes */
static void gpencil_stroke_copy_point(bGPDstroke *gps,
                                      MDeformVert *dvert,
                                      bGPDspoint *point,
                                      const float delta[3],
                                      float pressure,
                                      float strength,
                                      float deltatime)
{
  bGPDspoint *newpoint;

  gps->points = (bGPDspoint *)MEM_reallocN(gps->points, sizeof(bGPDspoint) * (gps->totpoints + 1));
  if (gps->dvert != nullptr) {
    gps->dvert = (MDeformVert *)MEM_reallocN(gps->dvert,
                                             sizeof(MDeformVert) * (gps->totpoints + 1));
  }
  else {
    /* If destination has weight add weight to origin. */
    if (dvert != nullptr) {
      gps->dvert = (MDeformVert *)MEM_callocN(sizeof(MDeformVert) * (gps->totpoints + 1),
                                              __func__);
    }
  }

  gps->totpoints++;
  newpoint = &gps->points[gps->totpoints - 1];

  newpoint->x = point->x * delta[0];
  newpoint->y = point->y * delta[1];
  newpoint->z = point->z * delta[2];
  newpoint->flag = point->flag;
  newpoint->pressure = pressure;
  newpoint->strength = strength;
  newpoint->time = point->time + deltatime;
  copy_v4_v4(newpoint->vert_color, point->vert_color);

  if (gps->dvert != nullptr) {
    MDeformVert *newdvert = &gps->dvert[gps->totpoints - 1];

    if (dvert != nullptr) {
      newdvert->totweight = dvert->totweight;
      newdvert->dw = (MDeformWeight *)MEM_dupallocN(dvert->dw);
    }
    else {
      newdvert->totweight = 0;
      newdvert->dw = nullptr;
    }
  }
}

void BKE_gpencil_stroke_join(bGPDstroke *gps_a,
                             bGPDstroke *gps_b,
                             const bool leave_gaps,
                             const bool fit_thickness,
                             const bool smooth,
                             bool auto_flip)
{
  bGPDspoint point;
  bGPDspoint *pt;
  int i;
  const float delta[3] = {1.0f, 1.0f, 1.0f};
  float deltatime = 0.0f;

  /* sanity checks */
  if (ELEM(nullptr, gps_a, gps_b)) {
    return;
  }

  if ((gps_a->totpoints == 0) || (gps_b->totpoints == 0)) {
    return;
  }

  if (auto_flip) {
    /* define start and end points of each stroke */
    float start_a[3], start_b[3], end_a[3], end_b[3];
    pt = &gps_a->points[0];
    copy_v3_v3(start_a, &pt->x);

    pt = &gps_a->points[gps_a->totpoints - 1];
    copy_v3_v3(end_a, &pt->x);

    pt = &gps_b->points[0];
    copy_v3_v3(start_b, &pt->x);

    pt = &gps_b->points[gps_b->totpoints - 1];
    copy_v3_v3(end_b, &pt->x);

    /* Check if need flip strokes. */
    float dist = len_squared_v3v3(end_a, start_b);
    bool flip_a = false;
    bool flip_b = false;
    float lowest = dist;

    dist = len_squared_v3v3(end_a, end_b);
    if (dist < lowest) {
      lowest = dist;
      flip_a = false;
      flip_b = true;
    }

    dist = len_squared_v3v3(start_a, start_b);
    if (dist < lowest) {
      lowest = dist;
      flip_a = true;
      flip_b = false;
    }

    dist = len_squared_v3v3(start_a, end_b);
    if (dist < lowest) {
      lowest = dist;
      flip_a = true;
      flip_b = true;
    }

    if (flip_a) {
      BKE_gpencil_stroke_flip(gps_a);
    }
    if (flip_b) {
      BKE_gpencil_stroke_flip(gps_b);
    }
  }

  /* don't visibly link the first and last points? */
  if (leave_gaps) {
    /* 1st: add one tail point to start invisible area */
    point = blender::dna::shallow_copy(gps_a->points[gps_a->totpoints - 1]);
    deltatime = point.time;

    gpencil_stroke_copy_point(gps_a, nullptr, &point, delta, 0.0f, 0.0f, 0.0f);

    /* 2nd: add one head point to finish invisible area */
    point = blender::dna::shallow_copy(gps_b->points[0]);
    gpencil_stroke_copy_point(gps_a, nullptr, &point, delta, 0.0f, 0.0f, deltatime);
  }

  /* Ratio to apply in the points to keep the same thickness in the joined stroke using the
   * destination stroke thickness. */
  const float ratio = (fit_thickness && gps_a->thickness > 0.0f) ?
                          float(gps_b->thickness) / float(gps_a->thickness) :
                          1.0f;

  /* 3rd: add all points */
  const int totpoints_a = gps_a->totpoints;
  for (i = 0, pt = gps_b->points; i < gps_b->totpoints && pt; i++, pt++) {
    MDeformVert *dvert = (gps_b->dvert) ? &gps_b->dvert[i] : nullptr;
    gpencil_stroke_copy_point(
        gps_a, dvert, pt, delta, pt->pressure * ratio, pt->strength, deltatime);
  }
  /* Smooth the join to avoid hard thickness changes. */
  if (smooth) {
    const int sample_points = 8;
    /* Get the segment to smooth using n points on each side of the join. */
    int start = MAX2(0, totpoints_a - sample_points);
    int end = MIN2(gps_a->totpoints - 1, start + (sample_points * 2));
    const int len = (end - start);
    float step = 1.0f / ((len / 2) + 1);

    /* Calc the average pressure. */
    float avg_pressure = 0.0f;
    for (i = start; i < end; i++) {
      pt = &gps_a->points[i];
      avg_pressure += pt->pressure;
    }
    avg_pressure = avg_pressure / len;

    /* Smooth segment thickness and position. */
    float ratio = step;
    for (i = start; i < end; i++) {
      pt = &gps_a->points[i];
      pt->pressure += (avg_pressure - pt->pressure) * ratio;
      BKE_gpencil_stroke_smooth_point(gps_a, i, ratio * 0.6f, 2, false, true, gps_a);

      ratio += step;
      /* In the center, reverse the ratio. */
      if (ratio > 1.0f) {
        ratio = ratio - step - step;
        step *= -1.0f;
      }
    }
  }
}

void BKE_gpencil_stroke_start_set(bGPDstroke *gps, int start_idx)
{
  if ((start_idx < 1) || (start_idx >= gps->totpoints) || (gps->totpoints < 2)) {
    return;
  }

  /* Only cyclic strokes. */
  if ((gps->flag & GP_STROKE_CYCLIC) == 0) {
    return;
  }

  bGPDstroke *gps_b = BKE_gpencil_stroke_duplicate(gps, true, false);
  BKE_gpencil_stroke_trim_points(gps_b, 0, start_idx - 1, true);
  BKE_gpencil_stroke_trim_points(gps, start_idx, gps->totpoints - 1, true);

  /* Join both strokes. */
  BKE_gpencil_stroke_join(gps, gps_b, false, false, false, false);

  BKE_gpencil_free_stroke(gps_b);
}

void BKE_gpencil_stroke_copy_to_keyframes(
    bGPdata *gpd, bGPDlayer *gpl, bGPDframe *gpf, bGPDstroke *gps, const bool tail)
{
  GHash *frame_list = BLI_ghash_int_new_ex(__func__, 64);
  BKE_gpencil_frame_selected_hash(gpd, frame_list);

  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, frame_list) {
    int cfra = POINTER_AS_INT(BLI_ghashIterator_getKey(&gh_iter));

    if (gpf->framenum != cfra) {
      bGPDframe *gpf_new = BKE_gpencil_layer_frame_find(gpl, cfra);
      if (gpf_new == nullptr) {
        gpf_new = BKE_gpencil_frame_addnew(gpl, cfra);
      }

      if (gpf_new == nullptr) {
        continue;
      }

      bGPDstroke *gps_new = BKE_gpencil_stroke_duplicate(gps, true, true);
      if (gps_new == nullptr) {
        continue;
      }

      if (tail) {
        BLI_addhead(&gpf_new->strokes, gps_new);
      }
      else {
        BLI_addtail(&gpf_new->strokes, gps_new);
      }
    }
  }

  /* Free hash table. */
  BLI_ghash_free(frame_list, nullptr, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Uniform Subdivide
 * \{ */

struct tSamplePoint {
  struct tSamplePoint *next, *prev;
  float x, y, z;
  float pressure, strength, time;
  float vertex_color[4];
  struct MDeformWeight *dw;
  int totweight;
};

struct tSampleEdge {
  float length_sq;
  tSamplePoint *from;
  tSamplePoint *to;
};

/* Helper: creates a tSamplePoint from a bGPDspoint and (optionally) a MDeformVert. */
static tSamplePoint *new_sample_point_from_gp_point(const bGPDspoint *pt, const MDeformVert *dvert)
{
  tSamplePoint *new_pt = MEM_cnew<tSamplePoint>(__func__);
  copy_v3_v3(&new_pt->x, &pt->x);
  new_pt->pressure = pt->pressure;
  new_pt->strength = pt->strength;
  new_pt->time = pt->time;
  copy_v4_v4((float *)&new_pt->vertex_color, (float *)&pt->vert_color);
  if (dvert != nullptr) {
    new_pt->totweight = dvert->totweight;
    new_pt->dw = (MDeformWeight *)MEM_callocN(sizeof(MDeformWeight) * new_pt->totweight, __func__);
    for (uint i = 0; i < new_pt->totweight; ++i) {
      MDeformWeight *dw = &new_pt->dw[i];
      MDeformWeight *dw_from = &dvert->dw[i];
      dw->def_nr = dw_from->def_nr;
      dw->weight = dw_from->weight;
    }
  }
  return new_pt;
}

/* Helper: creates a tSampleEdge from two tSamplePoints. Also calculates the length (squared) of
 * the edge. */
static tSampleEdge *new_sample_edge_from_sample_points(tSamplePoint *from, tSamplePoint *to)
{
  tSampleEdge *new_edge = MEM_cnew<tSampleEdge>(__func__);
  new_edge->from = from;
  new_edge->to = to;
  new_edge->length_sq = len_squared_v3v3(&from->x, &to->x);
  return new_edge;
}

void BKE_gpencil_stroke_uniform_subdivide(bGPdata *gpd,
                                          bGPDstroke *gps,
                                          const uint32_t target_number,
                                          const bool select)
{
  /* Stroke needs at least two points and strictly less points than the target number. */
  if (gps == nullptr || gps->totpoints < 2 || gps->totpoints >= target_number) {
    return;
  }

  const int totpoints = gps->totpoints;
  const bool has_dverts = (gps->dvert != nullptr);
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC);

  ListBase points = {nullptr, nullptr};
  Heap *edges = BLI_heap_new();

  /* Add all points into list. */
  for (uint32_t i = 0; i < totpoints; ++i) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = has_dverts ? &gps->dvert[i] : nullptr;
    tSamplePoint *sp = new_sample_point_from_gp_point(pt, dvert);
    BLI_addtail(&points, sp);
  }

  /* Iterate over edges and insert them into the heap. */
  for (tSamplePoint *pt = ((tSamplePoint *)points.first)->next; pt != nullptr; pt = pt->next) {
    tSampleEdge *se = new_sample_edge_from_sample_points(pt->prev, pt);
    /* BLI_heap is a min-heap, but we need the largest key to be at the top, so we take the
     * negative of the squared length. */
    BLI_heap_insert(edges, -(se->length_sq), se);
  }

  if (is_cyclic) {
    tSamplePoint *sp_first = (tSamplePoint *)points.first;
    tSamplePoint *sp_last = (tSamplePoint *)points.last;
    tSampleEdge *se = new_sample_edge_from_sample_points(sp_last, sp_first);
    BLI_heap_insert(edges, -(se->length_sq), se);
  }

  int num_points_needed = target_number - totpoints;
  BLI_assert(num_points_needed > 0);

  while (num_points_needed > 0) {
    tSampleEdge *se = (tSampleEdge *)BLI_heap_pop_min(edges);
    tSamplePoint *sp = se->from;
    tSamplePoint *sp_next = se->to;

    /* Subdivide the edge. */
    tSamplePoint *new_sp = MEM_cnew<tSamplePoint>(__func__);
    interp_v3_v3v3(&new_sp->x, &sp->x, &sp_next->x, 0.5f);
    new_sp->pressure = interpf(sp->pressure, sp_next->pressure, 0.5f);
    new_sp->strength = interpf(sp->strength, sp_next->strength, 0.5f);
    new_sp->time = interpf(sp->time, sp_next->time, 0.5f);
    interp_v4_v4v4((float *)&new_sp->vertex_color,
                   (float *)&sp->vertex_color,
                   (float *)&sp_next->vertex_color,
                   0.5f);
    if (sp->dw && sp_next->dw) {
      new_sp->totweight = MIN2(sp->totweight, sp_next->totweight);
      new_sp->dw = (MDeformWeight *)MEM_callocN(sizeof(MDeformWeight) * new_sp->totweight,
                                                __func__);
      for (uint32_t i = 0; i < new_sp->totweight; ++i) {
        MDeformWeight *dw = &new_sp->dw[i];
        MDeformWeight *dw_from = &sp->dw[i];
        MDeformWeight *dw_to = &sp_next->dw[i];
        dw->def_nr = dw_from->def_nr;
        dw->weight = interpf(dw_from->weight, dw_to->weight, 0.5f);
      }
    }
    BLI_insertlinkafter(&points, sp, new_sp);

    tSampleEdge *se_prev = new_sample_edge_from_sample_points(sp, new_sp);
    tSampleEdge *se_next = new_sample_edge_from_sample_points(new_sp, sp_next);
    BLI_heap_insert(edges, -(se_prev->length_sq), se_prev);
    BLI_heap_insert(edges, -(se_next->length_sq), se_next);

    MEM_freeN(se);
    num_points_needed--;
  }

  /* Edges are no longer needed. Heap is freed. */
  BLI_heap_free(edges, (HeapFreeFP)MEM_freeN);

  gps->totpoints = target_number;
  gps->points = (bGPDspoint *)MEM_recallocN(gps->points, sizeof(bGPDspoint) * gps->totpoints);
  if (has_dverts) {
    gps->dvert = (MDeformVert *)MEM_recallocN(gps->dvert, sizeof(MDeformVert) * gps->totpoints);
  }

  /* Convert list back to stroke point array. */
  tSamplePoint *sp = (tSamplePoint *)points.first;
  for (uint32_t i = 0; i < gps->totpoints && sp; ++i, sp = sp->next) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = &gps->dvert[i];

    copy_v3_v3(&pt->x, &sp->x);
    pt->pressure = sp->pressure;
    pt->strength = sp->strength;
    pt->time = sp->time;
    copy_v4_v4((float *)&pt->vert_color, (float *)&sp->vertex_color);

    if (sp->dw) {
      dvert->totweight = sp->totweight;
      dvert->dw = (MDeformWeight *)MEM_callocN(sizeof(MDeformWeight) * dvert->totweight, __func__);
      for (uint32_t j = 0; j < dvert->totweight; ++j) {
        MDeformWeight *dw = &dvert->dw[j];
        MDeformWeight *dw_from = &sp->dw[j];
        dw->def_nr = dw_from->def_nr;
        dw->weight = dw_from->weight;
      }
    }
    if (select) {
      pt->flag |= GP_SPOINT_SELECT;
    }
  }

  if (select) {
    gps->flag |= GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_set(gpd, gps);
  }

  /* Free the sample points. Important to use the mutable loop here because we are erasing the list
   * elements. */
  LISTBASE_FOREACH_MUTABLE (tSamplePoint *, temp, &points) {
    if (temp->dw != nullptr) {
      MEM_freeN(temp->dw);
    }
    MEM_SAFE_FREE(temp);
  }

  /* Update the geometry of the stroke. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

void BKE_gpencil_stroke_to_view_space(bGPDstroke *gps,
                                      float viewmat[4][4],
                                      const float diff_mat[4][4])
{
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    /* Point to parent space. */
    mul_v3_m4v3(&pt->x, diff_mat, &pt->x);
    /* point to view space */
    mul_m4_v3(viewmat, &pt->x);
  }
}

void BKE_gpencil_stroke_from_view_space(bGPDstroke *gps,
                                        float viewinv[4][4],
                                        const float diff_mat[4][4])
{
  float inverse_diff_mat[4][4];
  invert_m4_m4(inverse_diff_mat, diff_mat);

  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    mul_v3_m4v3(&pt->x, viewinv, &pt->x);
    mul_m4_v3(inverse_diff_mat, &pt->x);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke to Perimeter
 * \{ */

struct tPerimeterPoint {
  struct tPerimeterPoint *next, *prev;
  float x, y, z;
};

static tPerimeterPoint *new_perimeter_point(const float pt[3])
{
  tPerimeterPoint *new_pt = MEM_cnew<tPerimeterPoint>(__func__);
  copy_v3_v3(&new_pt->x, pt);
  return new_pt;
}

static int generate_arc_from_point_to_point(ListBase *list,
                                            tPerimeterPoint *from,
                                            tPerimeterPoint *to,
                                            float center_pt[3],
                                            int subdivisions,
                                            bool clockwise)
{
  float vec_from[2];
  float vec_to[2];
  sub_v2_v2v2(vec_from, &from->x, center_pt);
  sub_v2_v2v2(vec_to, &to->x, center_pt);
  if (is_zero_v2(vec_from) || is_zero_v2(vec_to)) {
    return 0;
  }

  float dot = dot_v2v2(vec_from, vec_to);
  float det = cross_v2v2(vec_from, vec_to);
  float angle = clockwise ? M_PI - atan2f(-det, -dot) : atan2f(-det, -dot) + M_PI;

  /* Number of points is 2^(n+1) + 1 on half a circle (n=subdivisions)
   * so we multiply by (angle / pi) to get the right amount of
   * points to insert. */
  int num_points = int(((1 << (subdivisions + 1)) - 1) * (angle / M_PI));
  if (num_points > 0) {
    float angle_incr = angle / float(num_points);

    float vec_p[3];
    float vec_t[3];
    float tmp_angle;
    tPerimeterPoint *last_point;
    if (clockwise) {
      last_point = to;
      copy_v2_v2(vec_t, vec_to);
    }
    else {
      last_point = from;
      copy_v2_v2(vec_t, vec_from);
    }

    for (int i = 0; i < num_points - 1; i++) {
      tmp_angle = (i + 1) * angle_incr;

      rotate_v2_v2fl(vec_p, vec_t, tmp_angle);
      add_v2_v2(vec_p, center_pt);
      vec_p[2] = center_pt[2];

      tPerimeterPoint *new_point = new_perimeter_point(vec_p);
      if (clockwise) {
        BLI_insertlinkbefore(list, last_point, new_point);
      }
      else {
        BLI_insertlinkafter(list, last_point, new_point);
      }

      last_point = new_point;
    }

    return num_points - 1;
  }

  return 0;
}

static int generate_semi_circle_from_point_to_point(ListBase *list,
                                                    tPerimeterPoint *from,
                                                    tPerimeterPoint *to,
                                                    int subdivisions)
{
  int num_points = (1 << (subdivisions + 1)) + 1;
  float center_pt[3];
  interp_v3_v3v3(center_pt, &from->x, &to->x, 0.5f);

  float vec_center[2];
  sub_v2_v2v2(vec_center, &from->x, center_pt);
  if (is_zero_v2(vec_center)) {
    return 0;
  }

  float vec_p[3];
  float angle_incr = M_PI / (float(num_points) - 1);

  tPerimeterPoint *last_point = from;
  for (int i = 1; i < num_points; i++) {
    float angle = i * angle_incr;

    /* Rotate vector around point to get perimeter points. */
    rotate_v2_v2fl(vec_p, vec_center, angle);
    add_v2_v2(vec_p, center_pt);
    vec_p[2] = center_pt[2];

    tPerimeterPoint *new_point = new_perimeter_point(vec_p);
    BLI_insertlinkafter(list, last_point, new_point);

    last_point = new_point;
  }

  return num_points - 1;
}

static int generate_perimeter_cap(const float point[4],
                                  const float other_point[4],
                                  float radius,
                                  ListBase *list,
                                  int subdivisions,
                                  short cap_type)
{
  float cap_vec[2];
  sub_v2_v2v2(cap_vec, other_point, point);
  normalize_v2(cap_vec);

  float cap_nvec[2];
  if (is_zero_v2(cap_vec)) {
    cap_nvec[0] = 0;
    cap_nvec[1] = radius;
  }
  else {
    cap_nvec[0] = -cap_vec[1];
    cap_nvec[1] = cap_vec[0];
    mul_v2_fl(cap_nvec, radius);
  }
  float cap_nvec_inv[2];
  negate_v2_v2(cap_nvec_inv, cap_nvec);

  float vec_perimeter[3];
  copy_v3_v3(vec_perimeter, point);
  add_v2_v2(vec_perimeter, cap_nvec);

  float vec_perimeter_inv[3];
  copy_v3_v3(vec_perimeter_inv, point);
  add_v2_v2(vec_perimeter_inv, cap_nvec_inv);

  tPerimeterPoint *p_pt = new_perimeter_point(vec_perimeter);
  tPerimeterPoint *p_pt_inv = new_perimeter_point(vec_perimeter_inv);

  BLI_addtail(list, p_pt);
  BLI_addtail(list, p_pt_inv);

  int num_points = 0;
  if (cap_type == GP_STROKE_CAP_ROUND) {
    num_points += generate_semi_circle_from_point_to_point(list, p_pt, p_pt_inv, subdivisions);
  }

  return num_points + 2;
}

/**
 * Calculate the perimeter (outline) of a stroke as list of tPerimeterPoint.
 * \param subdivisions: Number of subdivisions for the start and end caps
 * \return: list of tPerimeterPoint
 */
static ListBase *gpencil_stroke_perimeter_ex(const bGPdata *gpd,
                                             const bGPDlayer *gpl,
                                             const bGPDstroke *gps,
                                             int subdivisions,
                                             const float thickness_chg,
                                             int *r_num_perimeter_points)
{
  /* sanity check */
  if (gps->totpoints < 1) {
    return nullptr;
  }

  float defaultpixsize = 1000.0f / gpd->pixfactor;
  float ovr_radius = thickness_chg / defaultpixsize / 2.0f;
  float stroke_radius = ((gps->thickness + gpl->line_change) / defaultpixsize) / 2.0f;
  stroke_radius = max_ff(stroke_radius - ovr_radius, 0.0f);

  ListBase *perimeter_right_side = MEM_cnew<ListBase>(__func__);
  ListBase *perimeter_left_side = MEM_cnew<ListBase>(__func__);
  int num_perimeter_points = 0;

  bGPDspoint *first = &gps->points[0];
  bGPDspoint *last = &gps->points[gps->totpoints - 1];

  float first_radius = stroke_radius * first->pressure;
  float last_radius = stroke_radius * last->pressure;

  bGPDspoint *first_next;
  bGPDspoint *last_prev;
  if (gps->totpoints > 1) {
    first_next = &gps->points[1];
    last_prev = &gps->points[gps->totpoints - 2];
  }
  else {
    first_next = first;
    last_prev = last;
  }

  float first_pt[3];
  float last_pt[3];
  float first_next_pt[3];
  float last_prev_pt[3];
  copy_v3_v3(first_pt, &first->x);
  copy_v3_v3(last_pt, &last->x);
  copy_v3_v3(first_next_pt, &first_next->x);
  copy_v3_v3(last_prev_pt, &last_prev->x);

  /* Edge-case if single point. */
  if (gps->totpoints == 1) {
    first_next_pt[0] += 1.0f;
    last_prev_pt[0] -= 1.0f;
  }

  /* Generate points for start cap. */
  num_perimeter_points += generate_perimeter_cap(
      first_pt, first_next_pt, first_radius, perimeter_right_side, subdivisions, gps->caps[0]);

  /* Generate perimeter points. */
  float curr_pt[3], next_pt[3], prev_pt[3];
  float vec_next[2], vec_prev[2];
  float nvec_next[2], nvec_prev[2];
  float nvec_next_pt[3], nvec_prev_pt[3];
  float vec_tangent[2];

  float vec_miter_left[2], vec_miter_right[2];
  float miter_left_pt[3], miter_right_pt[3];

  for (int i = 1; i < gps->totpoints - 1; i++) {
    bGPDspoint *curr = &gps->points[i];
    bGPDspoint *prev = &gps->points[i - 1];
    bGPDspoint *next = &gps->points[i + 1];
    float radius = stroke_radius * curr->pressure;

    copy_v3_v3(curr_pt, &curr->x);
    copy_v3_v3(next_pt, &next->x);
    copy_v3_v3(prev_pt, &prev->x);

    sub_v2_v2v2(vec_prev, curr_pt, prev_pt);
    sub_v2_v2v2(vec_next, next_pt, curr_pt);
    float prev_length = len_v2(vec_prev);
    float next_length = len_v2(vec_next);

    if (normalize_v2(vec_prev) == 0.0f) {
      vec_prev[0] = 1.0f;
      vec_prev[1] = 0.0f;
    }
    if (normalize_v2(vec_next) == 0.0f) {
      vec_next[0] = 1.0f;
      vec_next[1] = 0.0f;
    }

    nvec_prev[0] = -vec_prev[1];
    nvec_prev[1] = vec_prev[0];

    nvec_next[0] = -vec_next[1];
    nvec_next[1] = vec_next[0];

    add_v2_v2v2(vec_tangent, vec_prev, vec_next);
    if (normalize_v2(vec_tangent) == 0.0f) {
      copy_v2_v2(vec_tangent, nvec_prev);
    }

    vec_miter_left[0] = -vec_tangent[1];
    vec_miter_left[1] = vec_tangent[0];

    /* calculate miter length */
    float an1 = dot_v2v2(vec_miter_left, nvec_prev);
    if (an1 == 0.0f) {
      an1 = 1.0f;
    }
    float miter_length = radius / an1;
    if (miter_length <= 0.0f) {
      miter_length = 0.01f;
    }

    normalize_v2_length(vec_miter_left, miter_length);

    copy_v2_v2(vec_miter_right, vec_miter_left);
    negate_v2(vec_miter_right);

    float angle = dot_v2v2(vec_next, nvec_prev);
    /* Add two points if angle is close to being straight. */
    if (fabsf(angle) < 0.0001f) {
      normalize_v2_length(nvec_prev, radius);
      normalize_v2_length(nvec_next, radius);

      copy_v3_v3(nvec_prev_pt, curr_pt);
      add_v2_v2(nvec_prev_pt, nvec_prev);

      copy_v3_v3(nvec_next_pt, curr_pt);
      negate_v2(nvec_next);
      add_v2_v2(nvec_next_pt, nvec_next);

      tPerimeterPoint *normal_prev = new_perimeter_point(nvec_prev_pt);
      tPerimeterPoint *normal_next = new_perimeter_point(nvec_next_pt);

      BLI_addtail(perimeter_left_side, normal_prev);
      BLI_addtail(perimeter_right_side, normal_next);
      num_perimeter_points += 2;
    }
    else {
      /* bend to the left */
      if (angle < 0.0f) {
        normalize_v2_length(nvec_prev, radius);
        normalize_v2_length(nvec_next, radius);

        copy_v3_v3(nvec_prev_pt, curr_pt);
        add_v2_v2(nvec_prev_pt, nvec_prev);

        copy_v3_v3(nvec_next_pt, curr_pt);
        add_v2_v2(nvec_next_pt, nvec_next);

        tPerimeterPoint *normal_prev = new_perimeter_point(nvec_prev_pt);
        tPerimeterPoint *normal_next = new_perimeter_point(nvec_next_pt);

        BLI_addtail(perimeter_left_side, normal_prev);
        BLI_addtail(perimeter_left_side, normal_next);
        num_perimeter_points += 2;

        num_perimeter_points += generate_arc_from_point_to_point(
            perimeter_left_side, normal_prev, normal_next, curr_pt, subdivisions, true);

        if (miter_length < prev_length && miter_length < next_length) {
          copy_v3_v3(miter_right_pt, curr_pt);
          add_v2_v2(miter_right_pt, vec_miter_right);
        }
        else {
          copy_v3_v3(miter_right_pt, curr_pt);
          negate_v2(nvec_next);
          add_v2_v2(miter_right_pt, nvec_next);
        }

        tPerimeterPoint *miter_right = new_perimeter_point(miter_right_pt);
        BLI_addtail(perimeter_right_side, miter_right);
        num_perimeter_points++;
      }
      /* bend to the right */
      else {
        normalize_v2_length(nvec_prev, -radius);
        normalize_v2_length(nvec_next, -radius);

        copy_v3_v3(nvec_prev_pt, curr_pt);
        add_v2_v2(nvec_prev_pt, nvec_prev);

        copy_v3_v3(nvec_next_pt, curr_pt);
        add_v2_v2(nvec_next_pt, nvec_next);

        tPerimeterPoint *normal_prev = new_perimeter_point(nvec_prev_pt);
        tPerimeterPoint *normal_next = new_perimeter_point(nvec_next_pt);

        BLI_addtail(perimeter_right_side, normal_prev);
        BLI_addtail(perimeter_right_side, normal_next);
        num_perimeter_points += 2;

        num_perimeter_points += generate_arc_from_point_to_point(
            perimeter_right_side, normal_prev, normal_next, curr_pt, subdivisions, false);

        if (miter_length < prev_length && miter_length < next_length) {
          copy_v3_v3(miter_left_pt, curr_pt);
          add_v2_v2(miter_left_pt, vec_miter_left);
        }
        else {
          copy_v3_v3(miter_left_pt, curr_pt);
          negate_v2(nvec_prev);
          add_v2_v2(miter_left_pt, nvec_prev);
        }

        tPerimeterPoint *miter_left = new_perimeter_point(miter_left_pt);
        BLI_addtail(perimeter_left_side, miter_left);
        num_perimeter_points++;
      }
    }
  }

  /* generate points for end cap */
  num_perimeter_points += generate_perimeter_cap(
      last_pt, last_prev_pt, last_radius, perimeter_right_side, subdivisions, gps->caps[1]);

  /* merge both sides to one list */
  BLI_listbase_reverse(perimeter_right_side);
  BLI_movelisttolist(perimeter_left_side,
                     perimeter_right_side);  // perimeter_left_side contains entire list
  ListBase *perimeter_list = perimeter_left_side;

  /* close by creating a point close to the first (make a small gap) */
  float close_pt[3];
  tPerimeterPoint *close_first = (tPerimeterPoint *)perimeter_list->first;
  tPerimeterPoint *close_last = (tPerimeterPoint *)perimeter_list->last;
  interp_v3_v3v3(close_pt, &close_last->x, &close_first->x, 0.99f);

  if (compare_v3v3(close_pt, &close_first->x, FLT_EPSILON) == false) {
    tPerimeterPoint *close_p_pt = new_perimeter_point(close_pt);
    BLI_addtail(perimeter_list, close_p_pt);
    num_perimeter_points++;
  }

  /* free temp data */
  BLI_freelistN(perimeter_right_side);
  MEM_freeN(perimeter_right_side);

  *r_num_perimeter_points = num_perimeter_points;
  return perimeter_list;
}

bGPDstroke *BKE_gpencil_stroke_perimeter_from_view(float viewmat[4][4],
                                                   bGPdata *gpd,
                                                   const bGPDlayer *gpl,
                                                   bGPDstroke *gps,
                                                   const int subdivisions,
                                                   const float diff_mat[4][4],
                                                   const float thickness_chg)
{
  if (gps->totpoints == 0) {
    return nullptr;
  }

  float viewinv[4][4];
  invert_m4_m4(viewinv, viewmat);

  /* Duplicate only points and fill data. Weight and Curve are not needed. */
  bGPDstroke *gps_temp = (bGPDstroke *)MEM_dupallocN(gps);
  gps_temp->prev = gps_temp->next = nullptr;
  gps_temp->triangles = (bGPDtriangle *)MEM_dupallocN(gps->triangles);
  gps_temp->points = (bGPDspoint *)MEM_dupallocN(gps->points);
  gps_temp->dvert = nullptr;
  gps_temp->editcurve = nullptr;

  const bool cyclic = ((gps_temp->flag & GP_STROKE_CYCLIC) != 0);

  /* If Cyclic, add a new point. */
  if (cyclic && (gps_temp->totpoints > 1)) {
    gps_temp->totpoints++;
    gps_temp->points = (bGPDspoint *)MEM_recallocN(
        gps_temp->points, sizeof(*gps_temp->points) * gps_temp->totpoints);
    bGPDspoint *pt_src = &gps_temp->points[0];
    bGPDspoint *pt_dst = &gps_temp->points[gps_temp->totpoints - 1];
    copy_v3_v3(&pt_dst->x, &pt_src->x);
    pt_dst->pressure = pt_src->pressure;
    pt_dst->strength = pt_src->strength;
    pt_dst->uv_fac = 1.0f;
    pt_dst->uv_rot = 0;
  }

  BKE_gpencil_stroke_to_view_space(gps_temp, viewmat, diff_mat);
  int num_perimeter_points = 0;
  ListBase *perimeter_points = gpencil_stroke_perimeter_ex(
      gpd, gpl, gps_temp, subdivisions, thickness_chg, &num_perimeter_points);

  if (num_perimeter_points == 0) {
    return nullptr;
  }

  /* Create new stroke. */
  bGPDstroke *perimeter_stroke = BKE_gpencil_stroke_new(gps_temp->mat_nr, num_perimeter_points, 1);

  int i = 0;
  LISTBASE_FOREACH_INDEX (tPerimeterPoint *, curr, perimeter_points, i) {
    bGPDspoint *pt = &perimeter_stroke->points[i];

    copy_v3_v3(&pt->x, &curr->x);
    pt->pressure = 0.0f;
    pt->strength = 1.0f;

    pt->flag |= GP_SPOINT_SELECT;
  }

  BKE_gpencil_stroke_from_view_space(perimeter_stroke, viewinv, diff_mat);

  /* Free temp data. */
  BLI_freelistN(perimeter_points);
  MEM_freeN(perimeter_points);

  /* Triangles cache needs to be recalculated. */
  BKE_gpencil_stroke_geometry_update(gpd, perimeter_stroke);

  perimeter_stroke->flag |= GP_STROKE_SELECT | GP_STROKE_CYCLIC;

  BKE_gpencil_free_stroke(gps_temp);

  return perimeter_stroke;
}

float BKE_gpencil_stroke_average_pressure_get(bGPDstroke *gps)
{

  if (gps->totpoints == 1) {
    return gps->points[0].pressure;
  }

  float tot = 0.0f;
  for (int i = 0; i < gps->totpoints; i++) {
    const bGPDspoint *pt = &gps->points[i];
    tot += pt->pressure;
  }

  return tot / float(gps->totpoints);
}

bool BKE_gpencil_stroke_is_pressure_constant(bGPDstroke *gps)
{
  if (gps->totpoints == 1) {
    return true;
  }

  const float first_pressure = gps->points[0].pressure;
  for (int i = 0; i < gps->totpoints; i++) {
    const bGPDspoint *pt = &gps->points[i];
    if (pt->pressure != first_pressure) {
      return false;
    }
  }

  return true;
}

/** \} */
