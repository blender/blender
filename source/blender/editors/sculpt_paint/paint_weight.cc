/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 *
 * Used for vertex color & weight paint and mode switching.
 *
 * \note This file is already big,
 * use `paint_vertex_color_ops.cc` & `paint_vertex_weight_ops.cc` for general purpose operators.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.h"
#include "BLI_color.hh"
#include "BLI_color_mix.hh"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"
#include "BKE_paint.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "ED_image.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

/* For IMB_BlendMode only. */
#include "IMB_imbuf.hh"

#include "BKE_ccg.h"
#include "bmesh.hh"

#include "paint_intern.hh" /* own include */
#include "sculpt_intern.hh"

using namespace blender;
using namespace blender::ed::sculpt_paint;
using blender::ed::sculpt_paint::vwpaint::NormalAnglePrecalc;

struct WPaintAverageAccum {
  uint len;
  double value;
};

/**
 * Variables stored both for 'active' and 'mirror' sides.
 */
struct WeightPaintGroupData {
  /**
   * Index of active group or its mirror:
   *
   * - 'active' is always `ob->actdef`.
   * - 'mirror' is -1 when 'ME_EDIT_MIRROR_X' flag id disabled,
   *   otherwise this will be set to the mirror or the active group (if the group isn't mirrored).
   */
  int index;
  /**
   * Lock that includes the 'index' as locked too:
   *
   * - 'active' is set of locked or active/selected groups.
   * - 'mirror' is set of locked or mirror groups.
   */
  const bool *lock;
};

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

/**
 * VPaint has 'vpaint_blend'
 */
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

static float wpaint_clamp_monotonic(float oldval, float curval, float newval)
{
  if (newval < oldval) {
    return std::min(newval, curval);
  }
  if (newval > oldval) {
    return std::max(newval, curval);
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
  Mesh *mesh = (Mesh *)ob->data;
  MDeformVert *dv = &wpi->dvert[index];
  bool topology = (mesh->editflag & ME_EDIT_MIRROR_TOPO) != 0;

  MDeformWeight *dw;
  float weight_prev, weight_cur;
  float dw_rel_locked = 0.0f, dw_rel_free = 1.0f;

  /* mirror vars */
  int index_mirr;
  int vgroup_mirr;

  MDeformVert *dv_mirr;
  MDeformWeight *dw_mirr;

  /* Check if we should mirror vertex groups (X-axis). */
  if (ME_USING_MIRROR_X_VERTEX_GROUPS(mesh)) {
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

  if (!vwpaint::brush_use_accumulate(wp)) {
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
     * can give wrong results #26193, least confusing if normalize is done last */

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
  Mesh *mesh = (Mesh *)ob->data;
  MDeformVert *dv = &wpi->dvert[index];
  bool topology = (mesh->editflag & ME_EDIT_MIRROR_TOPO) != 0;

  /* mirror vars */
  int index_mirr = -1;
  MDeformVert *dv_mirr = nullptr;

  /* weights */
  float curw, curw_real, oldw, neww, change, curw_mirr, change_mirr;
  float dw_rel_free, dw_rel_locked;

  /* Check if we should mirror vertex groups (X-axis). */
  if (ME_USING_MIRROR_X_VERTEX_GROUPS(mesh)) {
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

  if (!vwpaint::brush_use_accumulate(wp)) {
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

static bool wpaint_stroke_test_start(bContext *C, wmOperator *op, const float mouse[2])
{
  Scene *scene = CTX_data_scene(C);
  PaintStroke *stroke = (PaintStroke *)op->customdata;
  ToolSettings *ts = scene->toolsettings;
  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = BKE_mesh_from_object(ob);
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
    dg = (bDeformGroup *)BLI_findlink(&mesh->vertex_group_names, vgroup_index.active);
    if (dg->flag & DG_LOCK_WEIGHT) {
      BKE_report(op->reports, RPT_WARNING, "Active group is locked, aborting");
      return false;
    }
    if (vgroup_index.mirror != -1) {
      dg = (bDeformGroup *)BLI_findlink(&mesh->vertex_group_names, vgroup_index.mirror);
      if (dg->flag & DG_LOCK_WEIGHT) {
        BKE_report(op->reports, RPT_WARNING, "Mirror group is locked, aborting");
        return false;
      }
    }
  }

  /* check that multipaint groups are unlocked */
  defbase_tot = BLI_listbase_count(&mesh->vertex_group_names);
  defbase_sel = BKE_object_defgroup_selected_get(ob, defbase_tot, &defbase_tot_sel);

  if (ts->multipaint && defbase_tot_sel > 1) {
    int i;
    bDeformGroup *dg;

    if (ME_USING_MIRROR_X_VERTEX_GROUPS(mesh)) {
      BKE_object_defgroup_mirror_selection(
          ob, defbase_tot, defbase_sel, defbase_sel, &defbase_tot_sel);
    }

    for (i = 0; i < defbase_tot; i++) {
      if (defbase_sel[i]) {
        dg = (bDeformGroup *)BLI_findlink(&mesh->vertex_group_names, i);
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
  wpd->vc = ED_view3d_viewcontext_init(C, depsgraph);
  vwpaint::view_angle_limits_init(&wpd->normal_angle_precalc,
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
      ts->wpaint_lock_relative)
  {
    wpd->vgroup_validmap = BKE_object_defgroup_validmap_get(ob, wpd->defbase_tot);
  }

  /* Compute the set of all locked deform groups when Lock Relative is active. */
  if (ts->wpaint_lock_relative &&
      BKE_object_defgroup_check_lock_relative(
          wpd->lock_flags, wpd->vgroup_validmap, wpd->active.index) &&
      (!wpd->do_multipaint || BKE_object_defgroup_check_lock_relative_multi(
                                  defbase_tot, wpd->lock_flags, defbase_sel, defbase_tot_sel)))
  {
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
  vwpaint::init_stroke(depsgraph, ob);
  vwpaint::update_cache_invariants(C, vp, ss, op, mouse);
  vwpaint::init_session_data(ts, ob);

  if (ELEM(vp->paint.brush->weightpaint_tool, WPAINT_TOOL_SMEAR, WPAINT_TOOL_BLUR)) {
    wpd->precomputed_weight = (float *)MEM_mallocN(sizeof(float) * mesh->verts_num, __func__);
  }

  if (ob->sculpt->mode.wpaint.dvert_prev != nullptr) {
    MDeformVert *dv = ob->sculpt->mode.wpaint.dvert_prev;
    for (int i = 0; i < mesh->verts_num; i++, dv++) {
      /* Use to show this isn't initialized, never apply to the mesh data. */
      dv->flag = 1;
    }
  }

  return true;
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

static void precompute_weight_values(
    Object *ob, Brush *brush, WPaintData *wpd, WeightPaintInfo *wpi, Mesh *mesh)
{
  using namespace blender;
  if (wpd->precomputed_weight_ready && !vwpaint::brush_use_accumulate_ex(brush, ob->mode)) {
    return;
  }

  threading::parallel_for(IndexRange(mesh->verts_num), 512, [&](const IndexRange range) {
    for (const int i : range) {
      const MDeformVert *dv = &wpi->dvert[i];
      wpd->precomputed_weight[i] = wpaint_get_active_weight(dv, wpi);
    }
  });

  wpd->precomputed_weight_ready = true;
}

/* -------------------------------------------------------------------- */
/** \name Weight paint brushes.
 * \{ */

static void do_wpaint_brush_blur_task(const Scene *scene,
                                      Object *ob,
                                      const Brush *brush,
                                      VPaint *vp,
                                      WPaintData *wpd,
                                      const WeightPaintInfo *wpi,
                                      Mesh *mesh,
                                      PBVHNode *node)
{
  using namespace blender;
  SculptSession *ss = ob->sculpt;
  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
  const bool has_grids = (pbvh_type == PBVH_GRIDS);
  const SculptVertexPaintGeomMap *gmap = &ss->mode.wpaint.gmap;

  const StrokeCache *cache = ss->cache;

  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
  vwpaint::get_brush_alpha_data(
      scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint::use_normal(vp);
  const bool use_face_sel = (mesh->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const bool use_vert_sel = (mesh->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);
  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, brush->falloff_shape);

  const blender::bke::AttributeAccessor attributes = mesh->attributes();
  const blender::VArray<bool> select_vert = *attributes.lookup_or_default<bool>(
      ".select_vert", bke::AttrDomain::Point, false);

  /* For each vertex */
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    /* Test to see if the vertex coordinates are within the spherical brush region. */
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    /* For grid based pbvh, take the vert whose loop corresponds to the current grid.
     * Otherwise, take the current vert. */
    const int v_index = has_grids ? ss->corner_verts[vd.grid_indices[vd.g]] :
                                    vd.vert_indices[vd.i];
    const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;
    /* If the vertex is selected */
    if ((use_face_sel || use_vert_sel) && !select_vert[v_index]) {
      continue;
    }
    /* Get the average face weight */
    int total_hit_loops = 0;
    float weight_final = 0.0f;
    for (const int p_index : gmap->vert_to_face[v_index]) {
      const blender::IndexRange face = ss->faces[p_index];

      total_hit_loops += face.size();
      for (const int vert : ss->corner_verts.slice(face)) {
        weight_final += wpd->precomputed_weight[vert];
      }
    }

    /* Apply the weight to the vertex. */
    if (total_hit_loops == 0) {
      continue;
    }

    float brush_strength = cache->bstrength;
    const float angle_cos = (use_normal && vd.no) ? dot_v3v3(sculpt_normal_frontface, vd.no) :
                                                    1.0f;
    if (!vwpaint::test_brush_angle_falloff(
            *brush, wpd->normal_angle_precalc, angle_cos, &brush_strength))
    {
      continue;
    }

    const float brush_fade = BKE_brush_curve_strength(brush, sqrtf(test.dist), cache->radius);
    const float final_alpha = brush_fade * brush_strength * grid_alpha * brush_alpha_pressure;

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
    do_weight_paint_vertex(vp, ob, wpi, v_index, final_alpha, weight_final);
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_wpaint_brush_smear_task(const Scene *scene,
                                       Object *ob,
                                       const Brush *brush,
                                       VPaint *vp,
                                       WPaintData *wpd,
                                       const WeightPaintInfo *wpi,
                                       Mesh *mesh,
                                       PBVHNode *node)
{
  using namespace blender;
  SculptSession *ss = ob->sculpt;
  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
  const bool has_grids = (pbvh_type == PBVH_GRIDS);
  const SculptVertexPaintGeomMap *gmap = &ss->mode.wpaint.gmap;

  const StrokeCache *cache = ss->cache;
  if (!cache->is_last_valid) {
    return;
  }

  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
  vwpaint::get_brush_alpha_data(
      scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint::use_normal(vp);
  const bool use_face_sel = (mesh->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const bool use_vert_sel = (mesh->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
  float brush_dir[3];

  sub_v3_v3v3(brush_dir, cache->location, cache->last_location);
  project_plane_v3_v3v3(brush_dir, brush_dir, cache->view_normal);
  if (normalize_v3(brush_dir) == 0.0f) {
    return;
  }

  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArray<bool> select_vert = *attributes.lookup_or_default<bool>(
      ".select_vert", bke::AttrDomain::Point, false);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);
  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, brush->falloff_shape);

  /* For each vertex */
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    /* Test to see if the vertex coordinates are within the spherical brush region. */
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    /* For grid based pbvh, take the vert whose loop corresponds to the current grid.
     * Otherwise, take the current vert. */
    const int v_index = has_grids ? ss->corner_verts[vd.grid_indices[vd.g]] :
                                    vd.vert_indices[vd.i];
    const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;
    const float3 &mv_curr = ss->vert_positions[v_index];

    /* If the vertex is selected */
    if ((use_face_sel || use_vert_sel) && !select_vert[v_index]) {
      continue;
    }

    float brush_strength = cache->bstrength;
    const float angle_cos = (use_normal && vd.no) ? dot_v3v3(sculpt_normal_frontface, vd.no) :
                                                    1.0f;
    if (!vwpaint::test_brush_angle_falloff(
            *brush, wpd->normal_angle_precalc, angle_cos, &brush_strength))
    {
      continue;
    }

    bool do_color = false;
    /* Minimum dot product between brush direction and current
     * to neighbor direction is 0.0, meaning orthogonal. */
    float stroke_dot_max = 0.0f;

    /* Get the color of the loop in the opposite direction of the brush movement
     * (this callback is specifically for smear.) */
    float weight_final = 0.0;
    for (const int p_index : gmap->vert_to_face[v_index]) {
      for (const int v_other_index : ss->corner_verts.slice(ss->faces[p_index])) {
        if (v_other_index == v_index) {
          continue;
        }

        /* Get the direction from the selected vert to the neighbor. */
        float other_dir[3];
        sub_v3_v3v3(other_dir, mv_curr, ss->vert_positions[v_other_index]);
        project_plane_v3_v3v3(other_dir, other_dir, cache->view_normal);

        normalize_v3(other_dir);

        const float stroke_dot = dot_v3v3(other_dir, brush_dir);

        if (stroke_dot > stroke_dot_max) {
          stroke_dot_max = stroke_dot;
          weight_final = wpd->precomputed_weight[v_other_index];
          do_color = true;
        }
      }
      /* Apply weight to vertex */
      if (!do_color) {
        continue;
      }
      const float brush_fade = BKE_brush_curve_strength(brush, sqrtf(test.dist), cache->radius);
      const float final_alpha = brush_fade * brush_strength * grid_alpha * brush_alpha_pressure;

      if (final_alpha <= 0.0f) {
        continue;
      }

      do_weight_paint_vertex(vp, ob, wpi, v_index, final_alpha, float(weight_final));
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_wpaint_brush_draw_task(const Scene *scene,
                                      Object *ob,
                                      const Brush *brush,
                                      VPaint *vp,
                                      WPaintData *wpd,
                                      const WeightPaintInfo *wpi,
                                      Mesh *mesh,
                                      const float strength,
                                      PBVHNode *node)
{
  using namespace blender;
  SculptSession *ss = ob->sculpt;
  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
  const bool has_grids = (pbvh_type == PBVH_GRIDS);

  const StrokeCache *cache = ss->cache;
  /* NOTE: normally `BKE_brush_weight_get(scene, brush)` is used,
   * however in this case we calculate a new weight each time. */
  const float paintweight = strength;
  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
  vwpaint::get_brush_alpha_data(
      scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint::use_normal(vp);
  const bool use_face_sel = (mesh->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const bool use_vert_sel = (mesh->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);
  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, brush->falloff_shape);

  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArray<bool> select_vert = *attributes.lookup_or_default<bool>(
      ".select_vert", bke::AttrDomain::Point, false);

  /* For each vertex */
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    /* Test to see if the vertex coordinates are within the spherical brush region. */
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    /* NOTE: grids are 1:1 with corners (aka loops).
     * For multires, take the vert whose loop corresponds to the current grid.
     * Otherwise, take the current vert. */
    const int v_index = has_grids ? ss->corner_verts[vd.grid_indices[vd.g]] :
                                    vd.vert_indices[vd.i];
    const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;

    /* If the vertex is selected */
    if ((use_face_sel || use_vert_sel) && !select_vert[v_index]) {
      continue;
    }
    float brush_strength = cache->bstrength;
    const float angle_cos = (use_normal && vd.no) ? dot_v3v3(sculpt_normal_frontface, vd.no) :
                                                    1.0f;
    if (!vwpaint::test_brush_angle_falloff(
            *brush, wpd->normal_angle_precalc, angle_cos, &brush_strength))
    {
      continue;
    }
    const float brush_fade = BKE_brush_curve_strength(brush, sqrtf(test.dist), cache->radius);
    const float final_alpha = brush_fade * brush_strength * grid_alpha * brush_alpha_pressure;

    if ((brush->flag & BRUSH_ACCUMULATE) == 0) {
      if (ss->mode.wpaint.alpha_weight[v_index] < final_alpha) {
        ss->mode.wpaint.alpha_weight[v_index] = final_alpha;
      }
      else {
        continue;
      }
    }

    do_weight_paint_vertex(vp, ob, wpi, v_index, final_alpha, paintweight);
  }
  BKE_pbvh_vertex_iter_end;
}

static WPaintAverageAccum do_wpaint_brush_calc_average_weight(Object *ob,
                                                              const Mesh *mesh,
                                                              const Brush *brush,
                                                              const VPaint *vp,
                                                              WeightPaintInfo *wpi,
                                                              PBVHNode *node)
{
  using namespace blender;
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
  const bool has_grids = (pbvh_type == PBVH_GRIDS);

  const bool use_normal = vwpaint::use_normal(vp);
  const bool use_face_sel = (mesh->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const bool use_vert_sel = (mesh->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

  WPaintAverageAccum accum{};
  accum.len = 0;
  accum.value = 0.0;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);
  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, brush->falloff_shape);

  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArray<bool> select_vert = *attributes.lookup_or_default<bool>(
      ".select_vert", bke::AttrDomain::Point, false);

  /* For each vertex */
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    /* Test to see if the vertex coordinates are within the spherical brush region. */
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    const float angle_cos = (use_normal && vd.no) ? dot_v3v3(sculpt_normal_frontface, vd.no) :
                                                    1.0f;
    if (angle_cos <= 0.0f ||
        BKE_brush_curve_strength(brush, sqrtf(test.dist), cache->radius) <= 0.0f)
    {
      continue;
    }

    const int v_index = has_grids ? ss->corner_verts[vd.grid_indices[vd.g]] :
                                    vd.vert_indices[vd.i];
    /* If the vertex is selected. */
    if ((use_face_sel || use_vert_sel) && !select_vert[v_index]) {
      continue;
    }

    const MDeformVert *dv = &wpi->dvert[v_index];
    accum.len += 1;
    accum.value += wpaint_get_active_weight(dv, wpi);
  }
  BKE_pbvh_vertex_iter_end;

  return accum;
}

static float calculate_average_weight(Object *ob,
                                      const Mesh *mesh,
                                      const Brush *brush,
                                      VPaint *vp,
                                      WeightPaintInfo *wpi,
                                      Span<PBVHNode *> nodes)
{
  WPaintAverageAccum *accum = (WPaintAverageAccum *)MEM_mallocN(sizeof(*accum) * nodes.size(),
                                                                __func__);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      accum[i] = do_wpaint_brush_calc_average_weight(ob, mesh, brush, vp, wpi, nodes[i]);
    }
  });

  float strength = 0.0f;

  uint accum_len = 0;
  double accum_weight = 0.0;
  for (int i = 0; i < nodes.size(); i++) {
    accum_len += accum[i].len;
    accum_weight += accum[i].value;
  }
  if (accum_len != 0) {
    accum_weight /= accum_len;
    strength = float(accum_weight);
  }

  MEM_SAFE_FREE(accum);

  return strength;
}

static void wpaint_paint_leaves(bContext *C,
                                Object *ob,
                                VPaint *vp,
                                WPaintData *wpd,
                                WeightPaintInfo *wpi,
                                Mesh *mesh,
                                Span<PBVHNode *> nodes)
{
  Scene *scene = CTX_data_scene(C);
  const Brush *brush = ob->sculpt->cache->brush;

  /* Use this so average can modify its weight without touching the brush. */
  float strength = BKE_brush_weight_get(scene, brush);
  if (brush->weightpaint_tool == WPAINT_TOOL_AVERAGE) {
    strength = calculate_average_weight(ob, mesh, brush, vp, wpi, nodes);
  }

  if (ME_USING_MIRROR_X_VERTEX_GROUPS(mesh)) {
    /* NOTE: current mirroring code cannot be run in parallel */
    switch ((eBrushWeightPaintTool)brush->weightpaint_tool) {
      case WPAINT_TOOL_AVERAGE: {
        for (const int i : nodes.index_range()) {
          do_wpaint_brush_draw_task(scene, ob, brush, vp, wpd, wpi, mesh, strength, nodes[i]);
        }
        break;
      }
      case WPAINT_TOOL_SMEAR:
        for (const int i : nodes.index_range()) {
          do_wpaint_brush_smear_task(scene, ob, brush, vp, wpd, wpi, mesh, nodes[i]);
        }
        break;
      case WPAINT_TOOL_BLUR:
        for (const int i : nodes.index_range()) {
          do_wpaint_brush_blur_task(scene, ob, brush, vp, wpd, wpi, mesh, nodes[i]);
        }
        break;
      case WPAINT_TOOL_DRAW:
        for (const int i : nodes.index_range()) {
          do_wpaint_brush_draw_task(scene, ob, brush, vp, wpd, wpi, mesh, strength, nodes[i]);
        }
        break;
    }
  }
  else {
    switch ((eBrushWeightPaintTool)brush->weightpaint_tool) {
      case WPAINT_TOOL_AVERAGE: {
        threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
          for (const int i : range) {
            do_wpaint_brush_draw_task(scene, ob, brush, vp, wpd, wpi, mesh, strength, nodes[i]);
          }
        });
        break;
      }
      case WPAINT_TOOL_SMEAR:
        threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
          for (const int i : range) {
            do_wpaint_brush_smear_task(scene, ob, brush, vp, wpd, wpi, mesh, nodes[i]);
          }
        });
        break;
      case WPAINT_TOOL_BLUR:
        threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
          for (const int i : range) {
            do_wpaint_brush_blur_task(scene, ob, brush, vp, wpd, wpi, mesh, nodes[i]);
          }
        });
        break;
      case WPAINT_TOOL_DRAW:
        threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
          for (const int i : range) {
            do_wpaint_brush_draw_task(scene, ob, brush, vp, wpd, wpi, mesh, strength, nodes[i]);
          }
        });
        break;
    }
  }
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Enter Weight Paint Mode
 * \{ */

static void grease_pencil_wpaintmode_enter(Scene *scene, Object *ob)
{
  const PaintMode paint_mode = PaintMode::Weight;
  Paint *weight_paint = BKE_paint_get_active_from_paintmode(scene, paint_mode);
  BKE_paint_ensure(scene->toolsettings, &weight_paint);

  ob->mode |= OB_MODE_WEIGHT_PAINT;

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

void ED_object_wpaintmode_enter_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      vwpaint::mode_enter_generic(bmain, depsgraph, scene, ob, OB_MODE_WEIGHT_PAINT);
      break;
    case OB_GREASE_PENCIL:
      grease_pencil_wpaintmode_enter(scene, ob);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
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
/** \name Exit Weight Paint Mode
 * \{ */

void ED_object_wpaintmode_exit_ex(Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      vwpaint::mode_exit_generic(ob, OB_MODE_WEIGHT_PAINT);
      break;
    case OB_GREASE_PENCIL: {
      ob->mode &= ~OB_MODE_WEIGHT_PAINT;
      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }
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

bool weight_paint_mode_poll(bContext *C)
{
  const Object *ob = CTX_data_active_object(C);

  return ob && ob->mode == OB_MODE_WEIGHT_PAINT && ((const Mesh *)ob->data)->faces_num;
}

bool weight_paint_mode_region_view3d_poll(bContext *C)
{
  return weight_paint_mode_poll(C) && ED_operator_region_view3d_active(C);
}

static bool weight_paint_poll_ex(bContext *C, bool check_tool)
{
  const Object *ob = CTX_data_active_object(C);
  const ScrArea *area;

  if ((ob != nullptr) && (ob->mode & OB_MODE_WEIGHT_PAINT) &&
      (BKE_paint_brush(&CTX_data_tool_settings(C)->wpaint->paint) != nullptr) &&
      (area = CTX_wm_area(C)) && (area->spacetype == SPACE_VIEW3D))
  {
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

/**
 * \note Keep in sync with #vpaint_mode_toggle_exec
 */
static int wpaint_mode_toggle_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  wmMsgBus *mbus = CTX_wm_message_bus(C);
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

  if (ob->type == OB_MESH) {
    /* Weight-paint works by overriding colors in mesh,
     * so need to make sure we recalculate on enter and
     * exit (exit needs doing regardless because we
     * should re-deform).
     */
    Mesh *mesh = BKE_mesh_from_object(ob);
    DEG_id_tag_update(&mesh->id, 0);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);

  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

  WM_toolsystem_update_from_context_view3d(C);

  return OPERATOR_FINISHED;
}

void PAINT_OT_weight_paint_toggle(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Weight Paint Mode";
  ot->idname = "PAINT_OT_weight_paint_toggle";
  ot->description = "Toggle weight paint mode in 3D view";

  /* api callbacks */
  ot->exec = wpaint_mode_toggle_exec;
  ot->poll = vwpaint::mode_toggle_poll_test;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/** \name Weight Paint Operator
 * \{ */

static void wpaint_do_paint(bContext *C,
                            Object *ob,
                            VPaint *wp,
                            WPaintData *wpd,
                            WeightPaintInfo *wpi,
                            Mesh *mesh,
                            Brush *brush,
                            const ePaintSymmetryFlags symm,
                            const int axis,
                            const int i,
                            const float angle)
{
  SculptSession *ss = ob->sculpt;
  ss->cache->radial_symmetry_pass = i;
  SCULPT_cache_calc_brushdata_symm(ss->cache, symm, axis, angle);

  Vector<PBVHNode *> nodes = vwpaint::pbvh_gather_generic(ob, wp, brush);

  wpaint_paint_leaves(C, ob, wp, wpd, wpi, mesh, nodes);
}

static void wpaint_do_radial_symmetry(bContext *C,
                                      Object *ob,
                                      VPaint *wp,
                                      WPaintData *wpd,
                                      WeightPaintInfo *wpi,
                                      Mesh *mesh,
                                      Brush *brush,
                                      const ePaintSymmetryFlags symm,
                                      const int axis)
{
  for (int i = 1; i < wp->radial_symm[axis - 'X']; i++) {
    const float angle = (2.0 * M_PI) * i / wp->radial_symm[axis - 'X'];
    wpaint_do_paint(C, ob, wp, wpd, wpi, mesh, brush, symm, axis, i, angle);
  }
}

/* near duplicate of: sculpt.cc's,
 * 'do_symmetrical_brush_actions' and 'vpaint_do_symmetrical_brush_actions'. */
static void wpaint_do_symmetrical_brush_actions(
    bContext *C, Object *ob, VPaint *wp, WPaintData *wpd, WeightPaintInfo *wpi)
{
  Brush *brush = BKE_paint_brush(&wp->paint);
  Mesh *mesh = (Mesh *)ob->data;
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  int i = 0;

  /* initial stroke */
  cache->mirror_symmetry_pass = ePaintSymmetryFlags(0);
  wpaint_do_paint(C, ob, wp, wpd, wpi, mesh, brush, ePaintSymmetryFlags(0), 'X', 0, 0);
  wpaint_do_radial_symmetry(C, ob, wp, wpd, wpi, mesh, brush, ePaintSymmetryFlags(0), 'X');
  wpaint_do_radial_symmetry(C, ob, wp, wpd, wpi, mesh, brush, ePaintSymmetryFlags(0), 'Y');
  wpaint_do_radial_symmetry(C, ob, wp, wpd, wpi, mesh, brush, ePaintSymmetryFlags(0), 'Z');

  cache->symmetry = symm;

  if (mesh->editflag & ME_EDIT_MIRROR_VERTEX_GROUPS) {
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
        wpaint_do_paint(C, ob, wp, wpd, wpi, mesh, brush, symm, 'X', 0, 0);
        wpaint_do_radial_symmetry(C, ob, wp, wpd, wpi, mesh, brush, symm, 'X');
      }
      if (i & (1 << 1)) {
        wpaint_do_paint(C, ob, wp, wpd, wpi, mesh, brush, symm, 'Y', 0, 0);
        wpaint_do_radial_symmetry(C, ob, wp, wpd, wpi, mesh, brush, symm, 'Y');
      }
      if (i & (1 << 2)) {
        wpaint_do_paint(C, ob, wp, wpd, wpi, mesh, brush, symm, 'Z', 0, 0);
        wpaint_do_radial_symmetry(C, ob, wp, wpd, wpi, mesh, brush, symm, 'Z');
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

  vwpaint::update_cache_variants(C, wp, ob, itemptr);

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
  mul_m4_m4m4(mat, vc->rv3d->persmat, ob->object_to_world().ptr());

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
    precompute_weight_values(ob, brush, wpd, &wpi, mesh);
  }

  wpaint_do_symmetrical_brush_actions(C, ob, wp, wpd, &wpi);

  swap_m4m4(vc->rv3d->persmat, mat);

  /* Calculate pivot for rotation around selection if needed.
   * also needed for "Frame Selected" on last stroke. */
  float loc_world[3];
  mul_v3_m4v3(loc_world, ob->object_to_world().ptr(), ss->cache->true_location);
  vwpaint::last_stroke_update(scene, loc_world);

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
    vwpaint::smooth_brush_toggle_off(C, &vp->paint, ss->cache);
  }

  /* and particles too */
  if (ob->particlesystem.first) {
    LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
      for (int i = 0; i < PSYS_TOT_VG; i++) {
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

  paint_stroke_operator_properties(ot, true);
}

/** \} */
