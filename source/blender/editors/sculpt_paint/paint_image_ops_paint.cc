/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * \brief Painting operator to paint in 2D and 3D.
 */

#include "DNA_brush_types.h"
#include "DNA_color_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_brush.hh"
#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_paint.hh"
#include "BKE_undo_system.h"

#include "ED_paint.h"
#include "ED_view3d.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_image.h"

#include "paint_intern.hh"

namespace blender::ed::sculpt_paint::image::ops::paint {

/**
 * Interface to use the same painting operator for 3D and 2D painting. Interface removes the
 * differences between the actual calls that are being performed.
 */
class AbstractPaintMode {
 public:
  virtual ~AbstractPaintMode() = default;
  virtual void *paint_new_stroke(
      bContext *C, wmOperator *op, Object *ob, const float mouse[2], int mode) = 0;
  virtual void paint_stroke(bContext *C,
                            void *stroke_handle,
                            float prev_mouse[2],
                            float mouse[2],
                            int eraser,
                            float pressure,
                            float distance,
                            float size) = 0;

  virtual void paint_stroke_redraw(const bContext *C, void *stroke_handle, bool final) = 0;
  virtual void paint_stroke_done(void *stroke_handle) = 0;
  virtual void paint_gradient_fill(const bContext *C,
                                   const Scene *scene,
                                   Brush *brush,
                                   PaintStroke *stroke,
                                   void *stroke_handle,
                                   float mouse_start[2],
                                   float mouse_end[2]) = 0;
  virtual void paint_bucket_fill(const bContext *C,
                                 const Scene *scene,
                                 Brush *brush,
                                 PaintStroke *stroke,
                                 void *stroke_handle,
                                 float mouse_start[2],
                                 float mouse_end[2]) = 0;
};

class ImagePaintMode : public AbstractPaintMode {
 public:
  void *paint_new_stroke(
      bContext *C, wmOperator *op, Object * /*ob*/, const float /*mouse*/[2], int mode) override
  {
    return paint_2d_new_stroke(C, op, mode);
  }

  void paint_stroke(bContext * /*C*/,
                    void *stroke_handle,
                    float prev_mouse[2],
                    float mouse[2],
                    int eraser,
                    float pressure,
                    float distance,
                    float size) override
  {
    paint_2d_stroke(stroke_handle, prev_mouse, mouse, eraser, pressure, distance, size);
  }

  void paint_stroke_redraw(const bContext *C, void *stroke_handle, bool final) override
  {
    paint_2d_redraw(C, stroke_handle, final);
  }

  void paint_stroke_done(void *stroke_handle) override
  {
    paint_2d_stroke_done(stroke_handle);
  }

  void paint_gradient_fill(const bContext *C,
                           const Scene * /*scene*/,
                           Brush *brush,
                           PaintStroke * /*stroke*/,
                           void *stroke_handle,
                           float mouse_start[2],
                           float mouse_end[2]) override
  {
    paint_2d_gradient_fill(C, brush, mouse_start, mouse_end, stroke_handle);
  }

  void paint_bucket_fill(const bContext *C,
                         const Scene *scene,
                         Brush *brush,
                         PaintStroke *stroke,
                         void *stroke_handle,
                         float mouse_start[2],
                         float mouse_end[2]) override
  {
    float color[3];
    if (paint_stroke_inverted(stroke)) {
      srgb_to_linearrgb_v3_v3(color, BKE_brush_secondary_color_get(scene, brush));
    }
    else {
      srgb_to_linearrgb_v3_v3(color, BKE_brush_color_get(scene, brush));
    }
    paint_2d_bucket_fill(C, color, brush, mouse_start, mouse_end, stroke_handle);
  }
};

class ProjectionPaintMode : public AbstractPaintMode {
 public:
  void *paint_new_stroke(
      bContext *C, wmOperator * /*op*/, Object *ob, const float mouse[2], int mode) override
  {
    return paint_proj_new_stroke(C, ob, mouse, mode);
  }

  void paint_stroke(bContext *C,
                    void *stroke_handle,
                    float prev_mouse[2],
                    float mouse[2],
                    int eraser,
                    float pressure,
                    float distance,
                    float size) override
  {
    paint_proj_stroke(C, stroke_handle, prev_mouse, mouse, eraser, pressure, distance, size);
  };

  void paint_stroke_redraw(const bContext *C, void *stroke_handle, bool final) override
  {
    paint_proj_redraw(C, stroke_handle, final);
  }

  void paint_stroke_done(void *stroke_handle) override
  {
    paint_proj_stroke_done(stroke_handle);
  }

  void paint_gradient_fill(const bContext *C,
                           const Scene *scene,
                           Brush *brush,
                           PaintStroke *stroke,
                           void *stroke_handle,
                           float mouse_start[2],
                           float mouse_end[2]) override
  {
    paint_fill(C, scene, brush, stroke, stroke_handle, mouse_start, mouse_end);
  }

  void paint_bucket_fill(const bContext *C,
                         const Scene *scene,
                         Brush *brush,
                         PaintStroke *stroke,
                         void *stroke_handle,
                         float mouse_start[2],
                         float mouse_end[2]) override
  {
    paint_fill(C, scene, brush, stroke, stroke_handle, mouse_start, mouse_end);
  }

 private:
  void paint_fill(const bContext *C,
                  const Scene *scene,
                  Brush *brush,
                  PaintStroke *stroke,
                  void *stroke_handle,
                  float mouse_start[2],
                  float mouse_end[2])
  {
    paint_proj_stroke(C,
                      stroke_handle,
                      mouse_start,
                      mouse_end,
                      paint_stroke_flipped(stroke),
                      1.0,
                      0.0,
                      BKE_brush_size_get(scene, brush));
    /* two redraws, one for GPU update, one for notification */
    paint_proj_redraw(C, stroke_handle, false);
    paint_proj_redraw(C, stroke_handle, true);
  }
};

struct PaintOperation {
  AbstractPaintMode *mode = nullptr;

  void *stroke_handle = nullptr;

  float prevmouse[2] = {0.0f, 0.0f};
  float startmouse[2] = {0.0f, 0.0f};
  double starttime = 0.0;

  wmPaintCursor *cursor = nullptr;
  ViewContext vc = {nullptr};

  PaintOperation() = default;
  ~PaintOperation()
  {
    MEM_delete(mode);
    mode = nullptr;

    if (cursor) {
      WM_paint_cursor_end(cursor);
      cursor = nullptr;
    }
  }
};

static void gradient_draw_line(bContext * /*C*/, int x, int y, void *customdata)
{
  PaintOperation *pop = (PaintOperation *)customdata;

  if (pop) {
    GPU_line_smooth(true);
    GPU_blend(GPU_BLEND_ALPHA);

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);

    ARegion *region = pop->vc.region;

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

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
  PaintOperation *pop = MEM_new<PaintOperation>("PaintOperation"); /* caller frees */
  Brush *brush = BKE_paint_brush(&settings->imapaint.paint);
  int mode = RNA_enum_get(op->ptr, "mode");
  ED_view3d_viewcontext_init(C, &pop->vc, depsgraph);

  copy_v2_v2(pop->prevmouse, mouse);
  copy_v2_v2(pop->startmouse, mouse);

  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  /* initialize from context */
  if (CTX_wm_region_view3d(C)) {
    bool uvs, mat, tex, stencil;
    if (!ED_paint_proj_mesh_data_check(scene, ob, &uvs, &mat, &tex, &stencil)) {
      ED_paint_data_warning(op->reports, uvs, mat, tex, stencil);
      MEM_delete(pop);
      WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, nullptr);
      return nullptr;
    }
    pop->mode = MEM_new<ProjectionPaintMode>("ProjectionPaintMode");
  }
  else {
    pop->mode = MEM_new<ImagePaintMode>("ImagePaintMode");
  }

  pop->stroke_handle = pop->mode->paint_new_stroke(C, op, ob, mouse, mode);
  if (!pop->stroke_handle) {
    MEM_delete(pop);
    return nullptr;
  }

  if ((brush->imagepaint_tool == PAINT_TOOL_FILL) && (brush->flag & BRUSH_USE_GRADIENT)) {
    pop->cursor = WM_paint_cursor_activate(
        SPACE_TYPE_ANY, RGN_TYPE_ANY, ED_image_tools_paint_poll, gradient_draw_line, pop);
  }

  settings->imapaint.flag |= IMAGEPAINT_DRAWING;
  ED_image_undo_push_begin(op->type->name, PAINT_MODE_TEXTURE_2D);

  return pop;
}

static void paint_stroke_update_step(bContext *C,
                                     wmOperator * /*op*/,
                                     PaintStroke *stroke,
                                     PointerRNA *itemptr)
{
  PaintOperation *pop = static_cast<PaintOperation *>(paint_stroke_mode_data(stroke));
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

  pop->mode->paint_stroke(
      C, pop->stroke_handle, pop->prevmouse, mouse, eraser, pressure, distance, size);

  copy_v2_v2(pop->prevmouse, mouse);

  /* restore brush values */
  BKE_brush_alpha_set(scene, brush, startalpha);
}

static void paint_stroke_redraw(const bContext *C, PaintStroke *stroke, bool final)
{
  PaintOperation *pop = static_cast<PaintOperation *>(paint_stroke_mode_data(stroke));
  pop->mode->paint_stroke_redraw(C, pop->stroke_handle, final);
}

static void paint_stroke_done(const bContext *C, PaintStroke *stroke)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *toolsettings = scene->toolsettings;
  PaintOperation *pop = static_cast<PaintOperation *>(paint_stroke_mode_data(stroke));
  Brush *brush = BKE_paint_brush(&toolsettings->imapaint.paint);

  toolsettings->imapaint.flag &= ~IMAGEPAINT_DRAWING;

  if (brush->imagepaint_tool == PAINT_TOOL_FILL) {
    if (brush->flag & BRUSH_USE_GRADIENT) {
      pop->mode->paint_gradient_fill(
          C, scene, brush, stroke, pop->stroke_handle, pop->startmouse, pop->prevmouse);
    }
    else {
      pop->mode->paint_bucket_fill(
          C, scene, brush, stroke, pop->stroke_handle, pop->startmouse, pop->prevmouse);
    }
  }
  pop->mode->paint_stroke_done(pop->stroke_handle);
  pop->stroke_handle = nullptr;

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
  MEM_delete(pop);
}

static bool paint_stroke_test_start(bContext *C, wmOperator *op, const float mouse[2])
{
  PaintOperation *pop;

  /* TODO: Should avoid putting this here. Instead, last position should be requested
   * from stroke system. */

  if (!(pop = texture_paint_init(C, op, mouse))) {
    return false;
  }

  paint_stroke_set_mode_data(static_cast<PaintStroke *>(op->customdata), pop);

  return true;
}

static int paint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;

  op->customdata = paint_stroke_new(C,
                                    op,
                                    nullptr,
                                    paint_stroke_test_start,
                                    paint_stroke_update_step,
                                    paint_stroke_redraw,
                                    paint_stroke_done,
                                    event->type);

  if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
    paint_stroke_free(C, op, static_cast<PaintStroke *>(op->customdata));
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
                                    nullptr,
                                    paint_stroke_test_start,
                                    paint_stroke_update_step,
                                    paint_stroke_redraw,
                                    paint_stroke_done,
                                    0);
  /* frees op->customdata */
  return paint_stroke_exec(C, op, static_cast<PaintStroke *>(op->customdata));
}

static int paint_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, reinterpret_cast<PaintStroke **>(&op->customdata));
}

static void paint_cancel(bContext *C, wmOperator *op)
{
  paint_stroke_cancel(C, op, static_cast<PaintStroke *>(op->customdata));
}
}  // namespace blender::ed::sculpt_paint::image::ops::paint

void PAINT_OT_image_paint(wmOperatorType *ot)
{
  using namespace blender::ed::sculpt_paint::image::ops::paint;

  /* identifiers */
  ot->name = "Image Paint";
  ot->idname = "PAINT_OT_image_paint";
  ot->description = "Paint a stroke into the image";

  /* api callbacks */
  ot->invoke = paint_invoke;
  ot->modal = paint_modal;
  ot->exec = paint_exec;
  ot->poll = ED_image_tools_paint_poll;
  ot->cancel = paint_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING;

  paint_stroke_operator_properties(ot);
}
