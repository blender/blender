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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spseq
 */

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_sequencer.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"

#include "UI_view2d.h"

#include "RNA_define.h"

/* For menu, popup, icons, etc. */
#include "ED_anim_api.h"
#include "ED_screen.h"
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

  boundbox_seq(CTX_data_scene(C), &box);
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
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);
#if 0
  ARegion *region = CTX_wm_region(C);
  SpaceSeq *sseq = area->spacedata.first;
  Scene *scene = CTX_data_scene(C);
#endif
  View2D *v2d = UI_view2d_fromcontext(C);

  v2d->cur = v2d->tot;
  UI_view2d_curRect_validate(v2d);
  UI_view2d_sync(screen, area, v2d, V2D_LOCK_COPY);

#if 0
  /* Like zooming on an image view. */
  float zoomX, zoomY;
  int width, height, imgwidth, imgheight;

  width = region->winx;
  height = region->winy;

  seq_reset_imageofs(sseq);

  imgwidth = (scene->r.size * scene->r.xsch) / 100;
  imgheight = (scene->r.size * scene->r.ysch) / 100;

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

  float winx = (int)(rd->size * rd->xsch) / 100;
  float winy = (int)(rd->size * rd->ysch) / 100;

  float facx = BLI_rcti_size_x(&v2d->mask) / winx;
  float facy = BLI_rcti_size_y(&v2d->mask) / winy;

  BLI_rctf_resize(&v2d->cur, ceilf(winx * facx / ratio + 0.5f), ceilf(winy * facy / ratio + 0.5f));

  ED_region_tag_redraw(CTX_wm_region(C));

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
/** \name View Toggle Operator
 * \{ */

#if 0
static const EnumPropertyItem view_type_items[] = {
    {SEQ_VIEW_SEQUENCE, "SEQUENCER", ICON_SEQ_SEQUENCER, "Sequencer", ""},
    {SEQ_VIEW_PREVIEW, "PREVIEW", ICON_SEQ_PREVIEW, "Image Preview", ""},
    {SEQ_VIEW_SEQUENCE_PREVIEW,
     "SEQUENCER_PREVIEW",
     ICON_SEQ_SEQUENCER,
     "Sequencer and Image Preview",
     ""},
    {0, NULL, 0, NULL, NULL},
};
#endif

static int sequencer_view_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceSeq *sseq = (SpaceSeq *)CTX_wm_space_data(C);

  sseq->view++;
  if (sseq->view > SEQ_VIEW_SEQUENCE_PREVIEW) {
    sseq->view = SEQ_VIEW_SEQUENCE;
  }

  ED_area_tag_refresh(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_toggle(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "View Toggle";
  ot->idname = "SEQUENCER_OT_view_toggle";
  ot->description = "Toggle between sequencer views (sequence, preview, both)";

  /* Api callbacks. */
  ot->exec = sequencer_view_toggle_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Selected Operator
 * \{ */

static int sequencer_view_selected_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  View2D *v2d = UI_view2d_fromcontext(C);
  ARegion *region = CTX_wm_region(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *last_seq = BKE_sequencer_active_get(scene);
  Sequence *seq;
  rctf cur_new = v2d->cur;

  int xmin = MAXFRAME * 2;
  int xmax = -MAXFRAME * 2;
  int ymin = MAXSEQ + 1;
  int ymax = 0;
  int orig_height;
  int ymid;
  int ymargin = 1;
  int xmargin = FPS;

  if (ed == NULL) {
    return OPERATOR_CANCELLED;
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq->flag & SELECT) || (seq == last_seq)) {
      xmin = min_ii(xmin, seq->startdisp);
      xmax = max_ii(xmax, seq->enddisp);

      ymin = min_ii(ymin, seq->machine);
      ymax = max_ii(ymax, seq->machine);
    }
  }

  if (ymax != 0) {
    const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

    xmax += xmargin;
    xmin -= xmargin;
    ymax += ymargin;
    ymin -= ymargin;

    orig_height = BLI_rctf_size_y(&cur_new);

    cur_new.xmin = xmin;
    cur_new.xmax = xmax;

    cur_new.ymin = ymin;
    cur_new.ymax = ymax;

    /* Only zoom out vertically. */
    if (orig_height > BLI_rctf_size_y(&cur_new)) {
      ymid = BLI_rctf_cent_y(&cur_new);

      cur_new.ymin = ymid - (orig_height / 2);
      cur_new.ymax = ymid + (orig_height / 2);
    }

    UI_view2d_smooth_view(C, region, &cur_new, smooth_viewtx);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void SEQUENCER_OT_view_selected(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Frame Selected";
  ot->idname = "SEQUENCER_OT_view_selected";
  ot->description = "Zoom the sequencer on the selected strips";

  /* Api callbacks. */
  ot->exec = sequencer_view_selected_exec;
  ot->poll = ED_operator_sequencer_active;

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

  scene->ed->over_border = rect;

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_ghost_border(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Border Offset View";
  ot->idname = "SEQUENCER_OT_view_ghost_border";
  ot->description = "Set the boundaries of the border used for offset-view";

  /* Api callbacks. */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = view_ghost_border_exec;
  ot->modal = WM_gesture_box_modal;
  ot->poll = sequencer_view_preview_poll;
  ot->cancel = WM_gesture_box_cancel;

  /* Flags. */
  ot->flag = 0;

  /* Properties. */
  WM_operator_properties_gesture_box(ot);
}

/** \} */
