/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include <cerrno>
#include <fcntl.h>
#include <mutex>
#include <sys/types.h>

#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_scene_types.h" /* min/max frames */
#include "DNA_userdef_types.h"

#include "BLI_fileops.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_movieclip.h"
#include "BKE_report.hh"
#include "BKE_tracking.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "MOV_read.hh"

#include "ED_clip.hh"
#include "ED_screen.hh"

#include "UI_interface.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "UI_view2d.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "clip_intern.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name View Navigation Utilities
 * \{ */

static void sclip_zoom_set(const bContext *C,
                           float zoom,
                           const float location[2],
                           const bool zoom_to_pos)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);

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
    else if (BLI_rcti_size_x(&region->winrct) <= sc->zoom) {
      sc->zoom = oldzoom;
    }
    else if (BLI_rcti_size_y(&region->winrct) <= sc->zoom) {
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

    if (clip_view_has_locked_selection(C)) {
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
  ARegion *region = CTX_wm_region(C);

  float location[2], *mpos = nullptr;

  if (event) {
    SpaceClip *sc = CTX_wm_space_clip(C);

    ED_clip_mouse_pos(sc, region, event->mval, location);
    mpos = location;
  }

  sclip_zoom_set_factor(C, factor, mpos, mpos ? (U.uiflag & USER_ZOOM_TO_MOUSEPOS) : false);

  ED_region_tag_redraw(region);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Open Clip Operator
 * \{ */

static void clip_filesel(bContext *C, wmOperator *op, const char *dirpath)
{
  RNA_string_set(op->ptr, "directory", dirpath);

  WM_event_add_fileselect(C, op);
}

static void open_init(bContext *C, wmOperator *op)
{
  PropertyPointerRNA *pprop;

  op->customdata = pprop = MEM_new<PropertyPointerRNA>("OpenPropertyPointerRNA");
  UI_context_active_but_prop_get_templateID(C, &pprop->ptr, &pprop->prop);
}

static void open_cancel(bContext * /*C*/, wmOperator *op)
{
  if (op->customdata) {
    PropertyPointerRNA *pprop = static_cast<PropertyPointerRNA *>(op->customdata);
    op->customdata = nullptr;
    MEM_delete(pprop);
  }
}

static wmOperatorStatus open_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  bScreen *screen = CTX_wm_screen(C);
  Main *bmain = CTX_data_main(C);
  PropertyPointerRNA *pprop;
  MovieClip *clip = nullptr;
  char filepath[FILE_MAX];

  if (!RNA_collection_is_empty(op->ptr, "files")) {
    PointerRNA fileptr;
    PropertyRNA *prop;
    char dir_only[FILE_MAX], file_only[FILE_MAX];
    bool relative = RNA_boolean_get(op->ptr, "relative_path");

    RNA_string_get(op->ptr, "directory", dir_only);
    if (relative) {
      BLI_path_rel(dir_only, bmain->filepath);
    }

    prop = RNA_struct_find_property(op->ptr, "files");
    RNA_property_collection_lookup_int(op->ptr, prop, 0, &fileptr);
    RNA_string_get(&fileptr, "name", file_only);

    BLI_path_join(filepath, sizeof(filepath), dir_only, file_only);
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "No files selected to be opened");

    return OPERATOR_CANCELLED;
  }

  /* default to frame 1 if there's no scene in context */

  errno = 0;

  clip = BKE_movieclip_file_add_exists(bmain, filepath);

  if (!clip) {
    if (op->customdata) {
      pprop = static_cast<PropertyPointerRNA *>(op->customdata);
      op->customdata = nullptr;
      MEM_delete(pprop);
    }

    BKE_reportf(op->reports,
                RPT_ERROR,
                "Cannot read '%s': %s",
                filepath,
                errno ? strerror(errno) : RPT_("unsupported movie clip format"));

    return OPERATOR_CANCELLED;
  }

  if (!op->customdata) {
    open_init(C, op);
  }

  /* hook into UI */
  pprop = static_cast<PropertyPointerRNA *>(op->customdata);

  if (pprop->prop) {
    /* when creating new ID blocks, use is already 1, but RNA
     * pointer use also increases user, so this compensates it */
    id_us_min(&clip->id);

    PointerRNA idptr = RNA_id_pointer_create(&clip->id);
    RNA_property_pointer_set(&pprop->ptr, pprop->prop, idptr, nullptr);
    RNA_property_update(C, &pprop->ptr, pprop->prop);
  }
  else if (sc) {
    ED_space_clip_set_clip(C, screen, sc, clip);
  }

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_ADDED, clip);

  DEG_relations_tag_update(bmain);
  op->customdata = nullptr;
  MEM_delete(pprop);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus open_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  char dirpath[FILE_MAX];
  MovieClip *clip = nullptr;

  if (sc) {
    clip = ED_space_clip_get_clip(sc);
  }

  if (clip) {
    STRNCPY(dirpath, clip->filepath);

    BLI_path_abs(dirpath, ID_BLEND_PATH_FROM_GLOBAL(&clip->id));
    BLI_path_parent_dir(dirpath);
  }
  else {
    STRNCPY(dirpath, U.textudir);
  }

  if (RNA_struct_property_is_set(op->ptr, "files")) {
    return open_exec(C, op);
  }

  if (!RNA_struct_property_is_set(op->ptr, "relative_path")) {
    RNA_boolean_set(op->ptr, "relative_path", (U.flag & USER_RELPATHS) != 0);
  }

  open_init(C, op);

  clip_filesel(C, op, dirpath);

  return OPERATOR_RUNNING_MODAL;
}

void CLIP_OT_open(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Open Clip";
  ot->description = "Load a sequence of frames or a movie file";
  ot->idname = "CLIP_OT_open";

  /* API callbacks. */
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
                                 FILE_SORT_DEFAULT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reload Clip Operator
 * \{ */

static wmOperatorStatus reload_exec(bContext *C, wmOperator * /*op*/)
{
  MovieClip *clip = CTX_data_edit_movieclip(C);

  if (!clip) {
    return OPERATOR_CANCELLED;
  }

  WM_jobs_kill_type(CTX_wm_manager(C), nullptr, WM_JOB_TYPE_CLIP_PREFETCH);
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

  /* API callbacks. */
  ot->exec = reload_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Pan Operator
 * \{ */

namespace {

struct ViewPanData {
  float x, y;
  float xof, yof, xorig, yorig;
  int launch_event;
  bool own_cursor;
  float *vec;
};

}  // namespace

static void view_pan_init(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  ViewPanData *vpd;

  op->customdata = vpd = MEM_callocN<ViewPanData>("ClipViewPanData");

  /* Grab will be set when running from gizmo. */
  vpd->own_cursor = WM_cursor_modal_is_set_ok(win);
  if (vpd->own_cursor) {
    WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
  }

  vpd->x = event->xy[0];
  vpd->y = event->xy[1];

  if (clip_view_has_locked_selection(C)) {
    vpd->vec = &sc->xlockof;
  }
  else {
    vpd->vec = &sc->xof;
  }

  copy_v2_v2(&vpd->xof, vpd->vec);
  copy_v2_v2(&vpd->xorig, &vpd->xof);

  vpd->launch_event = WM_userdef_event_type_from_keymap_type(event->type);

  WM_event_add_modal_handler(C, op);
}

static void view_pan_exit(bContext *C, wmOperator *op, bool cancel)
{
  ViewPanData *vpd = static_cast<ViewPanData *>(op->customdata);

  if (cancel) {
    copy_v2_v2(vpd->vec, &vpd->xorig);

    ED_region_tag_redraw(CTX_wm_region(C));
  }

  if (vpd->own_cursor) {
    WM_cursor_modal_restore(CTX_wm_window(C));
  }
  MEM_freeN(vpd);
}

static wmOperatorStatus view_pan_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  float offset[2];

  RNA_float_get_array(op->ptr, "offset", offset);

  if (clip_view_has_locked_selection(C)) {
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

static wmOperatorStatus view_pan_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type == MOUSEPAN) {
    SpaceClip *sc = CTX_wm_space_clip(C);
    float offset[2];

    offset[0] = (event->prev_xy[0] - event->xy[0]) / sc->zoom;
    offset[1] = (event->prev_xy[1] - event->xy[1]) / sc->zoom;

    RNA_float_set_array(op->ptr, "offset", offset);

    view_pan_exec(C, op);

    return OPERATOR_FINISHED;
  }

  view_pan_init(C, op, event);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus view_pan_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ViewPanData *vpd = static_cast<ViewPanData *>(op->customdata);
  float offset[2];

  switch (event->type) {
    case MOUSEMOVE:
      copy_v2_v2(vpd->vec, &vpd->xorig);
      offset[0] = (vpd->x - event->xy[0]) / sc->zoom;
      offset[1] = (vpd->y - event->xy[1]) / sc->zoom;
      RNA_float_set_array(op->ptr, "offset", offset);
      view_pan_exec(C, op);
      break;
    case EVT_ESCKEY:
      view_pan_exit(C, op, true);

      return OPERATOR_CANCELLED;
    case EVT_SPACEKEY:
      view_pan_exit(C, op, false);

      return OPERATOR_FINISHED;
    default:
      if (event->type == vpd->launch_event && event->val == KM_RELEASE) {
        view_pan_exit(C, op, false);

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

  /* API callbacks. */
  ot->exec = view_pan_exec;
  ot->invoke = view_pan_invoke;
  ot->modal = view_pan_modal;
  ot->cancel = view_pan_cancel;
  ot->poll = ED_space_clip_view_clip_poll;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY | OPTYPE_LOCK_BYPASS;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "offset",
                       2,
                       nullptr,
                       -FLT_MAX,
                       FLT_MAX,
                       "Offset",
                       "Offset in floating-point units, 1.0 is the width and height of the image",
                       -FLT_MAX,
                       FLT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Zoom Operator
 * \{ */

namespace {

struct ViewZoomData {
  float x, y;
  float zoom;
  int launch_event;
  float location[2];
  wmTimer *timer;
  double timer_lastdraw;
  bool own_cursor;
};

}  // namespace

static void view_zoom_init(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);
  ViewZoomData *vpd;

  op->customdata = vpd = MEM_callocN<ViewZoomData>("ClipViewZoomData");

  /* Grab will be set when running from gizmo. */
  vpd->own_cursor = WM_cursor_modal_is_set_ok(win);
  if (vpd->own_cursor) {
    WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
  }

  if (U.viewzoom == USER_ZOOM_CONTINUE) {
    /* needs a timer to continue redrawing */
    vpd->timer = WM_event_timer_add(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);
    vpd->timer_lastdraw = BLI_time_now_seconds();
  }

  vpd->x = event->xy[0];
  vpd->y = event->xy[1];
  vpd->zoom = sc->zoom;
  vpd->launch_event = WM_userdef_event_type_from_keymap_type(event->type);

  ED_clip_mouse_pos(sc, region, event->mval, vpd->location);

  WM_event_add_modal_handler(C, op);
}

static void view_zoom_exit(bContext *C, wmOperator *op, bool cancel)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ViewZoomData *vpd = static_cast<ViewZoomData *>(op->customdata);

  if (cancel) {
    sc->zoom = vpd->zoom;
    ED_region_tag_redraw(CTX_wm_region(C));
  }

  if (vpd->timer) {
    WM_event_timer_remove(CTX_wm_manager(C), vpd->timer->win, vpd->timer);
  }

  if (vpd->own_cursor) {
    WM_cursor_modal_restore(CTX_wm_window(C));
  }
  MEM_freeN(vpd);
}

static wmOperatorStatus view_zoom_exec(bContext *C, wmOperator *op)
{
  sclip_zoom_set_factor(C, RNA_float_get(op->ptr, "factor"), nullptr, false);

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

static wmOperatorStatus view_zoom_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (ELEM(event->type, MOUSEZOOM, MOUSEPAN)) {
    float delta, factor;

    delta = event->prev_xy[0] - event->xy[0] + event->prev_xy[1] - event->xy[1];

    if (U.uiflag & USER_ZOOM_INVERT) {
      delta *= -1;
    }

    factor = 1.0f + delta / 300.0f;
    RNA_float_set(op->ptr, "factor", factor);

    sclip_zoom_set_factor_exec(C, event, factor);

    return OPERATOR_FINISHED;
  }

  view_zoom_init(C, op, event);

  return OPERATOR_RUNNING_MODAL;
}

static void view_zoom_apply(
    bContext *C, ViewZoomData *vpd, wmOperator *op, const wmEvent *event, const bool zoom_to_pos)
{
  float factor;
  float delta;

  if (U.viewzoom != USER_ZOOM_SCALE) {
    if (U.uiflag & USER_ZOOM_HORIZ) {
      delta = float(event->xy[0] - vpd->x);
    }
    else {
      delta = float(event->xy[1] - vpd->y);
    }
  }
  else {
    delta = event->xy[0] - vpd->x + event->xy[1] - vpd->y;
  }

  delta /= U.pixelsize;

  if (U.uiflag & USER_ZOOM_INVERT) {
    delta = -delta;
  }

  if (U.viewzoom == USER_ZOOM_CONTINUE) {
    SpaceClip *sclip = CTX_wm_space_clip(C);
    double time = BLI_time_now_seconds();
    float time_step = float(time - vpd->timer_lastdraw);
    float zfac;

    zfac = 1.0f + ((delta / 20.0f) * time_step);
    vpd->timer_lastdraw = time;
    factor = (sclip->zoom * zfac) / vpd->zoom;
  }
  else {
    factor = 1.0f + delta / 300.0f;
  }

  RNA_float_set(op->ptr, "factor", factor);
  sclip_zoom_set(C, vpd->zoom * factor, vpd->location, zoom_to_pos);
  ED_region_tag_redraw(CTX_wm_region(C));
}

static wmOperatorStatus view_zoom_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewZoomData *vpd = static_cast<ViewZoomData *>(op->customdata);
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
      if (event->type == vpd->launch_event && event->val == KM_RELEASE) {
        view_zoom_exit(C, op, false);

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

  /* API callbacks. */
  ot->exec = view_zoom_exec;
  ot->invoke = view_zoom_invoke;
  ot->modal = view_zoom_modal;
  ot->cancel = view_zoom_cancel;
  ot->poll = ED_space_clip_view_clip_poll;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY | OPTYPE_LOCK_BYPASS;

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Zoom In/Out Operator
 * \{ */

static wmOperatorStatus view_zoom_in_exec(bContext *C, wmOperator *op)
{
  float location[2];

  RNA_float_get_array(op->ptr, "location", location);

  sclip_zoom_set_factor(C, powf(2.0f, 1.0f / 3.0f), location, U.uiflag & USER_ZOOM_TO_MOUSEPOS);

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

static wmOperatorStatus view_zoom_in_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);

  float location[2];

  ED_clip_mouse_pos(sc, region, event->mval, location);
  RNA_float_set_array(op->ptr, "location", location);

  return view_zoom_in_exec(C, op);
}

void CLIP_OT_view_zoom_in(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Zoom In";
  ot->idname = "CLIP_OT_view_zoom_in";
  ot->description = "Zoom in the view";

  /* API callbacks. */
  ot->exec = view_zoom_in_exec;
  ot->invoke = view_zoom_in_invoke;
  ot->poll = ED_space_clip_view_clip_poll;

  /* flags */
  ot->flag = OPTYPE_LOCK_BYPASS;

  /* properties */
  prop = RNA_def_float_vector(ot->srna,
                              "location",
                              2,
                              nullptr,
                              -FLT_MAX,
                              FLT_MAX,
                              "Location",
                              "Cursor location in screen coordinates",
                              -10.0f,
                              10.0f);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

static wmOperatorStatus view_zoom_out_exec(bContext *C, wmOperator *op)
{
  float location[2];

  RNA_float_get_array(op->ptr, "location", location);

  sclip_zoom_set_factor(C, powf(0.5f, 1.0f / 3.0f), location, U.uiflag & USER_ZOOM_TO_MOUSEPOS);

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

static wmOperatorStatus view_zoom_out_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);

  float location[2];

  ED_clip_mouse_pos(sc, region, event->mval, location);
  RNA_float_set_array(op->ptr, "location", location);

  return view_zoom_out_exec(C, op);
}

void CLIP_OT_view_zoom_out(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Zoom Out";
  ot->idname = "CLIP_OT_view_zoom_out";
  ot->description = "Zoom out the view";

  /* API callbacks. */
  ot->exec = view_zoom_out_exec;
  ot->invoke = view_zoom_out_invoke;
  ot->poll = ED_space_clip_view_clip_poll;

  /* flags */
  ot->flag = OPTYPE_LOCK_BYPASS;

  /* properties */
  prop = RNA_def_float_vector(ot->srna,
                              "location",
                              2,
                              nullptr,
                              -FLT_MAX,
                              FLT_MAX,
                              "Location",
                              "Cursor location in normalized (0.0 to 1.0) coordinates",
                              -10.0f,
                              10.0f);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Zoom Ratio Operator
 * \{ */

static wmOperatorStatus view_zoom_ratio_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  sclip_zoom_set(C, RNA_float_get(op->ptr, "ratio"), nullptr, false);

  /* ensure pixel exact locations for draw */
  sc->xof = int(sc->xof);
  sc->yof = int(sc->yof);

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

void CLIP_OT_view_zoom_ratio(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Zoom Ratio";
  ot->idname = "CLIP_OT_view_zoom_ratio";
  ot->description = "Set the zoom ratio (based on clip size)";

  /* API callbacks. */
  ot->exec = view_zoom_ratio_exec;
  ot->poll = ED_space_clip_view_clip_poll;

  /* flags */
  ot->flag = OPTYPE_LOCK_BYPASS;

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name View All Operator
 * \{ */

static wmOperatorStatus view_all_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc;
  ARegion *region;
  int w, h, width, height;
  float aspx, aspy;
  bool fit_view = RNA_boolean_get(op->ptr, "fit_view");
  float zoomx, zoomy;

  /* retrieve state */
  sc = CTX_wm_space_clip(C);
  region = CTX_wm_region(C);

  ED_space_clip_get_size(sc, &w, &h);
  ED_space_clip_get_aspect(sc, &aspx, &aspy);

  w = w * aspx;
  h = h * aspy;

  /* check if the image will fit in the image with zoom == 1 */
  width = BLI_rcti_size_x(&region->winrct) + 1;
  height = BLI_rcti_size_y(&region->winrct) + 1;

  if (fit_view) {
    const int margin = 5; /* margin from border */

    zoomx = float(width) / (w + 2 * margin);
    zoomy = float(height) / (h + 2 * margin);

    sclip_zoom_set(C, min_ff(zoomx, zoomy), nullptr, false);
  }
  else {
    if ((w >= width || h >= height) && (width > 0 && height > 0)) {
      zoomx = float(width) / w;
      zoomy = float(height) / h;

      /* find the zoom value that will fit the image in the image space */
      sclip_zoom_set(C, 1.0f / power_of_2(1.0f / min_ff(zoomx, zoomy)), nullptr, false);
    }
    else {
      sclip_zoom_set(C, 1.0f, nullptr, false);
    }
  }

  sc->xof = sc->yof = 0.0f;

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

void CLIP_OT_view_all(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Frame All";
  ot->idname = "CLIP_OT_view_all";
  ot->description = "View whole image with markers";

  /* API callbacks. */
  ot->exec = view_all_exec;
  ot->poll = ED_space_clip_view_clip_poll;

  /* flags */
  ot->flag = OPTYPE_LOCK_BYPASS;

  /* properties */
  prop = RNA_def_boolean(ot->srna, "fit_view", false, "Fit View", "Fit frame to the viewport");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Center View To Cursor Operator
 * \{ */

static wmOperatorStatus view_center_cursor_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);

  clip_view_center_to_point(sc, sc->cursor[0], sc->cursor[1]);

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

void CLIP_OT_view_center_cursor(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Center View to Cursor";
  ot->description = "Center the view so that the cursor is in the middle of the view";
  ot->idname = "CLIP_OT_view_center_cursor";

  /* API callbacks. */
  ot->exec = view_center_cursor_exec;
  ot->poll = ED_space_clip_maskedit_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Selected Operator
 * \{ */

static wmOperatorStatus view_selected_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);

  sc->xlockof = 0.0f;
  sc->ylockof = 0.0f;

  ED_clip_view_selection(C, region, true);
  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

void CLIP_OT_view_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Frame Selected";
  ot->idname = "CLIP_OT_view_selected";
  ot->description = "View all selected elements";

  /* API callbacks. */
  ot->exec = view_selected_exec;
  ot->poll = ED_space_clip_view_clip_poll;

  /* flags */
  ot->flag = OPTYPE_LOCK_BYPASS;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Change Frame Operator
 * \{ */

static bool change_frame_poll(bContext *C)
{
  /* prevent changes during render */
  if (G.is_rendering) {
    return false;
  }
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  return space_clip != nullptr;
}

static void change_frame_apply(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  /* set the new frame number */
  scene->r.cfra = RNA_int_get(op->ptr, "frame");
  FRAMENUMBER_MIN_CLAMP(scene->r.cfra);
  scene->r.subframe = 0.0f;

  /* do updates */
  DEG_id_tag_update(&scene->id, ID_RECALC_FRAME_CHANGE);
  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
}

static wmOperatorStatus change_frame_exec(bContext *C, wmOperator *op)
{
  change_frame_apply(C, op);

  return OPERATOR_FINISHED;
}

static int frame_from_event(bContext *C, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  int framenr = 0;

  if (region->regiontype == RGN_TYPE_WINDOW) {
    float sfra = scene->r.sfra, efra = scene->r.efra, framelen = region->winx / (efra - sfra + 1);

    framenr = sfra + event->mval[0] / framelen;
  }
  else {
    float viewx, viewy;

    UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &viewx, &viewy);

    framenr = round_fl_to_int(viewx);
  }

  return framenr;
}

static wmOperatorStatus change_frame_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);

  if (region->regiontype == RGN_TYPE_WINDOW) {
    if (event->mval[1] > 16 * UI_SCALE_FAC) {
      return OPERATOR_PASS_THROUGH;
    }
  }

  RNA_int_set(op->ptr, "frame", frame_from_event(C, event));

  change_frame_apply(C, op);

  /* add temp handler */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus change_frame_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  switch (event->type) {
    case EVT_ESCKEY:
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
    default: {
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

void CLIP_OT_change_frame(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Change Frame";
  ot->idname = "CLIP_OT_change_frame";
  ot->description = "Interactively change the current frame number";

  /* API callbacks. */
  ot->exec = change_frame_exec;
  ot->invoke = change_frame_invoke;
  ot->modal = change_frame_modal;
  ot->poll = change_frame_poll;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_UNDO;

  /* rna */
  RNA_def_int(ot->srna, "frame", 0, MINAFRAME, MAXFRAME, "Frame", "", MINAFRAME, MAXFRAME);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rebuild Proxies Operator
 * \{ */

struct ProxyJob {
  Scene *scene;
  Main *main;
  MovieClip *clip;
  int clip_flag;
  bool stop;
  MovieProxyBuilder *proxy_builder;
};

static void proxy_freejob(void *pjv)
{
  ProxyJob *pj = static_cast<ProxyJob *>(pjv);

  MEM_freeN(pj);
}

static int proxy_bitflag_to_array(int size_flag, int build_sizes[4], int undistort)
{
  int build_count = 0;
  const int size_flags[2][4] = {
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
                           int * /*build_sizes*/,
                           int /*build_count*/,
                           const int *build_undistort_sizes,
                           int build_undistort_count,
                           bool *stop,
                           bool *do_update,
                           float *progress)
{
  ProxyJob *pj = static_cast<ProxyJob *>(pjv);
  MovieClip *clip = pj->clip;
  MovieDistortion *distortion = nullptr;

  if (pj->proxy_builder) {
    MOV_proxy_builder_process(pj->proxy_builder, stop, do_update, progress);
  }

  if (!build_undistort_count) {
    if (*stop) {
      pj->stop = true;
    }

    return;
  }

  const int sfra = 1;
  const int efra = clip->len;

  if (build_undistort_count) {
    int width, height;

    BKE_movieclip_get_size(clip, nullptr, &width, &height);

    distortion = BKE_tracking_distortion_new(&clip->tracking, width, height);
  }

  for (int cfra = sfra; cfra <= efra; cfra++) {
    BKE_movieclip_build_proxy_frame(
        clip, pj->clip_flag, distortion, cfra, build_undistort_sizes, build_undistort_count, true);

    if (*stop || G.is_break) {
      break;
    }

    *do_update = true;
    *progress = (float(cfra) - sfra) / (efra - sfra);
  }

  if (distortion) {
    BKE_tracking_distortion_free(distortion);
  }

  if (*stop) {
    pj->stop = true;
  }
}

/* *****
 * special case for sequences -- handle different frames in different threads,
 * loading from disk happens in critical section, decoding frame happens from
 * thread for maximal speed
 */

struct ProxyQueue {
  int cfra;
  int sfra;
  int efra;
  std::mutex mutex;

  const bool *stop;
  bool *do_update;
  float *progress;
};

struct ProxyThread {
  MovieClip *clip;
  MovieDistortion *distortion;
  int *build_sizes, build_count;
  int *build_undistort_sizes, build_undistort_count;
};

static uchar *proxy_thread_next_frame(ProxyQueue *queue,
                                      MovieClip *clip,
                                      size_t *r_size,
                                      int *r_cfra)
{
  uchar *mem = nullptr;

  std::lock_guard lock(queue->mutex);
  if (!*queue->stop && queue->cfra <= queue->efra) {
    MovieClipUser user = *DNA_struct_default_get(MovieClipUser);
    char filepath[FILE_MAX];
    int file;

    user.framenr = queue->cfra;

    BKE_movieclip_filepath_for_frame(clip, &user, filepath);

    file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
    if (file < 0) {
      return nullptr;
    }

    const size_t size = BLI_file_descriptor_size(file);
    if (UNLIKELY(ELEM(size, 0, size_t(-1)))) {
      close(file);
      return nullptr;
    }

    mem = MEM_calloc_arrayN<uchar>(size, "movieclip proxy memory file");

    if (BLI_read(file, mem, size) != size) {
      close(file);
      MEM_freeN(mem);
      return nullptr;
    }

    *r_size = size;
    *r_cfra = queue->cfra;

    queue->cfra++;
    close(file);

    *queue->do_update = true;
    *queue->progress = float(queue->cfra - queue->sfra) / (queue->efra - queue->sfra);
  }
  return mem;
}

static void proxy_task_func(TaskPool *__restrict pool, void *task_data)
{
  ProxyThread *data = (ProxyThread *)task_data;
  ProxyQueue *queue = (ProxyQueue *)BLI_task_pool_user_data(pool);
  uchar *mem;
  size_t size;
  int cfra;

  while ((mem = proxy_thread_next_frame(queue, data->clip, &size, &cfra))) {
    ImBuf *ibuf;

    ibuf = IMB_load_image_from_memory(mem,
                                      size,
                                      IB_byte_data | IB_multilayer | IB_alphamode_detect,
                                      "proxy frame",
                                      nullptr,
                                      data->clip->colorspace_settings.name);

    BKE_movieclip_build_proxy_frame_for_ibuf(
        data->clip, ibuf, nullptr, cfra, data->build_sizes, data->build_count, false);

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
                              /* Cannot be const, because it is assigned to a non-const variable.
                               * NOLINTNEXTLINE: readability-non-const-parameter. */
                              bool *stop,
                              bool *do_update,
                              float *progress)
{
  ProxyJob *pj = static_cast<ProxyJob *>(pjv);
  MovieClip *clip = pj->clip;
  Scene *scene = pj->scene;
  int sfra = scene->r.sfra, efra = scene->r.efra;
  ProxyThread *handles;
  int tot_thread = BLI_task_scheduler_num_threads();
  int width, height;

  if (build_undistort_count) {
    BKE_movieclip_get_size(clip, nullptr, &width, &height);
  }

  ProxyQueue queue;
  queue.cfra = sfra;
  queue.sfra = sfra;
  queue.efra = efra;
  queue.stop = stop;
  queue.do_update = do_update;
  queue.progress = progress;

  TaskPool *task_pool = BLI_task_pool_create(&queue, TASK_PRIORITY_LOW);
  handles = MEM_calloc_arrayN<ProxyThread>(tot_thread, "proxy threaded handles");
  for (int i = 0; i < tot_thread; i++) {
    ProxyThread *handle = &handles[i];

    handle->clip = clip;

    handle->build_count = build_count;
    handle->build_sizes = build_sizes;

    handle->build_undistort_count = build_undistort_count;
    handle->build_undistort_sizes = build_undistort_sizes;

    if (build_undistort_count) {
      handle->distortion = BKE_tracking_distortion_new(&clip->tracking, width, height);
    }

    BLI_task_pool_push(task_pool, proxy_task_func, handle, false, nullptr);
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  if (build_undistort_count) {
    for (int i = 0; i < tot_thread; i++) {
      ProxyThread *handle = &handles[i];
      BKE_tracking_distortion_free(handle->distortion);
    }
  }

  MEM_freeN(handles);
}

static void proxy_startjob(void *pjv, wmJobWorkerStatus *worker_status)
{
  ProxyJob *pj = static_cast<ProxyJob *>(pjv);
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
                   &worker_status->stop,
                   &worker_status->do_update,
                   &worker_status->progress);
  }
  else {
    do_sequence_proxy(pjv,
                      build_sizes,
                      build_count,
                      build_undistort_sizes,
                      build_undistort_count,
                      &worker_status->stop,
                      &worker_status->do_update,
                      &worker_status->progress);
  }
}

static void proxy_endjob(void *pjv)
{
  ProxyJob *pj = static_cast<ProxyJob *>(pjv);

  if (pj->clip->anim) {
    MOV_close_proxies(pj->clip->anim);
  }

  if (pj->proxy_builder) {
    MOV_proxy_builder_finish(pj->proxy_builder, pj->stop);
  }

  if (pj->clip->source == MCLIP_SRC_MOVIE) {
    /* Time-code might have changed, so do a full reload to deal with this. */
    DEG_id_tag_update(&pj->clip->id, ID_RECALC_SOURCE);
  }
  else {
    /* For image sequences we'll preserve original cache. */
    BKE_movieclip_clear_proxy_cache(pj->clip);
  }

  WM_main_add_notifier(NC_MOVIECLIP | ND_DISPLAY, pj->clip);
}

static wmOperatorStatus clip_rebuild_proxy_exec(bContext *C, wmOperator * /*op*/)
{
  wmJob *wm_job;
  ProxyJob *pj;
  Scene *scene = CTX_data_scene(C);
  ScrArea *area = CTX_wm_area(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);

  if ((clip->flag & MCLIP_USE_PROXY) == 0) {
    return OPERATOR_CANCELLED;
  }

  wm_job = WM_jobs_get(CTX_wm_manager(C),
                       CTX_wm_window(C),
                       scene,
                       "Building proxies...",
                       WM_JOB_PROGRESS,
                       WM_JOB_TYPE_CLIP_BUILD_PROXY);

  pj = MEM_callocN<ProxyJob>("proxy rebuild job");
  pj->scene = scene;
  pj->main = CTX_data_main(C);
  pj->clip = clip;
  pj->clip_flag = clip->flag & MCLIP_TIMECODE_FLAGS;

  if (clip->anim) {
    pj->proxy_builder = MOV_proxy_builder_start(clip->anim,
                                                IMB_Timecode_Type(clip->proxy.build_tc_flag),
                                                IMB_Proxy_Size(clip->proxy.build_size_flag),
                                                clip->proxy.quality,
                                                true,
                                                nullptr,
                                                false);
  }

  WM_jobs_customdata_set(wm_job, pj, proxy_freejob);
  WM_jobs_timer(wm_job, 0.2, NC_MOVIECLIP | ND_DISPLAY, 0);
  WM_jobs_callbacks(wm_job, proxy_startjob, nullptr, nullptr, proxy_endjob);

  G.is_break = false;
  WM_jobs_start(CTX_wm_manager(C), wm_job);

  ED_area_tag_redraw(area);

  return OPERATOR_FINISHED;
}

void CLIP_OT_rebuild_proxy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rebuild Proxy and Timecode Indices";
  ot->idname = "CLIP_OT_rebuild_proxy";
  ot->description = "Rebuild all selected proxies and timecode indices in the background";

  /* API callbacks. */
  ot->exec = clip_rebuild_proxy_exec;
  ot->poll = ED_space_clip_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mode Set Operator
 * \{ */

static wmOperatorStatus mode_set_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  int mode = RNA_enum_get(op->ptr, "mode");

  sc->mode = mode;

  if (sc->mode == SC_MODE_MASKEDIT && sc->view != SC_VIEW_CLIP) {
    /* Make sure we are in the right view for mask editing */
    sc->view = SC_VIEW_CLIP;
  }

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_CLIP, nullptr);

  return OPERATOR_FINISHED;
}

void CLIP_OT_mode_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Clip Mode";
  ot->description = "Set the clip interaction mode";
  ot->idname = "CLIP_OT_mode_set";

  /* API callbacks. */
  ot->exec = mode_set_exec;

  ot->poll = ED_space_clip_poll;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", rna_enum_clip_editor_mode_items, SC_MODE_TRACKING, "Mode", "");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
}

/** \} */

#ifdef WITH_INPUT_NDOF

/* -------------------------------------------------------------------- */
/** \name NDOF Operator
 * \{ */

/* Combined pan/zoom from a 3D mouse device.
 * Z zooms, XY pans
 * "view" (not "paper") control -- user moves the viewpoint, not the image being viewed
 * that explains the negative signs in the code below
 */

static wmOperatorStatus clip_view_ndof_invoke(bContext *C,
                                              wmOperator * /*op*/,
                                              const wmEvent *event)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);

  const wmNDOFMotionData &ndof = *static_cast<wmNDOFMotionData *>(event->customdata);
  const float pan_speed = NDOF_PIXELS_PER_SECOND;

  blender::float3 pan_vec = ndof.time_delta * WM_event_ndof_translation_get_for_navigation(ndof);
  mul_v2_fl(pan_vec, pan_speed / sc->zoom);

  sclip_zoom_set_factor(C, max_ff(0.0f, 1.0f - pan_vec[2]), nullptr, false);
  sc->xof += pan_vec[0];
  sc->yof += pan_vec[1];

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

void CLIP_OT_view_ndof(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "NDOF Pan/Zoom";
  ot->idname = "CLIP_OT_view_ndof";
  ot->description = "Use a 3D mouse device to pan/zoom the view";

  /* API callbacks. */
  ot->invoke = clip_view_ndof_invoke;
  ot->poll = ED_space_clip_view_clip_poll;

  /* flags */
  ot->flag = OPTYPE_LOCK_BYPASS;
}

/** \} */

#endif /* WITH_INPUT_NDOF */

/* -------------------------------------------------------------------- */
/** \name Prefetch Operator
 * \{ */

static wmOperatorStatus clip_prefetch_modal(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  /* no running blender, remove handler and pass through */
  if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_wm_area(C), WM_JOB_TYPE_CLIP_PREFETCH)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* running render */
  switch (event->type) {
    case EVT_ESCKEY:
      return OPERATOR_RUNNING_MODAL;
    default: {
      break;
    }
  }

  return OPERATOR_PASS_THROUGH;
}

static wmOperatorStatus clip_prefetch_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent * /*_event*/)
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

  /* API callbacks. */
  ot->poll = ED_space_clip_view_clip_poll;
  ot->invoke = clip_prefetch_invoke;
  ot->modal = clip_prefetch_modal;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Scene Frames Operator
 * \{ */

static wmOperatorStatus clip_set_scene_frames_exec(bContext *C, wmOperator * /*op*/)
{
  MovieClip *clip = CTX_data_edit_movieclip(C);
  Scene *scene = CTX_data_scene(C);
  int clip_length;

  if (ELEM(nullptr, scene, clip)) {
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

  /* API callbacks. */
  ot->poll = ED_space_clip_view_clip_poll;
  ot->exec = clip_set_scene_frames_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set 3d Cursor Operator
 * \{ */

static wmOperatorStatus clip_set_2d_cursor_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sclip = CTX_wm_space_clip(C);
  bool show_cursor = false;

  show_cursor |= sclip->mode == SC_MODE_MASKEDIT;
  show_cursor |= sclip->around == V3D_AROUND_CURSOR;

  if (!show_cursor) {
    return OPERATOR_CANCELLED;
  }

  RNA_float_get_array(op->ptr, "location", sclip->cursor);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* Use pass-through to allow click-drag to transform the cursor. */
  return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
}

static wmOperatorStatus clip_set_2d_cursor_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceClip *sclip = CTX_wm_space_clip(C);
  float location[2];

  ED_clip_mouse_pos(sclip, region, event->mval, location);
  RNA_float_set_array(op->ptr, "location", location);

  return clip_set_2d_cursor_exec(C, op);
}

void CLIP_OT_cursor_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set 2D Cursor";
  ot->description = "Set 2D cursor location";
  ot->idname = "CLIP_OT_cursor_set";

  /* API callbacks. */
  ot->exec = clip_set_2d_cursor_exec;
  ot->invoke = clip_set_2d_cursor_invoke;
  ot->poll = ED_space_clip_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "location",
                       2,
                       nullptr,
                       -FLT_MAX,
                       FLT_MAX,
                       "Location",
                       "Cursor location in normalized clip coordinates",
                       -10.0f,
                       10.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Lock To Selection Operator
 * \{ */

static wmOperatorStatus lock_selection_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);

  ClipViewLockState lock_state;
  ED_clip_view_lock_state_store(C, &lock_state);

  space_clip->flag ^= SC_LOCK_SELECTION;

  ED_clip_view_lock_state_restore_no_jump(C, &lock_state);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_CLIP, nullptr);

  return OPERATOR_FINISHED;
}

void CLIP_OT_lock_selection_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Lock Selection";
  ot->description = "Toggle Lock Selection option of the current clip editor";
  ot->idname = "CLIP_OT_lock_selection_toggle";

  /* API callbacks. */
  ot->poll = ED_space_clip_poll;
  ot->exec = lock_selection_toggle_exec;

  /* flags */
  ot->flag = OPTYPE_LOCK_BYPASS;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Macros
 * \{ */

void ED_operatormacros_clip()
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("CLIP_OT_add_marker_move",
                                    "Add Marker and Move",
                                    "Add new marker and move it on movie",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "CLIP_OT_add_marker");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_struct_system_idprops_unset(otmacro->ptr, "release_confirm");

  ot = WM_operatortype_append_macro(
      "CLIP_OT_add_marker_slide",
      "Add Marker and Slide",
      "Add new marker and slide it with mouse until mouse button release",
      OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "CLIP_OT_add_marker");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "release_confirm", true);
}

/** \} */
