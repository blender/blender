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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup spimage
 */

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_camera_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_image_save.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_state.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_moviecache.h"
#include "intern/openexr/openexr_multi.h"

#include "RE_pipeline.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_image.h"
#include "ED_mask.h"
#include "ED_paint.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_util.h"
#include "ED_util_imbuf.h"
#include "ED_uvedit.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "PIL_time.h"

#include "RE_engine.h"

#include "image_intern.h"

/* -------------------------------------------------------------------- */
/** \name View Navigation Utilities
 * \{ */

static void sima_zoom_set(
    SpaceImage *sima, ARegion *region, float zoom, const float location[2], const bool zoom_to_pos)
{
  float oldzoom = sima->zoom;
  int width, height;

  sima->zoom = zoom;

  if (sima->zoom < 0.1f || sima->zoom > 4.0f) {
    /* check zoom limits */
    ED_space_image_get_size(sima, &width, &height);

    width *= sima->zoom;
    height *= sima->zoom;

    if ((width < 4) && (height < 4) && sima->zoom < oldzoom) {
      sima->zoom = oldzoom;
    }
    else if (BLI_rcti_size_x(&region->winrct) <= sima->zoom) {
      sima->zoom = oldzoom;
    }
    else if (BLI_rcti_size_y(&region->winrct) <= sima->zoom) {
      sima->zoom = oldzoom;
    }
  }

  if (zoom_to_pos && location) {
    float aspx, aspy, w, h;

    ED_space_image_get_size(sima, &width, &height);
    ED_space_image_get_aspect(sima, &aspx, &aspy);

    w = width * aspx;
    h = height * aspy;

    sima->xof += ((location[0] - 0.5f) * w - sima->xof) * (sima->zoom - oldzoom) / sima->zoom;
    sima->yof += ((location[1] - 0.5f) * h - sima->yof) * (sima->zoom - oldzoom) / sima->zoom;
  }
}

static void sima_zoom_set_factor(SpaceImage *sima,
                                 ARegion *region,
                                 float zoomfac,
                                 const float location[2],
                                 const bool zoom_to_pos)
{
  sima_zoom_set(sima, region, sima->zoom * zoomfac, location, zoom_to_pos);
}

/**
 * Fits the view to the bounds exactly, caller should add margin if needed.
 */
static void sima_zoom_set_from_bounds(SpaceImage *sima, ARegion *region, const rctf *bounds)
{
  int image_size[2];
  float aspx, aspy;

  ED_space_image_get_size(sima, &image_size[0], &image_size[1]);
  ED_space_image_get_aspect(sima, &aspx, &aspy);

  image_size[0] = image_size[0] * aspx;
  image_size[1] = image_size[1] * aspy;

  /* adjust offset and zoom */
  sima->xof = roundf((BLI_rctf_cent_x(bounds) - 0.5f) * image_size[0]);
  sima->yof = roundf((BLI_rctf_cent_y(bounds) - 0.5f) * image_size[1]);

  float size_xy[2], size;
  size_xy[0] = BLI_rcti_size_x(&region->winrct) / (BLI_rctf_size_x(bounds) * image_size[0]);
  size_xy[1] = BLI_rcti_size_y(&region->winrct) / (BLI_rctf_size_y(bounds) * image_size[1]);

  size = min_ff(size_xy[0], size_xy[1]);
  CLAMP_MAX(size, 100.0f);

  sima_zoom_set(sima, region, size, NULL, false);
}

static Image *image_from_context(const bContext *C)
{
  /* Edit image is set by templates used throughout the interface, so image
   * operations work outside the image editor. */
  Image *ima = CTX_data_pointer_get_type(C, "edit_image", &RNA_Image).data;

  if (ima) {
    return ima;
  }
  else {
    /* Image editor. */
    SpaceImage *sima = CTX_wm_space_image(C);
    return (sima) ? sima->image : NULL;
  }
}

static ImageUser *image_user_from_context(const bContext *C)
{
  /* Edit image user is set by templates used throughout the interface, so
   * image operations work outside the image editor. */
  ImageUser *iuser = CTX_data_pointer_get_type(C, "edit_image_user", &RNA_ImageUser).data;

  if (iuser) {
    return iuser;
  }
  else {
    /* Image editor. */
    SpaceImage *sima = CTX_wm_space_image(C);
    return (sima) ? &sima->iuser : NULL;
  }
}

static bool image_from_context_has_data_poll(bContext *C)
{
  Image *ima = image_from_context(C);
  ImageUser *iuser = image_user_from_context(C);

  if (ima == NULL) {
    return false;
  }

  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);
  const bool has_buffer = (ibuf && (ibuf->rect || ibuf->rect_float));
  BKE_image_release_ibuf(ima, ibuf, lock);
  return has_buffer;
}

/**
 * Use this when the image buffer is accessed without the image user.
 */
static bool image_from_context_has_data_poll_no_image_user(bContext *C)
{
  Image *ima = image_from_context(C);

  return BKE_image_has_ibuf(ima, NULL);
}

static bool image_not_packed_poll(bContext *C)
{
  /* Do not run 'replace' on packed images, it does not give user expected results at all. */
  Image *ima = image_from_context(C);
  return (ima && BLI_listbase_is_empty(&ima->packedfiles));
}

bool space_image_main_region_poll(bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  /* XXX ARegion *region = CTX_wm_region(C); */

  if (sima) {
    return true; /* XXX (region && region->type->regionid == RGN_TYPE_WINDOW); */
  }
  return false;
}

/* For IMAGE_OT_curves_point_set to avoid sampling when in uv smooth mode or editmode */
static bool space_image_main_area_not_uv_brush_poll(bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *toolsettings = scene->toolsettings;

  if (sima && !toolsettings->uvsculpt && (CTX_data_edit_object(C) == NULL)) {
    return 1;
  }

  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Pan Operator
 * \{ */

typedef struct ViewPanData {
  float x, y;
  float xof, yof;
  int launch_event;
  bool own_cursor;
} ViewPanData;

static void image_view_pan_init(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  ViewPanData *vpd;

  op->customdata = vpd = MEM_callocN(sizeof(ViewPanData), "ImageViewPanData");

  /* Grab will be set when running from gizmo. */
  vpd->own_cursor = (win->grabcursor == 0);
  if (vpd->own_cursor) {
    WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
  }

  vpd->x = event->x;
  vpd->y = event->y;
  vpd->xof = sima->xof;
  vpd->yof = sima->yof;
  vpd->launch_event = WM_userdef_event_type_from_keymap_type(event->type);

  WM_event_add_modal_handler(C, op);
}

static void image_view_pan_exit(bContext *C, wmOperator *op, bool cancel)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ViewPanData *vpd = op->customdata;

  if (cancel) {
    sima->xof = vpd->xof;
    sima->yof = vpd->yof;
    ED_region_tag_redraw(CTX_wm_region(C));
  }

  if (vpd->own_cursor) {
    WM_cursor_modal_restore(CTX_wm_window(C));
  }
  MEM_freeN(op->customdata);
}

static int image_view_pan_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  float offset[2];

  RNA_float_get_array(op->ptr, "offset", offset);
  sima->xof += offset[0];
  sima->yof += offset[1];

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

static int image_view_pan_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type == MOUSEPAN) {
    SpaceImage *sima = CTX_wm_space_image(C);
    float offset[2];

    offset[0] = (event->prevx - event->x) / sima->zoom;
    offset[1] = (event->prevy - event->y) / sima->zoom;
    RNA_float_set_array(op->ptr, "offset", offset);

    image_view_pan_exec(C, op);
    return OPERATOR_FINISHED;
  }
  else {
    image_view_pan_init(C, op, event);
    return OPERATOR_RUNNING_MODAL;
  }
}

static int image_view_pan_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ViewPanData *vpd = op->customdata;
  float offset[2];

  switch (event->type) {
    case MOUSEMOVE:
      sima->xof = vpd->xof;
      sima->yof = vpd->yof;
      offset[0] = (vpd->x - event->x) / sima->zoom;
      offset[1] = (vpd->y - event->y) / sima->zoom;
      RNA_float_set_array(op->ptr, "offset", offset);
      image_view_pan_exec(C, op);
      break;
    default:
      if (event->type == vpd->launch_event && event->val == KM_RELEASE) {
        image_view_pan_exit(C, op, false);
        return OPERATOR_FINISHED;
      }
      break;
  }

  return OPERATOR_RUNNING_MODAL;
}

static void image_view_pan_cancel(bContext *C, wmOperator *op)
{
  image_view_pan_exit(C, op, true);
}

void IMAGE_OT_view_pan(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pan View";
  ot->idname = "IMAGE_OT_view_pan";
  ot->description = "Pan the view";

  /* api callbacks */
  ot->exec = image_view_pan_exec;
  ot->invoke = image_view_pan_invoke;
  ot->modal = image_view_pan_modal;
  ot->cancel = image_view_pan_cancel;
  ot->poll = space_image_main_region_poll;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY | OPTYPE_LOCK_BYPASS;

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Zoom Operator
 * \{ */

typedef struct ViewZoomData {
  float origx, origy;
  float zoom;
  int launch_event;
  float location[2];

  /* needed for continuous zoom */
  wmTimer *timer;
  double timer_lastdraw;
  bool own_cursor;

  /* */
  SpaceImage *sima;
  ARegion *region;
} ViewZoomData;

static void image_view_zoom_init(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);
  ViewZoomData *vpd;

  op->customdata = vpd = MEM_callocN(sizeof(ViewZoomData), "ImageViewZoomData");

  /* Grab will be set when running from gizmo. */
  vpd->own_cursor = (win->grabcursor == 0);
  if (vpd->own_cursor) {
    WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
  }

  vpd->origx = event->x;
  vpd->origy = event->y;
  vpd->zoom = sima->zoom;
  vpd->launch_event = WM_userdef_event_type_from_keymap_type(event->type);

  UI_view2d_region_to_view(
      &region->v2d, event->mval[0], event->mval[1], &vpd->location[0], &vpd->location[1]);

  if (U.viewzoom == USER_ZOOM_CONT) {
    /* needs a timer to continue redrawing */
    vpd->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);
    vpd->timer_lastdraw = PIL_check_seconds_timer();
  }

  vpd->sima = sima;
  vpd->region = region;

  WM_event_add_modal_handler(C, op);
}

static void image_view_zoom_exit(bContext *C, wmOperator *op, bool cancel)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ViewZoomData *vpd = op->customdata;

  if (cancel) {
    sima->zoom = vpd->zoom;
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

static int image_view_zoom_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);

  sima_zoom_set_factor(sima, region, RNA_float_get(op->ptr, "factor"), NULL, false);

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

enum {
  VIEW_PASS = 0,
  VIEW_APPLY,
  VIEW_CONFIRM,
};

static int image_view_zoom_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type == MOUSEZOOM || event->type == MOUSEPAN) {
    SpaceImage *sima = CTX_wm_space_image(C);
    ARegion *region = CTX_wm_region(C);
    float delta, factor, location[2];

    UI_view2d_region_to_view(
        &region->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);

    delta = event->prevx - event->x + event->prevy - event->y;

    if (U.uiflag & USER_ZOOM_INVERT) {
      delta *= -1;
    }

    factor = 1.0f + delta / 300.0f;
    RNA_float_set(op->ptr, "factor", factor);
    const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");
    sima_zoom_set(sima,
                  region,
                  sima->zoom * factor,
                  location,
                  (use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS)));
    ED_region_tag_redraw(region);

    return OPERATOR_FINISHED;
  }
  else {
    image_view_zoom_init(C, op, event);
    return OPERATOR_RUNNING_MODAL;
  }
}

static void image_zoom_apply(ViewZoomData *vpd,
                             wmOperator *op,
                             const int x,
                             const int y,
                             const short viewzoom,
                             const short zoom_invert,
                             const bool zoom_to_pos)
{
  float factor;
  float delta;

  if (viewzoom != USER_ZOOM_SCALE) {
    if (U.uiflag & USER_ZOOM_HORIZ) {
      delta = (float)(x - vpd->origx);
    }
    else {
      delta = (float)(y - vpd->origy);
    }
  }
  else {
    delta = x - vpd->origx + y - vpd->origy;
  }

  delta /= U.pixelsize;

  if (zoom_invert) {
    delta = -delta;
  }

  if (viewzoom == USER_ZOOM_CONT) {
    double time = PIL_check_seconds_timer();
    float time_step = (float)(time - vpd->timer_lastdraw);
    float zfac;

    /* oldstyle zoom */
    zfac = 1.0f + ((delta / 20.0f) * time_step);
    vpd->timer_lastdraw = time;
    /* this is the final zoom, but instead make it into a factor */
    factor = (vpd->sima->zoom * zfac) / vpd->zoom;
  }
  else {
    factor = 1.0f + delta / 300.0f;
  }

  RNA_float_set(op->ptr, "factor", factor);
  sima_zoom_set(vpd->sima, vpd->region, vpd->zoom * factor, vpd->location, zoom_to_pos);
  ED_region_tag_redraw(vpd->region);
}

static int image_view_zoom_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewZoomData *vpd = op->customdata;
  short event_code = VIEW_PASS;

  /* execute the events */
  if (event->type == TIMER && event->customdata == vpd->timer) {
    /* continuous zoom */
    event_code = VIEW_APPLY;
  }
  else if (event->type == MOUSEMOVE) {
    event_code = VIEW_APPLY;
  }
  else if (event->type == vpd->launch_event && event->val == KM_RELEASE) {
    event_code = VIEW_CONFIRM;
  }

  if (event_code == VIEW_APPLY) {
    const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");
    image_zoom_apply(vpd,
                     op,
                     event->x,
                     event->y,
                     U.viewzoom,
                     (U.uiflag & USER_ZOOM_INVERT) != 0,
                     (use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS)));
  }
  else if (event_code == VIEW_CONFIRM) {
    image_view_zoom_exit(C, op, false);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_RUNNING_MODAL;
}

static void image_view_zoom_cancel(bContext *C, wmOperator *op)
{
  image_view_zoom_exit(C, op, true);
}

void IMAGE_OT_view_zoom(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Zoom View";
  ot->idname = "IMAGE_OT_view_zoom";
  ot->description = "Zoom in/out the image";

  /* api callbacks */
  ot->exec = image_view_zoom_exec;
  ot->invoke = image_view_zoom_invoke;
  ot->modal = image_view_zoom_modal;
  ot->cancel = image_view_zoom_cancel;
  ot->poll = space_image_main_region_poll;

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

#ifdef WITH_INPUT_NDOF

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Operator
 * \{ */

/* Combined pan/zoom from a 3D mouse device.
 * Z zooms, XY pans
 * "view" (not "paper") control -- user moves the viewpoint, not the image being viewed
 * that explains the negative signs in the code below
 */

static int image_view_ndof_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }
  else {
    SpaceImage *sima = CTX_wm_space_image(C);
    ARegion *region = CTX_wm_region(C);
    float pan_vec[3];

    const wmNDOFMotionData *ndof = event->customdata;
    const float speed = NDOF_PIXELS_PER_SECOND;

    WM_event_ndof_pan_get(ndof, pan_vec, true);

    mul_v2_fl(pan_vec, (speed * ndof->dt) / sima->zoom);
    pan_vec[2] *= -ndof->dt;

    sima_zoom_set_factor(sima, region, 1.0f + pan_vec[2], NULL, false);
    sima->xof += pan_vec[0];
    sima->yof += pan_vec[1];

    ED_region_tag_redraw(region);

    return OPERATOR_FINISHED;
  }
}

void IMAGE_OT_view_ndof(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "NDOF Pan/Zoom";
  ot->idname = "IMAGE_OT_view_ndof";
  ot->description = "Use a 3D mouse device to pan/zoom the view";

  /* api callbacks */
  ot->invoke = image_view_ndof_invoke;
  ot->poll = space_image_main_region_poll;

  /* flags */
  ot->flag = OPTYPE_LOCK_BYPASS;
}

/** \} */

#endif /* WITH_INPUT_NDOF */

/* -------------------------------------------------------------------- */
/** \name View All Operator
 * \{ */

/* Updates the fields of the View2D member of the SpaceImage struct.
 * Default behavior is to reset the position of the image and set the zoom to 1
 * If the image will not fit within the window rectangle, the zoom is adjusted */

static int image_view_all_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima;
  ARegion *region;
  float aspx, aspy, zoomx, zoomy, w, h;
  int width, height;
  const bool fit_view = RNA_boolean_get(op->ptr, "fit_view");

  /* retrieve state */
  sima = CTX_wm_space_image(C);
  region = CTX_wm_region(C);

  ED_space_image_get_size(sima, &width, &height);
  ED_space_image_get_aspect(sima, &aspx, &aspy);

  w = width * aspx;
  h = height * aspy;

  float xof = 0.0f, yof = 0.0f;
  if ((sima->image == NULL) || (sima->image->source == IMA_SRC_TILED)) {
    /* Extend the shown area to cover all UDIM tiles. */
    int x_tiles, y_tiles;
    if (sima->image == NULL) {
      x_tiles = sima->tile_grid_shape[0];
      y_tiles = sima->tile_grid_shape[1];
    }
    else {
      x_tiles = y_tiles = 1;
      LISTBASE_FOREACH (ImageTile *, tile, &sima->image->tiles) {
        int tile_x = (tile->tile_number - 1001) % 10;
        int tile_y = (tile->tile_number - 1001) / 10;
        x_tiles = max_ii(x_tiles, tile_x + 1);
        y_tiles = max_ii(y_tiles, tile_y + 1);
      }
    }
    xof = 0.5f * (x_tiles - 1.0f) * w;
    yof = 0.5f * (y_tiles - 1.0f) * h;
    w *= x_tiles;
    h *= y_tiles;
  }

  /* check if the image will fit in the image with (zoom == 1) */
  width = BLI_rcti_size_x(&region->winrct) + 1;
  height = BLI_rcti_size_y(&region->winrct) + 1;

  if (fit_view) {
    const int margin = 5; /* margin from border */

    zoomx = (float)width / (w + 2 * margin);
    zoomy = (float)height / (h + 2 * margin);

    sima_zoom_set(sima, region, min_ff(zoomx, zoomy), NULL, false);
  }
  else {
    if ((w >= width || h >= height) && (width > 0 && height > 0)) {
      zoomx = (float)width / w;
      zoomy = (float)height / h;

      /* find the zoom value that will fit the image in the image space */
      sima_zoom_set(sima, region, 1.0f / power_of_2(1.0f / min_ff(zoomx, zoomy)), NULL, false);
    }
    else {
      sima_zoom_set(sima, region, 1.0f, NULL, false);
    }
  }

  sima->xof = xof;
  sima->yof = yof;

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_view_all(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Frame All";
  ot->idname = "IMAGE_OT_view_all";
  ot->description = "View the entire image";

  /* api callbacks */
  ot->exec = image_view_all_exec;
  ot->poll = space_image_main_region_poll;

  /* flags */
  ot->flag = OPTYPE_LOCK_BYPASS;

  /* properties */
  prop = RNA_def_boolean(ot->srna, "fit_view", 0, "Fit View", "Fit frame to the viewport");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Center View To Cursor Operator
 * \{ */

static int view_center_cursor_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);

  ED_image_view_center_to_point(sima, sima->cursor[0], sima->cursor[1]);

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_view_center_cursor(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Center View to Cursor";
  ot->description = "Center the view so that the cursor is in the middle of the view";
  ot->idname = "IMAGE_OT_view_center_cursor";

  /* api callbacks */
  ot->exec = view_center_cursor_exec;
  ot->poll = ED_space_image_cursor_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Selected Operator
 * \{ */

static int image_view_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceImage *sima;
  ARegion *region;
  Scene *scene;
  ViewLayer *view_layer;
  Object *obedit;

  /* retrieve state */
  sima = CTX_wm_space_image(C);
  region = CTX_wm_region(C);
  scene = CTX_data_scene(C);
  view_layer = CTX_data_view_layer(C);
  obedit = CTX_data_edit_object(C);

  /* get bounds */
  float min[2], max[2];
  if (ED_space_image_show_uvedit(sima, obedit)) {
    uint objects_len = 0;
    Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
        view_layer, ((View3D *)NULL), &objects_len);
    bool success = ED_uvedit_minmax_multi(scene, objects, objects_len, min, max);
    MEM_freeN(objects);
    if (!success) {
      return OPERATOR_CANCELLED;
    }
  }
  else if (ED_space_image_check_show_maskedit(sima, view_layer)) {
    if (!ED_mask_selected_minmax(C, min, max)) {
      return OPERATOR_CANCELLED;
    }
  }
  rctf bounds = {
      .xmin = min[0],
      .ymin = min[1],
      .xmax = max[0],
      .ymax = max[1],
  };

  /* add some margin */
  BLI_rctf_scale(&bounds, 1.4f);

  sima_zoom_set_from_bounds(sima, region, &bounds);

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

static bool image_view_selected_poll(bContext *C)
{
  return (space_image_main_region_poll(C) && (ED_operator_uvedit(C) || ED_operator_mask(C)));
}

void IMAGE_OT_view_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Center";
  ot->idname = "IMAGE_OT_view_selected";
  ot->description = "View all selected UVs";

  /* api callbacks */
  ot->exec = image_view_selected_exec;
  ot->poll = image_view_selected_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Zoom In/Out Operator
 * \{ */

static int image_view_zoom_in_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);
  float location[2];

  RNA_float_get_array(op->ptr, "location", location);

  sima_zoom_set_factor(
      sima, region, powf(2.0f, 1.0f / 3.0f), location, U.uiflag & USER_ZOOM_TO_MOUSEPOS);

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

static int image_view_zoom_in_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  float location[2];

  UI_view2d_region_to_view(
      &region->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);
  RNA_float_set_array(op->ptr, "location", location);

  return image_view_zoom_in_exec(C, op);
}

void IMAGE_OT_view_zoom_in(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Zoom In";
  ot->idname = "IMAGE_OT_view_zoom_in";
  ot->description = "Zoom in the image (centered around 2D cursor)";

  /* api callbacks */
  ot->invoke = image_view_zoom_in_invoke;
  ot->exec = image_view_zoom_in_exec;
  ot->poll = space_image_main_region_poll;

  /* flags */
  ot->flag = OPTYPE_LOCK_BYPASS;

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

static int image_view_zoom_out_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);
  float location[2];

  RNA_float_get_array(op->ptr, "location", location);

  sima_zoom_set_factor(
      sima, region, powf(0.5f, 1.0f / 3.0f), location, U.uiflag & USER_ZOOM_TO_MOUSEPOS);

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

static int image_view_zoom_out_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  float location[2];

  UI_view2d_region_to_view(
      &region->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);
  RNA_float_set_array(op->ptr, "location", location);

  return image_view_zoom_out_exec(C, op);
}

void IMAGE_OT_view_zoom_out(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Zoom Out";
  ot->idname = "IMAGE_OT_view_zoom_out";
  ot->description = "Zoom out the image (centered around 2D cursor)";

  /* api callbacks */
  ot->invoke = image_view_zoom_out_invoke;
  ot->exec = image_view_zoom_out_exec;
  ot->poll = space_image_main_region_poll;

  /* flags */
  ot->flag = OPTYPE_LOCK_BYPASS;

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Zoom Ratio Operator
 * \{ */

static int image_view_zoom_ratio_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);

  sima_zoom_set(sima, region, RNA_float_get(op->ptr, "ratio"), NULL, false);

  /* ensure pixel exact locations for draw */
  sima->xof = (int)sima->xof;
  sima->yof = (int)sima->yof;

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_view_zoom_ratio(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Zoom Ratio";
  ot->idname = "IMAGE_OT_view_zoom_ratio";
  ot->description = "Set zoom ratio of the view";

  /* api callbacks */
  ot->exec = image_view_zoom_ratio_exec;
  ot->poll = space_image_main_region_poll;

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
/** \name View Border-Zoom Operator
 * \{ */

static int image_view_zoom_border_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);
  rctf bounds;
  const bool zoom_in = !RNA_boolean_get(op->ptr, "zoom_out");

  WM_operator_properties_border_to_rctf(op, &bounds);

  UI_view2d_region_to_view_rctf(&region->v2d, &bounds, &bounds);

  const struct {
    float xof;
    float yof;
    float zoom;
  } sima_view_prev = {
      .xof = sima->xof,
      .yof = sima->yof,
      .zoom = sima->zoom,
  };

  sima_zoom_set_from_bounds(sima, region, &bounds);

  /* zoom out */
  if (!zoom_in) {
    sima->xof = sima_view_prev.xof + (sima->xof - sima_view_prev.xof);
    sima->yof = sima_view_prev.yof + (sima->yof - sima_view_prev.yof);
    sima->zoom = sima_view_prev.zoom * (sima_view_prev.zoom / sima->zoom);
  }

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_view_zoom_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Zoom to Border";
  ot->description = "Zoom in the view to the nearest item contained in the border";
  ot->idname = "IMAGE_OT_view_zoom_border";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = image_view_zoom_border_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = space_image_main_region_poll;

  /* rna */
  WM_operator_properties_gesture_box_zoom(ot);
}

/* load/replace/save callbacks */
static void image_filesel(bContext *C, wmOperator *op, const char *path)
{
  RNA_string_set(op->ptr, "filepath", path);
  WM_event_add_fileselect(C, op);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Open Image Operator
 * \{ */

typedef struct ImageOpenData {
  PropertyPointerRNA pprop;
  ImageUser *iuser;
  ImageFormatData im_format;
} ImageOpenData;

static void image_open_init(bContext *C, wmOperator *op)
{
  ImageOpenData *iod;
  op->customdata = iod = MEM_callocN(sizeof(ImageOpenData), __func__);
  iod->iuser = CTX_data_pointer_get_type(C, "image_user", &RNA_ImageUser).data;
  UI_context_active_but_prop_get_templateID(C, &iod->pprop.ptr, &iod->pprop.prop);
}

static void image_open_cancel(bContext *UNUSED(C), wmOperator *op)
{
  MEM_freeN(op->customdata);
  op->customdata = NULL;
}

static Image *image_open_single(Main *bmain,
                                wmOperator *op,
                                ImageFrameRange *range,
                                const char *relbase,
                                bool is_relative_path,
                                bool use_multiview)
{
  bool exists = false;
  Image *ima = NULL;

  errno = 0;
  ima = BKE_image_load_exists_ex(bmain, range->filepath, &exists);

  if (!ima) {
    if (op->customdata) {
      MEM_freeN(op->customdata);
    }
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Cannot read '%s': %s",
                range->filepath,
                errno ? strerror(errno) : TIP_("unsupported image format"));
    return NULL;
  }

  if (!exists) {
    /* only image path after save, never ibuf */
    if (is_relative_path) {
      BLI_path_rel(ima->filepath, relbase);
    }

    /* handle multiview images */
    if (use_multiview) {
      ImageOpenData *iod = op->customdata;
      ImageFormatData *imf = &iod->im_format;

      ima->flag |= IMA_USE_VIEWS;
      ima->views_format = imf->views_format;
      *ima->stereo3d_format = imf->stereo3d_format;
    }
    else {
      ima->flag &= ~IMA_USE_VIEWS;
      BKE_image_free_views(ima);
    }

    if ((range->length > 1) && (ima->source == IMA_SRC_FILE)) {
      if (range->udim_tiles.first && range->offset == 1001) {
        ima->source = IMA_SRC_TILED;
        LISTBASE_FOREACH (LinkData *, node, &range->udim_tiles) {
          BKE_image_add_tile(ima, POINTER_AS_INT(node->data), NULL);
        }
      }
      else {
        ima->source = IMA_SRC_SEQUENCE;
      }
    }
  }

  return ima;
}

static int image_open_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ScrArea *area = CTX_wm_area(C);
  Scene *scene = CTX_data_scene(C);
  Object *obedit = CTX_data_edit_object(C);
  ImageUser *iuser = NULL;
  ImageOpenData *iod = op->customdata;
  Image *ima = NULL;
  int frame_seq_len = 0;
  int frame_ofs = 1;

  const bool is_relative_path = RNA_boolean_get(op->ptr, "relative_path");
  const bool use_multiview = RNA_boolean_get(op->ptr, "use_multiview");
  const bool use_udim = RNA_boolean_get(op->ptr, "use_udim_detecting");

  if (!op->customdata) {
    image_open_init(C, op);
  }

  ListBase ranges = ED_image_filesel_detect_sequences(bmain, op, use_udim);
  LISTBASE_FOREACH (ImageFrameRange *, range, &ranges) {
    Image *ima_range = image_open_single(
        bmain, op, range, BKE_main_blendfile_path(bmain), is_relative_path, use_multiview);

    /* take the first image */
    if ((ima == NULL) && ima_range) {
      ima = ima_range;
      frame_seq_len = range->length;
      frame_ofs = range->offset;
    }

    BLI_freelistN(&range->udim_tiles);
  }
  BLI_freelistN(&ranges);

  if (ima == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* hook into UI */
  iod = op->customdata;

  if (iod->pprop.prop) {
    /* when creating new ID blocks, use is already 1, but RNA
     * pointer use also increases user, so this compensates it */
    id_us_min(&ima->id);

    PointerRNA imaptr;
    RNA_id_pointer_create(&ima->id, &imaptr);
    RNA_property_pointer_set(&iod->pprop.ptr, iod->pprop.prop, imaptr, NULL);
    RNA_property_update(C, &iod->pprop.ptr, iod->pprop.prop);
  }

  if (iod->iuser) {
    iuser = iod->iuser;
  }
  else if (area && area->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = area->spacedata.first;
    ED_space_image_set(bmain, sima, obedit, ima, false);
    iuser = &sima->iuser;
  }
  else {
    Tex *tex = CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
    if (tex && tex->type == TEX_IMAGE) {
      iuser = &tex->iuser;
    }

    if (iuser == NULL) {
      Camera *cam = CTX_data_pointer_get_type(C, "camera", &RNA_Camera).data;
      if (cam) {
        LISTBASE_FOREACH (CameraBGImage *, bgpic, &cam->bg_images) {
          if (bgpic->ima == ima) {
            iuser = &bgpic->iuser;
            break;
          }
        }
      }
    }
  }

  /* initialize because of new image */
  if (iuser) {
    /* If the sequence was a tiled image, we only have one frame. */
    iuser->frames = (ima->source == IMA_SRC_SEQUENCE) ? frame_seq_len : 1;
    iuser->sfra = 1;
    iuser->framenr = 1;
    if (ima->source == IMA_SRC_MOVIE) {
      iuser->offset = 0;
    }
    else {
      iuser->offset = frame_ofs - 1;
    }
    iuser->scene = scene;
    BKE_image_init_imageuser(ima, iuser);
  }

  /* XXX BKE_packedfile_unpack_image frees image buffers */
  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  BKE_image_signal(bmain, ima, iuser, IMA_SIGNAL_RELOAD);
  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

  MEM_freeN(op->customdata);

  return OPERATOR_FINISHED;
}

static int image_open_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  SpaceImage *sima = CTX_wm_space_image(C); /* XXX other space types can call */
  const char *path = U.textudir;
  Image *ima = NULL;
  Scene *scene = CTX_data_scene(C);

  if (sima) {
    ima = sima->image;
  }

  if (ima == NULL) {
    Tex *tex = CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
    if (tex && tex->type == TEX_IMAGE) {
      ima = tex->ima;
    }
  }

  if (ima == NULL) {
    PointerRNA ptr;
    PropertyRNA *prop;

    /* hook into UI */
    UI_context_active_but_prop_get_templateID(C, &ptr, &prop);

    if (prop) {
      PointerRNA oldptr;
      Image *oldima;

      oldptr = RNA_property_pointer_get(&ptr, prop);
      oldima = (Image *)oldptr.owner_id;
      /* unlikely to fail but better avoid strange crash */
      if (oldima && GS(oldima->id.name) == ID_IM) {
        ima = oldima;
      }
    }
  }

  if (ima) {
    path = ima->filepath;
  }

  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    return image_open_exec(C, op);
  }

  image_open_init(C, op);

  /* show multiview save options only if scene has multiviews */
  PropertyRNA *prop;
  prop = RNA_struct_find_property(op->ptr, "show_multiview");
  RNA_property_boolean_set(op->ptr, prop, (scene->r.scemode & R_MULTIVIEW) != 0);

  image_filesel(C, op, path);

  return OPERATOR_RUNNING_MODAL;
}

static bool image_open_draw_check_prop(PointerRNA *UNUSED(ptr),
                                       PropertyRNA *prop,
                                       void *UNUSED(user_data))
{
  const char *prop_id = RNA_property_identifier(prop);

  return !(STREQ(prop_id, "filepath") || STREQ(prop_id, "directory") ||
           STREQ(prop_id, "filename"));
}

static void image_open_draw(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *layout = op->layout;
  ImageOpenData *iod = op->customdata;
  ImageFormatData *imf = &iod->im_format;
  PointerRNA imf_ptr, ptr;

  /* main draw call */
  RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
  uiDefAutoButsRNA(
      layout, &ptr, image_open_draw_check_prop, NULL, NULL, UI_BUT_LABEL_ALIGN_NONE, false);

  /* image template */
  RNA_pointer_create(NULL, &RNA_ImageFormatSettings, imf, &imf_ptr);

  /* multiview template */
  if (RNA_boolean_get(op->ptr, "show_multiview")) {
    uiTemplateImageFormatViews(layout, &imf_ptr, op->ptr);
  }
}

/* called by other space types too */
void IMAGE_OT_open(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Open Image";
  ot->description = "Open image";
  ot->idname = "IMAGE_OT_open";

  /* api callbacks */
  ot->exec = image_open_exec;
  ot->invoke = image_open_invoke;
  ot->cancel = image_open_cancel;
  ot->ui = image_open_draw;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILES |
                                     WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);

  RNA_def_boolean(
      ot->srna,
      "use_sequence_detection",
      true,
      "Detect Sequences",
      "Automatically detect animated sequences in selected images (based on file names)");
  RNA_def_boolean(ot->srna,
                  "use_udim_detecting",
                  true,
                  "Detect UDIMs",
                  "Detect selected UDIM files and load all matching tiles");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Match Movie Length Operator
 * \{ */

static int image_match_len_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Image *ima = image_from_context(C);
  ImageUser *iuser = image_user_from_context(C);

  if (!ima || !iuser) {
    /* Try to get a Texture, or a SpaceImage from context... */
    Tex *tex = CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
    if (tex && tex->type == TEX_IMAGE) {
      ima = tex->ima;
      iuser = &tex->iuser;
    }
  }

  if (!ima || !iuser || !BKE_image_has_anim(ima)) {
    return OPERATOR_CANCELLED;
  }

  struct anim *anim = ((ImageAnim *)ima->anims.first)->anim;
  if (!anim) {
    return OPERATOR_CANCELLED;
  }
  iuser->frames = IMB_anim_get_duration(anim, IMB_TC_RECORD_RUN);
  BKE_image_user_frame_calc(ima, iuser, scene->r.cfra);

  return OPERATOR_FINISHED;
}

/* called by other space types too */
void IMAGE_OT_match_movie_length(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Match Movie Length";
  ot->description = "Set image's user's length to the one of this video";
  ot->idname = "IMAGE_OT_match_movie_length";

  /* api callbacks */
  ot->exec = image_match_len_exec;

  /* flags */
  /* Don't think we need undo for that. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_INTERNAL /* | OPTYPE_UNDO */;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Replace Image Operator
 * \{ */

static int image_replace_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  char str[FILE_MAX];

  if (!sima->image) {
    return OPERATOR_CANCELLED;
  }

  RNA_string_get(op->ptr, "filepath", str);

  /* we cant do much if the str is longer then FILE_MAX :/ */
  BLI_strncpy(sima->image->filepath, str, sizeof(sima->image->filepath));

  if (sima->image->source == IMA_SRC_GENERATED) {
    sima->image->source = IMA_SRC_FILE;
    BKE_image_signal(bmain, sima->image, &sima->iuser, IMA_SIGNAL_SRC_CHANGE);
  }

  if (BLI_path_extension_check_array(str, imb_ext_movie)) {
    sima->image->source = IMA_SRC_MOVIE;
  }
  else {
    sima->image->source = IMA_SRC_FILE;
  }

  /* XXX BKE_packedfile_unpack_image frees image buffers */
  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  BKE_icon_changed(BKE_icon_id_ensure(&sima->image->id));
  BKE_image_signal(bmain, sima->image, &sima->iuser, IMA_SIGNAL_RELOAD);
  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, sima->image);

  return OPERATOR_FINISHED;
}

static int image_replace_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  SpaceImage *sima = CTX_wm_space_image(C);

  if (!sima->image) {
    return OPERATOR_CANCELLED;
  }

  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    return image_replace_exec(C, op);
  }

  if (!RNA_struct_property_is_set(op->ptr, "relative_path")) {
    RNA_boolean_set(op->ptr, "relative_path", BLI_path_is_rel(sima->image->filepath));
  }

  image_filesel(C, op, sima->image->filepath);

  return OPERATOR_RUNNING_MODAL;
}

void IMAGE_OT_replace(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Replace Image";
  ot->idname = "IMAGE_OT_replace";
  ot->description = "Replace current image by another one from disk";

  /* api callbacks */
  ot->exec = image_replace_exec;
  ot->invoke = image_replace_invoke;
  ot->poll = image_not_packed_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Save Image As Operator
 * \{ */

typedef struct ImageSaveData {
  ImageUser *iuser;
  Image *image;
  ImageFormatData im_format;
} ImageSaveData;

static char imtype_best_depth(ImBuf *ibuf, const char imtype)
{
  const char depth_ok = BKE_imtype_valid_depths(imtype);

  if (ibuf->rect_float) {
    if (depth_ok & R_IMF_CHAN_DEPTH_32) {
      return R_IMF_CHAN_DEPTH_32;
    }
    if (depth_ok & R_IMF_CHAN_DEPTH_24) {
      return R_IMF_CHAN_DEPTH_24;
    }
    if (depth_ok & R_IMF_CHAN_DEPTH_16) {
      return R_IMF_CHAN_DEPTH_16;
    }
    if (depth_ok & R_IMF_CHAN_DEPTH_12) {
      return R_IMF_CHAN_DEPTH_12;
    }
    return R_IMF_CHAN_DEPTH_8;
  }
  else {
    if (depth_ok & R_IMF_CHAN_DEPTH_8) {
      return R_IMF_CHAN_DEPTH_8;
    }
    if (depth_ok & R_IMF_CHAN_DEPTH_12) {
      return R_IMF_CHAN_DEPTH_12;
    }
    if (depth_ok & R_IMF_CHAN_DEPTH_16) {
      return R_IMF_CHAN_DEPTH_16;
    }
    if (depth_ok & R_IMF_CHAN_DEPTH_24) {
      return R_IMF_CHAN_DEPTH_24;
    }
    if (depth_ok & R_IMF_CHAN_DEPTH_32) {
      return R_IMF_CHAN_DEPTH_32;
    }
    return R_IMF_CHAN_DEPTH_8; /* fallback, should not get here */
  }
}

static int image_save_options_init(Main *bmain,
                                   ImageSaveOptions *opts,
                                   Image *ima,
                                   ImageUser *iuser,
                                   const bool guess_path,
                                   const bool save_as_render)
{
  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);

  if (ibuf) {
    Scene *scene = opts->scene;
    bool is_depth_set = false;

    if (ELEM(ima->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE)) {
      /* imtype */
      opts->im_format = scene->r.im_format;
      is_depth_set = true;
      if (!BKE_image_is_multiview(ima)) {
        /* In case multiview is disabled,
         * render settings would be invalid for render result in this area. */
        opts->im_format.stereo3d_format = *ima->stereo3d_format;
        opts->im_format.views_format = ima->views_format;
      }
    }
    else {
      if (ima->source == IMA_SRC_GENERATED) {
        opts->im_format.imtype = R_IMF_IMTYPE_PNG;
        opts->im_format.compress = ibuf->foptions.quality;
        opts->im_format.planes = ibuf->planes;
      }
      else {
        BKE_imbuf_to_image_format(&opts->im_format, ibuf);
      }

      /* use the multiview image settings as the default */
      opts->im_format.stereo3d_format = *ima->stereo3d_format;
      opts->im_format.views_format = ima->views_format;
    }

    BLI_strncpy(opts->filepath, ibuf->name, sizeof(opts->filepath));

    /* sanitize all settings */

    /* unlikely but just in case */
    if (ELEM(opts->im_format.planes, R_IMF_PLANES_BW, R_IMF_PLANES_RGB, R_IMF_PLANES_RGBA) == 0) {
      opts->im_format.planes = R_IMF_PLANES_RGBA;
    }

    /* depth, account for float buffer and format support */
    if (is_depth_set == false) {
      opts->im_format.depth = imtype_best_depth(ibuf, opts->im_format.imtype);
    }

    /* some formats don't use quality so fallback to scenes quality */
    if (opts->im_format.quality == 0) {
      opts->im_format.quality = scene->r.im_format.quality;
    }

    /* check for empty path */
    if (guess_path && opts->filepath[0] == 0) {
      const bool is_prev_save = !STREQ(G.ima, "//");
      if (save_as_render) {
        if (is_prev_save) {
          BLI_strncpy(opts->filepath, G.ima, sizeof(opts->filepath));
        }
        else {
          BLI_strncpy(opts->filepath, "//untitled", sizeof(opts->filepath));
          BLI_path_abs(opts->filepath, BKE_main_blendfile_path(bmain));
        }
      }
      else {
        BLI_snprintf(opts->filepath, sizeof(opts->filepath), "//%s", ima->id.name + 2);
        BLI_path_make_safe(opts->filepath);
        BLI_path_abs(opts->filepath, is_prev_save ? G.ima : BKE_main_blendfile_path(bmain));
      }

      /* append UDIM numbering if not present */
      if (ima->source == IMA_SRC_TILED &&
          (BLI_path_sequence_decode(ima->filepath, NULL, NULL, NULL) != 1001)) {
        int len = strlen(opts->filepath);
        STR_CONCAT(opts->filepath, len, ".1001");
      }
    }

    /* color management */
    BKE_color_managed_display_settings_copy(&opts->im_format.display_settings,
                                            &scene->display_settings);

    BKE_color_managed_view_settings_free(&opts->im_format.view_settings);
    BKE_color_managed_view_settings_copy(&opts->im_format.view_settings, &scene->view_settings);
  }

  BKE_image_release_ibuf(ima, ibuf, lock);

  return (ibuf != NULL);
}

static void image_save_options_from_op(Main *bmain,
                                       ImageSaveOptions *opts,
                                       wmOperator *op,
                                       ImageFormatData *imf)
{
  if (imf) {
    BKE_color_managed_view_settings_free(&opts->im_format.view_settings);
    opts->im_format = *imf;
  }

  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    RNA_string_get(op->ptr, "filepath", opts->filepath);
    BLI_path_abs(opts->filepath, BKE_main_blendfile_path(bmain));
  }
}

static void image_save_options_to_op(ImageSaveOptions *opts, wmOperator *op)
{
  if (op->customdata) {
    ImageSaveData *isd = op->customdata;
    BKE_color_managed_view_settings_free(&isd->im_format.view_settings);
    isd->im_format = opts->im_format;
  }

  RNA_string_set(op->ptr, "filepath", opts->filepath);
}

static bool save_image_op(
    Main *bmain, Image *ima, ImageUser *iuser, wmOperator *op, ImageSaveOptions *opts)
{
  opts->relative = (RNA_struct_find_property(op->ptr, "relative_path") &&
                    RNA_boolean_get(op->ptr, "relative_path"));
  opts->save_copy = (RNA_struct_find_property(op->ptr, "copy") &&
                     RNA_boolean_get(op->ptr, "copy"));
  opts->save_as_render = (RNA_struct_find_property(op->ptr, "save_as_render") &&
                          RNA_boolean_get(op->ptr, "save_as_render"));

  WM_cursor_wait(1);

  bool ok = BKE_image_save(op->reports, bmain, ima, iuser, opts);

  WM_cursor_wait(0);

  /* Remember file path for next save. */
  BLI_strncpy(G.ima, opts->filepath, sizeof(G.ima));

  WM_main_add_notifier(NC_IMAGE | NA_EDITED, ima);

  return ok;
}

static void image_save_as_free(wmOperator *op)
{
  if (op->customdata) {
    ImageSaveData *isd = op->customdata;
    BKE_color_managed_view_settings_free(&isd->im_format.view_settings);

    MEM_freeN(op->customdata);
    op->customdata = NULL;
  }
}

static int image_save_as_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ImageSaveOptions opts;

  Image *image = NULL;
  ImageUser *iuser = NULL;
  ImageFormatData *imf = NULL;
  if (op->customdata) {
    ImageSaveData *isd = op->customdata;
    image = isd->image;
    iuser = isd->iuser;
    imf = &isd->im_format;
  }
  else {
    image = image_from_context(C);
    iuser = image_user_from_context(C);
  }

  BKE_image_save_options_init(&opts, bmain, scene);

  /* just in case to initialize values,
   * these should be set on invoke or by the caller. */
  image_save_options_init(bmain, &opts, image, iuser, false, false);

  image_save_options_from_op(bmain, &opts, op, imf);
  opts.do_newpath = true;

  save_image_op(bmain, image, iuser, op, &opts);

  if (opts.save_copy == false) {
    BKE_image_free_packedfiles(image);
  }

  image_save_as_free(op);

  return OPERATOR_FINISHED;
}

static bool image_save_as_check(bContext *UNUSED(C), wmOperator *op)
{
  ImageSaveData *isd = op->customdata;
  return WM_operator_filesel_ensure_ext_imtype(op, &isd->im_format);
}

static int image_save_as_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Main *bmain = CTX_data_main(C);
  Image *ima = image_from_context(C);
  ImageUser *iuser = image_user_from_context(C);
  Scene *scene = CTX_data_scene(C);
  ImageSaveOptions opts;
  PropertyRNA *prop;
  const bool save_as_render = (ima->source == IMA_SRC_VIEWER);

  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    return image_save_as_exec(C, op);
  }

  BKE_image_save_options_init(&opts, bmain, scene);

  if (image_save_options_init(bmain, &opts, ima, iuser, true, save_as_render) == 0) {
    return OPERATOR_CANCELLED;
  }
  image_save_options_to_op(&opts, op);

  /* enable save_copy by default for render results */
  if (ELEM(ima->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE) &&
      !RNA_struct_property_is_set(op->ptr, "copy")) {
    RNA_boolean_set(op->ptr, "copy", true);
  }

  RNA_boolean_set(op->ptr, "save_as_render", save_as_render);

  ImageSaveData *isd = MEM_callocN(sizeof(*isd), __func__);
  isd->image = ima;
  isd->iuser = iuser;

  memcpy(&isd->im_format, &opts.im_format, sizeof(opts.im_format));
  op->customdata = isd;

  /* show multiview save options only if image has multiviews */
  prop = RNA_struct_find_property(op->ptr, "show_multiview");
  RNA_property_boolean_set(op->ptr, prop, BKE_image_is_multiview(ima));
  prop = RNA_struct_find_property(op->ptr, "use_multiview");
  RNA_property_boolean_set(op->ptr, prop, BKE_image_is_multiview(ima));

  image_filesel(C, op, opts.filepath);

  return OPERATOR_RUNNING_MODAL;
}

static void image_save_as_cancel(bContext *UNUSED(C), wmOperator *op)
{
  image_save_as_free(op);
}

static bool image_save_as_draw_check_prop(PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          void *UNUSED(user_data))
{
  const char *prop_id = RNA_property_identifier(prop);

  return !(STREQ(prop_id, "filepath") || STREQ(prop_id, "directory") ||
           STREQ(prop_id, "filename") ||
           /* when saving a copy, relative path has no effect */
           ((STREQ(prop_id, "relative_path")) && RNA_boolean_get(ptr, "copy")));
}

static void image_save_as_draw(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *layout = op->layout;
  ImageSaveData *isd = op->customdata;
  PointerRNA imf_ptr, ptr;
  const bool is_multiview = RNA_boolean_get(op->ptr, "show_multiview");

  /* image template */
  RNA_pointer_create(NULL, &RNA_ImageFormatSettings, &isd->im_format, &imf_ptr);
  uiTemplateImageSettings(layout, &imf_ptr, false);

  /* main draw call */
  RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
  uiDefAutoButsRNA(
      layout, &ptr, image_save_as_draw_check_prop, NULL, NULL, UI_BUT_LABEL_ALIGN_NONE, false);

  /* multiview template */
  if (is_multiview) {
    uiTemplateImageFormatViews(layout, &imf_ptr, op->ptr);
  }
}

static bool image_save_as_poll(bContext *C)
{
  if (!image_from_context_has_data_poll(C)) {
    return false;
  }

  if (G.is_rendering) {
    /* no need to NULL check here */
    Image *ima = image_from_context(C);

    if (ima->source == IMA_SRC_VIEWER) {
      CTX_wm_operator_poll_msg_set(C, "can't save image while rendering");
      return false;
    }
  }

  return true;
}

void IMAGE_OT_save_as(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Save As Image";
  ot->idname = "IMAGE_OT_save_as";
  ot->description = "Save the image with another name and/or settings";

  /* api callbacks */
  ot->exec = image_save_as_exec;
  ot->check = image_save_as_check;
  ot->invoke = image_save_as_invoke;
  ot->cancel = image_save_as_cancel;
  ot->ui = image_save_as_draw;
  ot->poll = image_save_as_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "save_as_render",
                  0,
                  "Save As Render",
                  "Apply render part of display transform when saving byte image");
  RNA_def_boolean(ot->srna,
                  "copy",
                  0,
                  "Copy",
                  "Create a new image file without modifying the current image in blender");

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE,
                                 FILE_SPECIAL,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Save Image Operator
 * \{ */

static bool image_file_path_saveable(bContext *C, Image *ima, ImageUser *iuser)
{
  /* Can always repack images. */
  if (BKE_image_has_packedfile(ima)) {
    return true;
  }

  /* Test for valid filepath. */
  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);
  bool ret = false;

  if (ibuf) {
    Main *bmain = CTX_data_main(C);
    char name[FILE_MAX];
    BLI_strncpy(name, ibuf->name, FILE_MAX);
    BLI_path_abs(name, BKE_main_blendfile_path(bmain));

    if (BLI_exists(name) == false) {
      CTX_wm_operator_poll_msg_set(C, "image file not found");
    }
    else if (!BLI_file_is_writable(name)) {
      CTX_wm_operator_poll_msg_set(C, "image path can't be written to");
    }
    else if (!BKE_image_buffer_format_writable(ibuf)) {
      CTX_wm_operator_poll_msg_set(C, "image format is read-only");
    }
    else {
      ret = true;
    }
  }

  BKE_image_release_ibuf(ima, ibuf, lock);
  return ret;
}

static bool image_save_poll(bContext *C)
{
  /* Can't save if there are no pixels. */
  if (image_from_context_has_data_poll(C) == false) {
    return false;
  }

  Image *ima = image_from_context(C);
  ImageUser *iuser = image_user_from_context(C);

  /* Images without a filepath will go to save as. */
  if (!BKE_image_has_filepath(ima)) {
    return true;
  }

  /* Check if there is a valid file path and image format we can write. */
  return image_file_path_saveable(C, ima, iuser);
}

static int image_save_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Image *image = image_from_context(C);
  ImageUser *iuser = image_user_from_context(C);
  Scene *scene = CTX_data_scene(C);
  ImageSaveOptions opts;
  bool ok = false;

  if (BKE_image_has_packedfile(image)) {
    /* Save packed files to memory. */
    BKE_image_memorypack(image);
    return OPERATOR_FINISHED;
  }

  BKE_image_save_options_init(&opts, bmain, scene);
  if (image_save_options_init(bmain, &opts, image, iuser, false, false) == 0) {
    return OPERATOR_CANCELLED;
  }
  image_save_options_from_op(bmain, &opts, op, NULL);

  if (BLI_exists(opts.filepath) && BLI_file_is_writable(opts.filepath)) {
    if (save_image_op(bmain, image, iuser, op, &opts)) {
      /* report since this can be called from key-shortcuts */
      BKE_reportf(op->reports, RPT_INFO, "Saved Image '%s'", opts.filepath);
      ok = true;
    }
  }
  else {
    BKE_reportf(
        op->reports, RPT_ERROR, "Cannot save image, path '%s' is not writable", opts.filepath);
  }

  BKE_color_managed_view_settings_free(&opts.im_format.view_settings);

  if (ok) {
    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

static int image_save_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Image *ima = image_from_context(C);

  if (!BKE_image_has_packedfile(ima) && !BKE_image_has_filepath(ima)) {
    WM_operator_name_call(C, "IMAGE_OT_save_as", WM_OP_INVOKE_DEFAULT, NULL);
    return OPERATOR_CANCELLED;
  }
  else {
    return image_save_exec(C, op);
  }
}

void IMAGE_OT_save(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Save Image";
  ot->idname = "IMAGE_OT_save";
  ot->description = "Save the image with current name and settings";

  /* api callbacks */
  ot->exec = image_save_exec;
  ot->invoke = image_save_invoke;
  ot->poll = image_save_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Save Sequence Operator
 * \{ */

static int image_save_sequence_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Image *image = image_from_context(C);
  ImBuf *ibuf, *first_ibuf = NULL;
  int tot = 0;
  char di[FILE_MAX];
  struct MovieCacheIter *iter;

  if (image == NULL) {
    return OPERATOR_CANCELLED;
  }

  if (image->source != IMA_SRC_SEQUENCE) {
    BKE_report(op->reports, RPT_ERROR, "Can only save sequence on image sequences");
    return OPERATOR_CANCELLED;
  }

  if (image->type == IMA_TYPE_MULTILAYER) {
    BKE_report(op->reports, RPT_ERROR, "Cannot save multilayer sequences");
    return OPERATOR_CANCELLED;
  }

  /* get total dirty buffers and first dirty buffer which is used for menu */
  ibuf = NULL;
  if (image->cache != NULL) {
    iter = IMB_moviecacheIter_new(image->cache);
    while (!IMB_moviecacheIter_done(iter)) {
      ibuf = IMB_moviecacheIter_getImBuf(iter);
      if (ibuf->userflags & IB_BITMAPDIRTY) {
        if (first_ibuf == NULL) {
          first_ibuf = ibuf;
        }
        tot++;
      }
      IMB_moviecacheIter_step(iter);
    }
    IMB_moviecacheIter_free(iter);
  }

  if (tot == 0) {
    BKE_report(op->reports, RPT_WARNING, "No images have been changed");
    return OPERATOR_CANCELLED;
  }

  /* get a filename for menu */
  BLI_split_dir_part(first_ibuf->name, di, sizeof(di));
  BKE_reportf(op->reports, RPT_INFO, "%d image(s) will be saved in %s", tot, di);

  iter = IMB_moviecacheIter_new(image->cache);
  while (!IMB_moviecacheIter_done(iter)) {
    ibuf = IMB_moviecacheIter_getImBuf(iter);

    if (ibuf->userflags & IB_BITMAPDIRTY) {
      char name[FILE_MAX];
      BLI_strncpy(name, ibuf->name, sizeof(name));

      BLI_path_abs(name, BKE_main_blendfile_path(bmain));

      if (0 == IMB_saveiff(ibuf, name, IB_rect | IB_zbuf | IB_zbuffloat)) {
        BKE_reportf(op->reports, RPT_ERROR, "Could not write image: %s", strerror(errno));
        break;
      }

      BKE_reportf(op->reports, RPT_INFO, "Saved %s", ibuf->name);
      ibuf->userflags &= ~IB_BITMAPDIRTY;
    }

    IMB_moviecacheIter_step(iter);
  }
  IMB_moviecacheIter_free(iter);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_save_sequence(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Save Sequence";
  ot->idname = "IMAGE_OT_save_sequence";
  ot->description = "Save a sequence of images";

  /* api callbacks */
  ot->exec = image_save_sequence_exec;
  ot->poll = image_from_context_has_data_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Save All Operator
 * \{ */

static bool image_should_be_saved_when_modified(Image *ima)
{
  return !ELEM(ima->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE);
}

static bool image_should_be_saved(Image *ima, bool *is_format_writable)
{
  if (BKE_image_is_dirty_writable(ima, is_format_writable) &&
      ELEM(ima->source, IMA_SRC_FILE, IMA_SRC_GENERATED, IMA_SRC_TILED)) {
    return image_should_be_saved_when_modified(ima);
  }
  else {
    return false;
  }
}

static bool image_has_valid_path(Image *ima)
{
  return strchr(ima->filepath, '\\') || strchr(ima->filepath, '/');
}

bool ED_image_should_save_modified(const Main *bmain)
{
  ReportList reports;
  BKE_reports_init(&reports, RPT_STORE);

  uint modified_images_count = ED_image_save_all_modified_info(bmain, &reports);
  bool should_save = modified_images_count || !BLI_listbase_is_empty(&reports.list);

  BKE_reports_clear(&reports);

  return should_save;
}

int ED_image_save_all_modified_info(const Main *bmain, ReportList *reports)
{
  GSet *unique_paths = BLI_gset_str_new(__func__);

  int num_saveable_images = 0;

  for (Image *ima = bmain->images.first; ima; ima = ima->id.next) {
    bool is_format_writable;

    if (image_should_be_saved(ima, &is_format_writable)) {
      if (BKE_image_has_packedfile(ima) || (ima->source == IMA_SRC_GENERATED)) {
        if (ima->id.lib == NULL) {
          num_saveable_images++;
        }
        else {
          BKE_reportf(reports,
                      RPT_WARNING,
                      "Packed library image can't be saved: \"%s\" from \"%s\"",
                      ima->id.name + 2,
                      ima->id.lib->filepath);
        }
      }
      else if (!is_format_writable) {
        BKE_reportf(reports,
                    RPT_WARNING,
                    "Image can't be saved, use a different file format: \"%s\"",
                    ima->id.name + 2);
      }
      else {
        if (image_has_valid_path(ima)) {
          num_saveable_images++;
          if (BLI_gset_haskey(unique_paths, ima->filepath)) {
            BKE_reportf(reports,
                        RPT_WARNING,
                        "Multiple images can't be saved to an identical path: \"%s\"",
                        ima->filepath);
          }
          else {
            BLI_gset_insert(unique_paths, BLI_strdup(ima->filepath));
          }
        }
        else {
          BKE_reportf(reports,
                      RPT_WARNING,
                      "Image can't be saved, no valid file path: \"%s\"",
                      ima->filepath);
        }
      }
    }
  }

  BLI_gset_free(unique_paths, MEM_freeN);
  return num_saveable_images;
}

bool ED_image_save_all_modified(const bContext *C, ReportList *reports)
{
  Main *bmain = CTX_data_main(C);

  ED_image_save_all_modified_info(bmain, reports);

  bool ok = true;

  for (Image *ima = bmain->images.first; ima; ima = ima->id.next) {
    bool is_format_writable;

    if (image_should_be_saved(ima, &is_format_writable)) {
      if (BKE_image_has_packedfile(ima) || (ima->source == IMA_SRC_GENERATED)) {
        BKE_image_memorypack(ima);
      }
      else if (is_format_writable) {
        if (image_has_valid_path(ima)) {
          ImageSaveOptions opts;
          Scene *scene = CTX_data_scene(C);
          BKE_image_save_options_init(&opts, bmain, scene);
          if (image_save_options_init(bmain, &opts, ima, NULL, false, false)) {
            bool saved_successfully = BKE_image_save(reports, bmain, ima, NULL, &opts);
            ok = ok && saved_successfully;
          }
        }
      }
    }
  }
  return ok;
}

static bool image_save_all_modified_poll(bContext *C)
{
  int num_files = ED_image_save_all_modified_info(CTX_data_main(C), NULL);
  return num_files > 0;
}

static int image_save_all_modified_exec(bContext *C, wmOperator *op)
{
  ED_image_save_all_modified(C, op->reports);
  return OPERATOR_FINISHED;
}

void IMAGE_OT_save_all_modified(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Save All Modified";
  ot->idname = "IMAGE_OT_save_all_modified";
  ot->description = "Save all modified images";

  /* api callbacks */
  ot->exec = image_save_all_modified_exec;
  ot->poll = image_save_all_modified_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reload Image Operator
 * \{ */

static int image_reload_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Image *ima = image_from_context(C);
  ImageUser *iuser = image_user_from_context(C);

  if (!ima) {
    return OPERATOR_CANCELLED;
  }

  /* XXX BKE_packedfile_unpack_image frees image buffers */
  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  BKE_image_signal(bmain, ima, iuser, IMA_SIGNAL_RELOAD);
  DEG_id_tag_update(&ima->id, 0);

  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_reload(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reload Image";
  ot->idname = "IMAGE_OT_reload";
  ot->description = "Reload current image from disk";

  /* api callbacks */
  ot->exec = image_reload_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER; /* no undo, image buffer is not handled by undo */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Image Operator
 * \{ */

#define IMA_DEF_NAME N_("Untitled")

enum {
  GEN_CONTEXT_NONE = 0,
  GEN_CONTEXT_PAINT_CANVAS = 1,
  GEN_CONTEXT_PAINT_STENCIL = 2,
};

typedef struct ImageNewData {
  PropertyPointerRNA pprop;
} ImageNewData;

static ImageNewData *image_new_init(bContext *C, wmOperator *op)
{
  if (op->customdata) {
    return op->customdata;
  }

  ImageNewData *data = MEM_callocN(sizeof(ImageNewData), __func__);
  UI_context_active_but_prop_get_templateID(C, &data->pprop.ptr, &data->pprop.prop);
  op->customdata = data;
  return data;
}

static void image_new_free(wmOperator *op)
{
  if (op->customdata) {
    MEM_freeN(op->customdata);
    op->customdata = NULL;
  }
}

static int image_new_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima;
  Object *obedit;
  Image *ima;
  Main *bmain;
  PropertyRNA *prop;
  char name_buffer[MAX_ID_NAME - 2];
  const char *name;
  float color[4];
  int width, height, floatbuf, gen_type, alpha;
  int stereo3d;

  /* retrieve state */
  sima = CTX_wm_space_image(C);
  obedit = CTX_data_edit_object(C);
  bmain = CTX_data_main(C);

  prop = RNA_struct_find_property(op->ptr, "name");
  RNA_property_string_get(op->ptr, prop, name_buffer);
  if (!RNA_property_is_set(op->ptr, prop)) {
    /* Default value, we can translate! */
    name = DATA_(name_buffer);
  }
  else {
    name = name_buffer;
  }
  width = RNA_int_get(op->ptr, "width");
  height = RNA_int_get(op->ptr, "height");
  floatbuf = RNA_boolean_get(op->ptr, "float");
  gen_type = RNA_enum_get(op->ptr, "generated_type");
  RNA_float_get_array(op->ptr, "color", color);
  alpha = RNA_boolean_get(op->ptr, "alpha");
  stereo3d = RNA_boolean_get(op->ptr, "use_stereo_3d");
  bool tiled = RNA_boolean_get(op->ptr, "tiled");

  if (!alpha) {
    color[3] = 1.0f;
  }

  ima = BKE_image_add_generated(bmain,
                                width,
                                height,
                                name,
                                alpha ? 32 : 24,
                                floatbuf,
                                gen_type,
                                color,
                                stereo3d,
                                false,
                                tiled);

  if (!ima) {
    image_new_free(op);
    return OPERATOR_CANCELLED;
  }

  /* hook into UI */
  ImageNewData *data = image_new_init(C, op);

  if (data->pprop.prop) {
    /* when creating new ID blocks, use is already 1, but RNA
     * pointer use also increases user, so this compensates it */
    id_us_min(&ima->id);

    PointerRNA imaptr;
    RNA_id_pointer_create(&ima->id, &imaptr);
    RNA_property_pointer_set(&data->pprop.ptr, data->pprop.prop, imaptr, NULL);
    RNA_property_update(C, &data->pprop.ptr, data->pprop.prop);
  }
  else if (sima) {
    ED_space_image_set(bmain, sima, obedit, ima, false);
  }

  BKE_image_signal(bmain, ima, (sima) ? &sima->iuser : NULL, IMA_SIGNAL_USER_NEW_IMAGE);

  WM_event_add_notifier(C, NC_IMAGE | NA_ADDED, ima);

  image_new_free(op);

  return OPERATOR_FINISHED;
}

static int image_new_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  /* Get property in advance, it doesn't work after WM_operator_props_dialog_popup. */
  ImageNewData *data;
  op->customdata = data = MEM_callocN(sizeof(ImageNewData), __func__);
  UI_context_active_but_prop_get_templateID(C, &data->pprop.ptr, &data->pprop.prop);

  /* Better for user feedback. */
  RNA_string_set(op->ptr, "name", DATA_(IMA_DEF_NAME));
  return WM_operator_props_dialog_popup(C, op, 300);
}

static void image_new_draw(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *split, *col[2];
  uiLayout *layout = op->layout;
  PointerRNA ptr;
#if 0
  Scene *scene = CTX_data_scene(C);
  const bool is_multiview = (scene->r.scemode & R_MULTIVIEW) != 0;
#endif

  RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);

  /* copy of WM_operator_props_dialog_popup() layout */

  split = uiLayoutSplit(layout, 0.5f, false);
  col[0] = uiLayoutColumn(split, false);
  col[1] = uiLayoutColumn(split, false);

  uiItemL(col[0], IFACE_("Name"), ICON_NONE);
  uiItemR(col[1], &ptr, "name", 0, "", ICON_NONE);

  uiItemL(col[0], IFACE_("Width"), ICON_NONE);
  uiItemR(col[1], &ptr, "width", 0, "", ICON_NONE);

  uiItemL(col[0], IFACE_("Height"), ICON_NONE);
  uiItemR(col[1], &ptr, "height", 0, "", ICON_NONE);

  uiItemL(col[0], IFACE_("Color"), ICON_NONE);
  uiItemR(col[1], &ptr, "color", 0, "", ICON_NONE);

  uiItemL(col[0], "", ICON_NONE);
  uiItemR(col[1], &ptr, "alpha", 0, NULL, ICON_NONE);

  uiItemL(col[0], IFACE_("Generated Type"), ICON_NONE);
  uiItemR(col[1], &ptr, "generated_type", 0, "", ICON_NONE);

  uiItemL(col[0], "", ICON_NONE);
  uiItemR(col[1], &ptr, "float", 0, NULL, ICON_NONE);

  uiItemL(col[0], "", ICON_NONE);
  uiItemR(col[1], &ptr, "tiled", 0, NULL, ICON_NONE);

#if 0
  if (is_multiview) {
    uiItemL(col[0], "", ICON_NONE);
    uiItemR(col[1], &ptr, "use_stereo_3d", 0, NULL, ICON_NONE);
  }
#endif
}

static void image_new_cancel(bContext *UNUSED(C), wmOperator *op)
{
  image_new_free(op);
}

void IMAGE_OT_new(wmOperatorType *ot)
{
  PropertyRNA *prop;
  static float default_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

  /* identifiers */
  ot->name = "New Image";
  ot->description = "Create a new image";
  ot->idname = "IMAGE_OT_new";

  /* api callbacks */
  ot->exec = image_new_exec;
  ot->invoke = image_new_invoke;
  ot->ui = image_new_draw;
  ot->cancel = image_new_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  RNA_def_string(ot->srna, "name", IMA_DEF_NAME, MAX_ID_NAME - 2, "Name", "Image data-block name");
  prop = RNA_def_int(ot->srna, "width", 1024, 1, INT_MAX, "Width", "Image width", 1, 16384);
  RNA_def_property_subtype(prop, PROP_PIXEL);
  prop = RNA_def_int(ot->srna, "height", 1024, 1, INT_MAX, "Height", "Image height", 1, 16384);
  RNA_def_property_subtype(prop, PROP_PIXEL);
  prop = RNA_def_float_color(
      ot->srna, "color", 4, NULL, 0.0f, FLT_MAX, "Color", "Default fill color", 0.0f, 1.0f);
  RNA_def_property_subtype(prop, PROP_COLOR_GAMMA);
  RNA_def_property_float_array_default(prop, default_color);
  RNA_def_boolean(ot->srna, "alpha", 1, "Alpha", "Create an image with an alpha channel");
  RNA_def_enum(ot->srna,
               "generated_type",
               rna_enum_image_generated_type_items,
               IMA_GENTYPE_BLANK,
               "Generated Type",
               "Fill the image with a grid for UV map testing");
  RNA_def_boolean(
      ot->srna, "float", 0, "32 bit Float", "Create image with 32 bit floating point bit depth");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_boolean(
      ot->srna, "use_stereo_3d", 0, "Stereo 3D", "Create an image with left and right views");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_boolean(ot->srna, "tiled", 0, "Tiled", "Create a tiled image");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

#undef IMA_DEF_NAME

/** \} */

/* -------------------------------------------------------------------- */
/** \name Invert Operators
 * \{ */

static int image_invert_exec(bContext *C, wmOperator *op)
{
  Image *ima = image_from_context(C);
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
  SpaceImage *sima = CTX_wm_space_image(C);
  const bool is_paint = ((sima != NULL) && (sima->mode == SI_MODE_PAINT));

  /* flags indicate if this channel should be inverted */
  const bool r = RNA_boolean_get(op->ptr, "invert_r");
  const bool g = RNA_boolean_get(op->ptr, "invert_g");
  const bool b = RNA_boolean_get(op->ptr, "invert_b");
  const bool a = RNA_boolean_get(op->ptr, "invert_a");

  size_t i;

  if (ibuf == NULL) {
    /* TODO: this should actually never happen, but does for render-results -> cleanup */
    return OPERATOR_CANCELLED;
  }

  ED_image_undo_push_begin_with_image(op->type->name, ima, ibuf, &sima->iuser);

  if (is_paint) {
    ED_imapaint_clear_partial_redraw();
  }

  /* TODO: make this into an IMB_invert_channels(ibuf,r,g,b,a) method!? */
  if (ibuf->rect_float) {

    float *fp = (float *)ibuf->rect_float;
    for (i = ((size_t)ibuf->x) * ibuf->y; i > 0; i--, fp += 4) {
      if (r) {
        fp[0] = 1.0f - fp[0];
      }
      if (g) {
        fp[1] = 1.0f - fp[1];
      }
      if (b) {
        fp[2] = 1.0f - fp[2];
      }
      if (a) {
        fp[3] = 1.0f - fp[3];
      }
    }

    if (ibuf->rect) {
      IMB_rect_from_float(ibuf);
    }
  }
  else if (ibuf->rect) {

    char *cp = (char *)ibuf->rect;
    for (i = ((size_t)ibuf->x) * ibuf->y; i > 0; i--, cp += 4) {
      if (r) {
        cp[0] = 255 - cp[0];
      }
      if (g) {
        cp[1] = 255 - cp[1];
      }
      if (b) {
        cp[2] = 255 - cp[2];
      }
      if (a) {
        cp[3] = 255 - cp[3];
      }
    }
  }
  else {
    BKE_image_release_ibuf(ima, ibuf, NULL);
    return OPERATOR_CANCELLED;
  }

  ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
  BKE_image_mark_dirty(ima, ibuf);

  if (ibuf->mipmap[0]) {
    ibuf->userflags |= IB_MIPMAP_INVALID;
  }

  ED_image_undo_push_end();

  /* force GPU reupload, all image is invalid */
  GPU_free_image(ima);

  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

  BKE_image_release_ibuf(ima, ibuf, NULL);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_invert(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Invert Channels";
  ot->idname = "IMAGE_OT_invert";
  ot->description = "Invert image's channels";

  /* api callbacks */
  ot->exec = image_invert_exec;
  ot->poll = image_from_context_has_data_poll_no_image_user;

  /* properties */
  prop = RNA_def_boolean(ot->srna, "invert_r", 0, "Red", "Invert Red Channel");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "invert_g", 0, "Green", "Invert Green Channel");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "invert_b", 0, "Blue", "Invert Blue Channel");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "invert_a", 0, "Alpha", "Invert Alpha Channel");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scale Operator
 * \{ */

static int image_scale_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Image *ima = image_from_context(C);
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "size");
  if (!RNA_property_is_set(op->ptr, prop)) {
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
    const int size[2] = {ibuf->x, ibuf->y};
    RNA_property_int_set_array(op->ptr, prop, size);
    BKE_image_release_ibuf(ima, ibuf, NULL);
  }
  return WM_operator_props_dialog_popup(C, op, 200);
}

static int image_scale_exec(bContext *C, wmOperator *op)
{
  Image *ima = image_from_context(C);
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
  SpaceImage *sima = CTX_wm_space_image(C);
  const bool is_paint = ((sima != NULL) && (sima->mode == SI_MODE_PAINT));

  if (ibuf == NULL) {
    /* TODO: this should actually never happen, but does for render-results -> cleanup */
    return OPERATOR_CANCELLED;
  }

  if (is_paint) {
    ED_imapaint_clear_partial_redraw();
  }

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "size");
  int size[2];
  if (RNA_property_is_set(op->ptr, prop)) {
    RNA_property_int_get_array(op->ptr, prop, size);
  }
  else {
    size[0] = ibuf->x;
    size[1] = ibuf->y;
    RNA_property_int_set_array(op->ptr, prop, size);
  }

  ED_image_undo_push_begin_with_image(op->type->name, ima, ibuf, &sima->iuser);

  ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
  IMB_scaleImBuf(ibuf, size[0], size[1]);
  BKE_image_release_ibuf(ima, ibuf, NULL);

  ED_image_undo_push_end();

  /* force GPU reupload, all image is invalid */
  GPU_free_image(ima);

  DEG_id_tag_update(&ima->id, 0);
  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_resize(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Resize Image";
  ot->idname = "IMAGE_OT_resize";
  ot->description = "Resize the image";

  /* api callbacks */
  ot->invoke = image_scale_invoke;
  ot->exec = image_scale_exec;
  ot->poll = image_from_context_has_data_poll_no_image_user;

  /* properties */
  RNA_def_int_vector(ot->srna, "size", 2, NULL, 1, INT_MAX, "Size", "", 1, SHRT_MAX);

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pack Operator
 * \{ */

static bool image_pack_test(bContext *C, wmOperator *op)
{
  Image *ima = image_from_context(C);

  if (!ima) {
    return 0;
  }

  if (ELEM(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE, IMA_SRC_TILED)) {
    BKE_report(
        op->reports, RPT_ERROR, "Packing movies, image sequences or tiled images not supported");
    return 0;
  }

  return 1;
}

static int image_pack_exec(bContext *C, wmOperator *op)
{
  struct Main *bmain = CTX_data_main(C);
  Image *ima = image_from_context(C);

  if (!image_pack_test(C, op)) {
    return OPERATOR_CANCELLED;
  }

  if (BKE_image_is_dirty(ima)) {
    BKE_image_memorypack(ima);
  }
  else {
    BKE_image_packfiles(op->reports, ima, ID_BLEND_PATH(bmain, &ima->id));
  }

  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_pack(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pack Image";
  ot->description = "Pack an image as embedded data into the .blend file";
  ot->idname = "IMAGE_OT_pack";

  /* api callbacks */
  ot->exec = image_pack_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unpack Operator
 * \{ */

static int image_unpack_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Image *ima = image_from_context(C);
  int method = RNA_enum_get(op->ptr, "method");

  /* find the supplied image by name */
  if (RNA_struct_property_is_set(op->ptr, "id")) {
    char imaname[MAX_ID_NAME - 2];
    RNA_string_get(op->ptr, "id", imaname);
    ima = BLI_findstring(&bmain->images, imaname, offsetof(ID, name) + 2);
    if (!ima) {
      ima = image_from_context(C);
    }
  }

  if (!ima || !BKE_image_has_packedfile(ima)) {
    return OPERATOR_CANCELLED;
  }

  if (ELEM(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE, IMA_SRC_TILED)) {
    BKE_report(
        op->reports, RPT_ERROR, "Unpacking movies, image sequences or tiled images not supported");
    return OPERATOR_CANCELLED;
  }

  if (G.fileflags & G_FILE_AUTOPACK) {
    BKE_report(op->reports,
               RPT_WARNING,
               "AutoPack is enabled, so image will be packed again on file save");
  }

  /* XXX BKE_packedfile_unpack_image frees image buffers */
  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  BKE_packedfile_unpack_image(CTX_data_main(C), op->reports, ima, method);

  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

  return OPERATOR_FINISHED;
}

static int image_unpack_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Image *ima = image_from_context(C);

  if (RNA_struct_property_is_set(op->ptr, "id")) {
    return image_unpack_exec(C, op);
  }

  if (!ima || !BKE_image_has_packedfile(ima)) {
    return OPERATOR_CANCELLED;
  }

  if (ELEM(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE, IMA_SRC_TILED)) {
    BKE_report(
        op->reports, RPT_ERROR, "Unpacking movies, image sequences or tiled images not supported");
    return OPERATOR_CANCELLED;
  }

  if (G.fileflags & G_FILE_AUTOPACK) {
    BKE_report(op->reports,
               RPT_WARNING,
               "AutoPack is enabled, so image will be packed again on file save");
  }

  unpack_menu(C,
              "IMAGE_OT_unpack",
              ima->id.name + 2,
              ima->filepath,
              "textures",
              BKE_image_has_packedfile(ima) ?
                  ((ImagePackedFile *)ima->packedfiles.first)->packedfile :
                  NULL);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_unpack(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unpack Image";
  ot->description = "Save an image packed in the .blend file to disk";
  ot->idname = "IMAGE_OT_unpack";

  /* api callbacks */
  ot->exec = image_unpack_exec;
  ot->invoke = image_unpack_invoke;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(
      ot->srna, "method", rna_enum_unpack_method_items, PF_USE_LOCAL, "Method", "How to unpack");
  /* XXX, weak!, will fail with library, name collisions */
  RNA_def_string(
      ot->srna, "id", NULL, MAX_ID_NAME - 2, "Image Name", "Image data-block name to unpack");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sample Image Operator
 * \{ */

/* Returns color in linear space, matching ED_space_node_color_sample(). */
bool ED_space_image_color_sample(SpaceImage *sima, ARegion *region, int mval[2], float r_col[3])
{
  if (sima->image == NULL) {
    return false;
  }
  float uv[2];
  UI_view2d_region_to_view(&region->v2d, mval[0], mval[1], &uv[0], &uv[1]);
  int tile = BKE_image_get_tile_from_pos(sima->image, uv, uv, NULL);

  void *lock;
  ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock, tile);
  bool ret = false;

  if (ibuf == NULL) {
    ED_space_image_release_buffer(sima, ibuf, lock);
    return false;
  }

  if (uv[0] >= 0.0f && uv[1] >= 0.0f && uv[0] < 1.0f && uv[1] < 1.0f) {
    const float *fp;
    uchar *cp;
    int x = (int)(uv[0] * ibuf->x), y = (int)(uv[1] * ibuf->y);

    CLAMP(x, 0, ibuf->x - 1);
    CLAMP(y, 0, ibuf->y - 1);

    if (ibuf->rect_float) {
      fp = (ibuf->rect_float + (ibuf->channels) * (y * ibuf->x + x));
      copy_v3_v3(r_col, fp);
      ret = true;
    }
    else if (ibuf->rect) {
      cp = (uchar *)(ibuf->rect + y * ibuf->x + x);
      rgb_uchar_to_float(r_col, cp);
      IMB_colormanagement_colorspace_to_scene_linear_v3(r_col, ibuf->rect_colorspace);
      ret = true;
    }
  }

  ED_space_image_release_buffer(sima, ibuf, lock);
  return ret;
}

void IMAGE_OT_sample(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sample Color";
  ot->idname = "IMAGE_OT_sample";
  ot->description = "Use mouse to sample a color in current image";

  /* api callbacks */
  ot->invoke = ED_imbuf_sample_invoke;
  ot->modal = ED_imbuf_sample_modal;
  ot->cancel = ED_imbuf_sample_cancel;
  ot->poll = ED_imbuf_sample_poll;

  /* flags */
  ot->flag = OPTYPE_BLOCKING;

  PropertyRNA *prop;
  prop = RNA_def_int(ot->srna, "size", 1, 1, 128, "Sample Size", "", 1, 64);
  RNA_def_property_subtype(prop, PROP_PIXEL);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sample Line Operator
 * \{ */

static int image_sample_line_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  Image *ima = ED_space_image(sima);

  int x_start = RNA_int_get(op->ptr, "xstart");
  int y_start = RNA_int_get(op->ptr, "ystart");
  int x_end = RNA_int_get(op->ptr, "xend");
  int y_end = RNA_int_get(op->ptr, "yend");

  float uv1[2], uv2[2], ofs[2];
  UI_view2d_region_to_view(&region->v2d, x_start, y_start, &uv1[0], &uv1[1]);
  UI_view2d_region_to_view(&region->v2d, x_end, y_end, &uv2[0], &uv2[1]);

  /* If the image has tiles, shift the positions accordingly. */
  int tile = BKE_image_get_tile_from_pos(ima, uv1, uv1, ofs);
  sub_v2_v2(uv2, ofs);

  void *lock;
  ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock, tile);
  Histogram *hist = &sima->sample_line_hist;

  if (ibuf == NULL) {
    ED_space_image_release_buffer(sima, ibuf, lock);
    return OPERATOR_CANCELLED;
  }
  /* hmmmm */
  if (ibuf->channels < 3) {
    ED_space_image_release_buffer(sima, ibuf, lock);
    return OPERATOR_CANCELLED;
  }

  copy_v2_v2(hist->co[0], uv1);
  copy_v2_v2(hist->co[1], uv2);

  /* enable line drawing */
  hist->flag |= HISTO_FLAG_SAMPLELINE;

  BKE_histogram_update_sample_line(hist, ibuf, &scene->view_settings, &scene->display_settings);

  /* reset y zoom */
  hist->ymax = 1.0f;

  ED_space_image_release_buffer(sima, ibuf, lock);

  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

static int image_sample_line_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceImage *sima = CTX_wm_space_image(C);

  Histogram *hist = &sima->sample_line_hist;
  hist->flag &= ~HISTO_FLAG_SAMPLELINE;

  if (!ED_space_image_has_buffer(sima)) {
    return OPERATOR_CANCELLED;
  }

  return WM_gesture_straightline_invoke(C, op, event);
}

void IMAGE_OT_sample_line(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sample Line";
  ot->idname = "IMAGE_OT_sample_line";
  ot->description = "Sample a line and show it in Scope panels";

  /* api callbacks */
  ot->invoke = image_sample_line_invoke;
  ot->modal = WM_gesture_straightline_modal;
  ot->exec = image_sample_line_exec;
  ot->poll = space_image_main_region_poll;
  ot->cancel = WM_gesture_straightline_cancel;

  /* flags */
  ot->flag = 0; /* no undo/register since this operates on the space */

  WM_operator_properties_gesture_straightline(ot, WM_CURSOR_EDIT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Curve Point Operator
 * \{ */

void IMAGE_OT_curves_point_set(wmOperatorType *ot)
{
  static const EnumPropertyItem point_items[] = {
      {0, "BLACK_POINT", 0, "Black Point", ""},
      {1, "WHITE_POINT", 0, "White Point", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Set Curves Point";
  ot->idname = "IMAGE_OT_curves_point_set";
  ot->description = "Set black point or white point for curves";

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->invoke = ED_imbuf_sample_invoke;
  ot->modal = ED_imbuf_sample_modal;
  ot->cancel = ED_imbuf_sample_cancel;
  ot->poll = space_image_main_area_not_uv_brush_poll;

  /* properties */
  RNA_def_enum(
      ot->srna, "point", point_items, 0, "Point", "Set black point or white point for curves");

  PropertyRNA *prop;
  prop = RNA_def_int(ot->srna, "size", 1, 1, 128, "Sample Size", "", 1, 64);
  RNA_def_property_subtype(prop, PROP_PIXEL);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cycle Render Slot Operator
 * \{ */

static bool image_cycle_render_slot_poll(bContext *C)
{
  Image *ima = image_from_context(C);

  return (ima && ima->type == IMA_TYPE_R_RESULT);
}

static int image_cycle_render_slot_exec(bContext *C, wmOperator *op)
{
  Image *ima = image_from_context(C);
  const int direction = RNA_boolean_get(op->ptr, "reverse") ? -1 : 1;

  if (!ED_image_slot_cycle(ima, direction)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);

  /* no undo push for browsing existing */
  RenderSlot *slot = BKE_image_get_renderslot(ima, ima->render_slot);
  if ((slot && slot->render) || ima->render_slot == ima->last_render_slot) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void IMAGE_OT_cycle_render_slot(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cycle Render Slot";
  ot->idname = "IMAGE_OT_cycle_render_slot";
  ot->description = "Cycle through all non-void render slots";

  /* api callbacks */
  ot->exec = image_cycle_render_slot_exec;
  ot->poll = image_cycle_render_slot_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;

  RNA_def_boolean(ot->srna, "reverse", 0, "Cycle in Reverse", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Render Slot Operator
 * \{ */

static int image_clear_render_slot_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Image *ima = image_from_context(C);

  if (!BKE_image_clear_renderslot(ima, &sima->iuser, ima->render_slot)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_clear_render_slot(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Render Slot";
  ot->idname = "IMAGE_OT_clear_render_slot";
  ot->description = "Clear the currently selected render slot";

  /* api callbacks */
  ot->exec = image_clear_render_slot_exec;
  ot->poll = image_cycle_render_slot_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Render Slot Operator
 * \{ */

static int image_add_render_slot_exec(bContext *C, wmOperator *UNUSED(op))
{
  Image *ima = image_from_context(C);

  RenderSlot *slot = BKE_image_add_renderslot(ima, NULL);
  ima->render_slot = BLI_findindex(&ima->renderslots, slot);

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_add_render_slot(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Render Slot";
  ot->idname = "IMAGE_OT_add_render_slot";
  ot->description = "Add a new render slot";

  /* api callbacks */
  ot->exec = image_add_render_slot_exec;
  ot->poll = image_cycle_render_slot_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Render Slot Operator
 * \{ */

static int image_remove_render_slot_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Image *ima = image_from_context(C);

  if (!BKE_image_remove_renderslot(ima, &sima->iuser, ima->render_slot)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_remove_render_slot(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Render Slot";
  ot->idname = "IMAGE_OT_remove_render_slot";
  ot->description = "Remove the current render slot";

  /* api callbacks */
  ot->exec = image_remove_render_slot_exec;
  ot->poll = image_cycle_render_slot_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Change Frame Operator
 * \{ */

static bool change_frame_poll(bContext *C)
{
  /* prevent changes during render */
  if (G.is_rendering) {
    return 0;
  }

  return space_image_main_region_poll(C);
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
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  int framenr = 0;

  if (region->regiontype == RGN_TYPE_WINDOW) {
    float sfra = SFRA, efra = EFRA, framelen = region->winx / (efra - sfra + 1);

    framenr = sfra + event->mval[0] / framelen;
  }
  else {
    float viewx, viewy;

    UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &viewx, &viewy);

    framenr = round_fl_to_int(viewx);
  }

  return framenr;
}

static int change_frame_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);

  if (region->regiontype == RGN_TYPE_WINDOW) {
    SpaceImage *sima = CTX_wm_space_image(C);

    /* Local coordinate visible rect inside region, to accommodate overlapping ui. */
    const rcti *rect_visible = ED_region_visible_rect(region);
    const int region_bottom = rect_visible->ymin;

    if (event->mval[1] > (region_bottom + 16 * UI_DPI_FAC) || !ED_space_image_show_cache(sima)) {
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
  }

  return OPERATOR_RUNNING_MODAL;
}

void IMAGE_OT_change_frame(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Change Frame";
  ot->idname = "IMAGE_OT_change_frame";
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

/* Reload cached render results... */
/* goes over all scenes, reads render layers */
static int image_read_viewlayers_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  Image *ima;

  ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_R_RESULT, "Render Result");
  if (sima->image == NULL) {
    ED_space_image_set(bmain, sima, NULL, ima, false);
  }

  RE_ReadRenderResult(scene, scene);

  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);
  return OPERATOR_FINISHED;
}

void IMAGE_OT_read_viewlayers(wmOperatorType *ot)
{
  ot->name = "Open Cached Render";
  ot->idname = "IMAGE_OT_read_viewlayers";
  ot->description = "Read all the current scene's view layers from cache, as needed";

  ot->poll = space_image_main_region_poll;
  ot->exec = image_read_viewlayers_exec;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Border Operator
 * \{ */

static int render_border_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  Render *re = RE_GetSceneRender(scene);
  RenderData *rd;
  rctf border;

  if (re == NULL) {
    /* Shouldn't happen, but better be safe close to the release. */
    return OPERATOR_CANCELLED;
  }

  rd = RE_engine_get_render_data(re);
  if ((rd->mode & (R_BORDER | R_CROP)) == (R_BORDER | R_CROP)) {
    BKE_report(op->reports, RPT_INFO, "Can not set border from a cropped render");
    return OPERATOR_CANCELLED;
  }

  /* get rectangle from operator */
  WM_operator_properties_border_to_rctf(op, &border);
  UI_view2d_region_to_view_rctf(&region->v2d, &border, &border);

  /* actually set border */
  CLAMP(border.xmin, 0.0f, 1.0f);
  CLAMP(border.ymin, 0.0f, 1.0f);
  CLAMP(border.xmax, 0.0f, 1.0f);
  CLAMP(border.ymax, 0.0f, 1.0f);
  scene->r.border = border;

  /* drawing a border surrounding the entire camera view switches off border rendering
   * or the border covers no pixels */
  if ((border.xmin <= 0.0f && border.xmax >= 1.0f && border.ymin <= 0.0f && border.ymax >= 1.0f) ||
      (border.xmin == border.xmax || border.ymin == border.ymax)) {
    scene->r.mode &= ~R_BORDER;
  }
  else {
    scene->r.mode |= R_BORDER;
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_render_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Render Region";
  ot->description = "Set the boundaries of the render region and enable render region";
  ot->idname = "IMAGE_OT_render_border";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = render_border_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;
  ot->poll = image_cycle_render_slot_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna */
  WM_operator_properties_border(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Render Border Operator
 * \{ */

static int clear_render_border_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  scene->r.mode &= ~R_BORDER;
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  BLI_rctf_init(&scene->r.border, 0.0f, 1.0f, 0.0f, 1.0f);
  return OPERATOR_FINISHED;
}

void IMAGE_OT_clear_render_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Render Region";
  ot->description = "Clear the boundaries of the render region and disable render region";
  ot->idname = "IMAGE_OT_clear_render_border";

  /* api callbacks */
  ot->exec = clear_render_border_exec;
  ot->poll = image_cycle_render_slot_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Tile Operator
 * \{ */

static bool do_fill_tile(PointerRNA *ptr, Image *ima, ImageTile *tile)
{
  float color[4];
  RNA_float_get_array(ptr, "color", color);
  int gen_type = RNA_enum_get(ptr, "generated_type");
  int width = RNA_int_get(ptr, "width");
  int height = RNA_int_get(ptr, "height");
  bool is_float = RNA_boolean_get(ptr, "float");
  int planes = RNA_boolean_get(ptr, "alpha") ? 32 : 24;

  return BKE_image_fill_tile(ima, tile, width, height, color, gen_type, planes, is_float);
}

static void draw_fill_tile(PointerRNA *ptr, uiLayout *layout)
{
  uiLayout *split, *col[2];

  split = uiLayoutSplit(layout, 0.5f, false);
  col[0] = uiLayoutColumn(split, false);
  col[1] = uiLayoutColumn(split, false);

  uiItemL(col[0], IFACE_("Color"), ICON_NONE);
  uiItemR(col[1], ptr, "color", 0, "", ICON_NONE);

  uiItemL(col[0], IFACE_("Width"), ICON_NONE);
  uiItemR(col[1], ptr, "width", 0, "", ICON_NONE);

  uiItemL(col[0], IFACE_("Height"), ICON_NONE);
  uiItemR(col[1], ptr, "height", 0, "", ICON_NONE);

  uiItemL(col[0], "", ICON_NONE);
  uiItemR(col[1], ptr, "alpha", 0, NULL, ICON_NONE);

  uiItemL(col[0], IFACE_("Generated Type"), ICON_NONE);
  uiItemR(col[1], ptr, "generated_type", 0, "", ICON_NONE);

  uiItemL(col[0], "", ICON_NONE);
  uiItemR(col[1], ptr, "float", 0, NULL, ICON_NONE);
}

static void initialize_fill_tile(PointerRNA *ptr, Image *ima, ImageTile *tile)
{
  ImageUser iuser;
  BKE_imageuser_default(&iuser);
  if (tile != NULL) {
    iuser.tile = tile->tile_number;
  }

  /* Acquire ibuf to get the default values.
   * If the specified tile has no ibuf, try acquiring the main tile instead
   * (unless the specified tile already was the main tile).*/
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, NULL);
  if (ibuf == NULL && (tile != NULL) && (tile->tile_number != 1001)) {
    ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
  }

  if (ibuf != NULL) {
    /* Initialize properties from reference tile. */
    RNA_int_set(ptr, "width", ibuf->x);
    RNA_int_set(ptr, "height", ibuf->y);
    RNA_boolean_set(ptr, "float", ibuf->rect_float != NULL);
    RNA_boolean_set(ptr, "alpha", ibuf->planes > 24);

    BKE_image_release_ibuf(ima, ibuf, NULL);
  }
}

static void def_fill_tile(StructOrFunctionRNA *srna)
{
  PropertyRNA *prop;
  static float default_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  prop = RNA_def_float_color(
      srna, "color", 4, NULL, 0.0f, FLT_MAX, "Color", "Default fill color", 0.0f, 1.0f);
  RNA_def_property_subtype(prop, PROP_COLOR_GAMMA);
  RNA_def_property_float_array_default(prop, default_color);
  RNA_def_enum(srna,
               "generated_type",
               rna_enum_image_generated_type_items,
               IMA_GENTYPE_BLANK,
               "Generated Type",
               "Fill the image with a grid for UV map testing");
  prop = RNA_def_int(srna, "width", 1024, 1, INT_MAX, "Width", "Image width", 1, 16384);
  RNA_def_property_subtype(prop, PROP_PIXEL);
  prop = RNA_def_int(srna, "height", 1024, 1, INT_MAX, "Height", "Image height", 1, 16384);
  RNA_def_property_subtype(prop, PROP_PIXEL);

  /* Only needed when filling the first tile. */
  RNA_def_boolean(
      srna, "float", 0, "32 bit Float", "Create image with 32 bit floating point bit depth");
  RNA_def_boolean(srna, "alpha", 1, "Alpha", "Create an image with an alpha channel");
}

static bool tile_add_poll(bContext *C)
{
  Image *ima = CTX_data_edit_image(C);

  return (ima != NULL && ima->source == IMA_SRC_TILED && BKE_image_has_ibuf(ima, NULL));
}

static int tile_add_exec(bContext *C, wmOperator *op)
{
  Image *ima = CTX_data_edit_image(C);

  int start_tile = RNA_int_get(op->ptr, "number");
  int end_tile = start_tile + RNA_int_get(op->ptr, "count");

  if (start_tile < 1001 || end_tile > IMA_UDIM_MAX) {
    BKE_report(op->reports, RPT_ERROR, "Invalid UDIM index range was specified");
    return OPERATOR_CANCELLED;
  }

  bool fill_tile = RNA_boolean_get(op->ptr, "fill");
  char *label = RNA_string_get_alloc(op->ptr, "label", NULL, 0);

  bool created_tile = false;
  for (int tile_number = start_tile; tile_number < end_tile; tile_number++) {
    ImageTile *tile = BKE_image_add_tile(ima, tile_number, label);

    if (tile != NULL) {
      ima->active_tile_index = BLI_findindex(&ima->tiles, tile);

      if (fill_tile) {
        do_fill_tile(op->ptr, ima, tile);
      }

      created_tile = true;
    }
  }
  MEM_freeN(label);

  if (!created_tile) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);
  return OPERATOR_FINISHED;
}

static int tile_add_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Image *ima = CTX_data_edit_image(C);

  /* Find the first gap in tile numbers or the number after the last if
   * no gap exists. */
  int next_number = 0;
  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    next_number = tile->tile_number + 1;
    if (tile->next == NULL || tile->next->tile_number > next_number) {
      break;
    }
  }

  ImageTile *tile = BLI_findlink(&ima->tiles, ima->active_tile_index);
  initialize_fill_tile(op->ptr, ima, tile);

  RNA_int_set(op->ptr, "number", next_number);
  RNA_int_set(op->ptr, "count", 1);
  RNA_string_set(op->ptr, "label", "");

  return WM_operator_props_dialog_popup(C, op, 10 * UI_UNIT_X);
}

static void tile_add_draw(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *split, *col[2];
  uiLayout *layout = op->layout;
  PointerRNA ptr;

  RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);

  split = uiLayoutSplit(layout, 0.5f, false);
  col[0] = uiLayoutColumn(split, false);
  col[1] = uiLayoutColumn(split, false);

  uiItemL(col[0], IFACE_("Number"), ICON_NONE);
  uiItemR(col[1], &ptr, "number", 0, "", ICON_NONE);

  uiItemL(col[0], IFACE_("Count"), ICON_NONE);
  uiItemR(col[1], &ptr, "count", 0, "", ICON_NONE);

  uiItemL(col[0], IFACE_("Label"), ICON_NONE);
  uiItemR(col[1], &ptr, "label", 0, "", ICON_NONE);

  uiItemR(layout, &ptr, "fill", 0, NULL, ICON_NONE);

  if (RNA_boolean_get(&ptr, "fill")) {
    draw_fill_tile(&ptr, layout);
  }
}

void IMAGE_OT_tile_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add tile";
  ot->description = "Adds a tile to the image";
  ot->idname = "IMAGE_OT_tile_add";

  /* api callbacks */
  ot->poll = tile_add_poll;
  ot->exec = tile_add_exec;
  ot->invoke = tile_add_invoke;
  ot->ui = tile_add_draw;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "number",
              1002,
              1001,
              IMA_UDIM_MAX,
              "Number",
              "UDIM number of the tile",
              1001,
              1099);
  RNA_def_int(ot->srna, "count", 1, 1, INT_MAX, "Count", "How many tiles to add", 1, 1000);
  RNA_def_string(ot->srna, "label", NULL, 0, "Label", "Optional tile label");
  RNA_def_boolean(ot->srna, "fill", true, "Fill", "Fill new tile with a generated image");
  def_fill_tile(ot->srna);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Tile Operator
 * \{ */

static bool tile_remove_poll(bContext *C)
{
  Image *ima = CTX_data_edit_image(C);

  return (ima != NULL && ima->source == IMA_SRC_TILED && ima->active_tile_index != 0);
}

static int tile_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
  Image *ima = CTX_data_edit_image(C);

  ImageTile *tile = BLI_findlink(&ima->tiles, ima->active_tile_index);
  if (!BKE_image_remove_tile(ima, tile)) {
    return OPERATOR_CANCELLED;
  }

  /* Ensure that the active index is valid. */
  ima->active_tile_index = min_ii(ima->active_tile_index, BLI_listbase_count(&ima->tiles) - 1);

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_tile_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove tile";
  ot->description = "Removes a tile from the image";
  ot->idname = "IMAGE_OT_tile_remove";

  /* api callbacks */
  ot->poll = tile_remove_poll;
  ot->exec = tile_remove_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Fill Tile Operator
 * \{ */

static bool tile_fill_poll(bContext *C)
{
  Image *ima = CTX_data_edit_image(C);

  if (ima != NULL && ima->source == IMA_SRC_TILED) {
    /* Filling secondary tiles is only allowed if the primary tile exists. */
    return (ima->active_tile_index == 0) || BKE_image_has_ibuf(ima, NULL);
  }
  return false;
}

static int tile_fill_exec(bContext *C, wmOperator *op)
{
  Image *ima = CTX_data_edit_image(C);

  ImageTile *tile = BLI_findlink(&ima->tiles, ima->active_tile_index);
  if (!do_fill_tile(op->ptr, ima, tile)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);

  return OPERATOR_FINISHED;
}

static int tile_fill_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  initialize_fill_tile(op->ptr, CTX_data_edit_image(C), NULL);

  return WM_operator_props_dialog_popup(C, op, 15 * UI_UNIT_X);
}

static void tile_fill_draw(bContext *UNUSED(C), wmOperator *op)
{
  PointerRNA ptr;
  RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);

  draw_fill_tile(&ptr, op->layout);
}

void IMAGE_OT_tile_fill(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Fill Tile";
  ot->description = "Fill the current tile with a generated image";
  ot->idname = "IMAGE_OT_tile_fill";

  /* api callbacks */
  ot->poll = tile_fill_poll;
  ot->exec = tile_fill_exec;
  ot->invoke = tile_fill_invoke;
  ot->ui = tile_fill_draw;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  def_fill_tile(ot->srna);
}
