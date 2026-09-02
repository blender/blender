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

/* TODO: Remove this usage in favor of the `PaintStroke` information */
struct PaintGradientData {
  float prev_mouse[2] = {0.0f, 0.0f};
  float start_mouse[2] = {0.0f, 0.0f};
};

static void gradient_draw_line(bContext *C,
                               const int2 &xy,
                               const float2 & /*tilt*/,
                               void *customdata)
{
  PaintGradientData *gradient_data = static_cast<PaintGradientData *>(customdata);

  if (gradient_data) {
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
    immVertex2f(pos,
                gradient_data->start_mouse[0] + region->winrct.xmin,
                gradient_data->start_mouse[1] + region->winrct.ymin);
    immEnd();

    GPU_line_width(2.0);
    immUniformColor4ub(255, 255, 255, 255);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2fv(pos, float2(xy));
    immVertex2f(pos,
                gradient_data->start_mouse[0] + region->winrct.xmin,
                gradient_data->start_mouse[1] + region->winrct.ymin);
    immEnd();

    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
    GPU_line_smooth(false);
  }
}

struct ImagePaintStroke final : public PaintStroke {
 public:
  PaintGradientData gradient_data = {};

 private:
  void *stroke_handle_ = nullptr;
  wmPaintCursor *cursor_ = nullptr;

 public:
  ImagePaintStroke(bContext *C, wmOperator *op, const wmEvent *event) : PaintStroke(C, op, event)
  {
  }

  std::optional<float3> get_location(float2 mouse, bool force_original) override;
  bool test_start(wmOperator *op, float2 mouse) override;
  void update_step(wmOperator *op, const StrokeStep &stroke_step) override;
  void redraw(bool final) override;
  bool test_cancel() override;
  void done(bool is_cancel, bool stroke_started) override;
};

void ImagePaintStroke::update_step(wmOperator *op, const StrokeStep &stroke_step)
{
  Paint *paint = BKE_paint_get_active_from_context(this->evil_C);
  bke::PaintRuntime *paint_runtime = paint->runtime;
  Brush *brush = BKE_paint_brush(paint);

  float alphafac = (brush->flag & BRUSH_ACCUMULATE) ? paint_runtime->overlap_factor : 1.0f;

  /* initial brush values. Maybe it should be considered moving these to stroke system */
  float startalpha = BKE_brush_alpha_get(paint, brush);

  float distance = this->stroke_distance();
  int eraser = RNA_boolean_get(op->ptr, "pen_flip");

  float mouse[2];
  copy_v2_v2(mouse, stroke_step.mouse);
  float pressure = stroke_step.pressure;
  float size = stroke_step.size;

  /* stroking with fill tool only acts on stroke end */
  if (brush->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_FILL) {
    copy_v2_v2(this->gradient_data.prev_mouse, mouse);
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

  paint_2d_stroke(
      stroke_handle_, this->gradient_data.prev_mouse, mouse, eraser, pressure, distance, size);

  copy_v2_v2(this->gradient_data.prev_mouse, mouse);

  /* restore brush values */
  BKE_brush_alpha_set(paint, brush, startalpha);
}

void ImagePaintStroke::redraw(bool final)
{
  paint_2d_redraw(evil_C, stroke_handle_, final);
}

void ImagePaintStroke::done(const bool is_cancel, const bool /*stroke_started*/)
{
  Scene *scene = CTX_data_scene(this->evil_C);
  ToolSettings *toolsettings = scene->toolsettings;

  const Paint *paint = BKE_paint_get_active_from_context(this->evil_C);
  Brush *brush = BKE_paint_brush(&toolsettings->imapaint.paint);

  if (stroke_handle_ == nullptr) {
    paint_brush_exit_tex(brush);
    return;
  }

  toolsettings->imapaint.flag &= ~IMAGEPAINT_DRAWING;

  if (brush->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_FILL) {
    if (brush->flag & BRUSH_USE_GRADIENT) {
      paint_2d_gradient_fill(evil_C,
                             brush,
                             this->gradient_data.start_mouse,
                             this->gradient_data.prev_mouse,
                             stroke_handle_);
    }
    else {
      float color[3];
      if (this->stroke_inverted()) {
        copy_v3_v3(color, BKE_brush_secondary_color_get(paint, brush));
      }
      else {
        copy_v3_v3(color, BKE_brush_color_get(paint, brush));
      }
      paint_2d_bucket_fill(evil_C,
                           color,
                           brush,
                           this->gradient_data.start_mouse,
                           this->gradient_data.prev_mouse,
                           stroke_handle_);
    }
  }
  paint_2d_stroke_done(stroke_handle_, cursor_);

  if (!is_cancel) {
    ED_image_undo_push_end();
  }
}
std::optional<float3> ImagePaintStroke::get_location(const float2 /*mouse*/,
                                                     bool /*force_original*/)
{
  return std::nullopt;
}

bool ImagePaintStroke::test_cancel()
{
  return true;
}

bool ImagePaintStroke::test_start(wmOperator *op, const float2 mouse)
{
  CTX_data_ensure_evaluated_depsgraph(evil_C);

  const Main *bmain = CTX_data_main(evil_C);
  Scene *scene = CTX_data_scene(evil_C);
  ToolSettings *settings = scene->toolsettings;

  ViewLayer *view_layer = CTX_data_view_layer(evil_C);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);

  BLI_assert(!CTX_wm_region_view3d(evil_C));

  Brush *brush = BKE_paint_brush(&settings->imapaint.paint);
  auto mode = BrushStrokeMode(RNA_enum_get(op->ptr, "mode"));

  copy_v2_v2(this->gradient_data.prev_mouse, mouse);
  copy_v2_v2(this->gradient_data.start_mouse, mouse);

  stroke_handle_ = paint_2d_new_stroke(evil_C, op, mode);
  if (stroke_handle_ == nullptr) {
    return false;
  }

  if ((brush->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_FILL) &&
      (brush->flag & BRUSH_USE_GRADIENT))
  {
    this->cursor_ = WM_paint_cursor_activate(SPACE_TYPE_ANY,
                                             RGN_TYPE_ANY,
                                             ED_image_tools_paint_poll,
                                             gradient_draw_line,
                                             &this->gradient_data);
  }

  settings->imapaint.flag |= IMAGEPAINT_DRAWING;
  ED_image_undo_push_begin(op->type->name, PaintMode::Texture2D);

  BKE_curvemapping_init(brush->curve_rand_hue);
  BKE_curvemapping_init(brush->curve_rand_saturation);
  BKE_curvemapping_init(brush->curve_rand_value);

  return true;
}

struct TexturePaintStroke final : public PaintStroke {
 public:
  PaintGradientData gradient_data = {};

 private:
  void *stroke_handle_ = nullptr;
  wmPaintCursor *cursor_ = nullptr;

 public:
  TexturePaintStroke(bContext *C, wmOperator *op, const wmEvent *event) : PaintStroke(C, op, event)
  {
  }

  std::optional<float3> get_location(float2 mouse, bool force_original) override;
  bool test_start(wmOperator *op, float2 mouse) override;
  void update_step(wmOperator *op, const StrokeStep &stroke_step) override;
  void redraw(bool final) override;
  bool test_cancel() override;
  void done(bool is_cancel, bool stroke_started) override;
};

void TexturePaintStroke::update_step(wmOperator *op, const StrokeStep &stroke_step)
{
  Paint *paint = BKE_paint_get_active_from_context(this->evil_C);
  bke::PaintRuntime *paint_runtime = paint->runtime;
  Brush *brush = BKE_paint_brush(paint);

  float alphafac = (brush->flag & BRUSH_ACCUMULATE) ? paint_runtime->overlap_factor : 1.0f;

  /* initial brush values. Maybe it should be considered moving these to stroke system */
  float startalpha = BKE_brush_alpha_get(paint, brush);

  float distance = this->stroke_distance();
  int eraser = RNA_boolean_get(op->ptr, "pen_flip");

  float mouse[2];
  copy_v2_v2(mouse, stroke_step.mouse);
  float pressure = stroke_step.pressure;
  float size = stroke_step.size;

  /* stroking with fill tool only acts on stroke end */
  if (brush->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_FILL) {
    copy_v2_v2(this->gradient_data.prev_mouse, mouse);
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

  paint_proj_stroke(evil_C,
                    stroke_handle_,
                    this->gradient_data.prev_mouse,
                    mouse,
                    eraser,
                    pressure,
                    distance,
                    size);

  copy_v2_v2(this->gradient_data.prev_mouse, mouse);

  /* restore brush values */
  BKE_brush_alpha_set(paint, brush, startalpha);
}

void TexturePaintStroke::redraw(bool final)
{
  paint_proj_redraw(evil_C, stroke_handle_, final);
}

void TexturePaintStroke::done(const bool is_cancel, const bool /*stroke_started*/)
{
  Scene *scene = CTX_data_scene(this->evil_C);
  ToolSettings *toolsettings = scene->toolsettings;

  const Paint *paint = BKE_paint_get_active_from_context(this->evil_C);
  Brush *brush = BKE_paint_brush(&toolsettings->imapaint.paint);

  if (stroke_handle_ == nullptr) {
    paint_brush_exit_tex(brush);
    return;
  }

  toolsettings->imapaint.flag &= ~IMAGEPAINT_DRAWING;

  if (brush->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_FILL) {
    paint_proj_stroke(evil_C,
                      stroke_handle_,
                      gradient_data.start_mouse,
                      gradient_data.prev_mouse,
                      this->stroke_flipped(),
                      1.0,
                      0.0,
                      BKE_brush_radius_get(paint, brush));
    /* two redraws, one for GPU update, one for notification */
    paint_proj_redraw(evil_C, stroke_handle_, false);
    paint_proj_redraw(evil_C, stroke_handle_, true);
  }
  paint_proj_stroke_done(stroke_handle_, cursor_);

  if (!is_cancel) {
    ED_image_undo_push_end();
  }
}
std::optional<float3> TexturePaintStroke::get_location(const float2 /*mouse*/,
                                                       bool /*force_original*/)
{
  /* TODO: This value is a dummy value and not actually used by the rest of the stroke system,
   * this could be replaced with std::nullopt, but that requires further refactoring. */
  return float3(0.0f, 0.0f, 0.0f);
}

bool TexturePaintStroke::test_cancel()
{
  return true;
}

bool TexturePaintStroke::test_start(wmOperator *op, const float2 mouse)
{
  CTX_data_ensure_evaluated_depsgraph(evil_C);

  const Main *bmain = CTX_data_main(evil_C);
  Scene *scene = CTX_data_scene(evil_C);
  ToolSettings *settings = scene->toolsettings;

  ViewLayer *view_layer = CTX_data_view_layer(evil_C);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  BLI_assert(CTX_wm_view3d(evil_C));

  bool uvs, mat, tex, stencil;
  if (!ED_paint_proj_mesh_data_check(*scene, *ob, &uvs, &mat, &tex, &stencil)) {
    ED_paint_data_warning(op->reports, uvs, mat, tex, stencil);
    WM_event_add_notifier(evil_C, NC_SCENE | ND_TOOLSETTINGS, nullptr);
    return false;
  }

  Brush *brush = BKE_paint_brush(&settings->imapaint.paint);
  auto mode = BrushStrokeMode(RNA_enum_get(op->ptr, "mode"));
  auto brush_switch_mode = BrushSwitchMode(RNA_enum_get(op->ptr, "brush_toggle"));

  copy_v2_v2(this->gradient_data.prev_mouse, mouse);
  copy_v2_v2(this->gradient_data.start_mouse, mouse);

  /* initialize from context */

  stroke_handle_ = paint_proj_new_stroke(evil_C, ob, mouse, mode, brush_switch_mode);
  if (stroke_handle_ == nullptr) {
    return false;
  }

  if ((brush->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_FILL) &&
      (brush->flag & BRUSH_USE_GRADIENT))
  {
    cursor_ = WM_paint_cursor_activate(SPACE_TYPE_ANY,
                                       RGN_TYPE_ANY,
                                       ED_image_tools_paint_poll,
                                       gradient_draw_line,
                                       &this->gradient_data);
  }

  settings->imapaint.flag |= IMAGEPAINT_DRAWING;
  ED_image_undo_push_begin(op->type->name, PaintMode::Texture3D);

  BKE_curvemapping_init(brush->curve_rand_hue);
  BKE_curvemapping_init(brush->curve_rand_saturation);
  BKE_curvemapping_init(brush->curve_rand_value);

  return true;
}

static wmOperatorStatus paint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  PaintStroke *stroke = nullptr;
  if (CTX_wm_region_view3d(C)) {
    stroke = MEM_new<TexturePaintStroke>(__func__, C, op, event);
  }
  else {
    stroke = MEM_new<ImagePaintStroke>(__func__, C, op, event);
  }

  op->customdata = stroke;

  const wmOperatorStatus retval = op->type->modal(C, op, event);
  OPERATOR_RETVAL_CHECK(retval);

  if (retval == OPERATOR_FINISHED) {
    if (CTX_wm_region_view3d(C)) {
      stroke = static_cast<TexturePaintStroke *>(op->customdata);
    }
    else {
      stroke = static_cast<ImagePaintStroke *>(op->customdata);
    }
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
  PaintStroke *stroke = nullptr;
  if (CTX_wm_region_view3d(C)) {
    stroke = MEM_new<TexturePaintStroke>(__func__, C, op, nullptr);
  }
  else {
    stroke = MEM_new<ImagePaintStroke>(__func__, C, op, nullptr);
  }
  op->customdata = stroke;

  wmOperatorStatus ret_val = stroke->exec(C, op);

  MEM_delete(stroke);

  return ret_val;
}

static wmOperatorStatus paint_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  PaintStroke *stroke = nullptr;
  if (CTX_wm_region_view3d(C)) {
    stroke = static_cast<TexturePaintStroke *>(op->customdata);
  }
  else {
    stroke = static_cast<ImagePaintStroke *>(op->customdata);
  }
  const wmOperatorStatus retval = stroke->modal(C, op, event);

  if (ELEM(retval, OPERATOR_FINISHED, OPERATOR_CANCELLED)) {
    MEM_delete(stroke);
    op->customdata = nullptr;
  }

  return retval;
}

static void paint_cancel(bContext *C, wmOperator *op)
{
  PaintStroke *stroke = nullptr;
  if (CTX_wm_region_view3d(C)) {
    stroke = static_cast<TexturePaintStroke *>(op->customdata);
  }
  else {
    stroke = static_cast<ImagePaintStroke *>(op->customdata);
  }
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
