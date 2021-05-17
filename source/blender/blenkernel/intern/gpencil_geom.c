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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup bke
 */

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_heap.h"
#include "BLI_math_vector.h"
#include "BLI_polyfill_2d.h"

#include "BLT_translation.h"

#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_curve.h"
#include "BKE_gpencil_geom.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_object.h"

#include "DEG_depsgraph_query.h"

/* GP Object - Boundbox Support */
/**
 *Get min/max coordinate bounds for single stroke.
 * \param gps: Grease pencil stroke
 * \param use_select: Include only selected points
 * \param r_min: Result minimum coordinates
 * \param r_max: Result maximum coordinates
 * \return True if it was possible to calculate
 */
bool BKE_gpencil_stroke_minmax(const bGPDstroke *gps,
                               const bool use_select,
                               float r_min[3],
                               float r_max[3])
{
  const bGPDspoint *pt;
  int i;
  bool changed = false;

  if (ELEM(NULL, gps, r_min, r_max)) {
    return false;
  }

  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    if ((use_select == false) || (pt->flag & GP_SPOINT_SELECT)) {
      minmax_v3v3_v3(r_min, r_max, &pt->x);
      changed = true;
    }
  }
  return changed;
}

/**
 * Get min/max bounds of all strokes in grease pencil data-block.
 * \param gpd: Grease pencil datablock
 * \param r_min: Result minimum coordinates
 * \param r_max: Result maximum coordinates
 * \return True if it was possible to calculate
 */
bool BKE_gpencil_data_minmax(const bGPdata *gpd, float r_min[3], float r_max[3])
{
  bool changed = false;

  INIT_MINMAX(r_min, r_max);

  if (gpd == NULL) {
    return changed;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = gpl->actframe;

    if (gpf != NULL) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        changed |= BKE_gpencil_stroke_minmax(gps, false, r_min, r_max);
      }
    }
  }

  return changed;
}

/**
 * Compute center of bounding box.
 * \param gpd: Grease pencil data-block
 * \param r_centroid: Location of the center
 */
void BKE_gpencil_centroid_3d(bGPdata *gpd, float r_centroid[3])
{
  float min[3], max[3], tot[3];

  BKE_gpencil_data_minmax(gpd, min, max);

  add_v3_v3v3(tot, min, max);
  mul_v3_v3fl(r_centroid, tot, 0.5f);
}

/**
 * Compute stroke bounding box.
 * \param gps: Grease pencil Stroke
 */
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
  BoundBox *bb;
  bGPdata *gpd;
  float min[3], max[3];

  if (ob->runtime.bb == NULL) {
    ob->runtime.bb = MEM_callocN(sizeof(BoundBox), "GPencil boundbox");
  }

  bb = ob->runtime.bb;
  gpd = ob->data;

  if (!BKE_gpencil_data_minmax(gpd, min, max)) {
    min[0] = min[1] = min[2] = -1.0f;
    max[0] = max[1] = max[2] = 1.0f;
  }

  BKE_boundbox_init_from_minmax(bb, min, max);

  bb->flag &= ~BOUNDBOX_DIRTY;
}

/**
 * Get grease pencil object bounding box.
 * \param ob: Grease pencil object
 * \return Bounding box
 */
BoundBox *BKE_gpencil_boundbox_get(Object *ob)
{
  if (ELEM(NULL, ob, ob->data)) {
    return NULL;
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
  if (!ELEM(ob_orig, NULL, ob)) {
    if (ob_orig->runtime.bb == NULL) {
      ob_orig->runtime.bb = MEM_callocN(sizeof(BoundBox), "GPencil boundbox");
    }
    for (int i = 0; i < 8; i++) {
      copy_v3_v3(ob_orig->runtime.bb->vec[i], ob->runtime.bb->vec[i]);
    }
  }

  return ob->runtime.bb;
}

/* ************************************************** */

static int stroke_march_next_point(const bGPDstroke *gps,
                                   const int index_next_pt,
                                   const float *current,
                                   const float dist,
                                   float *result,
                                   float *pressure,
                                   float *strength,
                                   float *vert_color,
                                   float *ratio_result,
                                   int *index_from,
                                   int *index_to)
{
  float remaining_till_next = 0.0f;
  float remaining_march = dist;
  float step_start[3];
  float point[3];
  int next_point_index = index_next_pt;
  bGPDspoint *pt = NULL;

  if (!(next_point_index < gps->totpoints)) {
    return -1;
  }

  copy_v3_v3(step_start, current);
  pt = &gps->points[next_point_index];
  copy_v3_v3(point, &pt->x);
  remaining_till_next = len_v3v3(point, step_start);

  while (remaining_till_next < remaining_march) {
    remaining_march -= remaining_till_next;
    pt = &gps->points[next_point_index];
    copy_v3_v3(point, &pt->x);
    copy_v3_v3(step_start, point);
    next_point_index++;
    if (!(next_point_index < gps->totpoints)) {
      next_point_index = gps->totpoints - 1;
      break;
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

    *index_from = next_point_index - 1;
    *index_to = next_point_index;
    *ratio_result = 1.0f;

    return 0;
  }

  float ratio = remaining_march / remaining_till_next;
  interp_v3_v3v3(result, step_start, point, ratio);
  *pressure = interpf(
      gps->points[next_point_index].pressure, gps->points[next_point_index - 1].pressure, ratio);
  *strength = interpf(
      gps->points[next_point_index].strength, gps->points[next_point_index - 1].strength, ratio);
  interp_v4_v4v4(vert_color,
                 gps->points[next_point_index - 1].vert_color,
                 gps->points[next_point_index].vert_color,
                 ratio);

  *index_from = next_point_index - 1;
  *index_to = next_point_index;
  *ratio_result = ratio;

  return next_point_index;
}

static int stroke_march_next_point_no_interp(const bGPDstroke *gps,
                                             const int index_next_pt,
                                             const float *current,
                                             const float dist,
                                             float *result)
{
  float remaining_till_next = 0.0f;
  float remaining_march = dist;
  float step_start[3];
  float point[3];
  int next_point_index = index_next_pt;
  bGPDspoint *pt = NULL;

  if (!(next_point_index < gps->totpoints)) {
    return -1;
  }

  copy_v3_v3(step_start, current);
  pt = &gps->points[next_point_index];
  copy_v3_v3(point, &pt->x);
  remaining_till_next = len_v3v3(point, step_start);

  while (remaining_till_next < remaining_march) {
    remaining_march -= remaining_till_next;
    pt = &gps->points[next_point_index];
    copy_v3_v3(point, &pt->x);
    copy_v3_v3(step_start, point);
    next_point_index++;
    if (!(next_point_index < gps->totpoints)) {
      next_point_index = gps->totpoints - 1;
      break;
    }
    pt = &gps->points[next_point_index];
    copy_v3_v3(point, &pt->x);
    remaining_till_next = len_v3v3(point, step_start);
  }
  if (remaining_till_next < remaining_march) {
    pt = &gps->points[next_point_index];
    copy_v3_v3(result, &pt->x);
    return 0;
  }

  float ratio = remaining_march / remaining_till_next;
  interp_v3_v3v3(result, step_start, point, ratio);
  return next_point_index;
}

static int stroke_march_count(const bGPDstroke *gps, const float dist)
{
  int point_count = 0;
  float point[3];
  int next_point_index = 1;
  bGPDspoint *pt = NULL;

  pt = &gps->points[0];
  copy_v3_v3(point, &pt->x);
  point_count++;

  while ((next_point_index = stroke_march_next_point_no_interp(
              gps, next_point_index, point, dist, point)) > -1) {
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
      for (ld = result->first; ld; ld = ld->next) {
        if (ld->data == POINTER_FROM_INT(dw->def_nr)) {
          found = true;
          break;
        }
      }
      if (!found) {
        ld = MEM_callocN(sizeof(LinkData), "def_nr_item");
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
  MDeformVert *dst = MEM_mallocN(count * sizeof(MDeformVert), "new_deformVert");

  for (i = 0; i < count; i++) {
    dst[i].dw = MEM_mallocN(sizeof(MDeformWeight) * totweight, "new_deformWeight");
    dst[i].totweight = totweight;
    j = 0;
    /* re-assign deform groups */
    for (ld = def_nr_list->first; ld; ld = ld->next) {
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

/**
 * Resample a stroke
 * \param gpd: Grease pencil data-block
 * \param gps: Stroke to sample
 * \param dist: Distance of one segment
 */
bool BKE_gpencil_stroke_sample(bGPdata *gpd, bGPDstroke *gps, const float dist, const bool select)
{
  bGPDspoint *pt = gps->points;
  bGPDspoint *pt1 = NULL;
  bGPDspoint *pt2 = NULL;
  LinkData *ld;
  ListBase def_nr_list = {0};

  if (gps->totpoints < 2 || dist < FLT_EPSILON) {
    return false;
  }
  /* TODO: Implement feature point preservation. */
  int count = stroke_march_count(gps, dist);

  bGPDspoint *new_pt = MEM_callocN(sizeof(bGPDspoint) * count, "gp_stroke_points_sampled");
  MDeformVert *new_dv = NULL;

  int result_totweight;

  if (gps->dvert != NULL) {
    stroke_defvert_create_nr_list(gps->dvert, gps->totpoints, &def_nr_list, &result_totweight);
    new_dv = stroke_defvert_new_count(count, result_totweight, &def_nr_list);
  }

  int next_point_index = 1;
  int i = 0;
  float pressure, strength, ratio_result;
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
                                                     &ratio_result,
                                                     &index_from,
                                                     &index_to)) > -1) {
    pt2 = &new_pt[i];
    copy_v3_v3(&pt2->x, last_coord);
    new_pt[i].pressure = pressure;
    new_pt[i].strength = strength;
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
    while ((ld = BLI_pophead(&def_nr_list))) {
      MEM_freeN(ld);
    }

    gps->dvert = new_dv;
  }

  gps->totpoints = i;

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);

  return true;
}

/**
 * Backbone stretch similar to Freestyle.
 * \param gps: Stroke to sample
 * \param dist: Distance of one segment
 * \param tip_length: Ignore tip jittering, set zero to use default value.
 */
bool BKE_gpencil_stroke_stretch(bGPDstroke *gps, const float dist, const float tip_length)
{
  bGPDspoint *pt = gps->points, *last_pt, *second_last, *next_pt;
  float threshold = (tip_length == 0 ? 0.001f : tip_length);

  if (gps->totpoints < 2 || dist < FLT_EPSILON) {
    return false;
  }

  last_pt = &pt[gps->totpoints - 1];
  second_last = &pt[gps->totpoints - 2];
  next_pt = &pt[1];

  float len1 = 0.0f;
  float len2 = 0.0f;

  int i = 1;
  while (len1 < threshold && gps->totpoints > i) {
    next_pt = &pt[i];
    len1 = len_v3v3(&next_pt->x, &pt->x);
    i++;
  }

  i = 2;
  while (len2 < threshold && gps->totpoints >= i) {
    second_last = &pt[gps->totpoints - i];
    len2 = len_v3v3(&last_pt->x, &second_last->x);
    i++;
  }

  float extend1 = (len1 + dist) / len1;
  float extend2 = (len2 + dist) / len2;

  float result1[3], result2[3];

  interp_v3_v3v3(result1, &next_pt->x, &pt->x, extend1);
  interp_v3_v3v3(result2, &second_last->x, &last_pt->x, extend2);

  copy_v3_v3(&pt->x, result1);
  copy_v3_v3(&last_pt->x, result2);

  return true;
}

/**
 * Trim stroke to needed segments
 * \param gps: Target stroke
 * \param index_from: the index of the first point to be used in the trimmed result
 * \param index_to: the index of the last point to be used in the trimmed result
 */
bool BKE_gpencil_stroke_trim_points(bGPDstroke *gps, const int index_from, const int index_to)
{
  bGPDspoint *pt = gps->points, *new_pt;
  MDeformVert *dv, *new_dv;

  const int new_count = index_to - index_from + 1;

  if (new_count >= gps->totpoints) {
    return false;
  }

  if (new_count == 1) {
    BKE_gpencil_free_stroke_weights(gps);
    MEM_freeN(gps->points);
    gps->points = NULL;
    gps->dvert = NULL;
    gps->totpoints = 0;
    return false;
  }

  new_pt = MEM_callocN(sizeof(bGPDspoint) * new_count, "gp_stroke_points_trimmed");

  for (int i = 0; i < new_count; i++) {
    memcpy(&new_pt[i], &pt[i + index_from], sizeof(bGPDspoint));
  }

  if (gps->dvert) {
    new_dv = MEM_callocN(sizeof(MDeformVert) * new_count, "gp_stroke_dverts_trimmed");
    for (int i = 0; i < new_count; i++) {
      dv = &gps->dvert[i + index_from];
      new_dv[i].flag = dv->flag;
      new_dv[i].totweight = dv->totweight;
      new_dv[i].dw = MEM_callocN(sizeof(MDeformWeight) * dv->totweight,
                                 "gp_stroke_dverts_dw_trimmed");
      for (int j = 0; j < dv->totweight; j++) {
        new_dv[i].dw[j].weight = dv->dw[j].weight;
        new_dv[i].dw[j].def_nr = dv->dw[j].def_nr;
      }
    }
    MEM_freeN(gps->dvert);
    gps->dvert = new_dv;
  }

  MEM_freeN(gps->points);
  gps->points = new_pt;
  gps->totpoints = new_count;

  return true;
}

/**
 * Split stroke.
 * \param gpd: Grease pencil data-block
 * \param gpf: Grease pencil frame
 * \param gps: Grease pencil original stroke
 * \param before_index: Position of the point to split
 * \param remaining_gps: Secondary stroke after split.
 * \return True if the split was done
 */
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

  for (int i = 0; i < new_count; i++) {
    memcpy(&new_pt[i], &pt[i + before_index], sizeof(bGPDspoint));
  }

  if (gps->dvert) {
    new_dv = MEM_callocN(sizeof(MDeformVert) * new_count,
                         "gp_stroke_dverts_remaining(MDeformVert)");
    for (int i = 0; i < new_count; i++) {
      dv = &gps->dvert[i + before_index];
      new_dv[i].flag = dv->flag;
      new_dv[i].totweight = dv->totweight;
      new_dv[i].dw = MEM_callocN(sizeof(MDeformWeight) * dv->totweight,
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

  BKE_gpencil_stroke_trim_points(gps, 0, old_count);
  BKE_gpencil_stroke_geometry_update(gpd, gps);
  return true;
}

/**
 * Shrink the stroke by length.
 * \param gps: Stroke to shrink
 * \param dist: delta length
 */
bool BKE_gpencil_stroke_shrink(bGPDstroke *gps, const float dist)
{
  bGPDspoint *pt = gps->points, *second_last;
  int i;

  if (gps->totpoints < 2 || dist < FLT_EPSILON) {
    return false;
  }

  second_last = &pt[gps->totpoints - 2];

  float len1, this_len1, cut_len1;
  float len2, this_len2, cut_len2;
  int index_start, index_end;

  len1 = len2 = this_len1 = this_len2 = cut_len1 = cut_len2 = 0.0f;

  i = 1;
  while (len1 < dist && gps->totpoints > i - 1) {
    this_len1 = len_v3v3(&pt[i].x, &pt[i + 1].x);
    len1 += this_len1;
    cut_len1 = len1 - dist;
    i++;
  }
  index_start = i - 2;

  i = 2;
  while (len2 < dist && gps->totpoints >= i) {
    second_last = &pt[gps->totpoints - i];
    this_len2 = len_v3v3(&second_last[1].x, &second_last->x);
    len2 += this_len2;
    cut_len2 = len2 - dist;
    i++;
  }
  index_end = gps->totpoints - i + 2;

  if (len1 < dist || len2 < dist || index_end <= index_start) {
    index_start = index_end = 0; /* empty stroke */
  }

  if ((index_end == index_start + 1) && (cut_len1 + cut_len2 > 1.0f)) {
    index_start = index_end = 0; /* no length left to cut */
  }

  BKE_gpencil_stroke_trim_points(gps, index_start, index_end);

  if (gps->totpoints == 0) {
    return false;
  }

  pt = gps->points;

  float cut1 = cut_len1 / this_len1;
  float cut2 = cut_len2 / this_len2;

  float result1[3], result2[3];

  interp_v3_v3v3(result1, &pt[1].x, &pt[0].x, cut1);
  interp_v3_v3v3(result2, &pt[gps->totpoints - 2].x, &pt[gps->totpoints - 1].x, cut2);

  copy_v3_v3(&pt[0].x, result1);
  copy_v3_v3(&pt[gps->totpoints - 1].x, result2);

  return true;
}

/**
 * Apply smooth position to stroke point.
 * \param gps: Stroke to smooth
 * \param i: Point index
 * \param inf: Amount of smoothing to apply
 */
bool BKE_gpencil_stroke_smooth(bGPDstroke *gps, int i, float inf)
{
  bGPDspoint *pt = &gps->points[i];
  float sco[3] = {0.0f};

  /* Do nothing if not enough points to smooth out */
  if (gps->totpoints <= 2) {
    return false;
  }

  /* Only affect endpoints by a fraction of the normal strength,
   * to prevent the stroke from shrinking too much
   */
  if (ELEM(i, 0, gps->totpoints - 1)) {
    inf *= 0.1f;
  }

  /* Compute smoothed coordinate by taking the ones nearby */
  /* XXX: This is potentially slow,
   *      and suffers from accumulation error as earlier points are handled before later ones. */
  {
    /* XXX: this is hardcoded to look at 2 points on either side of the current one
     * (i.e. 5 items total). */
    const int steps = 2;
    const float average_fac = 1.0f / (float)(steps * 2 + 1);
    int step;

    /* add the point itself */
    madd_v3_v3fl(sco, &pt->x, average_fac);

    /* n-steps before/after current point */
    /* XXX: review how the endpoints are treated by this algorithm. */
    /* XXX: falloff measures should also introduce some weighting variations,
     *      so that further-out points get less weight. */
    for (step = 1; step <= steps; step++) {
      bGPDspoint *pt1, *pt2;
      int before = i - step;
      int after = i + step;

      CLAMP_MIN(before, 0);
      CLAMP_MAX(after, gps->totpoints - 1);

      pt1 = &gps->points[before];
      pt2 = &gps->points[after];

      /* add both these points to the average-sum (s += p[i]/n) */
      madd_v3_v3fl(sco, &pt1->x, average_fac);
      madd_v3_v3fl(sco, &pt2->x, average_fac);
    }
  }

  /* Based on influence factor, blend between original and optimal smoothed coordinate */
  interp_v3_v3v3(&pt->x, &pt->x, sco, inf);

  return true;
}

/**
 * Apply smooth strength to stroke point.
 * \param gps: Stroke to smooth
 * \param point_index: Point index
 * \param influence: Amount of smoothing to apply
 */
bool BKE_gpencil_stroke_smooth_strength(bGPDstroke *gps, int point_index, float influence)
{
  bGPDspoint *ptb = &gps->points[point_index];

  /* Do nothing if not enough points */
  if ((gps->totpoints <= 2) || (point_index < 1)) {
    return false;
  }
  /* Only affect endpoints by a fraction of the normal influence */
  float inf = influence;
  if (ELEM(point_index, 0, gps->totpoints - 1)) {
    inf *= 0.01f;
  }
  /* Limit max influence to reduce pop effect. */
  CLAMP_MAX(inf, 0.98f);

  float total = 0.0f;
  float max_strength = 0.0f;
  const int steps = 4;
  const float average_fac = 1.0f / (float)(steps * 2 + 1);
  int step;

  /* add the point itself */
  total += ptb->strength * average_fac;
  max_strength = ptb->strength;

  /* n-steps before/after current point */
  for (step = 1; step <= steps; step++) {
    bGPDspoint *pt1, *pt2;
    int before = point_index - step;
    int after = point_index + step;

    CLAMP_MIN(before, 0);
    CLAMP_MAX(after, gps->totpoints - 1);

    pt1 = &gps->points[before];
    pt2 = &gps->points[after];

    /* add both these points to the average-sum (s += p[i]/n) */
    total += pt1->strength * average_fac;
    total += pt2->strength * average_fac;
    /* Save max value. */
    if (max_strength < pt1->strength) {
      max_strength = pt1->strength;
    }
    if (max_strength < pt2->strength) {
      max_strength = pt2->strength;
    }
  }

  /* Based on influence factor, blend between original and optimal smoothed value. */
  ptb->strength = interpf(ptb->strength, total, inf);
  /* Clamp to maximum stroke strength to avoid weird results. */
  CLAMP_MAX(ptb->strength, max_strength);

  return true;
}

/**
 * Apply smooth for thickness to stroke point (use pressure).
 * \param gps: Stroke to smooth
 * \param point_index: Point index
 * \param influence: Amount of smoothing to apply
 */
bool BKE_gpencil_stroke_smooth_thickness(bGPDstroke *gps, int point_index, float influence)
{
  bGPDspoint *ptb = &gps->points[point_index];

  /* Do nothing if not enough points */
  if ((gps->totpoints <= 2) || (point_index < 1)) {
    return false;
  }
  /* Only affect endpoints by a fraction of the normal influence */
  float inf = influence;
  if (ELEM(point_index, 0, gps->totpoints - 1)) {
    inf *= 0.01f;
  }
  /* Limit max influence to reduce pop effect. */
  CLAMP_MAX(inf, 0.98f);

  float total = 0.0f;
  float max_pressure = 0.0f;
  const int steps = 4;
  const float average_fac = 1.0f / (float)(steps * 2 + 1);
  int step;

  /* add the point itself */
  total += ptb->pressure * average_fac;
  max_pressure = ptb->pressure;

  /* n-steps before/after current point */
  for (step = 1; step <= steps; step++) {
    bGPDspoint *pt1, *pt2;
    int before = point_index - step;
    int after = point_index + step;

    CLAMP_MIN(before, 0);
    CLAMP_MAX(after, gps->totpoints - 1);

    pt1 = &gps->points[before];
    pt2 = &gps->points[after];

    /* add both these points to the average-sum (s += p[i]/n) */
    total += pt1->pressure * average_fac;
    total += pt2->pressure * average_fac;
    /* Save max value. */
    if (max_pressure < pt1->pressure) {
      max_pressure = pt1->pressure;
    }
    if (max_pressure < pt2->pressure) {
      max_pressure = pt2->pressure;
    }
  }

  /* Based on influence factor, blend between original and optimal smoothed value. */
  ptb->pressure = interpf(ptb->pressure, total, inf);
  /* Clamp to maximum stroke thickness to avoid weird results. */
  CLAMP_MAX(ptb->pressure, max_pressure);
  return true;
}

/**
 * Apply smooth for UV rotation to stroke point (use pressure).
 * \param gps: Stroke to smooth
 * \param point_index: Point index
 * \param influence: Amount of smoothing to apply
 */
bool BKE_gpencil_stroke_smooth_uv(bGPDstroke *gps, int point_index, float influence)
{
  bGPDspoint *ptb = &gps->points[point_index];

  /* Do nothing if not enough points */
  if (gps->totpoints <= 2) {
    return false;
  }

  /* Compute theoretical optimal value */
  bGPDspoint *pta, *ptc;
  int before = point_index - 1;
  int after = point_index + 1;

  CLAMP_MIN(before, 0);
  CLAMP_MAX(after, gps->totpoints - 1);

  pta = &gps->points[before];
  ptc = &gps->points[after];

  /* the optimal value is the corresponding to the interpolation of the pressure
   * at the distance of point b
   */
  float fac = line_point_factor_v3(&ptb->x, &pta->x, &ptc->x);
  /* sometimes the factor can be wrong due stroke geometry, so use middle point */
  if ((fac < 0.0f) || (fac > 1.0f)) {
    fac = 0.5f;
  }
  float optimal = interpf(ptc->uv_rot, pta->uv_rot, fac);

  /* Based on influence factor, blend between original and optimal */
  ptb->uv_rot = interpf(optimal, ptb->uv_rot, influence);
  CLAMP(ptb->uv_rot, -M_PI_2, M_PI_2);

  return true;
}

/**
 * Get points of stroke always flat to view not affected
 * by camera view or view position.
 * \param points: Array of grease pencil points (3D)
 * \param totpoints: Total of points
 * \param points2d: Result array of 2D points
 * \param r_direction: Return Concave (-1), Convex (1), or Auto-detect (0)
 */
void BKE_gpencil_stroke_2d_flat(const bGPDspoint *points,
                                int totpoints,
                                float (*points2d)[2],
                                int *r_direction)
{
  BLI_assert(totpoints >= 2);

  const bGPDspoint *pt0 = &points[0];
  const bGPDspoint *pt1 = &points[1];
  const bGPDspoint *pt3 = &points[(int)(totpoints * 0.75)];

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

/**
 * Get points of stroke always flat to view not affected by camera view or view position
 * using another stroke as reference.
 * \param ref_points: Array of reference points (3D)
 * \param ref_totpoints: Total reference points
 * \param points: Array of points to flat (3D)
 * \param totpoints: Total points
 * \param points2d: Result array of 2D points
 * \param scale: Scale factor
 * \param r_direction: Return Concave (-1), Convex (1), or Auto-detect (0)
 */
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
  const bGPDspoint *pt3 = &ref_points[(int)(ref_totpoints * 0.75)];

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
  *r_direction = (int)locy[2];
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

/**
 * Triangulate stroke to generate data for filling areas.
 * \param gps: Grease pencil stroke
 */
void BKE_gpencil_stroke_fill_triangulate(bGPDstroke *gps)
{
  BLI_assert(gps->totpoints >= 3);

  /* allocate memory for temporary areas */
  gps->tot_triangles = gps->totpoints - 2;
  uint(*tmp_triangles)[3] = MEM_mallocN(sizeof(*tmp_triangles) * gps->tot_triangles,
                                        "GP Stroke temp triangulation");
  float(*points2d)[2] = MEM_mallocN(sizeof(*points2d) * gps->totpoints,
                                    "GP Stroke temp 2d points");
  float(*uv)[2] = MEM_mallocN(sizeof(*uv) * gps->totpoints, "GP Stroke temp 2d uv data");

  int direction = 0;

  /* convert to 2d and triangulate */
  BKE_gpencil_stroke_2d_flat(gps->points, gps->totpoints, points2d, &direction);
  BLI_polyfill_calc(points2d, (uint)gps->totpoints, direction, tmp_triangles);

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
    gps->triangles = MEM_callocN(sizeof(*gps->triangles) * gps->tot_triangles,
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

    gps->triangles = NULL;
  }

  /* clear memory */
  MEM_SAFE_FREE(tmp_triangles);
  MEM_SAFE_FREE(points2d);
  MEM_SAFE_FREE(uv);
}

/**
 * Update Stroke UV data.
 * \param gps: Grease pencil stroke
 */
void BKE_gpencil_stroke_uv_update(bGPDstroke *gps)
{
  if (gps == NULL || gps->totpoints == 0) {
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

/**
 * Recalc all internal geometry data for the stroke
 * \param gpd: Grease pencil data-block
 * \param gps: Grease pencil stroke
 */
void BKE_gpencil_stroke_geometry_update(bGPdata *gpd, bGPDstroke *gps)
{
  if (gps == NULL) {
    return;
  }

  if (gps->editcurve != NULL) {
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

/**
 * Calculate grease pencil stroke length.
 * \param gps: Grease pencil stroke
 * \param use_3d: Set to true to use 3D points
 * \return Length of the stroke
 */
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

/** Calculate grease pencil stroke length between points. */
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

/**
 * Trim stroke to the first intersection or loop.
 * \param gps: Stroke data
 */
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
    bGPDspoint *old_points = MEM_dupallocN(gps->points);
    MDeformVert *old_dvert = NULL;
    MDeformVert *dvert_src = NULL;

    if (gps->dvert != NULL) {
      old_dvert = MEM_dupallocN(gps->dvert);
    }

    /* resize gps */
    int newtot = end - start + 1;

    gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * newtot);
    if (gps->dvert != NULL) {
      gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * newtot);
    }

    for (int i = 0; i < newtot; i++) {
      int idx = start + i;
      bGPDspoint *pt_src = &old_points[idx];
      bGPDspoint *pt_new = &gps->points[i];
      memcpy(pt_new, pt_src, sizeof(bGPDspoint));
      if (gps->dvert != NULL) {
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

/**
 * Close grease pencil stroke.
 * \param gps: Stroke to close
 */
bool BKE_gpencil_stroke_close(bGPDstroke *gps)
{
  bGPDspoint *pt1 = NULL;
  bGPDspoint *pt2 = NULL;

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
  gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints);
  if (gps->dvert != NULL) {
    gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints);
  }

  /* Generate new points */
  pt1 = &gps->points[old_tot - 1];
  pt2 = &gps->points[0];
  bGPDspoint *pt = &gps->points[old_tot];
  for (int i = 1; i < tot_newpoints + 1; i++, pt++) {
    float step = (tot_newpoints > 1) ? ((float)i / (float)tot_newpoints) : 0.99f;
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

    /* Set weights. */
    if (gps->dvert != NULL) {
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

/**
 * Dissolve points in stroke.
 * \param gpd: Grease pencil data-block
 * \param gpf: Grease pencil frame
 * \param gps: Grease pencil stroke
 * \param tag: Type of tag for point
 */
void BKE_gpencil_dissolve_points(bGPdata *gpd, bGPDframe *gpf, bGPDstroke *gps, const short tag)
{
  bGPDspoint *pt;
  MDeformVert *dvert = NULL;
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
    bGPDspoint *new_points = MEM_callocN(sizeof(bGPDspoint) * tot, "new gp stroke points copy");
    bGPDspoint *npt = new_points;

    MDeformVert *new_dvert = NULL;
    MDeformVert *ndvert = NULL;

    if (gps->dvert != NULL) {
      new_dvert = MEM_callocN(sizeof(MDeformVert) * tot, "new gp stroke weights copy");
      ndvert = new_dvert;
    }

    (gps->dvert != NULL) ? dvert = gps->dvert : NULL;
    for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
      if ((pt->flag & tag) == 0) {
        *npt = *pt;
        npt++;

        if (gps->dvert != NULL) {
          *ndvert = *dvert;
          ndvert->dw = MEM_dupallocN(dvert->dw);
          ndvert++;
        }
      }
      if (gps->dvert != NULL) {
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

/**
 * Calculate stroke normals.
 * \param gps: Grease pencil stroke
 * \param r_normal: Return Normal vector normalized
 */
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
  const bGPDspoint *pt3 = &points[(int)(totpoints * 0.75)];

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

/* Stroke Simplify ------------------------------------- */

/**
 * Reduce a series of points to a simplified version, but
 * maintains the general shape of the series
 *
 * Ramer - Douglas - Peucker algorithm
 * by http://en.wikipedia.org/wiki/Ramer-Douglas-Peucker_algorithm
 * \param gpd: Grease pencil data-block
 * \param gps: Grease pencil stroke
 * \param epsilon: Epsilon value to define precision of the algorithm
 */
void BKE_gpencil_stroke_simplify_adaptive(bGPdata *gpd, bGPDstroke *gps, float epsilon)
{
  bGPDspoint *old_points = MEM_dupallocN(gps->points);
  int totpoints = gps->totpoints;
  char *marked = NULL;
  char work;

  int start = 0;
  int end = gps->totpoints - 1;

  marked = MEM_callocN(totpoints, "GP marked array");
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
  MDeformVert *old_dvert = NULL;
  MDeformVert *dvert_src = NULL;

  if (gps->dvert != NULL) {
    old_dvert = MEM_dupallocN(gps->dvert);
  }
  /* resize gps */
  int j = 0;
  for (int i = 0; i < totpoints; i++) {
    bGPDspoint *pt_src = &old_points[i];
    bGPDspoint *pt = &gps->points[j];

    if ((marked[i]) || (i == 0) || (i == totpoints - 1)) {
      memcpy(pt, pt_src, sizeof(bGPDspoint));
      if (gps->dvert != NULL) {
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
      if (gps->dvert != NULL) {
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

/**
 * Simplify alternate vertex of stroke except extremes.
 * \param gpd: Grease pencil data-block
 * \param gps: Grease pencil stroke
 */
void BKE_gpencil_stroke_simplify_fixed(bGPdata *gpd, bGPDstroke *gps)
{
  if (gps->totpoints < 5) {
    return;
  }

  /* save points */
  bGPDspoint *old_points = MEM_dupallocN(gps->points);
  MDeformVert *old_dvert = NULL;
  MDeformVert *dvert_src = NULL;

  if (gps->dvert != NULL) {
    old_dvert = MEM_dupallocN(gps->dvert);
  }

  /* resize gps */
  int newtot = (gps->totpoints - 2) / 2;
  if (((gps->totpoints - 2) % 2) > 0) {
    newtot++;
  }
  newtot += 2;

  gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * newtot);
  if (gps->dvert != NULL) {
    gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * newtot);
  }

  int j = 0;
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt_src = &old_points[i];
    bGPDspoint *pt = &gps->points[j];

    if ((i == 0) || (i == gps->totpoints - 1) || ((i % 2) > 0.0)) {
      memcpy(pt, pt_src, sizeof(bGPDspoint));
      if (gps->dvert != NULL) {
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
      if (gps->dvert != NULL) {
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

/**
 * Subdivide a stroke
 * \param gpd: Grease pencil data-block
 * \param gps: Stroke
 * \param level: Level of subdivision
 * \param type: Type of subdivision
 */
void BKE_gpencil_stroke_subdivide(bGPdata *gpd, bGPDstroke *gps, int level, int type)
{
  bGPDspoint *temp_points;
  MDeformVert *temp_dverts = NULL;
  MDeformVert *dvert = NULL;
  MDeformVert *dvert_final = NULL;
  MDeformVert *dvert_next = NULL;
  int totnewpoints, oldtotpoints;
  int i2;

  for (int s = 0; s < level; s++) {
    totnewpoints = gps->totpoints - 1;
    /* duplicate points in a temp area */
    temp_points = MEM_dupallocN(gps->points);
    oldtotpoints = gps->totpoints;

    /* resize the points arrays */
    gps->totpoints += totnewpoints;
    gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints);
    if (gps->dvert != NULL) {
      temp_dverts = MEM_dupallocN(gps->dvert);
      gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints);
    }

    /* move points from last to first to new place */
    i2 = gps->totpoints - 1;
    for (int i = oldtotpoints - 1; i > 0; i--) {
      bGPDspoint *pt = &temp_points[i];
      bGPDspoint *pt_final = &gps->points[i2];

      copy_v3_v3(&pt_final->x, &pt->x);
      pt_final->pressure = pt->pressure;
      pt_final->strength = pt->strength;
      pt_final->time = pt->time;
      pt_final->flag = pt->flag;
      pt_final->runtime.pt_orig = pt->runtime.pt_orig;
      pt_final->runtime.idx_orig = pt->runtime.idx_orig;
      copy_v4_v4(pt_final->vert_color, pt->vert_color);

      if (gps->dvert != NULL) {
        dvert = &temp_dverts[i];
        dvert_final = &gps->dvert[i2];
        dvert_final->totweight = dvert->totweight;
        dvert_final->dw = dvert->dw;
      }
      i2 -= 2;
    }
    /* interpolate mid points */
    i2 = 1;
    for (int i = 0; i < oldtotpoints - 1; i++) {
      bGPDspoint *pt = &temp_points[i];
      bGPDspoint *next = &temp_points[i + 1];
      bGPDspoint *pt_final = &gps->points[i2];

      /* add a half way point */
      interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
      pt_final->pressure = interpf(pt->pressure, next->pressure, 0.5f);
      pt_final->strength = interpf(pt->strength, next->strength, 0.5f);
      CLAMP(pt_final->strength, GPENCIL_STRENGTH_MIN, 1.0f);
      pt_final->time = interpf(pt->time, next->time, 0.5f);
      pt_final->runtime.pt_orig = NULL;
      pt_final->flag = 0;
      interp_v4_v4v4(pt_final->vert_color, pt->vert_color, next->vert_color, 0.5f);

      if (gps->dvert != NULL) {
        dvert = &temp_dverts[i];
        dvert_next = &temp_dverts[i + 1];
        dvert_final = &gps->dvert[i2];

        dvert_final->totweight = dvert->totweight;
        dvert_final->dw = MEM_dupallocN(dvert->dw);

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

      i2 += 2;
    }

    MEM_SAFE_FREE(temp_points);
    MEM_SAFE_FREE(temp_dverts);

    /* move points to smooth stroke (not simple type )*/
    if (type != GP_SUBDIV_SIMPLE) {
      /* duplicate points in a temp area with the new subdivide data */
      temp_points = MEM_dupallocN(gps->points);

      /* extreme points are not changed */
      for (int i = 0; i < gps->totpoints - 2; i++) {
        bGPDspoint *pt = &temp_points[i];
        bGPDspoint *next = &temp_points[i + 1];
        bGPDspoint *pt_final = &gps->points[i + 1];

        /* move point */
        interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
      }
      /* free temp memory */
      MEM_SAFE_FREE(temp_points);
    }
  }

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

/* Merge by distance ------------------------------------- */

/**
 * Reduce a series of points when the distance is below a threshold.
 * Special case for first and last points (both are keeped) for other points,
 * the merge point always is at first point.
 * \param gpd: Grease pencil data-block
 * \param gpf: Grease Pencil frame
 * \param gps: Grease Pencil stroke
 * \param threshold: Distance between points
 * \param use_unselected: Set to true to analyze all stroke and not only selected points
 */
void BKE_gpencil_stroke_merge_distance(bGPdata *gpd,
                                       bGPDframe *gpf,
                                       bGPDstroke *gps,
                                       const float threshold,
                                       const bool use_unselected)
{
  bGPDspoint *pt = NULL;
  bGPDspoint *pt_next = NULL;
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
      /* Jump to next pair of points, keeping first point segment equals.*/
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

typedef struct GpEdge {
  uint v1, v2;
  /* Coordinates. */
  float v1_co[3], v2_co[3];
  /* Normals. */
  float n1[3], n2[3];
  /* Direction of the segment. */
  float vec[3];
  int flag;
} GpEdge;

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

      /* Avoid to follow already visited vertice. */
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
                                       const bool use_seams)
{
  Mesh *me = (Mesh *)ob->data;
  if (me->totedge == 0) {
    return;
  }

  /* Arrays for all edge vertices (forward and backward) that form a edge loop.
   * This is reused for each edgeloop to create gpencil stroke. */
  uint *stroke = MEM_callocN(sizeof(uint) * me->totedge * 2, __func__);
  uint *stroke_fw = MEM_callocN(sizeof(uint) * me->totedge, __func__);
  uint *stroke_bw = MEM_callocN(sizeof(uint) * me->totedge, __func__);

  /* Create array with all edges. */
  GpEdge *gp_edges = MEM_callocN(sizeof(GpEdge) * me->totedge, __func__);
  GpEdge *gped = NULL;
  for (int i = 0; i < me->totedge; i++) {
    MEdge *ed = &me->medge[i];
    gped = &gp_edges[i];
    MVert *mv1 = &me->mvert[ed->v1];
    normal_short_to_float_v3(gped->n1, mv1->no);

    gped->v1 = ed->v1;
    copy_v3_v3(gped->v1_co, mv1->co);

    MVert *mv2 = &me->mvert[ed->v2];
    normal_short_to_float_v3(gped->n2, mv2->no);
    gped->v2 = ed->v2;
    copy_v3_v3(gped->v2_co, mv2->co);

    sub_v3_v3v3(gped->vec, mv1->co, mv2->co);

    /* If use seams, mark as done if not a seam. */
    if ((use_seams) && ((ed->flag & ME_SEAM) == 0)) {
      gped->flag = 1;
    }
  }

  /* Loop edges to find edgeloops */
  bool pending = true;
  int e = 0;
  while (pending) {
    /* Clear arrays of stroke. */
    memset(stroke_fw, 0, sizeof(uint) * me->totedge);
    memset(stroke_bw, 0, sizeof(uint) * me->totedge);
    memset(stroke, 0, sizeof(uint) * me->totedge * 2);

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

    /* Hash used to avoid loop over same vertice. */
    GHash *v_table = BLI_ghash_int_new(__func__);
    /* Look forward edges. */
    int totedges = gpencil_walk_edge(v_table, gp_edges, me->totedge, stroke_fw, e, angle, false);
    /* Look backward edges. */
    int totbw = gpencil_walk_edge(v_table, gp_edges, me->totedge, stroke_bw, e, angle, true);

    BLI_ghash_free(v_table, NULL, NULL);

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

    /* Create first segment. */
    float fpt[3];
    uint v = stroke[0];
    gped = &gp_edges[v];
    bGPDspoint *pt = &gps_stroke->points[0];
    mul_v3_v3fl(fpt, gped->n1, offset);
    add_v3_v3v3(&pt->x, gped->v1_co, fpt);
    mul_m4_v3(matrix, &pt->x);

    pt->pressure = 1.0f;
    pt->strength = 1.0f;

    pt = &gps_stroke->points[1];
    mul_v3_v3fl(fpt, gped->n2, offset);
    add_v3_v3v3(&pt->x, gped->v2_co, fpt);
    mul_m4_v3(matrix, &pt->x);

    pt->pressure = 1.0f;
    pt->strength = 1.0f;

    /* Add next segments. */
    for (int i = 1; i < array_len; i++) {
      v = stroke[i];
      gped = &gp_edges[v];

      pt = &gps_stroke->points[i + 1];
      mul_v3_v3fl(fpt, gped->n2, offset);
      add_v3_v3v3(&pt->x, gped->v2_co, fpt);
      mul_m4_v3(matrix, &pt->x);

      pt->pressure = 1.0f;
      pt->strength = 1.0f;
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
    if ((ma != NULL) && (ma->gp_style != NULL) && (STREQ(ma->id.name + 2, name))) {
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

/**
 * Convert a mesh object to grease pencil stroke.
 *
 * \param bmain: Main thread pointer.
 * \param depsgraph: Original depsgraph.
 * \param scene: Original scene.
 * \param ob_gp: Grease pencil object to add strokes.
 * \param ob_mesh: Mesh to convert.
 * \param angle: Limit angle to consider a edgeloop ends.
 * \param thickness: Thickness of the strokes.
 * \param offset: Offset along the normals.
 * \param matrix: Transformation matrix.
 * \param frame_offset: Destination frame number offset.
 * \param use_seams: Only export seam edges.
 * \param use_faces: Export faces as filled strokes.
 */
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
                              const bool use_faces)
{
  if (ELEM(NULL, ob_gp, ob_mesh) || (ob_gp->type != OB_GPENCIL) || (ob_gp->data == NULL)) {
    return false;
  }

  bGPdata *gpd = (bGPdata *)ob_gp->data;

  /* Use evaluated data to get mesh with all modifiers on top. */
  Object *ob_eval = (Object *)DEG_get_evaluated_object(depsgraph, ob_mesh);
  Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);
  MPoly *mp, *mpoly = me_eval->mpoly;
  MLoop *mloop = me_eval->mloop;
  int mpoly_len = me_eval->totpoly;
  char element_name[200];

  /* Need at least an edge. */
  if (me_eval->totvert < 2) {
    return false;
  }

  const float default_colors[2][4] = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.7f, 0.7f, 0.7f, 1.0f}};
  /* Create stroke material. */
  make_element_name(ob_mesh->id.name + 2, "Stroke", 64, element_name);
  int stroke_mat_index = gpencil_material_find_index_by_name(ob_gp, element_name);

  if (stroke_mat_index == -1) {
    gpencil_add_material(
        bmain, ob_gp, element_name, default_colors[0], true, false, &stroke_mat_index);
  }

  /* Export faces as filled strokes. */
  if (use_faces) {

    /* Read all polygons and create fill for each. */
    if (mpoly_len > 0) {
      make_element_name(ob_mesh->id.name + 2, "Fills", 128, element_name);
      /* Create Layer and Frame. */
      bGPDlayer *gpl_fill = BKE_gpencil_layer_named_get(gpd, element_name);
      if (gpl_fill == NULL) {
        gpl_fill = BKE_gpencil_layer_addnew(gpd, element_name, true, false);
      }
      bGPDframe *gpf_fill = BKE_gpencil_layer_frame_get(
          gpl_fill, CFRA + frame_offset, GP_GETFRAME_ADD_NEW);
      int i;
      for (i = 0, mp = mpoly; i < mpoly_len; i++, mp++) {
        MLoop *ml = &mloop[mp->loopstart];
        /* Find material. */
        int mat_idx = 0;
        Material *ma = BKE_object_material_get(ob_mesh, mp->mat_nr + 1);
        make_element_name(
            ob_mesh->id.name + 2, (ma != NULL) ? ma->id.name + 2 : "Fill", 64, element_name);
        mat_idx = BKE_gpencil_material_find_index_by_name_prefix(ob_gp, element_name);
        if (mat_idx == -1) {
          float color[4];
          if (ma != NULL) {
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

        /* Add points to strokes. */
        for (int j = 0; j < mp->totloop; j++, ml++) {
          MVert *mv = &me_eval->mvert[ml->v];
          bGPDspoint *pt = &gps_fill->points[j];
          copy_v3_v3(&pt->x, mv->co);
          mul_m4_v3(matrix, &pt->x);
          pt->pressure = 1.0f;
          pt->strength = 1.0f;
        }
        /* If has only 3 points subdivide. */
        if (mp->totloop == 3) {
          BKE_gpencil_stroke_subdivide(gpd, gps_fill, 1, GP_SUBDIV_SIMPLE);
        }

        BKE_gpencil_stroke_geometry_update(gpd, gps_fill);
      }
    }
  }

  /* Create stroke from edges. */
  make_element_name(ob_mesh->id.name + 2, "Lines", 128, element_name);

  /* Create Layer and Frame. */
  bGPDlayer *gpl_stroke = BKE_gpencil_layer_named_get(gpd, element_name);
  if (gpl_stroke == NULL) {
    gpl_stroke = BKE_gpencil_layer_addnew(gpd, element_name, true, false);
  }
  bGPDframe *gpf_stroke = BKE_gpencil_layer_frame_get(
      gpl_stroke, CFRA + frame_offset, GP_GETFRAME_ADD_NEW);

  gpencil_generate_edgeloops(
      ob_eval, gpd, gpf_stroke, stroke_mat_index, angle, thickness, offset, matrix, use_seams);

  /* Tag for recalculation */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);

  return true;
}

/**
 * Apply grease pencil Transforms.
 * \param gpd: Grease pencil data-block
 * \param mat: Transformation matrix
 */
void BKE_gpencil_transform(bGPdata *gpd, const float mat[4][4])
{
  if (gpd == NULL) {
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

/* Used for "move only origins" in object_data_transform.c */
int BKE_gpencil_stroke_point_count(bGPdata *gpd)
{
  int total_points = 0;

  if (gpd == NULL) {
    return 0;
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
        total_points += gps->totpoints;
      }
    }
  }
  return total_points;
}

/* Used for "move only origins" in object_data_transform.c */
void BKE_gpencil_point_coords_get(bGPdata *gpd, GPencilPointCoordinates *elem_data)
{
  if (gpd == NULL) {
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

/* Used for "move only origins" in object_data_transform.c */
void BKE_gpencil_point_coords_apply(bGPdata *gpd, const GPencilPointCoordinates *elem_data)
{
  if (gpd == NULL) {
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

/* Used for "move only origins" in object_data_transform.c */
void BKE_gpencil_point_coords_apply_with_mat4(bGPdata *gpd,
                                              const GPencilPointCoordinates *elem_data,
                                              const float mat[4][4])
{
  if (gpd == NULL) {
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

/**
 * Set a random color to stroke using vertex color.
 * \param gps: Stroke
 */
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

/* Flip stroke. */
void BKE_gpencil_stroke_flip(bGPDstroke *gps)
{
  int end = gps->totpoints - 1;

  for (int i = 0; i < gps->totpoints / 2; i++) {
    bGPDspoint *point, *point2;
    bGPDspoint pt;

    /* save first point */
    point = &gps->points[i];
    pt.x = point->x;
    pt.y = point->y;
    pt.z = point->z;
    pt.flag = point->flag;
    pt.pressure = point->pressure;
    pt.strength = point->strength;
    pt.time = point->time;
    copy_v4_v4(pt.vert_color, point->vert_color);

    /* replace first point with last point */
    point2 = &gps->points[end];
    point->x = point2->x;
    point->y = point2->y;
    point->z = point2->z;
    point->flag = point2->flag;
    point->pressure = point2->pressure;
    point->strength = point2->strength;
    point->time = point2->time;
    copy_v4_v4(point->vert_color, point2->vert_color);

    /* replace last point with first saved before */
    point = &gps->points[end];
    point->x = pt.x;
    point->y = pt.y;
    point->z = pt.z;
    point->flag = pt.flag;
    point->pressure = pt.pressure;
    point->strength = pt.strength;
    point->time = pt.time;
    copy_v4_v4(point->vert_color, pt.vert_color);

    end--;
  }
}

/* Temp data for storing information about an "island" of points
 * that should be kept when splitting up a stroke. Used in:
 * gpencil_stroke_delete_tagged_points()
 */
typedef struct tGPDeleteIsland {
  int start_idx;
  int end_idx;
} tGPDeleteIsland;

static void gpencil_stroke_join_islands(bGPdata *gpd,
                                        bGPDframe *gpf,
                                        bGPDstroke *gps_first,
                                        bGPDstroke *gps_last)
{
  bGPDspoint *pt = NULL;
  bGPDspoint *pt_final = NULL;
  const int totpoints = gps_first->totpoints + gps_last->totpoints;

  /* create new stroke */
  bGPDstroke *join_stroke = BKE_gpencil_stroke_duplicate(gps_first, false, true);

  join_stroke->points = MEM_callocN(sizeof(bGPDspoint) * totpoints, __func__);
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
  if ((gps_first->dvert != NULL) || (gps_last->dvert != NULL)) {
    join_stroke->dvert = MEM_callocN(sizeof(MDeformVert) * totpoints, __func__);
    MDeformVert *dvert_src = NULL;
    MDeformVert *dvert_dst = NULL;

    /* Copy weights (last before)*/
    e1 = 0;
    e2 = 0;
    for (int i = 0; i < totpoints; i++) {
      dvert_dst = &join_stroke->dvert[i];
      dvert_src = NULL;
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
        dvert_dst->dw = MEM_dupallocN(dvert_src->dw);
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

/* Split the given stroke into several new strokes, partitioning
 * it based on whether the stroke points have a particular flag
 * is set (e.g. "GP_SPOINT_SELECT" in most cases, but not always)
 *
 * The algorithm used here is as follows:
 * 1) We firstly identify the number of "islands" of non-tagged points
 *    which will all end up being in new strokes.
 *    - In the most extreme case (i.e. every other vert is a 1-vert island),
 *      we have at most n / 2 islands
 *    - Once we start having larger islands than that, the number required
 *      becomes much less
 * 2) Each island gets converted to a new stroke
 * If the number of points is <= limit, the stroke is deleted
 */
bGPDstroke *BKE_gpencil_stroke_delete_tagged_points(bGPdata *gpd,
                                                    bGPDframe *gpf,
                                                    bGPDstroke *gps,
                                                    bGPDstroke *next_stroke,
                                                    int tag_flags,
                                                    bool select,
                                                    int limit)
{
  tGPDeleteIsland *islands = MEM_callocN(sizeof(tGPDeleteIsland) * (gps->totpoints + 1) / 2,
                                         "gp_point_islands");
  bool in_island = false;
  int num_islands = 0;

  bGPDstroke *new_stroke = NULL;
  bGPDstroke *gps_first = NULL;
  const bool is_cyclic = (bool)(gps->flag & GP_STROKE_CYCLIC);

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

      /* if cyclic and first stroke, save to join later */
      if ((is_cyclic) && (gps_first == NULL)) {
        gps_first = new_stroke;
      }

      new_stroke->flag &= ~GP_STROKE_CYCLIC;

      /* Compute new buffer size (+ 1 needed as the endpoint index is "inclusive") */
      new_stroke->totpoints = island->end_idx - island->start_idx + 1;

      /* Copy over the relevant point data */
      new_stroke->points = MEM_callocN(sizeof(bGPDspoint) * new_stroke->totpoints,
                                       "gp delete stroke fragment");
      memcpy(new_stroke->points,
             gps->points + island->start_idx,
             sizeof(bGPDspoint) * new_stroke->totpoints);

      /* Copy over vertex weight data (if available) */
      if (gps->dvert != NULL) {
        /* Copy over the relevant vertex-weight points */
        new_stroke->dvert = MEM_callocN(sizeof(MDeformVert) * new_stroke->totpoints,
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
            dvert_dst->dw = MEM_dupallocN(dvert_src->dw);
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

        new_stroke->inittime += (double)delta;

        pts = new_stroke->points;
        for (j = 0; j < new_stroke->totpoints; j++, pts++) {
          pts->time -= delta;
          /* set flag for select again later */
          if (select == true) {
            pts->flag &= ~GP_SPOINT_SELECT;
            pts->flag |= GP_SPOINT_TAG;
          }
        }
      }

      /* Add new stroke to the frame or delete if below limit */
      if ((limit > 0) && (new_stroke->totpoints <= limit)) {
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
    if ((is_cyclic) && (gps_first != NULL) && (gps_first != new_stroke)) {
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
  if (gpc == NULL) {
    return;
  }
  const bool is_cyclic = gps->flag & GP_STROKE_CYCLIC;
  const int idx_last = gpc->tot_curve_points - 1;
  bGPDstroke *gps_first = NULL;
  bGPDstroke *gps_last = NULL;

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
      new_stroke->points = NULL;
      new_stroke->flag &= ~GP_STROKE_CYCLIC;
      new_stroke->editcurve = BKE_gpencil_stroke_editcurve_new(island_length);

      if (gps_first == NULL) {
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
  if (is_cyclic && gps_first != NULL && gps_last != NULL && gps_first != gps_last) {
    bGPDcurve *gpc_first = gps_first->editcurve;
    bGPDcurve *gpc_last = gps_last->editcurve;
    int first_tot_points = gpc_first->tot_curve_points;
    int old_tot_points = gpc_last->tot_curve_points;

    gpc_last->tot_curve_points = first_tot_points + old_tot_points;
    gpc_last->curve_points = MEM_recallocN(gpc_last->curve_points,
                                           sizeof(bGPDcurve_point) * gpc_last->tot_curve_points);
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

  gps->points = MEM_reallocN(gps->points, sizeof(bGPDspoint) * (gps->totpoints + 1));
  if (gps->dvert != NULL) {
    gps->dvert = MEM_reallocN(gps->dvert, sizeof(MDeformVert) * (gps->totpoints + 1));
  }
  else {
    /* If destination has weight add weight to origin. */
    if (dvert != NULL) {
      gps->dvert = MEM_callocN(sizeof(MDeformVert) * (gps->totpoints + 1), __func__);
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

  if (gps->dvert != NULL) {
    MDeformVert *newdvert = &gps->dvert[gps->totpoints - 1];

    if (dvert != NULL) {
      newdvert->totweight = dvert->totweight;
      newdvert->dw = MEM_dupallocN(dvert->dw);
    }
    else {
      newdvert->totweight = 0;
      newdvert->dw = NULL;
    }
  }
}

/* Join two strokes using the shortest distance (reorder stroke if necessary ) */
void BKE_gpencil_stroke_join(bGPDstroke *gps_a,
                             bGPDstroke *gps_b,
                             const bool leave_gaps,
                             const bool fit_thickness)
{
  bGPDspoint point;
  bGPDspoint *pt;
  int i;
  const float delta[3] = {1.0f, 1.0f, 1.0f};
  float deltatime = 0.0f;

  /* sanity checks */
  if (ELEM(NULL, gps_a, gps_b)) {
    return;
  }

  if ((gps_a->totpoints == 0) || (gps_b->totpoints == 0)) {
    return;
  }

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

  /* don't visibly link the first and last points? */
  if (leave_gaps) {
    /* 1st: add one tail point to start invisible area */
    point = gps_a->points[gps_a->totpoints - 1];
    deltatime = point.time;

    gpencil_stroke_copy_point(gps_a, NULL, &point, delta, 0.0f, 0.0f, 0.0f);

    /* 2nd: add one head point to finish invisible area */
    point = gps_b->points[0];
    gpencil_stroke_copy_point(gps_a, NULL, &point, delta, 0.0f, 0.0f, deltatime);
  }

  const float ratio = (fit_thickness && gps_a->thickness > 0.0f) ?
                          (float)gps_b->thickness / (float)gps_a->thickness :
                          1.0f;

  /* 3rd: add all points */
  for (i = 0, pt = gps_b->points; i < gps_b->totpoints && pt; i++, pt++) {
    MDeformVert *dvert = (gps_b->dvert) ? &gps_b->dvert[i] : NULL;
    gpencil_stroke_copy_point(
        gps_a, dvert, pt, delta, pt->pressure * ratio, pt->strength, deltatime);
  }
}

/* Copy the stroke of the frame to all frames selected (except current). */
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
      if (gpf_new == NULL) {
        gpf_new = BKE_gpencil_frame_addnew(gpl, cfra);
      }

      if (gpf_new == NULL) {
        continue;
      }

      bGPDstroke *gps_new = BKE_gpencil_stroke_duplicate(gps, true, true);
      if (gps_new == NULL) {
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
  BLI_ghash_free(frame_list, NULL, NULL);
}

/* Stroke Uniform Subdivide  ------------------------------------- */

typedef struct tSamplePoint {
  struct tSamplePoint *next, *prev;
  float x, y, z;
  float pressure, strength, time;
  float vertex_color[4];
  struct MDeformWeight *dw;
  int totweight;
} tSamplePoint;

typedef struct tSampleEdge {
  float length_sq;
  tSamplePoint *from;
  tSamplePoint *to;
} tSampleEdge;

/* Helper: creates a tSamplePoint from a bGPDspoint and (optionally) a MDeformVert. */
static tSamplePoint *new_sample_point_from_gp_point(const bGPDspoint *pt, const MDeformVert *dvert)
{
  tSamplePoint *new_pt = MEM_callocN(sizeof(tSamplePoint), __func__);
  copy_v3_v3(&new_pt->x, &pt->x);
  new_pt->pressure = pt->pressure;
  new_pt->strength = pt->strength;
  new_pt->time = pt->time;
  copy_v4_v4((float *)&new_pt->vertex_color, (float *)&pt->vert_color);
  if (dvert != NULL) {
    new_pt->totweight = dvert->totweight;
    new_pt->dw = MEM_callocN(sizeof(MDeformWeight) * new_pt->totweight, __func__);
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
  tSampleEdge *new_edge = MEM_callocN(sizeof(tSampleEdge), __func__);
  new_edge->from = from;
  new_edge->to = to;
  new_edge->length_sq = len_squared_v3v3(&from->x, &to->x);
  return new_edge;
}

/**
 * Subdivide the grease pencil stroke so the number of points is target_number.
 * Does not change the shape of the stroke. The new points will be distributed as
 * uniformly as possible by repeatedly subdividing the current longest edge.
 *
 * \param gps: The stroke to be up-sampled.
 * \param target_number: The number of points the up-sampled stroke should have.
 * \param select: Select/Deselect the stroke.
 */
void BKE_gpencil_stroke_uniform_subdivide(bGPdata *gpd,
                                          bGPDstroke *gps,
                                          const uint32_t target_number,
                                          const bool select)
{
  /* Stroke needs at least two points and strictly less points than the target number. */
  if (gps == NULL || gps->totpoints < 2 || gps->totpoints >= target_number) {
    return;
  }

  const int totpoints = gps->totpoints;
  const bool has_dverts = (gps->dvert != NULL);
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC);

  ListBase points = {NULL, NULL};
  Heap *edges = BLI_heap_new();

  /* Add all points into list. */
  for (uint32_t i = 0; i < totpoints; ++i) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = has_dverts ? &gps->dvert[i] : NULL;
    tSamplePoint *sp = new_sample_point_from_gp_point(pt, dvert);
    BLI_addtail(&points, sp);
  }

  /* Iterate over edges and insert them into the heap. */
  for (tSamplePoint *pt = ((tSamplePoint *)points.first)->next; pt != NULL; pt = pt->next) {
    tSampleEdge *se = new_sample_edge_from_sample_points(pt->prev, pt);
    /* BLI_heap is a min-heap, but we need the largest key to be at the top, so we take the
     * negative of the squared length. */
    BLI_heap_insert(edges, -(se->length_sq), se);
  }

  if (is_cyclic) {
    tSamplePoint *sp_first = points.first;
    tSamplePoint *sp_last = points.last;
    tSampleEdge *se = new_sample_edge_from_sample_points(sp_last, sp_first);
    BLI_heap_insert(edges, -(se->length_sq), se);
  }

  int num_points_needed = target_number - totpoints;
  BLI_assert(num_points_needed > 0);

  while (num_points_needed > 0) {
    tSampleEdge *se = BLI_heap_pop_min(edges);
    tSamplePoint *sp = se->from;
    tSamplePoint *sp_next = se->to;

    /* Subdivide the edge. */
    tSamplePoint *new_sp = MEM_callocN(sizeof(tSamplePoint), __func__);
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
      new_sp->dw = MEM_callocN(sizeof(MDeformWeight) * new_sp->totweight, __func__);
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
  gps->points = MEM_recallocN(gps->points, sizeof(bGPDspoint) * gps->totpoints);
  if (has_dverts) {
    gps->dvert = MEM_recallocN(gps->dvert, sizeof(MDeformVert) * gps->totpoints);
  }

  /* Convert list back to stroke point array. */
  tSamplePoint *sp = points.first;
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
      dvert->dw = MEM_callocN(sizeof(MDeformWeight) * dvert->totweight, __func__);
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
    if (temp->dw != NULL) {
      MEM_freeN(temp->dw);
    }
    MEM_SAFE_FREE(temp);
  }

  /* Update the geometry of the stroke. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

/**
 * Stroke to view space
 * Transforms a stroke to view space. This allows for manipulations in 2D but also easy conversion
 * back to 3D.
 * Note: also takes care of parent space transform
 */
void BKE_gpencil_stroke_to_view_space(RegionView3D *rv3d,
                                      bGPDstroke *gps,
                                      const float diff_mat[4][4])
{
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    /* Point to parent space. */
    mul_v3_m4v3(&pt->x, diff_mat, &pt->x);
    /* point to view space */
    mul_m4_v3(rv3d->viewmat, &pt->x);
  }
}

/**
 * Stroke from view space
 * Transforms a stroke from view space back to world space. Inverse of
 * BKE_gpencil_stroke_to_view_space
 * Note: also takes care of parent space transform
 */
void BKE_gpencil_stroke_from_view_space(RegionView3D *rv3d,
                                        bGPDstroke *gps,
                                        const float diff_mat[4][4])
{
  float inverse_diff_mat[4][4];
  invert_m4_m4(inverse_diff_mat, diff_mat);

  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    mul_v3_m4v3(&pt->x, rv3d->viewinv, &pt->x);
    mul_m4_v3(inverse_diff_mat, &pt->x);
  }
}

/* ----------------------------------------------------------------------------- */
/* Stroke to perimeter */

typedef struct tPerimeterPoint {
  struct tPerimeterPoint *next, *prev;
  float x, y, z;
} tPerimeterPoint;

static tPerimeterPoint *new_perimeter_point(const float pt[3])
{
  tPerimeterPoint *new_pt = MEM_callocN(sizeof(tPerimeterPoint), __func__);
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
  int num_points = (int)(((1 << (subdivisions + 1)) - 1) * (angle / M_PI));
  if (num_points > 0) {
    float angle_incr = angle / (float)num_points;

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
  float angle_incr = M_PI / ((float)num_points - 1);

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
                                             int *r_num_perimeter_points)
{
  /* sanity check */
  if (gps->totpoints < 1) {
    return NULL;
  }

  float defaultpixsize = 1000.0f / gpd->pixfactor;
  float stroke_radius = ((gps->thickness + gpl->line_change) / defaultpixsize) / 2.0f;

  ListBase *perimeter_right_side = MEM_callocN(sizeof(ListBase), __func__);
  ListBase *perimeter_left_side = MEM_callocN(sizeof(ListBase), __func__);
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

  /* generate points for start cap */
  num_perimeter_points += generate_perimeter_cap(
      first_pt, first_next_pt, first_radius, perimeter_right_side, subdivisions, gps->caps[0]);

  /* generate perimeter points  */
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

/**
 * Calculates the perimeter of a stroke projected from the view and
 * returns it as a new stroke.
 * \param subdivisions: Number of subdivisions for the start and end caps
 * \return: bGPDstroke pointer to stroke perimeter
 */
bGPDstroke *BKE_gpencil_stroke_perimeter_from_view(struct RegionView3D *rv3d,
                                                   bGPdata *gpd,
                                                   const bGPDlayer *gpl,
                                                   bGPDstroke *gps,
                                                   const int subdivisions,
                                                   const float diff_mat[4][4])
{
  if (gps->totpoints == 0) {
    return NULL;
  }
  bGPDstroke *gps_temp = BKE_gpencil_stroke_duplicate(gps, true, false);
  const bool cyclic = ((gps_temp->flag & GP_STROKE_CYCLIC) != 0);

  /* If Cyclic, add a new point. */
  if (cyclic && (gps_temp->totpoints > 1)) {
    gps_temp->totpoints++;
    gps_temp->points = MEM_recallocN(gps_temp->points,
                                     sizeof(*gps_temp->points) * gps_temp->totpoints);
    bGPDspoint *pt_src = &gps_temp->points[0];
    bGPDspoint *pt_dst = &gps_temp->points[gps_temp->totpoints - 1];
    copy_v3_v3(&pt_dst->x, &pt_src->x);
    pt_dst->pressure = pt_src->pressure;
    pt_dst->strength = pt_src->strength;
    pt_dst->uv_fac = 1.0f;
    pt_dst->uv_rot = 0;
  }

  BKE_gpencil_stroke_to_view_space(rv3d, gps_temp, diff_mat);
  int num_perimeter_points = 0;
  ListBase *perimeter_points = gpencil_stroke_perimeter_ex(
      gpd, gpl, gps_temp, subdivisions, &num_perimeter_points);

  if (num_perimeter_points == 0) {
    return NULL;
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

  BKE_gpencil_stroke_from_view_space(rv3d, perimeter_stroke, diff_mat);

  /* Free temp data. */
  BLI_freelistN(perimeter_points);
  MEM_freeN(perimeter_points);

  /* Triangles cache needs to be recalculated. */
  BKE_gpencil_stroke_geometry_update(gpd, perimeter_stroke);

  perimeter_stroke->flag |= GP_STROKE_SELECT | GP_STROKE_CYCLIC;

  BKE_gpencil_free_stroke(gps_temp);

  return perimeter_stroke;
}

/** Get average pressure. */
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

  return tot / (float)gps->totpoints;
}

/** Check if the thickness of the stroke is constant. */
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
