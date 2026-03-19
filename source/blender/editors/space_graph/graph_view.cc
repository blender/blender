/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spgraph
 */

#include <cmath>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_rect.h"

#include "DNA_anim_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_nla.hh"

#include "UI_view2d.hh"

#include "ED_anim_api.hh"
#include "ED_markers.hh"
#include "ED_screen.hh"
#include "ED_space_graph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "graph_intern.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Calculate Range
 * \{ */

/* Used to ensure that the extents are not too extreme that view implodes. We need different
 * values for x and y due to the nature of the data displayed. The minimum distance on x for
 * keyframes is BEZT_BINARYSEARCH_THRESH so differences smaller than that cannot occur. For the y
 * value there is no such limit, so we have to choose a smaller number. The units are frames for
 * the x-axis and value for the y-axis. */
constexpr float2 zoom_threshold(BEZT_BINARYSEARCH_THRESH, 0.0001f);

/**
 * Sets the given rect to reasonable defaults. Useful in case no bounds could be found by other
 * means.
 */
static void keyframe_bounds_defaults(bAnimContext &ac, rctf &r_bounds)
{
  /* Set default range. */
  if (ac.scene) {
    Scene *scene = ac.scene;
    r_bounds.xmin = float(PSFRA);
    /* The scene range can have the same start and end frame. */
    r_bounds.xmax = max_ff(float(PEFRA), r_bounds.xmin + 1);
  }
  else {
    /* The hardcoded values for x and y are completely arbitrary. */
    r_bounds.xmin = -5;
    r_bounds.xmax = 100;
  }
  r_bounds.ymin = -5;
  r_bounds.ymax = 5;
}

/**
 * Return the bounds of keyframes elements in anim_data. The bounds will be in frame (x) and value
 * (y) units.
 *
 * For a description on the arguments, see `BKE_fcurve_calc_bounds`.
 *
 * \returns true if any bounds are found. If false is returned the `r_bounds` are not a valid rect.
 */
static bool calculate_keyframe_bounds(const ListBaseT<bAnimListElem> &anim_data,
                                      bAnimContext &ac,
                                      const bool only_selected,
                                      const bool include_handles,
                                      rctf &r_bounds)
{
  /* Check if any channels to set range with. */
  if (!anim_data.first) {
    return false;
  }
  constexpr float inf = std::numeric_limits<float>::infinity();
  r_bounds.xmin = inf;
  r_bounds.xmax = -inf;
  r_bounds.ymin = inf;
  r_bounds.ymax = -inf;

  /* Go through channels, finding max extents. */
  for (bAnimListElem &ale : anim_data) {
    FCurve *fcu = static_cast<FCurve *>(ale.key_data);
    rctf fcu_bounds;

    /* Get range. */
    if (!BKE_fcurve_calc_bounds(fcu, only_selected, include_handles, nullptr, &fcu_bounds)) {
      continue;
    }
    const short mapping_flag = ANIM_get_normalization_flags(ac.sl);

    /* Apply NLA scaling. */
    fcu_bounds.xmin = ANIM_nla_tweakedit_remap(&ale, fcu_bounds.xmin, NLATIME_CONVERT_MAP);
    fcu_bounds.xmax = ANIM_nla_tweakedit_remap(&ale, fcu_bounds.xmax, NLATIME_CONVERT_MAP);

    /* Apply unit corrections. */
    float offset;
    const float unitFac = ANIM_unit_mapping_get_factor(
        ac.scene, ale.id, fcu, mapping_flag, &offset);
    fcu_bounds.ymin += offset;
    fcu_bounds.ymax += offset;
    fcu_bounds.ymin *= unitFac;
    fcu_bounds.ymax *= unitFac;
    BLI_rctf_sanitize(&fcu_bounds);
    BLI_rctf_union(&r_bounds, &fcu_bounds);
  }
  return BLI_rctf_is_valid(&r_bounds);
}

void get_graph_keyframe_extents(bAnimContext *ac,
                                float *xmin,
                                float *xmax,
                                float *ymin,
                                float *ymax,
                                const bool do_sel_only,
                                const bool include_handles)
{
  ListBaseT<bAnimListElem> anim_data = ed::graph::get_editable_fcurves(*ac);
  rctf fcurve_bounds;
  const bool foundBounds = calculate_keyframe_bounds(
      anim_data, *ac, do_sel_only, include_handles, fcurve_bounds);
  ANIM_animdata_freelist(&anim_data);

  /* Ensure that the extents are not too extreme that view implodes. */
  if (foundBounds) {
    if (fabsf(fcurve_bounds.xmax - fcurve_bounds.xmin) < zoom_threshold.x) {
      fcurve_bounds.xmin -= zoom_threshold.x / 2;
      fcurve_bounds.xmax += zoom_threshold.x / 2;
    }
    if (fabsf(fcurve_bounds.ymax - fcurve_bounds.ymin) < zoom_threshold.y) {
      fcurve_bounds.ymin -= zoom_threshold.y / 2;
      fcurve_bounds.ymax += zoom_threshold.y / 2;
    }
  }
  else {
    keyframe_bounds_defaults(*ac, fcurve_bounds);
  }

  *xmin = fcurve_bounds.xmin;
  *xmax = fcurve_bounds.xmax;
  if (ymin) {
    *ymin = fcurve_bounds.ymin;
  }
  if (ymax) {
    *ymax = fcurve_bounds.ymax;
  }
}

/**
 * Adds padding to the given view bounds based on surrounding keyframe data of selected keys.
 */
static void add_contextual_padding(bAnimContext &ac,
                                   ListBaseT<bAnimListElem> &anim_data,
                                   const bool pad_x,
                                   const bool pad_y,
                                   rctf &r_bounds)
{
  if (!pad_x && !pad_y) {
    return;
  }
  const short mapping_flag = ANIM_get_normalization_flags(ac.sl);
  for (bAnimListElem &ale : anim_data) {
    FCurve *fcu = static_cast<FCurve *>(ale.key_data);
    if (!fcu->bezt || fcu->totvert == 0) {
      continue;
    }
    float offset;
    const float unit_factor = ANIM_unit_mapping_get_factor(
        ac.scene, ale.id, fcu, mapping_flag, &offset);
    for (int i : IndexRange(fcu->totvert)) {
      BezTriple *bezt = &fcu->bezt[i];
      if (!BEZT_ISSEL_ANY(bezt)) {
        continue;
      }
      const float2 key(bezt->vec[1][0], (bezt->vec[1][1] + offset) * unit_factor);
      /* Check the previous/next key if they exist and add half the distance to them as padding to
       * the bounds. */
      if (i - 1 >= 0) {
        const float2 prev_key(fcu->bezt[i - 1].vec[1][0],
                              (fcu->bezt[i - 1].vec[1][1] + offset) * unit_factor);
        if (pad_x) {
          const float pad = (key.x - prev_key.x) / 2.0;
          BLI_rctf_union_x(&r_bounds, key.x - pad);
        }
        if (pad_y) {
          const float pad = fabsf(prev_key.y - key.y) / 2.0;
          /* Add to top and bottom to keep selection centered. */
          BLI_rctf_union_y(&r_bounds, key.y - pad);
          BLI_rctf_union_y(&r_bounds, key.y + pad);
        }
      }
      if (i + 1 < fcu->totvert) {
        const float2 next_key(fcu->bezt[i + 1].vec[1][0],
                              (fcu->bezt[i + 1].vec[1][1] + offset) * unit_factor);
        if (pad_x) {
          const float pad = (next_key.x - key.x) / 2.0;
          BLI_rctf_union_x(&r_bounds, key.x + pad);
        }
        if (pad_y) {
          const float pad = fabsf(next_key.y - key.y) / 2.0;
          /* Add to top and bottom to keep selection centered. */
          BLI_rctf_union_y(&r_bounds, key.y - pad);
          BLI_rctf_union_y(&r_bounds, key.y + pad);
        }
      }
    }
  }
}

/**
 * Generate a rect for framing keyframes in the graph editor.
 * This function tries to find a reasonable minimum bound for the case where the actual bounds are
 * 0 on either axis.
 */
static void get_graph_view_bounds(bAnimContext *ac,
                                  rctf &r_bounds,
                                  const bool only_selected,
                                  const bool include_handles)
{
  ListBaseT<bAnimListElem> anim_data = ed::graph::get_editable_fcurves(*ac);
  const bool found_bounds = calculate_keyframe_bounds(
      anim_data, *ac, only_selected, include_handles, r_bounds);

  if (found_bounds) {
    if (only_selected) {
      /* Only adding contextual padding when looking at selected keys. When framing all data,
       * there is no additional information we could possibly use. */
      add_contextual_padding(*ac,
                             anim_data,
                             BLI_rctf_size_x(&r_bounds) < zoom_threshold.x,
                             BLI_rctf_size_y(&r_bounds) < zoom_threshold.y,
                             r_bounds);
    }
    if (fabsf(r_bounds.xmax - r_bounds.xmin) < zoom_threshold.x) {
      r_bounds.xmin -= zoom_threshold.x / 2;
      r_bounds.xmax += zoom_threshold.x / 2;
    }
    if (fabsf(r_bounds.ymax - r_bounds.ymin) < zoom_threshold.y) {
      r_bounds.ymin -= zoom_threshold.y / 2;
      r_bounds.ymax += zoom_threshold.y / 2;
    }
  }
  else {
    keyframe_bounds_defaults(*ac, r_bounds);
  }

  ANIM_animdata_freelist(&anim_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Automatic Preview-Range Operator
 * \{ */

static wmOperatorStatus graphkeys_previewrange_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;
  Scene *scene;
  float min, max;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }
  if (ac.scene == nullptr) {
    return OPERATOR_CANCELLED;
  }

  scene = ac.scene;

  /* Set the range directly. */
  get_graph_keyframe_extents(&ac, &min, &max, nullptr, nullptr, true, false);
  scene->r.flag |= SCER_PRV_RANGE;
  scene->r.psfra = round_fl_to_int(min);
  scene->r.pefra = round_fl_to_int(max);

  /* Set notifier that things have changed. */
  /* XXX: Err... there's nothing for frame ranges yet, but this should do fine too. */
  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, ac.scene);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_previewrange_set(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Set Preview Range to Selected";
  ot->idname = "GRAPH_OT_previewrange_set";
  ot->description = "Set Preview Range based on range of selected keyframes";

  /* API callbacks */
  ot->exec = graphkeys_previewrange_exec;
  /* XXX: unchecked poll to get F-samples working too, but makes modifier damage trickier. */
  ot->poll = ED_operator_graphedit_active;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View-All Operator
 * \{ */

static wmOperatorStatus graphkeys_viewall(bContext *C,
                                          const bool do_sel_only,
                                          const bool include_handles,
                                          const int smooth_viewtx)
{
  bAnimContext ac;
  rctf cur_new;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Set the horizontal range, with an extra offset so that the extreme keys will be in view. */
  get_graph_view_bounds(&ac, cur_new, do_sel_only, include_handles);

  /* Give some more space at the borders. */
  cur_new = ANIM_frame_range_view2d_add_xmargin(ac.region->v2d, cur_new);
  BLI_rctf_resize_y(&cur_new, 1.1f * BLI_rctf_size_y(&cur_new));

  /* Take regions into account, that could block the view.
   * Marker region is supposed to be larger than the scroll-bar, so prioritize it. */
  float pad_top = UI_TIME_SCRUB_MARGIN_Y;
  float pad_bottom = BLI_listbase_is_empty(ED_context_get_markers(C)) ? V2D_SCROLL_HANDLE_HEIGHT :
                                                                        UI_MARKER_MARGIN_Y;
  BLI_rctf_pad_y(&cur_new, ac.region->winy, pad_bottom, pad_top);

  ui::view2d_smooth_view(C, ac.region, &cur_new, smooth_viewtx);
  return OPERATOR_FINISHED;
}

/* ......... */

static wmOperatorStatus graphkeys_viewall_exec(bContext *C, wmOperator *op)
{
  const bool include_handles = RNA_boolean_get(op->ptr, "include_handles");
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  /* Whole range */
  return graphkeys_viewall(C, false, include_handles, smooth_viewtx);
}

static wmOperatorStatus graphkeys_view_selected_exec(bContext *C, wmOperator *op)
{
  const bool include_handles = RNA_boolean_get(op->ptr, "include_handles");
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  /* Only selected. */
  return graphkeys_viewall(C, true, include_handles, smooth_viewtx);
}

/* ......... */

void GRAPH_OT_view_all(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Frame All";
  ot->idname = "GRAPH_OT_view_all";
  ot->description = "Reset viewable area to show full keyframe range";

  /* API callbacks */
  ot->exec = graphkeys_viewall_exec;
  /* XXX: Unchecked poll to get F-samples working too, but makes modifier damage trickier. */
  ot->poll = ED_operator_graphedit_active;

  /* Flags */
  ot->flag = 0;

  /* Props */
  ot->prop = RNA_def_boolean(ot->srna,
                             "include_handles",
                             true,
                             "Include Handles",
                             "Include handles of keyframes when calculating extents");
}

void GRAPH_OT_view_selected(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Frame Selected";
  ot->idname = "GRAPH_OT_view_selected";
  ot->description = "Reset viewable area to show selected keyframe range";

  /* API callbacks */
  ot->exec = graphkeys_view_selected_exec;
  /* XXX: Unchecked poll to get F-samples working too, but makes modifier damage trickier. */
  ot->poll = ED_operator_graphedit_active;

  /* Flags */
  ot->flag = 0;

  /* Props */
  ot->prop = RNA_def_boolean(ot->srna,
                             "include_handles",
                             true,
                             "Include Handles",
                             "Include handles of keyframes when calculating extents");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Frame Operator
 * \{ */

static wmOperatorStatus graphkeys_view_frame_exec(bContext *C, wmOperator *op)
{
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
  ANIM_center_frame(C, smooth_viewtx);
  return OPERATOR_FINISHED;
}

void GRAPH_OT_view_frame(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Go to Current Frame";
  ot->idname = "GRAPH_OT_view_frame";
  ot->description = "Move the view to the current frame";

  /* API callbacks */
  ot->exec = graphkeys_view_frame_exec;
  ot->poll = ED_operator_graphedit_active;

  /* Flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Create Ghost-Curves Operator
 *
 * This operator samples the data of the selected F-Curves to F-Points, storing them
 * as 'ghost curves' in the active Graph Editor.
 * \{ */

/* Bake each F-Curve into a set of samples, and store as a ghost curve. */
static void create_ghost_curves(bAnimContext *ac, int start, int end)
{
  SpaceGraph *sipo = reinterpret_cast<SpaceGraph *>(ac->sl);
  ListBaseT<bAnimListElem> anim_data = {nullptr, nullptr};
  int filter;

  /* Free existing ghost curves. */
  BKE_fcurves_free(&sipo->runtime.ghost_curves);

  /* Sanity check. */
  if (start >= end) {
    printf("Error: Frame range for Ghost F-Curve creation is inappropriate\n");
    return;
  }

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_SEL | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through filtered data and add keys between selected keyframes on every frame. */
  for (bAnimListElem &ale : anim_data) {
    FCurve *fcu = static_cast<FCurve *>(ale.key_data);
    FCurve *gcu = BKE_fcurve_create();
    ChannelDriver *driver = fcu->driver;
    FPoint *fpt;
    float unitFac, offset;
    int cfra;
    short mapping_flag = ANIM_get_normalization_flags(ac->sl);

    /* Disable driver so that it don't muck up the sampling process. */
    fcu->driver = nullptr;

    /* Calculate unit-mapping factor. */
    unitFac = ANIM_unit_mapping_get_factor(ac->scene, ale.id, fcu, mapping_flag, &offset);

    /* Create samples, but store them in a new curve
     * - we cannot use fcurve_store_samples() as that will only overwrite the original curve.
     */
    gcu->fpt = fpt = MEM_new_array_zeroed<FPoint>((end - start + 1), "Ghost FPoint Samples");
    gcu->totvert = end - start + 1;

    /* Use the sampling callback at 1-frame intervals from start to end frames. */
    for (cfra = start; cfra <= end; cfra++, fpt++) {
      const float cfrae = ANIM_nla_tweakedit_remap(&ale, cfra, NLATIME_CONVERT_UNMAP);

      fpt->vec[0] = cfrae;
      fpt->vec[1] = (fcurve_samplingcb_evalcurve(fcu, nullptr, cfrae) + offset) * unitFac;
    }

    /* Set color of ghost curve
     * - make the color slightly darker.
     */
    gcu->color[0] = fcu->color[0] - 0.07f;
    gcu->color[1] = fcu->color[1] - 0.07f;
    gcu->color[2] = fcu->color[2] - 0.07f;

    /* Store new ghost curve. */
    BLI_addtail(&sipo->runtime.ghost_curves, gcu);

    /* Restore driver. */
    fcu->driver = driver;
  }

  /* Admin and redraws. */
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static wmOperatorStatus graphkeys_create_ghostcurves_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;
  View2D *v2d;
  int start, end;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Ghost curves are snapshots of the visible portions of the curves,
   * so set range to be the visible range. */
  v2d = &ac.region->v2d;
  start = int(v2d->cur.xmin);
  end = int(v2d->cur.xmax);

  /* Bake selected curves into a ghost curve. */
  create_ghost_curves(&ac, start, end);

  /* Update this editor only. */
  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

void GRAPH_OT_ghost_curves_create(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Create Ghost Curves";
  ot->idname = "GRAPH_OT_ghost_curves_create";
  ot->description =
      "Create snapshot (Ghosts) of selected F-Curves as background aid for active Graph Editor";

  /* API callbacks */
  ot->exec = graphkeys_create_ghostcurves_exec;
  ot->poll = graphop_visible_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* TODO: add props for start/end frames */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Ghost-Curves Operator
 *
 * This operator clears the 'ghost curves' for the active Graph Editor.
 * \{ */

static wmOperatorStatus graphkeys_clear_ghostcurves_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;
  SpaceGraph *sipo;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }
  sipo = reinterpret_cast<SpaceGraph *>(ac.sl);

  /* If no ghost curves, don't do anything. */
  if (BLI_listbase_is_empty(&sipo->runtime.ghost_curves)) {
    return OPERATOR_CANCELLED;
  }
  /* Free ghost curves. */
  BKE_fcurves_free(&sipo->runtime.ghost_curves);

  /* Update this editor only. */
  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

void GRAPH_OT_ghost_curves_clear(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Clear Ghost Curves";
  ot->idname = "GRAPH_OT_ghost_curves_clear";
  ot->description = "Clear F-Curve snapshots (Ghosts) for active Graph Editor";

  /* API callbacks */
  ot->exec = graphkeys_clear_ghostcurves_exec;
  ot->poll = ED_operator_graphedit_active;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

}  // namespace blender
