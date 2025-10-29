/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "BLI_bounds_types.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.hh"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "BKE_context.hh"
#include "BKE_scene.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_define.hh"

#include "UI_view2d.hh"

#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"

/* For menu, popup, icons, etc. */
#include "ED_anim_api.hh"
#include "ED_markers.hh"
#include "ED_screen.hh"
#include "ED_sequencer.hh"
#include "ED_util_imbuf.hh"

/* Own include. */
#include "sequencer_intern.hh"

namespace blender::ed::vse {

/* -------------------------------------------------------------------- */
/** \name Sequencer Sample Backdrop Operator
 * \{ */

void SEQUENCER_OT_sample(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sample Color";
  ot->idname = "SEQUENCER_OT_sample";
  ot->description = "Use mouse to sample color in current frame";

  /* API callbacks. */
  ot->invoke = ED_imbuf_sample_invoke;
  ot->modal = ED_imbuf_sample_modal;
  ot->cancel = ED_imbuf_sample_cancel;
  ot->poll = ED_imbuf_sample_poll;

  /* Flags. */
  ot->flag = OPTYPE_BLOCKING;

  /* Not implemented. */
  PropertyRNA *prop;
  prop = RNA_def_int(ot->srna, "size", 1, 1, 128, "Sample Size", "", 1, 64);
  RNA_def_property_subtype(prop, PROP_PIXEL);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sequencer Frame All Operator
 * \{ */
void SEQ_get_timeline_region_padding(const bContext *C, float *r_pad_top, float *r_pad_bottom)
{
  *r_pad_top = UI_TIME_SCRUB_MARGIN_Y;
  const SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (sseq->flag & SEQ_SHOW_OVERLAY && sseq->cache_overlay.flag & SEQ_CACHE_SHOW &&
      sseq->cache_overlay.flag & SEQ_CACHE_SHOW_FINAL_OUT)
  {
    *r_pad_top += UI_TIME_CACHE_MARGIN_Y;
  }

  *r_pad_bottom = BLI_listbase_is_empty(ED_sequencer_context_get_markers(C)) ?
                      V2D_SCROLL_HANDLE_HEIGHT :
                      UI_MARKER_MARGIN_Y;
}

void SEQ_add_timeline_region_padding(const bContext *C, rctf *view_box)
{
  /* Calculate and add margin to the view.
   * This is needed so that the focused strips are not occluded by the scrub area or other
   * overlays.
   */
  float pad_top, pad_bottom;
  SEQ_get_timeline_region_padding(C, &pad_top, &pad_bottom);

  ARegion *region = CTX_wm_region(C);
  BLI_rctf_pad_y(view_box, region->winy, pad_bottom, pad_top);
}

static wmOperatorStatus sequencer_view_all_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  rctf box;

  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
  Scene *scene = CTX_data_sequencer_scene(C);
  const Editing *ed = seq::editing_get(scene);

  seq::timeline_init_boundbox(scene, &box);
  MetaStack *ms = seq::meta_stack_active_get(ed);
  /* Use meta strip range instead of scene. */
  if (ms != nullptr) {
    box.xmin = ms->disp_range[0] - 1;
    box.xmax = ms->disp_range[1] + 1;
  }
  seq::timeline_expand_boundbox(scene, seq::active_seqbase_get(ed), &box);

  SEQ_add_timeline_region_padding(C, &box);

  UI_view2d_smooth_view(C, region, &box, smooth_viewtx);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_all(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Frame All";
  ot->idname = "SEQUENCER_OT_view_all";
  ot->description = "View all the strips in the sequencer";

  /* API callbacks. */
  ot->exec = sequencer_view_all_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Go to Current Frame Operator
 * \{ */

static wmOperatorStatus sequencer_view_frame_exec(bContext *C, wmOperator *op)
{
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
  ANIM_center_frame(C, smooth_viewtx);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_frame(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Go to Current Frame";
  ot->idname = "SEQUENCER_OT_view_frame";
  ot->description = "Move the view to the current frame";

  /* API callbacks. */
  ot->exec = sequencer_view_frame_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = 0;
}

/** \} */

/* For frame all/selected operators, when we are in preview region
 * with histogram/waveform display mode, frame the extents of the scope. */
static bool view_frame_preview_scope(bContext *C, wmOperator *op, ARegion *region)
{
  if (!region || region->regiontype != RGN_TYPE_PREVIEW) {
    return false;
  }
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (!sseq) {
    return false;
  }
  const View2D *v2d = UI_view2d_fromcontext(C);
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  if (sseq->mainb == SEQ_DRAW_IMG_HISTOGRAM) {
    /* For histogram scope, use extents of the histogram. */
    const ScopeHistogram &hist = sseq->runtime->scopes.histogram;
    if (hist.data.is_empty()) {
      return false;
    }

    rctf cur_new = v2d->tot;
    const float val_max = ScopeHistogram::bin_to_float(math::reduce_max(hist.max_bin));
    cur_new.xmax = cur_new.xmin + (cur_new.xmax - cur_new.xmin) * val_max;

    /* Add some padding around whole histogram. */
    BLI_rctf_scale(&cur_new, 1.1f);

    UI_view2d_smooth_view(C, region, &cur_new, smooth_viewtx);
    return true;
  }

  if (ELEM(sseq->mainb, SEQ_DRAW_IMG_WAVEFORM, SEQ_DRAW_IMG_RGBPARADE)) {
    /* For waveform/parade scopes, use 3.0 display space Y value as bounds
     * for HDR content. */
    const bool hdr = sseq->runtime->scopes.last_ibuf_float;
    rctf cur_new = v2d->tot;
    if (hdr) {
      const float val_max = 3.0f;
      cur_new.ymax = cur_new.ymin + (cur_new.ymax - cur_new.ymin) * val_max;
    }
    UI_view2d_smooth_view(C, region, &cur_new, smooth_viewtx);
    return true;
  }

  return false;
}

/* -------------------------------------------------------------------- */
/** \name Preview Frame All Operator
 * \{ */

static wmOperatorStatus sequencer_view_all_preview_exec(bContext *C, wmOperator *op)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);

  if (view_frame_preview_scope(C, op, CTX_wm_region(C))) {
    return OPERATOR_FINISHED;
  }

#if 0
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_sequencer_scene(C);
#endif
  View2D *v2d = UI_view2d_fromcontext(C);

  v2d->cur = v2d->tot;
  UI_view2d_curRect_changed(C, v2d);
  UI_view2d_sync(screen, area, v2d, V2D_LOCK_COPY);

#if 0
  /* Like zooming on an image view. */
  float zoomX, zoomY;
  int width, height, imgwidth, imgheight;

  width = region->winx;
  height = region->winy;

  seq_reset_imageofs(sseq);

  BKE_render_resolution(&scene->r, false, &imgwidth, &imgheight);

  /* Apply aspect, doesn't need to be that accurate. */
  imgwidth = int(imgwidth * (scene->r.xasp / scene->r.yasp));

  if (((imgwidth >= width) || (imgheight >= height)) && ((width > 0) && (height > 0))) {
    /* Find the zoom value that will fit the image in the image space. */
    zoomX = float(width) / float(imgwidth);
    zoomY = float(height) / float(imgheight);
    sseq->zoom = (zoomX < zoomY) ? zoomX : zoomY;

    sseq->zoom = 1.0f / power_of_2(1 / min_ff(zoomX, zoomY));
  }
  else {
    sseq->zoom = 1.0f;
  }
#endif

  sseq->flag |= SEQ_ZOOM_TO_FIT;

  ED_area_tag_redraw(CTX_wm_area(C));
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_all_preview(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Frame All";
  ot->idname = "SEQUENCER_OT_view_all_preview";
  ot->description = "Zoom preview to fit in the area";

  /* API callbacks. */
  ot->exec = sequencer_view_all_preview_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sequencer View Zoom Ratio Operator
 * \{ */

static wmOperatorStatus sequencer_view_zoom_ratio_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  const RenderData *rd = &scene->r;
  View2D *v2d = UI_view2d_fromcontext(C);

  float ratio = RNA_float_get(op->ptr, "ratio");

  int winx, winy;
  BKE_render_resolution(rd, false, &winx, &winy);

  float facx = (BLI_rcti_size_x(&v2d->mask) + 1) / float(winx);
  float facy = (BLI_rcti_size_y(&v2d->mask) + 1) / float(winy);

  BLI_rctf_resize(&v2d->cur, ceilf(winx * facx / ratio + 0.5f), ceilf(winy * facy / ratio + 0.5f));

  ED_region_tag_redraw(CTX_wm_region(C));

  UI_view2d_curRect_changed(C, v2d);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_zoom_ratio(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sequencer View Zoom Ratio";
  ot->idname = "SEQUENCER_OT_view_zoom_ratio";
  ot->description = "Change zoom ratio of sequencer preview";

  /* API callbacks. */
  ot->exec = sequencer_view_zoom_ratio_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Properties. */
  RNA_def_float(ot->srna,
                "ratio",
                1.0f,
                -FLT_MAX,
                FLT_MAX,
                "Ratio",
                "Zoom ratio, 1.0 is 1:1, higher is zoomed in, lower is zoomed out",
                -FLT_MAX,
                FLT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Selected Operator
 * \{ */

static void seq_view_collection_rect_preview(Scene *scene, Span<Strip *> strips, rctf *rect)
{
  const Bounds<float2> box = seq::image_transform_bounding_box_from_collection(
      scene, strips, true);

  rect->xmin = box.min[0];
  rect->xmax = box.max[0];
  rect->ymin = box.min[1];
  rect->ymax = box.max[1];

  float minsize = min_ff(BLI_rctf_size_x(rect), BLI_rctf_size_y(rect));

  /* If the size of the strip is smaller than a pixel, add padding to prevent division by zero. */
  if (minsize < 1.0f) {
    BLI_rctf_pad(rect, 20.0f, 20.0f);
  }

  /* Add padding. */
  BLI_rctf_scale(rect, 1.1f);
}

static void seq_view_collection_rect_timeline(const bContext *C, Span<Strip *> strips, rctf *rect)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  int xmin = MAXFRAME * 2;
  int xmax = -MAXFRAME * 2;
  int ymin = seq::MAX_CHANNELS + 1;
  int ymax = 0;
  int xmargin = scene->frames_per_second();

  for (Strip *strip : strips) {
    xmin = min_ii(xmin, seq::time_left_handle_frame_get(scene, strip));
    xmax = max_ii(xmax, seq::time_right_handle_frame_get(scene, strip));

    ymin = min_ii(ymin, strip->channel);
    /* "+1" because each channel has a thickness of 1. */
    ymax = max_ii(ymax, strip->channel + 1);
  }

  xmax += xmargin;
  xmin -= xmargin;

  float orig_height = BLI_rctf_size_y(rect);
  rctf new_viewport;

  new_viewport.xmin = xmin;
  new_viewport.xmax = xmax;

  new_viewport.ymin = ymin;
  new_viewport.ymax = ymax;

  SEQ_add_timeline_region_padding(C, &new_viewport);

  /* Y axis should only zoom out if needed, never zoom in. */
  if (orig_height > BLI_rctf_size_y(&new_viewport)) {
    /* Get the current max/min channel we can display. */
    const Editing *ed = seq::editing_get(scene);
    rctf box;
    seq::timeline_boundbox(scene, seq::active_seqbase_get(ed), &box);
    SEQ_add_timeline_region_padding(C, &box);
    float timeline_ymin = box.ymin;
    float timeline_ymax = box.ymax;

    if (orig_height > timeline_ymax - timeline_ymin) {
      /* Only apply the x axis movement, we can't align the viewport any better
       * on the y-axis if we are zoomed out further than the current timeline bounds. */
      rect->xmin = new_viewport.xmin;
      rect->xmax = new_viewport.xmax;
      return;
    }

    float ymid = BLI_rctf_cent_y(&new_viewport);

    new_viewport.ymin = ymid - (orig_height / 2.0f);
    new_viewport.ymax = ymid + (orig_height / 2.0f);

    if (new_viewport.ymin < timeline_ymin) {
      new_viewport.ymin = timeline_ymin;
      new_viewport.ymax = new_viewport.ymin + orig_height;
    }
    else if (new_viewport.ymax > timeline_ymax) {
      new_viewport.ymax = timeline_ymax;
      new_viewport.ymin = new_viewport.ymax - orig_height;
    }
  }
  *rect = new_viewport;
}

static wmOperatorStatus sequencer_view_selected_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = UI_view2d_fromcontext(C);
  rctf cur_new = v2d->cur;

  if (view_frame_preview_scope(C, op, region)) {
    return OPERATOR_FINISHED;
  }

  VectorSet strips = selected_strips_from_context(C);
  if (strips.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  if (sequencer_view_has_preview_poll(C) && !sequencer_view_preview_only_poll(C)) {
    return OPERATOR_CANCELLED;
  }

  if (region && region->regiontype == RGN_TYPE_PREVIEW) {
    seq_view_collection_rect_preview(scene, strips, &cur_new);
  }
  else {
    seq_view_collection_rect_timeline(C, strips, &cur_new);
  }

  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
  UI_view2d_smooth_view(C, region, &cur_new, smooth_viewtx);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_selected(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Frame Selected";
  ot->idname = "SEQUENCER_OT_view_selected";
  ot->description = "Zoom the sequencer on the selected strips";

  /* API callbacks. */
  ot->exec = sequencer_view_selected_exec;
  ot->poll = sequencer_editing_initialized_and_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Border Offset View Operator
 * \{ */

static wmOperatorStatus view_ghost_border_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  View2D *v2d = UI_view2d_fromcontext(C);

  rctf rect;

  /* Convert coordinates of rect to 'tot' rect coordinates. */
  WM_operator_properties_border_to_rctf(op, &rect);
  UI_view2d_region_to_view_rctf(v2d, &rect, &rect);

  rect.xmin /= fabsf(BLI_rctf_size_x(&v2d->tot));
  rect.ymin /= fabsf(BLI_rctf_size_y(&v2d->tot));

  rect.xmax /= fabsf(BLI_rctf_size_x(&v2d->tot));
  rect.ymax /= fabsf(BLI_rctf_size_y(&v2d->tot));

  rect.xmin += 0.5f;
  rect.xmax += 0.5f;
  rect.ymin += 0.5f;
  rect.ymax += 0.5f;

  CLAMP(rect.xmin, 0.0f, 1.0f);
  CLAMP(rect.ymin, 0.0f, 1.0f);
  CLAMP(rect.xmax, 0.0f, 1.0f);
  CLAMP(rect.ymax, 0.0f, 1.0f);

  scene->ed->overlay_frame_rect = rect;

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_ghost_border(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Border Offset View";
  ot->idname = "SEQUENCER_OT_view_ghost_border";
  ot->description = "Set the boundaries of the border used for offset view";

  /* API callbacks. */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = view_ghost_border_exec;
  ot->modal = WM_gesture_box_modal;
  ot->poll = sequencer_view_has_preview_poll;
  ot->cancel = WM_gesture_box_cancel;

  /* Flags. */
  ot->flag = 0;

  /* Properties. */
  WM_operator_properties_gesture_box(ot);
}

/** \} */

}  // namespace blender::ed::vse
