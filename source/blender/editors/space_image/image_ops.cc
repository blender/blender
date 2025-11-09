/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spimage
 */

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_camera_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_icons.hh"
#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BKE_image_save.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_mask.h"
#include "BKE_packedFile.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_moviecache.hh"

#include "MOV_read.hh"

#include "RE_pipeline.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "ED_image.hh"
#include "ED_mask.hh"
#include "ED_paint.hh"
#include "ED_render.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"
#include "ED_util.hh"
#include "ED_util_imbuf.hh"
#include "ED_uvedit.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RE_engine.h"

#include "image_intern.hh"

using blender::Vector;

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

  sima_zoom_set(sima, region, size, nullptr, false);
}

static Image *image_from_context(const bContext *C)
{
  /* Edit image is set by templates used throughout the interface, so image
   * operations work outside the image editor. */
  Image *ima = static_cast<Image *>(CTX_data_pointer_get_type(C, "edit_image", &RNA_Image).data);

  if (ima) {
    return ima;
  }

  /* Image editor. */
  SpaceImage *sima = CTX_wm_space_image(C);
  return (sima) ? sima->image : nullptr;
}

static ImageUser *image_user_from_context(const bContext *C)
{
  /* Edit image user is set by templates used throughout the interface, so
   * image operations work outside the image editor. */
  ImageUser *iuser = static_cast<ImageUser *>(
      CTX_data_pointer_get_type(C, "edit_image_user", &RNA_ImageUser).data);

  if (iuser) {
    return iuser;
  }

  /* Image editor. */
  SpaceImage *sima = CTX_wm_space_image(C);
  return (sima) ? &sima->iuser : nullptr;
}

static ImageUser image_user_from_context_and_active_tile(const bContext *C, Image *ima)
{
  /* Try to get image user from context if available, otherwise use default. */
  ImageUser *iuser_context = image_user_from_context(C);
  ImageUser iuser;
  if (iuser_context) {
    iuser = *iuser_context;
  }
  else {
    BKE_imageuser_default(&iuser);
  }

  /* Use the file associated with the active tile. Otherwise use the first tile. */
  if (ima && ima->source == IMA_SRC_TILED) {
    const ImageTile *active = (ImageTile *)BLI_findlink(&ima->tiles, ima->active_tile_index);
    iuser.tile = active ? active->tile_number : ((ImageTile *)ima->tiles.first)->tile_number;
  }

  return iuser;
}

static bool image_from_context_has_data_poll(bContext *C)
{
  Image *ima = image_from_context(C);
  ImageUser *iuser = image_user_from_context(C);

  if (ima == nullptr) {
    return false;
  }

  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);
  const bool has_buffer = (ibuf && (ibuf->byte_buffer.data || ibuf->float_buffer.data));
  BKE_image_release_ibuf(ima, ibuf, lock);
  return has_buffer;
}

/**
 * Use this when the image buffer is accessing the active tile without the image user.
 */
static bool image_from_context_editable_has_data_poll_active_tile(bContext *C)
{
  Image *ima = image_from_context(C);

  if (ima && !ID_IS_EDITABLE(&ima->id)) {
    CTX_wm_operator_poll_msg_set(C, "Image is not editable");
    return false;
  }

  ImageUser iuser = image_user_from_context_and_active_tile(C, ima);

  return BKE_image_has_ibuf(ima, &iuser);
}

static bool image_not_packed_poll(bContext *C)
{
  /* Do not run 'replace' on packed images, it does not give user expected results at all. */
  Image *ima = image_from_context(C);
  return (ima && BLI_listbase_is_empty(&ima->packedfiles));
}

static void image_view_all(SpaceImage *sima, ARegion *region, wmOperator *op)
{
  float aspx, aspy, zoomx, zoomy, w, h;
  int width, height;
  const bool fit_view = RNA_boolean_get(op->ptr, "fit_view");

  ED_space_image_get_size(sima, &width, &height);
  ED_space_image_get_aspect(sima, &aspx, &aspy);

  w = width * aspx;
  h = height * aspy;

  float xof = 0.0f, yof = 0.0f;
  if ((sima->image == nullptr) || (sima->image->source == IMA_SRC_TILED)) {
    /* Extend the shown area to cover all UDIM tiles. */
    int x_tiles, y_tiles;
    if (sima->image == nullptr) {
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

    zoomx = float(width) / (w + 2 * margin);
    zoomy = float(height) / (h + 2 * margin);

    sima_zoom_set(sima, region, min_ff(zoomx, zoomy), nullptr, false);
  }
  else {
    if ((w >= width || h >= height) && (width > 0 && height > 0)) {
      zoomx = float(width) / w;
      zoomy = float(height) / h;

      /* find the zoom value that will fit the image in the image space */
      sima_zoom_set(sima, region, 1.0f / power_of_2(1.0f / min_ff(zoomx, zoomy)), nullptr, false);
    }
    else {
      sima_zoom_set(sima, region, 1.0f, nullptr, false);
    }
  }

  sima->xof = xof;
  sima->yof = yof;
}

bool space_image_main_region_poll(bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  // ARegion *region = CTX_wm_region(C); /* XXX. */

  if (sima) {
    return true; /* XXX (region && region->runtime->type->regionid == RGN_TYPE_WINDOW); */
  }
  return false;
}

/** For #IMAGE_OT_curves_point_set to avoid sampling when in uv smooth mode or edit-mode. */
static bool space_image_main_area_not_uv_brush_poll(bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);

  if (sima && (CTX_data_edit_object(C) == nullptr)) {
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Pan Operator
 * \{ */

namespace {

struct ViewPanData {
  float x, y;
  float xof, yof;
  int launch_event;
  bool own_cursor;
};

}  // namespace

static void image_view_pan_init(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  ViewPanData *vpd;

  op->customdata = vpd = MEM_callocN<ViewPanData>("ImageViewPanData");

  /* Grab will be set when running from gizmo. */
  vpd->own_cursor = WM_cursor_modal_is_set_ok(win);
  if (vpd->own_cursor) {
    WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
  }

  vpd->x = event->xy[0];
  vpd->y = event->xy[1];
  vpd->xof = sima->xof;
  vpd->yof = sima->yof;
  vpd->launch_event = WM_userdef_event_type_from_keymap_type(event->type);

  WM_event_add_modal_handler(C, op);
}

static void image_view_pan_exit(bContext *C, wmOperator *op, bool cancel)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ViewPanData *vpd = static_cast<ViewPanData *>(op->customdata);

  if (cancel) {
    sima->xof = vpd->xof;
    sima->yof = vpd->yof;
    ED_region_tag_redraw(CTX_wm_region(C));
  }

  if (vpd->own_cursor) {
    WM_cursor_modal_restore(CTX_wm_window(C));
  }
  MEM_freeN(vpd);
}

static wmOperatorStatus image_view_pan_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  float offset[2];

  RNA_float_get_array(op->ptr, "offset", offset);
  sima->xof += offset[0];
  sima->yof += offset[1];

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

static wmOperatorStatus image_view_pan_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type == MOUSEPAN) {
    SpaceImage *sima = CTX_wm_space_image(C);
    float offset[2];

    offset[0] = (event->prev_xy[0] - event->xy[0]) / sima->zoom;
    offset[1] = (event->prev_xy[1] - event->xy[1]) / sima->zoom;
    RNA_float_set_array(op->ptr, "offset", offset);

    image_view_pan_exec(C, op);
    return OPERATOR_FINISHED;
  }

  image_view_pan_init(C, op, event);
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus image_view_pan_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ViewPanData *vpd = static_cast<ViewPanData *>(op->customdata);
  float offset[2];

  switch (event->type) {
    case MOUSEMOVE:
      sima->xof = vpd->xof;
      sima->yof = vpd->yof;
      offset[0] = (vpd->x - event->xy[0]) / sima->zoom;
      offset[1] = (vpd->y - event->xy[1]) / sima->zoom;
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

  /* API callbacks. */
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
};

}  // namespace

static void image_view_zoom_init(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);
  ViewZoomData *vpd;

  op->customdata = vpd = MEM_callocN<ViewZoomData>("ImageViewZoomData");

  /* Grab will be set when running from gizmo. */
  vpd->own_cursor = WM_cursor_modal_is_set_ok(win);
  if (vpd->own_cursor) {
    WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
  }

  vpd->origx = event->xy[0];
  vpd->origy = event->xy[1];
  vpd->zoom = sima->zoom;
  vpd->launch_event = WM_userdef_event_type_from_keymap_type(event->type);

  UI_view2d_region_to_view(
      &region->v2d, event->mval[0], event->mval[1], &vpd->location[0], &vpd->location[1]);

  if (U.viewzoom == USER_ZOOM_CONTINUE) {
    /* needs a timer to continue redrawing */
    vpd->timer = WM_event_timer_add(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);
    vpd->timer_lastdraw = BLI_time_now_seconds();
  }

  vpd->sima = sima;
  vpd->region = region;

  WM_event_add_modal_handler(C, op);
}

static void image_view_zoom_exit(bContext *C, wmOperator *op, bool cancel)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ViewZoomData *vpd = static_cast<ViewZoomData *>(op->customdata);

  if (cancel) {
    sima->zoom = vpd->zoom;
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

static wmOperatorStatus image_view_zoom_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);

  sima_zoom_set_factor(sima, region, RNA_float_get(op->ptr, "factor"), nullptr, false);

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

enum {
  VIEW_PASS = 0,
  VIEW_APPLY,
  VIEW_CONFIRM,
};

static wmOperatorStatus image_view_zoom_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (ELEM(event->type, MOUSEZOOM, MOUSEPAN)) {
    SpaceImage *sima = CTX_wm_space_image(C);
    ARegion *region = CTX_wm_region(C);
    float delta, factor, location[2];

    UI_view2d_region_to_view(
        &region->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);

    delta = event->prev_xy[0] - event->xy[0] + event->prev_xy[1] - event->xy[1];

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

  image_view_zoom_init(C, op, event);
  return OPERATOR_RUNNING_MODAL;
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
      delta = float(x - vpd->origx);
    }
    else {
      delta = float(y - vpd->origy);
    }
  }
  else {
    delta = x - vpd->origx + y - vpd->origy;
  }

  delta /= U.pixelsize;

  if (zoom_invert) {
    delta = -delta;
  }

  if (viewzoom == USER_ZOOM_CONTINUE) {
    double time = BLI_time_now_seconds();
    float time_step = float(time - vpd->timer_lastdraw);
    float zfac;
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

static wmOperatorStatus image_view_zoom_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewZoomData *vpd = static_cast<ViewZoomData *>(op->customdata);
  short event_code = VIEW_PASS;
  wmOperatorStatus ret = OPERATOR_RUNNING_MODAL;

  /* Execute the events. */
  if (event->type == MOUSEMOVE) {
    event_code = VIEW_APPLY;
  }
  else if (event->type == TIMER) {
    /* Continuous zoom. */
    if (event->customdata == vpd->timer) {
      event_code = VIEW_APPLY;
    }
  }
  else if (event->type == vpd->launch_event) {
    if (event->val == KM_RELEASE) {
      event_code = VIEW_CONFIRM;
    }
  }

  switch (event_code) {
    case VIEW_APPLY: {
      const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");
      image_zoom_apply(vpd,
                       op,
                       event->xy[0],
                       event->xy[1],
                       U.viewzoom,
                       (U.uiflag & USER_ZOOM_INVERT) != 0,
                       (use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS)));
      break;
    }
    case VIEW_CONFIRM: {
      ret = OPERATOR_FINISHED;
      break;
    }
  }

  if ((ret & OPERATOR_RUNNING_MODAL) == 0) {
    image_view_zoom_exit(C, op, false);
  }

  return ret;
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

  /* API callbacks. */
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

static wmOperatorStatus image_view_ndof_invoke(bContext *C,
                                               wmOperator * /*op*/,
                                               const wmEvent *event)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);

  const wmNDOFMotionData &ndof = *static_cast<const wmNDOFMotionData *>(event->customdata);
  const float pan_speed = NDOF_PIXELS_PER_SECOND;

  blender::float3 pan_vec = ndof.time_delta * WM_event_ndof_translation_get_for_navigation(ndof);

  mul_v2_fl(pan_vec, pan_speed / sima->zoom);

  sima_zoom_set_factor(sima, region, max_ff(0.0f, 1.0f - pan_vec[2]), nullptr, false);
  sima->xof += pan_vec[0];
  sima->yof += pan_vec[1];

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_view_ndof(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "NDOF Pan/Zoom";
  ot->idname = "IMAGE_OT_view_ndof";
  ot->description = "Use a 3D mouse device to pan/zoom the view";

  /* API callbacks. */
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

static wmOperatorStatus image_view_all_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima;
  ARegion *region;

  /* retrieve state */
  sima = CTX_wm_space_image(C);
  region = CTX_wm_region(C);

  image_view_all(sima, region, op);

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

  /* API callbacks. */
  ot->exec = image_view_all_exec;
  ot->poll = space_image_main_region_poll;

  /* flags */
  ot->flag = OPTYPE_LOCK_BYPASS;

  /* properties */
  prop = RNA_def_boolean(ot->srna, "fit_view", false, "Fit View", "Fit frame to the viewport");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor To Center View Operator
 * \{ */

static wmOperatorStatus view_cursor_center_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima;
  ARegion *region;

  sima = CTX_wm_space_image(C);
  region = CTX_wm_region(C);

  image_view_all(sima, region, op);

  sima->cursor[0] = 0.5f;
  sima->cursor[1] = 0.5f;

  /* Needed for updating the cursor. */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_view_cursor_center(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Cursor To Center View";
  ot->description = "Set 2D Cursor To Center View location";
  ot->idname = "IMAGE_OT_view_cursor_center";

  /* API callbacks. */
  ot->exec = view_cursor_center_exec;
  ot->poll = ED_space_image_cursor_poll;

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

  /* API callbacks. */
  ot->exec = view_center_cursor_exec;
  ot->poll = ED_space_image_cursor_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Selected Operator
 * \{ */

static wmOperatorStatus image_view_selected_exec(bContext *C, wmOperator * /*op*/)
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
    Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
        scene, view_layer, nullptr);
    bool success = ED_uvedit_minmax_multi(scene, objects, min, max);
    if (!success) {
      return OPERATOR_CANCELLED;
    }
  }
  else if (ED_space_image_check_show_maskedit(sima, obedit)) {
    if (!ED_mask_selected_minmax(C, min, max, false)) {
      return OPERATOR_CANCELLED;
    }
    BKE_mask_coord_to_image(sima->image, &sima->iuser, min, min);
    BKE_mask_coord_to_image(sima->image, &sima->iuser, max, max);
  }
  rctf bounds{};
  bounds.xmin = min[0];
  bounds.ymin = min[1];
  bounds.xmax = max[0];
  bounds.ymax = max[1];

  /* add some margin */
  BLI_rctf_scale(&bounds, 1.4f);

  sima_zoom_set_from_bounds(sima, region, &bounds);

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

static bool image_view_selected_poll(bContext *C)
{
  return (space_image_main_region_poll(C) && (ED_operator_uvedit(C) || ED_maskedit_poll(C)));
}

void IMAGE_OT_view_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Center";
  ot->idname = "IMAGE_OT_view_selected";
  ot->description = "View all selected UVs";

  /* API callbacks. */
  ot->exec = image_view_selected_exec;
  ot->poll = image_view_selected_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Zoom In/Out Operator
 * \{ */

static wmOperatorStatus image_view_zoom_in_exec(bContext *C, wmOperator *op)
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

static wmOperatorStatus image_view_zoom_in_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
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

  /* API callbacks. */
  ot->invoke = image_view_zoom_in_invoke;
  ot->exec = image_view_zoom_in_exec;
  ot->poll = space_image_main_region_poll;

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

static wmOperatorStatus image_view_zoom_out_exec(bContext *C, wmOperator *op)
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

static wmOperatorStatus image_view_zoom_out_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
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

  /* API callbacks. */
  ot->invoke = image_view_zoom_out_invoke;
  ot->exec = image_view_zoom_out_exec;
  ot->poll = space_image_main_region_poll;

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Zoom Ratio Operator
 * \{ */

static wmOperatorStatus image_view_zoom_ratio_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);

  sima_zoom_set(sima, region, RNA_float_get(op->ptr, "ratio"), nullptr, false);

  /* ensure pixel exact locations for draw */
  sima->xof = int(sima->xof);
  sima->yof = int(sima->yof);

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_view_zoom_ratio(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Zoom Ratio";
  ot->idname = "IMAGE_OT_view_zoom_ratio";
  ot->description = "Set zoom ratio of the view";

  /* API callbacks. */
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

static wmOperatorStatus image_view_zoom_border_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);
  rctf bounds;
  const bool zoom_in = !RNA_boolean_get(op->ptr, "zoom_out");

  WM_operator_properties_border_to_rctf(op, &bounds);

  UI_view2d_region_to_view_rctf(&region->v2d, &bounds, &bounds);

  struct {
    float xof;
    float yof;
    float zoom;
  } sima_view_prev{};
  sima_view_prev.xof = sima->xof;
  sima_view_prev.yof = sima->yof;
  sima_view_prev.zoom = sima->zoom;

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

  /* API callbacks. */
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

struct ImageOpenData {
  PropertyPointerRNA pprop;
  ImageUser *iuser;
  ImageFormatData im_format;
};

static void image_open_init(bContext *C, wmOperator *op)
{
  ImageOpenData *iod;
  op->customdata = iod = MEM_new<ImageOpenData>(__func__);
  iod->iuser = static_cast<ImageUser *>(
      CTX_data_pointer_get_type(C, "image_user", &RNA_ImageUser).data);
  UI_context_active_but_prop_get_templateID(C, &iod->pprop.ptr, &iod->pprop.prop);
}

static void image_open_cancel(bContext * /*C*/, wmOperator *op)
{
  ImageOpenData *iod = static_cast<ImageOpenData *>(op->customdata);
  op->customdata = nullptr;
  MEM_delete(iod);
}

static Image *image_open_single(Main *bmain,
                                Library *owner_library,
                                wmOperator *op,
                                const ImageFrameRange *range,
                                const bool use_multiview)
{
  bool exists = false;
  Image *ima = nullptr;

  errno = 0;
  ima = BKE_image_load_exists_in_lib(bmain, owner_library, range->filepath, &exists);

  if (!ima) {
    if (op->customdata) {
      MEM_delete(static_cast<ImageOpenData *>(op->customdata));
    }
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Cannot read '%s': %s",
                range->filepath,
                errno ? strerror(errno) : RPT_("unsupported image format"));
    return nullptr;
  }

  /* If image already exists, update its file path based on relative path property, see: #109561.
   */
  if (exists) {
    STRNCPY(ima->filepath, range->filepath);
    return ima;
  }

  /* handle multiview images */
  if (use_multiview) {
    ImageOpenData *iod = static_cast<ImageOpenData *>(op->customdata);
    ImageFormatData *imf = &iod->im_format;

    ima->flag |= IMA_USE_VIEWS;
    ima->views_format = imf->views_format;
    *ima->stereo3d_format = imf->stereo3d_format;
  }
  else {
    ima->flag &= ~IMA_USE_VIEWS;
    BKE_image_free_views(ima);
  }

  if (ima->source == IMA_SRC_FILE) {
    if (range->udims_detected && range->udim_tiles.first) {
      ima->source = IMA_SRC_TILED;
      ImageTile *first_tile = static_cast<ImageTile *>(ima->tiles.first);
      first_tile->tile_number = range->offset;
      LISTBASE_FOREACH (LinkData *, node, &range->udim_tiles) {
        BKE_image_add_tile(ima, POINTER_AS_INT(node->data), nullptr);
      }
    }
    else if (range->length > 1) {
      ima->source = IMA_SRC_SEQUENCE;
    }
  }

  return ima;
}

static wmOperatorStatus image_open_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ScrArea *area = CTX_wm_area(C);
  Scene *scene = CTX_data_scene(C);
  ImageUser *iuser = nullptr;
  Image *ima = nullptr;
  int frame_seq_len = 0;
  int frame_ofs = 1;

  const bool use_multiview = RNA_boolean_get(op->ptr, "use_multiview");
  const bool use_udim = RNA_boolean_get(op->ptr, "use_udim_detecting");

  if (!op->customdata) {
    image_open_init(C, op);
  }

  ImageOpenData *iod = static_cast<ImageOpenData *>(op->customdata);
  ID *owner_id = iod->pprop.ptr.owner_id;
  Library *owner_library = owner_id ? owner_id->lib : nullptr;
  blender::StringRefNull root_path = owner_library ? owner_library->runtime->filepath_abs :
                                                     BKE_main_blendfile_path(bmain);

  ListBase ranges = ED_image_filesel_detect_sequences(root_path, op, use_udim);
  LISTBASE_FOREACH (ImageFrameRange *, range, &ranges) {
    Image *ima_range = image_open_single(bmain, owner_library, op, range, use_multiview);

    /* take the first image */
    if ((ima == nullptr) && ima_range) {
      ima = ima_range;
      frame_seq_len = range->length;
      frame_ofs = range->offset;
    }

    BLI_freelistN(&range->udim_tiles);
    BLI_freelistN(&range->frames);
  }
  BLI_freelistN(&ranges);

  if (ima == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* hook into UI */
  if (iod->pprop.prop) {
    /* when creating new ID blocks, use is already 1, but RNA
     * pointer use also increases user, so this compensates it */
    id_us_min(&ima->id);

    if (iod->pprop.ptr.owner_id) {
      BKE_id_move_to_same_lib(*bmain, ima->id, *iod->pprop.ptr.owner_id);
    }

    PointerRNA imaptr = RNA_id_pointer_create(&ima->id);
    RNA_property_pointer_set(&iod->pprop.ptr, iod->pprop.prop, imaptr, nullptr);
    RNA_property_update(C, &iod->pprop.ptr, iod->pprop.prop);
  }

  if (iod->iuser) {
    iuser = iod->iuser;
  }
  else if (area && area->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
    ED_space_image_set(bmain, sima, ima, false);
    iuser = &sima->iuser;
  }
  else {
    Tex *tex = static_cast<Tex *>(CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data);
    if (tex && tex->type == TEX_IMAGE) {
      iuser = &tex->iuser;
    }

    if (iuser == nullptr) {
      Camera *cam = static_cast<Camera *>(
          CTX_data_pointer_get_type(C, "camera", &RNA_Camera).data);
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

  op->customdata = nullptr;
  MEM_delete(iod);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus image_open_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  SpaceImage *sima = CTX_wm_space_image(C); /* XXX other space types can call */
  const char *path = U.textudir;
  Image *ima = nullptr;
  Scene *scene = CTX_data_scene(C);

  if (sima) {
    ima = sima->image;
  }

  if (ima == nullptr) {
    Tex *tex = static_cast<Tex *>(CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data);
    if (tex && tex->type == TEX_IMAGE) {
      ima = tex->ima;
    }
  }

  if (ima == nullptr) {
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

  /* Show multi-view save options only if scene has multi-views. */
  PropertyRNA *prop;
  prop = RNA_struct_find_property(op->ptr, "show_multiview");
  RNA_property_boolean_set(op->ptr, prop, (scene->r.scemode & R_MULTIVIEW) != 0);

  image_filesel(C, op, path);

  return OPERATOR_RUNNING_MODAL;
}

static bool image_open_draw_check_prop(PointerRNA * /*ptr*/,
                                       PropertyRNA *prop,
                                       void * /*user_data*/)
{
  const char *prop_id = RNA_property_identifier(prop);

  return !STR_ELEM(prop_id, "filepath", "directory", "filename");
}

static void image_open_draw(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  ImageOpenData *iod = static_cast<ImageOpenData *>(op->customdata);
  ImageFormatData *imf = &iod->im_format;

  /* main draw call */
  uiDefAutoButsRNA(layout,
                   op->ptr,
                   image_open_draw_check_prop,
                   nullptr,
                   nullptr,
                   UI_BUT_LABEL_ALIGN_NONE,
                   false);

  /* image template */
  PointerRNA imf_ptr = RNA_pointer_create_discrete(nullptr, &RNA_ImageFormatSettings, imf);

  /* multiview template */
  if (RNA_boolean_get(op->ptr, "show_multiview")) {
    uiTemplateImageFormatViews(layout, &imf_ptr, op->ptr);
  }
}

static void image_operator_prop_allow_tokens(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "allow_path_tokens", true, "", "Allow the path to contain substitution tokens");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

void IMAGE_OT_open(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Open Image";
  ot->description = "Open image";
  ot->idname = "IMAGE_OT_open";

  /* API callbacks. */
  ot->exec = image_open_exec;
  ot->invoke = image_open_invoke;
  ot->cancel = image_open_cancel;
  ot->ui = image_open_draw;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  image_operator_prop_allow_tokens(ot);
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILES |
                                     WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

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
/** \name Browse Image Operator
 * \{ */

static wmOperatorStatus image_file_browse_exec(bContext *C, wmOperator *op)
{
  Image *ima = static_cast<Image *>(op->customdata);
  if (ima == nullptr) {
    return OPERATOR_CANCELLED;
  }

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);
  if (BLI_path_is_rel(filepath)) {
    Main *bmain = CTX_data_main(C);
    /* Relative path created by the file-browser are always relative to the current blend-file,
     * need to be made relative to the library blend-file path in case image is an editable
     * linked data. */
    BLI_path_abs(filepath, BKE_main_blendfile_path(bmain));
    /* TODO(@mont29): make this a BKE_lib_id helper (already a static function in BKE_image too),
     * we likely need this in more places in the future. */
    BLI_path_rel(filepath, ID_BLEND_PATH(bmain, &ima->id));
  }

  /* If loading into a tiled texture, ensure that the filename is tokenized. */
  if (ima->source == IMA_SRC_TILED) {
    BKE_image_ensure_tile_token(filepath, sizeof(filepath));
  }

  PropertyRNA *imaprop;
  PointerRNA imaptr = RNA_id_pointer_create(&ima->id);
  imaprop = RNA_struct_find_property(&imaptr, "filepath");

  RNA_property_string_set(&imaptr, imaprop, filepath);
  RNA_property_update(C, &imaptr, imaprop);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus image_file_browse_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Image *ima = image_from_context(C);
  if (!ima) {
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  char filepath[FILE_MAX];
  STRNCPY(filepath, ima->filepath);
  BLI_path_abs(filepath, ID_BLEND_PATH(bmain, &ima->id));

  /* Shift+Click to open the file, Alt+Click to browse a folder in the OS's browser. */
  if (event->modifier & (KM_SHIFT | KM_ALT)) {
    wmOperatorType *ot = WM_operatortype_find("WM_OT_path_open", true);
    PointerRNA props_ptr;

    if (event->modifier & KM_ALT) {
      char *lslash = (char *)BLI_path_slash_rfind(filepath);
      if (lslash) {
        *lslash = '\0';
      }
    }
    else if (ima->source == IMA_SRC_TILED) {
      ImageUser iuser = image_user_from_context_and_active_tile(C, ima);
      BKE_image_user_file_path(&iuser, ima, filepath);
    }

    WM_operator_properties_create_ptr(&props_ptr, ot);
    RNA_string_set(&props_ptr, "filepath", filepath);
    WM_operator_name_call_ptr(C, ot, blender::wm::OpCallContext::ExecDefault, &props_ptr, nullptr);
    WM_operator_properties_free(&props_ptr);

    return OPERATOR_CANCELLED;
  }

  /* The image is typically passed to the operator via layout/button context (e.g.
   * #uiLayout::context_ptr_set. The File Browser doesn't support
   * restoring this context when calling `exec()` though, so we have to pass it the image via
   * custom data. */
  op->customdata = ima;

  image_filesel(C, op, filepath);

  return OPERATOR_RUNNING_MODAL;
}

static bool image_file_browse_poll(bContext *C)
{
  Image *ima = image_from_context(C);
  return (ima && ID_IS_EDITABLE(ima));
}

void IMAGE_OT_file_browse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Browse Image";
  ot->description =
      "Open an image file browser, hold Shift to open the file, Alt to browse containing "
      "directory";
  ot->idname = "IMAGE_OT_file_browse";

  /* API callbacks. */
  ot->exec = image_file_browse_exec;
  ot->invoke = image_file_browse_invoke;
  ot->poll = image_file_browse_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Match Movie Length Operator
 * \{ */

static wmOperatorStatus image_match_len_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  Image *ima = image_from_context(C);
  ImageUser *iuser = image_user_from_context(C);

  if (!ima || !iuser) {
    /* Try to get a Texture, or a SpaceImage from context... */
    Tex *tex = static_cast<Tex *>(CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data);
    if (tex && tex->type == TEX_IMAGE) {
      ima = tex->ima;
      iuser = &tex->iuser;
    }
  }

  if (!ima || !iuser || !BKE_image_has_anim(ima)) {
    return OPERATOR_CANCELLED;
  }

  MovieReader *anim = ((ImageAnim *)ima->anims.first)->anim;
  if (!anim) {
    return OPERATOR_CANCELLED;
  }
  iuser->frames = MOV_get_duration_frames(anim, IMB_TC_RECORD_RUN);
  BKE_image_user_frame_calc(ima, iuser, scene->r.cfra);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_match_movie_length(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Match Movie Length";
  ot->description = "Set image's user's length to the one of this video";
  ot->idname = "IMAGE_OT_match_movie_length";

  /* API callbacks. */
  ot->exec = image_match_len_exec;

  /* flags */
  /* Don't think we need undo for that. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_INTERNAL /* | OPTYPE_UNDO */;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Replace Image Operator
 * \{ */

static wmOperatorStatus image_replace_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  char filepath[FILE_MAX];

  if (!sima->image) {
    return OPERATOR_CANCELLED;
  }

  RNA_string_get(op->ptr, "filepath", filepath);

  /* we can't do much if the filepath is longer than FILE_MAX :/ */
  STRNCPY(sima->image->filepath, filepath);

  if (sima->image->source == IMA_SRC_GENERATED) {
    sima->image->source = IMA_SRC_FILE;
    BKE_image_signal(bmain, sima->image, &sima->iuser, IMA_SIGNAL_SRC_CHANGE);
  }

  if (BLI_path_extension_check_array(filepath, imb_ext_movie)) {
    sima->image->source = IMA_SRC_MOVIE;
  }
  else {
    sima->image->source = IMA_SRC_FILE;
  }

  /* XXX BKE_packedfile_unpack_image frees image buffers */
  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  BKE_icon_changed(BKE_icon_id_ensure(&sima->image->id));
  BKE_image_signal(bmain, sima->image, &sima->iuser, IMA_SIGNAL_RELOAD);
  DEG_id_tag_update(&sima->image->id, 0);
  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, sima->image);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus image_replace_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent * /*event*/)
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

  /* API callbacks. */
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
                                 FILE_SORT_DEFAULT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Save Image As Operator
 * \{ */

struct ImageSaveData {
  ImageUser *iuser;
  Image *image;
  ImageSaveOptions opts;
};

static void image_save_options_from_op(Main *bmain, ImageSaveOptions *opts, wmOperator *op)
{
  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    RNA_string_get(op->ptr, "filepath", opts->filepath);
    BLI_path_abs(opts->filepath, BKE_main_blendfile_path(bmain));
  }

  opts->relative = (RNA_struct_find_property(op->ptr, "relative_path") &&
                    RNA_boolean_get(op->ptr, "relative_path"));
  opts->save_copy = (RNA_struct_find_property(op->ptr, "copy") &&
                     RNA_boolean_get(op->ptr, "copy"));
  opts->save_as_render = (RNA_struct_find_property(op->ptr, "save_as_render") &&
                          RNA_boolean_get(op->ptr, "save_as_render"));
}

static bool save_image_op(
    Main *bmain, Image *ima, ImageUser *iuser, wmOperator *op, const ImageSaveOptions *opts)
{
  WM_cursor_wait(true);

  bool ok = BKE_image_save(op->reports, bmain, ima, iuser, opts);

  WM_cursor_wait(false);

  /* Remember file path for next save. */
  STRNCPY(G.filepath_last_image, opts->filepath);

  WM_main_add_notifier(NC_IMAGE | NA_EDITED, ima);

  return ok;
}

static ImageSaveData *image_save_as_init(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Image *image = image_from_context(C);
  ImageUser *iuser = image_user_from_context(C);
  Scene *scene = CTX_data_scene(C);

  ImageSaveData *isd = MEM_callocN<ImageSaveData>(__func__);
  isd->image = image;
  isd->iuser = iuser;

  if (!BKE_image_save_options_init(&isd->opts, bmain, scene, image, iuser, true, false)) {
    BKE_image_save_options_free(&isd->opts);
    MEM_freeN(isd);
    return nullptr;
  }

  isd->opts.do_newpath = true;

  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    RNA_string_set(op->ptr, "filepath", isd->opts.filepath);
  }

  /* Enable save_copy by default for render results. */
  if (image->source == IMA_SRC_VIEWER && !RNA_struct_property_is_set(op->ptr, "copy")) {
    RNA_boolean_set(op->ptr, "copy", true);
  }

  if (!RNA_struct_property_is_set(op->ptr, "save_as_render")) {
    RNA_boolean_set(op->ptr, "save_as_render", isd->opts.save_as_render);
  }

  /* Show multi-view save options only if image has multi-views. */
  PropertyRNA *prop;
  prop = RNA_struct_find_property(op->ptr, "show_multiview");
  RNA_property_boolean_set(op->ptr, prop, BKE_image_is_multiview(image));
  prop = RNA_struct_find_property(op->ptr, "use_multiview");
  RNA_property_boolean_set(op->ptr, prop, BKE_image_is_multiview(image));

  op->customdata = isd;

  return isd;
}

static void image_save_as_free(wmOperator *op)
{
  if (op->customdata) {
    ImageSaveData *isd = static_cast<ImageSaveData *>(op->customdata);
    BKE_image_save_options_free(&isd->opts);

    MEM_freeN(isd);
    op->customdata = nullptr;
  }
}

static wmOperatorStatus image_save_as_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ImageSaveData *isd;

  if (op->customdata) {
    isd = static_cast<ImageSaveData *>(op->customdata);
  }
  else {
    isd = image_save_as_init(C, op);
    if (isd == nullptr) {
      return OPERATOR_CANCELLED;
    }
  }

  image_save_options_from_op(bmain, &isd->opts, op);
  BKE_image_save_options_update(&isd->opts, isd->image);

  save_image_op(bmain, isd->image, isd->iuser, op, &isd->opts);

  if (isd->opts.save_copy == false) {
    BKE_image_free_packedfiles(isd->image);
  }

  image_save_as_free(op);

  return OPERATOR_FINISHED;
}

static bool image_save_as_check(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ImageSaveData *isd = static_cast<ImageSaveData *>(op->customdata);

  image_save_options_from_op(bmain, &isd->opts, op);
  BKE_image_save_options_update(&isd->opts, isd->image);

  return WM_operator_filesel_ensure_ext_imtype(op, &isd->opts.im_format);
}

static wmOperatorStatus image_save_as_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent * /*event*/)
{
  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    return image_save_as_exec(C, op);
  }

  ImageSaveData *isd = image_save_as_init(C, op);
  if (isd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  image_filesel(C, op, isd->opts.filepath);

  return OPERATOR_RUNNING_MODAL;
}

static void image_save_as_cancel(bContext * /*C*/, wmOperator *op)
{
  image_save_as_free(op);
}

static bool image_save_as_draw_check_prop(PointerRNA *ptr, PropertyRNA *prop, void *user_data)
{
  ImageSaveData *isd = static_cast<ImageSaveData *>(user_data);
  const char *prop_id = RNA_property_identifier(prop);

  return !(STREQ(prop_id, "filepath") || STREQ(prop_id, "directory") ||
           STREQ(prop_id, "filename") ||
           /* when saving a copy, relative path has no effect */
           (STREQ(prop_id, "relative_path") && RNA_boolean_get(ptr, "copy")) ||
           (STREQ(prop_id, "save_as_render") && isd->image->source == IMA_SRC_VIEWER));
}

static void image_save_as_draw(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  ImageSaveData *isd = static_cast<ImageSaveData *>(op->customdata);
  const bool is_multiview = RNA_boolean_get(op->ptr, "show_multiview");
  const bool save_as_render = RNA_boolean_get(op->ptr, "save_as_render");

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  /* Operator settings. */
  uiDefAutoButsRNA(layout,
                   op->ptr,
                   image_save_as_draw_check_prop,
                   isd,
                   nullptr,
                   UI_BUT_LABEL_ALIGN_NONE,
                   false);

  layout->separator();

  /* Image format settings. */
  PointerRNA imf_ptr = RNA_pointer_create_discrete(
      nullptr, &RNA_ImageFormatSettings, &isd->opts.im_format);
  uiTemplateImageSettings(layout, C, &imf_ptr, save_as_render);

  if (!save_as_render) {
    PointerRNA linear_settings_ptr = RNA_pointer_get(&imf_ptr, "linear_colorspace_settings");
    uiLayout *col = &layout->column(true);
    col->separator();
    col->prop(&linear_settings_ptr, "name", UI_ITEM_NONE, IFACE_("Color Space"), ICON_NONE);
  }

  /* Multiview settings. */
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
    /* no need to nullptr check here */
    Image *ima = image_from_context(C);

    if (ima->source == IMA_SRC_VIEWER) {
      CTX_wm_operator_poll_msg_set(C, "Cannot save image while rendering");
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

  /* API callbacks. */
  ot->exec = image_save_as_exec;
  ot->check = image_save_as_check;
  ot->invoke = image_save_as_invoke;
  ot->cancel = image_save_as_cancel;
  ot->ui = image_save_as_draw;
  ot->poll = image_save_as_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_boolean(
      ot->srna,
      "save_as_render",
      false,
      "Save As Render",
      "Save image with render color management.\n"
      "For display image formats like PNG, apply view and display transform.\n"
      "For intermediate image formats like OpenEXR, use the default render output color space");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "copy",
                         false,
                         "Copy",
                         "Create a new image file without modifying the current image in Blender");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  image_operator_prop_allow_tokens(ot);
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE,
                                 FILE_SPECIAL,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Save Image Operator
 * \{ */

/**
 * \param iuser: Image user or nullptr when called outside the image space.
 */
static bool image_file_format_writable(Image *ima, ImageUser *iuser)
{
  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);
  bool ret = false;

  if (ibuf && BKE_image_buffer_format_writable(ibuf)) {
    ret = true;
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

  /* Check if there is a valid file path and image format we can write
   * outside of the 'poll' so we can show a report with a pop-up. */

  /* Can always repack images.
   * Images without a filepath will go to "Save As". */
  return true;
}

static wmOperatorStatus image_save_exec(bContext *C, wmOperator *op)
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
    /* Report since this can be called from key shortcuts. */
    BKE_reportf(op->reports, RPT_INFO, "Packed to memory image \"%s\"", image->filepath);
    return OPERATOR_FINISHED;
  }

  if (!BKE_image_save_options_init(&opts, bmain, scene, image, iuser, false, false)) {
    BKE_image_save_options_free(&opts);
    return OPERATOR_CANCELLED;
  }
  image_save_options_from_op(bmain, &opts, op);

  /* Check if file write permission is ok. */
  if (BLI_exists(opts.filepath) && !BLI_file_is_writable(opts.filepath)) {
    BKE_reportf(
        op->reports, RPT_ERROR, "Cannot save image, path \"%s\" is not writable", opts.filepath);
  }
  else if (save_image_op(bmain, image, iuser, op, &opts)) {
    /* Report since this can be called from key shortcuts. */
    BKE_reportf(op->reports, RPT_INFO, "Saved image \"%s\"", opts.filepath);
    ok = true;
  }

  BKE_image_save_options_free(&opts);

  if (ok) {
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static wmOperatorStatus image_save_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Image *ima = image_from_context(C);
  ImageUser *iuser = image_user_from_context(C);

  /* Not writable formats or images without a file-path will go to "Save As". */
  if (!BKE_image_has_packedfile(ima) &&
      (!BKE_image_has_filepath(ima) || !image_file_format_writable(ima, iuser)))
  {
    WM_operator_name_call(
        C, "IMAGE_OT_save_as", blender::wm::OpCallContext::InvokeDefault, nullptr, event);
    return OPERATOR_CANCELLED;
  }
  return image_save_exec(C, op);
}

void IMAGE_OT_save(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Save Image";
  ot->idname = "IMAGE_OT_save";
  ot->description = "Save the image with current name and settings";

  /* API callbacks. */
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

static wmOperatorStatus image_save_sequence_exec(bContext *C, wmOperator *op)
{
  Image *image = image_from_context(C);
  ImBuf *ibuf, *first_ibuf = nullptr;
  int tot = 0;
  char di[FILE_MAX];
  MovieCacheIter *iter;

  if (image == nullptr) {
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
  ibuf = nullptr;
  if (image->cache != nullptr) {
    iter = IMB_moviecacheIter_new(image->cache);
    while (!IMB_moviecacheIter_done(iter)) {
      ibuf = IMB_moviecacheIter_getImBuf(iter);
      if (ibuf != nullptr && ibuf->userflags & IB_BITMAPDIRTY) {
        if (first_ibuf == nullptr) {
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
  BLI_path_split_dir_part(first_ibuf->filepath, di, sizeof(di));
  BKE_reportf(op->reports, RPT_INFO, "%d image(s) will be saved in %s", tot, di);

  iter = IMB_moviecacheIter_new(image->cache);
  while (!IMB_moviecacheIter_done(iter)) {
    ibuf = IMB_moviecacheIter_getImBuf(iter);

    if (ibuf != nullptr && ibuf->userflags & IB_BITMAPDIRTY) {
      if (0 == IMB_save_image(ibuf, ibuf->filepath, IB_byte_data)) {
        BKE_reportf(op->reports, RPT_ERROR, "Could not write image: %s", strerror(errno));
        break;
      }

      BKE_reportf(op->reports, RPT_INFO, "Saved %s", ibuf->filepath);
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

  /* API callbacks. */
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

static bool image_should_be_saved(Image *ima, bool *r_is_format_writable)
{
  if (BKE_image_is_dirty_writable(ima, r_is_format_writable) &&
      ELEM(ima->source, IMA_SRC_FILE, IMA_SRC_GENERATED, IMA_SRC_TILED))
  {
    return image_should_be_saved_when_modified(ima);
  }
  return false;
}

static bool image_has_valid_path(Image *ima)
{
  return strchr(ima->filepath, '\\') || strchr(ima->filepath, '/');
}

static bool image_should_pack_during_save_all(const Image *ima)
{
  /* Images without a filepath (implied with IMA_SRC_GENERATED) should
   * be packed during a save_all operation. */
  return (ima->source == IMA_SRC_GENERATED) ||
         (ima->source == IMA_SRC_TILED && !BKE_image_has_filepath(ima));
}

bool ED_image_should_save_modified(const Main *bmain)
{
  ReportList reports;
  BKE_reports_init(&reports, RPT_STORE);

  uint modified_images_count = ED_image_save_all_modified_info(bmain, &reports);
  bool should_save = modified_images_count || !BLI_listbase_is_empty(&reports.list);

  BKE_reports_free(&reports);

  return should_save;
}

int ED_image_save_all_modified_info(const Main *bmain, ReportList *reports)
{
  blender::Set<std::string> unique_paths;

  int num_saveable_images = 0;

  for (Image *ima = static_cast<Image *>(bmain->images.first); ima;
       ima = static_cast<Image *>(ima->id.next))
  {
    bool is_format_writable;

    if (image_should_be_saved(ima, &is_format_writable)) {
      if (BKE_image_has_packedfile(ima) || image_should_pack_during_save_all(ima)) {
        if (ID_IS_EDITABLE(ima)) {
          num_saveable_images++;
        }
        else {
          BKE_reportf(reports,
                      RPT_WARNING,
                      "Packed library image cannot be saved: \"%s\" from \"%s\"",
                      ima->id.name + 2,
                      ima->id.lib->filepath);
        }
      }
      else if (!is_format_writable) {
        BKE_reportf(reports,
                    RPT_WARNING,
                    "Image cannot be saved, use a different file format: \"%s\"",
                    ima->id.name + 2);
      }
      else {
        if (image_has_valid_path(ima)) {
          num_saveable_images++;
          if (unique_paths.contains_as(ima->filepath)) {
            BKE_reportf(reports,
                        RPT_WARNING,
                        "Multiple images cannot be saved to an identical path: \"%s\"",
                        ima->filepath);
          }
          else {
            unique_paths.add(ima->filepath);
          }
        }
        else {
          BKE_reportf(reports,
                      RPT_WARNING,
                      "Image cannot be saved, no valid file path: \"%s\"",
                      ima->filepath);
        }
      }
    }
  }

  return num_saveable_images;
}

bool ED_image_save_all_modified(const bContext *C, ReportList *reports)
{
  Main *bmain = CTX_data_main(C);

  ED_image_save_all_modified_info(bmain, reports);

  bool ok = true;

  for (Image *ima = static_cast<Image *>(bmain->images.first); ima;
       ima = static_cast<Image *>(ima->id.next))
  {
    bool is_format_writable;

    if (image_should_be_saved(ima, &is_format_writable)) {
      if (BKE_image_has_packedfile(ima) || image_should_pack_during_save_all(ima)) {
        BKE_image_memorypack(ima);
      }
      else if (is_format_writable) {
        if (image_has_valid_path(ima)) {
          ImageSaveOptions opts;
          Scene *scene = CTX_data_scene(C);
          if (BKE_image_save_options_init(&opts, bmain, scene, ima, nullptr, false, false)) {
            bool saved_successfully = BKE_image_save(reports, bmain, ima, nullptr, &opts);
            ok = ok && saved_successfully;
          }
          BKE_image_save_options_free(&opts);
        }
      }
    }
  }
  return ok;
}

static bool image_save_all_modified_poll(bContext *C)
{
  int num_files = ED_image_save_all_modified_info(CTX_data_main(C), nullptr);
  return num_files > 0;
}

static wmOperatorStatus image_save_all_modified_exec(bContext *C, wmOperator *op)
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

  /* API callbacks. */
  ot->exec = image_save_all_modified_exec;
  ot->poll = image_save_all_modified_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reload Image Operator
 * \{ */

static wmOperatorStatus image_reload_exec(bContext *C, wmOperator * /*op*/)
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
  DEG_id_tag_update(&ima->id, ID_RECALC_EDITORS | ID_RECALC_SOURCE);

  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_reload(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reload Image";
  ot->idname = "IMAGE_OT_reload";
  ot->description = "Reload current image from disk";

  /* API callbacks. */
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

struct ImageNewData {
  PropertyPointerRNA pprop = {};
};

static ImageNewData *image_new_init(bContext *C, wmOperator *op)
{
  if (op->customdata) {
    return static_cast<ImageNewData *>(op->customdata);
  }

  ImageNewData *data = MEM_new<ImageNewData>(__func__);
  UI_context_active_but_prop_get_templateID(C, &data->pprop.ptr, &data->pprop.prop);
  op->customdata = data;
  return data;
}

static void image_new_free(wmOperator *op)
{
  if (op->customdata) {
    MEM_delete(static_cast<ImageNewData *>(op->customdata));
  }
}

static wmOperatorStatus image_new_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima;
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

    if (data->pprop.ptr.owner_id) {
      BKE_id_move_to_same_lib(*bmain, ima->id, *data->pprop.ptr.owner_id);
    }

    PointerRNA imaptr = RNA_id_pointer_create(&ima->id);
    RNA_property_pointer_set(&data->pprop.ptr, data->pprop.prop, imaptr, nullptr);
    RNA_property_update(C, &data->pprop.ptr, data->pprop.prop);
  }
  else if (sima) {
    ED_space_image_set(bmain, sima, ima, false);
  }
  else {
    /* #BKE_image_add_generated creates one user by default, remove it if image is not linked to
     * anything. ref. #94599. */
    id_us_min(&ima->id);
  }

  BKE_image_signal(bmain, ima, (sima) ? &sima->iuser : nullptr, IMA_SIGNAL_USER_NEW_IMAGE);

  WM_event_add_notifier(C, NC_IMAGE | NA_ADDED, ima);

  image_new_free(op);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus image_new_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  /* Get property in advance, it doesn't work after WM_operator_props_dialog_popup. */
  ImageNewData *data;
  op->customdata = data = MEM_new<ImageNewData>(__func__);
  UI_context_active_but_prop_get_templateID(C, &data->pprop.ptr, &data->pprop.prop);

  /* Better for user feedback. */
  RNA_string_set(op->ptr, "name", DATA_(IMA_DEF_NAME));
  return WM_operator_props_dialog_popup(
      C, op, 300, IFACE_("Create a New Image"), IFACE_("New Image"));
}

static void image_new_draw(bContext * /*C*/, wmOperator *op)
{
  uiLayout *col;
  uiLayout *layout = op->layout;
#if 0
  Scene *scene = CTX_data_scene(C);
  const bool is_multiview = (scene->r.scemode & R_MULTIVIEW) != 0;
#endif

  /* copy of WM_operator_props_dialog_popup() layout */

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  col = &layout->column(false);
  col->prop(op->ptr, "name", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(op->ptr, "width", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(op->ptr, "height", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(op->ptr, "color", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(op->ptr, "alpha", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(op->ptr, "generated_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(op->ptr, "float", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(op->ptr, "tiled", UI_ITEM_NONE, std::nullopt, ICON_NONE);

#if 0
  if (is_multiview) {
    col[0].label("", ICON_NONE);
    col[1].prop( op->ptr, "use_stereo_3d", 0, std::nullopt, ICON_NONE);
  }
#endif
}

static void image_new_cancel(bContext * /*C*/, wmOperator *op)
{
  image_new_free(op);
}

void IMAGE_OT_new(wmOperatorType *ot)
{
  PropertyRNA *prop;
  static const float default_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

  /* identifiers */
  ot->name = "New Image";
  ot->description = "Create a new image";
  ot->idname = "IMAGE_OT_new";

  /* API callbacks. */
  ot->exec = image_new_exec;
  ot->invoke = image_new_invoke;
  ot->ui = image_new_draw;
  ot->cancel = image_new_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_string(
      ot->srna, "name", IMA_DEF_NAME, MAX_ID_NAME - 2, "Name", "Image data-block name");
  prop = RNA_def_int(ot->srna, "width", 1024, 1, INT_MAX, "Width", "Image width", 1, 16384);
  RNA_def_property_subtype(prop, PROP_PIXEL);
  prop = RNA_def_int(ot->srna, "height", 1024, 1, INT_MAX, "Height", "Image height", 1, 16384);
  RNA_def_property_subtype(prop, PROP_PIXEL);
  prop = RNA_def_float_color(
      ot->srna, "color", 4, nullptr, 0.0f, FLT_MAX, "Color", "Default fill color", 0.0f, 1.0f);
  RNA_def_property_subtype(prop, PROP_COLOR_GAMMA);
  RNA_def_property_float_array_default(prop, default_color);
  RNA_def_boolean(ot->srna, "alpha", true, "Alpha", "Create an image with an alpha channel");
  RNA_def_enum(ot->srna,
               "generated_type",
               rna_enum_image_generated_type_items,
               IMA_GENTYPE_BLANK,
               "Generated Type",
               "Fill the image with a grid for UV map testing");
  RNA_def_boolean(ot->srna,
                  "float",
                  false,
                  "32-bit Float",
                  "Create image with 32-bit floating-point bit depth");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_boolean(
      ot->srna, "use_stereo_3d", false, "Stereo 3D", "Create an image with left and right views");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_boolean(ot->srna, "tiled", false, "Tiled", "Create a tiled image");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

#undef IMA_DEF_NAME

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flip Operator
 * \{ */

static wmOperatorStatus image_flip_exec(bContext *C, wmOperator *op)
{
  Image *ima = image_from_context(C);
  ImageUser iuser = image_user_from_context_and_active_tile(C, ima);
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);
  SpaceImage *sima = CTX_wm_space_image(C);
  const bool is_paint = ((sima != nullptr) && (sima->mode == SI_MODE_PAINT));

  if (ibuf == nullptr) {
    /* TODO: this should actually never happen, but does for render-results -> cleanup. */
    return OPERATOR_CANCELLED;
  }

  const bool use_flip_x = RNA_boolean_get(op->ptr, "use_flip_x");
  const bool use_flip_y = RNA_boolean_get(op->ptr, "use_flip_y");

  if (!use_flip_x && !use_flip_y) {
    BKE_image_release_ibuf(ima, ibuf, nullptr);
    return OPERATOR_FINISHED;
  }

  ED_image_undo_push_begin_with_image(op->type->name, ima, ibuf, &iuser);

  if (is_paint) {
    ED_imapaint_clear_partial_redraw();
  }

  const int size_x = ibuf->x;
  const int size_y = ibuf->y;

  if (ibuf->float_buffer.data) {
    float *float_pixels = ibuf->float_buffer.data;

    float *orig_float_pixels = static_cast<float *>(MEM_dupallocN(float_pixels));
    for (int x = 0; x < size_x; x++) {
      const int source_pixel_x = use_flip_x ? size_x - x - 1 : x;
      for (int y = 0; y < size_y; y++) {
        const int source_pixel_y = use_flip_y ? size_y - y - 1 : y;

        const float *source_pixel =
            &orig_float_pixels[4 * (source_pixel_x + source_pixel_y * size_x)];
        float *target_pixel = &float_pixels[4 * (x + y * size_x)];

        copy_v4_v4(target_pixel, source_pixel);
      }
    }
    MEM_freeN(orig_float_pixels);

    if (ibuf->byte_buffer.data) {
      IMB_byte_from_float(ibuf);
    }
  }
  else if (ibuf->byte_buffer.data) {
    uchar *char_pixels = ibuf->byte_buffer.data;
    uchar *orig_char_pixels = static_cast<uchar *>(MEM_dupallocN(char_pixels));
    for (int x = 0; x < size_x; x++) {
      const int source_pixel_x = use_flip_x ? size_x - x - 1 : x;
      for (int y = 0; y < size_y; y++) {
        const int source_pixel_y = use_flip_y ? size_y - y - 1 : y;

        const uchar *source_pixel =
            &orig_char_pixels[4 * (source_pixel_x + source_pixel_y * size_x)];
        uchar *target_pixel = &char_pixels[4 * (x + y * size_x)];

        copy_v4_v4_uchar(target_pixel, source_pixel);
      }
    }
    MEM_freeN(orig_char_pixels);
  }
  else {
    BKE_image_release_ibuf(ima, ibuf, nullptr);
    return OPERATOR_CANCELLED;
  }

  ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
  BKE_image_mark_dirty(ima, ibuf);

  ED_image_undo_push_end();

  BKE_image_partial_update_mark_full_update(ima);

  DEG_id_tag_update(&ima->id, ID_RECALC_EDITORS);
  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

  BKE_image_release_ibuf(ima, ibuf, nullptr);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_flip(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Flip Image";
  ot->idname = "IMAGE_OT_flip";
  ot->description = "Flip the image";

  /* API callbacks. */
  ot->exec = image_flip_exec;
  ot->poll = image_from_context_editable_has_data_poll_active_tile;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_boolean(
      ot->srna, "use_flip_x", false, "Horizontal", "Flip the image horizontally");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "use_flip_y", false, "Vertical", "Flip the image vertically");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rotate Orthogonal Operator (90, 180, 270)
 * \{ */

static wmOperatorStatus image_rotate_orthogonal_exec(bContext *C, wmOperator *op)
{
  Image *ima = image_from_context(C);
  ImageUser iuser = image_user_from_context_and_active_tile(C, ima);
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);
  SpaceImage *sima = CTX_wm_space_image(C);
  const bool is_paint = ((sima != nullptr) && (sima->mode == SI_MODE_PAINT));

  if (ibuf == nullptr) {
    /* TODO: this should actually never happen, but does for render-results -> cleanup. */
    return OPERATOR_CANCELLED;
  }

  int degrees = RNA_enum_get(op->ptr, "degrees");

  ED_image_undo_push_begin_with_image(op->type->name, ima, ibuf, &iuser);

  if (is_paint) {
    ED_imapaint_clear_partial_redraw();
  }

  if (!IMB_rotate_orthogonal(ibuf, degrees)) {
    BKE_image_release_ibuf(ima, ibuf, nullptr);
    return OPERATOR_CANCELLED;
  }

  ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
  BKE_image_mark_dirty(ima, ibuf);

  ED_image_undo_push_end();

  BKE_image_partial_update_mark_full_update(ima);

  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

  DEG_id_tag_update(&ima->id, ID_RECALC_EDITORS);
  BKE_image_release_ibuf(ima, ibuf, nullptr);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_rotate_orthogonal(wmOperatorType *ot)
{
  static const EnumPropertyItem orthogonal_rotation_items[] = {
      {90, "90", 0, "90 Degrees", "Rotate 90 degrees clockwise"},
      {180, "180", 0, "180 Degrees", "Rotate 180 degrees clockwise"},
      {270, "270", 0, "270 Degrees", "Rotate 270 degrees clockwise"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Rotate Image Orthogonal";
  ot->idname = "IMAGE_OT_rotate_orthogonal";
  ot->description = "Rotate the image";

  /* API callbacks. */
  ot->exec = image_rotate_orthogonal_exec;
  ot->poll = image_from_context_editable_has_data_poll_active_tile;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_enum(ot->srna,
                      "degrees",
                      orthogonal_rotation_items,
                      90,
                      "Degrees",
                      "Amount of rotation in degrees (90, 180, 270)");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clipboard Copy Operator
 * \{ */

static wmOperatorStatus image_clipboard_copy_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Image *ima = image_from_context(C);
  if (ima == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (G.is_rendering && ima->source == IMA_SRC_VIEWER) {
    BKE_report(op->reports, RPT_ERROR, "Images cannot be copied while rendering");
    return OPERATOR_CANCELLED;
  }

  ImageUser *iuser = image_user_from_context(C);
  WM_cursor_wait(true);
  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);
  bool changed = false;
  if (ibuf) {
    /* Clipboard uses byte buffer, so match saving an 8 bit PNG for color management. */
    const bool save_as_render = ima->flag & IMA_VIEW_AS_RENDER;

    ImageFormatData image_format;
    BKE_image_format_init_for_write(&image_format, scene, nullptr);
    BKE_image_format_set(&image_format, nullptr, R_IMF_IMTYPE_PNG);
    image_format.depth = R_IMF_CHAN_DEPTH_8;

    ImBuf *colormanaged_ibuf = IMB_colormanagement_imbuf_for_write(
        ibuf, save_as_render, true, &image_format);

    if (WM_clipboard_image_set_byte_buffer(colormanaged_ibuf)) {
      changed = true;
    }

    if (colormanaged_ibuf != ibuf) {
      IMB_freeImBuf(colormanaged_ibuf);
    }
    BKE_image_format_free(&image_format);
  }
  BKE_image_release_ibuf(ima, ibuf, lock);
  WM_cursor_wait(false);

  return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static bool image_clipboard_copy_poll(bContext *C)
{
  if (!image_from_context_has_data_poll(C)) {
    CTX_wm_operator_poll_msg_set(C, "No images available");
    return false;
  }

  return true;
}

void IMAGE_OT_clipboard_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Image";
  ot->idname = "IMAGE_OT_clipboard_copy";
  ot->description = "Copy the image to the clipboard";

  /* API callbacks. */
  ot->exec = image_clipboard_copy_exec;
  ot->poll = image_clipboard_copy_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clipboard Paste Operator
 * \{ */

static wmOperatorStatus image_clipboard_paste_exec(bContext *C, wmOperator *op)
{
  bool changed = false;

  WM_cursor_wait(true);
  ImBuf *ibuf = WM_clipboard_image_get();
  if (ibuf) {
    ED_undo_push_op(C, op);

    Main *bmain = CTX_data_main(C);
    SpaceImage *sima = CTX_wm_space_image(C);
    Image *ima = BKE_image_add_from_imbuf(bmain, ibuf, "Clipboard");
    IMB_freeImBuf(ibuf);

    ED_space_image_set(bmain, sima, ima, false);
    BKE_image_signal(bmain, ima, (sima) ? &sima->iuser : nullptr, IMA_SIGNAL_USER_NEW_IMAGE);
    WM_event_add_notifier(C, NC_IMAGE | NA_ADDED, ima);
    changed = true;
  }
  WM_cursor_wait(false);

  return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static bool image_clipboard_paste_poll(bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  if (!sima) {
    CTX_wm_operator_poll_msg_set(C, "Image Editor not found");
    return false;
  }

  if (!WM_clipboard_image_available()) {
    CTX_wm_operator_poll_msg_set(C, "No compatible images are on the clipboard");
    return false;
  }

  return true;
}

void IMAGE_OT_clipboard_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Image";
  ot->idname = "IMAGE_OT_clipboard_paste";
  ot->description = "Paste new image from the clipboard";

  /* API callbacks. */
  ot->exec = image_clipboard_paste_exec;
  ot->poll = image_clipboard_paste_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Invert Operators
 * \{ */

static wmOperatorStatus image_invert_exec(bContext *C, wmOperator *op)
{
  Image *ima = image_from_context(C);
  ImageUser iuser = image_user_from_context_and_active_tile(C, ima);
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);
  SpaceImage *sima = CTX_wm_space_image(C);
  const bool is_paint = ((sima != nullptr) && (sima->mode == SI_MODE_PAINT));

  /* flags indicate if this channel should be inverted */
  const bool r = RNA_boolean_get(op->ptr, "invert_r");
  const bool g = RNA_boolean_get(op->ptr, "invert_g");
  const bool b = RNA_boolean_get(op->ptr, "invert_b");
  const bool a = RNA_boolean_get(op->ptr, "invert_a");

  size_t i;

  if (ibuf == nullptr) {
    /* TODO: this should actually never happen, but does for render-results -> cleanup */
    return OPERATOR_CANCELLED;
  }

  ED_image_undo_push_begin_with_image(op->type->name, ima, ibuf, &iuser);

  if (is_paint) {
    ED_imapaint_clear_partial_redraw();
  }

  /* TODO: make this into an IMB_invert_channels(ibuf,r,g,b,a) method!? */
  if (ibuf->float_buffer.data) {

    float *fp = ibuf->float_buffer.data;
    for (i = size_t(ibuf->x) * ibuf->y; i > 0; i--, fp += 4) {
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

    if (ibuf->byte_buffer.data) {
      IMB_byte_from_float(ibuf);
    }
  }
  else if (ibuf->byte_buffer.data) {

    uchar *cp = ibuf->byte_buffer.data;
    for (i = size_t(ibuf->x) * ibuf->y; i > 0; i--, cp += 4) {
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
    BKE_image_release_ibuf(ima, ibuf, nullptr);
    return OPERATOR_CANCELLED;
  }

  ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
  BKE_image_mark_dirty(ima, ibuf);

  ED_image_undo_push_end();

  BKE_image_partial_update_mark_full_update(ima);

  DEG_id_tag_update(&ima->id, ID_RECALC_EDITORS);

  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

  BKE_image_release_ibuf(ima, ibuf, nullptr);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_invert(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Invert Channels";
  ot->idname = "IMAGE_OT_invert";
  ot->description = "Invert image's channels";

  /* API callbacks. */
  ot->exec = image_invert_exec;
  ot->poll = image_from_context_editable_has_data_poll_active_tile;

  /* properties */
  prop = RNA_def_boolean(ot->srna, "invert_r", false, "Red", "Invert red channel");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "invert_g", false, "Green", "Invert green channel");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "invert_b", false, "Blue", "Invert blue channel");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "invert_a", false, "Alpha", "Invert alpha channel");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scale Operator
 * \{ */

static wmOperatorStatus image_scale_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Image *ima = image_from_context(C);
  ImageUser iuser = image_user_from_context_and_active_tile(C, ima);
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "size");
  if (!RNA_property_is_set(op->ptr, prop)) {
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);
    const int size[2] = {ibuf->x, ibuf->y};
    RNA_property_int_set_array(op->ptr, prop, size);
    BKE_image_release_ibuf(ima, ibuf, nullptr);
  }
  return WM_operator_props_dialog_popup(
      C, op, 200, IFACE_("Scale Image to New Size"), IFACE_("Resize"));
}

static wmOperatorStatus image_scale_exec(bContext *C, wmOperator *op)
{
  Image *ima = image_from_context(C);
  ImageUser iuser = image_user_from_context_and_active_tile(C, ima);
  SpaceImage *sima = CTX_wm_space_image(C);
  const bool is_paint = ((sima != nullptr) && (sima->mode == SI_MODE_PAINT));

  if (is_paint) {
    ED_imapaint_clear_partial_redraw();
  }

  const bool is_scaling_all = RNA_boolean_get(op->ptr, "all_udims");

  if (!is_scaling_all) {
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);

    if (ibuf == nullptr) {
      /* TODO: this should actually never happen, but does for render-results -> cleanup */
      return OPERATOR_CANCELLED;
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

    ED_image_undo_push_begin_with_image(op->type->name, ima, ibuf, &iuser);
    ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
    IMB_scale(ibuf, size[0], size[1], IMBScaleFilter::Box, false);
    BKE_image_mark_dirty(ima, ibuf);
    BKE_image_release_ibuf(ima, ibuf, nullptr);
    ED_image_undo_push_end();
  }
  else {
    // Ensure that an image buffer can be acquired for all UDIM tiles
    LISTBASE_FOREACH (ImageTile *, current_tile, &ima->tiles) {
      iuser.tile = current_tile->tile_number;

      ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);

      if (ibuf == nullptr) {
        /* TODO: this should actually never happen, but does for render-results -> cleanup */
        return OPERATOR_CANCELLED;
      }

      BKE_image_release_ibuf(ima, ibuf, nullptr);
    }

    ED_image_undo_push_begin_with_image_all_udims(op->type->name, ima, &iuser);
    LISTBASE_FOREACH (ImageTile *, current_tile, &ima->tiles) {
      iuser.tile = current_tile->tile_number;

      ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);

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

      ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
      IMB_scale(ibuf, size[0], size[1], IMBScaleFilter::Box, false);
      BKE_image_mark_dirty(ima, ibuf);
      BKE_image_release_ibuf(ima, ibuf, nullptr);
    }
    ED_image_undo_push_end();
  }

  BKE_image_partial_update_mark_full_update(ima);

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

  /* API callbacks. */
  ot->invoke = image_scale_invoke;
  ot->exec = image_scale_exec;
  ot->poll = image_from_context_editable_has_data_poll_active_tile;

  /* properties */
  RNA_def_int_vector(ot->srna, "size", 2, nullptr, 1, INT_MAX, "Size", "", 1, SHRT_MAX);
  RNA_def_boolean(
      ot->srna, "all_udims", false, "All UDIM Tiles", "Scale all the image's UDIM tiles");

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pack Operator
 * \{ */

static bool image_pack_test(Image *ima, const char **r_error_message)
{
  if (!ima) {
    return false;
  }

  if (!ID_IS_EDITABLE(&ima->id)) {
    *r_error_message = "Image is not editable";
    return false;
  }

  if (ELEM(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
    *r_error_message = "Movies or image sequences do not support packing";
    return false;
  }

  return true;
}

static bool image_pack_poll(bContext *C)
{
  Image *ima = image_from_context(C);
  const char *error_message = nullptr;

  if (image_pack_test(ima, &error_message)) {
    return true;
  }

  if (error_message) {
    CTX_wm_operator_poll_msg_set(C, error_message);
  }
  return false;
}

static wmOperatorStatus image_pack_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Image *ima = image_from_context(C);

  const char *error_message = nullptr;
  if (!image_pack_test(ima, &error_message)) {
    if (error_message) {
      BKE_report(op->reports, RPT_ERROR, error_message);
    }
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

  /* API callbacks. */
  ot->exec = image_pack_exec;
  ot->poll = image_pack_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unpack Operator
 * \{ */

static wmOperatorStatus image_unpack_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Image *ima = image_from_context(C);
  int method = RNA_enum_get(op->ptr, "method");

  /* find the supplied image by name */
  if (RNA_struct_property_is_set(op->ptr, "id")) {
    char imaname[MAX_ID_NAME - 2];
    RNA_string_get(op->ptr, "id", imaname);
    ima = static_cast<Image *>(BLI_findstring(&bmain->images, imaname, offsetof(ID, name) + 2));
    if (!ima) {
      ima = image_from_context(C);
    }
  }

  if (!ima || !BKE_image_has_packedfile(ima)) {
    return OPERATOR_CANCELLED;
  }

  if (!ID_IS_EDITABLE(&ima->id)) {
    BKE_report(op->reports, RPT_ERROR, "Image is not editable");
    return OPERATOR_CANCELLED;
  }

  if (ELEM(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
    BKE_report(op->reports, RPT_ERROR, "Unpacking movies or image sequences not supported");
    return OPERATOR_CANCELLED;
  }

  if (G.fileflags & G_FILE_AUTOPACK) {
    BKE_report(op->reports,
               RPT_WARNING,
               "AutoPack is enabled, so image will be packed again on file save");
  }

  /* XXX BKE_packedfile_unpack_image frees image buffers */
  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  BKE_packedfile_unpack_image(CTX_data_main(C), op->reports, ima, ePF_FileStatus(method));

  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus image_unpack_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Image *ima = image_from_context(C);

  if (RNA_struct_property_is_set(op->ptr, "id")) {
    return image_unpack_exec(C, op);
  }

  if (!ima || !BKE_image_has_packedfile(ima)) {
    return OPERATOR_CANCELLED;
  }

  if (!ID_IS_EDITABLE(&ima->id)) {
    BKE_report(op->reports, RPT_ERROR, "Image is not editable");
    return OPERATOR_CANCELLED;
  }

  if (ELEM(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
    BKE_report(op->reports, RPT_ERROR, "Unpacking movies or image sequences not supported");
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
                  nullptr);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_unpack(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unpack Image";
  ot->description = "Save an image packed in the .blend file to disk";
  ot->idname = "IMAGE_OT_unpack";

  /* API callbacks. */
  ot->exec = image_unpack_exec;
  ot->invoke = image_unpack_invoke;
  ot->poll = image_pack_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(
      ot->srna, "method", rna_enum_unpack_method_items, PF_USE_LOCAL, "Method", "How to unpack");
  /* XXX, weak!, will fail with library, name collisions */
  RNA_def_string(
      ot->srna, "id", nullptr, MAX_ID_NAME - 2, "Image Name", "Image data-block name to unpack");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sample Image Operator
 * \{ */

bool ED_space_image_get_position(SpaceImage *sima,
                                 ARegion *region,
                                 const int mval[2],
                                 float r_fpos[2])
{
  void *lock;
  ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock, 0);

  if (ibuf == nullptr) {
    ED_space_image_release_buffer(sima, ibuf, lock);
    return false;
  }

  UI_view2d_region_to_view(&region->v2d, mval[0], mval[1], &r_fpos[0], &r_fpos[1]);

  ED_space_image_release_buffer(sima, ibuf, lock);
  return true;
}

bool ED_space_image_color_sample(
    SpaceImage *sima, ARegion *region, const int mval[2], float r_col[3], bool *r_is_data)
{
  if (r_is_data) {
    *r_is_data = false;
  }
  if (sima->image == nullptr) {
    return false;
  }
  float uv[2];
  UI_view2d_region_to_view(&region->v2d, mval[0], mval[1], &uv[0], &uv[1]);
  int tile = BKE_image_get_tile_from_pos(sima->image, uv, uv, nullptr);

  void *lock;
  ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock, tile);
  bool ret = false;

  if (ibuf == nullptr) {
    ED_space_image_release_buffer(sima, ibuf, lock);
    return false;
  }

  if (uv[0] >= 0.0f && uv[1] >= 0.0f && uv[0] < 1.0f && uv[1] < 1.0f) {
    const float *fp;
    uchar *cp;
    int x = int(uv[0] * ibuf->x), y = int(uv[1] * ibuf->y);

    CLAMP(x, 0, ibuf->x - 1);
    CLAMP(y, 0, ibuf->y - 1);

    if (ibuf->float_buffer.data) {
      fp = (ibuf->float_buffer.data + (ibuf->channels) * (y * ibuf->x + x));
      copy_v3_v3(r_col, fp);
      ret = true;
    }
    else if (ibuf->byte_buffer.data) {
      cp = ibuf->byte_buffer.data + 4 * (y * ibuf->x + x);
      rgb_uchar_to_float(r_col, cp);
      IMB_colormanagement_colorspace_to_scene_linear_v3(r_col, ibuf->byte_buffer.colorspace);
      ret = true;
    }
  }

  if (r_is_data) {
    *r_is_data = (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA) != 0;
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

  /* API callbacks. */
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

static wmOperatorStatus image_sample_line_exec(bContext *C, wmOperator *op)
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

  if (ibuf == nullptr) {
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

static wmOperatorStatus image_sample_line_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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

  /* API callbacks. */
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Set Curves Point";
  ot->idname = "IMAGE_OT_curves_point_set";
  ot->description = "Set black point or white point for curves";

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* API callbacks. */
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

static wmOperatorStatus image_cycle_render_slot_exec(bContext *C, wmOperator *op)
{
  Image *ima = image_from_context(C);
  const int direction = RNA_boolean_get(op->ptr, "reverse") ? -1 : 1;

  if (!ED_image_slot_cycle(ima, direction)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, nullptr);

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

  /* API callbacks. */
  ot->exec = image_cycle_render_slot_exec;
  ot->poll = image_cycle_render_slot_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;

  RNA_def_boolean(ot->srna, "reverse", false, "Cycle in Reverse", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Render Slot Operator
 * \{ */

static wmOperatorStatus image_clear_render_slot_exec(bContext *C, wmOperator * /*op*/)
{
  Image *ima = image_from_context(C);
  ImageUser *iuser = image_user_from_context(C);

  if (!BKE_image_clear_renderslot(ima, iuser, ima->render_slot)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, nullptr);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_clear_render_slot(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Render Slot";
  ot->idname = "IMAGE_OT_clear_render_slot";
  ot->description = "Clear the currently selected render slot";

  /* API callbacks. */
  ot->exec = image_clear_render_slot_exec;
  ot->poll = image_cycle_render_slot_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Render Slot Operator
 * \{ */

static wmOperatorStatus image_add_render_slot_exec(bContext *C, wmOperator * /*op*/)
{
  Image *ima = image_from_context(C);

  RenderSlot *slot = BKE_image_add_renderslot(ima, nullptr);
  ima->render_slot = BLI_findindex(&ima->renderslots, slot);

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, nullptr);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_add_render_slot(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Render Slot";
  ot->idname = "IMAGE_OT_add_render_slot";
  ot->description = "Add a new render slot";

  /* API callbacks. */
  ot->exec = image_add_render_slot_exec;
  ot->poll = image_cycle_render_slot_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Render Slot Operator
 * \{ */

static wmOperatorStatus image_remove_render_slot_exec(bContext *C, wmOperator * /*op*/)
{
  Image *ima = image_from_context(C);
  ImageUser *iuser = image_user_from_context(C);

  if (!BKE_image_remove_renderslot(ima, iuser, ima->render_slot)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, nullptr);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_remove_render_slot(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Render Slot";
  ot->idname = "IMAGE_OT_remove_render_slot";
  ot->description = "Remove the current render slot";

  /* API callbacks. */
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
    return false;
  }

  return space_image_main_region_poll(C);
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
    const SpaceImage *sima = CTX_wm_space_image(C);
    if (!ED_space_image_show_cache_and_mval_over(sima, region, event->mval)) {
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

void IMAGE_OT_change_frame(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Change Frame";
  ot->idname = "IMAGE_OT_change_frame";
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

/* Reload cached render results... */
/* goes over all scenes, reads render layers */
static wmOperatorStatus image_read_viewlayers_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  Image *ima;

  ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_R_RESULT, "Render Result");
  if (sima->image == nullptr) {
    ED_space_image_set(bmain, sima, ima, false);
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

static wmOperatorStatus render_border_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  Render *re = RE_GetSceneRender(scene);
  SpaceImage *sima = CTX_wm_space_image(C);

  if (re == nullptr) {
    /* Shouldn't happen, but better be safe close to the release. */
    return OPERATOR_CANCELLED;
  }

  /* Get information about the previous render, or current scene if no render yet. */
  int width, height;
  BKE_render_resolution(&scene->r, false, &width, &height);
  const RenderData *rd = ED_space_image_has_buffer(sima) ? RE_engine_get_render_data(re) :
                                                           &scene->r;

  /* Get rectangle from the operator. */
  rctf border;
  WM_operator_properties_border_to_rctf(op, &border);
  UI_view2d_region_to_view_rctf(&region->v2d, &border, &border);

  /* Adjust for cropping. */
  if ((rd->mode & (R_BORDER | R_CROP)) == (R_BORDER | R_CROP)) {
    border.xmin = rd->border.xmin + border.xmin * (rd->border.xmax - rd->border.xmin);
    border.xmax = rd->border.xmin + border.xmax * (rd->border.xmax - rd->border.xmin);
    border.ymin = rd->border.ymin + border.ymin * (rd->border.ymax - rd->border.ymin);
    border.ymax = rd->border.ymin + border.ymax * (rd->border.ymax - rd->border.ymin);
  }

  CLAMP(border.xmin, 0.0f, 1.0f);
  CLAMP(border.ymin, 0.0f, 1.0f);
  CLAMP(border.xmax, 0.0f, 1.0f);
  CLAMP(border.ymax, 0.0f, 1.0f);

  /* Drawing a border surrounding the entire camera view switches off border rendering
   * or the border covers no pixels. */
  if ((border.xmin <= 0.0f && border.xmax >= 1.0f && border.ymin <= 0.0f && border.ymax >= 1.0f) ||
      (border.xmin == border.xmax || border.ymin == border.ymax))
  {
    scene->r.mode &= ~R_BORDER;
  }
  else {
    /* Snap border to pixel boundaries, so drawing a border within a pixel selects that pixel. */
    border.xmin = floorf(border.xmin * width) / width;
    border.xmax = ceilf(border.xmax * width) / width;
    border.ymin = floorf(border.ymin * height) / height;
    border.ymax = ceilf(border.ymax * height) / height;

    /* Set border. */
    scene->r.border = border;
    scene->r.mode |= R_BORDER;
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_render_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Render Region";
  ot->description = "Set the boundaries of the render region and enable render region";
  ot->idname = "IMAGE_OT_render_border";

  /* API callbacks. */
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

static wmOperatorStatus clear_render_border_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  scene->r.mode &= ~R_BORDER;
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  BLI_rctf_init(&scene->r.border, 0.0f, 1.0f, 0.0f, 1.0f);
  return OPERATOR_FINISHED;
}

void IMAGE_OT_clear_render_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Render Region";
  ot->description = "Clear the boundaries of the render region and disable render region";
  ot->idname = "IMAGE_OT_clear_render_border";

  /* API callbacks. */
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
  RNA_float_get_array(ptr, "color", tile->gen_color);
  tile->gen_type = RNA_enum_get(ptr, "generated_type");
  tile->gen_x = RNA_int_get(ptr, "width");
  tile->gen_y = RNA_int_get(ptr, "height");
  bool is_float = RNA_boolean_get(ptr, "float");

  tile->gen_flag = is_float ? IMA_GEN_FLOAT : 0;
  tile->gen_depth = RNA_boolean_get(ptr, "alpha") ? 32 : 24;

  return BKE_image_fill_tile(ima, tile);
}

static void draw_fill_tile(PointerRNA *ptr, uiLayout *layout)
{
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  uiLayout *col = &layout->column(false);
  col->prop(ptr, "color", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "width", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "height", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "alpha", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "generated_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "float", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void tile_fill_init(PointerRNA *ptr, Image *ima, ImageTile *tile)
{
  ImageUser iuser;
  BKE_imageuser_default(&iuser);
  if (tile != nullptr) {
    iuser.tile = tile->tile_number;
  }

  /* Acquire ibuf to get the default values.
   * If the specified tile has no ibuf, try acquiring the main tile instead
   * (unless the specified tile already was the first tile). */
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);
  if (ibuf == nullptr && (tile != nullptr) && (tile != ima->tiles.first)) {
    ibuf = BKE_image_acquire_ibuf(ima, nullptr, nullptr);
  }

  if (ibuf != nullptr) {
    /* Initialize properties from reference tile. */
    RNA_int_set(ptr, "width", ibuf->x);
    RNA_int_set(ptr, "height", ibuf->y);
    RNA_boolean_set(ptr, "float", ibuf->float_buffer.data != nullptr);
    RNA_boolean_set(ptr, "alpha", ibuf->planes > 24);

    BKE_image_release_ibuf(ima, ibuf, nullptr);
  }
}

static void def_fill_tile(StructOrFunctionRNA *srna)
{
  PropertyRNA *prop;
  static const float default_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  prop = RNA_def_float_color(
      srna, "color", 4, nullptr, 0.0f, FLT_MAX, "Color", "Default fill color", 0.0f, 1.0f);
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
      srna, "float", false, "32-bit Float", "Create image with 32-bit floating-point bit depth");
  RNA_def_boolean(srna, "alpha", true, "Alpha", "Create an image with an alpha channel");
}

static bool tile_add_poll(bContext *C)
{
  Image *ima = CTX_data_edit_image(C);

  return (ima != nullptr && ima->source == IMA_SRC_TILED && BKE_image_has_ibuf(ima, nullptr));
}

static wmOperatorStatus tile_add_exec(bContext *C, wmOperator *op)
{
  Image *ima = CTX_data_edit_image(C);

  int start_tile = RNA_int_get(op->ptr, "number");
  int end_tile = start_tile + RNA_int_get(op->ptr, "count") - 1;

  if (start_tile < 1001 || end_tile > IMA_UDIM_MAX) {
    BKE_report(op->reports, RPT_ERROR, "Invalid UDIM index range was specified");
    return OPERATOR_CANCELLED;
  }

  bool fill_tile = RNA_boolean_get(op->ptr, "fill");
  std::string label = RNA_string_get(op->ptr, "label");

  /* BKE_image_add_tile assumes a pre-sorted list of tiles. */
  BKE_image_sort_tiles(ima);

  ImageTile *last_tile_created = nullptr;
  for (int tile_number = start_tile; tile_number <= end_tile; tile_number++) {
    ImageTile *tile = BKE_image_add_tile(ima, tile_number, label.c_str());

    if (tile != nullptr) {
      if (fill_tile) {
        do_fill_tile(op->ptr, ima, tile);
      }

      last_tile_created = tile;
    }
  }

  if (!last_tile_created) {
    BKE_report(op->reports, RPT_WARNING, "No UDIM tiles were created");
    return OPERATOR_CANCELLED;
  }

  ima->active_tile_index = BLI_findindex(&ima->tiles, last_tile_created);

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, nullptr);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus tile_add_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Image *ima = CTX_data_edit_image(C);

  /* Find the first gap in tile numbers or the number after the last if
   * no gap exists. */
  int next_number = 0;
  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    next_number = tile->tile_number + 1;
    if (tile->next == nullptr || tile->next->tile_number > next_number) {
      break;
    }
  }

  ImageTile *tile = static_cast<ImageTile *>(BLI_findlink(&ima->tiles, ima->active_tile_index));
  tile_fill_init(op->ptr, ima, tile);

  RNA_int_set(op->ptr, "number", next_number);
  RNA_int_set(op->ptr, "count", 1);
  RNA_string_set(op->ptr, "label", "");

  return WM_operator_props_dialog_popup(C,
                                        op,
                                        300,
                                        IFACE_("Add Tile to Image"),
                                        CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Add"));
}

static void tile_add_draw(bContext * /*C*/, wmOperator *op)
{
  uiLayout *col;
  uiLayout *layout = op->layout;

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  col = &layout->column(false);
  col->prop(op->ptr, "number", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(op->ptr, "count", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(op->ptr, "label", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "fill", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (RNA_boolean_get(op->ptr, "fill")) {
    draw_fill_tile(op->ptr, layout);
  }
}

void IMAGE_OT_tile_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Tile";
  ot->description = "Adds a tile to the image";
  ot->idname = "IMAGE_OT_tile_add";

  /* API callbacks. */
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
  RNA_def_string(ot->srna, "label", nullptr, 0, "Label", "Optional tile label");
  prop = RNA_def_boolean(ot->srna, "fill", true, "Fill", "Fill new tile with a generated image");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_IMAGE);
  def_fill_tile(ot->srna);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Tile Operator
 * \{ */

static bool tile_remove_poll(bContext *C)
{
  Image *ima = CTX_data_edit_image(C);

  return (ima != nullptr && ima->source == IMA_SRC_TILED && !BLI_listbase_is_single(&ima->tiles));
}

static wmOperatorStatus tile_remove_exec(bContext *C, wmOperator * /*op*/)
{
  Image *ima = CTX_data_edit_image(C);

  ImageTile *tile = static_cast<ImageTile *>(BLI_findlink(&ima->tiles, ima->active_tile_index));
  if (!BKE_image_remove_tile(ima, tile)) {
    return OPERATOR_CANCELLED;
  }

  /* Ensure that the active index is valid. */
  ima->active_tile_index = min_ii(ima->active_tile_index, BLI_listbase_count(&ima->tiles) - 1);

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, nullptr);

  return OPERATOR_FINISHED;
}

void IMAGE_OT_tile_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Tile";
  ot->description = "Removes a tile from the image";
  ot->idname = "IMAGE_OT_tile_remove";

  /* API callbacks. */
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

  if (ima != nullptr && ima->source == IMA_SRC_TILED) {
    /* Filling secondary tiles is only allowed if the primary tile exists. */
    return (ima->active_tile_index == 0) || BKE_image_has_ibuf(ima, nullptr);
  }
  return false;
}

static wmOperatorStatus tile_fill_exec(bContext *C, wmOperator *op)
{
  Image *ima = CTX_data_edit_image(C);

  ImageTile *tile = static_cast<ImageTile *>(BLI_findlink(&ima->tiles, ima->active_tile_index));
  if (!do_fill_tile(op->ptr, ima, tile)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus tile_fill_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  tile_fill_init(op->ptr, CTX_data_edit_image(C), nullptr);

  return WM_operator_props_dialog_popup(C,
                                        op,
                                        300,
                                        IFACE_("Fill Tile With Generated Image"),
                                        CTX_IFACE_(BLT_I18NCONTEXT_ID_IMAGE, "Fill"));
}

static void tile_fill_draw(bContext * /*C*/, wmOperator *op)
{
  draw_fill_tile(op->ptr, op->layout);
}

void IMAGE_OT_tile_fill(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Fill Tile";
  ot->description = "Fill the current tile with a generated image";
  ot->idname = "IMAGE_OT_tile_fill";

  /* API callbacks. */
  ot->poll = tile_fill_poll;
  ot->exec = tile_fill_exec;
  ot->invoke = tile_fill_invoke;
  ot->ui = tile_fill_draw;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  def_fill_tile(ot->srna);
}

/** \} */
