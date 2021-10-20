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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 */

/** \file
 * \ingroup edsculpt
 * \brief Functions to paint images in 2D and 3D.
 */

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_brush_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.h"
#include "BKE_colorband.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_paint.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "ED_image.h"
#include "ED_object.h"
#include "ED_paint.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "IMB_colormanagement.h"

#include "paint_intern.h"

/**
 * This is a static resource for non-global access.
 * Maybe it should be exposed as part of the paint operation,
 * but for now just give a public interface.
 */
static ImagePaintPartialRedraw imapaintpartial = {0, 0, 0, 0, 0};

ImagePaintPartialRedraw *get_imapaintpartial(void)
{
  return &imapaintpartial;
}

void set_imapaintpartial(struct ImagePaintPartialRedraw *ippr)
{
  imapaintpartial = *ippr;
}

/* Imagepaint Partial Redraw & Dirty Region */

void ED_imapaint_clear_partial_redraw(void)
{
  memset(&imapaintpartial, 0, sizeof(imapaintpartial));
}

void imapaint_region_tiles(
    ImBuf *ibuf, int x, int y, int w, int h, int *tx, int *ty, int *tw, int *th)
{
  int srcx = 0, srcy = 0;

  IMB_rectclip(ibuf, NULL, &x, &y, &srcx, &srcy, &w, &h);

  *tw = ((x + w - 1) >> ED_IMAGE_UNDO_TILE_BITS);
  *th = ((y + h - 1) >> ED_IMAGE_UNDO_TILE_BITS);
  *tx = (x >> ED_IMAGE_UNDO_TILE_BITS);
  *ty = (y >> ED_IMAGE_UNDO_TILE_BITS);
}

void ED_imapaint_dirty_region(
    Image *ima, ImBuf *ibuf, ImageUser *iuser, int x, int y, int w, int h, bool find_old)
{
  ImBuf *tmpibuf = NULL;
  int tilex, tiley, tilew, tileh, tx, ty;
  int srcx = 0, srcy = 0;

  IMB_rectclip(ibuf, NULL, &x, &y, &srcx, &srcy, &w, &h);

  if (w == 0 || h == 0) {
    return;
  }

  if (!imapaintpartial.enabled) {
    imapaintpartial.x1 = x;
    imapaintpartial.y1 = y;
    imapaintpartial.x2 = x + w;
    imapaintpartial.y2 = y + h;
    imapaintpartial.enabled = 1;
  }
  else {
    imapaintpartial.x1 = min_ii(imapaintpartial.x1, x);
    imapaintpartial.y1 = min_ii(imapaintpartial.y1, y);
    imapaintpartial.x2 = max_ii(imapaintpartial.x2, x + w);
    imapaintpartial.y2 = max_ii(imapaintpartial.y2, y + h);
  }

  imapaint_region_tiles(ibuf, x, y, w, h, &tilex, &tiley, &tilew, &tileh);

  ListBase *undo_tiles = ED_image_paint_tile_list_get();

  for (ty = tiley; ty <= tileh; ty++) {
    for (tx = tilex; tx <= tilew; tx++) {
      ED_image_paint_tile_push(
          undo_tiles, ima, ibuf, &tmpibuf, iuser, tx, ty, NULL, NULL, false, find_old);
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
  if (imapaintpartial.x1 != imapaintpartial.x2 && imapaintpartial.y1 != imapaintpartial.y2) {
    IMB_partial_display_buffer_update_delayed(
        ibuf, imapaintpartial.x1, imapaintpartial.y1, imapaintpartial.x2, imapaintpartial.y2);
  }

  if (ibuf->mipmap[0]) {
    ibuf->userflags |= IB_MIPMAP_INVALID;
  }

  /* TODO: should set_tpage create ->rect? */
  if (texpaint || (sima && sima->lock)) {
    int w = imapaintpartial.x2 - imapaintpartial.x1;
    int h = imapaintpartial.y2 - imapaintpartial.y1;
    if (w && h) {
      /* Testing with partial update in uv editor too */
      BKE_image_update_gputexture(image, iuser, imapaintpartial.x1, imapaintpartial.y1, w, h);
    }
  }
}

/* paint blur kernels. Projective painting enforces use of a 2x2 kernel due to lagging */
BlurKernel *paint_new_blur_kernel(Brush *br, bool proj)
{
  int i, j;
  BlurKernel *kernel = MEM_mallocN(sizeof(BlurKernel), "blur kernel");
  float radius;
  int side;
  eBlurKernelType type = br->blur_mode;

  if (proj) {
    radius = 0.5f;

    side = kernel->side = 2;
    kernel->side_squared = kernel->side * kernel->side;
    kernel->wdata = MEM_mallocN(sizeof(float) * kernel->side_squared, "blur kernel data");
    kernel->pixel_len = radius;
  }
  else {
    if (br->blur_kernel_radius <= 0) {
      br->blur_kernel_radius = 1;
    }

    radius = br->blur_kernel_radius;

    side = kernel->side = radius * 2 + 1;
    kernel->side_squared = kernel->side * kernel->side;
    kernel->wdata = MEM_mallocN(sizeof(float) * kernel->side_squared, "blur kernel data");
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
      MEM_freeN(kernel->wdata);
      MEM_freeN(kernel);
      return NULL;
  }

  return kernel;
}

void paint_delete_blur_kernel(BlurKernel *kernel)
{
  if (kernel->wdata) {
    MEM_freeN(kernel->wdata);
  }
}

/************************ image paint poll ************************/

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
      if (sima->image != NULL && ID_IS_LINKED(sima->image)) {
        return false;
      }
      ARegion *region = CTX_wm_region(C);

      if ((sima->mode == SI_MODE_PAINT) && region->regiontype == RGN_TYPE_WINDOW) {
        return true;
      }
    }
  }

  return false;
}

static bool image_paint_poll(bContext *C)
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

  if (!CTX_wm_region_view3d(C) && image_paint_poll(C)) {
    if (brush && (brush->imagepaint_tool == PAINT_TOOL_CLONE)) {
      if (brush->clone.image) {
        return true;
      }
    }
  }

  return false;
}

/************************ paint operator ************************/
typedef enum eTexPaintMode {
  PAINT_MODE_2D,
  PAINT_MODE_3D_PROJECT,
} eTexPaintMode;

typedef struct PaintOperation {
  eTexPaintMode mode;

  void *custom_paint;

  float prevmouse[2];
  float startmouse[2];
  double starttime;

  void *cursor;
  ViewContext vc;
} PaintOperation;

bool paint_use_opacity_masking(Brush *brush)
{
  return ((brush->flag & BRUSH_AIRBRUSH) || (brush->flag & BRUSH_DRAG_DOT) ||
                  (brush->flag & BRUSH_ANCHORED) ||
                  (ELEM(brush->imagepaint_tool, PAINT_TOOL_SMEAR, PAINT_TOOL_SOFTEN)) ||
                  (brush->imagepaint_tool == PAINT_TOOL_FILL) ||
                  (brush->flag & BRUSH_USE_GRADIENT) ||
                  (brush->mtex.tex && !ELEM(brush->mtex.brush_map_mode,
                                            MTEX_MAP_MODE_TILED,
                                            MTEX_MAP_MODE_STENCIL,
                                            MTEX_MAP_MODE_3D)) ?
              false :
              true);
}

void paint_brush_color_get(struct Scene *scene,
                           struct Brush *br,
                           bool color_correction,
                           bool invert,
                           float distance,
                           float pressure,
                           float color[3],
                           struct ColorManagedDisplay *display)
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
      /* Gradient / Colorband colors are not considered PROP_COLOR_GAMMA.
       * Brush colors are expected to be in sRGB though. */
      IMB_colormanagement_scene_linear_to_srgb_v3(color_gr);

      copy_v3_v3(color, color_gr);
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
      ntreeTexEndExecTree(mtex->tex->nodetree->execdata);
    }
    mtex = &brush->mask_mtex;
    if (mtex->tex && mtex->tex->nodetree) {
      ntreeTexEndExecTree(mtex->tex->nodetree->execdata);
    }
  }
}

static void gradient_draw_line(bContext *UNUSED(C), int x, int y, void *customdata)
{
  PaintOperation *pop = (PaintOperation *)customdata;

  if (pop) {
    GPU_line_smooth(true);
    GPU_blend(GPU_BLEND_ALPHA);

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);

    ARegion *region = pop->vc.region;

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    GPU_line_width(4.0);
    immUniformColor4ub(0, 0, 0, 255);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2i(pos, x, y);
    immVertex2i(
        pos, pop->startmouse[0] + region->winrct.xmin, pop->startmouse[1] + region->winrct.ymin);
    immEnd();

    GPU_line_width(2.0);
    immUniformColor4ub(255, 255, 255, 255);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2i(pos, x, y);
    immVertex2i(
        pos, pop->startmouse[0] + region->winrct.xmin, pop->startmouse[1] + region->winrct.ymin);
    immEnd();

    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
    GPU_line_smooth(false);
  }
}

static PaintOperation *texture_paint_init(bContext *C, wmOperator *op, const float mouse[2])
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *settings = scene->toolsettings;
  PaintOperation *pop = MEM_callocN(sizeof(PaintOperation), "PaintOperation"); /* caller frees */
  Brush *brush = BKE_paint_brush(&settings->imapaint.paint);
  int mode = RNA_enum_get(op->ptr, "mode");
  ED_view3d_viewcontext_init(C, &pop->vc, depsgraph);

  copy_v2_v2(pop->prevmouse, mouse);
  copy_v2_v2(pop->startmouse, mouse);

  /* initialize from context */
  if (CTX_wm_region_view3d(C)) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Object *ob = OBACT(view_layer);
    bool uvs, mat, tex, stencil;
    if (!ED_paint_proj_mesh_data_check(scene, ob, &uvs, &mat, &tex, &stencil)) {
      ED_paint_data_warning(op->reports, uvs, mat, tex, stencil);
      MEM_freeN(pop);
      WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, NULL);
      return NULL;
    }
    pop->mode = PAINT_MODE_3D_PROJECT;
    pop->custom_paint = paint_proj_new_stroke(C, ob, mouse, mode);
  }
  else {
    pop->mode = PAINT_MODE_2D;
    pop->custom_paint = paint_2d_new_stroke(C, op, mode);
  }

  if (!pop->custom_paint) {
    MEM_freeN(pop);
    return NULL;
  }

  if ((brush->imagepaint_tool == PAINT_TOOL_FILL) && (brush->flag & BRUSH_USE_GRADIENT)) {
    pop->cursor = WM_paint_cursor_activate(
        SPACE_TYPE_ANY, RGN_TYPE_ANY, image_paint_poll, gradient_draw_line, pop);
  }

  settings->imapaint.flag |= IMAGEPAINT_DRAWING;
  ED_image_undo_push_begin(op->type->name, PAINT_MODE_TEXTURE_2D);

  return pop;
}

static void paint_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
  PaintOperation *pop = paint_stroke_mode_data(stroke);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *toolsettings = CTX_data_tool_settings(C);
  UnifiedPaintSettings *ups = &toolsettings->unified_paint_settings;
  Brush *brush = BKE_paint_brush(&toolsettings->imapaint.paint);

  float alphafac = (brush->flag & BRUSH_ACCUMULATE) ? ups->overlap_factor : 1.0f;

  /* initial brush values. Maybe it should be considered moving these to stroke system */
  float startalpha = BKE_brush_alpha_get(scene, brush);

  float mouse[2];
  float pressure;
  float size;
  float distance = paint_stroke_distance_get(stroke);
  int eraser;

  RNA_float_get_array(itemptr, "mouse", mouse);
  pressure = RNA_float_get(itemptr, "pressure");
  eraser = RNA_boolean_get(itemptr, "pen_flip");
  size = RNA_float_get(itemptr, "size");

  /* stroking with fill tool only acts on stroke end */
  if (brush->imagepaint_tool == PAINT_TOOL_FILL) {
    copy_v2_v2(pop->prevmouse, mouse);
    return;
  }

  if (BKE_brush_use_alpha_pressure(brush)) {
    BKE_brush_alpha_set(scene, brush, max_ff(0.0f, startalpha * pressure * alphafac));
  }
  else {
    BKE_brush_alpha_set(scene, brush, max_ff(0.0f, startalpha * alphafac));
  }

  if ((brush->flag & BRUSH_DRAG_DOT) || (brush->flag & BRUSH_ANCHORED)) {
    UndoStack *ustack = CTX_wm_manager(C)->undo_stack;
    ED_image_undo_restore(ustack->step_init);
  }

  if (pop->mode == PAINT_MODE_3D_PROJECT) {
    paint_proj_stroke(
        C, pop->custom_paint, pop->prevmouse, mouse, eraser, pressure, distance, size);
  }
  else {
    paint_2d_stroke(pop->custom_paint, pop->prevmouse, mouse, eraser, pressure, distance, size);
  }

  copy_v2_v2(pop->prevmouse, mouse);

  /* restore brush values */
  BKE_brush_alpha_set(scene, brush, startalpha);
}

static void paint_stroke_redraw(const bContext *C, struct PaintStroke *stroke, bool final)
{
  PaintOperation *pop = paint_stroke_mode_data(stroke);

  if (pop->mode == PAINT_MODE_3D_PROJECT) {
    paint_proj_redraw(C, pop->custom_paint, final);
  }
  else {
    paint_2d_redraw(C, pop->custom_paint, final);
  }
}

static void paint_stroke_done(const bContext *C, struct PaintStroke *stroke)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *toolsettings = scene->toolsettings;
  PaintOperation *pop = paint_stroke_mode_data(stroke);
  Brush *brush = BKE_paint_brush(&toolsettings->imapaint.paint);

  toolsettings->imapaint.flag &= ~IMAGEPAINT_DRAWING;

  if (brush->imagepaint_tool == PAINT_TOOL_FILL) {
    if (brush->flag & BRUSH_USE_GRADIENT) {
      if (pop->mode == PAINT_MODE_2D) {
        paint_2d_gradient_fill(C, brush, pop->startmouse, pop->prevmouse, pop->custom_paint);
      }
      else {
        paint_proj_stroke(C,
                          pop->custom_paint,
                          pop->startmouse,
                          pop->prevmouse,
                          paint_stroke_flipped(stroke),
                          1.0,
                          0.0,
                          BKE_brush_size_get(scene, brush));
        /* two redraws, one for GPU update, one for notification */
        paint_proj_redraw(C, pop->custom_paint, false);
        paint_proj_redraw(C, pop->custom_paint, true);
      }
    }
    else {
      if (pop->mode == PAINT_MODE_2D) {
        float color[3];
        if (paint_stroke_inverted(stroke)) {
          srgb_to_linearrgb_v3_v3(color, BKE_brush_secondary_color_get(scene, brush));
        }
        else {
          srgb_to_linearrgb_v3_v3(color, BKE_brush_color_get(scene, brush));
        }
        paint_2d_bucket_fill(C, color, brush, pop->startmouse, pop->prevmouse, pop->custom_paint);
      }
      else {
        paint_proj_stroke(C,
                          pop->custom_paint,
                          pop->startmouse,
                          pop->prevmouse,
                          paint_stroke_flipped(stroke),
                          1.0,
                          0.0,
                          BKE_brush_size_get(scene, brush));
        /* two redraws, one for GPU update, one for notification */
        paint_proj_redraw(C, pop->custom_paint, false);
        paint_proj_redraw(C, pop->custom_paint, true);
      }
    }
  }
  if (pop->mode == PAINT_MODE_3D_PROJECT) {
    paint_proj_stroke_done(pop->custom_paint);
  }
  else {
    paint_2d_stroke_done(pop->custom_paint);
  }

  if (pop->cursor) {
    WM_paint_cursor_end(pop->cursor);
  }

  ED_image_undo_push_end();

  /* duplicate warning, see texpaint_init */
#if 0
  if (pop->s.warnmultifile) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Image requires 4 color channels to paint: %s",
                pop->s.warnmultifile);
  }
  if (pop->s.warnpackedfile) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Packed MultiLayer files cannot be painted: %s",
                pop->s.warnpackedfile);
  }
#endif
  MEM_freeN(pop);
}

static bool paint_stroke_test_start(bContext *C, wmOperator *op, const float mouse[2])
{
  PaintOperation *pop;

  /* TODO: Should avoid putting this here. Instead, last position should be requested
   * from stroke system. */

  if (!(pop = texture_paint_init(C, op, mouse))) {
    return false;
  }

  paint_stroke_set_mode_data(op->customdata, pop);

  return true;
}

static int paint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;

  op->customdata = paint_stroke_new(C,
                                    op,
                                    NULL,
                                    paint_stroke_test_start,
                                    paint_stroke_update_step,
                                    paint_stroke_redraw,
                                    paint_stroke_done,
                                    event->type);

  if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
    paint_stroke_free(C, op);
    return OPERATOR_FINISHED;
  }
  /* add modal handler */
  WM_event_add_modal_handler(C, op);

  OPERATOR_RETVAL_CHECK(retval);
  BLI_assert(retval == OPERATOR_RUNNING_MODAL);

  return OPERATOR_RUNNING_MODAL;
}

static int paint_exec(bContext *C, wmOperator *op)
{
  PropertyRNA *strokeprop;
  PointerRNA firstpoint;
  float mouse[2];

  strokeprop = RNA_struct_find_property(op->ptr, "stroke");

  if (!RNA_property_collection_lookup_int(op->ptr, strokeprop, 0, &firstpoint)) {
    return OPERATOR_CANCELLED;
  }

  RNA_float_get_array(&firstpoint, "mouse", mouse);

  op->customdata = paint_stroke_new(C,
                                    op,
                                    NULL,
                                    paint_stroke_test_start,
                                    paint_stroke_update_step,
                                    paint_stroke_redraw,
                                    paint_stroke_done,
                                    0);
  /* frees op->customdata */
  return paint_stroke_exec(C, op);
}

void PAINT_OT_image_paint(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Image Paint";
  ot->idname = "PAINT_OT_image_paint";
  ot->description = "Paint a stroke into the image";

  /* api callbacks */
  ot->invoke = paint_invoke;
  ot->modal = paint_stroke_modal;
  ot->exec = paint_exec;
  ot->poll = image_paint_poll;
  ot->cancel = paint_stroke_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING;

  paint_stroke_operator_properties(ot);
}

bool get_imapaint_zoom(bContext *C, float *zoomx, float *zoomy)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = area->spacedata.first;
    if (sima->mode == SI_MODE_PAINT) {
      ARegion *region = CTX_wm_region(C);
      ED_space_image_get_zoom(sima, region, zoomx, zoomy);
      return true;
    }
  }

  *zoomx = *zoomy = 1;

  return false;
}

/************************ cursor drawing *******************************/

static void toggle_paint_cursor(Scene *scene, bool enable)
{
  ToolSettings *settings = scene->toolsettings;
  Paint *p = &settings->imapaint.paint;

  if (p->paint_cursor && !enable) {
    WM_paint_cursor_end(p->paint_cursor);
    p->paint_cursor = NULL;
    paint_cursor_delete_textures();
  }
  else if (enable) {
    paint_cursor_start(p, image_paint_poll);
  }
}

/* enable the paint cursor if it isn't already.
 *
 * purpose is to make sure the paint cursor is shown if paint
 * mode is enabled in the image editor. the paint poll will
 * ensure that the cursor is hidden when not in paint mode */
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
    BKE_paint_init(bmain, scene, PAINT_MODE_TEXTURE_2D, PAINT_CURSOR_TEXTURE_PAINT);

    paint_cursor_start(&imapaint->paint, image_paint_poll);
  }
  else {
    paint_cursor_delete_textures();
  }
}

/************************ grab clone operator ************************/

typedef struct GrabClone {
  float startoffset[2];
  int startx, starty;
} GrabClone;

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

  cmv = MEM_callocN(sizeof(GrabClone), "GrabClone");
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
  GrabClone *cmv = op->customdata;
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

static void grab_clone_cancel(bContext *UNUSED(C), wmOperator *op)
{
  MEM_freeN(op->customdata);
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
                       NULL,
                       -FLT_MAX,
                       FLT_MAX,
                       "Delta",
                       "Delta offset of clone image in 0.0 to 1.0 coordinates",
                       -1.0f,
                       1.0f);
}

/******************** sample color operator ********************/
typedef struct {
  bool show_cursor;
  short launch_event;
  float initcolor[3];
  bool sample_palette;
} SampleColorData;

static void sample_color_update_header(SampleColorData *data, bContext *C)
{
  char msg[UI_MAX_DRAW_STR];
  ScrArea *area = CTX_wm_area(C);

  if (area) {
    BLI_snprintf(msg,
                 sizeof(msg),
                 TIP_("Sample color for %s"),
                 !data->sample_palette ?
                     TIP_("Brush. Use Left Click to sample for palette instead") :
                     TIP_("Palette. Use Left Click to sample more colors"));
    ED_workspace_status_text(C, msg);
  }
}

static int sample_color_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  ePaintMode mode = BKE_paintmode_get_active_from_context(C);
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
  const bool use_sample_texture = (mode == PAINT_MODE_TEXTURE_3D) &&
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
  SampleColorData *data = MEM_mallocN(sizeof(SampleColorData), "sample color custom data");
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

  ePaintMode mode = BKE_paintmode_get_active_from_context(C);
  const bool use_sample_texture = (mode == PAINT_MODE_TEXTURE_3D) &&
                                  !RNA_boolean_get(op->ptr, "merged");

  paint_sample_color(C, region, event->mval[0], event->mval[1], use_sample_texture, false);
  WM_cursor_modal_set(win, WM_CURSOR_EYEDROPPER);

  WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);

  return OPERATOR_RUNNING_MODAL;
}

static int sample_color_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  SampleColorData *data = op->customdata;
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
    MEM_freeN(data);
    ED_workspace_status_text(C, NULL);

    return OPERATOR_FINISHED;
  }

  ePaintMode mode = BKE_paintmode_get_active_from_context(C);
  const bool use_sample_texture = (mode == PAINT_MODE_TEXTURE_3D) &&
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
  return (image_paint_poll_ignore_tool(C) || vertex_paint_poll_ignore_tool(C));
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
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;

  prop = RNA_def_int_vector(ot->srna, "location", 2, NULL, 0, INT_MAX, "Location", "", 0, 16384);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);

  RNA_def_boolean(ot->srna, "merged", 0, "Sample Merged", "Sample the output display color");
  RNA_def_boolean(ot->srna, "palette", 0, "Add to Palette", "");
}

/******************** texture paint toggle operator ********************/

void ED_object_texture_paint_mode_enter_ex(Main *bmain, Scene *scene, Object *ob)
{
  Image *ima = NULL;
  ImagePaintSettings *imapaint = &scene->toolsettings->imapaint;

  /* This has to stay here to regenerate the texture paint
   * cache in case we are loading a file */
  BKE_texpaint_slots_refresh_object(scene, ob);

  ED_paint_proj_mesh_data_check(scene, ob, NULL, NULL, NULL, NULL);

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
    wmWindowManager *wm = bmain->wm.first;
    for (wmWindow *win = wm->windows.first; win; win = win->next) {
      const bScreen *screen = WM_window_get_active_screen(win);
      for (ScrArea *area = screen->areabase.first; area; area = area->next) {
        SpaceLink *sl = area->spacedata.first;
        if (sl->spacetype == SPACE_IMAGE) {
          SpaceImage *sima = (SpaceImage *)sl;

          if (!sima->pin) {
            ED_space_image_set(bmain, sima, ima, true);
          }
        }
      }
    }
  }

  ob->mode |= OB_MODE_TEXTURE_PAINT;

  BKE_paint_init(bmain, scene, PAINT_MODE_TEXTURE_3D, PAINT_CURSOR_TEXTURE_PAINT);

  BKE_paint_toolslots_brush_validate(bmain, &imapaint->paint);

  if (U.glreslimit != 0) {
    BKE_image_free_all_gputextures(bmain);
  }
  BKE_image_paint_set_mipmap(bmain, 0);

  toggle_paint_cursor(scene, true);

  Mesh *me = BKE_mesh_from_object(ob);
  BLI_assert(me != NULL);
  DEG_id_tag_update(&me->id, ID_RECALC_COPY_ON_WRITE);
  WM_main_add_notifier(NC_SCENE | ND_MODE, scene);
}

void ED_object_texture_paint_mode_enter(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  ED_object_texture_paint_mode_enter_ex(bmain, scene, ob);
}

void ED_object_texture_paint_mode_exit_ex(Main *bmain, Scene *scene, Object *ob)
{
  ob->mode &= ~OB_MODE_TEXTURE_PAINT;

  if (U.glreslimit != 0) {
    BKE_image_free_all_gputextures(bmain);
  }
  BKE_image_paint_set_mipmap(bmain, 1);
  toggle_paint_cursor(scene, false);

  Mesh *me = BKE_mesh_from_object(ob);
  BLI_assert(me != NULL);
  DEG_id_tag_update(&me->id, ID_RECALC_COPY_ON_WRITE);
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
  if (ob == NULL || ob->type != OB_MESH) {
    return false;
  }
  if (!ob->data || ID_IS_LINKED(ob->data)) {
    return false;
  }

  return true;
}

static int texture_paint_toggle_exec(bContext *C, wmOperator *op)
{
  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  const int mode_flag = OB_MODE_TEXTURE_PAINT;
  const bool is_mode_set = (ob->mode & mode_flag) != 0;

  if (!is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (ob->mode & mode_flag) {
    ED_object_texture_paint_mode_exit_ex(bmain, scene, ob);
  }
  else {
    ED_object_texture_paint_mode_enter_ex(bmain, scene, ob);
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

static int brush_colors_flip_exec(bContext *C, wmOperator *UNUSED(op))
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
  if (image_paint_poll(C)) {
    Brush *br = image_paint_brush(C);
    if (ELEM(br->imagepaint_tool, PAINT_TOOL_DRAW, PAINT_TOOL_FILL)) {
      return true;
    }
  }
  else {
    Object *ob = CTX_data_active_object(C);
    if (ob != NULL) {
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
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void ED_imapaint_bucket_fill(struct bContext *C,
                             float color[3],
                             wmOperator *op,
                             const int mouse[2])
{
  SpaceImage *sima = CTX_wm_space_image(C);

  if (sima && sima->image) {
    Image *ima = sima->image;

    ED_image_undo_push_begin(op->type->name, PAINT_MODE_TEXTURE_2D);

    const float mouse_init[2] = {mouse[0], mouse[1]};
    paint_2d_bucket_fill(C, color, NULL, mouse_init, NULL, NULL);

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
  return (texture_paint_poll(C) || image_paint_poll(C));
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
