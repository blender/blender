/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "BLI_blenlib.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"

#include "BLT_translation.hh"

#include "DNA_collection_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_gpencil_curve_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_main.hh"
#include "BKE_material.h"
#include "BKE_object.hh"

extern "C" {
#include "curve_fit_nd.h"
}

#include "DEG_depsgraph_query.hh"

#define COORD_FITTING_INFLUENCE 20.0f

/* -------------------------------------------------------------------- */
/** \name Convert to curve object
 * \{ */

/* Helper: Check materials with same color. */
static int gpencil_check_same_material_color(Object *ob_gp,
                                             const float color_stroke[4],
                                             const float color_fill[4],
                                             const bool do_stroke,
                                             const bool do_fill,
                                             Material **r_mat)
{
  int index = -1;
  Material *ma = nullptr;
  *r_mat = nullptr;
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
    *r_mat = nullptr;
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
                                                 int *r_index)
{
  Material *mat_gp = BKE_gpencil_object_material_new(bmain, ob_gp, "Material", r_index);
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
      ((gp_style->flag & GP_MATERIAL_FILL_SHOW) == 0))
  {
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

  const float step = 1.0f / (float(totpoints) - 1.0f);
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
  Collection *mycol = nullptr;
  FOREACH_SCENE_COLLECTION_BEGIN (scene, collection) {
    LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
      if ((mycol == nullptr) && (cob->ob == ob)) {
        mycol = collection;
      }
    }
  }
  FOREACH_SCENE_COLLECTION_END;

  return mycol;
}
static int gpencil_get_stroke_material_fromcurve(
    Main *bmain, Object *ob_gp, Object *ob_cu, bool *r_do_stroke, bool *r_do_fill)
{
  Curve *cu = (Curve *)ob_cu->data;

  Material *mat_gp = nullptr;
  Material *mat_curve_stroke = nullptr;
  Material *mat_curve_fill = nullptr;

  float color_stroke[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float color_fill[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  /* If the curve has 2 materials, the first is considered as Fill and the second as Stroke.
   * If the has only one material, if the name contains "_stroke",
   * it's used as a stroke, otherwise as fill. */
  if (ob_cu->totcol >= 2) {
    *r_do_stroke = true;
    *r_do_fill = true;
    mat_curve_fill = BKE_object_material_get(ob_cu, 1);
    mat_curve_stroke = BKE_object_material_get(ob_cu, 2);
  }
  else if (ob_cu->totcol == 1) {
    mat_curve_stroke = BKE_object_material_get(ob_cu, 1);
    if ((mat_curve_stroke) && (strstr(mat_curve_stroke->id.name, "_stroke") != nullptr)) {
      *r_do_stroke = true;
      *r_do_fill = false;
      mat_curve_fill = nullptr;
    }
    else {
      *r_do_stroke = false;
      *r_do_fill = true;
      /* Invert materials. */
      mat_curve_fill = mat_curve_stroke;
      mat_curve_stroke = nullptr;
    }
  }
  else {
    /* No materials in the curve. */
    *r_do_fill = false;
    return -1;
  }

  if (mat_curve_stroke) {
    copy_v4_v4(color_stroke, &mat_curve_stroke->r);
  }
  if (mat_curve_fill) {
    copy_v4_v4(color_fill, &mat_curve_fill->r);
  }

  int index = gpencil_check_same_material_color(
      ob_gp, color_stroke, color_fill, *r_do_stroke, *r_do_fill, &mat_gp);

  if ((ob_gp->totcol < index) || (index < 0)) {
    mat_gp = gpencil_add_from_curve_material(
        bmain, ob_gp, color_stroke, color_fill, *r_do_stroke, *r_do_fill, &index);
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

  return index;
}

/* Helper: Convert one spline to grease pencil stroke. */
static void gpencil_convert_spline(Main *bmain,
                                   Object *ob_gp,
                                   Object *ob_cu,
                                   const float scale_thickness,
                                   const float sample,
                                   bGPDframe *gpf,
                                   Nurb *nu)
{
  bGPdata *gpd = (bGPdata *)ob_gp->data;
  bool cyclic = true;

  /* Create Stroke. */
  bGPDstroke *gps = static_cast<bGPDstroke *>(MEM_callocN(sizeof(bGPDstroke), "bGPDstroke"));
  gps->thickness = 1.0f;
  gps->fill_opacity_fac = 1.0f;
  gps->hardness = 1.0f;
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
  int index = gpencil_get_stroke_material_fromcurve(bmain, ob_gp, ob_cu, &do_stroke, &do_fill);
  CLAMP_MIN(index, 0);

  /* Assign material index to stroke. */
  gps->mat_nr = index;

  /* Add stroke to frame. */
  BLI_addtail(&gpf->strokes, gps);

  float init_co[3];

  switch (nu->type) {
    case CU_POLY: {
      /* Allocate memory for storage points. */
      gps->totpoints = nu->pntsu;
      gps->points = static_cast<bGPDspoint *>(
          MEM_callocN(sizeof(bGPDspoint) * gps->totpoints, "gp_stroke_points"));
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
      gps->points = static_cast<bGPDspoint *>(
          MEM_callocN(sizeof(bGPDspoint) * gps->totpoints, "gp_stroke_points"));

      int init = 0;
      resolu = nu->resolu + 1;
      segments = nu->pntsu;
      if ((nu->flagu & CU_NURB_CYCLIC) == 0) {
        segments--;
      }
      /* Get all interpolated curve points of Bezier. */
      for (int s = 0; s < segments; s++) {
        int inext = (s + 1) % nu->pntsu;
        BezTriple *prevbezt = &nu->bezt[s];
        BezTriple *bezt = &nu->bezt[inext];
        bool last = bool(s == segments - 1);

        float *coord_array = static_cast<float *>(
            MEM_callocN(sizeof(float[3]) * resolu, __func__));
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
        MEM_freeN(coord_array);

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
        float *coord_array = static_cast<float *>(
            MEM_callocN(sizeof(float[3]) * nurb_points, __func__));
        BKE_nurb_makeCurve(nu, coord_array, nullptr, nullptr, nullptr, resolu, sizeof(float[3]));

        /* Allocate memory for storage points. */
        gps->totpoints = nurb_points;
        gps->points = static_cast<bGPDspoint *>(
            MEM_callocN(sizeof(bGPDspoint) * gps->totpoints, "gp_stroke_points"));

        /* Add points. */
        gpencil_add_new_points(gps, coord_array, 1.0f, 1.0f, 0, gps->totpoints, init_co, false);

        MEM_freeN(coord_array);
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
    BKE_gpencil_stroke_sample(gpd, gps, sample, false, 0);
  }

  /* Recalc fill geometry. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

static void gpencil_editstroke_deselect_all(bGPDcurve *gpc)
{
  for (int i = 0; i < gpc->tot_curve_points; i++) {
    bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
    BezTriple *bezt = &gpc_pt->bezt;
    gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
    BEZT_DESEL_ALL(bezt);
  }
  gpc->flag &= ~GP_CURVE_SELECT;
}

void BKE_gpencil_convert_curve(Main *bmain,
                               Scene *scene,
                               Object *ob_gp,
                               Object *ob_cu,
                               const bool use_collections,
                               const float scale_thickness,
                               const float sample)
{
  if (ELEM(nullptr, ob_gp, ob_cu) || (ob_gp->type != OB_GPENCIL_LEGACY) ||
      (ob_gp->data == nullptr))
  {
    return;
  }

  Curve *cu = (Curve *)ob_cu->data;
  bGPdata *gpd = (bGPdata *)ob_gp->data;
  bGPDlayer *gpl = nullptr;

  /* If the curve is empty, cancel. */
  if (cu->nurb.first == nullptr) {
    return;
  }

  /* Check if there is an active layer. */
  if (use_collections) {
    Collection *collection = gpencil_get_parent_collection(scene, ob_cu);
    if (collection != nullptr) {
      gpl = BKE_gpencil_layer_named_get(gpd, collection->id.name + 2);
      if (gpl == nullptr) {
        gpl = BKE_gpencil_layer_addnew(gpd, collection->id.name + 2, true, false);
      }
    }
  }

  if (gpl == nullptr) {
    gpl = BKE_gpencil_layer_active_get(gpd);
    if (gpl == nullptr) {
      gpl = BKE_gpencil_layer_addnew(gpd, DATA_("GP_Layer"), true, false);
    }
  }

  /* Check if there is an active frame and add if needed. */
  bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, scene->r.cfra, GP_GETFRAME_ADD_COPY);

  /* Read all splines of the curve and create a stroke for each. */
  LISTBASE_FOREACH (Nurb *, nu, &cu->nurb) {
    gpencil_convert_spline(bmain, ob_gp, ob_cu, scale_thickness, sample, gpf, nu);
  }

  /* Merge any similar material. */
  int removed = 0;
  BKE_gpencil_merge_materials(ob_gp, 0.001f, 0.001f, 0.001f, &removed);

  /* Remove any unused slot. */
  int actcol = ob_gp->actcol;

  for (int slot = 1; slot <= ob_gp->totcol; slot++) {
    while (slot <= ob_gp->totcol && !BKE_object_material_slot_used(ob_gp, slot)) {
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

/* -------------------------------------------------------------------- */
/** \name Edit-Curve Kernel Functions
 * \{ */

static bGPDcurve *gpencil_stroke_editcurve_generate_edgecases(bGPDstroke *gps,
                                                              const float stroke_radius)
{
  BLI_assert(gps->totpoints < 3);

  if (gps->totpoints == 1) {
    bGPDcurve *editcurve = BKE_gpencil_stroke_editcurve_new(1);
    bGPDspoint *pt = &gps->points[0];
    bGPDcurve_point *cpt = &editcurve->curve_points[0];
    BezTriple *bezt = &cpt->bezt;

    /* Handles are twice as long as the radius of the point. */
    float offset = (pt->pressure * stroke_radius) * 2.0f;

    float tmp_vec[3];
    for (int j = 0; j < 3; j++) {
      copy_v3_v3(tmp_vec, &pt->x);
      /* Move handles along the x-axis away from the control point */
      tmp_vec[0] += float(j - 1) * offset;
      copy_v3_v3(bezt->vec[j], tmp_vec);
    }

    cpt->pressure = pt->pressure;
    cpt->strength = pt->strength;
    copy_v4_v4(cpt->vert_color, pt->vert_color);

    /* default handle type */
    bezt->h1 = HD_FREE;
    bezt->h2 = HD_FREE;

    cpt->point_index = 0;

    return editcurve;
  }
  if (gps->totpoints == 2) {
    bGPDcurve *editcurve = BKE_gpencil_stroke_editcurve_new(2);
    bGPDspoint *first_pt = &gps->points[0];
    bGPDspoint *last_pt = &gps->points[1];

    float length = len_v3v3(&first_pt->x, &last_pt->x);
    float offset = length / 3;
    float dir[3];
    sub_v3_v3v3(dir, &last_pt->x, &first_pt->x);

    for (int i = 0; i < 2; i++) {
      bGPDspoint *pt = &gps->points[i];
      bGPDcurve_point *cpt = &editcurve->curve_points[i];
      BezTriple *bezt = &cpt->bezt;

      float tmp_vec[3];
      for (int j = 0; j < 3; j++) {
        copy_v3_v3(tmp_vec, dir);
        normalize_v3_length(tmp_vec, float(j - 1) * offset);
        add_v3_v3v3(bezt->vec[j], &pt->x, tmp_vec);
      }

      cpt->pressure = pt->pressure;
      cpt->strength = pt->strength;
      copy_v4_v4(cpt->vert_color, pt->vert_color);

      /* default handle type */
      bezt->h1 = HD_VECT;
      bezt->h2 = HD_VECT;

      cpt->point_index = 0;
    }

    return editcurve;
  }

  return nullptr;
}

bGPDcurve *BKE_gpencil_stroke_editcurve_generate(bGPDstroke *gps,
                                                 const float error_threshold,
                                                 const float corner_angle,
                                                 const float stroke_radius)
{
  if (gps->totpoints < 3) {
    return gpencil_stroke_editcurve_generate_edgecases(gps, stroke_radius);
  }
#define POINT_DIM 9

  float *points = static_cast<float *>(
      MEM_callocN(sizeof(float) * gps->totpoints * POINT_DIM, __func__));
  float diag_length = len_v3v3(gps->boundbox_min, gps->boundbox_max);
  float tmp_vec[3];

  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    int row = i * POINT_DIM;

    /* normalize coordinate to 0..1 */
    sub_v3_v3v3(tmp_vec, &pt->x, gps->boundbox_min);
    mul_v3_v3fl(&points[row], tmp_vec, COORD_FITTING_INFLUENCE / diag_length);
    points[row + 3] = pt->pressure / diag_length;

    /* strength and color are already normalized */
    points[row + 4] = pt->strength / diag_length;
    mul_v4_v4fl(&points[row + 5], pt->vert_color, 1.0f / diag_length);
  }

  uint calc_flag = CURVE_FIT_CALC_HIGH_QUALIY;
  if (gps->totpoints > 2 && gps->flag & GP_STROKE_CYCLIC) {
    calc_flag |= CURVE_FIT_CALC_CYCLIC;
  }

  float *r_cubic_array = nullptr;
  uint r_cubic_array_len = 0;
  uint *r_cubic_orig_index = nullptr;
  uint *r_corners_index_array = nullptr;
  uint r_corners_index_len = 0;
  int r = curve_fit_cubic_to_points_refit_fl(points,
                                             gps->totpoints,
                                             POINT_DIM,
                                             error_threshold,
                                             calc_flag,
                                             nullptr,
                                             0,
                                             corner_angle,
                                             &r_cubic_array,
                                             &r_cubic_array_len,
                                             &r_cubic_orig_index,
                                             &r_corners_index_array,
                                             &r_corners_index_len);

  if (r != 0 || r_cubic_array_len < 1) {
    return nullptr;
  }

  uint curve_point_size = 3 * POINT_DIM;

  bGPDcurve *editcurve = BKE_gpencil_stroke_editcurve_new(r_cubic_array_len);

  for (int i = 0; i < r_cubic_array_len; i++) {
    bGPDcurve_point *cpt = &editcurve->curve_points[i];
    BezTriple *bezt = &cpt->bezt;
    float *curve_point = &r_cubic_array[i * curve_point_size];

    for (int j = 0; j < 3; j++) {
      float *bez = &curve_point[j * POINT_DIM];
      madd_v3_v3v3fl(bezt->vec[j], gps->boundbox_min, bez, diag_length / COORD_FITTING_INFLUENCE);
    }

    float *ctrl_point = &curve_point[1 * POINT_DIM];
    cpt->pressure = ctrl_point[3] * diag_length;
    cpt->strength = ctrl_point[4] * diag_length;
    mul_v4_v4fl(cpt->vert_color, &ctrl_point[5], diag_length);

    /* default handle type */
    bezt->h1 = HD_ALIGN;
    bezt->h2 = HD_ALIGN;

    cpt->point_index = r_cubic_orig_index[i];
  }

  if (r_corners_index_len > 0 && r_corners_index_array != nullptr) {
    int start = 0, end = r_corners_index_len;
    if ((r_corners_index_len > 1) && (calc_flag & CURVE_FIT_CALC_CYCLIC) == 0) {
      start = 1;
      end = r_corners_index_len - 1;
    }
    for (int i = start; i < end; i++) {
      bGPDcurve_point *cpt = &editcurve->curve_points[r_corners_index_array[i]];
      BezTriple *bezt = &cpt->bezt;
      bezt->h1 = HD_FREE;
      bezt->h2 = HD_FREE;
    }
  }

  MEM_freeN(points);
  if (r_cubic_array) {
    free(r_cubic_array);
  }
  if (r_corners_index_array) {
    free(r_corners_index_array);
  }
  if (r_cubic_orig_index) {
    free(r_cubic_orig_index);
  }

#undef POINT_DIM
  return editcurve;
}

void BKE_gpencil_stroke_editcurve_update(bGPdata *gpd, bGPDlayer *gpl, bGPDstroke *gps)
{
  if (gps == nullptr || gps->totpoints < 0) {
    return;
  }

  if (gps->editcurve != nullptr) {
    BKE_gpencil_free_stroke_editcurve(gps);
  }

  float defaultpixsize = 1000.0f / gpd->pixfactor;
  float stroke_radius = ((gps->thickness + gpl->line_change) / defaultpixsize) / 2.0f;

  bGPDcurve *editcurve = BKE_gpencil_stroke_editcurve_generate(
      gps, gpd->curve_edit_threshold, gpd->curve_edit_corner_angle, stroke_radius);
  if (editcurve == nullptr) {
    return;
  }

  gps->editcurve = editcurve;
}

void BKE_gpencil_editcurve_stroke_sync_selection(bGPdata * /*gpd*/,
                                                 bGPDstroke *gps,
                                                 bGPDcurve *gpc)
{
  if (gps->flag & GP_STROKE_SELECT) {
    gpc->flag |= GP_CURVE_SELECT;

    for (int i = 0; i < gpc->tot_curve_points; i++) {
      bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
      bGPDspoint *pt = &gps->points[gpc_pt->point_index];
      if (pt->flag & GP_SPOINT_SELECT) {
        gpc_pt->flag |= GP_CURVE_POINT_SELECT;
        BEZT_SEL_ALL(&gpc_pt->bezt);
      }
      else {
        gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
        BEZT_DESEL_ALL(&gpc_pt->bezt);
      }
    }
  }
  else {
    gpc->flag &= ~GP_CURVE_SELECT;
    gpencil_editstroke_deselect_all(gpc);
  }
}

void BKE_gpencil_stroke_editcurve_sync_selection(bGPdata *gpd, bGPDstroke *gps, bGPDcurve *gpc)
{
  if (gpc->flag & GP_CURVE_SELECT) {
    gps->flag |= GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_set(gpd, gps);

    for (int i = 0; i < gpc->tot_curve_points - 1; i++) {
      bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
      bGPDspoint *pt = &gps->points[gpc_pt->point_index];
      bGPDcurve_point *gpc_pt_next = &gpc->curve_points[i + 1];

      if (gpc_pt->flag & GP_CURVE_POINT_SELECT) {
        pt->flag |= GP_SPOINT_SELECT;
        if (gpc_pt_next->flag & GP_CURVE_POINT_SELECT) {
          /* select all the points after */
          for (int j = gpc_pt->point_index + 1; j < gpc_pt_next->point_index; j++) {
            bGPDspoint *pt_next = &gps->points[j];
            pt_next->flag |= GP_SPOINT_SELECT;
          }
        }
      }
      else {
        pt->flag &= ~GP_SPOINT_SELECT;
        /* deselect all points after */
        for (int j = gpc_pt->point_index + 1; j < gpc_pt_next->point_index; j++) {
          bGPDspoint *pt_next = &gps->points[j];
          pt_next->flag &= ~GP_SPOINT_SELECT;
        }
      }
    }

    bGPDcurve_point *gpc_first = &gpc->curve_points[0];
    bGPDcurve_point *gpc_last = &gpc->curve_points[gpc->tot_curve_points - 1];
    bGPDspoint *last_pt = &gps->points[gpc_last->point_index];
    if (gpc_last->flag & GP_CURVE_POINT_SELECT) {
      last_pt->flag |= GP_SPOINT_SELECT;
    }
    else {
      last_pt->flag &= ~GP_SPOINT_SELECT;
    }

    if (gps->flag & GP_STROKE_CYCLIC) {
      if (gpc_first->flag & GP_CURVE_POINT_SELECT && gpc_last->flag & GP_CURVE_POINT_SELECT) {
        for (int i = gpc_last->point_index + 1; i < gps->totpoints; i++) {
          bGPDspoint *pt_next = &gps->points[i];
          pt_next->flag |= GP_SPOINT_SELECT;
        }
      }
      else {
        for (int i = gpc_last->point_index + 1; i < gps->totpoints; i++) {
          bGPDspoint *pt_next = &gps->points[i];
          pt_next->flag &= ~GP_SPOINT_SELECT;
        }
      }
    }
  }
  else {
    gps->flag &= ~GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_reset(gps);
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      pt->flag &= ~GP_SPOINT_SELECT;
    }
  }
}

static void gpencil_interpolate_fl_from_to(
    float from, float to, float *point_offset, int it, int stride)
{
  /* smooth interpolation */
  float *r = point_offset;
  for (int i = 0; i <= it; i++) {
    float fac = float(i) / float(it);
    fac = 3.0f * fac * fac - 2.0f * fac * fac * fac; /* Smooth. */
    *r = interpf(to, from, fac);
    r = static_cast<float *>(POINTER_OFFSET(r, stride));
  }
}

static void gpencil_interpolate_v4_from_to(
    float from[4], float to[4], float *point_offset, int it, int stride)
{
  /* smooth interpolation */
  float *r = point_offset;
  for (int i = 0; i <= it; i++) {
    float fac = float(i) / float(it);
    fac = 3.0f * fac * fac - 2.0f * fac * fac * fac; /* Smooth. */
    interp_v4_v4v4(r, from, to, fac);
    r = static_cast<float *>(POINTER_OFFSET(r, stride));
  }
}

static float gpencil_approximate_curve_segment_arclength(bGPDcurve_point *cpt_start,
                                                         bGPDcurve_point *cpt_end)
{
  BezTriple *bezt_start = &cpt_start->bezt;
  BezTriple *bezt_end = &cpt_end->bezt;

  float chord_len = len_v3v3(bezt_start->vec[1], bezt_end->vec[1]);
  float net_len = len_v3v3(bezt_start->vec[1], bezt_start->vec[2]);
  net_len += len_v3v3(bezt_start->vec[2], bezt_end->vec[0]);
  net_len += len_v3v3(bezt_end->vec[0], bezt_end->vec[1]);

  return (chord_len + net_len) / 2.0f;
}

static void gpencil_calculate_stroke_points_curve_segment(
    bGPDcurve_point *cpt, bGPDcurve_point *cpt_next, float *points_offset, int resolu, int stride)
{
  /* sample points on all 3 axis between two curve points */
  for (uint axis = 0; axis < 3; axis++) {
    BKE_curve_forward_diff_bezier(
        cpt->bezt.vec[1][axis],
        cpt->bezt.vec[2][axis],
        cpt_next->bezt.vec[0][axis],
        cpt_next->bezt.vec[1][axis],
        static_cast<float *>(POINTER_OFFSET(points_offset, sizeof(float) * axis)),
        int(resolu),
        stride);
  }

  /* interpolate other attributes */
  gpencil_interpolate_fl_from_to(
      cpt->pressure,
      cpt_next->pressure,
      static_cast<float *>(POINTER_OFFSET(points_offset, sizeof(float) * 3)),
      resolu,
      stride);
  gpencil_interpolate_fl_from_to(
      cpt->strength,
      cpt_next->strength,
      static_cast<float *>(POINTER_OFFSET(points_offset, sizeof(float) * 4)),
      resolu,
      stride);
  gpencil_interpolate_v4_from_to(
      cpt->vert_color,
      cpt_next->vert_color,
      static_cast<float *>(POINTER_OFFSET(points_offset, sizeof(float) * 5)),
      resolu,
      stride);
}

static float *gpencil_stroke_points_from_editcurve_adaptive_resolu(
    bGPDcurve_point *curve_point_array,
    int curve_point_array_len,
    int resolution,
    bool is_cyclic,
    int *r_points_len)
{
  /* One stride contains: `x, y, z, pressure, strength, Vr, Vg, Vb, Vmix_factor`. */
  const uint stride = sizeof(float[9]);
  const uint cpt_last = curve_point_array_len - 1;
  const uint num_segments = (is_cyclic) ? curve_point_array_len : curve_point_array_len - 1;
  int *segment_point_lengths = static_cast<int *>(
      MEM_callocN(sizeof(int) * num_segments, __func__));

  uint points_len = 1;
  for (int i = 0; i < cpt_last; i++) {
    bGPDcurve_point *cpt = &curve_point_array[i];
    bGPDcurve_point *cpt_next = &curve_point_array[i + 1];
    float arclen = gpencil_approximate_curve_segment_arclength(cpt, cpt_next);
    int segment_resolu = int(floorf(arclen * resolution));
    CLAMP_MIN(segment_resolu, 1);

    segment_point_lengths[i] = segment_resolu;
    points_len += segment_resolu;
  }

  if (is_cyclic) {
    bGPDcurve_point *cpt = &curve_point_array[cpt_last];
    bGPDcurve_point *cpt_next = &curve_point_array[0];
    float arclen = gpencil_approximate_curve_segment_arclength(cpt, cpt_next);
    int segment_resolu = int(floorf(arclen * resolution));
    CLAMP_MIN(segment_resolu, 1);

    segment_point_lengths[cpt_last] = segment_resolu;
    points_len += segment_resolu;
  }

  float(*r_points)[9] = static_cast<float(*)[9]>(
      MEM_callocN((stride * points_len * (is_cyclic ? 2 : 1)), __func__));
  float *points_offset = &r_points[0][0];
  int point_index = 0;
  for (int i = 0; i < cpt_last; i++) {
    bGPDcurve_point *cpt_curr = &curve_point_array[i];
    bGPDcurve_point *cpt_next = &curve_point_array[i + 1];
    int segment_resolu = segment_point_lengths[i];
    gpencil_calculate_stroke_points_curve_segment(
        cpt_curr, cpt_next, points_offset, segment_resolu, stride);
    /* update the index */
    cpt_curr->point_index = point_index;
    point_index += segment_resolu;
    points_offset = static_cast<float *>(POINTER_OFFSET(points_offset, segment_resolu * stride));
  }

  bGPDcurve_point *cpt_curr = &curve_point_array[cpt_last];
  cpt_curr->point_index = point_index;
  if (is_cyclic) {
    bGPDcurve_point *cpt_next = &curve_point_array[0];
    int segment_resolu = segment_point_lengths[cpt_last];
    gpencil_calculate_stroke_points_curve_segment(
        cpt_curr, cpt_next, points_offset, segment_resolu, stride);
  }

  MEM_freeN(segment_point_lengths);

  *r_points_len = points_len;
  return (float *)r_points;
}

/**
 * Helper: calculate the points on a curve with a fixed resolution.
 */
static float *gpencil_stroke_points_from_editcurve_fixed_resolu(bGPDcurve_point *curve_point_array,
                                                                int curve_point_array_len,
                                                                int resolution,
                                                                bool is_cyclic,
                                                                int *r_points_len)
{
  /* One stride contains: `x, y, z, pressure, strength, Vr, Vg, Vb, Vmix_factor`. */
  const uint stride = sizeof(float[9]);
  const uint array_last = curve_point_array_len - 1;
  const uint resolu_stride = resolution * stride;
  const uint points_len = BKE_curve_calc_coords_axis_len(
      curve_point_array_len, resolution, is_cyclic, false);

  float(*r_points)[9] = static_cast<float(*)[9]>(
      MEM_callocN((stride * points_len * (is_cyclic ? 2 : 1)), __func__));
  float *points_offset = &r_points[0][0];
  for (uint i = 0; i < array_last; i++) {
    bGPDcurve_point *cpt_curr = &curve_point_array[i];
    bGPDcurve_point *cpt_next = &curve_point_array[i + 1];

    gpencil_calculate_stroke_points_curve_segment(
        cpt_curr, cpt_next, points_offset, resolution, stride);
    /* update the index */
    cpt_curr->point_index = i * resolution;
    points_offset = static_cast<float *>(POINTER_OFFSET(points_offset, resolu_stride));
  }

  bGPDcurve_point *cpt_curr = &curve_point_array[array_last];
  cpt_curr->point_index = array_last * resolution;
  if (is_cyclic) {
    bGPDcurve_point *cpt_next = &curve_point_array[0];
    gpencil_calculate_stroke_points_curve_segment(
        cpt_curr, cpt_next, points_offset, resolution, stride);
  }

  *r_points_len = points_len;
  return (float *)r_points;
}

void BKE_gpencil_stroke_update_geometry_from_editcurve(bGPDstroke *gps,
                                                       const uint resolution,
                                                       const bool adaptive)
{
  if (gps == nullptr || gps->editcurve == nullptr) {
    return;
  }

  bGPDcurve *editcurve = gps->editcurve;
  bGPDcurve_point *curve_point_array = editcurve->curve_points;
  int curve_point_array_len = editcurve->tot_curve_points;
  if (curve_point_array_len == 0) {
    return;
  }
  /* Handle case for single curve point. */
  if (curve_point_array_len == 1) {
    bGPDcurve_point *cpt = &curve_point_array[0];
    /* resize stroke point array */
    gps->totpoints = 1;
    gps->points = static_cast<bGPDspoint *>(
        MEM_recallocN(gps->points, sizeof(bGPDspoint) * gps->totpoints));
    if (gps->dvert != nullptr) {
      gps->dvert = static_cast<MDeformVert *>(
          MEM_recallocN(gps->dvert, sizeof(MDeformVert) * gps->totpoints));
    }

    bGPDspoint *pt = &gps->points[0];
    copy_v3_v3(&pt->x, cpt->bezt.vec[1]);

    pt->pressure = cpt->pressure;
    pt->strength = cpt->strength;

    copy_v4_v4(pt->vert_color, cpt->vert_color);

    /* deselect */
    pt->flag &= ~GP_SPOINT_SELECT;
    gps->flag &= ~GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_reset(gps);

    return;
  }

  bool is_cyclic = gps->flag & GP_STROKE_CYCLIC;

  int points_len = 0;
  float(*points)[9] = nullptr;
  if (adaptive) {
    points = (float(*)[9])gpencil_stroke_points_from_editcurve_adaptive_resolu(
        curve_point_array, curve_point_array_len, resolution, is_cyclic, &points_len);
  }
  else {
    points = (float(*)[9])gpencil_stroke_points_from_editcurve_fixed_resolu(
        curve_point_array, curve_point_array_len, resolution, is_cyclic, &points_len);
  }

  if (points == nullptr || points_len == 0) {
    return;
  }

  /* resize stroke point array */
  gps->totpoints = points_len;
  gps->points = static_cast<bGPDspoint *>(
      MEM_recallocN(gps->points, sizeof(bGPDspoint) * gps->totpoints));
  if (gps->dvert != nullptr) {
    gps->dvert = static_cast<MDeformVert *>(
        MEM_recallocN(gps->dvert, sizeof(MDeformVert) * gps->totpoints));
  }

  /* write new data to stroke point array */
  for (int i = 0; i < points_len; i++) {
    bGPDspoint *pt = &gps->points[i];
    copy_v3_v3(&pt->x, &points[i][0]);

    pt->pressure = points[i][3];
    pt->strength = points[i][4];

    copy_v4_v4(pt->vert_color, &points[i][5]);

    /* deselect points */
    pt->flag &= ~GP_SPOINT_SELECT;
  }
  gps->flag &= ~GP_STROKE_SELECT;
  BKE_gpencil_stroke_select_index_reset(gps);

  /* free temp data */
  MEM_freeN(points);
}

void BKE_gpencil_editcurve_recalculate_handles(bGPDstroke *gps)
{
  if (gps == nullptr || gps->editcurve == nullptr) {
    return;
  }

  bool changed = false;
  bGPDcurve *gpc = gps->editcurve;
  if (gpc->tot_curve_points < 2) {
    return;
  }

  if (gpc->tot_curve_points == 1) {
    BKE_nurb_handle_calc(
        &(gpc->curve_points[0].bezt), nullptr, &(gpc->curve_points[0].bezt), false, 0);
    gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
  }

  for (int i = 1; i < gpc->tot_curve_points - 1; i++) {
    bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
    bGPDcurve_point *gpc_pt_prev = &gpc->curve_points[i - 1];
    bGPDcurve_point *gpc_pt_next = &gpc->curve_points[i + 1];
    /* update handle if point or neighbor is selected */
    if (gpc_pt->flag & GP_CURVE_POINT_SELECT || gpc_pt_prev->flag & GP_CURVE_POINT_SELECT ||
        gpc_pt_next->flag & GP_CURVE_POINT_SELECT)
    {
      BezTriple *bezt = &gpc_pt->bezt;
      BezTriple *bezt_prev = &gpc_pt_prev->bezt;
      BezTriple *bezt_next = &gpc_pt_next->bezt;

      BKE_nurb_handle_calc(bezt, bezt_prev, bezt_next, false, 0);
      changed = true;
    }
  }

  bGPDcurve_point *gpc_first = &gpc->curve_points[0];
  bGPDcurve_point *gpc_last = &gpc->curve_points[gpc->tot_curve_points - 1];
  bGPDcurve_point *gpc_first_next = &gpc->curve_points[1];
  bGPDcurve_point *gpc_last_prev = &gpc->curve_points[gpc->tot_curve_points - 2];
  if (gps->flag & GP_STROKE_CYCLIC) {
    if (gpc_first->flag & GP_CURVE_POINT_SELECT || gpc_last->flag & GP_CURVE_POINT_SELECT) {
      BezTriple *bezt_first = &gpc_first->bezt;
      BezTriple *bezt_last = &gpc_last->bezt;
      BezTriple *bezt_first_next = &gpc_first_next->bezt;
      BezTriple *bezt_last_prev = &gpc_last_prev->bezt;

      BKE_nurb_handle_calc(bezt_first, bezt_last, bezt_first_next, false, 0);
      BKE_nurb_handle_calc(bezt_last, bezt_last_prev, bezt_first, false, 0);
      changed = true;
    }
  }
  else {
    if (gpc_first->flag & GP_CURVE_POINT_SELECT || gpc_last->flag & GP_CURVE_POINT_SELECT) {
      BezTriple *bezt_first = &gpc_first->bezt;
      BezTriple *bezt_last = &gpc_last->bezt;
      BezTriple *bezt_first_next = &gpc_first_next->bezt;
      BezTriple *bezt_last_prev = &gpc_last_prev->bezt;

      BKE_nurb_handle_calc(bezt_first, nullptr, bezt_first_next, false, 0);
      BKE_nurb_handle_calc(bezt_last, bezt_last_prev, nullptr, false, 0);
      changed = true;
    }
  }

  if (changed) {
    gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
  }
}

/* Helper: count how many new curve points must be generated. */
static int gpencil_editcurve_subdivide_count(bGPDcurve *gpc, bool is_cyclic)
{
  int count = 0;
  for (int i = 0; i < gpc->tot_curve_points - 1; i++) {
    bGPDcurve_point *cpt = &gpc->curve_points[i];
    bGPDcurve_point *cpt_next = &gpc->curve_points[i + 1];

    if (cpt->flag & GP_CURVE_POINT_SELECT && cpt_next->flag & GP_CURVE_POINT_SELECT) {
      count++;
    }
  }

  if (is_cyclic) {
    bGPDcurve_point *cpt = &gpc->curve_points[0];
    bGPDcurve_point *cpt_next = &gpc->curve_points[gpc->tot_curve_points - 1];

    if (cpt->flag & GP_CURVE_POINT_SELECT && cpt_next->flag & GP_CURVE_POINT_SELECT) {
      count++;
    }
  }

  return count;
}

static void gpencil_editcurve_subdivide_curve_segment(bGPDcurve_point *cpt_start,
                                                      bGPDcurve_point *cpt_end,
                                                      bGPDcurve_point *cpt_new)
{
  BezTriple *bezt_start = &cpt_start->bezt;
  BezTriple *bezt_end = &cpt_end->bezt;
  BezTriple *bezt_new = &cpt_new->bezt;
  for (int axis = 0; axis < 3; axis++) {
    float p0, p1, p2, p3, m0, m1, q0, q1, b;
    p0 = bezt_start->vec[1][axis];
    p1 = bezt_start->vec[2][axis];
    p2 = bezt_end->vec[0][axis];
    p3 = bezt_end->vec[1][axis];

    m0 = (p0 + p1) / 2;
    q0 = (p0 + 2 * p1 + p2) / 4;
    b = (p0 + 3 * p1 + 3 * p2 + p3) / 8;
    q1 = (p1 + 2 * p2 + p3) / 4;
    m1 = (p2 + p3) / 2;

    bezt_new->vec[0][axis] = q0;
    bezt_new->vec[2][axis] = q1;
    bezt_new->vec[1][axis] = b;

    bezt_start->vec[2][axis] = m0;
    bezt_end->vec[0][axis] = m1;
  }

  cpt_new->pressure = interpf(cpt_end->pressure, cpt_start->pressure, 0.5f);
  cpt_new->strength = interpf(cpt_end->strength, cpt_start->strength, 0.5f);
  interp_v4_v4v4(cpt_new->vert_color, cpt_start->vert_color, cpt_end->vert_color, 0.5f);
}

void BKE_gpencil_editcurve_subdivide(bGPDstroke *gps, const int cuts)
{
  bGPDcurve *gpc = gps->editcurve;
  if (gpc == nullptr || gpc->tot_curve_points < 2) {
    return;
  }
  bool is_cyclic = gps->flag & GP_STROKE_CYCLIC;

  /* repeat for number of cuts */
  for (int s = 0; s < cuts; s++) {
    int old_tot_curve_points = gpc->tot_curve_points;
    int new_num_curve_points = gpencil_editcurve_subdivide_count(gpc, is_cyclic);
    if (new_num_curve_points == 0) {
      break;
    }
    int new_tot_curve_points = old_tot_curve_points + new_num_curve_points;

    bGPDcurve_point *temp_curve_points = (bGPDcurve_point *)MEM_callocN(
        sizeof(bGPDcurve_point) * new_tot_curve_points, __func__);

    bool prev_subdivided = false;
    int j = 0;
    for (int i = 0; i < old_tot_curve_points - 1; i++, j++) {
      bGPDcurve_point *cpt = &gpc->curve_points[i];
      bGPDcurve_point *cpt_next = &gpc->curve_points[i + 1];

      if (cpt->flag & GP_CURVE_POINT_SELECT && cpt_next->flag & GP_CURVE_POINT_SELECT) {
        bGPDcurve_point *cpt_new = &temp_curve_points[j + 1];
        gpencil_editcurve_subdivide_curve_segment(cpt, cpt_next, cpt_new);

        memcpy(&temp_curve_points[j], cpt, sizeof(bGPDcurve_point));
        memcpy(&temp_curve_points[j + 2], cpt_next, sizeof(bGPDcurve_point));

        cpt_new->flag |= GP_CURVE_POINT_SELECT;
        cpt_new->bezt.h1 = HD_ALIGN;
        cpt_new->bezt.h2 = HD_ALIGN;
        BEZT_SEL_ALL(&cpt_new->bezt);

        prev_subdivided = true;
        j++;
      }
      else if (!prev_subdivided) {
        memcpy(&temp_curve_points[j], cpt, sizeof(bGPDcurve_point));
        prev_subdivided = false;
      }
      else {
        prev_subdivided = false;
      }
    }

    if (is_cyclic) {
      bGPDcurve_point *cpt = &gpc->curve_points[old_tot_curve_points - 1];
      bGPDcurve_point *cpt_next = &gpc->curve_points[0];

      if (cpt->flag & GP_CURVE_POINT_SELECT && cpt_next->flag & GP_CURVE_POINT_SELECT) {
        bGPDcurve_point *cpt_new = &temp_curve_points[j + 1];
        gpencil_editcurve_subdivide_curve_segment(cpt, cpt_next, cpt_new);

        memcpy(&temp_curve_points[j], cpt, sizeof(bGPDcurve_point));
        memcpy(&temp_curve_points[0], cpt_next, sizeof(bGPDcurve_point));

        cpt_new->flag |= GP_CURVE_POINT_SELECT;
        cpt_new->bezt.h1 = HD_ALIGN;
        cpt_new->bezt.h2 = HD_ALIGN;
        BEZT_SEL_ALL(&cpt_new->bezt);
      }
      else if (!prev_subdivided) {
        memcpy(&temp_curve_points[j], cpt, sizeof(bGPDcurve_point));
      }
    }
    else {
      bGPDcurve_point *cpt = &gpc->curve_points[old_tot_curve_points - 1];
      memcpy(&temp_curve_points[j], cpt, sizeof(bGPDcurve_point));
    }

    MEM_freeN(gpc->curve_points);
    gpc->curve_points = temp_curve_points;
    gpc->tot_curve_points = new_tot_curve_points;
  }
}

void BKE_gpencil_strokes_selected_update_editcurve(bGPdata *gpd)
{
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  /* For all selected strokes, update edit curve. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl)) {
      continue;
    }
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);
    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && is_multiedit)) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip deselected stroke */
          if (!(gps->flag & GP_STROKE_SELECT)) {
            continue;
          }

          /* Generate the curve if there is none or the stroke was changed */
          if (gps->editcurve == nullptr) {
            BKE_gpencil_stroke_editcurve_update(gpd, gpl, gps);
            /* Continue if curve could not be generated. */
            if (gps->editcurve == nullptr) {
              continue;
            }
          }
          else if (gps->editcurve->flag & GP_CURVE_NEEDS_STROKE_UPDATE) {
            BKE_gpencil_stroke_editcurve_update(gpd, gpl, gps);
          }
          /* Update the selection from the stroke to the curve. */
          BKE_gpencil_editcurve_stroke_sync_selection(gpd, gps, gps->editcurve);

          gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
          BKE_gpencil_stroke_geometry_update(gpd, gps);
        }
      }
    }
  }
}

void BKE_gpencil_strokes_selected_sync_selection_editcurve(bGPdata *gpd)
{
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  /* Sync selection for all strokes with editcurve. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl)) {
      continue;
    }
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);
    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && is_multiedit)) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          bGPDcurve *gpc = gps->editcurve;
          if (gpc != nullptr) {
            /* Update the selection of every stroke that has an editcurve */
            BKE_gpencil_stroke_editcurve_sync_selection(gpd, gps, gpc);
          }
        }
      }
    }
  }
}

/** \} */
