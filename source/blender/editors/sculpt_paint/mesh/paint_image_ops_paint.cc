/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * \brief Painting operator to paint in 2D and 3D.
 */

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BLI_math_color_c.hh"
#include "BLI_math_vector_c.hh"

#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_undo_system.hh"

#include "ED_paint.hh"
#include "ED_view3d.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_image.hh"

#include "../paint_intern.hh"

namespace blender {

namespace ed::sculpt_paint::image::ops::paint {

/**
 * Interface to use the same painting operator for 3D and 2D painting. Interface removes the
 * differences between the actual calls that are being performed.
 */
class AbstractPaintMode : public PaintModeData {
 protected:
  void *stroke_handle = nullptr;

 public:
  wmPaintCursor *cursor = nullptr;
  float prev_mouse[2] = {0.0f, 0.0f};
  float start_mouse[2] = {0.0f, 0.0f};

  virtual ~AbstractPaintMode() = default;
  virtual bool paint_new_stroke(bContext *C,
                                wmOperator *op,
                                Object *ob,
                                const float mouse[2],
                                BrushStrokeMode mode,
                                BrushSwitchMode brush_switch_mode) = 0;
  virtual void paint_stroke(
      bContext *C, float mouse[2], int eraser, float pressure, float distance, float size) = 0;

  virtual void paint_stroke_redraw(const bContext *C, bool final) = 0;
  virtual void paint_stroke_done() = 0;
  virtual void paint_gradient_fill(const bContext *C,
                                   const Paint *paint,
                                   Brush *brush,
                                   PaintStroke *stroke) = 0;
  virtual void paint_bucket_fill(const bContext *C,
                                 const Paint *paint,
                                 Brush *brush,
                                 PaintStroke *stroke) = 0;
};

class ImagePaintMode : public AbstractPaintMode {
 public:
  bool paint_new_stroke(bContext *C,
                        wmOperator *op,
                        Object * /*ob*/,
                        const float /*mouse*/[2],
                        const BrushStrokeMode mode,
                        const BrushSwitchMode /*brush_switch_mode*/) override
  {
    stroke_handle = paint_2d_new_stroke(C, op, mode);
    return stroke_handle != nullptr;
  }

  void paint_stroke(bContext * /*C*/,
                    float mouse[2],
                    int eraser,
                    float pressure,
                    float distance,
                    float size) override
  {
    paint_2d_stroke(stroke_handle, prev_mouse, mouse, eraser, pressure, distance, size);
  }

  void paint_stroke_redraw(const bContext *C, bool final) override
  {
    paint_2d_redraw(C, stroke_handle, final);
  }

  void paint_stroke_done() override
  {
    paint_2d_stroke_done(stroke_handle, cursor);
  }

  void paint_gradient_fill(const bContext *C,
                           const Paint * /*paint*/,
                           Brush *brush,
                           PaintStroke * /*stroke*/) override
  {
    paint_2d_gradient_fill(C, brush, start_mouse, prev_mouse, stroke_handle);
  }

  void paint_bucket_fill(const bContext *C,
                         const Paint *paint,
                         Brush *brush,
                         PaintStroke *stroke) override
  {
    float color[3];
    if (stroke->stroke_inverted()) {
      copy_v3_v3(color, BKE_brush_secondary_color_get(paint, brush));
    }
    else {
      copy_v3_v3(color, BKE_brush_color_get(paint, brush));
    }
    paint_2d_bucket_fill(C, color, brush, start_mouse, prev_mouse, stroke_handle);
  }
};

class ProjectionPaintMode : public AbstractPaintMode {
 public:
  bool paint_new_stroke(bContext *C,
                        wmOperator * /*op*/,
                        Object *ob,
                        const float mouse[2],
                        BrushStrokeMode mode,
                        BrushSwitchMode brush_switch_mode) override
  {
    stroke_handle = paint_proj_new_stroke(C, ob, mouse, mode, brush_switch_mode);
    return stroke_handle != nullptr;
  }

  void paint_stroke(
      bContext *C, float mouse[2], int eraser, float pressure, float distance, float size) override
  {
    paint_proj_stroke(C, stroke_handle, prev_mouse, mouse, eraser, pressure, distance, size);
  };

  void paint_stroke_redraw(const bContext *C, bool final) override
  {
    paint_proj_redraw(C, stroke_handle, final);
  }

  void paint_stroke_done() override
  {
    paint_proj_stroke_done(stroke_handle, cursor);
  }

  void paint_gradient_fill(const bContext *C,
                           const Paint *paint,
                           Brush *brush,
                           PaintStroke *stroke) override
  {
    paint_fill(C, paint, brush, stroke, stroke_handle, start_mouse, prev_mouse);
  }

  void paint_bucket_fill(const bContext *C,
                         const Paint *paint,
                         Brush *brush,
                         PaintStroke *stroke) override
  {
    paint_fill(C, paint, brush, stroke, stroke_handle, start_mouse, prev_mouse);
  }

 private:
  void paint_fill(const bContext *C,
                  const Paint *paint,
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
                      stroke->stroke_flipped(),
                      1.0,
                      0.0,
                      BKE_brush_radius_get(paint, brush));
    /* two redraws, one for GPU update, one for notification */
    paint_proj_redraw(C, stroke_handle, false);
    paint_proj_redraw(C, stroke_handle, true);
  }
};

static void gradient_draw_line(bContext *C,
                               const int2 &xy,
                               const float2 & /*tilt*/,
                               void *customdata)
{
  AbstractPaintMode *pop = static_cast<AbstractPaintMode *>(customdata);

  if (pop) {
    GPU_line_smooth(true);
    GPU_blend(GPU_BLEND_ALPHA);

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);

    ARegion *region = CTX_wm_region(C);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    GPU_line_width(4.0);
    immUniformColor4ub(0, 0, 0, 255);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2fv(pos, float2(xy));
    immVertex2f(
        pos, pop->start_mouse[0] + region->winrct.xmin, pop->start_mouse[1] + region->winrct.ymin);
    immEnd();

    GPU_line_width(2.0);
    immUniformColor4ub(255, 255, 255, 255);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2fv(pos, float2(xy));
    immVertex2f(
        pos, pop->start_mouse[0] + region->winrct.xmin, pop->start_mouse[1] + region->winrct.ymin);
    immEnd();

    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
    GPU_line_smooth(false);
  }
}

static std::unique_ptr<AbstractPaintMode> texture_paint_init(bContext *C,
                                                             wmOperator *op,
                                                             const float mouse[2])
{
  CTX_data_ensure_evaluated_depsgraph(C);

  const Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *settings = scene->toolsettings;

  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  std::unique_ptr<AbstractPaintMode> paint_mode;
  if (CTX_wm_region_view3d(C)) {
    bool uvs, mat, tex, stencil;
    if (!ED_paint_proj_mesh_data_check(*scene, *ob, &uvs, &mat, &tex, &stencil)) {
      ED_paint_data_warning(op->reports, uvs, mat, tex, stencil);
      WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, nullptr);
      return nullptr;
    }
    paint_mode = std::make_unique<ProjectionPaintMode>();
  }
  else {
    paint_mode = std::make_unique<ImagePaintMode>();
  }

  Brush *brush = BKE_paint_brush(&settings->imapaint.paint);
  auto mode = BrushStrokeMode(RNA_enum_get(op->ptr, "mode"));
  auto brush_switch_mode = BrushSwitchMode(RNA_enum_get(op->ptr, "brush_toggle"));

  copy_v2_v2(paint_mode->prev_mouse, mouse);
  copy_v2_v2(paint_mode->start_mouse, mouse);

  /* initialize from context */

  const bool started = paint_mode->paint_new_stroke(C, op, ob, mouse, mode, brush_switch_mode);
  if (!started) {
    return nullptr;
  }

  if ((brush->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_FILL) &&
      (brush->flag & BRUSH_USE_GRADIENT))
  {
    paint_mode->cursor = WM_paint_cursor_activate(SPACE_TYPE_ANY,
                                                  RGN_TYPE_ANY,
                                                  ED_image_tools_paint_poll,
                                                  gradient_draw_line,
                                                  paint_mode.get());
  }

  settings->imapaint.flag |= IMAGEPAINT_DRAWING;
  ED_image_undo_push_begin(op->type->name, PaintMode::Texture2D);

  BKE_curvemapping_init(brush->curve_rand_hue);
  BKE_curvemapping_init(brush->curve_rand_saturation);
  BKE_curvemapping_init(brush->curve_rand_value);

  return paint_mode;
}

struct ImagePaintStroke final : public PaintStroke {
  ImagePaintStroke(bContext *C, wmOperator *op, const wmEvent *event) : PaintStroke(C, op, event)
  {
  }

  bool get_location(float location[3], const float mouse[2], bool force_original) override;
  bool test_start(wmOperator *op, const float mouse[2]) override;
  void update_step(wmOperator *op, PointerRNA *itemptr) override;
  void redraw(bool final) override;
  bool test_cancel() override;
  void done(bool is_cancel, bool stroke_started) override;
};

void ImagePaintStroke::update_step(wmOperator *op, PointerRNA *itemptr)
{
  AbstractPaintMode *paint_mode = static_cast<AbstractPaintMode *>(mode_data_.get());
  BLI_assert(paint_mode != nullptr);
  if (paint_mode == nullptr) {
    return;
  }

  Paint *paint = BKE_paint_get_active_from_context(this->evil_C);
  bke::PaintRuntime *paint_runtime = paint->runtime;
  Brush *brush = BKE_paint_brush(paint);

  float alphafac = (brush->flag & BRUSH_ACCUMULATE) ? paint_runtime->overlap_factor : 1.0f;

  /* initial brush values. Maybe it should be considered moving these to stroke system */
  float startalpha = BKE_brush_alpha_get(paint, brush);

  float mouse[2];
  float pressure;
  float size;
  float distance = this->stroke_distance();
  int eraser;

  RNA_float_get_array(itemptr, "mouse", mouse);
  pressure = RNA_float_get(itemptr, "pressure");
  eraser = RNA_boolean_get(op->ptr, "pen_flip");
  size = RNA_float_get(itemptr, "size");

  /* stroking with fill tool only acts on stroke end */
  if (brush->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_FILL) {
    copy_v2_v2(paint_mode->prev_mouse, mouse);
    return;
  }

  if (BKE_brush_use_alpha_pressure(brush)) {
    pressure = BKE_curvemapping_evaluateF(brush->curve_strength, 0, pressure);
    BKE_brush_alpha_set(paint, brush, max_ff(0.0f, startalpha * pressure * alphafac));
  }
  else {
    BKE_brush_alpha_set(paint, brush, max_ff(0.0f, startalpha * alphafac));
  }

  if (ELEM(brush->stroke_method, BRUSH_STROKE_DRAG_DOT, BRUSH_STROKE_ANCHORED)) {
    UndoStack *ustack = CTX_wm_manager(this->evil_C)->runtime->undo_stack;
    ED_image_undo_restore(ustack->step_init);
  }

  paint_mode->paint_stroke(this->evil_C, mouse, eraser, pressure, distance, size);

  copy_v2_v2(paint_mode->prev_mouse, mouse);

  /* restore brush values */
  BKE_brush_alpha_set(paint, brush, startalpha);
}

void ImagePaintStroke::redraw(bool final)
{
  AbstractPaintMode *paint_mode = static_cast<AbstractPaintMode *>(mode_data_.get());
  BLI_assert(paint_mode != nullptr);
  if (paint_mode == nullptr) {
    return;
  }

  paint_mode->paint_stroke_redraw(this->evil_C, final);
}

void ImagePaintStroke::done(const bool is_cancel, const bool /*stroke_started*/)
{
  Scene *scene = CTX_data_scene(this->evil_C);
  ToolSettings *toolsettings = scene->toolsettings;
  AbstractPaintMode *paint_mode = static_cast<AbstractPaintMode *>(mode_data_.get());

  if (!paint_mode) {
    return;
  }

  const Paint *paint = BKE_paint_get_active_from_context(this->evil_C);
  Brush *brush = BKE_paint_brush(&toolsettings->imapaint.paint);

  toolsettings->imapaint.flag &= ~IMAGEPAINT_DRAWING;

  if (brush->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_FILL) {
    if (brush->flag & BRUSH_USE_GRADIENT) {
      paint_mode->paint_gradient_fill(this->evil_C, paint, brush, this);
    }
    else {
      paint_mode->paint_bucket_fill(this->evil_C, paint, brush, this);
    }
  }
  paint_mode->paint_stroke_done();

  if (!is_cancel) {
    ED_image_undo_push_end();
  }
}
bool ImagePaintStroke::get_location(float /*location*/[3],
                                    const float /*mouse*/[2],
                                    bool /*force_original*/)
{
  return true;
}

bool ImagePaintStroke::test_cancel()
{
  return true;
}

bool ImagePaintStroke::test_start(wmOperator *op, const float mouse[2])
{
  std::unique_ptr<AbstractPaintMode> pop;

  if (!(pop = texture_paint_init(this->evil_C, op, mouse))) {
    return false;
  }

  mode_data_ = std::move(pop);

  return true;
}

static wmOperatorStatus paint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ImagePaintStroke *stroke = MEM_new<ImagePaintStroke>(__func__, C, op, event);
  op->customdata = stroke;

  const wmOperatorStatus retval = op->type->modal(C, op, event);
  OPERATOR_RETVAL_CHECK(retval);

  if (retval == OPERATOR_FINISHED) {
    ImagePaintStroke *stroke = static_cast<ImagePaintStroke *>(op->customdata);
    if (stroke) {
      stroke->finish(C);
      MEM_delete(stroke);
    }
    return OPERATOR_FINISHED;
  }
  /* add modal handler */
  WM_event_add_modal_handler(C, op);

  BLI_assert(retval == OPERATOR_RUNNING_MODAL);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus paint_exec(bContext *C, wmOperator *op)
{
  ImagePaintStroke *stroke = MEM_new<ImagePaintStroke>(__func__, C, op, nullptr);
  op->customdata = stroke;

  wmOperatorStatus ret_val = stroke->exec(C, op);

  MEM_delete(stroke);

  return ret_val;
}

static wmOperatorStatus paint_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ImagePaintStroke *stroke = static_cast<ImagePaintStroke *>(op->customdata);
  const wmOperatorStatus retval = stroke->modal(C, op, event);

  if (ELEM(retval, OPERATOR_FINISHED, OPERATOR_CANCELLED)) {
    MEM_delete(stroke);
    op->customdata = nullptr;
  }

  return retval;
}

static void paint_cancel(bContext *C, wmOperator *op)
{
  ImagePaintStroke *stroke = static_cast<ImagePaintStroke *>(op->customdata);
  UndoStack *ustack = CTX_wm_manager(C)->runtime->undo_stack;
  if (ustack->step_init) {
    /* If the user cancels a stroke when none actually started, there is nothing to undo from. */
    ED_image_undo_restore(ustack->step_init);
  }

  stroke->cancel(C);
}
}  // namespace ed::sculpt_paint::image::ops::paint

void PAINT_OT_image_paint(wmOperatorType *ot)
{
  using namespace blender::ed::sculpt_paint::image::ops::paint;

  /* identifiers */
  ot->name = "Image Paint";
  ot->idname = "PAINT_OT_image_paint";
  ot->description = "Paint a stroke into the image";

  /* API callbacks. */
  ot->invoke = paint_invoke;
  ot->modal = paint_modal;
  ot->exec = paint_exec;
  ot->poll = ED_image_tools_paint_poll;
  ot->cancel = paint_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING;

  paint_stroke_operator_properties(ot);
}

}  // namespace blender
