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
#include "BLI_math_vector.h"

#include "BLT_translation.h"

#include "DNA_gpencil_types.h"

#include "BKE_collection.h"
#include "BKE_curve.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_curve.h"
#include "BKE_gpencil_geom.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_object.h"

#include "DEG_depsgraph_query.h"

/* Helper: Check materials with same color. */
static int gpencil_check_same_material_color(Object *ob_gp,
                                             const float color_stroke[4],
                                             const float color_fill[4],
                                             const bool do_fill,
                                             const bool do_stroke,
                                             Material **r_mat)
{
  int index = -1;
  Material *ma = NULL;
  *r_mat = NULL;
  float color_cu[4];
  float hsv_stroke[4], hsv_fill[4];

  copy_v4_v4(color_cu, color_stroke);
  zero_v3(hsv_stroke);
  rgb_to_hsv_v(color_cu, hsv_stroke);
  hsv_stroke[3] = color_stroke[3];

  copy_v4_v4(color_cu, color_fill);
  zero_v3(hsv_fill);
  rgb_to_hsv_v(color_cu, hsv_fill);
  hsv_fill[3] = color_fill[3];

  bool match_stroke = false;
  bool match_fill = false;

  for (int i = 1; i <= ob_gp->totcol; i++) {
    ma = BKE_object_material_get(ob_gp, i);
    MaterialGPencilStyle *gp_style = ma->gp_style;
    const bool fill = (gp_style->fill_style == GP_MATERIAL_FILL_STYLE_SOLID);
    const bool stroke = (gp_style->fill_style == GP_MATERIAL_STROKE_STYLE_SOLID);

    if (do_fill && !fill) {
      continue;
    }

    if (do_stroke && !stroke) {
      continue;
    }

    /* Check color with small tolerance (better result in HSV). */
    float hsv2[4];
    if (do_fill) {
      zero_v3(hsv2);
      rgb_to_hsv_v(gp_style->fill_rgba, hsv2);
      hsv2[3] = gp_style->fill_rgba[3];
      if (compare_v4v4(hsv_fill, hsv2, 0.01f)) {
        *r_mat = ma;
        index = i - 1;
        match_fill = true;
      }
    }
    else {
      match_fill = true;
    }

    if (do_stroke) {
      zero_v3(hsv2);
      rgb_to_hsv_v(gp_style->stroke_rgba, hsv2);
      hsv2[3] = gp_style->stroke_rgba[3];
      if (compare_v4v4(hsv_stroke, hsv2, 0.01f)) {
        *r_mat = ma;
        index = i - 1;
        match_stroke = true;
      }
    }
    else {
      match_stroke = true;
    }

    /* If match, don't look for more. */
    if (match_stroke || match_fill) {
      break;
    }
  }

  if (!match_stroke || !match_fill) {
    *r_mat = NULL;
    index = -1;
  }

  return index;
}

/* Helper: Add gpencil material using curve material as base. */
static Material *gpencil_add_from_curve_material(Main *bmain,
                                                 Object *ob_gp,
                                                 const float stroke_color[4],
                                                 const float fill_color[4],
                                                 const bool stroke,
                                                 const bool fill,
                                                 int *r_idx)
{
  Material *mat_gp = BKE_gpencil_object_material_new(bmain, ob_gp, "Material", r_idx);
  MaterialGPencilStyle *gp_style = mat_gp->gp_style;

  /* Stroke color. */
  if (stroke) {
    copy_v4_v4(mat_gp->gp_style->stroke_rgba, stroke_color);
    gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
  }

  /* Fill color. */
  if (fill) {
    copy_v4_v4(mat_gp->gp_style->fill_rgba, fill_color);
    gp_style->flag |= GP_MATERIAL_FILL_SHOW;
  }

  /* Check at least one is enabled. */
  if (((gp_style->flag & GP_MATERIAL_STROKE_SHOW) == 0) &&
      ((gp_style->flag & GP_MATERIAL_FILL_SHOW) == 0)) {
    gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
  }

  return mat_gp;
}

/* Helper: Create new stroke section. */
static void gpencil_add_new_points(bGPDstroke *gps,
                                   const float *coord_array,
                                   const float pressure_start,
                                   const float pressure_end,
                                   const int init,
                                   const int totpoints,
                                   const float init_co[3],
                                   const bool last)
{
  BLI_assert(totpoints > 0);

  const float step = 1.0f / ((float)totpoints - 1.0f);
  float factor = 0.0f;
  for (int i = 0; i < totpoints; i++) {
    bGPDspoint *pt = &gps->points[i + init];
    copy_v3_v3(&pt->x, &coord_array[3 * i]);
    /* Be sure the last point is not on top of the first point of the curve or
     * the close of the stroke will produce glitches. */
    if ((last) && (i > 0) && (i == totpoints - 1)) {
      float dist = len_v3v3(init_co, &pt->x);
      if (dist < 0.1f) {
        /* Interpolate between previous point and current to back slightly. */
        bGPDspoint *pt_prev = &gps->points[i + init - 1];
        interp_v3_v3v3(&pt->x, &pt_prev->x, &pt->x, 0.95f);
      }
    }

    pt->strength = 1.0f;
    pt->pressure = interpf(pressure_end, pressure_start, factor);
    factor += step;
  }
}

/* Helper: Get the first collection that includes the object. */
static Collection *gpencil_get_parent_collection(Scene *scene, Object *ob)
{
  Collection *mycol = NULL;
  FOREACH_SCENE_COLLECTION_BEGIN (scene, collection) {
    LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
      if ((mycol == NULL) && (cob->ob == ob)) {
        mycol = collection;
      }
    }
  }
  FOREACH_SCENE_COLLECTION_END;

  return mycol;
}
static int gpencil_get_stroke_material_fromcurve(
    Main *bmain, Object *ob_gp, Object *ob_cu, bool *do_stroke, bool *do_fill)
{
  Curve *cu = (Curve *)ob_cu->data;

  Material *mat_gp = NULL;
  Material *mat_curve_stroke = NULL;
  Material *mat_curve_fill = NULL;

  float color_stroke[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float color_fill[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  /* If the curve has 2 materials, the first is considered as Fill and the second as Stroke.
   * If the has only one material, if the name contains _stroke, the is used
   * as stroke, else as fill.*/
  if (ob_cu->totcol >= 2) {
    *do_stroke = true;
    *do_fill = true;
    mat_curve_fill = BKE_object_material_get(ob_cu, 1);
    mat_curve_stroke = BKE_object_material_get(ob_cu, 2);
  }
  else if (ob_cu->totcol == 1) {
    mat_curve_stroke = BKE_object_material_get(ob_cu, 1);
    if ((mat_curve_stroke) && (strstr(mat_curve_stroke->id.name, "_stroke") != NULL)) {
      *do_stroke = true;
      *do_fill = false;
      mat_curve_fill = NULL;
    }
    else {
      *do_stroke = false;
      *do_fill = true;
      /* Invert materials. */
      mat_curve_fill = mat_curve_stroke;
      mat_curve_stroke = NULL;
    }
  }
  else {
    /* No materials in the curve. */
    *do_fill = false;
    return -1;
  }

  if (mat_curve_stroke) {
    copy_v4_v4(color_stroke, &mat_curve_stroke->r);
  }
  if (mat_curve_fill) {
    copy_v4_v4(color_fill, &mat_curve_fill->r);
  }

  int r_idx = gpencil_check_same_material_color(
      ob_gp, color_stroke, color_fill, *do_stroke, *do_fill, &mat_gp);

  if ((ob_gp->totcol < r_idx) || (r_idx < 0)) {
    mat_gp = gpencil_add_from_curve_material(
        bmain, ob_gp, color_stroke, color_fill, *do_stroke, *do_fill, &r_idx);
  }

  /* Set fill and stroke depending of curve type (3D or 2D). */
  if ((cu->flag & CU_3D) || ((cu->flag & (CU_FRONT | CU_BACK)) == 0)) {
    mat_gp->gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
    mat_gp->gp_style->flag &= ~GP_MATERIAL_FILL_SHOW;
  }
  else {
    mat_gp->gp_style->flag &= ~GP_MATERIAL_STROKE_SHOW;
    mat_gp->gp_style->flag |= GP_MATERIAL_FILL_SHOW;
  }

  return r_idx;
}

/* Helper: Convert one spline to grease pencil stroke. */
static void gpencil_convert_spline(Main *bmain,
                                   Object *ob_gp,
                                   Object *ob_cu,
                                   const bool gpencil_lines,
                                   const float scale_thickness,
                                   const float sample,
                                   bGPDframe *gpf,
                                   Nurb *nu)
{
  bool cyclic = true;

  /* Create Stroke. */
  bGPDstroke *gps = MEM_callocN(sizeof(bGPDstroke), "bGPDstroke");
  gps->thickness = 1.0f;
  gps->fill_opacity_fac = 1.0f;
  gps->hardeness = 1.0f;
  gps->uv_scale = 1.0f;

  ARRAY_SET_ITEMS(gps->aspect_ratio, 1.0f, 1.0f);
  ARRAY_SET_ITEMS(gps->caps, GP_STROKE_CAP_ROUND, GP_STROKE_CAP_ROUND);
  gps->inittime = 0.0f;

  gps->flag &= ~GP_STROKE_SELECT;
  gps->flag |= GP_STROKE_3DSPACE;

  gps->mat_nr = 0;
  /* Count total points
   * The total of points must consider that last point of each segment is equal to the first
   * point of next segment.
   */
  int totpoints = 0;
  int segments = 0;
  int resolu = nu->resolu + 1;
  segments = nu->pntsu;
  if ((nu->flagu & CU_NURB_CYCLIC) == 0) {
    segments--;
    cyclic = false;
  }
  totpoints = (resolu * segments) - (segments - 1);

  /* Materials
   * Notice: The color of the material is the color of viewport and not the final shader color.
   */
  bool do_stroke, do_fill;
  int r_idx = gpencil_get_stroke_material_fromcurve(bmain, ob_gp, ob_cu, &do_stroke, &do_fill);
  CLAMP_MIN(r_idx, 0);

  /* Assign material index to stroke. */
  gps->mat_nr = r_idx;

  /* Add stroke to frame.*/
  BLI_addtail(&gpf->strokes, gps);

  float *coord_array = NULL;
  float init_co[3];

  switch (nu->type) {
    case CU_POLY: {
      /* Allocate memory for storage points. */
      gps->totpoints = nu->pntsu;
      gps->points = MEM_callocN(sizeof(bGPDspoint) * gps->totpoints, "gp_stroke_points");
      /* Increase thickness for this type. */
      gps->thickness = 10.0f;

      /* Get all curve points */
      for (int s = 0; s < gps->totpoints; s++) {
        BPoint *bp = &nu->bp[s];
        bGPDspoint *pt = &gps->points[s];
        copy_v3_v3(&pt->x, bp->vec);
        pt->pressure = bp->radius;
        pt->strength = 1.0f;
      }
      break;
    }
    case CU_BEZIER: {
      /* Allocate memory for storage points. */
      gps->totpoints = totpoints;
      gps->points = MEM_callocN(sizeof(bGPDspoint) * gps->totpoints, "gp_stroke_points");

      int init = 0;
      resolu = nu->resolu + 1;
      segments = nu->pntsu;
      if ((nu->flagu & CU_NURB_CYCLIC) == 0) {
        segments--;
      }
      /* Get all interpolated curve points of Beziert */
      for (int s = 0; s < segments; s++) {
        int inext = (s + 1) % nu->pntsu;
        BezTriple *prevbezt = &nu->bezt[s];
        BezTriple *bezt = &nu->bezt[inext];
        bool last = (bool)(s == segments - 1);

        coord_array = MEM_callocN((size_t)3 * resolu * sizeof(float), __func__);

        for (int j = 0; j < 3; j++) {
          BKE_curve_forward_diff_bezier(prevbezt->vec[1][j],
                                        prevbezt->vec[2][j],
                                        bezt->vec[0][j],
                                        bezt->vec[1][j],
                                        coord_array + j,
                                        resolu - 1,
                                        sizeof(float[3]));
        }
        /* Save first point coordinates. */
        if (s == 0) {
          copy_v3_v3(init_co, &coord_array[0]);
        }
        /* Add points to the stroke */
        float radius_start = prevbezt->radius * scale_thickness;
        float radius_end = bezt->radius * scale_thickness;

        gpencil_add_new_points(
            gps, coord_array, radius_start, radius_end, init, resolu, init_co, last);
        /* Free memory. */
        MEM_SAFE_FREE(coord_array);

        /* As the last point of segment is the first point of next segment, back one array
         * element to avoid duplicated points on the same location.
         */
        init += resolu - 1;
      }
      break;
    }
    case CU_NURBS: {
      if (nu->pntsv == 1) {

        int nurb_points;
        if (nu->flagu & CU_NURB_CYCLIC) {
          resolu++;
          nurb_points = nu->pntsu * resolu;
        }
        else {
          nurb_points = (nu->pntsu - 1) * resolu;
        }
        /* Get all curve points. */
        coord_array = MEM_callocN(sizeof(float[3]) * nurb_points, __func__);
        BKE_nurb_makeCurve(nu, coord_array, NULL, NULL, NULL, resolu, sizeof(float[3]));

        /* Allocate memory for storage points. */
        gps->totpoints = nurb_points;
        gps->points = MEM_callocN(sizeof(bGPDspoint) * gps->totpoints, "gp_stroke_points");

        /* Add points. */
        gpencil_add_new_points(gps, coord_array, 1.0f, 1.0f, 0, gps->totpoints, init_co, false);

        MEM_SAFE_FREE(coord_array);
      }
      break;
    }
    default: {
      break;
    }
  }
  /* Cyclic curve, close stroke. */
  if (cyclic) {
    BKE_gpencil_stroke_close(gps);
  }

  if (sample > 0.0f) {
    BKE_gpencil_stroke_sample(gps, sample, false);
  }

  /* Recalc fill geometry. */
  BKE_gpencil_stroke_geometry_update(gps);
}

/**
 * Convert a curve object to grease pencil stroke.
 *
 * \param bmain: Main thread pointer
 * \param scene: Original scene.
 * \param ob_gp: Grease pencil object to add strokes.
 * \param ob_cu: Curve to convert.
 * \param gpencil_lines: Use lines for strokes.
 * \param use_collections: Create layers using collection names.
 * \param scale_thickness: Scale thickness factor.
 * \param sample: Sample distance, zero to disable.
 */
void BKE_gpencil_convert_curve(Main *bmain,
                               Scene *scene,
                               Object *ob_gp,
                               Object *ob_cu,
                               const bool gpencil_lines,
                               const bool use_collections,
                               const float scale_thickness,
                               const float sample)
{
  if (ELEM(NULL, ob_gp, ob_cu) || (ob_gp->type != OB_GPENCIL) || (ob_gp->data == NULL)) {
    return;
  }

  Curve *cu = (Curve *)ob_cu->data;
  bGPdata *gpd = (bGPdata *)ob_gp->data;
  bGPDlayer *gpl = NULL;

  /* If the curve is empty, cancel. */
  if (cu->nurb.first == NULL) {
    return;
  }

  /* Check if there is an active layer. */
  if (use_collections) {
    Collection *collection = gpencil_get_parent_collection(scene, ob_cu);
    if (collection != NULL) {
      gpl = BKE_gpencil_layer_named_get(gpd, collection->id.name + 2);
      if (gpl == NULL) {
        gpl = BKE_gpencil_layer_addnew(gpd, collection->id.name + 2, true);
      }
    }
  }

  if (gpl == NULL) {
    gpl = BKE_gpencil_layer_active_get(gpd);
    if (gpl == NULL) {
      gpl = BKE_gpencil_layer_addnew(gpd, DATA_("GP_Layer"), true);
    }
  }

  /* Check if there is an active frame and add if needed. */
  bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, CFRA, GP_GETFRAME_ADD_COPY);

  /* Read all splines of the curve and create a stroke for each. */
  LISTBASE_FOREACH (Nurb *, nu, &cu->nurb) {
    gpencil_convert_spline(bmain, ob_gp, ob_cu, gpencil_lines, scale_thickness, sample, gpf, nu);
  }

  /* Merge any similar material. */
  int removed = 0;
  BKE_gpencil_merge_materials(ob_gp, 0.001f, 0.001f, 0.001f, &removed);

  /* Remove any unused slot. */
  int actcol = ob_gp->actcol;

  for (int slot = 1; slot <= ob_gp->totcol; slot++) {
    while (slot <= ob_gp->totcol && !BKE_object_material_slot_used(ob_gp->data, slot)) {
      ob_gp->actcol = slot;
      BKE_object_material_slot_remove(bmain, ob_gp);

      if (actcol >= slot) {
        actcol--;
      }
    }
  }

  ob_gp->actcol = actcol;

  /* Tag for recalculation */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  DEG_id_tag_update(&ob_gp->id, ID_RECALC_GEOMETRY);
}

/** \} */
