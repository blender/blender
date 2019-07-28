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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spclip
 */

#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>

#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"
#include "DNA_scene_types.h" /* min/max frames */

#include "BLI_utildefines.h"
#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_task.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_movieclip.h"
#include "BKE_report.h"
#include "BKE_tracking.h"

#include "WM_api.h"
#include "WM_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "UI_interface.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_view2d.h"

#include "PIL_time.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "clip_intern.h"  // own include

/******************** view navigation utilities *********************/

static void sclip_zoom_set(const bContext *C,
                           float zoom,
                           const float location[2],
                           const bool zoom_to_pos)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *ar = CTX_wm_region(C);

  float oldzoom = sc->zoom;
  int width, height;

  sc->zoom = zoom;

  if (sc->zoom < 0.1f || sc->zoom > 4.0f) {
    /* check zoom limits */
    ED_space_clip_get_size(sc, &width, &height);

    width *= sc->zoom;
    height *= sc->zoom;

    if ((width < 4) && (height < 4) && sc->zoom < oldzoom) {
      sc->zoom = oldzoom;
    }
    else if (BLI_rcti_size_x(&ar->winrct) <= sc->zoom) {
      sc->zoom = oldzoom;
    }
    else if (BLI_rcti_size_y(&ar->winrct) <= sc->zoom) {
      sc->zoom = oldzoom;
    }
  }

  if (zoom_to_pos && location) {
    float aspx, aspy, w, h, dx, dy;

    ED_space_clip_get_size(sc, &width, &height);
    ED_space_clip_get_aspect(sc, &aspx, &aspy);

    w = width * aspx;
    h = height * aspy;

    dx = ((location[0] - 0.5f) * w - sc->xof) * (sc->zoom - oldzoom) / sc->zoom;
    dy = ((location[1] - 0.5f) * h - sc->yof) * (sc->zoom - oldzoom) / sc->zoom;

    if (sc->flag & SC_LOCK_SELECTION) {
      sc->xlockof += dx;
      sc->ylockof += dy;
    }
    else {
      sc->xof += dx;
      sc->yof += dy;
    }
  }
}

static void sclip_zoom_set_factor(const bContext *C,
                                  float zoomfac,
                                  const float location[2],
                                  const bool zoom_to_pos)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  sclip_zoom_set(C, sc->zoom * zoomfac, location, zoom_to_pos);
}

static void sclip_zoom_set_factor_exec(bContext *C, const wmEvent *event, float factor)
{
  ARegion *ar = CTX_wm_region(C);

  float location[2], *mpos = NULL;

  if (event) {
    SpaceClip *sc = CTX_wm_space_clip(C);

    ED_clip_mouse_pos(sc, ar, event->mval, location);
    mpos = location;
  }

  sclip_zoom_set_factor(C, factor, mpos, mpos ? (U.uiflag & USER_ZOOM_TO_MOUSEPOS) : false);

  ED_region_tag_redraw(ar);
}

/******************** open clip operator ********************/

static void clip_filesel(bContext *C, wmOperator *op, const char *path)
{
  RNA_string_set(op->ptr, "directory", path);

  WM_event_add_fileselect(C, op);
}

static void open_init(bContext *C, wmOperator *op)
{
  PropertyPointerRNA *pprop;

  op->customdata = pprop = MEM_callocN(sizeof(PropertyPointerRNA), "OpenPropertyPointerRNA");
  UI_context_active_but_prop_get_templateID(C, &pprop->ptr, &pprop->prop);
}

static void open_cancel(bContext *UNUSED(C), wmOperator *op)
{
  MEM_freeN(op->customdata);
  op->customdata = NULL;
}

static int open_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  bScreen *screen = CTX_wm_screen(C);
  Main *bmain = CTX_data_main(C);
  PropertyPointerRNA *pprop;
  PointerRNA idptr;
  MovieClip *clip = NULL;
  char str[FILE_MAX];

  if (RNA_collection_length(op->ptr, "files")) {
    PointerRNA fileptr;
    PropertyRNA *prop;
    char dir_only[FILE_MAX], file_only[FILE_MAX];
    bool relative = RNA_boolean_get(op->ptr, "relative_path");

    RNA_string_get(op->ptr, "directory", dir_only);
    if (relative) {
      BLI_path_rel(dir_only, CTX_data_main(C)->name);
    }

    prop = RNA_struct_find_property(op->ptr, "files");
    RNA_property_collection_lookup_int(op->ptr, prop, 0, &fileptr);
    RNA_string_get(&fileptr, "name", file_only);

    BLI_join_dirfile(str, sizeof(str), dir_only, file_only);
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "No files selected to be opened");

    return OPERATOR_CANCELLED;
  }

  /* default to frame 1 if there's no scene in context */

  errno = 0;

  clip = BKE_movieclip_file_add_exists(bmain, str);

  if (!clip) {
    if (op->customdata) {
      MEM_freeN(op->customdata);
    }

    BKE_reportf(op->reports,
                RPT_ERROR,
                "Cannot read '%s': %s",
                str,
                errno ? strerror(errno) : TIP_("unsupported movie clip format"));

    return OPERATOR_CANCELLED;
  }

  if (!op->customdata) {
    open_init(C, op);
  }

  /* hook into UI */
  pprop = op->customdata;

  if (pprop->prop) {
    /* when creating new ID blocks, use is already 1, but RNA
     * pointer use also increases user, so this compensates it */
    id_us_min(&clip->id);

    RNA_id_pointer_create(&clip->id, &idptr);
    RNA_property_pointer_set(&pprop->ptr, pprop->prop, idptr, NULL);
    RNA_property_update(C, &pprop->ptr, pprop->prop);
  }
  else if (sc) {
    ED_space_clip_set_clip(C, screen, sc, clip);
  }

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_ADDED, clip);

  DEG_relations_tag_update(bmain);
  MEM_freeN(op->customdata);

  return OPERATOR_FINISHED;
}

static int open_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  char path[FILE_MAX];
  MovieClip *clip = NULL;

  if (sc) {
    clip = ED_space_clip_get_clip(sc);
  }

  if (clip) {
    BLI_strncpy(path, clip->name, sizeof(path));

    BLI_path_abs(path, CTX_data_main(C)->name);
    BLI_parent_dir(path);
  }
  else {
    BLI_strncpy(path, U.textudir, sizeof(path));
  }

  if (RNA_struct_property_is_set(op->ptr, "files")) {
    return open_exec(C, op);
  }

  if (!RNA_struct_property_is_set(op->ptr, "relative_path")) {
    RNA_boolean_set(op->ptr, "relative_path", (U.flag & USER_RELPATHS) != 0);
  }

  open_init(C, op);

  clip_filesel(C, op, path);

  return OPERATOR_RUNNING_MODAL;
}

void CLIP_OT_open(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Open Clip";
  ot->description = "Load a sequence of frames or a movie file";
  ot->idname = "CLIP_OT_open";

  /* api callbacks */
  ot->exec = open_exec;
  ot->invoke = open_invoke;
  ot->cancel = open_cancel;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_RELPATH | WM_FILESEL_FILES | WM_FILESEL_DIRECTORY,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);
}

/******************* reload clip operator *********************/

static int reload_exec(bContext *C, wmOperator *UNUSED(op))
{
  MovieClip *clip = CTX_data_edit_movieclip(C);

  if (!clip) {
    return OPERATOR_CANCELLED;
  }

  WM_jobs_kill_type(CTX_wm_manager(C), NULL, WM_JOB_TYPE_CLIP_PREFETCH);
  BKE_movieclip_reload(CTX_data_main(C), clip);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

  return OPERATOR_FINISHED;
}

void CLIP_OT_reload(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reload Clip";
  ot->description = "Reload clip";
  ot->idname = "CLIP_OT_reload";

  /* api callbacks */
  ot->exec = reload_exec;
}

/********************** view pan operator *********************/

typedef struct ViewPanData {
  float x, y;
  float xof, yof, xorig, yorig;
  int event_type;
  bool own_cursor;
  float *vec;
} ViewPanData;

static void view_pan_init(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  ViewPanData *vpd;

  op->customdata = vpd = MEM_callocN(sizeof(ViewPanData), "ClipViewPanData");

  /* Grab will be set when running from gizmo. */
  vpd->own_cursor = (win->grabcursor == 0);
  if (vpd->own_cursor) {
    WM_cursor_modal_set(win, BC_NSEW_SCROLLCURSOR);
  }

  vpd->x = event->x;
  vpd->y = event->y;

  if (sc->flag & SC_LOCK_SELECTION) {
    vpd->vec = &sc->xlockof;
  }
  else {
    vpd->vec = &sc->xof;
  }

  copy_v2_v2(&vpd->xof, vpd->vec);
  copy_v2_v2(&vpd->xorig, &vpd->xof);

  vpd->event_type = event->type;

  WM_event_add_modal_handler(C, op);
}

static void view_pan_exit(bContext *C, wmOperator *op, bool cancel)
{
  ViewPanData *vpd = op->customdata;

  if (cancel) {
    copy_v2_v2(vpd->vec, &vpd->xorig);

    ED_region_tag_redraw(CTX_wm_region(C));
  }

  if (vpd->own_cursor) {
    WM_cursor_modal_restore(CTX_wm_window(C));
  }
  MEM_freeN(op->customdata);
}

static int view_pan_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  float offset[2];

  RNA_float_get_array(op->ptr, "offset", offset);

  if (sc->flag & SC_LOCK_SELECTION) {
    sc->xlockof += offset[0];
    sc->ylockof += offset[1];
  }
  else {
    sc->xof += offset[0];
    sc->yof += offset[1];
  }

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

static int view_pan_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type == MOUSEPAN) {
    SpaceClip *sc = CTX_wm_space_clip(C);
    float offset[2];

    offset[0] = (event->prevx - event->x) / sc->zoom;
    offset[1] = (event->prevy - event->y) / sc->zoom;

    RNA_float_set_array(op->ptr, "offset", offset);

    view_pan_exec(C, op);

    return OPERATOR_FINISHED;
  }
  else {
    view_pan_init(C, op, event);

    return OPERATOR_RUNNING_MODAL;
  }
}

static int view_pan_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ViewPanData *vpd = op->customdata;
  float offset[2];

  switch (event->type) {
    case MOUSEMOVE:
      copy_v2_v2(vpd->vec, &vpd->xorig);
      offset[0] = (vpd->x - event->x) / sc->zoom;
      offset[1] = (vpd->y - event->y) / sc->zoom;
      RNA_float_set_array(op->ptr, "offset", offset);
      view_pan_exec(C, op);
      break;
    case ESCKEY:
      view_pan_exit(C, op, 1);

      return OPERATOR_CANCELLED;
    case SPACEKEY:
      view_pan_exit(C, op, 0);

      return OPERATOR_FINISHED;
    default:
      if (event->type == vpd->event_type && event->val == KM_RELEASE) {
        view_pan_exit(C, op, 0);

        return OPERATOR_FINISHED;
      }
      break;
  }

  return OPERATOR_RUNNING_MODAL;
}

static void view_pan_cancel(bContext *C, wmOperator *op)
{
  view_pan_exit(C, op, true);
}

void CLIP_OT_view_pan(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pan View";
  ot->idname = "CLIP_OT_view_pan";
  ot->description = "Pan the view";

  /* api callbacks */
  ot->exec = view_pan_exec;
  ot->invoke = view_pan_invoke;
  ot->modal = view_pan_modal;
  ot->cancel = view_pan_cancel;
  ot->poll = ED_space_clip_view_clip_poll;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "offset",
                       2,
                       NULL,
                       -FLT_MAX,
                       FLT_MAX,
                       "Offset",
                       "Offset in floating point units, 1.0 is the width and height of the image",
                       -FLT_MAX,
                       FLT_MAX);
}

/********************** view zoom operator *********************/

typedef struct ViewZoomData {
  float x, y;
  float zoom;
  int event_type;
  float location[2];
  wmTimer *timer;
  double timer_lastdraw;
  bool own_cursor;
} ViewZoomData;

static void view_zoom_init(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *ar = CTX_wm_region(C);
  ViewZoomData *vpd;

  op->customdata = vpd = MEM_callocN(sizeof(ViewZoomData), "ClipViewZoomData");

  /* Grab will be set when running from gizmo. */
  vpd->own_cursor = (win->grabcursor == 0);
  if (vpd->own_cursor) {
    WM_cursor_modal_set(win, BC_NSEW_SCROLLCURSOR);
  }

  if (U.viewzoom == USER_ZOOM_CONT) {
    /* needs a timer to continue redrawing */
    vpd->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);
    vpd->timer_lastdraw = PIL_check_seconds_timer();
  }

  vpd->x = event->x;
  vpd->y = event->y;
  vpd->zoom = sc->zoom;
  vpd->event_type = event->type;

  ED_clip_mouse_pos(sc, ar, event->mval, vpd->location);

  WM_event_add_modal_handler(C, op);
}

static void view_zoom_exit(bContext *C, wmOperator *op, bool cancel)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ViewZoomData *vpd = op->customdata;

  if (cancel) {
    sc->zoom = vpd->zoom;
    ED_region_tag_redraw(CTX_wm_region(C));
  }

  if (vpd->timer) {
    WM_event_remove_timer(CTX_wm_manager(C), vpd->timer->win, vpd->timer);
  }

  if (vpd->own_cursor) {
    WM_cursor_modal_restore(CTX_wm_window(C));
  }
  MEM_freeN(op->customdata);
}

static int view_zoom_exec(bContext *C, wmOperator *op)
{
  sclip_zoom_set_factor(C, RNA_float_get(op->ptr, "factor"), NULL, false);

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

static int view_zoom_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type == MOUSEZOOM || event->type == MOUSEPAN) {
    float delta, factor;

    delta = event->prevx - event->x + event->prevy - event->y;

    if (U.uiflag & USER_ZOOM_INVERT) {
      delta *= -1;
    }

    factor = 1.0f + delta / 300.0f;
    RNA_float_set(op->ptr, "factor", factor);

    sclip_zoom_set_factor_exec(C, event, factor);

    return OPERATOR_FINISHED;
  }
  else {
    view_zoom_init(C, op, event);

    return OPERATOR_RUNNING_MODAL;
  }
}

static void view_zoom_apply(
    bContext *C, ViewZoomData *vpd, wmOperator *op, const wmEvent *event, const bool zoom_to_pos)
{
  float factor;

  if (U.viewzoom == USER_ZOOM_CONT) {
    SpaceClip *sclip = CTX_wm_space_clip(C);
    double time = PIL_check_seconds_timer();
    float time_step = (float)(time - vpd->timer_lastdraw);
    float fac;
    float zfac;

    if (U.uiflag & USER_ZOOM_HORIZ) {
      fac = (float)(event->x - vpd->x);
    }
    else {
      fac = (float)(event->y - vpd->y);
    }

    if (U.uiflag & USER_ZOOM_INVERT) {
      fac = -fac;
    }

    zfac = 1.0f + ((fac / 20.0f) * time_step);
    vpd->timer_lastdraw = time;
    factor = (sclip->zoom * zfac) / vpd->zoom;
  }
  else {
    float delta = event->x - vpd->x + event->y - vpd->y;

    if (U.uiflag & USER_ZOOM_INVERT) {
      delta *= -1;
    }

    factor = 1.0f + delta / 300.0f;
  }

  RNA_float_set(op->ptr, "factor", factor);
  sclip_zoom_set(C, vpd->zoom * factor, vpd->location, zoom_to_pos);
  ED_region_tag_redraw(CTX_wm_region(C));
}

static int view_zoom_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewZoomData *vpd = op->customdata;
  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");
  switch (event->type) {
    case TIMER:
      if (event->customdata == vpd->timer) {
        view_zoom_apply(C, vpd, op, event, use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS));
      }
      break;
    case MOUSEMOVE:
      view_zoom_apply(C, vpd, op, event, use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS));
      break;
    default:
      if (event->type == vpd->event_type && event->val == KM_RELEASE) {
        view_zoom_exit(C, op, 0);

        return OPERATOR_FINISHED;
      }
      break;
  }

  return OPERATOR_RUNNING_MODAL;
}

static void view_zoom_cancel(bContext *C, wmOperator *op)
{
  view_zoom_exit(C, op, true);
}

void CLIP_OT_view_zoom(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "View Zoom";
  ot->idname = "CLIP_OT_view_zoom";
  ot->description = "Zoom in/out the view";

  /* api callbacks */
  ot->exec = view_zoom_exec;
  ot->invoke = view_zoom_invoke;
  ot->modal = view_zoom_modal;
  ot->cancel = view_zoom_cancel;
  ot->poll = ED_space_clip_view_clip_poll;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  /* properties */
  prop = RNA_def_float(ot->srna,
                       "factor",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Factor",
                       "Zoom factor, values higher than 1.0 zoom in, lower values zoom out",
                       -FLT_MAX,
                       FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);

  WM_operator_properties_use_cursor_init(ot);
}

/********************** view zoom in/out operator *********************/

static int view_zoom_in_exec(bContext *C, wmOperator *op)
{
  float location[2];

  RNA_float_get_array(op->ptr, "location", location);

  sclip_zoom_set_factor(C, powf(2.0f, 1.0f / 3.0f), location, U.uiflag & USER_ZOOM_TO_MOUSEPOS);

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

static int view_zoom_in_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *ar = CTX_wm_region(C);

  float location[2];

  ED_clip_mouse_pos(sc, ar, event->mval, location);
  RNA_float_set_array(op->ptr, "location", location);

  return view_zoom_in_exec(C, op);
}

void CLIP_OT_view_zoom_in(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "View Zoom In";
  ot->idname = "CLIP_OT_view_zoom_in";
  ot->description = "Zoom in the view";

  /* api callbacks */
  ot->exec = view_zoom_in_exec;
  ot->invoke = view_zoom_in_invoke;
  ot->poll = ED_space_clip_view_clip_poll;

  /* properties */
  prop = RNA_def_float_vector(ot->srna,
                              "location",
                              2,
                              NULL,
                              -FLT_MAX,
                              FLT_MAX,
                              "Location",
                              "Cursor location in screen coordinates",
                              -10.0f,
                              10.0f);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

static int view_zoom_out_exec(bContext *C, wmOperator *op)
{
  float location[2];

  RNA_float_get_array(op->ptr, "location", location);

  sclip_zoom_set_factor(C, powf(0.5f, 1.0f / 3.0f), location, U.uiflag & USER_ZOOM_TO_MOUSEPOS);

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

static int view_zoom_out_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *ar = CTX_wm_region(C);

  float location[2];

  ED_clip_mouse_pos(sc, ar, event->mval, location);
  RNA_float_set_array(op->ptr, "location", location);

  return view_zoom_out_exec(C, op);
}

void CLIP_OT_view_zoom_out(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "View Zoom Out";
  ot->idname = "CLIP_OT_view_zoom_out";
  ot->description = "Zoom out the view";

  /* api callbacks */
  ot->exec = view_zoom_out_exec;
  ot->invoke = view_zoom_out_invoke;
  ot->poll = ED_space_clip_view_clip_poll;

  /* properties */
  prop = RNA_def_float_vector(ot->srna,
                              "location",
                              2,
                              NULL,
                              -FLT_MAX,
                              FLT_MAX,
                              "Location",
                              "Cursor location in normalized (0.0-1.0) coordinates",
                              -10.0f,
                              10.0f);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/********************** view zoom ratio operator *********************/

static int view_zoom_ratio_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  sclip_zoom_set(C, RNA_float_get(op->ptr, "ratio"), NULL, false);

  /* ensure pixel exact locations for draw */
  sc->xof = (int)sc->xof;
  sc->yof = (int)sc->yof;

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

void CLIP_OT_view_zoom_ratio(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Zoom Ratio";
  ot->idname = "CLIP_OT_view_zoom_ratio";
  ot->description = "Set the zoom ratio (based on clip size)";

  /* api callbacks */
  ot->exec = view_zoom_ratio_exec;
  ot->poll = ED_space_clip_view_clip_poll;

  /* properties */
  RNA_def_float(ot->srna,
                "ratio",
                0.0f,
                -FLT_MAX,
                FLT_MAX,
                "Ratio",
                "Zoom ratio, 1.0 is 1:1, higher is zoomed in, lower is zoomed out",
                -FLT_MAX,
                FLT_MAX);
}

/********************** view all operator *********************/

static int view_all_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc;
  ARegion *ar;
  int w, h, width, height;
  float aspx, aspy;
  bool fit_view = RNA_boolean_get(op->ptr, "fit_view");
  float zoomx, zoomy;

  /* retrieve state */
  sc = CTX_wm_space_clip(C);
  ar = CTX_wm_region(C);

  ED_space_clip_get_size(sc, &w, &h);
  ED_space_clip_get_aspect(sc, &aspx, &aspy);

  w = w * aspx;
  h = h * aspy;

  /* check if the image will fit in the image with zoom == 1 */
  width = BLI_rcti_size_x(&ar->winrct) + 1;
  height = BLI_rcti_size_y(&ar->winrct) + 1;

  if (fit_view) {
    const int margin = 5; /* margin from border */

    zoomx = (float)width / (w + 2 * margin);
    zoomy = (float)height / (h + 2 * margin);

    sclip_zoom_set(C, min_ff(zoomx, zoomy), NULL, false);
  }
  else {
    if ((w >= width || h >= height) && (width > 0 && height > 0)) {
      zoomx = (float)width / w;
      zoomy = (float)height / h;

      /* find the zoom value that will fit the image in the image space */
      sclip_zoom_set(C, 1.0f / power_of_2(1.0f / min_ff(zoomx, zoomy)), NULL, false);
    }
    else {
      sclip_zoom_set(C, 1.0f, NULL, false);
    }
  }

  sc->xof = sc->yof = 0.0f;

  ED_region_tag_redraw(ar);

  return OPERATOR_FINISHED;
}

void CLIP_OT_view_all(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "View All";
  ot->idname = "CLIP_OT_view_all";
  ot->description = "View whole image with markers";

  /* api callbacks */
  ot->exec = view_all_exec;
  ot->poll = ED_space_clip_view_clip_poll;

  /* properties */
  prop = RNA_def_boolean(ot->srna, "fit_view", 0, "Fit View", "Fit frame to the viewport");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/********************** view selected operator *********************/

static int view_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *ar = CTX_wm_region(C);

  sc->xlockof = 0.0f;
  sc->ylockof = 0.0f;

  ED_clip_view_selection(C, ar, 1);
  ED_region_tag_redraw(ar);

  return OPERATOR_FINISHED;
}

void CLIP_OT_view_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Selected";
  ot->idname = "CLIP_OT_view_selected";
  ot->description = "View all selected elements";

  /* api callbacks */
  ot->exec = view_selected_exec;
  ot->poll = ED_space_clip_view_clip_poll;
}

/********************** change frame operator *********************/

static bool change_frame_poll(bContext *C)
{
  /* prevent changes during render */
  if (G.is_rendering) {
    return false;
  }
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  return space_clip != NULL;
}

static void change_frame_apply(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  /* set the new frame number */
  CFRA = RNA_int_get(op->ptr, "frame");
  FRAMENUMBER_MIN_CLAMP(CFRA);
  SUBFRA = 0.0f;

  /* do updates */
  DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_SEEK);
  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
}

static int change_frame_exec(bContext *C, wmOperator *op)
{
  change_frame_apply(C, op);

  return OPERATOR_FINISHED;
}

static int frame_from_event(bContext *C, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  int framenr = 0;

  if (ar->regiontype == RGN_TYPE_WINDOW) {
    float sfra = SFRA, efra = EFRA, framelen = ar->winx / (efra - sfra + 1);

    framenr = sfra + event->mval[0] / framelen;
  }
  else {
    float viewx, viewy;

    UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &viewx, &viewy);

    framenr = round_fl_to_int(viewx);
  }

  return framenr;
}

static int change_frame_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);

  if (ar->regiontype == RGN_TYPE_WINDOW) {
    if (event->mval[1] > 16) {
      return OPERATOR_PASS_THROUGH;
    }
  }

  RNA_int_set(op->ptr, "frame", frame_from_event(C, event));

  change_frame_apply(C, op);

  /* add temp handler */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int change_frame_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  switch (event->type) {
    case ESCKEY:
      return OPERATOR_FINISHED;

    case MOUSEMOVE:
      RNA_int_set(op->ptr, "frame", frame_from_event(C, event));
      change_frame_apply(C, op);
      break;

    case LEFTMOUSE:
    case RIGHTMOUSE:
      if (event->val == KM_RELEASE) {
        return OPERATOR_FINISHED;
      }
      break;
  }

  return OPERATOR_RUNNING_MODAL;
}

void CLIP_OT_change_frame(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Change Frame";
  ot->idname = "CLIP_OT_change_frame";
  ot->description = "Interactively change the current frame number";

  /* api callbacks */
  ot->exec = change_frame_exec;
  ot->invoke = change_frame_invoke;
  ot->modal = change_frame_modal;
  ot->poll = change_frame_poll;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_UNDO;

  /* rna */
  RNA_def_int(ot->srna, "frame", 0, MINAFRAME, MAXFRAME, "Frame", "", MINAFRAME, MAXFRAME);
}

/********************** rebuild proxies operator *********************/

typedef struct ProxyBuildJob {
  Scene *scene;
  struct Main *main;
  MovieClip *clip;
  int clip_flag;
  bool stop;
  struct IndexBuildContext *index_context;
} ProxyJob;

static void proxy_freejob(void *pjv)
{
  ProxyJob *pj = pjv;

  MEM_freeN(pj);
}

static int proxy_bitflag_to_array(int size_flag, int build_sizes[4], int undistort)
{
  int build_count = 0;
  int size_flags[2][4] = {
      {MCLIP_PROXY_SIZE_25, MCLIP_PROXY_SIZE_50, MCLIP_PROXY_SIZE_75, MCLIP_PROXY_SIZE_100},
      {MCLIP_PROXY_UNDISTORTED_SIZE_25,
       MCLIP_PROXY_UNDISTORTED_SIZE_50,
       MCLIP_PROXY_UNDISTORTED_SIZE_75,
       MCLIP_PROXY_UNDISTORTED_SIZE_100}};
  int size_nr = undistort ? 1 : 0;

  if (size_flag & size_flags[size_nr][0]) {
    build_sizes[build_count++] = MCLIP_PROXY_RENDER_SIZE_25;
  }

  if (size_flag & size_flags[size_nr][1]) {
    build_sizes[build_count++] = MCLIP_PROXY_RENDER_SIZE_50;
  }

  if (size_flag & size_flags[size_nr][2]) {
    build_sizes[build_count++] = MCLIP_PROXY_RENDER_SIZE_75;
  }

  if (size_flag & size_flags[size_nr][3]) {
    build_sizes[build_count++] = MCLIP_PROXY_RENDER_SIZE_100;
  }

  return build_count;
}

/* simple case for movies -- handle frame-by-frame, do threading within single frame */
static void do_movie_proxy(void *pjv,
                           int *UNUSED(build_sizes),
                           int UNUSED(build_count),
                           int *build_undistort_sizes,
                           int build_undistort_count,
                           short *stop,
                           short *do_update,
                           float *progress)
{
  ProxyJob *pj = pjv;
  Scene *scene = pj->scene;
  MovieClip *clip = pj->clip;
  struct MovieDistortion *distortion = NULL;
  int cfra, sfra = SFRA, efra = EFRA;

  if (pj->index_context) {
    IMB_anim_index_rebuild(pj->index_context, stop, do_update, progress);
  }

  if (!build_undistort_count) {
    if (*stop) {
      pj->stop = 1;
    }

    return;
  }
  else {
    sfra = 1;
    efra = clip->len;
  }

  if (build_undistort_count) {
    int threads = BLI_system_thread_count();
    int width, height;

    BKE_movieclip_get_size(clip, NULL, &width, &height);

    distortion = BKE_tracking_distortion_new(&clip->tracking, width, height);
    BKE_tracking_distortion_set_threads(distortion, threads);
  }

  for (cfra = sfra; cfra <= efra; cfra++) {
    BKE_movieclip_build_proxy_frame(
        clip, pj->clip_flag, distortion, cfra, build_undistort_sizes, build_undistort_count, 1);

    if (*stop || G.is_break) {
      break;
    }

    *do_update = true;
    *progress = ((float)cfra - sfra) / (efra - sfra);
  }

  if (distortion) {
    BKE_tracking_distortion_free(distortion);
  }

  if (*stop) {
    pj->stop = 1;
  }
}

/* *****
 * special case for sequences -- handle different frames in different threads,
 * loading from disk happens in critical section, decoding frame happens from
 * thread for maximal speed
 */

typedef struct ProxyQueue {
  int cfra;
  int sfra;
  int efra;
  SpinLock spin;

  const short *stop;
  short *do_update;
  float *progress;
} ProxyQueue;

typedef struct ProxyThread {
  MovieClip *clip;
  struct MovieDistortion *distortion;
  int *build_sizes, build_count;
  int *build_undistort_sizes, build_undistort_count;
} ProxyThread;

static unsigned char *proxy_thread_next_frame(ProxyQueue *queue,
                                              MovieClip *clip,
                                              size_t *size_r,
                                              int *cfra_r)
{
  unsigned char *mem = NULL;

  BLI_spin_lock(&queue->spin);
  if (!*queue->stop && queue->cfra <= queue->efra) {
    MovieClipUser user = {0};
    char name[FILE_MAX];
    size_t size;
    int file;

    user.framenr = queue->cfra;

    BKE_movieclip_filename_for_frame(clip, &user, name);

    file = BLI_open(name, O_BINARY | O_RDONLY, 0);
    if (file < 0) {
      BLI_spin_unlock(&queue->spin);
      return NULL;
    }

    size = BLI_file_descriptor_size(file);
    if (size < 1) {
      close(file);
      BLI_spin_unlock(&queue->spin);
      return NULL;
    }

    mem = MEM_mallocN(size, "movieclip proxy memory file");

    if (read(file, mem, size) != size) {
      close(file);
      BLI_spin_unlock(&queue->spin);
      MEM_freeN(mem);
      return NULL;
    }

    *size_r = size;
    *cfra_r = queue->cfra;

    queue->cfra++;
    close(file);

    *queue->do_update = 1;
    *queue->progress = (float)(queue->cfra - queue->sfra) / (queue->efra - queue->sfra);
  }
  BLI_spin_unlock(&queue->spin);

  return mem;
}

static void proxy_task_func(TaskPool *__restrict pool, void *task_data, int UNUSED(threadid))
{
  ProxyThread *data = (ProxyThread *)task_data;
  ProxyQueue *queue = (ProxyQueue *)BLI_task_pool_userdata(pool);
  unsigned char *mem;
  size_t size;
  int cfra;

  while ((mem = proxy_thread_next_frame(queue, data->clip, &size, &cfra))) {
    ImBuf *ibuf;

    ibuf = IMB_ibImageFromMemory(mem,
                                 size,
                                 IB_rect | IB_multilayer | IB_alphamode_detect,
                                 data->clip->colorspace_settings.name,
                                 "proxy frame");

    BKE_movieclip_build_proxy_frame_for_ibuf(
        data->clip, ibuf, NULL, cfra, data->build_sizes, data->build_count, false);

    BKE_movieclip_build_proxy_frame_for_ibuf(data->clip,
                                             ibuf,
                                             data->distortion,
                                             cfra,
                                             data->build_undistort_sizes,
                                             data->build_undistort_count,
                                             true);

    IMB_freeImBuf(ibuf);

    MEM_freeN(mem);
  }
}

static void do_sequence_proxy(void *pjv,
                              int *build_sizes,
                              int build_count,
                              int *build_undistort_sizes,
                              int build_undistort_count,
                              short *stop,
                              short *do_update,
                              float *progress)
{
  ProxyJob *pj = pjv;
  MovieClip *clip = pj->clip;
  Scene *scene = pj->scene;
  TaskScheduler *task_scheduler = BLI_task_scheduler_get();
  TaskPool *task_pool;
  int sfra = SFRA, efra = EFRA;
  ProxyThread *handles;
  int i, tot_thread = BLI_task_scheduler_num_threads(task_scheduler);
  int width, height;
  ProxyQueue queue;

  if (build_undistort_count) {
    BKE_movieclip_get_size(clip, NULL, &width, &height);
  }

  BLI_spin_init(&queue.spin);

  queue.cfra = sfra;
  queue.sfra = sfra;
  queue.efra = efra;
  queue.stop = stop;
  queue.do_update = do_update;
  queue.progress = progress;

  task_pool = BLI_task_pool_create(task_scheduler, &queue);
  handles = MEM_callocN(sizeof(ProxyThread) * tot_thread, "proxy threaded handles");
  for (i = 0; i < tot_thread; i++) {
    ProxyThread *handle = &handles[i];

    handle->clip = clip;

    handle->build_count = build_count;
    handle->build_sizes = build_sizes;

    handle->build_undistort_count = build_undistort_count;
    handle->build_undistort_sizes = build_undistort_sizes;

    if (build_undistort_count) {
      handle->distortion = BKE_tracking_distortion_new(&clip->tracking, width, height);
    }

    BLI_task_pool_push(task_pool, proxy_task_func, handle, false, TASK_PRIORITY_LOW);
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  if (build_undistort_count) {
    for (i = 0; i < tot_thread; i++) {
      ProxyThread *handle = &handles[i];
      BKE_tracking_distortion_free(handle->distortion);
    }
  }

  BLI_spin_end(&queue.spin);
  MEM_freeN(handles);
}

static void proxy_startjob(void *pjv, short *stop, short *do_update, float *progress)
{
  ProxyJob *pj = pjv;
  MovieClip *clip = pj->clip;

  short size_flag;
  int build_sizes[4], build_count = 0;
  int build_undistort_sizes[4], build_undistort_count = 0;

  size_flag = clip->proxy.build_size_flag;

  build_count = proxy_bitflag_to_array(size_flag, build_sizes, 0);
  build_undistort_count = proxy_bitflag_to_array(size_flag, build_undistort_sizes, 1);

  if (clip->source == MCLIP_SRC_MOVIE) {
    do_movie_proxy(pjv,
                   build_sizes,
                   build_count,
                   build_undistort_sizes,
                   build_undistort_count,
                   stop,
                   do_update,
                   progress);
  }
  else {
    do_sequence_proxy(pjv,
                      build_sizes,
                      build_count,
                      build_undistort_sizes,
                      build_undistort_count,
                      stop,
                      do_update,
                      progress);
  }
}

static void proxy_endjob(void *pjv)
{
  ProxyJob *pj = pjv;

  if (pj->clip->anim) {
    IMB_close_anim_proxies(pj->clip->anim);
  }

  if (pj->index_context) {
    IMB_anim_index_rebuild_finish(pj->index_context, pj->stop);
  }

  if (pj->clip->source == MCLIP_SRC_MOVIE) {
    /* Timecode might have changed, so do a full reload to deal with this. */
    DEG_id_tag_update(&pj->clip->id, ID_RECALC_SOURCE);
  }
  else {
    /* For image sequences we'll preserve original cache. */
    BKE_movieclip_clear_proxy_cache(pj->clip);
  }

  WM_main_add_notifier(NC_MOVIECLIP | ND_DISPLAY, pj->clip);
}

static int clip_rebuild_proxy_exec(bContext *C, wmOperator *UNUSED(op))
{
  wmJob *wm_job;
  ProxyJob *pj;
  Scene *scene = CTX_data_scene(C);
  ScrArea *sa = CTX_wm_area(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);

  if ((clip->flag & MCLIP_USE_PROXY) == 0) {
    return OPERATOR_CANCELLED;
  }

  wm_job = WM_jobs_get(CTX_wm_manager(C),
                       CTX_wm_window(C),
                       sa,
                       "Building Proxies",
                       WM_JOB_PROGRESS,
                       WM_JOB_TYPE_CLIP_BUILD_PROXY);

  pj = MEM_callocN(sizeof(ProxyJob), "proxy rebuild job");
  pj->scene = scene;
  pj->main = CTX_data_main(C);
  pj->clip = clip;
  pj->clip_flag = clip->flag & MCLIP_TIMECODE_FLAGS;

  if (clip->anim) {
    pj->index_context = IMB_anim_index_rebuild_context(clip->anim,
                                                       clip->proxy.build_tc_flag,
                                                       clip->proxy.build_size_flag,
                                                       clip->proxy.quality,
                                                       true,
                                                       NULL);
  }

  WM_jobs_customdata_set(wm_job, pj, proxy_freejob);
  WM_jobs_timer(wm_job, 0.2, NC_MOVIECLIP | ND_DISPLAY, 0);
  WM_jobs_callbacks(wm_job, proxy_startjob, NULL, NULL, proxy_endjob);

  G.is_break = false;
  WM_jobs_start(CTX_wm_manager(C), wm_job);

  ED_area_tag_redraw(sa);

  return OPERATOR_FINISHED;
}

void CLIP_OT_rebuild_proxy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rebuild Proxy and Timecode Indices";
  ot->idname = "CLIP_OT_rebuild_proxy";
  ot->description = "Rebuild all selected proxies and timecode indices in the background";

  /* api callbacks */
  ot->exec = clip_rebuild_proxy_exec;
  ot->poll = ED_space_clip_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/********************** mode set operator *********************/

static int mode_set_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  int mode = RNA_enum_get(op->ptr, "mode");

  sc->mode = mode;

  if (sc->mode == SC_MODE_MASKEDIT && sc->view != SC_VIEW_CLIP) {
    /* Make sure we are in the right view for mask editing */
    sc->view = SC_VIEW_CLIP;
  }

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_CLIP, NULL);

  return OPERATOR_FINISHED;
}

void CLIP_OT_mode_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Clip Mode";
  ot->description = "Set the clip interaction mode";
  ot->idname = "CLIP_OT_mode_set";

  /* api callbacks */
  ot->exec = mode_set_exec;

  ot->poll = ED_space_clip_poll;

  /* properties */
  RNA_def_enum(ot->srna, "mode", rna_enum_clip_editor_mode_items, SC_MODE_TRACKING, "Mode", "");
}

#ifdef WITH_INPUT_NDOF
/********************** NDOF operator *********************/

/* Combined pan/zoom from a 3D mouse device.
 * Z zooms, XY pans
 * "view" (not "paper") control -- user moves the viewpoint, not the image being viewed
 * that explains the negative signs in the code below
 */

static int clip_view_ndof_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }
  else {
    SpaceClip *sc = CTX_wm_space_clip(C);
    ARegion *ar = CTX_wm_region(C);
    float pan_vec[3];

    const wmNDOFMotionData *ndof = event->customdata;
    const float speed = NDOF_PIXELS_PER_SECOND;

    WM_event_ndof_pan_get(ndof, pan_vec, true);

    mul_v2_fl(pan_vec, (speed * ndof->dt) / sc->zoom);
    pan_vec[2] *= -ndof->dt;

    sclip_zoom_set_factor(C, 1.0f + pan_vec[2], NULL, false);
    sc->xof += pan_vec[0];
    sc->yof += pan_vec[1];

    ED_region_tag_redraw(ar);

    return OPERATOR_FINISHED;
  }
}

void CLIP_OT_view_ndof(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "NDOF Pan/Zoom";
  ot->idname = "CLIP_OT_view_ndof";
  ot->description = "Use a 3D mouse device to pan/zoom the view";

  /* api callbacks */
  ot->invoke = clip_view_ndof_invoke;
  ot->poll = ED_space_clip_view_clip_poll;
}
#endif /* WITH_INPUT_NDOF */

/********************** Prefetch operator *********************/

static int clip_prefetch_modal(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  /* no running blender, remove handler and pass through */
  if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_wm_area(C), WM_JOB_TYPE_CLIP_PREFETCH)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* running render */
  switch (event->type) {
    case ESCKEY:
      return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_PASS_THROUGH;
}

static int clip_prefetch_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(_event))
{
  clip_start_prefetch_job(C);

  /* add modal handler for ESC */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void CLIP_OT_prefetch(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Prefetch Frames";
  ot->idname = "CLIP_OT_prefetch";
  ot->description = "Prefetch frames from disk for faster playback/tracking";

  /* api callbacks */
  ot->poll = ED_space_clip_view_clip_poll;
  ot->invoke = clip_prefetch_invoke;
  ot->modal = clip_prefetch_modal;
}

/********************** Set scene frames *********************/

static int clip_set_scene_frames_exec(bContext *C, wmOperator *UNUSED(op))
{
  MovieClip *clip = CTX_data_edit_movieclip(C);
  Scene *scene = CTX_data_scene(C);
  int clip_length;

  if (ELEM(NULL, scene, clip)) {
    return OPERATOR_CANCELLED;
  }

  clip_length = BKE_movieclip_get_duration(clip);

  scene->r.sfra = clip->start_frame;
  scene->r.efra = scene->r.sfra + clip_length - 1;

  scene->r.efra = max_ii(scene->r.sfra, scene->r.efra);

  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

void CLIP_OT_set_scene_frames(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Scene Frames";
  ot->idname = "CLIP_OT_set_scene_frames";
  ot->description = "Set scene's start and end frame to match clip's start frame and length";

  /* api callbacks */
  ot->poll = ED_space_clip_view_clip_poll;
  ot->exec = clip_set_scene_frames_exec;
}

/******************** set 3d cursor operator ********************/

static int clip_set_2d_cursor_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sclip = CTX_wm_space_clip(C);
  bool show_cursor = false;

  show_cursor |= sclip->mode == SC_MODE_MASKEDIT;
  show_cursor |= sclip->around == V3D_AROUND_CURSOR;

  if (!show_cursor) {
    return OPERATOR_CANCELLED;
  }

  RNA_float_get_array(op->ptr, "location", sclip->cursor);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_CLIP, NULL);

  return OPERATOR_FINISHED;
}

static int clip_set_2d_cursor_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  SpaceClip *sclip = CTX_wm_space_clip(C);
  float location[2];

  ED_clip_mouse_pos(sclip, ar, event->mval, location);
  RNA_float_set_array(op->ptr, "location", location);

  return clip_set_2d_cursor_exec(C, op);
}

void CLIP_OT_cursor_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set 2D Cursor";
  ot->description = "Set 2D cursor location";
  ot->idname = "CLIP_OT_cursor_set";

  /* api callbacks */
  ot->exec = clip_set_2d_cursor_exec;
  ot->invoke = clip_set_2d_cursor_invoke;
  ot->poll = ED_space_clip_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "location",
                       2,
                       NULL,
                       -FLT_MAX,
                       FLT_MAX,
                       "Location",
                       "Cursor location in normalized clip coordinates",
                       -10.0f,
                       10.0f);
}

/********************** macros *********************/

void ED_operatormacros_clip(void)
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("CLIP_OT_add_marker_move",
                                    "Add Marker and Move",
                                    "Add new marker and move it on movie",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "CLIP_OT_add_marker");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_struct_idprops_unset(otmacro->ptr, "release_confirm");

  ot = WM_operatortype_append_macro(
      "CLIP_OT_add_marker_slide",
      "Add Marker and Slide",
      "Add new marker and slide it with mouse until mouse button release",
      OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "CLIP_OT_add_marker");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "release_confirm", true);
}
