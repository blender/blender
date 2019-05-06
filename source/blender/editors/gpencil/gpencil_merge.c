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
 * The Original Code is Copyright (C) 2019, Blender Foundation.
 * This is a new part of Blender
 * Operators for merge Grease Pencil strokes
 */

/** \file
 * \ingroup edgpencil
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"

#include "DNA_gpencil_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_material.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_gpencil.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

typedef struct tGPencilPointCache {
  float factor; /* value to sort */
  bGPDstroke *gps;
  float x, y, z;
  float pressure;
  float strength;
} tGPencilPointCache;

/* helper function to sort points */
static int gpencil_sort_points(const void *a1, const void *a2)
{
  const tGPencilPointCache *ps1 = a1, *ps2 = a2;

  if (ps1->factor < ps2->factor) {
    return -1;
  }
  else if (ps1->factor > ps2->factor) {
    return 1;
  }

  return 0;
}

static void gpencil_insert_points_to_stroke(bGPDstroke *gps,
                                            tGPencilPointCache *points_array,
                                            int totpoints)
{
  tGPencilPointCache *point_elem = NULL;

  for (int i = 0; i < totpoints; i++) {
    point_elem = &points_array[i];
    bGPDspoint *pt_dst = &gps->points[i];

    copy_v3_v3(&pt_dst->x, &point_elem->x);
    pt_dst->pressure = point_elem->pressure;
    pt_dst->strength = point_elem->strength;
    pt_dst->uv_fac = 1.0f;
    pt_dst->uv_rot = 0;
    pt_dst->flag |= GP_SPOINT_SELECT;
  }
}

static bGPDstroke *gpencil_prepare_stroke(bContext *C, wmOperator *op, int totpoints)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);

  int cfra_eval = (int)DEG_get_ctime(depsgraph);

  const bool back = RNA_boolean_get(op->ptr, "back");
  const bool additive = RNA_boolean_get(op->ptr, "additive");
  const bool cyclic = RNA_boolean_get(op->ptr, "cyclic");

  Paint *paint = &ts->gp_paint->paint;
  /* if not exist, create a new one */
  if ((paint->brush == NULL) || (paint->brush->gpencil_settings == NULL)) {
    /* create new brushes */
    BKE_brush_gpencil_presets(C);
  }
  Brush *brush = paint->brush;

  /* frame */
  short add_frame_mode;
  if (additive) {
    add_frame_mode = GP_GETFRAME_ADD_COPY;
  }
  else {
    add_frame_mode = GP_GETFRAME_ADD_NEW;
  }
  bGPDframe *gpf = BKE_gpencil_layer_getframe(gpl, cfra_eval, add_frame_mode);

  /* stroke */
  bGPDstroke *gps = MEM_callocN(sizeof(bGPDstroke), "gp_stroke");
  gps->totpoints = totpoints;
  gps->inittime = 0.0f;
  gps->thickness = brush->size;
  gps->gradient_f = brush->gpencil_settings->gradient_f;
  copy_v2_v2(gps->gradient_s, brush->gpencil_settings->gradient_s);
  gps->flag |= GP_STROKE_SELECT;
  gps->flag |= GP_STROKE_3DSPACE;
  gps->mat_nr = ob->actcol - 1;

  /* allocate memory for points */
  gps->points = MEM_callocN(sizeof(bGPDspoint) * totpoints, "gp_stroke_points");
  /* initialize triangle memory to dummy data */
  gps->tot_triangles = 0;
  gps->triangles = NULL;
  gps->flag |= GP_STROKE_RECALC_GEOMETRY;

  if (cyclic) {
    gps->flag |= GP_STROKE_CYCLIC;
  }

  /* add new stroke to frame */
  if (back) {
    BLI_addhead(&gpf->strokes, gps);
  }
  else {
    BLI_addtail(&gpf->strokes, gps);
  }

  return gps;
}

static void gpencil_get_elements_len(bContext *C, int *totstrokes, int *totpoints)
{
  bGPDspoint *pt;
  int i;

  /* count number of strokes and selected points */
  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    if (gps->flag & GP_STROKE_SELECT) {
      *totstrokes += 1;
      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        if (pt->flag & GP_SPOINT_SELECT) {
          *totpoints += 1;
        }
      }
    }
  }
  CTX_DATA_END;
}

static void gpencil_dissolve_points(bContext *C)
{
  bGPDstroke *gps, *gpsn;

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *gpf = gpl->actframe;
    if (gpf == NULL) {
      continue;
    }

    for (gps = gpf->strokes.first; gps; gps = gpsn) {
      gpsn = gps->next;
      gp_stroke_delete_tagged_points(gpf, gps, gpsn, GP_SPOINT_TAG, false, 0);
    }
  }
  CTX_DATA_END;
}

/* Calc a factor of each selected point and fill an array with all the data.
 *
 * The factor is calculated using an imaginary circle, using the angle relative
 * to this circle and the distance to the calculated center of the selected points.
 *
 * All the data is saved to be sorted and used later.
 */
static void gpencil_calc_points_factor(bContext *C,
                                       const int mode,
                                       int totpoints,
                                       const bool clear_point,
                                       const bool clear_stroke,
                                       tGPencilPointCache *src_array)
{
  bGPDspoint *pt;
  int i;
  int idx = 0;

  /* create selected point array an fill it */
  bGPDstroke **gps_array = MEM_callocN(sizeof(bGPDstroke *) * totpoints, __func__);
  bGPDspoint *pt_array = MEM_callocN(sizeof(bGPDspoint) * totpoints, __func__);

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *gpf = gpl->actframe;
    if (gpf == NULL) {
      continue;
    }
    for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
      if (gps->flag & GP_STROKE_SELECT) {
        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          if (clear_stroke) {
            pt->flag |= GP_SPOINT_TAG;
          }
          else {
            pt->flag &= ~GP_SPOINT_TAG;
          }

          if (pt->flag & GP_SPOINT_SELECT) {
            bGPDspoint *pt2 = &pt_array[idx];
            copy_v3_v3(&pt2->x, &pt->x);
            pt2->pressure = pt->pressure;
            pt2->strength = pt->strength;
            pt->flag &= ~GP_SPOINT_SELECT;
            if (clear_point) {
              pt->flag |= GP_SPOINT_TAG;
            }

            /* save stroke */
            gps_array[idx] = gps;

            idx++;
          }
        }
        gps->flag &= ~GP_STROKE_SELECT;
      }
    }
  }
  CTX_DATA_END;

  /* project in 2d plane */
  int direction = 0;
  float(*points2d)[2] = MEM_mallocN(sizeof(*points2d) * totpoints, "GP Stroke temp 2d points");
  BKE_gpencil_stroke_2d_flat(pt_array, totpoints, points2d, &direction);

  /* calc center */
  float center[2] = {0.0f, 0.0f};
  for (i = 0; i < totpoints; i++) {
    center[0] += points2d[i][0];
    center[1] += points2d[i][1];
  }
  mul_v2_fl(center, 1.0f / totpoints);

  /* calc angle and distance to center for each point */
  const float axis[2] = {1.0f, 0.0f};
  float v1[3];
  for (i = 0; i < totpoints; i++) {
    float ln = len_v2v2(center, points2d[i]);
    sub_v2_v2v2(v1, points2d[i], center);
    float angle = angle_signed_v2v2(axis, v1);
    if (angle < 0.0f) {
      angle = fabsf(angle);
    }
    else {
      angle = (M_PI * 2.0) - angle;
    }
    tGPencilPointCache *sort_pt = &src_array[i];
    bGPDspoint *pt2 = &pt_array[i];

    copy_v3_v3(&sort_pt->x, &pt2->x);
    sort_pt->pressure = pt2->pressure;
    sort_pt->strength = pt2->strength;

    sort_pt->gps = gps_array[i];

    if (mode == GP_MERGE_STROKE) {
      sort_pt->factor = angle;
    }
    else {
      sort_pt->factor = (angle * 100000.0f) + ln;
    }
  }
  MEM_SAFE_FREE(points2d);
  MEM_SAFE_FREE(gps_array);
  MEM_SAFE_FREE(pt_array);
}

/* insert a group of points in destination array */
static int gpencil_insert_to_array(tGPencilPointCache *src_array,
                                   tGPencilPointCache *dst_array,
                                   int totpoints,
                                   bGPDstroke *gps_filter,
                                   bool reverse,
                                   int last)
{
  tGPencilPointCache *src_elem = NULL;
  tGPencilPointCache *dst_elem = NULL;
  int idx = 0;

  for (int i = 0; i < totpoints; i++) {
    if (!reverse) {
      idx = i;
    }
    else {
      idx = totpoints - i - 1;
    }
    src_elem = &src_array[idx];
    /* check if all points or only a stroke */
    if ((gps_filter != NULL) && (gps_filter != src_elem->gps)) {
      continue;
    }

    dst_elem = &dst_array[last];
    last++;

    copy_v3_v3(&dst_elem->x, &src_elem->x);
    dst_elem->gps = src_elem->gps;
    dst_elem->pressure = src_elem->pressure;
    dst_elem->strength = src_elem->strength;
    dst_elem->factor = src_elem->factor;
  }

  return last;
}

/* get first and last point location */
static void gpencil_get_extremes(
    tGPencilPointCache *src_array, int totpoints, bGPDstroke *gps_filter, float *start, float *end)
{
  tGPencilPointCache *array_pt = NULL;
  int i;

  /* find first point */
  for (i = 0; i < totpoints; i++) {
    array_pt = &src_array[i];
    if (gps_filter == array_pt->gps) {
      copy_v3_v3(start, &array_pt->x);
      break;
    }
  }
  /* find last point */
  for (i = totpoints - 1; i >= 0; i--) {
    array_pt = &src_array[i];
    if (gps_filter == array_pt->gps) {
      copy_v3_v3(end, &array_pt->x);
      break;
    }
  }
}

static int gpencil_analyze_strokes(tGPencilPointCache *src_array,
                                   int totstrokes,
                                   int totpoints,
                                   tGPencilPointCache *dst_array)
{
  int i;
  int last = 0;
  GHash *all_strokes = BLI_ghash_ptr_new(__func__);
  /* add first stroke to array */
  tGPencilPointCache *sort_pt = &src_array[0];
  bGPDstroke *gps = sort_pt->gps;
  last = gpencil_insert_to_array(src_array, dst_array, totpoints, gps, false, last);
  float start[3];
  float end[3];
  float end_prv[3];
  gpencil_get_extremes(src_array, totpoints, gps, start, end);
  copy_v3_v3(end_prv, end);
  BLI_ghash_insert(all_strokes, sort_pt->gps, sort_pt->gps);

  /* look for near stroke */
  bool loop = (bool)(totstrokes > 1);
  while (loop) {
    bGPDstroke *gps_next = NULL;
    GHash *strokes = BLI_ghash_ptr_new(__func__);
    float dist_start = 0.0f;
    float dist_end = 0.0f;
    float dist = FLT_MAX;
    bool reverse = false;

    for (i = 0; i < totpoints; i++) {
      sort_pt = &src_array[i];
      /* avoid dups */
      if (BLI_ghash_haskey(all_strokes, sort_pt->gps)) {
        continue;
      }
      if (!BLI_ghash_haskey(strokes, sort_pt->gps)) {
        gpencil_get_extremes(src_array, totpoints, sort_pt->gps, start, end);
        /* distances to previous end */
        dist_start = len_v3v3(end_prv, start);
        dist_end = len_v3v3(end_prv, end);

        if (dist > dist_start) {
          gps_next = sort_pt->gps;
          dist = dist_start;
          reverse = false;
        }
        if (dist > dist_end) {
          gps_next = sort_pt->gps;
          dist = dist_end;
          reverse = true;
        }
        BLI_ghash_insert(strokes, sort_pt->gps, sort_pt->gps);
      }
    }
    BLI_ghash_free(strokes, NULL, NULL);

    /* add the stroke to array */
    if (gps->next != NULL) {
      BLI_ghash_insert(all_strokes, gps_next, gps_next);
      last = gpencil_insert_to_array(src_array, dst_array, totpoints, gps_next, reverse, last);
      /* replace last end */
      sort_pt = &dst_array[last - 1];
      copy_v3_v3(end_prv, &sort_pt->x);
    }

    /* loop exit */
    if (last >= totpoints) {
      loop = false;
    }
  }

  BLI_ghash_free(all_strokes, NULL, NULL);
  return last;
}

static bool gp_strokes_merge_poll(bContext *C)
{
  /* only supported with grease pencil objects */
  Object *ob = CTX_data_active_object(C);
  if ((ob == NULL) || (ob->type != OB_GPENCIL)) {
    return false;
  }

  /* check material */
  Material *ma = NULL;
  ma = give_current_material(ob, ob->actcol);
  if ((ma == NULL) || (ma->gp_style == NULL)) {
    return false;
  }

  /* check hidden or locked materials */
  MaterialGPencilStyle *gp_style = ma->gp_style;
  if ((gp_style->flag & GP_STYLE_COLOR_HIDE) || (gp_style->flag & GP_STYLE_COLOR_LOCKED)) {
    return false;
  }

  /* check layer */
  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
  if ((gpl == NULL) || (gpl->flag & GP_LAYER_LOCKED) || (gpl->flag & GP_LAYER_HIDE)) {
    return false;
  }

  /* NOTE: this is a bit slower, but is the most accurate... */
  return (CTX_DATA_COUNT(C, editable_gpencil_strokes) != 0) && ED_operator_view3d_active(C);
}

static int gp_stroke_merge_exec(bContext *C, wmOperator *op)
{
  const int mode = RNA_enum_get(op->ptr, "mode");
  const bool clear_point = RNA_boolean_get(op->ptr, "clear_point");
  const bool clear_stroke = RNA_boolean_get(op->ptr, "clear_stroke");

  Object *ob = CTX_data_active_object(C);
  /* sanity checks */
  if (!ob || ob->type != OB_GPENCIL) {
    return OPERATOR_CANCELLED;
  }

  bGPdata *gpd = (bGPdata *)ob->data;
  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
  if (gpl == NULL) {
    return OPERATOR_CANCELLED;
  }

  int totstrokes = 0;
  int totpoints = 0;

  /* count number of strokes and selected points */
  gpencil_get_elements_len(C, &totstrokes, &totpoints);

  if (totpoints == 0) {
    return OPERATOR_CANCELLED;
  }

  /* calc factor of each point and fill an array with all data */
  tGPencilPointCache *sorted_array = NULL;
  tGPencilPointCache *original_array = MEM_callocN(sizeof(tGPencilPointCache) * totpoints,
                                                   __func__);
  gpencil_calc_points_factor(C, mode, totpoints, clear_point, clear_stroke, original_array);

  /* for strokes analyze strokes and load sorted array */
  if (mode == GP_MERGE_STROKE) {
    sorted_array = MEM_callocN(sizeof(tGPencilPointCache) * totpoints, __func__);
    totpoints = gpencil_analyze_strokes(original_array, totstrokes, totpoints, sorted_array);
  }
  else {
    /* make a copy to sort */
    sorted_array = MEM_dupallocN(original_array);
    /* sort by factor around center */
    qsort(sorted_array, totpoints, sizeof(tGPencilPointCache), gpencil_sort_points);
  }

  /* prepare the new stroke */
  bGPDstroke *gps = gpencil_prepare_stroke(C, op, totpoints);

  /* copy original points to final stroke */
  gpencil_insert_points_to_stroke(gps, sorted_array, totpoints);

  /* dissolve all tagged points */
  if ((clear_point) || (clear_stroke)) {
    gpencil_dissolve_points(C);
  }

  /* free memory */
  MEM_SAFE_FREE(original_array);
  MEM_SAFE_FREE(sorted_array);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_merge(wmOperatorType *ot)
{
  static const EnumPropertyItem mode_type[] = {
      {GP_MERGE_STROKE, "STROKE", 0, "Stroke", ""},
      {GP_MERGE_POINT, "POINT", 0, "Point", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Merge Strokes";
  ot->idname = "GPENCIL_OT_stroke_merge";
  ot->description = "Create a new stroke with the selected stroke points";

  /* api callbacks */
  ot->exec = gp_stroke_merge_exec;
  ot->poll = gp_strokes_merge_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "mode", mode_type, GP_MERGE_STROKE, "Mode", "");
  RNA_def_boolean(
      ot->srna, "back", 0, "Draw on Back", "Draw new stroke below all previous strokes");
  RNA_def_boolean(ot->srna, "additive", 0, "Additive Drawing", "Add to previous drawing");
  RNA_def_boolean(ot->srna, "cyclic", 0, "Cyclic", "Close new stroke");
  RNA_def_boolean(ot->srna, "clear_point", 0, "Dissolve Points", "Dissolve old selected points");
  RNA_def_boolean(ot->srna, "clear_stroke", 0, "Delete Strokes", "Delete old selected strokes");
}
