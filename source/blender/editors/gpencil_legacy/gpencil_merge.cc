/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 * Operators for merge Grease Pencil strokes.
 */

#include <cstdio>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math_vector.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"

#include "BKE_brush.hh"
#include "BKE_context.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_report.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_screen.hh"

#include "DEG_depsgraph.h"

#include "gpencil_intern.h"

struct tGPencilPointCache {
  float factor; /* value to sort */
  bGPDstroke *gps;
  float x, y, z;
  float pressure;
  float strength;
  float vert_color[4];
};

/* helper function to sort points */
static int gpencil_sort_points(const void *a1, const void *a2)
{
  const tGPencilPointCache *ps1 = static_cast<const tGPencilPointCache *>(a1),
                           *ps2 = static_cast<const tGPencilPointCache *>(a2);

  if (ps1->factor < ps2->factor) {
    return -1;
  }

  if (ps1->factor > ps2->factor) {
    return 1;
  }

  return 0;
}

static void gpencil_insert_points_to_stroke(bGPDstroke *gps,
                                            tGPencilPointCache *points_array,
                                            int totpoints)
{
  tGPencilPointCache *point_elem = nullptr;

  for (int i = 0; i < totpoints; i++) {
    point_elem = &points_array[i];
    bGPDspoint *pt_dst = &gps->points[i];

    copy_v3_v3(&pt_dst->x, &point_elem->x);
    pt_dst->pressure = point_elem->pressure;
    pt_dst->strength = point_elem->strength;
    pt_dst->uv_fac = 1.0f;
    pt_dst->uv_rot = 0;
    pt_dst->flag |= GP_SPOINT_SELECT;
    copy_v4_v4(pt_dst->vert_color, point_elem->vert_color);
  }
}

static bGPDstroke *gpencil_prepare_stroke(bContext *C, wmOperator *op, int totpoints)
{
  Main *bmain = CTX_data_main(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);

  Scene *scene = CTX_data_scene(C);

  const bool back = RNA_boolean_get(op->ptr, "back");
  const bool additive = RNA_boolean_get(op->ptr, "additive");
  const bool cyclic = RNA_boolean_get(op->ptr, "cyclic");

  Paint *paint = &ts->gp_paint->paint;
  /* if not exist, create a new one */
  if ((paint->brush == nullptr) || (paint->brush->gpencil_settings == nullptr)) {
    /* create new brushes */
    BKE_brush_gpencil_paint_presets(bmain, ts, false);
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
  bGPDframe *gpf = BKE_gpencil_layer_frame_get(
      gpl, scene->r.cfra, eGP_GetFrame_Mode(add_frame_mode));

  /* stroke */
  bGPDstroke *gps = BKE_gpencil_stroke_new(MAX2(ob->actcol - 1, 0), totpoints, brush->size);
  gps->flag |= GP_STROKE_SELECT;
  BKE_gpencil_stroke_select_index_set(gpd, gps);

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
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *gpf = gpl->actframe;
    if (gpf == nullptr) {
      continue;
    }

    LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
      BKE_gpencil_stroke_delete_tagged_points(
          gpd, gpf, gps, gps->next, GP_SPOINT_TAG, false, false, 0);
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
  int idx = 0;

  /* create selected point array an fill it */
  bGPDstroke **gps_array = static_cast<bGPDstroke **>(
      MEM_callocN(sizeof(bGPDstroke *) * totpoints, __func__));
  bGPDspoint *pt_array = static_cast<bGPDspoint *>(
      MEM_callocN(sizeof(bGPDspoint) * totpoints, __func__));

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *gpf = gpl->actframe;
    if (gpf == nullptr) {
      continue;
    }
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      if (gps->flag & GP_STROKE_SELECT) {
        int i;
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
            copy_v4_v4(pt2->vert_color, pt->vert_color);
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
        BKE_gpencil_stroke_select_index_reset(gps);
      }
    }
  }
  CTX_DATA_END;

  /* project in 2d plane */
  int direction = 0;
  float(*points2d)[2] = static_cast<float(*)[2]>(
      MEM_mallocN(sizeof(*points2d) * totpoints, "GP Stroke temp 2d points"));
  BKE_gpencil_stroke_2d_flat(pt_array, totpoints, points2d, &direction);

  /* calc center */
  float center[2] = {0.0f, 0.0f};
  for (int i = 0; i < totpoints; i++) {
    center[0] += points2d[i][0];
    center[1] += points2d[i][1];
  }
  mul_v2_fl(center, 1.0f / totpoints);

  /* calc angle and distance to center for each point */
  const float axis[2] = {1.0f, 0.0f};
  float v1[3];
  for (int i = 0; i < totpoints; i++) {
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
    copy_v4_v4(sort_pt->vert_color, pt2->vert_color);

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
  tGPencilPointCache *src_elem = nullptr;
  tGPencilPointCache *dst_elem = nullptr;
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
    if (!ELEM(gps_filter, nullptr, src_elem->gps)) {
      continue;
    }

    dst_elem = &dst_array[last];
    last++;

    copy_v3_v3(&dst_elem->x, &src_elem->x);
    dst_elem->gps = src_elem->gps;
    dst_elem->pressure = src_elem->pressure;
    dst_elem->strength = src_elem->strength;
    dst_elem->factor = src_elem->factor;
    copy_v4_v4(dst_elem->vert_color, src_elem->vert_color);
  }

  return last;
}

/* get first and last point location */
static void gpencil_get_extremes(
    tGPencilPointCache *src_array, int totpoints, bGPDstroke *gps_filter, float *start, float *end)
{
  tGPencilPointCache *array_pt = nullptr;

  /* find first point */
  for (int i = 0; i < totpoints; i++) {
    array_pt = &src_array[i];
    if (gps_filter == array_pt->gps) {
      copy_v3_v3(start, &array_pt->x);
      break;
    }
  }
  /* find last point */
  for (int i = totpoints - 1; i >= 0; i--) {
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
  bool loop = bool(totstrokes > 1);
  while (loop) {
    bGPDstroke *gps_next = nullptr;
    GHash *strokes = BLI_ghash_ptr_new(__func__);
    float dist_start = 0.0f;
    float dist_end = 0.0f;
    float dist = FLT_MAX;
    bool reverse = false;

    for (i = 0; i < totpoints; i++) {
      sort_pt = &src_array[i];
      /* Avoid duplicates. */
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
    BLI_ghash_free(strokes, nullptr, nullptr);

    /* add the stroke to array */
    if (gps_next != nullptr) {
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

  BLI_ghash_free(all_strokes, nullptr, nullptr);
  return last;
}

static bool gpencil_strokes_merge_poll(bContext *C)
{
  /* only supported with grease pencil objects */
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }

  /* check material */
  Material *ma = nullptr;
  ma = BKE_gpencil_material(ob, ob->actcol);
  if ((ma == nullptr) || (ma->gp_style == nullptr)) {
    return false;
  }

  /* check hidden or locked materials */
  MaterialGPencilStyle *gp_style = ma->gp_style;
  if ((gp_style->flag & GP_MATERIAL_HIDE) || (gp_style->flag & GP_MATERIAL_LOCKED)) {
    return false;
  }

  /* check layer */
  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
  if ((gpl == nullptr) || (gpl->flag & GP_LAYER_LOCKED) || (gpl->flag & GP_LAYER_HIDE)) {
    return false;
  }

  /* NOTE: this is a bit slower, but is the most accurate... */
  return (CTX_DATA_COUNT(C, editable_gpencil_strokes) != 0) && ED_operator_view3d_active(C);
}

static int gpencil_stroke_merge_exec(bContext *C, wmOperator *op)
{
  const int mode = RNA_enum_get(op->ptr, "mode");
  const bool clear_point = RNA_boolean_get(op->ptr, "clear_point");
  const bool clear_stroke = RNA_boolean_get(op->ptr, "clear_stroke");

  Object *ob = CTX_data_active_object(C);
  /* sanity checks */
  if (!ob || ob->type != OB_GPENCIL_LEGACY) {
    return OPERATOR_CANCELLED;
  }

  bGPdata *gpd = (bGPdata *)ob->data;
  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
  if (gpl == nullptr) {
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
  tGPencilPointCache *sorted_array = nullptr;
  tGPencilPointCache *original_array = static_cast<tGPencilPointCache *>(
      MEM_callocN(sizeof(tGPencilPointCache) * totpoints, __func__));
  gpencil_calc_points_factor(C, mode, totpoints, clear_point, clear_stroke, original_array);

  /* for strokes analyze strokes and load sorted array */
  if (mode == GP_MERGE_STROKE) {
    sorted_array = static_cast<tGPencilPointCache *>(
        MEM_callocN(sizeof(tGPencilPointCache) * totpoints, __func__));
    totpoints = gpencil_analyze_strokes(original_array, totstrokes, totpoints, sorted_array);
  }
  else {
    /* make a copy to sort */
    sorted_array = static_cast<tGPencilPointCache *>(MEM_dupallocN(original_array));
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

  BKE_gpencil_stroke_geometry_update(gpd, gps);

  /* free memory */
  MEM_SAFE_FREE(original_array);
  MEM_SAFE_FREE(sorted_array);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_merge(wmOperatorType *ot)
{
  static const EnumPropertyItem mode_type[] = {
      {GP_MERGE_STROKE, "STROKE", 0, "Stroke", ""},
      {GP_MERGE_POINT, "POINT", 0, "Point", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Merge Strokes";
  ot->idname = "GPENCIL_OT_stroke_merge";
  ot->description = "Create a new stroke with the selected stroke points";

  /* api callbacks */
  ot->exec = gpencil_stroke_merge_exec;
  ot->poll = gpencil_strokes_merge_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "mode", mode_type, GP_MERGE_STROKE, "Mode", "");
  RNA_def_boolean(
      ot->srna, "back", false, "Draw on Back", "Draw new stroke below all previous strokes");
  RNA_def_boolean(ot->srna, "additive", false, "Additive Drawing", "Add to previous drawing");
  RNA_def_boolean(ot->srna, "cyclic", false, "Cyclic", "Close new stroke");
  RNA_def_boolean(
      ot->srna, "clear_point", false, "Dissolve Points", "Dissolve old selected points");
  RNA_def_boolean(
      ot->srna, "clear_stroke", false, "Delete Strokes", "Delete old selected strokes");
}

/* Merge similar materials. */
static bool gpencil_stroke_merge_material_poll(bContext *C)
{
  /* only supported with grease pencil objects */
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }

  return true;
}

static int gpencil_stroke_merge_material_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  const float hue_threshold = RNA_float_get(op->ptr, "hue_threshold");
  const float sat_threshold = RNA_float_get(op->ptr, "sat_threshold");
  const float val_threshold = RNA_float_get(op->ptr, "val_threshold");

  /* Review materials. */
  short *totcol = BKE_object_material_len_p(ob);
  if (totcol == nullptr) {
    return OPERATOR_CANCELLED;
  }

  int removed;
  bool changed = BKE_gpencil_merge_materials(
      ob, hue_threshold, sat_threshold, val_threshold, &removed);

  /* notifiers */
  if (changed) {
    BKE_reportf(op->reports, RPT_INFO, "Merged %d materials of %d", removed, *totcol);
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }
  else {
    BKE_report(op->reports, RPT_INFO, "Nothing to merge");
  }
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_merge_material(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Merge Grease Pencil Materials";
  ot->idname = "GPENCIL_OT_stroke_merge_material";
  ot->description = "Replace materials in strokes merging similar";

  /* api callbacks */
  ot->exec = gpencil_stroke_merge_material_exec;
  ot->poll = gpencil_stroke_merge_material_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_float(
      ot->srna, "hue_threshold", 0.001f, 0.0f, 1.0f, "Hue Threshold", "", 0.0f, 1.0f);
  prop = RNA_def_float(
      ot->srna, "sat_threshold", 0.001f, 0.0f, 1.0f, "Saturation Threshold", "", 0.0f, 1.0f);
  prop = RNA_def_float(
      ot->srna, "val_threshold", 0.001f, 0.0f, 1.0f, "Value Threshold", "", 0.0f, 1.0f);
  /* avoid re-using last var */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}
