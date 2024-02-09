/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * \brief Functions to paint images in 2D and 3D.
 */

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "DNA_brush_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.hh"
#include "BKE_colorband.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_image.h"
#include "BKE_main.hh"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_node_runtime.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_scene.h"

#include "NOD_texture.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "ED_grease_pencil.hh"
#include "ED_image.hh"
#include "ED_object.hh"
#include "ED_paint.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "IMB_colormanagement.hh"

#include "paint_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Image Paint Tile Utilities (Partial Update)
 * \{ */

/**
 * This is a static resource for non-global access.
 * Maybe it should be exposed as part of the paint operation,
 * but for now just give a public interface.
 */
static ImagePaintPartialRedraw imapaintpartial = {{0}};

ImagePaintPartialRedraw *get_imapaintpartial()
{
  return &imapaintpartial;
}

void set_imapaintpartial(ImagePaintPartialRedraw *ippr)
{
  imapaintpartial = *ippr;
}

/* Image paint Partial Redraw & Dirty Region. */

void ED_imapaint_clear_partial_redraw()
{
  BLI_rcti_init_minmax(&imapaintpartial.dirty_region);
}

void imapaint_region_tiles(
    ImBuf *ibuf, int x, int y, int w, int h, int *tx, int *ty, int *tw, int *th)
{
  int srcx = 0, srcy = 0;

  IMB_rectclip(ibuf, nullptr, &x, &y, &srcx, &srcy, &w, &h);

  *tw = ((x + w - 1) >> ED_IMAGE_UNDO_TILE_BITS);
  *th = ((y + h - 1) >> ED_IMAGE_UNDO_TILE_BITS);
  *tx = (x >> ED_IMAGE_UNDO_TILE_BITS);
  *ty = (y >> ED_IMAGE_UNDO_TILE_BITS);
}

void ED_imapaint_dirty_region(
    Image *ima, ImBuf *ibuf, ImageUser *iuser, int x, int y, int w, int h, bool find_old)
{
  ImBuf *tmpibuf = nullptr;
  int tilex, tiley, tilew, tileh, tx, ty;
  int srcx = 0, srcy = 0;

  IMB_rectclip(ibuf, nullptr, &x, &y, &srcx, &srcy, &w, &h);

  if (w == 0 || h == 0) {
    return;
  }

  rcti rect_to_merge;
  BLI_rcti_init(&rect_to_merge, x, x + w, y, y + h);
  BLI_rcti_do_minmax_rcti(&imapaintpartial.dirty_region, &rect_to_merge);

  imapaint_region_tiles(ibuf, x, y, w, h, &tilex, &tiley, &tilew, &tileh);

  PaintTileMap *undo_tiles = ED_image_paint_tile_map_get();

  for (ty = tiley; ty <= tileh; ty++) {
    for (tx = tilex; tx <= tilew; tx++) {
      ED_image_paint_tile_push(
          undo_tiles, ima, ibuf, &tmpibuf, iuser, tx, ty, nullptr, nullptr, false, find_old);
    }
  }

  BKE_image_mark_dirty(ima, ibuf);

  if (tmpibuf) {
    IMB_freeImBuf(tmpibuf);
  }
}

void imapaint_image_update(
    SpaceImage *sima, Image *image, ImBuf *ibuf, ImageUser *iuser, short texpaint)
{
  if (BLI_rcti_is_empty(&imapaintpartial.dirty_region)) {
    return;
  }

  if (ibuf->mipmap[0]) {
    ibuf->userflags |= IB_MIPMAP_INVALID;
  }

  IMB_partial_display_buffer_update_delayed(ibuf,
                                            imapaintpartial.dirty_region.xmin,
                                            imapaintpartial.dirty_region.ymin,
                                            imapaintpartial.dirty_region.xmax,
                                            imapaintpartial.dirty_region.ymax);

  /* When buffer is partial updated the planes should be set to a larger value than 8. This will
   * make sure that partial updating is working but uses more GPU memory as the gpu texture will
   * have 4 channels. When so the whole texture needs to be reuploaded to the GPU using the new
   * texture format. */
  if (ibuf != nullptr && ibuf->planes == 8) {
    ibuf->planes = 32;
    BKE_image_partial_update_mark_full_update(image);
    return;
  }

  /* TODO: should set_tpage create ->rect? */
  if (texpaint || (sima && sima->lock)) {
    const int w = BLI_rcti_size_x(&imapaintpartial.dirty_region);
    const int h = BLI_rcti_size_y(&imapaintpartial.dirty_region);
    /* Testing with partial update in uv editor too. */
    BKE_image_update_gputexture(
        image, iuser, imapaintpartial.dirty_region.xmin, imapaintpartial.dirty_region.ymin, w, h);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Paint Blur
 * \{ */

BlurKernel *paint_new_blur_kernel(Brush *br, bool proj)
{
  int i, j;
  BlurKernel *kernel = MEM_new<BlurKernel>("BlurKernel");

  float radius;
  int side;
  eBlurKernelType type = static_cast<eBlurKernelType>(br->blur_mode);

  if (proj) {
    radius = 0.5f;

    side = kernel->side = 2;
    kernel->side_squared = kernel->side * kernel->side;
    kernel->wdata = static_cast<float *>(
        MEM_mallocN(sizeof(float) * kernel->side_squared, "blur kernel data"));
    kernel->pixel_len = radius;
  }
  else {
    if (br->blur_kernel_radius <= 0) {
      br->blur_kernel_radius = 1;
    }

    radius = br->blur_kernel_radius;

    side = kernel->side = radius * 2 + 1;
    kernel->side_squared = kernel->side * kernel->side;
    kernel->wdata = static_cast<float *>(
        MEM_mallocN(sizeof(float) * kernel->side_squared, "blur kernel data"));
    kernel->pixel_len = br->blur_kernel_radius;
  }

  switch (type) {
    case KERNEL_BOX:
      for (i = 0; i < kernel->side_squared; i++) {
        kernel->wdata[i] = 1.0;
      }
      break;

    case KERNEL_GAUSSIAN: {
      /* at 3.0 standard deviations distance, kernel is about zero */
      float standard_dev = radius / 3.0f;

      /* make the necessary adjustment to the value for use in the normal distribution formula */
      standard_dev = -standard_dev * standard_dev * 2;

      for (i = 0; i < side; i++) {
        for (j = 0; j < side; j++) {
          float idist = radius - i;
          float jdist = radius - j;
          float value = exp((idist * idist + jdist * jdist) / standard_dev);

          kernel->wdata[i + j * side] = value;
        }
      }

      break;
    }

    default:
      printf("unidentified kernel type, aborting\n");
      paint_delete_blur_kernel(kernel);
      MEM_delete(kernel);
      return nullptr;
  }

  return kernel;
}

void paint_delete_blur_kernel(BlurKernel *kernel)
{
  if (kernel->wdata) {
    MEM_freeN(kernel->wdata);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Paint Poll
 * \{ */

static Brush *image_paint_brush(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *settings = scene->toolsettings;

  return BKE_paint_brush(&settings->imapaint.paint);
}

static bool image_paint_poll_ex(bContext *C, bool check_tool)
{
  Object *obact;

  if (!image_paint_brush(C)) {
    return false;
  }

  obact = CTX_data_active_object(C);
  if ((obact && obact->mode & OB_MODE_TEXTURE_PAINT) && CTX_wm_region_view3d(C)) {
    if (!check_tool || WM_toolsystem_active_tool_is_brush(C)) {
      return true;
    }
  }
  else {
    SpaceImage *sima = CTX_wm_space_image(C);

    if (sima) {
      if (sima->image != nullptr &&
          (ID_IS_LINKED(sima->image) || ID_IS_OVERRIDE_LIBRARY(sima->image)))
      {
        return false;
      }
      if (sima->mode == SI_MODE_PAINT) {
        const ARegion *region = CTX_wm_region(C);
        if (region->regiontype == RGN_TYPE_WINDOW) {
          return true;
        }
      }
    }
  }

  return false;
}

bool ED_image_tools_paint_poll(bContext *C)
{
  return image_paint_poll_ex(C, true);
}

static bool image_paint_poll_ignore_tool(bContext *C)
{
  return image_paint_poll_ex(C, false);
}

static bool image_paint_2d_clone_poll(bContext *C)
{
  Brush *brush = image_paint_brush(C);

  if (!CTX_wm_region_view3d(C) && ED_image_tools_paint_poll(C)) {
    if (brush && (brush->imagepaint_tool == PAINT_TOOL_CLONE)) {
      if (brush->clone.image) {
        return true;
      }
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paint Operator
 * \{ */

bool paint_use_opacity_masking(Brush *brush)
{
  return ((brush->flag & BRUSH_AIRBRUSH) || (brush->flag & BRUSH_DRAG_DOT) ||
                  (brush->flag & BRUSH_ANCHORED) ||
                  ELEM(brush->imagepaint_tool, PAINT_TOOL_SMEAR, PAINT_TOOL_SOFTEN) ||
                  (brush->imagepaint_tool == PAINT_TOOL_FILL) ||
                  (brush->flag & BRUSH_USE_GRADIENT) ||
                  (brush->mtex.tex && !ELEM(brush->mtex.brush_map_mode,
                                            MTEX_MAP_MODE_TILED,
                                            MTEX_MAP_MODE_STENCIL,
                                            MTEX_MAP_MODE_3D)) ?
              false :
              true);
}

void paint_brush_color_get(Scene *scene,
                           Brush *br,
                           bool color_correction,
                           bool invert,
                           float distance,
                           float pressure,
                           float color[3],
                           ColorManagedDisplay *display)
{
  if (invert) {
    copy_v3_v3(color, BKE_brush_secondary_color_get(scene, br));
  }
  else {
    if (br->flag & BRUSH_USE_GRADIENT) {
      float color_gr[4];
      switch (br->gradient_stroke_mode) {
        case BRUSH_GRADIENT_PRESSURE:
          BKE_colorband_evaluate(br->gradient, pressure, color_gr);
          break;
        case BRUSH_GRADIENT_SPACING_REPEAT: {
          float coord = fmod(distance / br->gradient_spacing, 1.0);
          BKE_colorband_evaluate(br->gradient, coord, color_gr);
          break;
        }
        case BRUSH_GRADIENT_SPACING_CLAMP: {
          BKE_colorband_evaluate(br->gradient, distance / br->gradient_spacing, color_gr);
          break;
        }
      }
      /* Gradient / Color-band colors are not considered #PROP_COLOR_GAMMA.
       * Brush colors are expected to be in sRGB though. */
      IMB_colormanagement_scene_linear_to_srgb_v3(color, color_gr);
    }
    else {
      copy_v3_v3(color, BKE_brush_color_get(scene, br));
    }
  }
  if (color_correction) {
    IMB_colormanagement_display_to_scene_linear_v3(color, display);
  }
}

void paint_brush_init_tex(Brush *brush)
{
  /* init mtex nodes */
  if (brush) {
    MTex *mtex = &brush->mtex;
    if (mtex->tex && mtex->tex->nodetree) {
      /* has internal flag to detect it only does it once */
      ntreeTexBeginExecTree(mtex->tex->nodetree);
    }
    mtex = &brush->mask_mtex;
    if (mtex->tex && mtex->tex->nodetree) {
      ntreeTexBeginExecTree(mtex->tex->nodetree);
    }
  }
}

void paint_brush_exit_tex(Brush *brush)
{
  if (brush) {
    MTex *mtex = &brush->mtex;
    if (mtex->tex && mtex->tex->nodetree) {
      ntreeTexEndExecTree(mtex->tex->nodetree->runtime->execdata);
    }
    mtex = &brush->mask_mtex;
    if (mtex->tex && mtex->tex->nodetree) {
      ntreeTexEndExecTree(mtex->tex->nodetree->runtime->execdata);
    }
  }
}

bool get_imapaint_zoom(bContext *C, float *zoomx, float *zoomy)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
    if (sima->mode == SI_MODE_PAINT) {
      ARegion *region = CTX_wm_region(C);
      ED_space_image_get_zoom(sima, region, zoomx, zoomy);
      return true;
    }
  }

  *zoomx = *zoomy = 1;

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor Drawing
 * \{ */

static void toggle_paint_cursor(Scene *scene, bool enable)
{
  ToolSettings *settings = scene->toolsettings;
  Paint *p = &settings->imapaint.paint;

  if (p->paint_cursor && !enable) {
    WM_paint_cursor_end(static_cast<wmPaintCursor *>(p->paint_cursor));
    p->paint_cursor = nullptr;
    paint_cursor_delete_textures();
  }
  else if (enable) {
    ED_paint_cursor_start(p, ED_image_tools_paint_poll);
  }
}

void ED_space_image_paint_update(Main *bmain, wmWindowManager *wm, Scene *scene)
{
  ToolSettings *settings = scene->toolsettings;
  ImagePaintSettings *imapaint = &settings->imapaint;
  bool enabled = false;

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    bScreen *screen = WM_window_get_active_screen(win);

    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->spacetype == SPACE_IMAGE) {
        if (((SpaceImage *)area->spacedata.first)->mode == SI_MODE_PAINT) {
          enabled = true;
        }
      }
    }
  }

  if (enabled) {
    BKE_paint_init(bmain, scene, PaintMode::Texture2D, PAINT_CURSOR_TEXTURE_PAINT);

    ED_paint_cursor_start(&imapaint->paint, ED_image_tools_paint_poll);
  }
  else {
    paint_cursor_delete_textures();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grab Clone Operator
 * \{ */

struct GrabClone {
  float startoffset[2];
  int startx, starty;
};

static void grab_clone_apply(bContext *C, wmOperator *op)
{
  Brush *brush = image_paint_brush(C);
  float delta[2];

  RNA_float_get_array(op->ptr, "delta", delta);
  add_v2_v2(brush->clone.offset, delta);
  ED_region_tag_redraw(CTX_wm_region(C));
}

static int grab_clone_exec(bContext *C, wmOperator *op)
{
  grab_clone_apply(C, op);

  return OPERATOR_FINISHED;
}

static int grab_clone_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Brush *brush = image_paint_brush(C);
  GrabClone *cmv;

  cmv = MEM_new<GrabClone>("GrabClone");
  copy_v2_v2(cmv->startoffset, brush->clone.offset);
  cmv->startx = event->xy[0];
  cmv->starty = event->xy[1];
  op->customdata = cmv;

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int grab_clone_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Brush *brush = image_paint_brush(C);
  ARegion *region = CTX_wm_region(C);
  GrabClone *cmv = static_cast<GrabClone *>(op->customdata);
  float startfx, startfy, fx, fy, delta[2];
  int xmin = region->winrct.xmin, ymin = region->winrct.ymin;

  switch (event->type) {
    case LEFTMOUSE:
    case MIDDLEMOUSE:
    case RIGHTMOUSE: /* XXX hardcoded */
      MEM_freeN(op->customdata);
      return OPERATOR_FINISHED;
    case MOUSEMOVE:
      /* mouse moved, so move the clone image */
      UI_view2d_region_to_view(
          &region->v2d, cmv->startx - xmin, cmv->starty - ymin, &startfx, &startfy);
      UI_view2d_region_to_view(&region->v2d, event->xy[0] - xmin, event->xy[1] - ymin, &fx, &fy);

      delta[0] = fx - startfx;
      delta[1] = fy - startfy;
      RNA_float_set_array(op->ptr, "delta", delta);

      copy_v2_v2(brush->clone.offset, cmv->startoffset);

      grab_clone_apply(C, op);
      break;
  }

  return OPERATOR_RUNNING_MODAL;
}

static void grab_clone_cancel(bContext * /*C*/, wmOperator *op)
{
  GrabClone *cmv = static_cast<GrabClone *>(op->customdata);
  MEM_delete(cmv);
}

void PAINT_OT_grab_clone(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Grab Clone";
  ot->idname = "PAINT_OT_grab_clone";
  ot->description = "Move the clone source image";

  /* api callbacks */
  ot->exec = grab_clone_exec;
  ot->invoke = grab_clone_invoke;
  ot->modal = grab_clone_modal;
  ot->cancel = grab_clone_cancel;
  ot->poll = image_paint_2d_clone_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "delta",
                       2,
                       nullptr,
                       -FLT_MAX,
                       FLT_MAX,
                       "Delta",
                       "Delta offset of clone image in 0.0 to 1.0 coordinates",
                       -1.0f,
                       1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sample Color Operator
 * \{ */

struct SampleColorData {
  bool show_cursor;
  short launch_event;
  float initcolor[3];
  bool sample_palette;
};

static void sample_color_update_header(SampleColorData *data, bContext *C)
{
  char msg[UI_MAX_DRAW_STR];
  ScrArea *area = CTX_wm_area(C);

  if (area) {
    SNPRINTF(msg,
             IFACE_("Sample color for %s"),
             !data->sample_palette ?
                 IFACE_("Brush. Use Left Click to sample for palette instead") :
                 IFACE_("Palette. Use Left Click to sample more colors"));
    ED_workspace_status_text(C, msg);
  }
}

static int sample_color_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  PaintMode mode = BKE_paintmode_get_active_from_context(C);
  ARegion *region = CTX_wm_region(C);
  wmWindow *win = CTX_wm_window(C);
  const bool show_cursor = ((paint->flags & PAINT_SHOW_BRUSH) != 0);
  int location[2];
  paint->flags &= ~PAINT_SHOW_BRUSH;

  /* force redraw without cursor */
  WM_paint_cursor_tag_redraw(win, region);
  WM_redraw_windows(C);

  RNA_int_get_array(op->ptr, "location", location);
  const bool use_palette = RNA_boolean_get(op->ptr, "palette");
  const bool use_sample_texture = (mode == PaintMode::Texture3D) &&
                                  !RNA_boolean_get(op->ptr, "merged");

  paint_sample_color(C, region, location[0], location[1], use_sample_texture, use_palette);

  if (show_cursor) {
    paint->flags |= PAINT_SHOW_BRUSH;
  }

  WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);

  return OPERATOR_FINISHED;
}

static int sample_color_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  SampleColorData *data = MEM_new<SampleColorData>("sample color custom data");
  ARegion *region = CTX_wm_region(C);
  wmWindow *win = CTX_wm_window(C);

  data->launch_event = WM_userdef_event_type_from_keymap_type(event->type);
  data->show_cursor = ((paint->flags & PAINT_SHOW_BRUSH) != 0);
  copy_v3_v3(data->initcolor, BKE_brush_color_get(scene, brush));
  data->sample_palette = false;
  op->customdata = data;
  paint->flags &= ~PAINT_SHOW_BRUSH;

  sample_color_update_header(data, C);

  WM_event_add_modal_handler(C, op);

  /* force redraw without cursor */
  WM_paint_cursor_tag_redraw(win, region);
  WM_redraw_windows(C);

  RNA_int_set_array(op->ptr, "location", event->mval);

  PaintMode mode = BKE_paintmode_get_active_from_context(C);
  const bool use_sample_texture = (mode == PaintMode::Texture3D) &&
                                  !RNA_boolean_get(op->ptr, "merged");

  paint_sample_color(C, region, event->mval[0], event->mval[1], use_sample_texture, false);
  WM_cursor_modal_set(win, WM_CURSOR_EYEDROPPER);

  WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);

  return OPERATOR_RUNNING_MODAL;
}

static int sample_color_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  SampleColorData *data = static_cast<SampleColorData *>(op->customdata);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);

  if ((event->type == data->launch_event) && (event->val == KM_RELEASE)) {
    if (data->show_cursor) {
      paint->flags |= PAINT_SHOW_BRUSH;
    }

    if (data->sample_palette) {
      BKE_brush_color_set(scene, brush, data->initcolor);
      RNA_boolean_set(op->ptr, "palette", true);
    }
    WM_cursor_modal_restore(CTX_wm_window(C));
    MEM_delete(data);
    ED_workspace_status_text(C, nullptr);

    return OPERATOR_FINISHED;
  }

  PaintMode mode = BKE_paintmode_get_active_from_context(C);
  const bool use_sample_texture = (mode == PaintMode::Texture3D) &&
                                  !RNA_boolean_get(op->ptr, "merged");

  switch (event->type) {
    case MOUSEMOVE: {
      ARegion *region = CTX_wm_region(C);
      RNA_int_set_array(op->ptr, "location", event->mval);
      paint_sample_color(C, region, event->mval[0], event->mval[1], use_sample_texture, false);
      WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);
      break;
    }

    case LEFTMOUSE:
      if (event->val == KM_PRESS) {
        ARegion *region = CTX_wm_region(C);
        RNA_int_set_array(op->ptr, "location", event->mval);
        paint_sample_color(C, region, event->mval[0], event->mval[1], use_sample_texture, true);
        if (!data->sample_palette) {
          data->sample_palette = true;
          sample_color_update_header(data, C);
        }
        WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);
      }
      break;
  }

  return OPERATOR_RUNNING_MODAL;
}

static bool sample_color_poll(bContext *C)
{
  return (image_paint_poll_ignore_tool(C) || vertex_paint_poll_ignore_tool(C) ||
          blender::ed::greasepencil::grease_pencil_painting_poll(C));
}

void PAINT_OT_sample_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sample Color";
  ot->idname = "PAINT_OT_sample_color";
  ot->description = "Use the mouse to sample a color in the image";

  /* api callbacks */
  ot->exec = sample_color_exec;
  ot->invoke = sample_color_invoke;
  ot->modal = sample_color_modal;
  ot->poll = sample_color_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  PropertyRNA *prop;

  prop = RNA_def_int_vector(
      ot->srna, "location", 2, nullptr, 0, INT_MAX, "Location", "", 0, 16384);
  RNA_def_property_flag(prop, static_cast<PropertyFlag>(PROP_SKIP_SAVE | PROP_HIDDEN));

  RNA_def_boolean(ot->srna, "merged", false, "Sample Merged", "Sample the output display color");
  RNA_def_boolean(ot->srna, "palette", false, "Add to Palette", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture Paint Toggle Operator
 * \{ */

static blender::float3 paint_init_pivot_mesh(Object *ob)
{
  using namespace blender;
  const Mesh *me_eval = BKE_object_get_evaluated_mesh(ob);
  if (!me_eval) {
    me_eval = (const Mesh *)ob->data;
  }

  const std::optional<Bounds<float3>> bounds = me_eval->bounds_min_max();
  if (!bounds) {
    return float3(0.0f);
  }

  return math::midpoint(bounds->min, bounds->max);
}

static blender::float3 paint_init_pivot_curves(Object *ob)
{
  const Curves &curves = *static_cast<const Curves *>(ob->data);
  const std::optional<blender::Bounds<blender::float3>> bounds =
      curves.geometry.wrap().bounds_min_max();
  if (bounds.has_value()) {
    return blender::math::midpoint(bounds->min, bounds->max);
  }
  return blender::float3(0);
}

static blender::float3 paint_init_pivot_grease_pencil(Object *ob, const int frame)
{
  using namespace blender;
  const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(ob->data);
  const std::optional<Bounds<float3>> bounds = grease_pencil.bounds_min_max(frame);
  if (bounds.has_value()) {
    return blender::math::midpoint(bounds->min, bounds->max);
  }
  return float3(0.0f);
}

void paint_init_pivot(Object *ob, Scene *scene)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

  blender::float3 location;
  switch (ob->type) {
    case OB_MESH:
      location = paint_init_pivot_mesh(ob);
      break;
    case OB_CURVES:
      location = paint_init_pivot_curves(ob);
      break;
    case OB_GREASE_PENCIL:
      location = paint_init_pivot_grease_pencil(ob, scene->r.cfra);
      break;
    default:
      BLI_assert_unreachable();
      ups->last_stroke_valid = false;
      return;
  }

  mul_m4_v3(ob->object_to_world, location);

  ups->last_stroke_valid = true;
  ups->average_stroke_counter = 1;
  copy_v3_v3(ups->average_stroke_accum, location);
}

void ED_object_texture_paint_mode_enter_ex(Main *bmain,
                                           Scene *scene,
                                           Depsgraph *depsgraph,
                                           Object *ob)
{
  Image *ima = nullptr;
  ImagePaintSettings *imapaint = &scene->toolsettings->imapaint;

  /* This has to stay here to regenerate the texture paint
   * cache in case we are loading a file */
  BKE_texpaint_slots_refresh_object(scene, ob);

  ED_paint_proj_mesh_data_check(scene, ob, nullptr, nullptr, nullptr, nullptr);

  /* entering paint mode also sets image to editors */
  if (imapaint->mode == IMAGEPAINT_MODE_MATERIAL) {
    /* set the current material active paint slot on image editor */
    Material *ma = BKE_object_material_get(ob, ob->actcol);

    if (ma && ma->texpaintslot) {
      ima = ma->texpaintslot[ma->paint_active_slot].ima;
    }
  }
  else if (imapaint->mode == IMAGEPAINT_MODE_IMAGE) {
    ima = imapaint->canvas;
  }

  if (ima) {
    ED_space_image_sync(bmain, ima, false);
  }

  ob->mode |= OB_MODE_TEXTURE_PAINT;

  BKE_paint_init(bmain, scene, PaintMode::Texture3D, PAINT_CURSOR_TEXTURE_PAINT);

  BKE_paint_toolslots_brush_validate(bmain, &imapaint->paint);

  if (U.glreslimit != 0) {
    BKE_image_free_all_gputextures(bmain);
  }
  BKE_image_paint_set_mipmap(bmain, false);

  toggle_paint_cursor(scene, true);

  Mesh *mesh = BKE_mesh_from_object(ob);
  BLI_assert(mesh != nullptr);
  DEG_id_tag_update(&mesh->id, ID_RECALC_COPY_ON_WRITE);

  /* Ensure we have evaluated data for bounding box. */
  BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

  /* Set pivot to bounding box center. */
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  paint_init_pivot(ob_eval ? ob_eval : ob, scene);

  WM_main_add_notifier(NC_SCENE | ND_MODE, scene);
}

void ED_object_texture_paint_mode_enter(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  ED_object_texture_paint_mode_enter_ex(bmain, scene, depsgraph, ob);
}

void ED_object_texture_paint_mode_exit_ex(Main *bmain, Scene *scene, Object *ob)
{
  ob->mode &= ~OB_MODE_TEXTURE_PAINT;

  if (U.glreslimit != 0) {
    BKE_image_free_all_gputextures(bmain);
  }
  BKE_image_paint_set_mipmap(bmain, true);
  toggle_paint_cursor(scene, false);

  Mesh *mesh = BKE_mesh_from_object(ob);
  BLI_assert(mesh != nullptr);
  DEG_id_tag_update(&mesh->id, ID_RECALC_COPY_ON_WRITE);
  WM_main_add_notifier(NC_SCENE | ND_MODE, scene);
}

void ED_object_texture_paint_mode_exit(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  ED_object_texture_paint_mode_exit_ex(bmain, scene, ob);
}

static bool texture_paint_toggle_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || ob->type != OB_MESH) {
    return false;
  }
  if (ob->data == nullptr || ID_IS_LINKED(ob->data) || ID_IS_OVERRIDE_LIBRARY(ob->data)) {
    return false;
  }

  return true;
}

static int texture_paint_toggle_exec(bContext *C, wmOperator *op)
{
  wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  const int mode_flag = OB_MODE_TEXTURE_PAINT;
  const bool is_mode_set = (ob->mode & mode_flag) != 0;

  if (!is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, static_cast<eObjectMode>(mode_flag), op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (ob->mode & mode_flag) {
    ED_object_texture_paint_mode_exit_ex(bmain, scene, ob);
  }
  else {
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
    ED_object_texture_paint_mode_enter_ex(bmain, scene, depsgraph, ob);
  }

  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

  WM_toolsystem_update_from_context_view3d(C);

  return OPERATOR_FINISHED;
}

void PAINT_OT_texture_paint_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Texture Paint Toggle";
  ot->idname = "PAINT_OT_texture_paint_toggle";
  ot->description = "Toggle texture paint mode in 3D view";

  /* api callbacks */
  ot->exec = texture_paint_toggle_exec;
  ot->poll = texture_paint_toggle_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Brush Color Flip Operator
 * \{ */

static int brush_colors_flip_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *br = BKE_paint_brush(paint);

  if (ups->flag & UNIFIED_PAINT_COLOR) {
    swap_v3_v3(ups->rgb, ups->secondary_rgb);
  }
  else if (br) {
    swap_v3_v3(br->rgb, br->secondary_rgb);
  }
  else {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, br);

  return OPERATOR_FINISHED;
}

static bool brush_colors_flip_poll(bContext *C)
{
  if (ED_image_tools_paint_poll(C)) {
    Brush *br = image_paint_brush(C);
    if (ELEM(br->imagepaint_tool, PAINT_TOOL_DRAW, PAINT_TOOL_FILL)) {
      return true;
    }
  }
  else {
    Object *ob = CTX_data_active_object(C);
    if (ob != nullptr) {
      if (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_TEXTURE_PAINT | OB_MODE_SCULPT)) {
        return true;
      }
    }
  }
  return false;
}

void PAINT_OT_brush_colors_flip(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Swap Colors";
  ot->idname = "PAINT_OT_brush_colors_flip";
  ot->description = "Swap primary and secondary brush colors";

  /* api callbacks */
  ot->exec = brush_colors_flip_exec;
  ot->poll = brush_colors_flip_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture Paint Bucket Fill Operator
 * \{ */

void ED_imapaint_bucket_fill(bContext *C, float color[3], wmOperator *op, const int mouse[2])
{
  SpaceImage *sima = CTX_wm_space_image(C);

  if (sima && sima->image) {
    Image *ima = sima->image;

    ED_image_undo_push_begin(op->type->name, PaintMode::Texture2D);

    const float mouse_init[2] = {float(mouse[0]), float(mouse[1])};
    paint_2d_bucket_fill(C, color, nullptr, mouse_init, nullptr, nullptr);

    ED_image_undo_push_end();

    DEG_id_tag_update(&ima->id, 0);
  }
}

static bool texture_paint_poll(bContext *C)
{
  if (texture_paint_toggle_poll(C)) {
    if (CTX_data_active_object(C)->mode & OB_MODE_TEXTURE_PAINT) {
      return true;
    }
  }

  return false;
}

bool image_texture_paint_poll(bContext *C)
{
  return (texture_paint_poll(C) || ED_image_tools_paint_poll(C));
}

bool facemask_paint_poll(bContext *C)
{
  return BKE_paint_select_face_test(CTX_data_active_object(C));
}

bool vert_paint_poll(bContext *C)
{
  return BKE_paint_select_vert_test(CTX_data_active_object(C));
}

bool mask_paint_poll(bContext *C)
{
  return BKE_paint_select_elem_test(CTX_data_active_object(C));
}

/** \} */
