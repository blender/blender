/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup edsculpt
 *
 * Used for vertex color & weight paint and mode switching.
 *
 * \note This file is already big,
 * use `paint_vertex_color_ops.c` & `paint_vertex_weight_ops.c` for general purpose operators.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.h"
#include "BLI_color.hh"
#include "BLI_color_mix.hh"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_task.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"

/* For IMB_BlendMode only. */
#include "IMB_imbuf.h"

#include "BKE_ccg.h"
#include "bmesh.h"

#include "paint_intern.h" /* own include */
#include "sculpt_intern.h"

using blender::IndexRange;
using namespace blender;
using namespace blender::color;

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

static uint color2uint(ColorPaint4b c)
{
  return *(reinterpret_cast<uint *>(&c));
}

static bool isZero(ColorPaint4f c)
{
  return c.r == 0.0f && c.g == 0.0f && c.b == 0.0f && c.a == 0.0f;
}

static bool isZero(ColorPaint4b c)
{
  return c.r == 0 && c.g == 0 && c.b == 0 && c.a == 0;
}

template<typename Color = ColorPaint4f> static ColorPaint4f toFloat(const Color &c)
{
  if constexpr (std::is_same_v<Color, ColorPaint4b>) {
    return c.decode();
  }
  else {
    return c;
  }
}

template<typename Color = ColorPaint4f> static Color fromFloat(const ColorPaint4f &c)
{
  if constexpr (std::is_same_v<Color, ColorPaint4b>) {
    return c.encode();
  }
  else {
    return c;
  }
}

/* Use for 'blur' brush, align with PBVH nodes, created and freed on each update. */
template<typename BlendType> struct VPaintAverageAccum {
  BlendType len;
  BlendType value[3];
};

struct WPaintAverageAccum {
  uint len;
  double value;
};

struct NormalAnglePrecalc {
  bool do_mask_normal;
  /* what angle to mask at */
  float angle;
  /* cos(angle), faster to compare */
  float angle__cos;
  float angle_inner;
  float angle_inner__cos;
  /* difference between angle and angle_inner, for easy access */
  float angle_range;
};

/* Returns number of elements. */
static int get_vcol_elements(Mesh *me, size_t *r_elem_size)
{
  const std::optional<bke::AttributeMetaData> meta_data = me->attributes().lookup_meta_data(
      me->active_color_attribute);

  if (r_elem_size) {
    *r_elem_size = meta_data->data_type == CD_PROP_COLOR ? sizeof(float[4]) : 4ULL;
  }

  switch (meta_data->domain) {
    case ATTR_DOMAIN_POINT:
      return me->totvert;
    case ATTR_DOMAIN_CORNER:
      return me->totloop;
    default:
      return 0;
  }
}

static void view_angle_limits_init(NormalAnglePrecalc *a, float angle, bool do_mask_normal)
{
  angle = RAD2DEGF(angle);
  a->do_mask_normal = do_mask_normal;
  if (do_mask_normal) {
    a->angle_inner = angle;
    a->angle = (a->angle_inner + 90.0f) * 0.5f;
  }
  else {
    a->angle_inner = a->angle = angle;
  }

  a->angle_inner *= float(M_PI_2 / 90);
  a->angle *= float(M_PI_2 / 90);
  a->angle_range = a->angle - a->angle_inner;

  if (a->angle_range <= 0.0f) {
    a->do_mask_normal = false; /* no need to do blending */
  }

  a->angle__cos = cosf(a->angle);
  a->angle_inner__cos = cosf(a->angle_inner);
}

static float view_angle_limits_apply_falloff(const NormalAnglePrecalc *a,
                                             float angle_cos,
                                             float *mask_p)
{
  if (angle_cos <= a->angle__cos) {
    /* outsize the normal limit */
    return false;
  }
  if (angle_cos < a->angle_inner__cos) {
    *mask_p *= (a->angle - acosf(angle_cos)) / a->angle_range;
    return true;
  }
  return true;
}

static bool vwpaint_use_normal(const VPaint *vp)
{
  return ((vp->paint.brush->flag & BRUSH_FRONTFACE) != 0) ||
         ((vp->paint.brush->flag & BRUSH_FRONTFACE_FALLOFF) != 0);
}

static bool brush_use_accumulate_ex(const Brush *brush, const int ob_mode)
{
  return ((brush->flag & BRUSH_ACCUMULATE) != 0 ||
          (ob_mode == OB_MODE_VERTEX_PAINT ? (brush->vertexpaint_tool == VPAINT_TOOL_SMEAR) :
                                             (brush->weightpaint_tool == WPAINT_TOOL_SMEAR)));
}

static bool brush_use_accumulate(const VPaint *vp)
{
  return brush_use_accumulate_ex(vp->paint.brush, vp->paint.runtime.ob_mode);
}

static MDeformVert *defweight_prev_init(MDeformVert *dvert_prev,
                                        MDeformVert *dvert_curr,
                                        int index)
{
  const MDeformVert *dv_curr = &dvert_curr[index];
  MDeformVert *dv_prev = &dvert_prev[index];
  if (dv_prev->flag == 1) {
    dv_prev->flag = 0;
    BKE_defvert_copy(dv_prev, dv_curr);
  }
  return dv_prev;
}

static void paint_last_stroke_update(Scene *scene, const float location[3])
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  ups->average_stroke_counter++;
  add_v3_v3(ups->average_stroke_accum, location);
  ups->last_stroke_valid = true;
}

bool vertex_paint_mode_poll(bContext *C)
{
  const Object *ob = CTX_data_active_object(C);
  if (!ob) {
    return false;
  }
  const Mesh *mesh = static_cast<const Mesh *>(ob->data);

  if (!(ob->mode == OB_MODE_VERTEX_PAINT && mesh->totpoly)) {
    return false;
  }

  if (!mesh->attributes().contains(mesh->active_color_attribute)) {
    return false;
  }

  return true;
}

static bool vertex_paint_poll_ex(bContext *C, bool check_tool)
{
  if (vertex_paint_mode_poll(C) && BKE_paint_brush(&CTX_data_tool_settings(C)->vpaint->paint)) {
    ScrArea *area = CTX_wm_area(C);
    if (area && area->spacetype == SPACE_VIEW3D) {
      ARegion *region = CTX_wm_region(C);
      if (region->regiontype == RGN_TYPE_WINDOW) {
        if (!check_tool || WM_toolsystem_active_tool_is_brush(C)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool vertex_paint_poll(bContext *C)
{
  return vertex_paint_poll_ex(C, true);
}

bool vertex_paint_poll_ignore_tool(bContext *C)
{
  return vertex_paint_poll_ex(C, false);
}

bool weight_paint_mode_poll(bContext *C)
{
  const Object *ob = CTX_data_active_object(C);

  return ob && ob->mode == OB_MODE_WEIGHT_PAINT && ((const Mesh *)ob->data)->totpoly;
}

static bool weight_paint_poll_ex(bContext *C, bool check_tool)
{
  const Object *ob = CTX_data_active_object(C);
  const ScrArea *area;

  if ((ob != nullptr) && (ob->mode & OB_MODE_WEIGHT_PAINT) &&
      (BKE_paint_brush(&CTX_data_tool_settings(C)->wpaint->paint) != nullptr) &&
      (area = CTX_wm_area(C)) && (area->spacetype == SPACE_VIEW3D)) {
    ARegion *region = CTX_wm_region(C);
    if (ELEM(region->regiontype, RGN_TYPE_WINDOW, RGN_TYPE_HUD)) {
      if (!check_tool || WM_toolsystem_active_tool_is_brush(C)) {
        return true;
      }
    }
  }
  return false;
}

bool weight_paint_poll(bContext *C)
{
  return weight_paint_poll_ex(C, true);
}

bool weight_paint_poll_ignore_tool(bContext *C)
{
  return weight_paint_poll_ex(C, false);
}

template<typename Color, typename Traits, eAttrDomain domain>
static Color vpaint_get_current_col(Scene *scene, VPaint *vp, bool secondary)
{
  const Brush *brush = BKE_paint_brush_for_read(&vp->paint);
  float color[4];
  const float *brush_color = secondary ? BKE_brush_secondary_color_get(scene, brush) :
                                         BKE_brush_color_get(scene, brush);
  IMB_colormanagement_srgb_to_scene_linear_v3(color, brush_color);

  color[3] = 1.0f; /* alpha isn't used, could even be removed to speedup paint a little */

  return fromFloat<Color>(ColorPaint4f(color));
}

uint vpaint_get_current_color(Scene *scene, VPaint *vp, bool secondary)
{
  ColorPaint4b col = vpaint_get_current_col<ColorPaint4b, ByteTraits, ATTR_DOMAIN_CORNER>(
      scene, vp, secondary);

  return color2uint(col);
}

/* wpaint has 'wpaint_blend' */
template<typename Color, typename Traits>
static Color vpaint_blend(const VPaint *vp,
                          Color color_curr,
                          Color color_orig,
                          Color color_paint,
                          const typename Traits::ValueType alpha,
                          const typename Traits::BlendType brush_alpha_value)
{
  using Value = typename Traits::ValueType;

  const Brush *brush = vp->paint.brush;
  const IMB_BlendMode blend = (IMB_BlendMode)brush->blend;

  const Color color_blend = BLI_mix_colors<Color, Traits>(blend, color_curr, color_paint, alpha);

  /* If no accumulate, clip color adding with `color_orig` & `color_test`. */
  if (!brush_use_accumulate(vp)) {
    uint a;
    Color color_test;
    Value *cp, *ct, *co;

    color_test = BLI_mix_colors<Color, Traits>(blend, color_orig, color_paint, brush_alpha_value);

    cp = (Value *)&color_blend;
    ct = (Value *)&color_test;
    co = (Value *)&color_orig;

    for (a = 0; a < 4; a++) {
      if (ct[a] < co[a]) {
        if (cp[a] < ct[a]) {
          cp[a] = ct[a];
        }
        else if (cp[a] > co[a]) {
          cp[a] = co[a];
        }
      }
      else {
        if (cp[a] < co[a]) {
          cp[a] = co[a];
        }
        else if (cp[a] > ct[a]) {
          cp[a] = ct[a];
        }
      }
    }
  }

  if ((brush->flag & BRUSH_LOCK_ALPHA) &&
      !ELEM(blend, IMB_BLEND_ERASE_ALPHA, IMB_BLEND_ADD_ALPHA)) {
    Value *cp, *cc;
    cp = (Value *)&color_blend;
    cc = (Value *)&color_curr;
    cp[3] = cc[3];
  }

  return color_blend;
}

/* vpaint has 'vpaint_blend' */
static float wpaint_blend(const VPaint *wp,
                          float weight,
                          const float alpha,
                          float paintval,
                          const float /*brush_alpha_value*/,
                          const bool do_flip)
{
  const Brush *brush = wp->paint.brush;
  IMB_BlendMode blend = (IMB_BlendMode)brush->blend;

  if (do_flip) {
    switch (blend) {
      case IMB_BLEND_MIX:
        paintval = 1.0f - paintval;
        break;
      case IMB_BLEND_ADD:
        blend = IMB_BLEND_SUB;
        break;
      case IMB_BLEND_SUB:
        blend = IMB_BLEND_ADD;
        break;
      case IMB_BLEND_LIGHTEN:
        blend = IMB_BLEND_DARKEN;
        break;
      case IMB_BLEND_DARKEN:
        blend = IMB_BLEND_LIGHTEN;
        break;
      default:
        break;
    }
  }

  weight = ED_wpaint_blend_tool(blend, weight, paintval, alpha);

  CLAMP(weight, 0.0f, 1.0f);

  return weight;
}

static void paint_and_tex_color_alpha_intern(VPaint *vp,
                                             const ViewContext *vc,
                                             const float co[3],
                                             float r_rgba[4])
{
  const Brush *brush = BKE_paint_brush(&vp->paint);
  const MTex *mtex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);
  BLI_assert(mtex->tex != nullptr);
  if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
    BKE_brush_sample_tex_3d(vc->scene, brush, mtex, co, r_rgba, 0, nullptr);
  }
  else {
    float co_ss[2]; /* screenspace */
    if (ED_view3d_project_float_object(
            vc->region,
            co,
            co_ss,
            (eV3DProjTest)(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) == V3D_PROJ_RET_OK) {
      const float co_ss_3d[3] = {co_ss[0], co_ss[1], 0.0f}; /* we need a 3rd empty value */
      BKE_brush_sample_tex_3d(vc->scene, brush, mtex, co_ss_3d, r_rgba, 0, nullptr);
    }
    else {
      zero_v4(r_rgba);
    }
  }
}

static float wpaint_clamp_monotonic(float oldval, float curval, float newval)
{
  if (newval < oldval) {
    return MIN2(newval, curval);
  }
  if (newval > oldval) {
    return MAX2(newval, curval);
  }
  return newval;
}

static float wpaint_undo_lock_relative(
    float weight, float old_weight, float locked_weight, float free_weight, bool auto_normalize)
{
  /* In auto-normalize mode, or when there is no unlocked weight,
   * compute based on locked weight. */
  if (auto_normalize || free_weight <= 0.0f) {
    if (locked_weight < 1.0f - VERTEX_WEIGHT_LOCK_EPSILON) {
      weight *= (1.0f - locked_weight);
    }
    else {
      weight = 0;
    }
  }
  else {
    /* When dealing with full unlocked weight, don't paint, as it is always displayed as 1. */
    if (old_weight >= free_weight) {
      weight = old_weight;
    }
    /* Try to compute a weight value that would produce the desired effect if normalized. */
    else if (weight < 1.0f) {
      weight = weight * (free_weight - old_weight) / (1 - weight);
    }
    else {
      weight = 1.0f;
    }
  }

  return weight;
}

/* ----------------------------------------------------- */

static void do_weight_paint_normalize_all(MDeformVert *dvert,
                                          const int defbase_tot,
                                          const bool *vgroup_validmap)
{
  float sum = 0.0f, fac;
  uint i, tot = 0;
  MDeformWeight *dw;

  for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
    if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
      tot++;
      sum += dw->weight;
    }
  }

  if ((tot == 0) || (sum == 1.0f)) {
    return;
  }

  if (sum != 0.0f) {
    fac = 1.0f / sum;

    for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
      if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
        dw->weight *= fac;
      }
    }
  }
  else {
    /* hrmf, not a factor in this case */
    fac = 1.0f / tot;

    for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
      if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
        dw->weight = fac;
      }
    }
  }
}

/**
 * A version of #do_weight_paint_normalize_all that includes locked weights
 * but only changes unlocked weights.
 */
static bool do_weight_paint_normalize_all_locked(MDeformVert *dvert,
                                                 const int defbase_tot,
                                                 const bool *vgroup_validmap,
                                                 const bool *lock_flags)
{
  float sum = 0.0f, fac;
  float sum_unlock = 0.0f;
  float lock_weight = 0.0f;
  uint i, tot = 0;
  MDeformWeight *dw;

  if (lock_flags == nullptr) {
    do_weight_paint_normalize_all(dvert, defbase_tot, vgroup_validmap);
    return true;
  }

  for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
    if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
      sum += dw->weight;

      if (lock_flags[dw->def_nr]) {
        lock_weight += dw->weight;
      }
      else {
        tot++;
        sum_unlock += dw->weight;
      }
    }
  }

  if (sum == 1.0f) {
    return true;
  }

  if (tot == 0) {
    return false;
  }

  if (lock_weight >= 1.0f - VERTEX_WEIGHT_LOCK_EPSILON) {
    /* locked groups make it impossible to fully normalize,
     * zero out what we can and return false */
    for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
      if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
        if (lock_flags[dw->def_nr] == false) {
          dw->weight = 0.0f;
        }
      }
    }

    return (lock_weight == 1.0f);
  }
  if (sum_unlock != 0.0f) {
    fac = (1.0f - lock_weight) / sum_unlock;

    for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
      if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
        if (lock_flags[dw->def_nr] == false) {
          dw->weight *= fac;
          /* paranoid but possibly with float error */
          CLAMP(dw->weight, 0.0f, 1.0f);
        }
      }
    }
  }
  else {
    /* hrmf, not a factor in this case */
    fac = (1.0f - lock_weight) / tot;
    /* paranoid but possibly with float error */
    CLAMP(fac, 0.0f, 1.0f);

    for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
      if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
        if (lock_flags[dw->def_nr] == false) {
          dw->weight = fac;
        }
      }
    }
  }

  return true;
}

/**
 * \note same as function above except it does a second pass without active group
 * if normalize fails with it.
 */
static void do_weight_paint_normalize_all_locked_try_active(MDeformVert *dvert,
                                                            const int defbase_tot,
                                                            const bool *vgroup_validmap,
                                                            const bool *lock_flags,
                                                            const bool *lock_with_active)
{
  /* first pass with both active and explicitly locked groups restricted from change */

  bool success = do_weight_paint_normalize_all_locked(
      dvert, defbase_tot, vgroup_validmap, lock_with_active);

  if (!success) {
    /**
     * Locks prevented the first pass from full completion,
     * so remove restriction on active group; e.g:
     *
     * - With 1.0 weight painted into active:
     *   nonzero locked weight; first pass zeroed out unlocked weight; scale 1 down to fit.
     * - With 0.0 weight painted into active:
     *   no unlocked groups; first pass did nothing; increase 0 to fit.
     */
    do_weight_paint_normalize_all_locked(dvert, defbase_tot, vgroup_validmap, lock_flags);
  }
}

#if 0 /* UNUSED */
static bool has_unselected_unlocked_bone_group(int defbase_tot,
                                               bool *defbase_sel,
                                               int selected,
                                               const bool *lock_flags,
                                               const bool *vgroup_validmap)
{
  int i;
  if (defbase_tot == selected) {
    return false;
  }
  for (i = 0; i < defbase_tot; i++) {
    if (vgroup_validmap[i] && !defbase_sel[i] && !lock_flags[i]) {
      return true;
    }
  }
  return false;
}
#endif

static void multipaint_clamp_change(MDeformVert *dvert,
                                    const int defbase_tot,
                                    const bool *defbase_sel,
                                    float *change_p)
{
  int i;
  MDeformWeight *dw;
  float val;
  float change = *change_p;

  /* verify that the change does not cause values exceeding 1 and clamp it */
  for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
    if (dw->def_nr < defbase_tot && defbase_sel[dw->def_nr]) {
      if (dw->weight) {
        val = dw->weight * change;
        if (val > 1) {
          change = 1.0f / dw->weight;
        }
      }
    }
  }

  *change_p = change;
}

static bool multipaint_verify_change(MDeformVert *dvert,
                                     const int defbase_tot,
                                     float change,
                                     const bool *defbase_sel)
{
  int i;
  MDeformWeight *dw;
  float val;

  /* in case the change is reduced, you need to recheck
   * the earlier values to make sure they are not 0
   * (precision error) */
  for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
    if (dw->def_nr < defbase_tot && defbase_sel[dw->def_nr]) {
      if (dw->weight) {
        val = dw->weight * change;
        /* the value should never reach zero while multi-painting if it
         * was nonzero beforehand */
        if (val <= 0) {
          return false;
        }
      }
    }
  }

  return true;
}

static void multipaint_apply_change(MDeformVert *dvert,
                                    const int defbase_tot,
                                    float change,
                                    const bool *defbase_sel)
{
  int i;
  MDeformWeight *dw;

  /* apply the valid change */
  for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
    if (dw->def_nr < defbase_tot && defbase_sel[dw->def_nr]) {
      if (dw->weight) {
        dw->weight = dw->weight * change;
        CLAMP(dw->weight, 0.0f, 1.0f);
      }
    }
  }
}

/**
 * Variables stored both for 'active' and 'mirror' sides.
 */
struct WeightPaintGroupData {
  /** index of active group or its mirror
   *
   * - 'active' is always `ob->actdef`.
   * - 'mirror' is -1 when 'ME_EDIT_MIRROR_X' flag id disabled,
   *   otherwise this will be set to the mirror or the active group (if the group isn't mirrored).
   */
  int index;
  /** lock that includes the 'index' as locked too
   *
   * - 'active' is set of locked or active/selected groups
   * - 'mirror' is set of locked or mirror groups
   */
  const bool *lock;
};

/* struct to avoid passing many args each call to do_weight_paint_vertex()
 * this _could_ be made a part of the operators 'WPaintData' struct, or at
 * least a member, but for now keep its own struct, initialized on every
 * paint stroke update - campbell */
struct WeightPaintInfo {

  MutableSpan<MDeformVert> dvert;

  int defbase_tot;

  /* both must add up to 'defbase_tot' */
  int defbase_tot_sel;
  int defbase_tot_unsel;

  WeightPaintGroupData active, mirror;

  /* boolean array for locked bones,
   * length of defbase_tot */
  const bool *lock_flags;
  /* boolean array for selected bones,
   * length of defbase_tot, can't be const because of how it's passed */
  const bool *defbase_sel;
  /* same as WeightPaintData.vgroup_validmap,
   * only added here for convenience */
  const bool *vgroup_validmap;
  /* same as WeightPaintData.vgroup_locked/unlocked,
   * only added here for convenience */
  const bool *vgroup_locked;
  const bool *vgroup_unlocked;

  bool do_flip;
  bool do_multipaint;
  bool do_auto_normalize;
  bool do_lock_relative;
  bool is_normalized;

  float brush_alpha_value; /* result of BKE_brush_alpha_get() */
};

static void do_weight_paint_vertex_single(
    /* vars which remain the same for every vert */
    const VPaint *wp,
    Object *ob,
    const WeightPaintInfo *wpi,
    /* vars which change on each stroke */
    const uint index,
    float alpha,
    float paintweight)
{
  Mesh *me = (Mesh *)ob->data;
  MDeformVert *dv = &wpi->dvert[index];
  bool topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;

  MDeformWeight *dw;
  float weight_prev, weight_cur;
  float dw_rel_locked = 0.0f, dw_rel_free = 1.0f;

  /* mirror vars */
  int index_mirr;
  int vgroup_mirr;

  MDeformVert *dv_mirr;
  MDeformWeight *dw_mirr;

  /* Check if we should mirror vertex groups (X-axis). */
  if (ME_USING_MIRROR_X_VERTEX_GROUPS(me)) {
    index_mirr = mesh_get_x_mirror_vert(ob, nullptr, index, topology);
    vgroup_mirr = wpi->mirror.index;

    /* another possible error - mirror group _and_ active group are the same (which is fine),
     * but we also are painting onto a center vertex - this would paint the same weight twice */
    if (index_mirr == index && vgroup_mirr == wpi->active.index) {
      index_mirr = vgroup_mirr = -1;
    }
  }
  else {
    index_mirr = vgroup_mirr = -1;
  }

  /* Check if painting should create new deform weight entries. */
  bool restrict_to_existing = (wp->flag & VP_FLAG_VGROUP_RESTRICT) != 0;

  if (wpi->do_lock_relative || wpi->do_auto_normalize) {
    /* Without do_lock_relative only dw_rel_locked is reliable, while dw_rel_free may be fake 0. */
    dw_rel_free = BKE_defvert_total_selected_weight(dv, wpi->defbase_tot, wpi->vgroup_unlocked);
    dw_rel_locked = BKE_defvert_total_selected_weight(dv, wpi->defbase_tot, wpi->vgroup_locked);
    CLAMP(dw_rel_locked, 0.0f, 1.0f);

    /* Do not create entries if there is not enough free weight to paint.
     * This logic is the same as in wpaint_undo_lock_relative and auto-normalize. */
    if (wpi->do_auto_normalize || dw_rel_free <= 0.0f) {
      if (dw_rel_locked >= 1.0f - VERTEX_WEIGHT_LOCK_EPSILON) {
        restrict_to_existing = true;
      }
    }
  }

  if (restrict_to_existing) {
    dw = BKE_defvert_find_index(dv, wpi->active.index);
  }
  else {
    dw = BKE_defvert_ensure_index(dv, wpi->active.index);
  }

  if (dw == nullptr) {
    return;
  }

  /* get the mirror def vars */
  if (index_mirr != -1) {
    dv_mirr = &wpi->dvert[index_mirr];
    if (wp->flag & VP_FLAG_VGROUP_RESTRICT) {
      dw_mirr = BKE_defvert_find_index(dv_mirr, vgroup_mirr);

      if (dw_mirr == nullptr) {
        index_mirr = vgroup_mirr = -1;
        dv_mirr = nullptr;
      }
    }
    else {
      if (index != index_mirr) {
        dw_mirr = BKE_defvert_ensure_index(dv_mirr, vgroup_mirr);
      }
      else {
        /* dv and dv_mirr are the same */
        int totweight_prev = dv_mirr->totweight;
        int dw_offset = int(dw - dv_mirr->dw);
        dw_mirr = BKE_defvert_ensure_index(dv_mirr, vgroup_mirr);

        /* if we added another, get our old one back */
        if (totweight_prev != dv_mirr->totweight) {
          dw = &dv_mirr->dw[dw_offset];
        }
      }
    }
  }
  else {
    dv_mirr = nullptr;
    dw_mirr = nullptr;
  }

  weight_cur = dw->weight;

  /* Handle weight caught up in locked defgroups for Lock Relative. */
  if (wpi->do_lock_relative) {
    weight_cur = BKE_defvert_calc_lock_relative_weight(weight_cur, dw_rel_locked, dw_rel_free);
  }

  if (!brush_use_accumulate(wp)) {
    MDeformVert *dvert_prev = ob->sculpt->mode.wpaint.dvert_prev;
    MDeformVert *dv_prev = defweight_prev_init(dvert_prev, wpi->dvert.data(), index);
    if (index_mirr != -1) {
      defweight_prev_init(dvert_prev, wpi->dvert.data(), index_mirr);
    }

    weight_prev = BKE_defvert_find_weight(dv_prev, wpi->active.index);

    if (wpi->do_lock_relative) {
      weight_prev = BKE_defvert_lock_relative_weight(
          weight_prev, dv_prev, wpi->defbase_tot, wpi->vgroup_locked, wpi->vgroup_unlocked);
    }
  }
  else {
    weight_prev = weight_cur;
  }

  /* If there are no normalize-locks or multipaint,
   * then there is no need to run the more complicated checks */

  {
    float new_weight = wpaint_blend(
        wp, weight_prev, alpha, paintweight, wpi->brush_alpha_value, wpi->do_flip);

    float weight = wpaint_clamp_monotonic(weight_prev, weight_cur, new_weight);

    /* Undo the lock relative weight correction. */
    if (wpi->do_lock_relative) {
      if (index_mirr == index) {
        /* When painting a center vertex with X Mirror and L/R pair,
         * handle both groups together. This avoids weird fighting
         * in the non-normalized weight mode. */
        float orig_weight = dw->weight + dw_mirr->weight;
        weight = 0.5f *
                 wpaint_undo_lock_relative(
                     weight * 2, orig_weight, dw_rel_locked, dw_rel_free, wpi->do_auto_normalize);
      }
      else {
        weight = wpaint_undo_lock_relative(
            weight, dw->weight, dw_rel_locked, dw_rel_free, wpi->do_auto_normalize);
      }

      CLAMP(weight, 0.0f, 1.0f);
    }

    dw->weight = weight;

    /* WATCH IT: take care of the ordering of applying mirror -> normalize,
     * can give wrong results T26193, least confusing if normalize is done last */

    /* apply mirror */
    if (index_mirr != -1) {
      /* copy, not paint again */
      dw_mirr->weight = dw->weight;
    }

    /* apply normalize */
    if (wpi->do_auto_normalize) {
      /* note on normalize - this used to be applied after painting and normalize all weights,
       * in some ways this is good because there is feedback where the more weights involved would
       * 'resist' so you couldn't instantly zero out other weights by painting 1.0 on the active.
       *
       * However this gave a problem since applying mirror, then normalize both verts
       * the resulting weight won't match on both sides.
       *
       * If this 'resisting', slower normalize is nicer, we could call
       * do_weight_paint_normalize_all() and only use...
       * do_weight_paint_normalize_all_active() when normalizing the mirror vertex.
       * - campbell
       */
      do_weight_paint_normalize_all_locked_try_active(
          dv, wpi->defbase_tot, wpi->vgroup_validmap, wpi->lock_flags, wpi->active.lock);

      if (index_mirr != -1) {
        /* only normalize if this is not a center vertex,
         * else we get a conflict, normalizing twice */
        if (index != index_mirr) {
          do_weight_paint_normalize_all_locked_try_active(
              dv_mirr, wpi->defbase_tot, wpi->vgroup_validmap, wpi->lock_flags, wpi->mirror.lock);
        }
        else {
          /* This case accounts for:
           * - Painting onto a center vertex of a mesh.
           * - X-mirror is enabled.
           * - Auto normalize is enabled.
           * - The group you are painting onto has a L / R version.
           *
           * We want L/R vgroups to have the same weight but this can't be if both are over 0.5,
           * We _could_ have special check for that, but this would need its own
           * normalize function which holds 2 groups from changing at once.
           *
           * So! just balance out the 2 weights, it keeps them equal and everything normalized.
           *
           * While it won't hit the desired weight immediately as the user waggles their mouse,
           * constant painting and re-normalizing will get there. this is also just simpler logic.
           * - campbell */
          dw_mirr->weight = dw->weight = (dw_mirr->weight + dw->weight) * 0.5f;
        }
      }
    }
  }
}

static void do_weight_paint_vertex_multi(
    /* vars which remain the same for every vert */
    const VPaint *wp,
    Object *ob,
    const WeightPaintInfo *wpi,
    /* vars which change on each stroke */
    const uint index,
    float alpha,
    float paintweight)
{
  Mesh *me = (Mesh *)ob->data;
  MDeformVert *dv = &wpi->dvert[index];
  bool topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;

  /* mirror vars */
  int index_mirr = -1;
  MDeformVert *dv_mirr = nullptr;

  /* weights */
  float curw, curw_real, oldw, neww, change, curw_mirr, change_mirr;
  float dw_rel_free, dw_rel_locked;

  /* Check if we should mirror vertex groups (X-axis). */
  if (ME_USING_MIRROR_X_VERTEX_GROUPS(me)) {
    index_mirr = mesh_get_x_mirror_vert(ob, nullptr, index, topology);

    if (!ELEM(index_mirr, -1, index)) {
      dv_mirr = &wpi->dvert[index_mirr];
    }
    else {
      index_mirr = -1;
    }
  }

  /* compute weight change by applying the brush to average or sum of group weights */
  curw = curw_real = BKE_defvert_multipaint_collective_weight(
      dv, wpi->defbase_tot, wpi->defbase_sel, wpi->defbase_tot_sel, wpi->is_normalized);

  if (curw == 0.0f) {
    /* NOTE: no weight to assign to this vertex, could add all groups? */
    return;
  }

  /* Handle weight caught up in locked defgroups for Lock Relative. */
  if (wpi->do_lock_relative) {
    dw_rel_free = BKE_defvert_total_selected_weight(dv, wpi->defbase_tot, wpi->vgroup_unlocked);
    dw_rel_locked = BKE_defvert_total_selected_weight(dv, wpi->defbase_tot, wpi->vgroup_locked);
    CLAMP(dw_rel_locked, 0.0f, 1.0f);

    curw = BKE_defvert_calc_lock_relative_weight(curw, dw_rel_locked, dw_rel_free);
  }

  if (!brush_use_accumulate(wp)) {
    MDeformVert *dvert_prev = ob->sculpt->mode.wpaint.dvert_prev;
    MDeformVert *dv_prev = defweight_prev_init(dvert_prev, wpi->dvert.data(), index);
    if (index_mirr != -1) {
      defweight_prev_init(dvert_prev, wpi->dvert.data(), index_mirr);
    }

    oldw = BKE_defvert_multipaint_collective_weight(
        dv_prev, wpi->defbase_tot, wpi->defbase_sel, wpi->defbase_tot_sel, wpi->is_normalized);

    if (wpi->do_lock_relative) {
      oldw = BKE_defvert_lock_relative_weight(
          oldw, dv_prev, wpi->defbase_tot, wpi->vgroup_locked, wpi->vgroup_unlocked);
    }
  }
  else {
    oldw = curw;
  }

  neww = wpaint_blend(wp, oldw, alpha, paintweight, wpi->brush_alpha_value, wpi->do_flip);
  neww = wpaint_clamp_monotonic(oldw, curw, neww);

  if (wpi->do_lock_relative) {
    neww = wpaint_undo_lock_relative(
        neww, curw_real, dw_rel_locked, dw_rel_free, wpi->do_auto_normalize);
  }

  change = neww / curw_real;

  /* verify for all groups that 0 < result <= 1 */
  multipaint_clamp_change(dv, wpi->defbase_tot, wpi->defbase_sel, &change);

  if (dv_mirr != nullptr) {
    curw_mirr = BKE_defvert_multipaint_collective_weight(
        dv_mirr, wpi->defbase_tot, wpi->defbase_sel, wpi->defbase_tot_sel, wpi->is_normalized);

    if (curw_mirr == 0.0f) {
      /* can't mirror into a zero weight vertex */
      dv_mirr = nullptr;
    }
    else {
      /* mirror is changed to achieve the same collective weight value */
      float orig = change_mirr = curw_real * change / curw_mirr;

      multipaint_clamp_change(dv_mirr, wpi->defbase_tot, wpi->defbase_sel, &change_mirr);

      if (!multipaint_verify_change(dv_mirr, wpi->defbase_tot, change_mirr, wpi->defbase_sel)) {
        return;
      }

      change *= change_mirr / orig;
    }
  }

  if (!multipaint_verify_change(dv, wpi->defbase_tot, change, wpi->defbase_sel)) {
    return;
  }

  /* apply validated change to vertex and mirror */
  multipaint_apply_change(dv, wpi->defbase_tot, change, wpi->defbase_sel);

  if (dv_mirr != nullptr) {
    multipaint_apply_change(dv_mirr, wpi->defbase_tot, change_mirr, wpi->defbase_sel);
  }

  /* normalize */
  if (wpi->do_auto_normalize) {
    do_weight_paint_normalize_all_locked_try_active(
        dv, wpi->defbase_tot, wpi->vgroup_validmap, wpi->lock_flags, wpi->active.lock);

    if (dv_mirr != nullptr) {
      do_weight_paint_normalize_all_locked_try_active(
          dv_mirr, wpi->defbase_tot, wpi->vgroup_validmap, wpi->lock_flags, wpi->active.lock);
    }
  }
}

static void do_weight_paint_vertex(
    /* vars which remain the same for every vert */
    const VPaint *wp,
    Object *ob,
    const WeightPaintInfo *wpi,
    /* vars which change on each stroke */
    const uint index,
    float alpha,
    float paintweight)
{
  if (wpi->do_multipaint) {
    do_weight_paint_vertex_multi(wp, ob, wpi, index, alpha, paintweight);
  }
  else {
    do_weight_paint_vertex_single(wp, ob, wpi, index, alpha, paintweight);
  }
}

/* Toggle operator for turning vertex paint mode on or off (copied from sculpt.cc) */
static void vertex_paint_init_session(Depsgraph *depsgraph,
                                      Scene *scene,
                                      Object *ob,
                                      eObjectMode object_mode)
{
  /* Create persistent sculpt mode data */
  BKE_sculpt_toolsettings_data_ensure(scene);

  BLI_assert(ob->sculpt == nullptr);
  ob->sculpt = (SculptSession *)MEM_callocN(sizeof(SculptSession), "sculpt session");
  ob->sculpt->mode_type = object_mode;
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, true);

  SCULPT_ensure_valid_pivot(ob, scene);
}

static void vwpaint_init_stroke(Depsgraph *depsgraph, Object *ob)
{
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, true);
  SculptSession *ss = ob->sculpt;

  /* Ensure ss->cache is allocated.  It will mostly be initialized in
   * vwpaint_update_cache_invariants and vwpaint_update_cache_variants.
   */
  if (!ss->cache) {
    ss->cache = (StrokeCache *)MEM_callocN(sizeof(StrokeCache), "stroke cache");
  }
}

static void vertex_paint_init_stroke(Scene *scene, Depsgraph *depsgraph, Object *ob)
{
  vwpaint_init_stroke(depsgraph, ob);

  SculptSession *ss = ob->sculpt;
  ToolSettings *ts = scene->toolsettings;

  /* Allocate scratch array for previous colors if needed. */
  if (!brush_use_accumulate(ts->vpaint)) {
    if (!ss->cache->prev_colors_vpaint) {
      Mesh *me = BKE_object_get_original_mesh(ob);
      size_t elem_size;
      int elem_num;

      elem_num = get_vcol_elements(me, &elem_size);

      ss->cache->prev_colors_vpaint = (uint *)MEM_callocN(elem_num * elem_size, __func__);
    }
  }
  else {
    MEM_SAFE_FREE(ss->cache->prev_colors_vpaint);
  }
}

static void vertex_paint_init_session_data(const ToolSettings *ts, Object *ob)
{
  /* Create maps */
  SculptVertexPaintGeomMap *gmap = nullptr;
  if (ob->mode == OB_MODE_VERTEX_PAINT) {
    gmap = &ob->sculpt->mode.vpaint.gmap;
    BLI_assert(ob->sculpt->mode_type == OB_MODE_VERTEX_PAINT);
  }
  else if (ob->mode == OB_MODE_WEIGHT_PAINT) {
    gmap = &ob->sculpt->mode.wpaint.gmap;
    BLI_assert(ob->sculpt->mode_type == OB_MODE_WEIGHT_PAINT);
  }
  else {
    ob->sculpt->mode_type = (eObjectMode)0;
    BLI_assert(0);
    return;
  }

  Mesh *me = (Mesh *)ob->data;
  const Span<MPoly> polys = me->polys();
  const Span<MLoop> loops = me->loops();

  if (gmap->vert_to_loop == nullptr) {
    gmap->vert_map_mem = nullptr;
    gmap->vert_to_loop = nullptr;
    gmap->poly_map_mem = nullptr;
    gmap->vert_to_poly = nullptr;
    BKE_mesh_vert_loop_map_create(&gmap->vert_to_loop,
                                  &gmap->vert_map_mem,
                                  polys.data(),
                                  loops.data(),
                                  me->totvert,
                                  me->totpoly,
                                  me->totloop);
    BKE_mesh_vert_poly_map_create(&gmap->vert_to_poly,
                                  &gmap->poly_map_mem,
                                  polys.data(),
                                  loops.data(),
                                  me->totvert,
                                  me->totpoly,
                                  me->totloop);
  }

  /* Create average brush arrays */
  if (ob->mode == OB_MODE_WEIGHT_PAINT) {
    if (!brush_use_accumulate(ts->wpaint)) {
      if (ob->sculpt->mode.wpaint.alpha_weight == nullptr) {
        ob->sculpt->mode.wpaint.alpha_weight = (float *)MEM_callocN(me->totvert * sizeof(float),
                                                                    __func__);
      }
      if (ob->sculpt->mode.wpaint.dvert_prev == nullptr) {
        ob->sculpt->mode.wpaint.dvert_prev = (MDeformVert *)MEM_callocN(
            me->totvert * sizeof(MDeformVert), __func__);
        MDeformVert *dv = ob->sculpt->mode.wpaint.dvert_prev;
        for (int i = 0; i < me->totvert; i++, dv++) {
          /* Use to show this isn't initialized, never apply to the mesh data. */
          dv->flag = 1;
        }
      }
    }
    else {
      MEM_SAFE_FREE(ob->sculpt->mode.wpaint.alpha_weight);
      if (ob->sculpt->mode.wpaint.dvert_prev != nullptr) {
        BKE_defvert_array_free_elems(ob->sculpt->mode.wpaint.dvert_prev, me->totvert);
        MEM_freeN(ob->sculpt->mode.wpaint.dvert_prev);
        ob->sculpt->mode.wpaint.dvert_prev = nullptr;
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Enter Vertex/Weight Paint Mode
 * \{ */

static void ed_vwpaintmode_enter_generic(
    Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob, const eObjectMode mode_flag)
{
  ob->mode |= mode_flag;
  Mesh *me = BKE_mesh_from_object(ob);

  /* Same as sculpt mode, make sure we don't have cached derived mesh which
   * points to freed arrays.
   */
  BKE_object_free_derived_caches(ob);

  if (mode_flag == OB_MODE_VERTEX_PAINT) {
    const ePaintMode paint_mode = PAINT_MODE_VERTEX;
    ED_mesh_color_ensure(me, nullptr);

    BKE_paint_ensure(scene->toolsettings, (Paint **)&scene->toolsettings->vpaint);
    Paint *paint = BKE_paint_get_active_from_paintmode(scene, paint_mode);
    ED_paint_cursor_start(paint, vertex_paint_poll);
    BKE_paint_init(bmain, scene, paint_mode, PAINT_CURSOR_VERTEX_PAINT);
  }
  else if (mode_flag == OB_MODE_WEIGHT_PAINT) {
    const ePaintMode paint_mode = PAINT_MODE_WEIGHT;

    BKE_paint_ensure(scene->toolsettings, (Paint **)&scene->toolsettings->wpaint);
    Paint *paint = BKE_paint_get_active_from_paintmode(scene, paint_mode);
    ED_paint_cursor_start(paint, weight_paint_poll);
    BKE_paint_init(bmain, scene, paint_mode, PAINT_CURSOR_WEIGHT_PAINT);

    /* weight paint specific */
    ED_mesh_mirror_spatial_table_end(ob);
    ED_vgroup_sync_from_pose(ob);
  }
  else {
    BLI_assert(0);
  }

  /* Create vertex/weight paint mode session data */
  if (ob->sculpt) {
    if (ob->sculpt->cache) {
      SCULPT_cache_free(ob->sculpt->cache);
      ob->sculpt->cache = nullptr;
    }
    BKE_sculptsession_free(ob);
  }

  vertex_paint_init_session(depsgraph, scene, ob, mode_flag);

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

void ED_object_vpaintmode_enter_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  ed_vwpaintmode_enter_generic(bmain, depsgraph, scene, ob, OB_MODE_VERTEX_PAINT);
}
void ED_object_vpaintmode_enter(bContext *C, Depsgraph *depsgraph)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  ED_object_vpaintmode_enter_ex(bmain, depsgraph, scene, ob);
}

void ED_object_wpaintmode_enter_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  ed_vwpaintmode_enter_generic(bmain, depsgraph, scene, ob, OB_MODE_WEIGHT_PAINT);
}
void ED_object_wpaintmode_enter(bContext *C, Depsgraph *depsgraph)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  ED_object_wpaintmode_enter_ex(bmain, depsgraph, scene, ob);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exit Vertex/Weight Paint Mode
 * \{ */

static void ed_vwpaintmode_exit_generic(Object *ob, const eObjectMode mode_flag)
{
  Mesh *me = BKE_mesh_from_object(ob);
  ob->mode &= ~mode_flag;

  if (mode_flag == OB_MODE_VERTEX_PAINT) {
    if (me->editflag & ME_EDIT_PAINT_FACE_SEL) {
      BKE_mesh_flush_select_from_polys(me);
    }
    else if (me->editflag & ME_EDIT_PAINT_VERT_SEL) {
      BKE_mesh_flush_select_from_verts(me);
    }
  }
  else if (mode_flag == OB_MODE_WEIGHT_PAINT) {
    if (me->editflag & ME_EDIT_PAINT_VERT_SEL) {
      BKE_mesh_flush_select_from_verts(me);
    }
    else if (me->editflag & ME_EDIT_PAINT_FACE_SEL) {
      BKE_mesh_flush_select_from_polys(me);
    }
  }
  else {
    BLI_assert(0);
  }

  /* If the cache is not released by a cancel or a done, free it now. */
  if (ob->sculpt && ob->sculpt->cache) {
    SCULPT_cache_free(ob->sculpt->cache);
    ob->sculpt->cache = nullptr;
  }

  BKE_sculptsession_free(ob);

  paint_cursor_delete_textures();

  if (mode_flag == OB_MODE_WEIGHT_PAINT) {
    ED_mesh_mirror_spatial_table_end(ob);
    ED_mesh_mirror_topo_table_end(ob);
  }

  /* Never leave derived meshes behind. */
  BKE_object_free_derived_caches(ob);

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

void ED_object_vpaintmode_exit_ex(Object *ob)
{
  ed_vwpaintmode_exit_generic(ob, OB_MODE_VERTEX_PAINT);
}
void ED_object_vpaintmode_exit(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  ED_object_vpaintmode_exit_ex(ob);
}

void ED_object_wpaintmode_exit_ex(Object *ob)
{
  ed_vwpaintmode_exit_generic(ob, OB_MODE_WEIGHT_PAINT);
}
void ED_object_wpaintmode_exit(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  ED_object_wpaintmode_exit_ex(ob);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Weight Paint Operator
 * \{ */

/**
 * \note Keep in sync with #vpaint_mode_toggle_exec
 */
static int wpaint_mode_toggle_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  Object *ob = CTX_data_active_object(C);
  const int mode_flag = OB_MODE_WEIGHT_PAINT;
  const bool is_mode_set = (ob->mode & mode_flag) != 0;
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;

  if (!is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, (eObjectMode)mode_flag, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  Mesh *me = BKE_mesh_from_object(ob);

  if (is_mode_set) {
    ED_object_wpaintmode_exit_ex(ob);
  }
  else {
    Depsgraph *depsgraph = CTX_data_depsgraph_on_load(C);
    if (depsgraph) {
      depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    }
    ED_object_wpaintmode_enter_ex(bmain, depsgraph, scene, ob);
    BKE_paint_toolslots_brush_validate(bmain, &ts->wpaint->paint);
  }

  /* Prepare armature posemode. */
  ED_object_posemode_set_for_weight_paint(C, bmain, ob, is_mode_set);

  /* Weight-paint works by overriding colors in mesh,
   * so need to make sure we recalculate on enter and
   * exit (exit needs doing regardless because we
   * should re-deform).
   */
  DEG_id_tag_update(&me->id, 0);

  WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);

  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

  WM_toolsystem_update_from_context_view3d(C);

  return OPERATOR_FINISHED;
}

static bool paint_mode_toggle_poll_test(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || ob->type != OB_MESH) {
    return false;
  }
  if (!ob->data || ID_IS_LINKED(ob->data)) {
    return false;
  }
  return true;
}

void PAINT_OT_weight_paint_toggle(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Weight Paint Mode";
  ot->idname = "PAINT_OT_weight_paint_toggle";
  ot->description = "Toggle weight paint mode in 3D view";

  /* api callbacks */
  ot->exec = wpaint_mode_toggle_exec;
  ot->poll = paint_mode_toggle_poll_test;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weight Paint Operator
 * \{ */

struct WPaintData {
  ViewContext vc;
  NormalAnglePrecalc normal_angle_precalc;

  WeightPaintGroupData active, mirror;

  /* variables for auto normalize */
  const bool *vgroup_validmap; /* stores if vgroups tie to deforming bones or not */
  const bool *lock_flags;
  const bool *vgroup_locked;   /* mask of locked defbones */
  const bool *vgroup_unlocked; /* mask of unlocked defbones */

  /* variables for multipaint */
  const bool *defbase_sel; /* set of selected groups */
  int defbase_tot_sel;     /* number of selected groups */
  bool do_multipaint;      /* true if multipaint enabled and multiple groups selected */
  bool do_lock_relative;

  int defbase_tot;

  /* original weight values for use in blur/smear */
  float *precomputed_weight;
  bool precomputed_weight_ready;
};

static void smooth_brush_toggle_on(const bContext *C, Paint *paint, StrokeCache *cache)
{
  Scene *scene = CTX_data_scene(C);
  Brush *brush = paint->brush;
  int cur_brush_size = BKE_brush_size_get(scene, brush);

  BLI_strncpy(
      cache->saved_active_brush_name, brush->id.name + 2, sizeof(cache->saved_active_brush_name));

  /* Switch to the blur (smooth) brush. */
  brush = BKE_paint_toolslots_brush_get(paint, WPAINT_TOOL_BLUR);
  if (brush) {
    BKE_paint_brush_set(paint, brush);
    cache->saved_smooth_size = BKE_brush_size_get(scene, brush);
    BKE_brush_size_set(scene, brush, cur_brush_size);
    BKE_curvemapping_init(brush->curve);
  }
}

static void smooth_brush_toggle_off(const bContext *C, Paint *paint, StrokeCache *cache)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Brush *brush = BKE_paint_brush(paint);
  /* The current brush should match with what we have stored in the cache. */
  BLI_assert(brush == cache->brush);

  /* Try to switch back to the saved/previous brush. */
  BKE_brush_size_set(scene, brush, cache->saved_smooth_size);
  brush = (Brush *)BKE_libblock_find_name(bmain, ID_BR, cache->saved_active_brush_name);
  if (brush) {
    BKE_paint_brush_set(paint, brush);
  }
}

/* Initialize the stroke cache invariants from operator properties */
static void vwpaint_update_cache_invariants(
    bContext *C, VPaint *vp, SculptSession *ss, wmOperator *op, const float mval[2])
{
  StrokeCache *cache;
  const Scene *scene = CTX_data_scene(C);
  UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
  ViewContext *vc = paint_stroke_view_context((PaintStroke *)op->customdata);
  Object *ob = CTX_data_active_object(C);
  float mat[3][3];
  float view_dir[3] = {0.0f, 0.0f, 1.0f};
  int mode;

  /* VW paint needs to allocate stroke cache before update is called. */
  if (!ss->cache) {
    cache = (StrokeCache *)MEM_callocN(sizeof(StrokeCache), "stroke cache");
    ss->cache = cache;
  }
  else {
    cache = ss->cache;
  }

  /* Initial mouse location */
  if (mval) {
    copy_v2_v2(cache->initial_mouse, mval);
  }
  else {
    zero_v2(cache->initial_mouse);
  }

  mode = RNA_enum_get(op->ptr, "mode");
  cache->invert = mode == BRUSH_STROKE_INVERT;
  cache->alt_smooth = mode == BRUSH_STROKE_SMOOTH;
  /* not very nice, but with current events system implementation
   * we can't handle brush appearance inversion hotkey separately (sergey) */
  if (cache->invert) {
    ups->draw_inverted = true;
  }
  else {
    ups->draw_inverted = false;
  }

  if (cache->alt_smooth) {
    smooth_brush_toggle_on(C, &vp->paint, cache);
  }

  copy_v2_v2(cache->mouse, cache->initial_mouse);
  const Brush *brush = vp->paint.brush;
  /* Truly temporary data that isn't stored in properties */
  cache->vc = vc;
  cache->brush = brush;
  cache->first_time = true;

  /* cache projection matrix */
  ED_view3d_ob_project_mat_get(cache->vc->rv3d, ob, cache->projection_mat);

  invert_m4_m4(ob->world_to_object, ob->object_to_world);
  copy_m3_m4(mat, cache->vc->rv3d->viewinv);
  mul_m3_v3(mat, view_dir);
  copy_m3_m4(mat, ob->world_to_object);
  mul_m3_v3(mat, view_dir);
  normalize_v3_v3(cache->true_view_normal, view_dir);

  copy_v3_v3(cache->view_normal, cache->true_view_normal);
  cache->bstrength = BKE_brush_alpha_get(scene, brush);
  cache->is_last_valid = false;
}

/* Initialize the stroke cache variants from operator properties */
static void vwpaint_update_cache_variants(bContext *C, VPaint *vp, Object *ob, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  Brush *brush = BKE_paint_brush(&vp->paint);

  /* This effects the actual brush radius, so things farther away
   * are compared with a larger radius and vice versa. */
  if (cache->first_time) {
    RNA_float_get_array(ptr, "location", cache->true_location);
  }

  RNA_float_get_array(ptr, "mouse", cache->mouse);

  /* XXX: Use pressure value from first brush step for brushes which don't
   * support strokes (grab, thumb). They depends on initial state and
   * brush coord/pressure/etc.
   * It's more an events design issue, which doesn't split coordinate/pressure/angle
   * changing events. We should avoid this after events system re-design */
  if (paint_supports_dynamic_size(brush, PAINT_MODE_SCULPT) || cache->first_time) {
    cache->pressure = RNA_float_get(ptr, "pressure");
  }

  /* Truly temporary data that isn't stored in properties */
  if (cache->first_time) {
    cache->initial_radius = paint_calc_object_space_radius(
        cache->vc, cache->true_location, BKE_brush_size_get(scene, brush));
    BKE_brush_unprojected_radius_set(scene, brush, cache->initial_radius);
  }

  if (BKE_brush_use_size_pressure(brush) &&
      paint_supports_dynamic_size(brush, PAINT_MODE_SCULPT)) {
    cache->radius = cache->initial_radius * cache->pressure;
  }
  else {
    cache->radius = cache->initial_radius;
  }

  cache->radius_squared = cache->radius * cache->radius;

  if (ss->pbvh) {
    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateRedraw | PBVH_UpdateBB);
  }
}

static bool wpaint_stroke_test_start(bContext *C, wmOperator *op, const float mouse[2])
{
  Scene *scene = CTX_data_scene(C);
  PaintStroke *stroke = (PaintStroke *)op->customdata;
  ToolSettings *ts = scene->toolsettings;
  Object *ob = CTX_data_active_object(C);
  Mesh *me = BKE_mesh_from_object(ob);
  WPaintData *wpd;
  WPaintVGroupIndex vgroup_index;
  int defbase_tot, defbase_tot_sel;
  bool *defbase_sel;
  SculptSession *ss = ob->sculpt;
  VPaint *vp = CTX_data_tool_settings(C)->wpaint;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  if (ED_wpaint_ensure_data(C, op->reports, WPAINT_ENSURE_MIRROR, &vgroup_index) == false) {
    return false;
  }

  {
    /* check if we are attempting to paint onto a locked vertex group,
     * and other options disallow it from doing anything useful */
    bDeformGroup *dg;
    dg = (bDeformGroup *)BLI_findlink(&me->vertex_group_names, vgroup_index.active);
    if (dg->flag & DG_LOCK_WEIGHT) {
      BKE_report(op->reports, RPT_WARNING, "Active group is locked, aborting");
      return false;
    }
    if (vgroup_index.mirror != -1) {
      dg = (bDeformGroup *)BLI_findlink(&me->vertex_group_names, vgroup_index.mirror);
      if (dg->flag & DG_LOCK_WEIGHT) {
        BKE_report(op->reports, RPT_WARNING, "Mirror group is locked, aborting");
        return false;
      }
    }
  }

  /* check that multipaint groups are unlocked */
  defbase_tot = BLI_listbase_count(&me->vertex_group_names);
  defbase_sel = BKE_object_defgroup_selected_get(ob, defbase_tot, &defbase_tot_sel);

  if (ts->multipaint && defbase_tot_sel > 1) {
    int i;
    bDeformGroup *dg;

    if (ME_USING_MIRROR_X_VERTEX_GROUPS(me)) {
      BKE_object_defgroup_mirror_selection(
          ob, defbase_tot, defbase_sel, defbase_sel, &defbase_tot_sel);
    }

    for (i = 0; i < defbase_tot; i++) {
      if (defbase_sel[i]) {
        dg = (bDeformGroup *)BLI_findlink(&me->vertex_group_names, i);
        if (dg->flag & DG_LOCK_WEIGHT) {
          BKE_report(op->reports, RPT_WARNING, "Multipaint group is locked, aborting");
          MEM_freeN(defbase_sel);
          return false;
        }
      }
    }
  }

  /* ALLOCATIONS! no return after this line */
  /* make mode data storage */
  wpd = (WPaintData *)MEM_callocN(sizeof(WPaintData), "WPaintData");
  paint_stroke_set_mode_data(stroke, wpd);
  ED_view3d_viewcontext_init(C, &wpd->vc, depsgraph);
  view_angle_limits_init(&wpd->normal_angle_precalc,
                         vp->paint.brush->falloff_angle,
                         (vp->paint.brush->flag & BRUSH_FRONTFACE_FALLOFF) != 0);

  wpd->active.index = vgroup_index.active;
  wpd->mirror.index = vgroup_index.mirror;

  /* multipaint */
  wpd->defbase_tot = defbase_tot;
  wpd->defbase_sel = defbase_sel;
  wpd->defbase_tot_sel = defbase_tot_sel > 1 ? defbase_tot_sel : 1;
  wpd->do_multipaint = (ts->multipaint && defbase_tot_sel > 1);

  /* set up auto-normalize, and generate map for detecting which
   * vgroups affect deform bones */
  wpd->lock_flags = BKE_object_defgroup_lock_flags_get(ob, wpd->defbase_tot);
  if (ts->auto_normalize || ts->multipaint || wpd->lock_flags != nullptr ||
      ts->wpaint_lock_relative) {
    wpd->vgroup_validmap = BKE_object_defgroup_validmap_get(ob, wpd->defbase_tot);
  }

  /* Compute the set of all locked deform groups when Lock Relative is active. */
  if (ts->wpaint_lock_relative &&
      BKE_object_defgroup_check_lock_relative(
          wpd->lock_flags, wpd->vgroup_validmap, wpd->active.index) &&
      (!wpd->do_multipaint || BKE_object_defgroup_check_lock_relative_multi(
                                  defbase_tot, wpd->lock_flags, defbase_sel, defbase_tot_sel))) {
    wpd->do_lock_relative = true;
  }

  if (wpd->do_lock_relative || (ts->auto_normalize && wpd->lock_flags && !wpd->do_multipaint)) {
    bool *unlocked = (bool *)MEM_dupallocN(wpd->vgroup_validmap);

    if (wpd->lock_flags) {
      bool *locked = (bool *)MEM_mallocN(sizeof(bool) * wpd->defbase_tot, __func__);
      BKE_object_defgroup_split_locked_validmap(
          wpd->defbase_tot, wpd->lock_flags, wpd->vgroup_validmap, locked, unlocked);
      wpd->vgroup_locked = locked;
    }

    wpd->vgroup_unlocked = unlocked;
  }

  if (wpd->do_multipaint && ts->auto_normalize) {
    bool *tmpflags;
    tmpflags = (bool *)MEM_mallocN(sizeof(bool) * defbase_tot, __func__);
    if (wpd->lock_flags) {
      BLI_array_binary_or(tmpflags, wpd->defbase_sel, wpd->lock_flags, wpd->defbase_tot);
    }
    else {
      memcpy(tmpflags, wpd->defbase_sel, sizeof(*tmpflags) * wpd->defbase_tot);
    }
    wpd->active.lock = tmpflags;
  }
  else if (ts->auto_normalize) {
    bool *tmpflags;

    tmpflags = wpd->lock_flags ? (bool *)MEM_dupallocN(wpd->lock_flags) :
                                 (bool *)MEM_callocN(sizeof(bool) * defbase_tot, __func__);
    tmpflags[wpd->active.index] = true;
    wpd->active.lock = tmpflags;

    tmpflags = wpd->lock_flags ? (bool *)MEM_dupallocN(wpd->lock_flags) :
                                 (bool *)MEM_callocN(sizeof(bool) * defbase_tot, __func__);
    tmpflags[(wpd->mirror.index != -1) ? wpd->mirror.index : wpd->active.index] = true;
    wpd->mirror.lock = tmpflags;
  }

  /* If not previously created, create vertex/weight paint mode session data */
  vwpaint_init_stroke(depsgraph, ob);
  vwpaint_update_cache_invariants(C, vp, ss, op, mouse);
  vertex_paint_init_session_data(ts, ob);

  if (ELEM(vp->paint.brush->weightpaint_tool, WPAINT_TOOL_SMEAR, WPAINT_TOOL_BLUR)) {
    wpd->precomputed_weight = (float *)MEM_mallocN(sizeof(float) * me->totvert, __func__);
  }

  if (ob->sculpt->mode.wpaint.dvert_prev != nullptr) {
    MDeformVert *dv = ob->sculpt->mode.wpaint.dvert_prev;
    for (int i = 0; i < me->totvert; i++, dv++) {
      /* Use to show this isn't initialized, never apply to the mesh data. */
      dv->flag = 1;
    }
  }

  return true;
}

static void get_brush_alpha_data(const Scene *scene,
                                 const SculptSession *ss,
                                 const Brush *brush,
                                 float *r_brush_size_pressure,
                                 float *r_brush_alpha_value,
                                 float *r_brush_alpha_pressure)
{
  *r_brush_size_pressure = BKE_brush_size_get(scene, brush) *
                           (BKE_brush_use_size_pressure(brush) ? ss->cache->pressure : 1.0f);
  *r_brush_alpha_value = BKE_brush_alpha_get(scene, brush);
  *r_brush_alpha_pressure = (BKE_brush_use_alpha_pressure(brush) ? ss->cache->pressure : 1.0f);
}

static float wpaint_get_active_weight(const MDeformVert *dv, const WeightPaintInfo *wpi)
{
  float weight;

  if (wpi->do_multipaint) {
    weight = BKE_defvert_multipaint_collective_weight(
        dv, wpi->defbase_tot, wpi->defbase_sel, wpi->defbase_tot_sel, wpi->is_normalized);
  }
  else {
    weight = BKE_defvert_find_weight(dv, wpi->active.index);
  }

  if (wpi->do_lock_relative) {
    weight = BKE_defvert_lock_relative_weight(
        weight, dv, wpi->defbase_tot, wpi->vgroup_locked, wpi->vgroup_unlocked);
  }

  CLAMP(weight, 0.0f, 1.0f);
  return weight;
}

static void do_wpaint_precompute_weight_cb_ex(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  const MDeformVert *dv = &data->wpi->dvert[n];

  data->wpd->precomputed_weight[n] = wpaint_get_active_weight(dv, data->wpi);
}

static void precompute_weight_values(
    bContext *C, Object *ob, Brush *brush, WPaintData *wpd, WeightPaintInfo *wpi, Mesh *me)
{
  if (wpd->precomputed_weight_ready && !brush_use_accumulate_ex(brush, ob->mode)) {
    return;
  }

  /* threaded loop over vertices */
  SculptThreadedTaskData data;
  data.C = C;
  data.ob = ob;
  data.wpd = wpd;
  data.wpi = wpi;
  data.me = me;

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  BLI_task_parallel_range(0, me->totvert, &data, do_wpaint_precompute_weight_cb_ex, &settings);

  wpd->precomputed_weight_ready = true;
}

static void do_wpaint_brush_blur_task_cb_ex(void *__restrict userdata,
                                            const int n,
                                            const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
  const bool has_grids = (pbvh_type == PBVH_GRIDS);
  const SculptVertexPaintGeomMap *gmap = &ss->mode.wpaint.gmap;

  const Brush *brush = data->brush;
  const StrokeCache *cache = ss->cache;
  Scene *scene = CTX_data_scene(data->C);

  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
  get_brush_alpha_data(
      scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint_use_normal(data->vp);
  const bool use_face_sel = (data->me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const bool use_vert_sel = (data->me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, data->brush->falloff_shape);

  const blender::bke::AttributeAccessor attributes = data->me->attributes();
  const blender::VArray<bool> select_vert = attributes.lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);

  /* For each vertex */
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    /* Test to see if the vertex coordinates are within the spherical brush region. */
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      /* For grid based pbvh, take the vert whose loop corresponds to the current grid.
       * Otherwise, take the current vert. */
      const int v_index = has_grids ? ss->mloop[vd.grid_indices[vd.g]].v : vd.vert_indices[vd.i];
      const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;
      /* If the vertex is selected */
      if (!(use_face_sel || use_vert_sel) || select_vert[v_index]) {
        /* Get the average poly weight */
        int total_hit_loops = 0;
        float weight_final = 0.0f;
        for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
          const int p_index = gmap->vert_to_poly[v_index].indices[j];
          const MPoly *mp = &ss->mpoly[p_index];

          total_hit_loops += mp->totloop;
          for (int k = 0; k < mp->totloop; k++) {
            const int l_index = mp->loopstart + k;
            const MLoop *ml = &ss->mloop[l_index];
            weight_final += data->wpd->precomputed_weight[ml->v];
          }
        }

        /* Apply the weight to the vertex. */
        if (total_hit_loops != 0) {
          float brush_strength = cache->bstrength;
          const float angle_cos = (use_normal && vd.no) ?
                                      dot_v3v3(sculpt_normal_frontface, vd.no) :
                                      1.0f;
          if (((brush->flag & BRUSH_FRONTFACE) == 0 || (angle_cos > 0.0f)) &&
              ((brush->flag & BRUSH_FRONTFACE_FALLOFF) == 0 ||
               view_angle_limits_apply_falloff(
                   &data->wpd->normal_angle_precalc, angle_cos, &brush_strength))) {
            const float brush_fade = BKE_brush_curve_strength(
                brush, sqrtf(test.dist), cache->radius);
            const float final_alpha = brush_fade * brush_strength * grid_alpha *
                                      brush_alpha_pressure;

            if ((brush->flag & BRUSH_ACCUMULATE) == 0) {
              if (ss->mode.wpaint.alpha_weight[v_index] < final_alpha) {
                ss->mode.wpaint.alpha_weight[v_index] = final_alpha;
              }
              else {
                continue;
              }
            }

            weight_final /= total_hit_loops;
            /* Only paint visible verts */
            do_weight_paint_vertex(
                data->vp, data->ob, data->wpi, v_index, final_alpha, weight_final);
          }
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_wpaint_brush_smear_task_cb_ex(void *__restrict userdata,
                                             const int n,
                                             const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
  const bool has_grids = (pbvh_type == PBVH_GRIDS);
  const SculptVertexPaintGeomMap *gmap = &ss->mode.wpaint.gmap;

  const Brush *brush = data->brush;
  const Scene *scene = CTX_data_scene(data->C);
  const StrokeCache *cache = ss->cache;
  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
  get_brush_alpha_data(
      scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint_use_normal(data->vp);
  const bool use_face_sel = (data->me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const bool use_vert_sel = (data->me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
  float brush_dir[3];

  sub_v3_v3v3(brush_dir, cache->location, cache->last_location);
  project_plane_v3_v3v3(brush_dir, brush_dir, cache->view_normal);

  const blender::bke::AttributeAccessor attributes = data->me->attributes();
  const blender::VArray<bool> select_vert = attributes.lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);

  if (cache->is_last_valid && (normalize_v3(brush_dir) != 0.0f)) {

    SculptBrushTest test;
    SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
        ss, &test, data->brush->falloff_shape);
    const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
        ss, data->brush->falloff_shape);

    /* For each vertex */
    PBVHVertexIter vd;
    BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
      /* Test to see if the vertex coordinates are within the spherical brush region. */
      if (sculpt_brush_test_sq_fn(&test, vd.co)) {
        /* For grid based pbvh, take the vert whose loop corresponds to the current grid.
         * Otherwise, take the current vert. */
        const int v_index = has_grids ? ss->mloop[vd.grid_indices[vd.g]].v : vd.vert_indices[vd.i];
        const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;
        const float3 &mv_curr = ss->vert_positions[v_index];

        /* If the vertex is selected */
        if (!(use_face_sel || use_vert_sel) || select_vert[v_index]) {
          float brush_strength = cache->bstrength;
          const float angle_cos = (use_normal && vd.no) ?
                                      dot_v3v3(sculpt_normal_frontface, vd.no) :
                                      1.0f;
          if (((brush->flag & BRUSH_FRONTFACE) == 0 || (angle_cos > 0.0f)) &&
              ((brush->flag & BRUSH_FRONTFACE_FALLOFF) == 0 ||
               view_angle_limits_apply_falloff(
                   &data->wpd->normal_angle_precalc, angle_cos, &brush_strength))) {
            bool do_color = false;
            /* Minimum dot product between brush direction and current
             * to neighbor direction is 0.0, meaning orthogonal. */
            float stroke_dot_max = 0.0f;

            /* Get the color of the loop in the opposite direction of the brush movement
             * (this callback is specifically for smear.) */
            float weight_final = 0.0;
            for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
              const int p_index = gmap->vert_to_poly[v_index].indices[j];
              const MPoly *mp = &ss->mpoly[p_index];
              const MLoop *ml_other = &ss->mloop[mp->loopstart];
              for (int k = 0; k < mp->totloop; k++, ml_other++) {
                const uint v_other_index = ml_other->v;
                if (v_other_index != v_index) {
                  const float3 &mv_other = ss->vert_positions[v_other_index];

                  /* Get the direction from the selected vert to the neighbor. */
                  float other_dir[3];
                  sub_v3_v3v3(other_dir, mv_curr, mv_other);
                  project_plane_v3_v3v3(other_dir, other_dir, cache->view_normal);

                  normalize_v3(other_dir);

                  const float stroke_dot = dot_v3v3(other_dir, brush_dir);

                  if (stroke_dot > stroke_dot_max) {
                    stroke_dot_max = stroke_dot;
                    weight_final = data->wpd->precomputed_weight[v_other_index];
                    do_color = true;
                  }
                }
              }
            }
            /* Apply weight to vertex */
            if (do_color) {
              const float brush_fade = BKE_brush_curve_strength(
                  brush, sqrtf(test.dist), cache->radius);
              const float final_alpha = brush_fade * brush_strength * grid_alpha *
                                        brush_alpha_pressure;

              if (final_alpha <= 0.0f) {
                continue;
              }

              do_weight_paint_vertex(
                  data->vp, data->ob, data->wpi, v_index, final_alpha, float(weight_final));
            }
          }
        }
      }
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void do_wpaint_brush_draw_task_cb_ex(void *__restrict userdata,
                                            const int n,
                                            const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
  const bool has_grids = (pbvh_type == PBVH_GRIDS);
  const Scene *scene = CTX_data_scene(data->C);

  const Brush *brush = data->brush;
  const StrokeCache *cache = ss->cache;
  /* NOTE: normally `BKE_brush_weight_get(scene, brush)` is used,
   * however in this case we calculate a new weight each time. */
  const float paintweight = data->strength;
  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
  get_brush_alpha_data(
      scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint_use_normal(data->vp);
  const bool use_face_sel = (data->me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const bool use_vert_sel = (data->me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, data->brush->falloff_shape);

  const blender::bke::AttributeAccessor attributes = data->me->attributes();
  const blender::VArray<bool> select_vert = attributes.lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);

  /* For each vertex */
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    /* Test to see if the vertex coordinates are within the spherical brush region. */
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      /* NOTE: grids are 1:1 with corners (aka loops).
       * For multires, take the vert whose loop corresponds to the current grid.
       * Otherwise, take the current vert. */
      const int v_index = has_grids ? ss->mloop[vd.grid_indices[vd.g]].v : vd.vert_indices[vd.i];
      const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;

      /* If the vertex is selected */
      if (!(use_face_sel || use_vert_sel) || select_vert[v_index]) {
        float brush_strength = cache->bstrength;
        const float angle_cos = (use_normal && vd.no) ? dot_v3v3(sculpt_normal_frontface, vd.no) :
                                                        1.0f;
        if (((brush->flag & BRUSH_FRONTFACE) == 0 || (angle_cos > 0.0f)) &&
            ((brush->flag & BRUSH_FRONTFACE_FALLOFF) == 0 ||
             view_angle_limits_apply_falloff(
                 &data->wpd->normal_angle_precalc, angle_cos, &brush_strength))) {
          const float brush_fade = BKE_brush_curve_strength(
              brush, sqrtf(test.dist), cache->radius);
          const float final_alpha = brush_fade * brush_strength * grid_alpha *
                                    brush_alpha_pressure;

          if ((brush->flag & BRUSH_ACCUMULATE) == 0) {
            if (ss->mode.wpaint.alpha_weight[v_index] < final_alpha) {
              ss->mode.wpaint.alpha_weight[v_index] = final_alpha;
            }
            else {
              continue;
            }
          }

          do_weight_paint_vertex(data->vp, data->ob, data->wpi, v_index, final_alpha, paintweight);
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_wpaint_brush_calc_average_weight_cb_ex(void *__restrict userdata,
                                                      const int n,
                                                      const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  StrokeCache *cache = ss->cache;
  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
  const bool has_grids = (pbvh_type == PBVH_GRIDS);

  const bool use_normal = vwpaint_use_normal(data->vp);
  const bool use_face_sel = (data->me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const bool use_vert_sel = (data->me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

  WPaintAverageAccum *accum = (WPaintAverageAccum *)data->custom_data + n;
  accum->len = 0;
  accum->value = 0.0;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, data->brush->falloff_shape);

  const blender::bke::AttributeAccessor attributes = data->me->attributes();
  const blender::VArray<bool> select_vert = attributes.lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);

  /* For each vertex */
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    /* Test to see if the vertex coordinates are within the spherical brush region. */
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float angle_cos = (use_normal && vd.no) ? dot_v3v3(sculpt_normal_frontface, vd.no) :
                                                      1.0f;
      if (angle_cos > 0.0 &&
          BKE_brush_curve_strength(data->brush, sqrtf(test.dist), cache->radius) > 0.0) {
        const int v_index = has_grids ? ss->mloop[vd.grid_indices[vd.g]].v : vd.vert_indices[vd.i];

        /* If the vertex is selected. */
        if (!(use_face_sel || use_vert_sel) || select_vert[v_index]) {
          const MDeformVert *dv = &data->wpi->dvert[v_index];
          accum->len += 1;
          accum->value += wpaint_get_active_weight(dv, data->wpi);
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void calculate_average_weight(SculptThreadedTaskData *data,
                                     PBVHNode ** /*nodes*/,
                                     int totnode)
{
  WPaintAverageAccum *accum = (WPaintAverageAccum *)MEM_mallocN(sizeof(*accum) * totnode,
                                                                __func__);
  data->custom_data = accum;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, data, do_wpaint_brush_calc_average_weight_cb_ex, &settings);

  uint accum_len = 0;
  double accum_weight = 0.0;
  for (int i = 0; i < totnode; i++) {
    accum_len += accum[i].len;
    accum_weight += accum[i].value;
  }
  if (accum_len != 0) {
    accum_weight /= accum_len;
    data->strength = float(accum_weight);
  }

  MEM_SAFE_FREE(data->custom_data); /* 'accum' */
}

static void wpaint_paint_leaves(bContext *C,
                                Object *ob,
                                Sculpt *sd,
                                VPaint *vp,
                                WPaintData *wpd,
                                WeightPaintInfo *wpi,
                                Mesh *me,
                                PBVHNode **nodes,
                                int totnode)
{
  Scene *scene = CTX_data_scene(C);
  const Brush *brush = ob->sculpt->cache->brush;

  /* threaded loop over nodes */
  SculptThreadedTaskData data = {nullptr};
  data.C = C;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.vp = vp;
  data.wpd = wpd;
  data.wpi = wpi;
  data.me = me;

  /* Use this so average can modify its weight without touching the brush. */
  data.strength = BKE_brush_weight_get(scene, brush);

  /* NOTE: current mirroring code cannot be run in parallel */
  TaskParallelSettings settings;
  const bool use_threading = !ME_USING_MIRROR_X_VERTEX_GROUPS(me);
  BKE_pbvh_parallel_range_settings(&settings, use_threading, totnode);

  switch ((eBrushWeightPaintTool)brush->weightpaint_tool) {
    case WPAINT_TOOL_AVERAGE:
      calculate_average_weight(&data, nodes, totnode);
      BLI_task_parallel_range(0, totnode, &data, do_wpaint_brush_draw_task_cb_ex, &settings);
      break;
    case WPAINT_TOOL_SMEAR:
      BLI_task_parallel_range(0, totnode, &data, do_wpaint_brush_smear_task_cb_ex, &settings);
      break;
    case WPAINT_TOOL_BLUR:
      BLI_task_parallel_range(0, totnode, &data, do_wpaint_brush_blur_task_cb_ex, &settings);
      break;
    case WPAINT_TOOL_DRAW:
      BLI_task_parallel_range(0, totnode, &data, do_wpaint_brush_draw_task_cb_ex, &settings);
      break;
  }
}

static PBVHNode **vwpaint_pbvh_gather_generic(
    Object *ob, VPaint *wp, Sculpt *sd, Brush *brush, int *r_totnode)
{
  SculptSession *ss = ob->sculpt;
  const bool use_normal = vwpaint_use_normal(wp);
  PBVHNode **nodes = nullptr;

  /* Build a list of all nodes that are potentially within the brush's area of influence */
  if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    SculptSearchSphereData data = {nullptr};
    data.ss = ss;
    data.sd = sd;
    data.radius_squared = ss->cache->radius_squared;
    data.original = true;

    BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data, &nodes, r_totnode);
    if (use_normal) {
      SCULPT_pbvh_calc_area_normal(
          brush, ob, nodes, *r_totnode, true, ss->cache->sculpt_normal_symm);
    }
    else {
      zero_v3(ss->cache->sculpt_normal_symm);
    }
  }
  else {
    DistRayAABB_Precalc dist_ray_to_aabb_precalc;
    dist_squared_ray_to_aabb_v3_precalc(
        &dist_ray_to_aabb_precalc, ss->cache->location, ss->cache->view_normal);
    SculptSearchCircleData data = {nullptr};
    data.ss = ss;
    data.sd = sd;
    data.radius_squared = ss->cache->radius_squared;
    data.original = true;
    data.dist_ray_to_aabb_precalc = &dist_ray_to_aabb_precalc;

    BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_circle_cb, &data, &nodes, r_totnode);
    if (use_normal) {
      copy_v3_v3(ss->cache->sculpt_normal_symm, ss->cache->view_normal);
    }
    else {
      zero_v3(ss->cache->sculpt_normal_symm);
    }
  }
  return nodes;
}

static void wpaint_do_paint(bContext *C,
                            Object *ob,
                            VPaint *wp,
                            Sculpt *sd,
                            WPaintData *wpd,
                            WeightPaintInfo *wpi,
                            Mesh *me,
                            Brush *brush,
                            const ePaintSymmetryFlags symm,
                            const int axis,
                            const int i,
                            const float angle)
{
  SculptSession *ss = ob->sculpt;
  ss->cache->radial_symmetry_pass = i;
  SCULPT_cache_calc_brushdata_symm(ss->cache, symm, axis, angle);

  int totnode;
  PBVHNode **nodes = vwpaint_pbvh_gather_generic(ob, wp, sd, brush, &totnode);

  wpaint_paint_leaves(C, ob, sd, wp, wpd, wpi, me, nodes, totnode);

  if (nodes) {
    MEM_freeN(nodes);
  }
}

static void wpaint_do_radial_symmetry(bContext *C,
                                      Object *ob,
                                      VPaint *wp,
                                      Sculpt *sd,
                                      WPaintData *wpd,
                                      WeightPaintInfo *wpi,
                                      Mesh *me,
                                      Brush *brush,
                                      const ePaintSymmetryFlags symm,
                                      const int axis)
{
  for (int i = 1; i < wp->radial_symm[axis - 'X']; i++) {
    const float angle = (2.0 * M_PI) * i / wp->radial_symm[axis - 'X'];
    wpaint_do_paint(C, ob, wp, sd, wpd, wpi, me, brush, symm, axis, i, angle);
  }
}

/* near duplicate of: sculpt.cc's,
 * 'do_symmetrical_brush_actions' and 'vpaint_do_symmetrical_brush_actions'. */
static void wpaint_do_symmetrical_brush_actions(
    bContext *C, Object *ob, VPaint *wp, Sculpt *sd, WPaintData *wpd, WeightPaintInfo *wpi)
{
  Brush *brush = BKE_paint_brush(&wp->paint);
  Mesh *me = (Mesh *)ob->data;
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  int i = 0;

  /* initial stroke */
  cache->mirror_symmetry_pass = ePaintSymmetryFlags(0);
  wpaint_do_paint(C, ob, wp, sd, wpd, wpi, me, brush, ePaintSymmetryFlags(0), 'X', 0, 0);
  wpaint_do_radial_symmetry(C, ob, wp, sd, wpd, wpi, me, brush, ePaintSymmetryFlags(0), 'X');
  wpaint_do_radial_symmetry(C, ob, wp, sd, wpd, wpi, me, brush, ePaintSymmetryFlags(0), 'Y');
  wpaint_do_radial_symmetry(C, ob, wp, sd, wpd, wpi, me, brush, ePaintSymmetryFlags(0), 'Z');

  cache->symmetry = symm;

  if (me->editflag & ME_EDIT_MIRROR_VERTEX_GROUPS) {
    /* We don't do any symmetry strokes when mirroring vertex groups. */
    copy_v3_v3(cache->true_last_location, cache->true_location);
    cache->is_last_valid = true;
    return;
  }

  /* symm is a bit combination of XYZ - 1 is mirror
   * X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  for (i = 1; i <= symm; i++) {
    if (symm & i && (symm != 5 || i != 3) && (symm != 6 || !ELEM(i, 3, 5))) {
      const ePaintSymmetryFlags symm = ePaintSymmetryFlags(i);
      cache->mirror_symmetry_pass = symm;
      cache->radial_symmetry_pass = 0;
      SCULPT_cache_calc_brushdata_symm(cache, symm, 0, 0);

      if (i & (1 << 0)) {
        wpaint_do_paint(C, ob, wp, sd, wpd, wpi, me, brush, symm, 'X', 0, 0);
        wpaint_do_radial_symmetry(C, ob, wp, sd, wpd, wpi, me, brush, symm, 'X');
      }
      if (i & (1 << 1)) {
        wpaint_do_paint(C, ob, wp, sd, wpd, wpi, me, brush, symm, 'Y', 0, 0);
        wpaint_do_radial_symmetry(C, ob, wp, sd, wpd, wpi, me, brush, symm, 'Y');
      }
      if (i & (1 << 2)) {
        wpaint_do_paint(C, ob, wp, sd, wpd, wpi, me, brush, symm, 'Z', 0, 0);
        wpaint_do_radial_symmetry(C, ob, wp, sd, wpd, wpi, me, brush, symm, 'Z');
      }
    }
  }
  copy_v3_v3(cache->true_last_location, cache->true_location);
  cache->is_last_valid = true;
}

static void wpaint_stroke_update_step(bContext *C,
                                      wmOperator * /*op*/,
                                      PaintStroke *stroke,
                                      PointerRNA *itemptr)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  VPaint *wp = ts->wpaint;
  Brush *brush = BKE_paint_brush(&wp->paint);
  WPaintData *wpd = (WPaintData *)paint_stroke_mode_data(stroke);
  ViewContext *vc;
  Object *ob = CTX_data_active_object(C);

  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  vwpaint_update_cache_variants(C, wp, ob, itemptr);

  float mat[4][4];

  const float brush_alpha_value = BKE_brush_alpha_get(scene, brush);

  /* intentionally don't initialize as nullptr, make sure we initialize all members below */
  WeightPaintInfo wpi;

  /* cannot paint if there is no stroke data */
  if (wpd == nullptr) {
    /* XXX: force a redraw here, since even though we can't paint,
     * at least view won't freeze until stroke ends */
    ED_region_tag_redraw(CTX_wm_region(C));
    return;
  }

  vc = &wpd->vc;
  ob = vc->obact;

  view3d_operator_needs_opengl(C);
  ED_view3d_init_mats_rv3d(ob, vc->rv3d);

  /* load projection matrix */
  mul_m4_m4m4(mat, vc->rv3d->persmat, ob->object_to_world);

  Mesh *mesh = static_cast<Mesh *>(ob->data);

  /* *** setup WeightPaintInfo - pass onto do_weight_paint_vertex *** */
  wpi.dvert = mesh->deform_verts_for_write();

  wpi.defbase_tot = wpd->defbase_tot;
  wpi.defbase_sel = wpd->defbase_sel;
  wpi.defbase_tot_sel = wpd->defbase_tot_sel;

  wpi.defbase_tot_unsel = wpi.defbase_tot - wpi.defbase_tot_sel;
  wpi.active = wpd->active;
  wpi.mirror = wpd->mirror;
  wpi.lock_flags = wpd->lock_flags;
  wpi.vgroup_validmap = wpd->vgroup_validmap;
  wpi.vgroup_locked = wpd->vgroup_locked;
  wpi.vgroup_unlocked = wpd->vgroup_unlocked;
  wpi.do_flip = RNA_boolean_get(itemptr, "pen_flip") || ss->cache->invert;
  wpi.do_multipaint = wpd->do_multipaint;
  wpi.do_auto_normalize = ((ts->auto_normalize != 0) && (wpi.vgroup_validmap != nullptr) &&
                           (wpi.do_multipaint || wpi.vgroup_validmap[wpi.active.index]));
  wpi.do_lock_relative = wpd->do_lock_relative;
  wpi.is_normalized = wpi.do_auto_normalize || wpi.do_lock_relative;
  wpi.brush_alpha_value = brush_alpha_value;
  /* *** done setting up WeightPaintInfo *** */

  if (wpd->precomputed_weight) {
    precompute_weight_values(C, ob, brush, wpd, &wpi, mesh);
  }

  wpaint_do_symmetrical_brush_actions(C, ob, wp, sd, wpd, &wpi);

  swap_m4m4(vc->rv3d->persmat, mat);

  /* Calculate pivot for rotation around selection if needed.
   * also needed for "Frame Selected" on last stroke. */
  float loc_world[3];
  mul_v3_m4v3(loc_world, ob->object_to_world, ss->cache->true_location);
  paint_last_stroke_update(scene, loc_world);

  BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);

  DEG_id_tag_update(&mesh->id, 0);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  swap_m4m4(wpd->vc.rv3d->persmat, mat);

  rcti r;
  if (SCULPT_get_redraw_rect(vc->region, CTX_wm_region_view3d(C), ob, &r)) {
    if (ss->cache) {
      ss->cache->current_r = r;
    }

    /* previous is not set in the current cache else
     * the partial rect will always grow */
    if (ss->cache) {
      if (!BLI_rcti_is_empty(&ss->cache->previous_r)) {
        BLI_rcti_union(&r, &ss->cache->previous_r);
      }
    }

    r.xmin += vc->region->winrct.xmin - 2;
    r.xmax += vc->region->winrct.xmin + 2;
    r.ymin += vc->region->winrct.ymin - 2;
    r.ymax += vc->region->winrct.ymin + 2;
  }
  ED_region_tag_redraw_partial(vc->region, &r, true);
}

static void wpaint_stroke_done(const bContext *C, PaintStroke *stroke)
{
  Object *ob = CTX_data_active_object(C);
  WPaintData *wpd = (WPaintData *)paint_stroke_mode_data(stroke);

  if (wpd) {
    MEM_SAFE_FREE(wpd->defbase_sel);
    MEM_SAFE_FREE(wpd->vgroup_validmap);
    MEM_SAFE_FREE(wpd->vgroup_locked);
    MEM_SAFE_FREE(wpd->vgroup_unlocked);
    MEM_SAFE_FREE(wpd->lock_flags);
    MEM_SAFE_FREE(wpd->active.lock);
    MEM_SAFE_FREE(wpd->mirror.lock);
    MEM_SAFE_FREE(wpd->precomputed_weight);
    MEM_freeN(wpd);
  }

  SculptSession *ss = ob->sculpt;

  if (ss->cache->alt_smooth) {
    ToolSettings *ts = CTX_data_tool_settings(C);
    VPaint *vp = ts->wpaint;
    smooth_brush_toggle_off(C, &vp->paint, ss->cache);
  }

  /* and particles too */
  if (ob->particlesystem.first) {
    ParticleSystem *psys;
    int i;

    for (psys = (ParticleSystem *)ob->particlesystem.first; psys; psys = psys->next) {
      for (i = 0; i < PSYS_TOT_VG; i++) {
        if (psys->vgroup[i] == BKE_object_defgroup_active_index_get(ob)) {
          psys->recalc |= ID_RECALC_PSYS_RESET;
          break;
        }
      }
    }
  }

  DEG_id_tag_update((ID *)ob->data, 0);

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  SCULPT_cache_free(ob->sculpt->cache);
  ob->sculpt->cache = nullptr;
}

static int wpaint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;

  op->customdata = paint_stroke_new(C,
                                    op,
                                    SCULPT_stroke_get_location,
                                    wpaint_stroke_test_start,
                                    wpaint_stroke_update_step,
                                    nullptr,
                                    wpaint_stroke_done,
                                    event->type);

  if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
    paint_stroke_free(C, op, (PaintStroke *)op->customdata);
    return OPERATOR_FINISHED;
  }
  /* add modal handler */
  WM_event_add_modal_handler(C, op);

  OPERATOR_RETVAL_CHECK(retval);
  BLI_assert(retval == OPERATOR_RUNNING_MODAL);

  return OPERATOR_RUNNING_MODAL;
}

static int wpaint_exec(bContext *C, wmOperator *op)
{
  op->customdata = paint_stroke_new(C,
                                    op,
                                    SCULPT_stroke_get_location,
                                    wpaint_stroke_test_start,
                                    wpaint_stroke_update_step,
                                    nullptr,
                                    wpaint_stroke_done,
                                    0);

  /* frees op->customdata */
  paint_stroke_exec(C, op, (PaintStroke *)op->customdata);

  return OPERATOR_FINISHED;
}

static void wpaint_cancel(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  if (ob->sculpt->cache) {
    SCULPT_cache_free(ob->sculpt->cache);
    ob->sculpt->cache = nullptr;
  }

  paint_stroke_cancel(C, op, (PaintStroke *)op->customdata);
}

static int wpaint_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, (PaintStroke **)&op->customdata);
}

void PAINT_OT_weight_paint(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Weight Paint";
  ot->idname = "PAINT_OT_weight_paint";
  ot->description = "Paint a stroke in the current vertex group's weights";

  /* api callbacks */
  ot->invoke = wpaint_invoke;
  ot->modal = wpaint_modal;
  ot->exec = wpaint_exec;
  ot->poll = weight_paint_poll;
  ot->cancel = wpaint_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

  paint_stroke_operator_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Vertex Paint Operator
 * \{ */

/**
 * \note Keep in sync with #wpaint_mode_toggle_exec
 */
static int vpaint_mode_toggle_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  Object *ob = CTX_data_active_object(C);
  const int mode_flag = OB_MODE_VERTEX_PAINT;
  const bool is_mode_set = (ob->mode & mode_flag) != 0;
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;

  if (!is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, (eObjectMode)mode_flag, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  Mesh *me = BKE_mesh_from_object(ob);

  /* toggle: end vpaint */
  if (is_mode_set) {
    ED_object_vpaintmode_exit_ex(ob);
  }
  else {
    Depsgraph *depsgraph = CTX_data_depsgraph_on_load(C);
    if (depsgraph) {
      depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    }
    ED_object_vpaintmode_enter_ex(bmain, depsgraph, scene, ob);
    BKE_paint_toolslots_brush_validate(bmain, &ts->vpaint->paint);
  }

  BKE_mesh_batch_cache_dirty_tag((Mesh *)ob->data, BKE_MESH_BATCH_DIRTY_ALL);

  /* update modifier stack for mapping requirements */
  DEG_id_tag_update(&me->id, 0);

  WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);
  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

  WM_toolsystem_update_from_context_view3d(C);

  return OPERATOR_FINISHED;
}

void PAINT_OT_vertex_paint_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Mode";
  ot->idname = "PAINT_OT_vertex_paint_toggle";
  ot->description = "Toggle the vertex paint mode in 3D view";

  /* api callbacks */
  ot->exec = vpaint_mode_toggle_exec;
  ot->poll = paint_mode_toggle_poll_test;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Paint Operator
 * \{ */

/* Implementation notes:
 *
 * Operator->invoke()
 * - Validate context (add #Mesh.mloopcol).
 * - Create custom-data storage.
 * - Call paint once (mouse click).
 * - Add modal handler.
 *
 * Operator->modal()
 * - For every mouse-move, apply vertex paint.
 * - Exit on mouse release, free custom-data.
 *   (return OPERATOR_FINISHED also removes handler and operator)
 *
 * For future:
 * - implement a stroke event (or mouse-move with past positions).
 * - revise whether op->customdata should be added in object, in set_vpaint.
 */

struct VPaintDataBase {
  ViewContext vc;
  eAttrDomain domain;
  eCustomDataType type;
};

template<typename Color, typename Traits, eAttrDomain domain>
struct VPaintData : public VPaintDataBase {
  NormalAnglePrecalc normal_angle_precalc;

  Color paintcol;

  struct VertProjHandle *vp_handle;
  CoNo *vertexcosnos;

  bool is_texbrush;

  /* Special storage for smear brush, avoid feedback loop - update each step. */
  struct {
    void *color_prev;
    void *color_curr;
  } smear;
};

template<typename Color, typename Traits, eAttrDomain domain>
static void *vpaint_init_vpaint(bContext *C,
                                wmOperator *op,
                                Scene *scene,
                                Depsgraph *depsgraph,
                                VPaint *vp,
                                Object *ob,
                                Mesh *me,
                                const Brush *brush)
{
  VPaintData<Color, Traits, domain> *vpd;

  /* make mode data storage */
  vpd = MEM_new<VPaintData<Color, Traits, domain>>("VPaintData");

  if constexpr (std::is_same_v<Color, ColorPaint4f>) {
    vpd->type = CD_PROP_COLOR;
  }
  else {
    vpd->type = CD_PROP_BYTE_COLOR;
  }

  vpd->domain = domain;

  ED_view3d_viewcontext_init(C, &vpd->vc, depsgraph);
  view_angle_limits_init(&vpd->normal_angle_precalc,
                         vp->paint.brush->falloff_angle,
                         (vp->paint.brush->flag & BRUSH_FRONTFACE_FALLOFF) != 0);

  vpd->paintcol = vpaint_get_current_col<Color, Traits, domain>(
      scene, vp, (RNA_enum_get(op->ptr, "mode") == BRUSH_STROKE_INVERT));

  vpd->is_texbrush = !(brush->vertexpaint_tool == VPAINT_TOOL_BLUR) && brush->mtex.tex;

  if (brush->vertexpaint_tool == VPAINT_TOOL_SMEAR) {
    const GVArray attribute = me->attributes().lookup(me->active_color_attribute, domain);
    vpd->smear.color_prev = MEM_malloc_arrayN(attribute.size(), attribute.type().size(), __func__);
    attribute.materialize(vpd->smear.color_prev);

    vpd->smear.color_curr = MEM_dupallocN(vpd->smear.color_prev);
  }

  /* Create projection handle */
  if (vpd->is_texbrush) {
    ob->sculpt->building_vp_handle = true;
    vpd->vp_handle = ED_vpaint_proj_handle_create(depsgraph, scene, ob, &vpd->vertexcosnos);
    ob->sculpt->building_vp_handle = false;
  }

  return static_cast<void *>(vpd);
}

static bool vpaint_stroke_test_start(bContext *C, wmOperator *op, const float mouse[2])
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  PaintStroke *stroke = (PaintStroke *)op->customdata;
  VPaint *vp = ts->vpaint;
  Brush *brush = BKE_paint_brush(&vp->paint);
  Object *ob = CTX_data_active_object(C);
  Mesh *me;
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  /* context checks could be a poll() */
  me = BKE_mesh_from_object(ob);
  if (me == nullptr || me->totpoly == 0) {
    return false;
  }

  ED_mesh_color_ensure(me, nullptr);

  const std::optional<bke::AttributeMetaData> meta_data = me->attributes().lookup_meta_data(
      me->active_color_attribute);

  if (!meta_data) {
    return false;
  }

  void *vpd = nullptr;

  if (meta_data->domain == ATTR_DOMAIN_POINT) {
    if (meta_data->data_type == CD_PROP_COLOR) {
      vpd = vpaint_init_vpaint<ColorPaint4f, FloatTraits, ATTR_DOMAIN_POINT>(
          C, op, scene, depsgraph, vp, ob, me, brush);
    }
    else if (meta_data->data_type == CD_PROP_BYTE_COLOR) {
      vpd = vpaint_init_vpaint<ColorPaint4b, ByteTraits, ATTR_DOMAIN_POINT>(
          C, op, scene, depsgraph, vp, ob, me, brush);
    }
  }
  else if (meta_data->domain == ATTR_DOMAIN_CORNER) {
    if (meta_data->data_type == CD_PROP_COLOR) {
      vpd = vpaint_init_vpaint<ColorPaint4f, FloatTraits, ATTR_DOMAIN_CORNER>(
          C, op, scene, depsgraph, vp, ob, me, brush);
    }
    else if (meta_data->data_type == CD_PROP_BYTE_COLOR) {
      vpd = vpaint_init_vpaint<ColorPaint4b, ByteTraits, ATTR_DOMAIN_CORNER>(
          C, op, scene, depsgraph, vp, ob, me, brush);
    }
  }

  BLI_assert(vpd != nullptr);

  paint_stroke_set_mode_data(stroke, vpd);

  /* If not previously created, create vertex/weight paint mode session data */
  vertex_paint_init_stroke(scene, depsgraph, ob);
  vwpaint_update_cache_invariants(C, vp, ss, op, mouse);
  vertex_paint_init_session_data(ts, ob);

  return true;
}

template<class Color = ColorPaint4b, typename Traits = ByteTraits>
static void do_vpaint_brush_blur_loops(bContext *C,
                                       Sculpt * /*sd*/,
                                       VPaint *vp,
                                       VPaintData<Color, Traits, ATTR_DOMAIN_CORNER> *vpd,
                                       Object *ob,
                                       Mesh *me,
                                       PBVHNode **nodes,
                                       int totnode,
                                       Color *lcol)
{
  using Blend = typename Traits::BlendType;

  SculptSession *ss = ob->sculpt;

  const Brush *brush = ob->sculpt->cache->brush;
  const Scene *scene = CTX_data_scene(C);

  Color *previous_color = static_cast<Color *>(ss->cache->prev_colors_vpaint);

  const blender::VArray<bool> select_vert = me->attributes().lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);
  const blender::VArray<bool> select_poly = me->attributes().lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);

  blender::threading::parallel_for(IndexRange(totnode), 1LL, [&](IndexRange range) {
    for (int n : range) {
      const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
      const bool has_grids = (pbvh_type == PBVH_GRIDS);

      const SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;
      const StrokeCache *cache = ss->cache;
      float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
      get_brush_alpha_data(
          scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
      const bool use_normal = vwpaint_use_normal(vp);
      const bool use_vert_sel = (me->editflag &
                                 (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) != 0;
      const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

      SculptBrushTest test;
      SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
          ss, &test, brush->falloff_shape);
      const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
          ss, brush->falloff_shape);

      /* For each vertex */
      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin (ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
        /* Test to see if the vertex coordinates are within the spherical brush region. */
        if (sculpt_brush_test_sq_fn(&test, vd.co)) {
          /* For grid based pbvh, take the vert whose loop corresponds to the current grid.
           * Otherwise, take the current vert. */
          const int v_index = has_grids ? ss->mloop[vd.grid_indices[vd.g]].v :
                                          vd.vert_indices[vd.i];
          const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;

          /* If the vertex is selected for painting. */
          if (!use_vert_sel || select_vert[v_index]) {
            float brush_strength = cache->bstrength;
            const float angle_cos = (use_normal && vd.no) ?
                                        dot_v3v3(sculpt_normal_frontface, vd.no) :
                                        1.0f;
            if (((brush->flag & BRUSH_FRONTFACE) == 0 || (angle_cos > 0.0f)) &&
                ((brush->flag & BRUSH_FRONTFACE_FALLOFF) == 0 ||
                 view_angle_limits_apply_falloff(
                     &vpd->normal_angle_precalc, angle_cos, &brush_strength))) {
              const float brush_fade = BKE_brush_curve_strength(
                  brush, sqrtf(test.dist), cache->radius);

              /* Get the average poly color */
              Color color_final(0, 0, 0, 0);

              int total_hit_loops = 0;
              Blend blend[4] = {0};

              for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
                int p_index = gmap->vert_to_poly[v_index].indices[j];
                const MPoly *mp = &ss->mpoly[p_index];
                if (!use_face_sel || select_poly[p_index]) {
                  total_hit_loops += mp->totloop;
                  for (int k = 0; k < mp->totloop; k++) {
                    const uint l_index = mp->loopstart + k;
                    Color *col = lcol + l_index;

                    /* Color is squared to compensate the sqrt color encoding. */
                    blend[0] += (Blend)col->r * (Blend)col->r;
                    blend[1] += (Blend)col->g * (Blend)col->g;
                    blend[2] += (Blend)col->b * (Blend)col->b;
                    blend[3] += (Blend)col->a * (Blend)col->a;
                  }
                }
              }

              if (total_hit_loops != 0) {
                /* Use rgb^2 color averaging. */
                Color *col = &color_final;

                color_final.r = Traits::round(
                    sqrtf(Traits::divide_round(blend[0], total_hit_loops)));
                color_final.g = Traits::round(
                    sqrtf(Traits::divide_round(blend[1], total_hit_loops)));
                color_final.b = Traits::round(
                    sqrtf(Traits::divide_round(blend[2], total_hit_loops)));
                color_final.a = Traits::round(
                    sqrtf(Traits::divide_round(blend[3], total_hit_loops)));

                /* For each poly owning this vert,
                 * paint each loop belonging to this vert. */
                for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
                  const int p_index = gmap->vert_to_poly[v_index].indices[j];
                  const int l_index = gmap->vert_to_loop[v_index].indices[j];
                  BLI_assert(ss->mloop[l_index].v == v_index);
                  if (!use_face_sel || select_poly[p_index]) {
                    Color color_orig(0, 0, 0, 0); /* unused when array is nullptr */

                    if (previous_color != nullptr) {
                      /* Get the previous loop color */
                      if (isZero(previous_color[l_index])) {
                        previous_color[l_index] = lcol[l_index];
                      }
                      color_orig = previous_color[l_index];
                    }
                    const float final_alpha = Traits::range * brush_fade * brush_strength *
                                              brush_alpha_pressure * grid_alpha;
                    /* Mix the new color with the original
                     * based on the brush strength and the curve. */
                    lcol[l_index] = vpaint_blend<Color, Traits>(vp,
                                                                lcol[l_index],
                                                                color_orig,
                                                                *col,
                                                                final_alpha,
                                                                Traits::range * brush_strength);
                  }
                }
              }
            }
          }
        }
      }
      BKE_pbvh_vertex_iter_end;
    };
  });
}

template<class Color = ColorPaint4b, typename Traits = ByteTraits>
static void do_vpaint_brush_blur_verts(bContext *C,
                                       Sculpt * /*sd*/,
                                       VPaint *vp,
                                       VPaintData<Color, Traits, ATTR_DOMAIN_POINT> *vpd,
                                       Object *ob,
                                       Mesh *me,
                                       PBVHNode **nodes,
                                       int totnode,
                                       Color *lcol)
{
  using Blend = typename Traits::BlendType;

  SculptSession *ss = ob->sculpt;

  const Brush *brush = ob->sculpt->cache->brush;
  const Scene *scene = CTX_data_scene(C);

  Color *previous_color = static_cast<Color *>(ss->cache->prev_colors_vpaint);

  const blender::VArray<bool> select_vert = me->attributes().lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);
  const blender::VArray<bool> select_poly = me->attributes().lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);

  blender::threading::parallel_for(IndexRange(totnode), 1LL, [&](IndexRange range) {
    for (int n : range) {
      const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
      const bool has_grids = (pbvh_type == PBVH_GRIDS);

      const SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;
      const StrokeCache *cache = ss->cache;
      float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
      get_brush_alpha_data(
          scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
      const bool use_normal = vwpaint_use_normal(vp);
      const bool use_vert_sel = (me->editflag &
                                 (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) != 0;
      const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

      SculptBrushTest test;
      SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
          ss, &test, brush->falloff_shape);
      const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
          ss, brush->falloff_shape);

      /* For each vertex */
      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin (ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
        /* Test to see if the vertex coordinates are within the spherical brush region. */
        if (sculpt_brush_test_sq_fn(&test, vd.co)) {
          /* For grid based pbvh, take the vert whose loop corresponds to the current grid.
           * Otherwise, take the current vert. */
          const int v_index = has_grids ? ss->mloop[vd.grid_indices[vd.g]].v :
                                          vd.vert_indices[vd.i];
          const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;

          /* If the vertex is selected for painting. */
          if (!use_vert_sel || select_vert[v_index]) {
            float brush_strength = cache->bstrength;
            const float angle_cos = (use_normal && vd.no) ?
                                        dot_v3v3(sculpt_normal_frontface, vd.no) :
                                        1.0f;
            if (((brush->flag & BRUSH_FRONTFACE) == 0 || (angle_cos > 0.0f)) &&
                ((brush->flag & BRUSH_FRONTFACE_FALLOFF) == 0 ||
                 view_angle_limits_apply_falloff(
                     &vpd->normal_angle_precalc, angle_cos, &brush_strength))) {
              const float brush_fade = BKE_brush_curve_strength(
                  brush, sqrtf(test.dist), cache->radius);

              /* Get the average poly color */
              Color color_final(0, 0, 0, 0);

              int total_hit_loops = 0;
              Blend blend[4] = {0};

              for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
                int p_index = gmap->vert_to_poly[v_index].indices[j];
                const MPoly *mp = &ss->mpoly[p_index];
                if (!use_face_sel || select_poly[p_index]) {
                  total_hit_loops += mp->totloop;
                  for (int k = 0; k < mp->totloop; k++) {
                    const uint l_index = mp->loopstart + k;
                    const uint v_index = ss->mloop[l_index].v;

                    Color *col = lcol + v_index;

                    /* Color is squared to compensate the sqrt color encoding. */
                    blend[0] += (Blend)col->r * (Blend)col->r;
                    blend[1] += (Blend)col->g * (Blend)col->g;
                    blend[2] += (Blend)col->b * (Blend)col->b;
                    blend[3] += (Blend)col->a * (Blend)col->a;
                  }
                }
              }

              if (total_hit_loops != 0) {
                /* Use rgb^2 color averaging. */
                Color *col = &color_final;

                color_final.r = Traits::round(
                    sqrtf(Traits::divide_round(blend[0], total_hit_loops)));
                color_final.g = Traits::round(
                    sqrtf(Traits::divide_round(blend[1], total_hit_loops)));
                color_final.b = Traits::round(
                    sqrtf(Traits::divide_round(blend[2], total_hit_loops)));
                color_final.a = Traits::round(
                    sqrtf(Traits::divide_round(blend[3], total_hit_loops)));

                /* For each poly owning this vert,
                 * paint each loop belonging to this vert. */
                for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
                  const int p_index = gmap->vert_to_poly[v_index].indices[j];

                  BLI_assert(ss->mloop[gmap->vert_to_loop[v_index].indices[j]].v == v_index);

                  if (!use_face_sel || select_poly[p_index]) {
                    Color color_orig(0, 0, 0, 0); /* unused when array is nullptr */

                    if (previous_color != nullptr) {
                      /* Get the previous loop color */
                      if (isZero(previous_color[v_index])) {
                        previous_color[v_index] = lcol[v_index];
                      }
                      color_orig = previous_color[v_index];
                    }
                    const float final_alpha = Traits::range * brush_fade * brush_strength *
                                              brush_alpha_pressure * grid_alpha;
                    /* Mix the new color with the original
                     * based on the brush strength and the curve. */
                    lcol[v_index] = vpaint_blend<Color, Traits>(vp,
                                                                lcol[v_index],
                                                                color_orig,
                                                                *col,
                                                                final_alpha,
                                                                Traits::range * brush_strength);
                  }
                }
              }
            }
          }
        }
      }
      BKE_pbvh_vertex_iter_end;
    };
  });
}

template<typename Color = ColorPaint4b, typename Traits, eAttrDomain domain>
static void do_vpaint_brush_smear(bContext *C,
                                  Sculpt * /*sd*/,
                                  VPaint *vp,
                                  VPaintData<Color, Traits, domain> *vpd,
                                  Object *ob,
                                  Mesh *me,
                                  PBVHNode **nodes,
                                  int totnode,
                                  Color *lcol)
{
  SculptSession *ss = ob->sculpt;

  const SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;
  const StrokeCache *cache = ss->cache;
  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
  const bool has_grids = (pbvh_type == PBVH_GRIDS);

  const Brush *brush = ob->sculpt->cache->brush;
  const Scene *scene = CTX_data_scene(C);
  Color *color_curr = static_cast<Color *>(vpd->smear.color_curr);
  Color *color_prev_smear = static_cast<Color *>(vpd->smear.color_prev);
  Color *color_prev = reinterpret_cast<Color *>(ss->cache->prev_colors_vpaint);

  const blender::VArray<bool> select_vert = me->attributes().lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);
  const blender::VArray<bool> select_poly = me->attributes().lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);

  blender::threading::parallel_for(IndexRange(totnode), 1LL, [&](IndexRange range) {
    for (int n : range) {
      float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;

      get_brush_alpha_data(
          scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
      float brush_dir[3];
      const bool use_normal = vwpaint_use_normal(vp);
      const bool use_vert_sel = (me->editflag &
                                 (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) != 0;
      const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

      sub_v3_v3v3(brush_dir, cache->location, cache->last_location);
      project_plane_v3_v3v3(brush_dir, brush_dir, cache->view_normal);

      if (cache->is_last_valid && (normalize_v3(brush_dir) != 0.0f)) {

        SculptBrushTest test;
        SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
            ss, &test, brush->falloff_shape);
        const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
            ss, brush->falloff_shape);

        /* For each vertex */
        PBVHVertexIter vd;
        BKE_pbvh_vertex_iter_begin (ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
          /* Test to see if the vertex coordinates are within the spherical brush region. */
          if (sculpt_brush_test_sq_fn(&test, vd.co)) {
            /* For grid based pbvh, take the vert whose loop corresponds to the current grid.
             * Otherwise, take the current vert. */
            const int v_index = has_grids ? ss->mloop[vd.grid_indices[vd.g]].v :
                                            vd.vert_indices[vd.i];
            const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;
            const float3 &mv_curr = &ss->vert_positions[v_index];

            /* if the vertex is selected for painting. */
            if (!use_vert_sel || select_vert[v_index]) {
              /* Calc the dot prod. between ray norm on surf and current vert
               * (ie splash prevention factor), and only paint front facing verts. */
              float brush_strength = cache->bstrength;
              const float angle_cos = (use_normal && vd.no) ?
                                          dot_v3v3(sculpt_normal_frontface, vd.no) :
                                          1.0f;
              if (((brush->flag & BRUSH_FRONTFACE) == 0 || (angle_cos > 0.0f)) &&
                  ((brush->flag & BRUSH_FRONTFACE_FALLOFF) == 0 ||
                   view_angle_limits_apply_falloff(
                       &vpd->normal_angle_precalc, angle_cos, &brush_strength))) {
                const float brush_fade = BKE_brush_curve_strength(
                    brush, sqrtf(test.dist), cache->radius);

                bool do_color = false;
                /* Minimum dot product between brush direction and current
                 * to neighbor direction is 0.0, meaning orthogonal. */
                float stroke_dot_max = 0.0f;

                /* Get the color of the loop in the opposite
                 * direction of the brush movement */
                Color color_final(0, 0, 0, 0);

                for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
                  const int p_index = gmap->vert_to_poly[v_index].indices[j];
                  const int l_index = gmap->vert_to_loop[v_index].indices[j];
                  BLI_assert(ss->mloop[l_index].v == v_index);
                  UNUSED_VARS_NDEBUG(l_index);
                  const MPoly *mp = &ss->mpoly[p_index];
                  if (!use_face_sel || select_poly[p_index]) {
                    const MLoop *ml_other = &ss->mloop[mp->loopstart];
                    for (int k = 0; k < mp->totloop; k++, ml_other++) {
                      const uint v_other_index = ml_other->v;
                      if (v_other_index != v_index) {
                        const float3 &mv_other = &ss->vert_positions[v_other_index];

                        /* Get the direction from the
                         * selected vert to the neighbor. */
                        float other_dir[3];
                        sub_v3_v3v3(other_dir, mv_curr, mv_other);
                        project_plane_v3_v3v3(other_dir, other_dir, cache->view_normal);

                        normalize_v3(other_dir);

                        const float stroke_dot = dot_v3v3(other_dir, brush_dir);
                        int elem_index;

                        if constexpr (domain == ATTR_DOMAIN_POINT) {
                          elem_index = ml_other->v;
                        }
                        else {
                          elem_index = mp->loopstart + k;
                        }

                        if (stroke_dot > stroke_dot_max) {
                          stroke_dot_max = stroke_dot;
                          color_final = color_prev_smear[elem_index];
                          do_color = true;
                        }
                      }
                    }
                  }
                }

                if (do_color) {
                  const float final_alpha = Traits::range * brush_fade * brush_strength *
                                            brush_alpha_pressure * grid_alpha;

                  /* For each poly owning this vert,
                   * paint each loop belonging to this vert. */
                  for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
                    const int p_index = gmap->vert_to_poly[v_index].indices[j];

                    int elem_index;
                    if constexpr (domain == ATTR_DOMAIN_POINT) {
                      elem_index = v_index;
                    }
                    else {
                      const int l_index = gmap->vert_to_loop[v_index].indices[j];
                      elem_index = l_index;
                      BLI_assert(ss->mloop[l_index].v == v_index);
                    }

                    if (!use_face_sel || select_poly[p_index]) {
                      /* Get the previous element color */
                      Color color_orig(0, 0, 0, 0); /* unused when array is nullptr */

                      if (color_prev != nullptr) {
                        /* Get the previous element color */
                        if (isZero(color_prev[elem_index])) {
                          color_prev[elem_index] = lcol[elem_index];
                        }
                        color_orig = color_prev[elem_index];
                      }
                      /* Mix the new color with the original
                       * based on the brush strength and the curve. */
                      lcol[elem_index] = vpaint_blend<Color, Traits>(vp,
                                                                     lcol[elem_index],
                                                                     color_orig,
                                                                     color_final,
                                                                     final_alpha,
                                                                     Traits::range *
                                                                         brush_strength);

                      color_curr[elem_index] = lcol[elem_index];
                    }
                  }
                }
              }
            }
          }
        }
        BKE_pbvh_vertex_iter_end;
      }
    }
  });
}

template<typename Color, typename Traits, eAttrDomain domain>
static void calculate_average_color(VPaintData<Color, Traits, domain> *vpd,
                                    Object *ob,
                                    Mesh *me,
                                    const Brush *brush,
                                    Color *lcol,
                                    PBVHNode **nodes,
                                    int totnode)
{
  using Blend = typename Traits::BlendType;

  const blender::VArray<bool> select_vert = me->attributes().lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);

  VPaintAverageAccum<Blend> *accum = (VPaintAverageAccum<Blend> *)MEM_mallocN(
      sizeof(*accum) * totnode, __func__);
  blender::threading::parallel_for(IndexRange(totnode), 1LL, [&](IndexRange range) {
    for (int n : range) {
      SculptSession *ss = ob->sculpt;
      const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
      const bool has_grids = (pbvh_type == PBVH_GRIDS);
      const SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;

      StrokeCache *cache = ss->cache;
      const bool use_vert_sel = (me->editflag &
                                 (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) != 0;

      VPaintAverageAccum<Blend> *accum2 = accum + n;
      accum2->len = 0;
      memset(accum2->value, 0, sizeof(accum2->value));

      SculptBrushTest test;
      SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
          ss, &test, brush->falloff_shape);

      /* For each vertex */
      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin (ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
        /* Test to see if the vertex coordinates are within the spherical brush region. */
        if (sculpt_brush_test_sq_fn(&test, vd.co)) {
          const int v_index = has_grids ? ss->mloop[vd.grid_indices[vd.g]].v :
                                          vd.vert_indices[vd.i];
          if (BKE_brush_curve_strength(brush, 0.0, cache->radius) > 0.0) {
            /* If the vertex is selected for painting. */
            if (!use_vert_sel || select_vert[v_index]) {
              accum2->len += gmap->vert_to_loop[v_index].count;
              /* if a vertex is within the brush region, then add its color to the blend. */
              for (int j = 0; j < gmap->vert_to_loop[v_index].count; j++) {
                int elem_index;

                if constexpr (domain == ATTR_DOMAIN_CORNER) {
                  elem_index = gmap->vert_to_loop[v_index].indices[j];
                }
                else {
                  elem_index = v_index;
                }

                Color *col = lcol + elem_index;

                /* Color is squared to compensate the sqrt color encoding. */
                accum2->value[0] += col->r * col->r;
                accum2->value[1] += col->g * col->g;
                accum2->value[2] += col->b * col->b;
              }
            }
          }
        }
      }
      BKE_pbvh_vertex_iter_end;
    }
  });

  Blend accum_len = 0;
  Blend accum_value[3] = {0};
  Color blend(0, 0, 0, 0);

  for (int i = 0; i < totnode; i++) {
    accum_len += accum[i].len;
    accum_value[0] += accum[i].value[0];
    accum_value[1] += accum[i].value[1];
    accum_value[2] += accum[i].value[2];
  }
  if (accum_len != 0) {
    blend.r = Traits::round(sqrtf(Traits::divide_round(accum_value[0], accum_len)));
    blend.g = Traits::round(sqrtf(Traits::divide_round(accum_value[1], accum_len)));
    blend.b = Traits::round(sqrtf(Traits::divide_round(accum_value[2], accum_len)));
    blend.a = Traits::range;

    vpd->paintcol = blend;
  }
}

template<typename Color, typename Traits, eAttrDomain domain>
static float paint_and_tex_color_alpha(VPaint *vp,
                                       VPaintData<Color, Traits, domain> *vpd,
                                       const float v_co[3],
                                       Color *r_color)
{
  ColorPaint4f rgba;
  ColorPaint4f rgba_br = toFloat(*r_color);

  paint_and_tex_color_alpha_intern(vp, &vpd->vc, v_co, &rgba.r);

  rgb_uchar_to_float(&rgba_br.r, (const uchar *)&vpd->paintcol);
  mul_v3_v3(rgba_br, rgba);

  *r_color = fromFloat<Color>(rgba_br);
  return rgba[3];
}

template<typename Color, typename Traits, eAttrDomain domain>
static void vpaint_do_draw(bContext *C,
                           Sculpt * /*sd*/,
                           VPaint *vp,
                           VPaintData<Color, Traits, domain> *vpd,
                           Object *ob,
                           Mesh *me,
                           PBVHNode **nodes,
                           int totnode,
                           Color *lcol)
{
  SculptSession *ss = ob->sculpt;
  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);

  const Brush *brush = ob->sculpt->cache->brush;
  const Scene *scene = CTX_data_scene(C);

  Color *previous_color = static_cast<Color *>(ss->cache->prev_colors_vpaint);

  const blender::VArray<bool> select_vert = me->attributes().lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);
  const blender::VArray<bool> select_poly = me->attributes().lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);

  blender::threading::parallel_for(IndexRange(totnode), 1LL, [&](IndexRange range) {
    for (int n : range) {
      const bool has_grids = (pbvh_type == PBVH_GRIDS);
      const SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;

      const StrokeCache *cache = ss->cache;
      float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
      get_brush_alpha_data(
          scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
      const bool use_normal = vwpaint_use_normal(vp);
      const bool use_vert_sel = (me->editflag &
                                 (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) != 0;
      const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

      SculptBrushTest test;
      SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
          ss, &test, brush->falloff_shape);
      const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
          ss, brush->falloff_shape);

      Color paintcol = vpd->paintcol;

      /* For each vertex */
      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin (ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
        /* Test to see if the vertex coordinates are within the spherical brush region. */
        if (sculpt_brush_test_sq_fn(&test, vd.co)) {
          /* NOTE: Grids are 1:1 with corners (aka loops).
           * For grid based pbvh, take the vert whose loop corresponds to the current grid.
           * Otherwise, take the current vert. */
          const int v_index = has_grids ? ss->mloop[vd.grid_indices[vd.g]].v :
                                          vd.vert_indices[vd.i];
          const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;

          /* If the vertex is selected for painting. */
          if (!use_vert_sel || select_vert[v_index]) {
            /* Calc the dot prod. between ray norm on surf and current vert
             * (ie splash prevention factor), and only paint front facing verts. */
            float brush_strength = cache->bstrength;
            const float angle_cos = (use_normal && vd.no) ?
                                        dot_v3v3(sculpt_normal_frontface, vd.no) :
                                        1.0f;
            if (((brush->flag & BRUSH_FRONTFACE) == 0 || (angle_cos > 0.0f)) &&
                ((brush->flag & BRUSH_FRONTFACE_FALLOFF) == 0 ||
                 view_angle_limits_apply_falloff(
                     &vpd->normal_angle_precalc, angle_cos, &brush_strength))) {
              const float brush_fade = BKE_brush_curve_strength(
                  brush, sqrtf(test.dist), cache->radius);

              Color color_final = paintcol;

              /* If we're painting with a texture, sample the texture color and alpha. */
              float tex_alpha = 1.0;
              if (vpd->is_texbrush) {
                /* NOTE: we may want to paint alpha as vertex color alpha. */
                tex_alpha = paint_and_tex_color_alpha<Color, Traits, domain>(
                    vp, vpd, vpd->vertexcosnos[v_index].co, &color_final);
              }

              Color color_orig(0, 0, 0, 0);

              if constexpr (domain == ATTR_DOMAIN_POINT) {
                int v_index = vd.index;

                if (previous_color != nullptr) {
                  /* Get the previous loop color */
                  if (isZero(previous_color[v_index])) {
                    previous_color[v_index] = lcol[v_index];
                  }
                  color_orig = previous_color[v_index];
                }
                const float final_alpha = Traits::frange * brush_fade * brush_strength *
                                          tex_alpha * brush_alpha_pressure * grid_alpha;

                lcol[v_index] = vpaint_blend<Color, Traits>(vp,
                                                            lcol[v_index],
                                                            color_orig,
                                                            color_final,
                                                            final_alpha,
                                                            Traits::range * brush_strength);
              }
              else {
                /* For each poly owning this vert, paint each loop belonging to this vert. */
                for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
                  const int p_index = gmap->vert_to_poly[v_index].indices[j];
                  const int l_index = gmap->vert_to_loop[v_index].indices[j];
                  BLI_assert(ss->mloop[l_index].v == v_index);
                  if (!use_face_sel || select_poly[p_index]) {
                    Color color_orig = Color(0, 0, 0, 0); /* unused when array is nullptr */

                    if (previous_color != nullptr) {
                      /* Get the previous loop color */
                      if (isZero(previous_color[l_index])) {
                        previous_color[l_index] = lcol[l_index];
                      }
                      color_orig = previous_color[l_index];
                    }
                    const float final_alpha = Traits::frange * brush_fade * brush_strength *
                                              tex_alpha * brush_alpha_pressure * grid_alpha;

                    /* Mix the new color with the original based on final_alpha. */
                    lcol[l_index] = vpaint_blend<Color, Traits>(vp,
                                                                lcol[l_index],
                                                                color_orig,
                                                                color_final,
                                                                final_alpha,
                                                                Traits::range * brush_strength);
                  }
                }
              }
            }
          }
        }
      }
      BKE_pbvh_vertex_iter_end;
    }
  });
}

template<typename Color, typename Traits, eAttrDomain domain>
static void vpaint_do_blur(bContext *C,
                           Sculpt *sd,
                           VPaint *vp,
                           VPaintData<Color, Traits, domain> *vpd,
                           Object *ob,
                           Mesh *me,
                           PBVHNode **nodes,
                           int totnode,
                           Color *lcol)
{
  if constexpr (domain == ATTR_DOMAIN_POINT) {
    do_vpaint_brush_blur_verts<Color, Traits>(C, sd, vp, vpd, ob, me, nodes, totnode, lcol);
  }
  else {
    do_vpaint_brush_blur_loops<Color, Traits>(C, sd, vp, vpd, ob, me, nodes, totnode, lcol);
  }
}

template<typename Color, typename Traits, eAttrDomain domain>
static void vpaint_paint_leaves(bContext *C,
                                Sculpt *sd,
                                VPaint *vp,
                                VPaintData<Color, Traits, domain> *vpd,
                                Object *ob,
                                Mesh *me,
                                Color *lcol,
                                PBVHNode **nodes,
                                int totnode)
{

  for (int i : IndexRange(totnode)) {
    SCULPT_undo_push_node(ob, nodes[i], SCULPT_UNDO_COLOR);
  }

  const Brush *brush = ob->sculpt->cache->brush;

  switch ((eBrushVertexPaintTool)brush->vertexpaint_tool) {
    case VPAINT_TOOL_AVERAGE:
      calculate_average_color<Color, Traits, domain>(vpd, ob, me, brush, lcol, nodes, totnode);
      break;
    case VPAINT_TOOL_DRAW:
      vpaint_do_draw<Color, Traits, domain>(C, sd, vp, vpd, ob, me, nodes, totnode, lcol);
      break;
    case VPAINT_TOOL_BLUR:
      vpaint_do_blur<Color, Traits, domain>(C, sd, vp, vpd, ob, me, nodes, totnode, lcol);
      break;
    case VPAINT_TOOL_SMEAR:
      do_vpaint_brush_smear<Color, Traits, domain>(C, sd, vp, vpd, ob, me, nodes, totnode, lcol);
      break;
    default:
      break;
  }
}

template<typename Color, typename Traits, eAttrDomain domain>
static void vpaint_do_paint(bContext *C,
                            Sculpt *sd,
                            VPaint *vp,
                            VPaintData<Color, Traits, domain> *vpd,
                            Object *ob,
                            Mesh *me,
                            Brush *brush,
                            const ePaintSymmetryFlags symm,
                            const int axis,
                            const int i,
                            const float angle)
{
  SculptSession *ss = ob->sculpt;
  ss->cache->radial_symmetry_pass = i;
  SCULPT_cache_calc_brushdata_symm(ss->cache, symm, axis, angle);

  int totnode;
  PBVHNode **nodes = vwpaint_pbvh_gather_generic(ob, vp, sd, brush, &totnode);

  bke::GSpanAttributeWriter attribute = me->attributes_for_write().lookup_for_write_span(
      me->active_color_attribute);
  BLI_assert(attribute.domain == domain);

  Color *color_data = static_cast<Color *>(attribute.span.data());

  /* Paint those leaves. */
  vpaint_paint_leaves<Color, Traits, domain>(C, sd, vp, vpd, ob, me, color_data, nodes, totnode);

  attribute.finish();
  if (nodes) {
    MEM_freeN(nodes);
  }
}

template<typename Color, typename Traits, eAttrDomain domain>
static void vpaint_do_radial_symmetry(bContext *C,
                                      Sculpt *sd,
                                      VPaint *vp,
                                      VPaintData<Color, Traits, domain> *vpd,
                                      Object *ob,
                                      Mesh *me,
                                      Brush *brush,
                                      const ePaintSymmetryFlags symm,
                                      const int axis)
{
  for (int i = 1; i < vp->radial_symm[axis - 'X']; i++) {
    const float angle = (2.0 * M_PI) * i / vp->radial_symm[axis - 'X'];
    vpaint_do_paint<Color, Traits, domain>(C, sd, vp, vpd, ob, me, brush, symm, axis, i, angle);
  }
}

/* near duplicate of: sculpt.cc's,
 * 'do_symmetrical_brush_actions' and 'wpaint_do_symmetrical_brush_actions'. */
template<typename Color, typename Traits, eAttrDomain domain>
static void vpaint_do_symmetrical_brush_actions(
    bContext *C, Sculpt *sd, VPaint *vp, VPaintData<Color, Traits, domain> *vpd, Object *ob)
{
  Brush *brush = BKE_paint_brush(&vp->paint);
  Mesh *me = (Mesh *)ob->data;
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  int i = 0;

  /* initial stroke */
  const ePaintSymmetryFlags initial_symm = ePaintSymmetryFlags(0);
  cache->mirror_symmetry_pass = ePaintSymmetryFlags(0);
  vpaint_do_paint<Color, Traits, domain>(C, sd, vp, vpd, ob, me, brush, initial_symm, 'X', 0, 0);
  vpaint_do_radial_symmetry<Color, Traits, domain>(
      C, sd, vp, vpd, ob, me, brush, initial_symm, 'X');
  vpaint_do_radial_symmetry<Color, Traits, domain>(
      C, sd, vp, vpd, ob, me, brush, initial_symm, 'Y');
  vpaint_do_radial_symmetry<Color, Traits, domain>(
      C, sd, vp, vpd, ob, me, brush, initial_symm, 'Z');

  cache->symmetry = symm;

  /* symm is a bit combination of XYZ - 1 is mirror
   * X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  for (i = 1; i <= symm; i++) {
    if (symm & i && (symm != 5 || i != 3) && (symm != 6 || !ELEM(i, 3, 5))) {
      const ePaintSymmetryFlags symm_pass = ePaintSymmetryFlags(i);
      cache->mirror_symmetry_pass = symm_pass;
      cache->radial_symmetry_pass = 0;
      SCULPT_cache_calc_brushdata_symm(cache, symm_pass, 0, 0);

      if (i & (1 << 0)) {
        vpaint_do_paint<Color, Traits, domain>(
            C, sd, vp, vpd, ob, me, brush, symm_pass, 'X', 0, 0);
        vpaint_do_radial_symmetry<Color, Traits, domain>(
            C, sd, vp, vpd, ob, me, brush, symm_pass, 'X');
      }
      if (i & (1 << 1)) {
        vpaint_do_paint<Color, Traits, domain>(
            C, sd, vp, vpd, ob, me, brush, symm_pass, 'Y', 0, 0);
        vpaint_do_radial_symmetry<Color, Traits, domain>(
            C, sd, vp, vpd, ob, me, brush, symm_pass, 'Y');
      }
      if (i & (1 << 2)) {
        vpaint_do_paint<Color, Traits, domain>(
            C, sd, vp, vpd, ob, me, brush, symm_pass, 'Z', 0, 0);
        vpaint_do_radial_symmetry<Color, Traits, domain>(
            C, sd, vp, vpd, ob, me, brush, symm_pass, 'Z');
      }
    }
  }

  copy_v3_v3(cache->true_last_location, cache->true_location);
  cache->is_last_valid = true;
}

template<typename Color, typename Traits, eAttrDomain domain>
static void vpaint_stroke_update_step_intern(bContext *C, PaintStroke *stroke, PointerRNA *itemptr)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  VPaintData<Color, Traits, domain> *vpd = static_cast<VPaintData<Color, Traits, domain> *>(
      paint_stroke_mode_data(stroke));
  VPaint *vp = ts->vpaint;
  ViewContext *vc = &vpd->vc;
  Object *ob = vc->obact;
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  vwpaint_update_cache_variants(C, vp, ob, itemptr);

  float mat[4][4];

  ED_view3d_init_mats_rv3d(ob, vc->rv3d);

  /* load projection matrix */
  mul_m4_m4m4(mat, vc->rv3d->persmat, ob->object_to_world);

  swap_m4m4(vc->rv3d->persmat, mat);

  vpaint_do_symmetrical_brush_actions<Color, Traits, domain>(C, sd, vp, vpd, ob);

  swap_m4m4(vc->rv3d->persmat, mat);

  BKE_mesh_batch_cache_dirty_tag((Mesh *)ob->data, BKE_MESH_BATCH_DIRTY_ALL);

  if (vp->paint.brush->vertexpaint_tool == VPAINT_TOOL_SMEAR) {
    Mesh *me = BKE_object_get_original_mesh(ob);

    size_t elem_size;
    int elem_num;

    elem_num = get_vcol_elements(me, &elem_size);
    memcpy(vpd->smear.color_prev, vpd->smear.color_curr, elem_size * elem_num);
  }

  /* Calculate pivot for rotation around selection if needed.
   * also needed for "Frame Selected" on last stroke. */
  float loc_world[3];
  mul_v3_m4v3(loc_world, ob->object_to_world, ss->cache->true_location);
  paint_last_stroke_update(scene, loc_world);

  ED_region_tag_redraw(vc->region);

  DEG_id_tag_update((ID *)ob->data, ID_RECALC_GEOMETRY);
}

static void vpaint_stroke_update_step(bContext *C,
                                      wmOperator * /*op*/,
                                      PaintStroke *stroke,
                                      PointerRNA *itemptr)
{
  VPaintDataBase *vpd = static_cast<VPaintDataBase *>(paint_stroke_mode_data(stroke));

  if (vpd->domain == ATTR_DOMAIN_POINT) {
    if (vpd->type == CD_PROP_COLOR) {
      vpaint_stroke_update_step_intern<ColorPaint4f, FloatTraits, ATTR_DOMAIN_POINT>(
          C, stroke, itemptr);
    }
    else if (vpd->type == CD_PROP_BYTE_COLOR) {
      vpaint_stroke_update_step_intern<ColorPaint4b, ByteTraits, ATTR_DOMAIN_POINT>(
          C, stroke, itemptr);
    }
  }
  else if (vpd->domain == ATTR_DOMAIN_CORNER) {
    if (vpd->type == CD_PROP_COLOR) {
      vpaint_stroke_update_step_intern<ColorPaint4f, FloatTraits, ATTR_DOMAIN_CORNER>(
          C, stroke, itemptr);
    }
    else if (vpd->type == CD_PROP_BYTE_COLOR) {
      vpaint_stroke_update_step_intern<ColorPaint4b, ByteTraits, ATTR_DOMAIN_CORNER>(
          C, stroke, itemptr);
    }
  }
}

template<typename Color, typename Traits, eAttrDomain domain>
static void vpaint_free_vpaintdata(Object * /*ob*/, void *_vpd)
{
  VPaintData<Color, Traits, domain> *vpd = static_cast<VPaintData<Color, Traits, domain> *>(_vpd);

  if (vpd->is_texbrush) {
    ED_vpaint_proj_handle_free(vpd->vp_handle);
  }

  if (vpd->smear.color_prev) {
    MEM_freeN(vpd->smear.color_prev);
  }
  if (vpd->smear.color_curr) {
    MEM_freeN(vpd->smear.color_curr);
  }

  MEM_delete<VPaintData<Color, Traits, domain>>(vpd);
}

static void vpaint_stroke_done(const bContext *C, PaintStroke *stroke)
{
  void *vpd_ptr = paint_stroke_mode_data(stroke);
  VPaintDataBase *vpd = static_cast<VPaintDataBase *>(vpd_ptr);

  ViewContext *vc = &vpd->vc;
  Object *ob = vc->obact;

  if (vpd->domain == ATTR_DOMAIN_POINT) {
    if (vpd->type == CD_PROP_COLOR) {
      vpaint_free_vpaintdata<ColorPaint4f, FloatTraits, ATTR_DOMAIN_POINT>(ob, vpd);
    }
    else if (vpd->type == CD_PROP_BYTE_COLOR) {
      vpaint_free_vpaintdata<ColorPaint4b, ByteTraits, ATTR_DOMAIN_POINT>(ob, vpd);
    }
  }
  else if (vpd->domain == ATTR_DOMAIN_CORNER) {
    if (vpd->type == CD_PROP_COLOR) {
      vpaint_free_vpaintdata<ColorPaint4f, FloatTraits, ATTR_DOMAIN_CORNER>(ob, vpd);
    }
    else if (vpd->type == CD_PROP_BYTE_COLOR) {
      vpaint_free_vpaintdata<ColorPaint4b, ByteTraits, ATTR_DOMAIN_CORNER>(ob, vpd);
    }
  }

  SculptSession *ss = ob->sculpt;

  if (ss->cache->alt_smooth) {
    ToolSettings *ts = CTX_data_tool_settings(C);
    VPaint *vp = ts->vpaint;
    smooth_brush_toggle_off(C, &vp->paint, ss->cache);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  SCULPT_undo_push_end(ob);

  SCULPT_cache_free(ob->sculpt->cache);
  ob->sculpt->cache = nullptr;
}

static int vpaint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;

  op->customdata = paint_stroke_new(C,
                                    op,
                                    SCULPT_stroke_get_location,
                                    vpaint_stroke_test_start,
                                    vpaint_stroke_update_step,
                                    nullptr,
                                    vpaint_stroke_done,
                                    event->type);

  Object *ob = CTX_data_active_object(C);

  if (SCULPT_has_loop_colors(ob) && ob->sculpt->pbvh) {
    BKE_pbvh_ensure_node_loops(ob->sculpt->pbvh);
  }

  SCULPT_undo_push_begin_ex(ob, "Vertex Paint");

  if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
    paint_stroke_free(C, op, (PaintStroke *)op->customdata);
    return OPERATOR_FINISHED;
  }

  /* add modal handler */
  WM_event_add_modal_handler(C, op);

  OPERATOR_RETVAL_CHECK(retval);
  BLI_assert(retval == OPERATOR_RUNNING_MODAL);

  return OPERATOR_RUNNING_MODAL;
}

static int vpaint_exec(bContext *C, wmOperator *op)
{
  op->customdata = paint_stroke_new(C,
                                    op,
                                    SCULPT_stroke_get_location,
                                    vpaint_stroke_test_start,
                                    vpaint_stroke_update_step,
                                    nullptr,
                                    vpaint_stroke_done,
                                    0);

  /* frees op->customdata */
  paint_stroke_exec(C, op, (PaintStroke *)op->customdata);

  return OPERATOR_FINISHED;
}

static void vpaint_cancel(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  if (ob->sculpt->cache) {
    SCULPT_cache_free(ob->sculpt->cache);
    ob->sculpt->cache = nullptr;
  }

  paint_stroke_cancel(C, op, (PaintStroke *)op->customdata);
}

static int vpaint_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, (PaintStroke **)&op->customdata);
}

void PAINT_OT_vertex_paint(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint";
  ot->idname = "PAINT_OT_vertex_paint";
  ot->description = "Paint a stroke in the active color attribute layer";

  /* api callbacks */
  ot->invoke = vpaint_invoke;
  ot->modal = vpaint_modal;
  ot->exec = vpaint_exec;
  ot->poll = vertex_paint_poll;
  ot->cancel = vpaint_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

  paint_stroke_operator_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Vertex Colors Operator
 * \{ */

template<typename T>
static void fill_bm_face_or_corner_attribute(BMesh &bm,
                                             const T &value,
                                             const eAttrDomain domain,
                                             const int cd_offset,
                                             const bool use_vert_sel)
{
  BMFace *f;
  BMIter iter;
  BM_ITER_MESH (f, &iter, &bm, BM_FACES_OF_MESH) {
    BMLoop *l = f->l_first;
    do {
      if (!(use_vert_sel && !BM_elem_flag_test(l->v, BM_ELEM_SELECT))) {
        if (domain == ATTR_DOMAIN_CORNER) {
          *static_cast<T *>(BM_ELEM_CD_GET_VOID_P(l, cd_offset)) = value;
        }
        else if (domain == ATTR_DOMAIN_POINT) {
          *static_cast<T *>(BM_ELEM_CD_GET_VOID_P(l->v, cd_offset)) = value;
        }
      }
    } while ((l = l->next) != f->l_first);
  }
}

template<typename T>
static void fill_mesh_face_or_corner_attribute(Mesh &mesh,
                                               const T &value,
                                               const eAttrDomain domain,
                                               const MutableSpan<T> data,
                                               const bool use_vert_sel,
                                               const bool use_face_sel)
{
  const VArray<bool> select_vert = mesh.attributes().lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);
  const VArray<bool> select_poly = mesh.attributes().lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);

  const Span<MPoly> polys = mesh.polys();
  const Span<MLoop> loops = mesh.loops();

  for (const int i : polys.index_range()) {
    if (use_face_sel && !select_poly[i]) {
      continue;
    }
    const MPoly &poly = polys[i];

    int j = 0;
    do {
      uint vidx = loops[poly.loopstart + j].v;

      if (!(use_vert_sel && !(select_vert[vidx]))) {
        if (domain == ATTR_DOMAIN_CORNER) {
          data[poly.loopstart + j] = value;
        }
        else {
          data[vidx] = value;
        }
      }
      j++;
    } while (j < poly.totloop);
  }

  BKE_mesh_tessface_clear(&mesh);
}

static void fill_mesh_color(Mesh &mesh,
                            const ColorPaint4f &color,
                            const StringRef attribute_name,
                            const bool use_vert_sel,
                            const bool use_face_sel)
{
  if (mesh.edit_mesh) {
    BMesh *bm = mesh.edit_mesh->bm;
    const std::string name = attribute_name;
    const CustomDataLayer *layer = BKE_id_attributes_color_find(&mesh.id, name.c_str());
    const eAttrDomain domain = BKE_id_attribute_domain(&mesh.id, layer);
    if (layer->type == CD_PROP_COLOR) {
      fill_bm_face_or_corner_attribute<ColorPaint4f>(
          *bm, color, domain, layer->offset, use_vert_sel);
    }
    else if (layer->type == CD_PROP_BYTE_COLOR) {
      fill_bm_face_or_corner_attribute<ColorPaint4b>(
          *bm, color.encode(), domain, layer->offset, use_vert_sel);
    }
  }
  else {
    bke::GSpanAttributeWriter attribute = mesh.attributes_for_write().lookup_for_write_span(
        attribute_name);
    if (attribute.span.type().is<ColorGeometry4f>()) {
      fill_mesh_face_or_corner_attribute<ColorPaint4f>(
          mesh,
          color,
          attribute.domain,
          attribute.span.typed<ColorGeometry4f>().cast<ColorPaint4f>(),
          use_vert_sel,
          use_face_sel);
    }
    else if (attribute.span.type().is<ColorGeometry4b>()) {
      fill_mesh_face_or_corner_attribute<ColorPaint4b>(
          mesh,
          color.encode(),
          attribute.domain,
          attribute.span.typed<ColorGeometry4b>().cast<ColorPaint4b>(),
          use_vert_sel,
          use_face_sel);
    }
    attribute.finish();
  }
}

/**
 * See doc-string for #BKE_object_attributes_active_color_fill.
 */
static bool paint_object_attributes_active_color_fill_ex(Object *ob,
                                                         ColorPaint4f fill_color,
                                                         bool only_selected = true)
{
  Mesh *me = BKE_mesh_from_object(ob);
  if (!me) {
    return false;
  }

  const bool use_face_sel = only_selected ? (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0 : false;
  const bool use_vert_sel = only_selected ? (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0 : false;
  fill_mesh_color(*me, fill_color, me->active_color_attribute, use_vert_sel, use_face_sel);

  DEG_id_tag_update(&me->id, ID_RECALC_COPY_ON_WRITE);

  /* NOTE: Original mesh is used for display, so tag it directly here. */
  BKE_mesh_batch_cache_dirty_tag(me, BKE_MESH_BATCH_DIRTY_ALL);

  return true;
}

bool BKE_object_attributes_active_color_fill(Object *ob,
                                             const float fill_color[4],
                                             bool only_selected)
{
  return paint_object_attributes_active_color_fill_ex(ob, ColorPaint4f(fill_color), only_selected);
}

static int vertex_color_set_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  Object *obact = CTX_data_active_object(C);

  // uint paintcol = vpaint_get_current_color(scene, scene->toolsettings->vpaint, false);
  ColorPaint4f paintcol = vpaint_get_current_col<ColorPaint4f, FloatTraits, ATTR_DOMAIN_POINT>(
      scene, scene->toolsettings->vpaint, false);

  if (paint_object_attributes_active_color_fill_ex(obact, paintcol)) {
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obact);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_vertex_color_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Vertex Colors";
  ot->idname = "PAINT_OT_vertex_color_set";
  ot->description = "Fill the active vertex color layer with the current paint color";

  /* api callbacks */
  ot->exec = vertex_color_set_exec;
  ot->poll = vertex_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
