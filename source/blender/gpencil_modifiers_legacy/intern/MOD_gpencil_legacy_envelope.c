/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

#include "MEM_guardedalloc.h"

static void initData(GpencilModifierData *md)
{
  EnvelopeGpencilModifierData *gpmd = (EnvelopeGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(EnvelopeGpencilModifierData), modifier);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

static float calc_min_radius_v3v3(float p1[3], float p2[3], float dir[3])
{
  /* Use plane-conic-intersections to choose the maximal radius.
   * The conic is defined in 4D as f({x,y,z,t}) = x*x + y*y + z*z - t*t = 0
   * Then a plane is defined parametrically as
   * {p}(u, v) = {p1,0}*u + {p2,0}*(1-u) + {dir,1}*v with 0 <= u <= 1 and v >= 0
   * Now compute the intersection point with the smallest t.
   * To do so, compute the parameters u, v such that f(p(u, v)) = 0 and v is minimal.
   * This can be done analytically and the solution is:
   * u = -dot(p2,dir) / dot(p1-p2, dir) +/- sqrt((dot(p2,dir) / dot(p1-p2, dir))^2 -
   * (2*dot(p1-p2,p2)*dot(p2,dir)-dot(p2,p2)*dot(p1-p2,dir))/(dot(p1-p2,dir)*dot(p1-p2,p1-p2)));
   * v = ({p1}u + {p2}*(1-u))^2 / (2*(dot(p1,dir)*u + dot(p2,dir)*(1-u)));
   */
  float diff[3];
  float p1_dir = dot_v3v3(p1, dir);
  float p2_dir = dot_v3v3(p2, dir);
  float p2_sqr = len_squared_v3(p2);
  float diff_dir = p1_dir - p2_dir;
  float u = 0.5f;
  if (diff_dir != 0.0f) {
    float p = p2_dir / diff_dir;
    sub_v3_v3v3(diff, p1, p2);
    float diff_sqr = len_squared_v3(diff);
    float diff_p2 = dot_v3v3(diff, p2);
    float q = (2 * diff_p2 * p2_dir - p2_sqr * diff_dir) / (diff_dir * diff_sqr);
    if (p * p - q >= 0) {
      u = -p - sqrtf(p * p - q) * copysign(1.0f, p);
      CLAMP(u, 0.0f, 1.0f);
    }
    else {
      u = 0.5f - copysign(0.5f, p);
    }
  }
  else {
    float p1_sqr = len_squared_v3(p1);
    u = p1_sqr < p2_sqr ? 1.0f : 0.0f;
  }
  float p[3];
  interp_v3_v3v3(p, p2, p1, u);
  /* v is the determined minimal radius. In case p1 and p2 are the same, there is a
   * simple proof for the following formula using the geometric mean theorem and Thales theorem. */
  float v = len_squared_v3(p) / (2 * interpf(p1_dir, p2_dir, u));
  if (v < 0 || !isfinite(v)) {
    /* No limit to the radius from this segment. */
    return 1e16f;
  }
  return v;
}

static float calc_radius_limit(
    bGPDstroke *gps, bGPDspoint *points, float dir[3], int spread, const int i)
{
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;
  bGPDspoint *pt = &points[i];

  /* NOTE this part is the second performance critical part. Improvements are welcome. */
  float radius_limit = 1e16f;
  float p1[3], p2[3];
  if (is_cyclic) {
    if (gps->totpoints / 2 < spread) {
      spread = gps->totpoints / 2;
    }
    const int start = i + gps->totpoints;
    for (int j = -spread; j <= spread; j++) {
      j += (j == 0);
      const int i1 = (start + j) % gps->totpoints;
      const int i2 = (start + j + (j > 0) - (j < 0)) % gps->totpoints;
      sub_v3_v3v3(p1, &points[i1].x, &pt->x);
      sub_v3_v3v3(p2, &points[i2].x, &pt->x);
      float r = calc_min_radius_v3v3(p1, p2, dir);
      radius_limit = min_ff(radius_limit, r);
    }
  }
  else {
    const int start = max_ii(-spread, 1 - i);
    const int end = min_ii(spread, gps->totpoints - 2 - i);
    for (int j = start; j <= end; j++) {
      if (j == 0) {
        continue;
      }
      const int i1 = i + j;
      const int i2 = i + j + (j > 0) - (j < 0);
      sub_v3_v3v3(p1, &points[i1].x, &pt->x);
      sub_v3_v3v3(p2, &points[i2].x, &pt->x);
      float r = calc_min_radius_v3v3(p1, p2, dir);
      radius_limit = min_ff(radius_limit, r);
    }
  }
  return radius_limit;
}

static void apply_stroke_envelope(bGPDstroke *gps,
                                  int spread,
                                  const int def_nr,
                                  const bool invert_vg,
                                  const float thickness,
                                  const float pixfactor)
{
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;
  if (is_cyclic) {
    const int half = gps->totpoints / 2;
    spread = abs(((spread + half) % gps->totpoints) - half);
  }
  else {
    spread = min_ii(spread, gps->totpoints - 1);
  }

  const int spread_left = (spread + 2) / 2;
  const int spread_right = (spread + 1) / 2;

  /* Copy the point data. Only need positions, but extracting them
   * is probably just as expensive as a full copy. */
  bGPDspoint *old_points = (bGPDspoint *)MEM_dupallocN(gps->points);

  /* Deform the stroke to match the envelope shape. */
  for (int i = 0; i < gps->totpoints; i++) {
    MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;

    /* Verify in vertex group. */
    float weight = get_modifier_point_weight(dvert, invert_vg, def_nr);
    if (weight < 0.0f) {
      continue;
    }

    int index1 = i - spread_left;
    int index2 = i + spread_right;
    CLAMP(index1, 0, gps->totpoints - 1);
    CLAMP(index2, 0, gps->totpoints - 1);

    bGPDspoint *point = &gps->points[i];
    point->pressure *= interpf(thickness, 1.0f, weight);

    float closest[3];
    float closest2[3];
    copy_v3_v3(closest2, &point->x);
    float dist = 0.0f;
    float dist2 = 0.0f;
    /* Create plane from point and neighbors and intersect that with the line. */
    float v1[3], v2[3], plane_no[3];
    sub_v3_v3v3(
        v1,
        &old_points[is_cyclic ? (i - 1 + gps->totpoints) % gps->totpoints : max_ii(0, i - 1)].x,
        &old_points[i].x);
    sub_v3_v3v3(
        v2,
        &old_points[is_cyclic ? (i + 1) % gps->totpoints : min_ii(gps->totpoints - 1, i + 1)].x,
        &old_points[i].x);
    normalize_v3(v1);
    normalize_v3(v2);
    sub_v3_v3v3(plane_no, v1, v2);
    if (normalize_v3(plane_no) == 0.0f) {
      continue;
    }
    /* Now find the intersections with the plane. */
    /* NOTE this part is the first performance critical part. Improvements are welcome. */
    float tmp_closest[3];
    for (int j = -spread_right; j <= spread_left; j++) {
      const int i1 = is_cyclic ? (i + j - spread_left + gps->totpoints) % gps->totpoints :
                                 max_ii(0, i + j - spread_left);
      const int i2 = is_cyclic ? (i + j + spread_right) % gps->totpoints :
                                 min_ii(gps->totpoints - 1, i + j + spread_right);
#if 0
      bool side = dot_v3v3(&old_points[i1].x, plane_no) < dot_v3v3(plane_no, &old_points[i2].x);
      if (side) {
        continue;
      }
#endif
      float lambda = line_plane_factor_v3(
          &point->x, plane_no, &old_points[i1].x, &old_points[i2].x);
      if (lambda <= 0.0f || lambda >= 1.0f) {
        continue;
      }
      interp_v3_v3v3(tmp_closest, &old_points[i1].x, &old_points[i2].x, lambda);

      float dir[3];
      sub_v3_v3v3(dir, tmp_closest, &point->x);
      float d = len_v3(dir);
      /* Use a formula to find the diameter of the circle that would touch the line. */
      float cos_angle = fabsf(dot_v3v3(plane_no, &old_points[i1].x) -
                              dot_v3v3(plane_no, &old_points[i2].x)) /
                        len_v3v3(&old_points[i1].x, &old_points[i2].x);
      d *= 2 * cos_angle / (1 + cos_angle);
      float to_closest[3];
      sub_v3_v3v3(to_closest, closest, &point->x);
      if (dist == 0.0f) {
        dist = d;
        copy_v3_v3(closest, tmp_closest);
      }
      else if (dot_v3v3(to_closest, dir) >= 0) {
        if (d > dist) {
          dist = d;
          copy_v3_v3(closest, tmp_closest);
        }
      }
      else {
        if (d > dist2) {
          dist2 = d;
          copy_v3_v3(closest2, tmp_closest);
        }
      }
    }
    if (dist == 0.0f) {
      copy_v3_v3(closest, &point->x);
    }
    if (dist2 == 0.0f) {
      copy_v3_v3(closest2, &point->x);
    }
    dist = dist + dist2;

    if (dist < FLT_EPSILON) {
      continue;
    }

    float use_dist = dist;

    /* Apply radius limiting to not cross existing lines. */
    float dir[3], new_center[3];
    interp_v3_v3v3(new_center, closest2, closest, 0.5f);
    sub_v3_v3v3(dir, new_center, &point->x);
    if (normalize_v3(dir) != 0.0f && (is_cyclic || (i > 0 && i < gps->totpoints - 1))) {
      const float max_radius = calc_radius_limit(gps, old_points, dir, spread, i);
      use_dist = min_ff(use_dist, 2 * max_radius);
    }

    float fac = use_dist * weight;
    point->pressure += fac * pixfactor;
    interp_v3_v3v3(&point->x, &point->x, new_center, fac / len_v3v3(closest, closest2));
  }

  MEM_freeN(old_points);
}

/**
 * Apply envelope effect to the stroke.
 */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{
  EnvelopeGpencilModifierData *mmd = (EnvelopeGpencilModifierData *)md;
  if (mmd->mode != GP_ENVELOPE_DEFORM) {
    return;
  }
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      3,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_ENVELOPE_INVERT_LAYER,
                                      mmd->flag & GP_ENVELOPE_INVERT_PASS,
                                      mmd->flag & GP_ENVELOPE_INVERT_LAYERPASS,
                                      mmd->flag & GP_ENVELOPE_INVERT_MATERIAL))
  {
    return;
  }

  if (mmd->spread <= 0) {
    return;
  }

  bGPdata *gpd = (bGPdata *)ob->data;
  const float pixfactor = 1000.0f / ((gps->thickness + gpl->line_change) * gpd->pixfactor);
  apply_stroke_envelope(gps,
                        mmd->spread,
                        def_nr,
                        (mmd->flag & GP_ENVELOPE_INVERT_VGROUP) != 0,
                        mmd->thickness,
                        pixfactor);
}

static void add_stroke(Object *ob,
                       bGPDstroke *gps,
                       const int point_index,
                       const int connection_index,
                       const int size2,
                       const int size1,
                       const int mat_nr,
                       const float thickness,
                       const float strength,
                       ListBase *results)
{
  const int size = size1 + size2;
  bGPdata *gpd = ob->data;
  bGPDstroke *gps_dst = BKE_gpencil_stroke_new(mat_nr, size, gps->thickness);
  gps_dst->runtime.gps_orig = gps->runtime.gps_orig;

  memcpy(&gps_dst->points[0], &gps->points[connection_index], size1 * sizeof(bGPDspoint));
  memcpy(&gps_dst->points[size1], &gps->points[point_index], size2 * sizeof(bGPDspoint));

  for (int i = 0; i < size; i++) {
    gps_dst->points[i].pressure *= thickness;
    gps_dst->points[i].strength *= strength;
  }

  if (gps->dvert != NULL) {
    gps_dst->dvert = MEM_malloc_arrayN(size, sizeof(MDeformVert), __func__);
    BKE_defvert_array_copy(&gps_dst->dvert[0], &gps->dvert[connection_index], size1);
    BKE_defvert_array_copy(&gps_dst->dvert[size1], &gps->dvert[point_index], size2);
  }

  BLI_addtail(results, gps_dst);

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps_dst);
}

static void add_stroke_cyclic(Object *ob,
                              bGPDstroke *gps,
                              const int point_index,
                              const int connection_index,
                              const int size,
                              const int mat_nr,
                              const float thickness,
                              const float strength,
                              ListBase *results)
{
  bGPdata *gpd = ob->data;
  bGPDstroke *gps_dst = BKE_gpencil_stroke_new(mat_nr, size * 2, gps->thickness);
  gps_dst->runtime.gps_orig = gps->runtime.gps_orig;

  if (gps->dvert != NULL) {
    gps_dst->dvert = MEM_malloc_arrayN(size * 2, sizeof(MDeformVert), __func__);
  }

  for (int i = 0; i < size; i++) {
    int a = (connection_index + i) % gps->totpoints;
    int b = (point_index + i) % gps->totpoints;

    gps_dst->points[i] = gps->points[a];
    bGPDspoint *pt_dst = &gps_dst->points[i];
    bGPDspoint *pt_orig = &gps->points[a];
    pt_dst->runtime.pt_orig = pt_orig->runtime.pt_orig;
    pt_dst->runtime.idx_orig = pt_orig->runtime.idx_orig;

    gps_dst->points[size + i] = gps->points[b];
    pt_dst = &gps_dst->points[size + i];
    pt_orig = &gps->points[b];
    pt_dst->runtime.pt_orig = pt_orig->runtime.pt_orig;
    pt_dst->runtime.idx_orig = pt_orig->runtime.idx_orig;

    if (gps->dvert != NULL) {
      BKE_defvert_array_copy(&gps_dst->dvert[i], &gps->dvert[a], 1);
      BKE_defvert_array_copy(&gps_dst->dvert[size + i], &gps->dvert[b], 1);
    }
  }
  for (int i = 0; i < size * 2; i++) {
    gps_dst->points[i].pressure *= thickness;
    gps_dst->points[i].strength *= strength;
    memset(&gps_dst->points[i].runtime, 0, sizeof(bGPDspoint_Runtime));
  }

  BLI_addtail(results, gps_dst);

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps_dst);
}

static void add_stroke_simple(Object *ob,
                              bGPDstroke *gps,
                              const int point_index,
                              const int connection_index,
                              const int mat_nr,
                              const float thickness,
                              const float strength,
                              ListBase *results)
{
  bGPdata *gpd = ob->data;
  bGPDstroke *gps_dst = BKE_gpencil_stroke_new(mat_nr, 2, gps->thickness);
  gps_dst->runtime.gps_orig = gps->runtime.gps_orig;

  gps_dst->points[0] = gps->points[connection_index];
  gps_dst->points[0].pressure *= thickness;
  gps_dst->points[0].strength *= strength;
  bGPDspoint *pt_dst = &gps_dst->points[0];
  bGPDspoint *pt_orig = &gps->points[connection_index];
  pt_dst->runtime.pt_orig = pt_orig->runtime.pt_orig;
  pt_dst->runtime.idx_orig = pt_orig->runtime.idx_orig;

  gps_dst->points[1] = gps->points[point_index];
  gps_dst->points[1].pressure *= thickness;
  gps_dst->points[1].strength *= strength;
  pt_dst = &gps_dst->points[1];
  pt_orig = &gps->points[point_index];
  pt_dst->runtime.pt_orig = pt_orig->runtime.pt_orig;
  pt_dst->runtime.idx_orig = pt_orig->runtime.idx_orig;

  if (gps->dvert != NULL) {
    gps_dst->dvert = MEM_malloc_arrayN(2, sizeof(MDeformVert), __func__);
    BKE_defvert_array_copy(&gps_dst->dvert[0], &gps->dvert[connection_index], 1);
    BKE_defvert_array_copy(&gps_dst->dvert[1], &gps->dvert[point_index], 1);
  }

  BLI_addtail(results, gps_dst);

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps_dst);
}

static void generate_geometry(GpencilModifierData *md, Object *ob, bGPDlayer *gpl, bGPDframe *gpf)
{
  EnvelopeGpencilModifierData *mmd = (EnvelopeGpencilModifierData *)md;
  ListBase duplicates = {0};
  LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
    if (!is_stroke_affected_by_modifier(ob,
                                        mmd->layername,
                                        mmd->material,
                                        mmd->pass_index,
                                        mmd->layer_pass,
                                        3,
                                        gpl,
                                        gps,
                                        mmd->flag & GP_ENVELOPE_INVERT_LAYER,
                                        mmd->flag & GP_ENVELOPE_INVERT_PASS,
                                        mmd->flag & GP_ENVELOPE_INVERT_LAYERPASS,
                                        mmd->flag & GP_ENVELOPE_INVERT_MATERIAL))
    {
      continue;
    }

    const int mat_nr = mmd->mat_nr < 0 ? gps->mat_nr : min_ii(mmd->mat_nr, ob->totcol - 1);
    if (mmd->mode == GP_ENVELOPE_FILLS) {
      const int skip = min_ii(mmd->skip, min_ii(mmd->spread / 2, gps->totpoints - 2));
      if (gps->flag & GP_STROKE_CYCLIC) {
        for (int i = 0; i < gps->totpoints; i++) {
          const int connection_index = (i + mmd->spread - skip) % gps->totpoints;
          add_stroke_cyclic(ob,
                            gps,
                            i,
                            connection_index,
                            2 + skip,
                            mat_nr,
                            mmd->thickness,
                            mmd->strength,
                            &duplicates);
          i += mmd->skip;
        }
      }
      else {
        for (int i = -mmd->spread + skip; i < gps->totpoints - 1; i++) {
          const int point_index = max_ii(0, i);
          const int connection_index = min_ii(i + mmd->spread + 1, gps->totpoints - 1);
          const int size1 = min_ii(2 + skip,
                                   min_ii(point_index + 1, gps->totpoints - point_index));
          const int size2 = min_ii(
              2 + skip, min_ii(connection_index + 1, gps->totpoints - connection_index));
          add_stroke(ob,
                     gps,
                     point_index,
                     connection_index + 1 - size2,
                     size1,
                     size2,
                     mat_nr,
                     mmd->thickness,
                     mmd->strength,
                     &duplicates);
          i += mmd->skip;
        }
      }
      BLI_remlink(&gpf->strokes, gps);
      BKE_gpencil_free_stroke(gps);
    }
    else {
      BLI_assert(mmd->mode == GP_ENVELOPE_SEGMENTS);
      if (gps->flag & GP_STROKE_CYCLIC) {
        for (int i = 0; i < gps->totpoints; i++) {
          const int connection_index = (i + 1 + mmd->spread) % gps->totpoints;
          add_stroke_simple(
              ob, gps, i, connection_index, mat_nr, mmd->thickness, mmd->strength, &duplicates);
          i += mmd->skip;
        }
      }
      else {
        for (int i = -mmd->spread; i < gps->totpoints - 1; i++) {
          const int connection_index = min_ii(i + 1 + mmd->spread, gps->totpoints - 1);
          add_stroke_simple(ob,
                            gps,
                            max_ii(0, i),
                            connection_index,
                            mat_nr,
                            mmd->thickness,
                            mmd->strength,
                            &duplicates);
          i += mmd->skip;
        }
      }
    }
  }
  if (!BLI_listbase_is_empty(&duplicates)) {
    /* Add strokes to the start of the stroke list to ensure the new lines are drawn underneath the
     * original line. */
    BLI_movelisttolist_reverse(&gpf->strokes, &duplicates);
  }
}

/**
 * Apply envelope effect to the strokes.
 */
static void generateStrokes(GpencilModifierData *md, Depsgraph *depsgraph, Object *ob)
{
  EnvelopeGpencilModifierData *mmd = (EnvelopeGpencilModifierData *)md;
  if (mmd->mode == GP_ENVELOPE_DEFORM || mmd->spread <= 0) {
    return;
  }
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = (bGPdata *)ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
    if (gpf == NULL) {
      continue;
    }
    generate_geometry(md, ob, gpl, gpf);
  }
}

static void bakeModifier(struct Main *UNUSED(bmain),
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  EnvelopeGpencilModifierData *mmd = (EnvelopeGpencilModifierData *)md;
  if (mmd->mode == GP_ENVELOPE_DEFORM) {
    generic_bake_deform_stroke(depsgraph, md, ob, false, deformStroke);
  }
  else {
    bGPdata *gpd = ob->data;

    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        generate_geometry(md, ob, gpl, gpf);
      }
    }
  }
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  EnvelopeGpencilModifierData *mmd = (EnvelopeGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);

  uiItemR(layout, ptr, "spread", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "thickness", 0, NULL, ICON_NONE);

  const int mode = RNA_enum_get(ptr, "mode");
  if (mode != GP_ENVELOPE_DEFORM) {
    uiItemR(layout, ptr, "strength", 0, NULL, ICON_NONE);
    uiItemR(layout, ptr, "mat_nr", 0, NULL, ICON_NONE);
    uiItemR(layout, ptr, "skip", 0, NULL, ICON_NONE);
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, true);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Envelope, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Envelope = {
    /*name*/ N_("Envelope"),
    /*structName*/ "EnvelopeGpencilModifierData",
    /*structSize*/ sizeof(EnvelopeGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copyData*/ copyData,

    /*deformStroke*/ deformStroke,
    /*generateStrokes*/ generateStrokes,
    /*bakeModifier*/ bakeModifier,
    /*remapTime*/ NULL,

    /*initData*/ initData,
    /*freeData*/ NULL,
    /*isDisabled*/ NULL,
    /*updateDepsgraph*/ NULL,
    /*dependsOnTime*/ NULL,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ NULL,
    /*panelRegister*/ panelRegister,
};
