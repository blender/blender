/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_anim_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_set.hh"

#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_layer.hh"
#include "BKE_nla.hh"

#include "ED_anim_api.hh"
#include "ED_keyframes_edit.hh"

#include "UI_view2d.hh"

#include "transform.hh"
#include "transform_constraints.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

namespace blender::ed::transform {

struct TransDataGraph {
  float unit_scale;
  float offset;
};

/* -------------------------------------------------------------------- */
/** \name Graph Editor Transform Creation
 * \{ */

/**
 * Helper function for createTransGraphEditData, which is responsible for associating
 * source data with transform data.
 */
static void bezt_to_transdata(TransData *td,
                              TransData2D *td2d,
                              TransDataGraph *tdg,
                              bAnimListElem *ale,
                              BezTriple *bezt,
                              int bi,
                              bool selected,
                              bool ishandle,
                              bool intvals,
                              const float mtx[3][3],
                              const float smtx[3][3],
                              float unit_scale,
                              float offset)
{
  float *loc = bezt->vec[bi];
  const float *cent = bezt->vec[1];

  /* New location from td gets dumped onto the old-location of td2d, which then
   * gets copied to the actual data at td2d->loc2d (bezt->vec[n])
   *
   * Due to NLA mapping, we apply NLA mapping to some of the verts here,
   * and then that mapping will be undone after transform is done.
   */

  if (ANIM_nla_mapping_allowed(ale)) {
    td2d->loc[0] = ANIM_nla_tweakedit_remap(ale, loc[0], NLATIME_CONVERT_MAP);
    td2d->loc[1] = (loc[1] + offset) * unit_scale;
    td2d->loc[2] = 0.0f;
    td2d->loc2d = loc;

    td->loc = td2d->loc;
    td->center[0] = ANIM_nla_tweakedit_remap(ale, cent[0], NLATIME_CONVERT_MAP);
    td->center[1] = (cent[1] + offset) * unit_scale;
    td->center[2] = 0.0f;

    copy_v3_v3(td->iloc, td->loc);
  }
  else {
    td2d->loc[0] = loc[0];
    td2d->loc[1] = (loc[1] + offset) * unit_scale;
    td2d->loc[2] = 0.0f;
    td2d->loc2d = loc;

    td->loc = td2d->loc;
    copy_v3_v3(td->center, cent);
    td->center[1] = (td->center[1] + offset) * unit_scale;
    copy_v3_v3(td->iloc, td->loc);
  }

  if (!ishandle) {
    td2d->h1 = bezt->vec[0];
    td2d->h2 = bezt->vec[2];
    copy_v2_v2(td2d->ih1, td2d->h1);
    copy_v2_v2(td2d->ih2, td2d->h2);
  }
  else {
    td2d->h1 = nullptr;
    td2d->h2 = nullptr;
  }

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->val = nullptr;

  /* Store AnimData info in td->extra, for applying mapping when flushing.
   *
   * We do this conditionally as a hacky way of indicating whether NLA remapping
   * should be done. This is left over from old code, most of which was changed
   * in #130440 to avoid using `adt == nullptr` as an indicator for that. This
   * was left that way because updating it cleanly was more involved than made
   * sense for the bug fix in #130440. */
  if (ANIM_nla_mapping_allowed(ale)) {
    td->extra = ale->adt;
  }

  if (selected) {
    td->flag |= TD_SELECTED;
    td->dist = 0.0f;
  }
  else {
    td->dist = FLT_MAX;
  }

  if (ishandle) {
    td->flag |= TD_NOTIMESNAP;
  }
  if (intvals) {
    td->flag |= TD_INTVALUES;
  }

  /* Copy space-conversion matrices for dealing with non-uniform scales. */
  copy_m3_m3(td->mtx, mtx);
  copy_m3_m3(td->smtx, smtx);

  tdg->unit_scale = unit_scale;
  tdg->offset = offset;
}

static bool graph_edit_is_translation_mode(TransInfo *t)
{
  return ELEM(t->mode, TFM_TRANSLATION, TFM_TIME_TRANSLATE, TFM_TIME_SLIDE);
}

static bool graph_edit_use_local_center(TransInfo *t)
{
  return ((t->around == V3D_AROUND_LOCAL_ORIGINS) && (graph_edit_is_translation_mode(t) == false));
}

static void enable_autolock(TransInfo *t, SpaceGraph *space_graph)
{
  /* Locking the axis makes most sense for translation. We may want to enable it for scaling as
   * well if artists require that. */
  if (t->mode != TFM_TRANSLATION) {
    return;
  }

  /* These flags are set when using tweak mode on handles. */
  if ((space_graph->runtime.flag & SIPO_RUNTIME_FLAG_TWEAK_HANDLES_LEFT) ||
      (space_graph->runtime.flag & SIPO_RUNTIME_FLAG_TWEAK_HANDLES_RIGHT))
  {
    return;
  }

  initSelectConstraint(t);
}

/**
 * Get the effective selection of a triple for transform, i.e. return if the left handle, right
 * handle and/or the center point should be affected by transform.
 */
static void graph_bezt_get_transform_selection(const TransInfo *t,
                                               const BezTriple *bezt,
                                               const bool use_handle,
                                               bool *r_left_handle,
                                               bool *r_key,
                                               bool *r_right_handle)
{
  SpaceGraph *sipo = (SpaceGraph *)t->area->spacedata.first;
  bool key = (bezt->f2 & SELECT) != 0;
  bool left = use_handle ? ((bezt->f1 & SELECT) != 0) : key;
  bool right = use_handle ? ((bezt->f3 & SELECT) != 0) : key;

  if (use_handle && t->is_launch_event_drag) {
    if (sipo->runtime.flag & SIPO_RUNTIME_FLAG_TWEAK_HANDLES_LEFT) {
      key = right = false;
    }
    else if (sipo->runtime.flag & SIPO_RUNTIME_FLAG_TWEAK_HANDLES_RIGHT) {
      left = key = false;
    }
  }

  /* Whenever we move the key, we also move both handles. */
  if (key) {
    left = right = true;
  }

  *r_key = key;
  *r_left_handle = left;
  *r_right_handle = right;
}

static float graph_key_shortest_dist(
    TransInfo *t, FCurve *fcu, TransData *td_start, TransData *td, int cfra, bool use_handle)
{
  int j = 0;
  TransData *td_iter = td_start;
  bool sel_key, sel_left, sel_right;

  float dist = FLT_MAX;
  for (; j < fcu->totvert; j++) {
    BezTriple *bezt = fcu->bezt + j;
    if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
      graph_bezt_get_transform_selection(t, bezt, use_handle, &sel_left, &sel_key, &sel_right);

      if (sel_left || sel_key || sel_right) {
        dist = min_fff(dist, td->dist, fabs(td_iter->center[0] - td->center[0]));
      }

      td_iter += 3;
    }
  }

  return dist;
}

/**
 * It is important to note that this doesn't always act on the selection (like it's usually done),
 * it acts on a subset of it. E.g. the selection code may leave a hint that we just dragged on a
 * left or right handle (SIPO_RUNTIME_FLAG_TWEAK_HANDLES_LEFT/RIGHT) and then we only transform the
 * selected left or right handles accordingly.
 * The points to be transformed are tagged with BEZT_FLAG_TEMP_TAG; some lower level curve
 * functions may need to be made aware of this. It's ugly that these act based on selection state
 * anyway.
 */
static void createTransGraphEditData(bContext *C, TransInfo *t)
{
  SpaceGraph *sipo = (SpaceGraph *)t->area->spacedata.first;
  Scene *scene = t->scene;
  ARegion *region = t->region;
  View2D *v2d = &region->v2d;

  TransData *td = nullptr;
  TransData2D *td2d = nullptr;
  TransDataGraph *tdg = nullptr;

  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  BezTriple *bezt;
  int count = 0, i;
  float mtx[3][3], smtx[3][3];
  const bool use_handle = !(sipo->flag & SIPO_NOHANDLES);
  const bool use_local_center = graph_edit_use_local_center(t);
  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
  short anim_map_flag = ANIM_UNITCONV_ONLYSEL | ANIM_UNITCONV_SELVERTS;
  bool sel_key, sel_left, sel_right;

  /* Determine what type of data we are operating on. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  anim_map_flag |= ANIM_get_normalization_flags(ac.sl);

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVE_VISIBLE |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  /* Which side of the current frame should be allowed. */
  /* XXX we still want this mode, but how to get this using standard transform too? */
  if (t->mode == TFM_TIME_EXTEND) {
    t->frame_side = transform_convert_frame_side_dir_get(t, float(scene->r.cfra));
  }
  else {
    /* Normal transform - both sides of current frame are considered. */
    t->frame_side = 'B';
  }

  /* Loop 1: count how many BezTriples (specifically their verts)
   * are selected (or should be edited). */
  Set<FCurve *> visited_fcurves;
  Vector<bAnimListElem *> unique_fcu_anim_list_elements;
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;
    /* If 2 or more objects share the same action, multiple bAnimListElem might reference the same
     * FCurve. */
    if (!visited_fcurves.add(fcu)) {
      continue;
    }
    unique_fcu_anim_list_elements.append(ale);
    int curvecount = 0;
    bool selected = false;

    /* F-Curve may not have any keyframes. */
    if (fcu->bezt == nullptr) {
      continue;
    }

    /* Convert current-frame to action-time (slightly less accurate, especially under
     * higher scaling ratios, but is faster than converting all points). */
    const float cfra = ANIM_nla_tweakedit_remap(ale, float(scene->r.cfra), NLATIME_CONVERT_UNMAP);

    for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
      /* Only include BezTriples whose 'keyframe'
       * occurs on the same side of the current frame as mouse. */
      if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
        graph_bezt_get_transform_selection(t, bezt, use_handle, &sel_left, &sel_key, &sel_right);

        if (is_prop_edit) {
          curvecount += 3;
          if (sel_key || sel_left || sel_right) {
            selected = true;
          }
        }
        else {
          if (sel_left) {
            count++;
          }

          if (sel_right) {
            count++;
          }

          /* Only include main vert if selected. */
          if (sel_key && !use_local_center) {
            count++;
          }
        }
      }
    }

    if (is_prop_edit) {
      if (selected) {
        count += curvecount;
        ale->tag = true;
      }
    }
  }

  /* Stop if trying to build list if nothing selected. */
  if (count == 0) {
    /* Cleanup temp list. */
    ANIM_animdata_freelist(&anim_data);
    return;
  }

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* Allocate memory for data. */
  tc->data_len = count;

  tc->data = MEM_calloc_arrayN<TransData>(tc->data_len, "TransData (Graph Editor)");
  /* For each 2d vert a 3d vector is allocated,
   * so that they can be treated just as if they were 3d verts. */
  tc->data_2d = MEM_calloc_arrayN<TransData2D>(tc->data_len, "TransData2D (Graph Editor)");
  tc->custom.type.data = MEM_callocN(tc->data_len * sizeof(TransDataGraph), "TransDataGraph");
  tc->custom.type.use_free = true;

  td = tc->data;
  td2d = tc->data_2d;
  tdg = static_cast<TransDataGraph *>(tc->custom.type.data);

  /* Precompute space-conversion matrices for dealing with non-uniform scaling of Graph Editor. */
  unit_m3(mtx);
  unit_m3(smtx);

  if (ELEM(t->mode, TFM_ROTATION, TFM_RESIZE)) {
    float xscale, yscale;

    /* Apply scale factors to x and y axes of space-conversion matrices. */
    UI_view2d_scale_get(v2d, &xscale, &yscale);

    /* `mtx` is data to global (i.e. view) conversion. */
    mul_v3_fl(mtx[0], xscale);
    mul_v3_fl(mtx[1], yscale);

    /* `smtx` is global (i.e. view) to data conversion. */
    if (IS_EQF(xscale, 0.0f) == 0) {
      mul_v3_fl(smtx[0], 1.0f / xscale);
    }
    if (IS_EQF(yscale, 0.0f) == 0) {
      mul_v3_fl(smtx[1], 1.0f / yscale);
    }
  }

  bool at_least_one_key_selected = false;

  /* Loop 2: build transdata arrays. */
  for (bAnimListElem *ale : unique_fcu_anim_list_elements) {
    FCurve *fcu = (FCurve *)ale->key_data;
    bool intvals = (fcu->flag & FCURVE_INT_VALUES) != 0;
    float unit_scale, offset;

    /* F-Curve may not have any keyframes. */
    if (fcu->bezt == nullptr || (is_prop_edit && ale->tag == 0)) {
      continue;
    }

    /* Convert current-frame to action-time (slightly less accurate, especially under
     * higher scaling ratios, but is faster than converting all points). */
    const float cfra = ANIM_nla_tweakedit_remap(ale, float(scene->r.cfra), NLATIME_CONVERT_UNMAP);

    unit_scale = ANIM_unit_mapping_get_factor(
        ac.scene, ale->id, static_cast<FCurve *>(ale->key_data), anim_map_flag, &offset);

    for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
      /* Ensure temp flag is cleared for all triples, we use it. */
      bezt->f1 &= ~BEZT_FLAG_TEMP_TAG;
      bezt->f2 &= ~BEZT_FLAG_TEMP_TAG;
      bezt->f3 &= ~BEZT_FLAG_TEMP_TAG;

      /* Only include BezTriples whose 'keyframe' occurs on the same side
       * of the current frame as mouse (if applicable). */
      if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
        TransDataCurveHandleFlags *hdata = nullptr;

        graph_bezt_get_transform_selection(t, bezt, use_handle, &sel_left, &sel_key, &sel_right);
        at_least_one_key_selected |= sel_key;
        if (is_prop_edit) {
          bool is_sel = (sel_key || sel_left || sel_right);
          /* We always select all handles for proportional editing * if central handle is
           * selected. */
          initTransDataCurveHandles(td, bezt);
          bezt_to_transdata(td++,
                            td2d++,
                            tdg++,
                            ale,
                            bezt,
                            0,
                            is_sel,
                            true,
                            intvals,
                            mtx,
                            smtx,
                            unit_scale,
                            offset);
          initTransDataCurveHandles(td, bezt);
          bezt_to_transdata(td++,
                            td2d++,
                            tdg++,
                            ale,
                            bezt,
                            1,
                            is_sel,
                            false,
                            intvals,
                            mtx,
                            smtx,
                            unit_scale,
                            offset);
          initTransDataCurveHandles(td, bezt);
          bezt_to_transdata(td++,
                            td2d++,
                            tdg++,
                            ale,
                            bezt,
                            2,
                            is_sel,
                            true,
                            intvals,
                            mtx,
                            smtx,
                            unit_scale,
                            offset);

          if (is_sel) {
            bezt->f1 |= BEZT_FLAG_TEMP_TAG;
            bezt->f2 |= BEZT_FLAG_TEMP_TAG;
            bezt->f3 |= BEZT_FLAG_TEMP_TAG;
          }
        }
        else {
          /* Only include handles if selected, irrespective of the interpolation modes.
           * also, only treat handles specially if the center point isn't selected. */
          if (sel_left) {
            hdata = initTransDataCurveHandles(td, bezt);
            bezt_to_transdata(td++,
                              td2d++,
                              tdg++,
                              ale,
                              bezt,
                              0,
                              sel_left,
                              true,
                              intvals,
                              mtx,
                              smtx,
                              unit_scale,
                              offset);
            bezt->f1 |= BEZT_FLAG_TEMP_TAG;
          }

          if (sel_right) {
            if (hdata == nullptr) {
              hdata = initTransDataCurveHandles(td, bezt);
            }
            bezt_to_transdata(td++,
                              td2d++,
                              tdg++,
                              ale,
                              bezt,
                              2,
                              sel_right,
                              true,
                              intvals,
                              mtx,
                              smtx,
                              unit_scale,
                              offset);
            bezt->f3 |= BEZT_FLAG_TEMP_TAG;
          }

          /* Only include main vert if selected. */
          if (sel_key && !use_local_center) {
            /* Move handles relative to center. */
            if (graph_edit_is_translation_mode(t)) {
              if (sel_left) {
                td->flag |= TD_MOVEHANDLE1;
              }
              if (sel_right) {
                td->flag |= TD_MOVEHANDLE2;
              }
            }

            /* If handles were not selected, store their selection status. */
            if (!(sel_left) || !(sel_right)) {
              if (hdata == nullptr) {
                hdata = initTransDataCurveHandles(td, bezt);
              }
            }

            bezt_to_transdata(td++,
                              td2d++,
                              tdg++,
                              ale,
                              bezt,
                              1,
                              sel_key,
                              false,
                              intvals,
                              mtx,
                              smtx,
                              unit_scale,
                              offset);
            bezt->f2 |= BEZT_FLAG_TEMP_TAG;
          }
          /* Special hack (must be done after #initTransDataCurveHandles(),
           * as that stores handle settings to restore...):
           *
           * - Check if we've got entire BezTriple selected and we're scaling/rotating that point,
           *   then check if we're using auto-handles.
           * - If so, change them auto-handles to aligned handles so that handles get affected too.
           */
          if (ELEM(bezt->h1, HD_AUTO, HD_AUTO_ANIM) && ELEM(bezt->h2, HD_AUTO, HD_AUTO_ANIM) &&
              ELEM(t->mode, TFM_ROTATION, TFM_RESIZE))
          {
            if (hdata && (sel_left) && (sel_right)) {
              bezt->h1 = HD_ALIGN;
              bezt->h2 = HD_ALIGN;
            }
          }
        }
      }
    }

    /* Sets handles based on the selection. */
    testhandles_fcurve(fcu, BEZT_FLAG_TEMP_TAG, use_handle);
  }

  if (is_prop_edit) {
    /* Loop 3: build proportional edit distances. */
    td = tc->data;

    for (bAnimListElem *ale : unique_fcu_anim_list_elements) {
      FCurve *fcu = (FCurve *)ale->key_data;
      TransData *td_start = td;

      /* F-Curve may not have any keyframes. */
      if (fcu->bezt == nullptr || (ale->tag == 0)) {
        continue;
      }

      /* Convert current-frame to action-time (slightly less accurate, especially under
       * higher scaling ratios, but is faster than converting all points). */
      const float cfra = ANIM_nla_tweakedit_remap(
          ale, float(scene->r.cfra), NLATIME_CONVERT_UNMAP);

      for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
        /* Only include BezTriples whose 'keyframe' occurs on the
         * same side of the current frame as mouse (if applicable). */
        if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
          graph_bezt_get_transform_selection(t, bezt, use_handle, &sel_left, &sel_key, &sel_right);

          /* Now determine to distance for proportional editing for all three TransData
           * (representing the key as well as both handles). Note though that the way
           * #bezt_to_transdata sets up the TransData, the td->center[0] will always be based on
           * the key (bezt->vec[1]) which means that #graph_key_shortest_dist will return the
           * same for all of them and we can reuse that (expensive) result if needed. Might be
           * worth looking into using a 2D KDTree in the future as well. */

          float dist = FLT_MAX;
          if (sel_left || sel_key || sel_right) {
            /* If either left handle or key or right handle is selected, all will move fully. */
            dist = 0.0f;
          }
          else {
            /* If nothing is selected, left handle and key and right handle will share the same (to
             * be calculated) distance. */
            dist = graph_key_shortest_dist(t, fcu, td_start, td, cfra, use_handle);
          }

          td->dist = td->rdist = dist;
          (td + 1)->dist = (td + 1)->rdist = dist;
          (td + 2)->dist = (td + 2)->rdist = dist;
          td += 3;
        }
      }
    }
  }

  if (sipo->flag & SIPO_AUTOLOCK_AXIS && at_least_one_key_selected) {
    enable_autolock(t, sipo);
  }

  /* Cleanup temp list. */
  ANIM_animdata_freelist(&anim_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Graph Editor Transform Flush
 * \{ */

static bool fcu_test_selected(FCurve *fcu)
{
  BezTriple *bezt = fcu->bezt;
  uint i;

  if (bezt == nullptr) { /* Ignore baked. */
    return false;
  }

  for (i = 0; i < fcu->totvert; i++, bezt++) {
    if (BEZT_ISSEL_ANY(bezt)) {
      return true;
    }
  }

  return false;
}

/**
 * This function is called on recalc_data to apply the transforms applied
 * to the transdata on to the actual keyframe data.
 */
static void flushTransGraphData(TransInfo *t)
{
  TransData *td;
  TransData2D *td2d;
  TransDataGraph *tdg;
  int a;

  eSnapMode snap_mode = t->tsnap.mode;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  /* Flush to 2d vector from internally used 3d vector. */
  for (a = 0,
      td = tc->data,
      td2d = tc->data_2d,
      tdg = static_cast<TransDataGraph *>(tc->custom.type.data);
       a < tc->data_len;
       a++, td++, td2d++, tdg++)
  {
    /* Pointers to relevant AnimData blocks are stored in the `td->extra` pointers. */
    AnimData *adt = (AnimData *)td->extra;

    float inv_unit_scale = 1.0f / tdg->unit_scale;

    /* Handle snapping for time values:
     * - We should still be in NLA-mapping time-space.
     * - Only apply to keyframes (but never to handles).
     * - Don't do this when canceling, or else these changes won't go away.
     */
    if ((t->tsnap.flag & SCE_SNAP) && (t->state != TRANS_CANCEL) && !(td->flag & TD_NOTIMESNAP)) {
      transform_snap_anim_flush_data(t, td, snap_mode, td->loc);
    }

    /* We need to unapply the nla-mapping from the time in some situations. */
    if (adt) {
      td2d->loc2d[0] = BKE_nla_tweakedit_remap(adt, td2d->loc[0], NLATIME_CONVERT_UNMAP);
    }
    else {
      td2d->loc2d[0] = td2d->loc[0];
    }

    /* If int-values only, truncate to integers. */
    if (td->flag & TD_INTVALUES) {
      td2d->loc2d[1] = floorf(td2d->loc[1] * inv_unit_scale - tdg->offset + 0.5f);
    }
    else {
      td2d->loc2d[1] = td2d->loc[1] * inv_unit_scale - tdg->offset;
    }

    transform_convert_flush_handle2D(td, td2d, inv_unit_scale);
  }
}

/** Struct for use in re-sorting BezTriples during Graph Editor transform. */
struct BeztMap {
  BezTriple *bezt;
  /** Index of `bezt` in `fcu->bezt` array before sorting. */
  uint oldIndex;
  /** Swap order of handles. Can happen when rotating keys around their common center. */
  bool swap_handles;
};

/**
 * Converts an FCurve's BezTriple array to a BeztMap vector.
 */
static Vector<BeztMap> bezt_to_beztmaps(BezTriple *bezts, const int totvert)
{
  if (totvert == 0 || bezts == nullptr) {
    return {};
  }

  Vector<BeztMap> bezms = Vector<BeztMap>(totvert);

  for (const int i : bezms.index_range()) {
    BezTriple *bezt = &bezts[i];
    BeztMap &bezm = bezms[i];
    bezm.bezt = bezt;
    bezm.swap_handles = false;
    bezm.oldIndex = i;
  }

  return bezms;
}

/* This function copies the code of sort_time_ipocurve, but acts on BeztMap structs instead. */
static void sort_time_beztmaps(const MutableSpan<BeztMap> bezms)
{
  /* Check if handles need to be swapped. */
  for (BeztMap &bezm : bezms) {
    /* Handles are only swapped if they are both on the wrong side of the key. Otherwise the one
     * handle out of place is just clamped at the key position later. */
    bezm.swap_handles = (bezm.bezt->vec[0][0] > bezm.bezt->vec[1][0] &&
                         bezm.bezt->vec[2][0] < bezm.bezt->vec[1][0]);
  }

  bool ok = true;
  const int bezms_size = bezms.size();
  if (bezms_size < 2) {
    /* No sorting is needed with only 0 or 1 entries. */
    return;
  }
  const IndexRange bezm_range = bezms.index_range().drop_back(1);

  /* Keep repeating the process until nothing is out of place anymore. */
  while (ok) {
    ok = false;
    for (const int i : bezm_range) {
      BeztMap *bezm = &bezms[i];
      /* Is current bezm out of order (i.e. occurs later than next)? */
      if (bezm->bezt->vec[1][0] > (bezm + 1)->bezt->vec[1][0]) {
        std::swap(*bezm, *(bezm + 1));
        ok = true;
      }
    }
  }
}

static inline void update_trans_data(TransData *td,
                                     const FCurve *fcu,
                                     const int new_index,
                                     const bool swap_handles)
{
  if (td->flag & TD_BEZTRIPLE && td->hdata) {
    if (swap_handles) {
      td->hdata->h1 = &fcu->bezt[new_index].h2;
      td->hdata->h2 = &fcu->bezt[new_index].h1;
    }
    else {
      td->hdata->h1 = &fcu->bezt[new_index].h1;
      td->hdata->h2 = &fcu->bezt[new_index].h2;
    }
  }
}

/* Adjust the pointers that the transdata has to each BezTriple. */
static void update_transdata_bezt_pointers(TransDataContainer *tc,
                                           const Map<float *, int> &trans_data_map,
                                           const FCurve *fcu,
                                           const Span<BeztMap> bezms)
{
  /* At this point, beztmaps are already sorted, so their current index is assumed to be what the
   * BezTriple index will be after sorting. */
  for (const int new_index : bezms.index_range()) {
    const BeztMap &bezm = bezms[new_index];
    if (new_index == bezm.oldIndex && !bezm.swap_handles) {
      /* If the index is the same, any pointers to BezTriple will still point to the correct data.
       * Handles might need to be swapped though. */
      continue;
    }

    TransData2D *td2d;
    TransData *td;

    if (const int *trans_data_index = trans_data_map.lookup_ptr(bezm.bezt->vec[0])) {
      td2d = &tc->data_2d[*trans_data_index];
      if (bezm.swap_handles) {
        td2d->loc2d = fcu->bezt[new_index].vec[2];
      }
      else {
        td2d->loc2d = fcu->bezt[new_index].vec[0];
      }
      td = &tc->data[*trans_data_index];
      update_trans_data(td, fcu, new_index, bezm.swap_handles);
    }
    if (const int *trans_data_index = trans_data_map.lookup_ptr(bezm.bezt->vec[2])) {
      td2d = &tc->data_2d[*trans_data_index];
      if (bezm.swap_handles) {
        td2d->loc2d = fcu->bezt[new_index].vec[0];
      }
      else {
        td2d->loc2d = fcu->bezt[new_index].vec[2];
      }
      td = &tc->data[*trans_data_index];
      update_trans_data(td, fcu, new_index, bezm.swap_handles);
    }
    if (const int *trans_data_index = trans_data_map.lookup_ptr(bezm.bezt->vec[1])) {
      td2d = &tc->data_2d[*trans_data_index];
      td2d->loc2d = fcu->bezt[new_index].vec[1];

      /* If only control point is selected, the handle pointers need to be updated as well. */
      if (td2d->h1) {
        td2d->h1 = fcu->bezt[new_index].vec[0];
      }
      if (td2d->h2) {
        td2d->h2 = fcu->bezt[new_index].vec[2];
      }
      td = &tc->data[*trans_data_index];
      update_trans_data(td, fcu, new_index, bezm.swap_handles);
    }
  }
}

/* This function is called by recalc_data during the Transform loop to recalculate
 * the handles of curves and sort the keyframes so that the curves draw correctly.
 * The Span of FCurves should only contain those that need sorting.
 */
static void remake_graph_transdata(TransInfo *t, const Span<FCurve *> fcurves)
{
  SpaceGraph *sipo = (SpaceGraph *)t->area->spacedata.first;
  const bool use_handle = (sipo->flag & SIPO_NOHANDLES) == 0;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* Build a map from the data that is being modified to its index. This is used to quickly update
   * the pointers to where the data ends up after sorting. */
  Map<float *, int> trans_data_map;
  for (int i = 0; i < tc->data_len; i++) {
    trans_data_map.add(tc->data_2d[i].loc2d, i);
  }

  /* The grain size of 8 was chosen based on measured runtimes of this function. While 1 is the
   * fastest, larger grain sizes are generally preferred and the difference between 1 and 8 was
   * only minimal (~330ms to ~336ms). */
  threading::parallel_for(fcurves.index_range(), 8, [&](const IndexRange range) {
    for (const int i : range) {
      FCurve *fcu = fcurves[i];

      if (!fcu->bezt) {
        continue;
      }

      /* Adjust transform-data pointers. */
      /* NOTE: none of these functions use 'use_handle', it could be removed. */
      Vector<BeztMap> bezms = bezt_to_beztmaps(fcu->bezt, fcu->totvert);
      sort_time_beztmaps(bezms);
      update_transdata_bezt_pointers(tc, trans_data_map, fcu, bezms);

      /* Re-sort actual beztriples
       * (perhaps this could be done using the beztmaps to save time?). */
      sort_time_fcurve(fcu);

      testhandles_fcurve(fcu, BEZT_FLAG_TEMP_TAG, use_handle);
    }
  });
}

static void recalcData_graphedit(TransInfo *t)
{
  SpaceGraph *sipo = (SpaceGraph *)t->area->spacedata.first;
  ViewLayer *view_layer = t->view_layer;

  ListBase anim_data = {nullptr, nullptr};
  bAnimContext ac = {nullptr};
  int filter;

  BKE_view_layer_synced_ensure(t->scene, t->view_layer);

  /* Initialize relevant anim-context 'context' data from TransInfo data. */
  /* NOTE: sync this with the code in #ANIM_animdata_get_context(). */
  ac.bmain = CTX_data_main(t->context);
  ac.scene = t->scene;
  ac.view_layer = t->view_layer;
  ac.obact = BKE_view_layer_active_object_get(view_layer);
  ac.area = t->area;
  ac.region = t->region;
  ac.sl = static_cast<SpaceLink *>((t->area) ? t->area->spacedata.first : nullptr);
  ac.spacetype = eSpace_Type((t->area) ? t->area->spacetype : 0);
  ac.regiontype = eRegion_Type((t->region) ? t->region->regiontype : 0);

  ANIM_animdata_context_getdata(&ac);

  /* Do the flush first. */
  flushTransGraphData(t);

  /* Get curves to check if a re-sort is needed. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVE_VISIBLE |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  Vector<FCurve *> unsorted_fcurves;
  /* Now test if there is a need to re-sort. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;

    /* Ignore FC-Curves without any selected verts. */
    if (!fcu_test_selected(fcu)) {
      continue;
    }

    /* Watch it: if the time is wrong: do not correct handles yet. */
    if (test_time_fcurve(fcu)) {
      unsorted_fcurves.append(fcu);
    }
    else {
      BKE_fcurve_handles_recalc_ex(fcu, BEZT_FLAG_TEMP_TAG);
    }

    /* Set refresh tags for objects using this animation,
     * BUT only if realtime updates are enabled. */
    if ((sipo->flag & SIPO_NOREALTIMEUPDATES) == 0) {
      ANIM_list_elem_update(CTX_data_main(t->context), t->scene, ale);
    }
  }

  /* Do resort and other updates? */
  if (!unsorted_fcurves.is_empty()) {
    remake_graph_transdata(t, unsorted_fcurves);
  }

  /* Now free temp channels. */
  ANIM_animdata_freelist(&anim_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Graph
 * \{ */

static void special_aftertrans_update__graph(bContext *C, TransInfo *t)
{
  SpaceGraph *sipo = (SpaceGraph *)t->area->spacedata.first;
  bAnimContext ac;
  const bool use_handle = (sipo->flag & SIPO_NOHANDLES) == 0;

  const bool canceled = (t->state == TRANS_CANCEL);
  const bool duplicate = (t->flag & T_DUPLICATED_KEYFRAMES) != 0;

  /* Initialize relevant anim-context 'context' data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  if (ac.datatype) {
    ListBase anim_data = {nullptr, nullptr};
    short filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVE_VISIBLE |
                    ANIMFILTER_FCURVESONLY);

    /* Get channels to work on. */
    ANIM_animdata_filter(
        &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      FCurve *fcu = (FCurve *)ale->key_data;

      /* 3 cases here for curve cleanups:
       * 1) NOTRANSKEYCULL on    -> cleanup of duplicates shouldn't be done.
       * 2) canceled == 0        -> user confirmed the transform,
       *                            so duplicates should be removed.
       * 3) canceled + duplicate -> user canceled the transform,
       *                            but we made duplicates, so get rid of these.
       */
      if ((sipo->flag & SIPO_NOTRANSKEYCULL) == 0 && ((canceled == 0) || (duplicate))) {
        ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcu, false, false);
        BKE_fcurve_merge_duplicate_keys(fcu, BEZT_FLAG_TEMP_TAG, use_handle);
        ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcu, true, false);
      }
    }

    /* Free temp memory. */
    ANIM_animdata_freelist(&anim_data);
  }

  /* Make sure all F-Curves are set correctly, but not if transform was
   * canceled, since then curves were already restored to initial state.
   * NOTE: if the refresh is really needed after cancel then some way
   *       has to be added to not update handle types, see #22289.
   */
  if (!canceled) {
    ANIM_editkeyframes_refresh(&ac);
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_Graph = {
    /*flags*/ (T_POINTS | T_2D_EDIT),
    /*create_trans_data*/ createTransGraphEditData,
    /*recalc_data*/ recalcData_graphedit,
    /*special_aftertrans_update*/ special_aftertrans_update__graph,
};

}  // namespace blender::ed::transform
