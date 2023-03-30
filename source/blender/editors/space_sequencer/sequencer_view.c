/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation */

/** \file
 * \ingroup spseq
 */

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_scene.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"

#include "UI_view2d.h"

#include "SEQ_iterator.h"
#include "SEQ_select.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"

/* For menu, popup, icons, etc. */
#include "ED_anim_api.h"
#include "ED_screen.h"
#include "ED_time_scrub_ui.h"
#include "ED_util_imbuf.h"

/* Own include. */
#include "sequencer_intern.h"

/* -------------------------------------------------------------------- */
/** \name Sequencer Sample Backdrop Operator
 * \{ */

void SEQUENCER_OT_sample(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sample Color";
  ot->idname = "SEQUENCER_OT_sample";
  ot->description = "Use mouse to sample color in current frame";

  /* Api callbacks. */
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

static int sequencer_view_all_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  rctf box;

  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
  Scene *scene = CTX_data_scene(C);
  const Editing *ed = SEQ_editing_get(scene);

  SEQ_timeline_init_boundbox(scene, &box);
  MetaStack *ms = SEQ_meta_stack_active_get(ed);
  /* Use meta strip range instead of scene. */
  if (ms != NULL) {
    box.xmin = ms->disp_range[0] - 1;
    box.xmax = ms->disp_range[1] + 1;
  }
  SEQ_timeline_expand_boundbox(scene, SEQ_active_seqbase_get(ed), &box);

  View2D *v2d = &region->v2d;
  rcti scrub_rect;
  ED_time_scrub_region_rect_get(region, &scrub_rect);
  const float pixel_view_size_y = BLI_rctf_size_y(&v2d->cur) / BLI_rcti_size_y(&v2d->mask);
  const float scrub_bar_height = BLI_rcti_size_y(&scrub_rect) * pixel_view_size_y;

  /* Channel n has range of <n, n+1>. */
  box.ymax += 1.0f + scrub_bar_height;

  UI_view2d_smooth_view(C, region, &box, smooth_viewtx);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_all(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Frame All";
  ot->idname = "SEQUENCER_OT_view_all";
  ot->description = "View all the strips in the sequencer";

  /* Api callbacks. */
  ot->exec = sequencer_view_all_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Go to Current Frame Operator
 * \{ */

static int sequencer_view_frame_exec(bContext *C, wmOperator *op)
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

  /* Api callbacks. */
  ot->exec = sequencer_view_frame_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preview Frame All Operator
 * \{ */

static int sequencer_view_all_preview_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);
#if 0
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
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
  imgwidth = (int)(imgwidth * (scene->r.xasp / scene->r.yasp));

  if (((imgwidth >= width) || (imgheight >= height)) && ((width > 0) && (height > 0))) {
    /* Find the zoom value that will fit the image in the image space. */
    zoomX = ((float)width) / ((float)imgwidth);
    zoomY = ((float)height) / ((float)imgheight);
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

  /* Api callbacks. */
  ot->exec = sequencer_view_all_preview_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sequencer View Zoom Ratio Operator
 * \{ */

static int sequencer_view_zoom_ratio_exec(bContext *C, wmOperator *op)
{
  RenderData *rd = &CTX_data_scene(C)->r;
  View2D *v2d = UI_view2d_fromcontext(C);

  float ratio = RNA_float_get(op->ptr, "ratio");

  int winx, winy;
  BKE_render_resolution(rd, false, &winx, &winy);

  float facx = BLI_rcti_size_x(&v2d->mask) / (float)winx;
  float facy = BLI_rcti_size_y(&v2d->mask) / (float)winy;

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

  /* Api callbacks. */
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

static void seq_view_collection_rect_preview(Scene *scene, SeqCollection *strips, rctf *rect)
{
  float min[2], max[2];
  SEQ_image_transform_bounding_box_from_collection(scene, strips, true, min, max);

  rect->xmin = min[0];
  rect->xmax = max[0];
  rect->ymin = min[1];
  rect->ymax = max[1];

  float minsize = min_ff(BLI_rctf_size_x(rect), BLI_rctf_size_y(rect));

  /* If the size of the strip is smaller than a pixel, add padding to prevent division by zero. */
  if (minsize < 1.0f) {
    BLI_rctf_pad(rect, 20.0f, 20.0f);
  }

  /* Add padding. */
  BLI_rctf_scale(rect, 1.1f);
}

static void seq_view_collection_rect_timeline(Scene *scene, SeqCollection *strips, rctf *rect)
{
  Sequence *seq;

  int xmin = MAXFRAME * 2;
  int xmax = -MAXFRAME * 2;
  int ymin = MAXSEQ + 1;
  int ymax = 0;
  int orig_height;
  int ymid;
  int ymargin = 1;
  int xmargin = FPS;

  SEQ_ITERATOR_FOREACH (seq, strips) {
    xmin = min_ii(xmin, SEQ_time_left_handle_frame_get(scene, seq));
    xmax = max_ii(xmax, SEQ_time_right_handle_frame_get(scene, seq));

    ymin = min_ii(ymin, seq->machine);
    ymax = max_ii(ymax, seq->machine);
  }

  xmax += xmargin;
  xmin -= xmargin;
  ymax += ymargin;
  ymin -= ymargin;

  orig_height = BLI_rctf_size_y(rect);

  rect->xmin = xmin;
  rect->xmax = xmax;

  rect->ymin = ymin;
  rect->ymax = ymax;

  /* Only zoom out vertically. */
  if (orig_height > BLI_rctf_size_y(rect)) {
    ymid = BLI_rctf_cent_y(rect);

    rect->ymin = ymid - (orig_height / 2);
    rect->ymax = ymid + (orig_height / 2);
  }
}

static int sequencer_view_selected_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  SeqCollection *strips = selected_strips_from_context(C);
  View2D *v2d = UI_view2d_fromcontext(C);
  rctf cur_new = v2d->cur;

  if (SEQ_collection_len(strips) == 0) {
    return OPERATOR_CANCELLED;
  }

  if (sequencer_view_has_preview_poll(C) && !sequencer_view_preview_only_poll(C)) {
    return OPERATOR_CANCELLED;
  }

  if (region && region->regiontype == RGN_TYPE_PREVIEW) {
    seq_view_collection_rect_preview(scene, strips, &cur_new);
  }
  else {
    seq_view_collection_rect_timeline(scene, strips, &cur_new);
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

  /* Api callbacks. */
  ot->exec = sequencer_view_selected_exec;
  ot->poll = sequencer_editing_initialized_and_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Border Offset View Operator
 * \{ */

static int view_ghost_border_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
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

  /* Api callbacks. */
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
