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
#include "DNA_windowmanager_types.h"

#include "BLI_listbase_iterator.hh"
#include "BLI_math_color_c.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector_c.hh"

#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_undo_system.hh"

#include "ED_image.hh"
#include "ED_paint.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "image_paint_intern.hh"
#include "mesh_paint.hh"
#include "sculpt_automask.hh"
#include "sculpt_intern.hh"

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
  ImagePaintStroke(bContext *C, wmOperator *op, const wmEvent *event)
      : PaintStroke(C, op, event, PaintMode::Texture2D)
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
  TexturePaintStroke(bContext *C, wmOperator *op, const wmEvent *event)
      : PaintStroke(C, op, event, PaintMode::Texture3D)
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

struct TexturePaintData : public PaintModeData {
  std::unique_ptr<ImageData> image_data;
};

struct ExperimentalTexturePaintStroke final : public PaintStroke {
  Base *base_;
  ImagePaintSettings *settings_;

  ExperimentalTexturePaintStroke(bContext *C, wmOperator *op, const wmEvent *event)
      : PaintStroke(C, op, event, PaintMode::Texture3D)
  {
    base_ = CTX_data_active_base(C);
    ToolSettings *tool_settings = CTX_data_tool_settings(C);
    settings_ = &tool_settings->imapaint;

    if (!G.background) {
      view3d_operator_needs_gpu(C);
    }

    Object &ob = *CTX_data_active_object(C);
    SculptSession &ss = *CTX_data_active_object(C)->runtime->sculpt_session;

    paint_brush_init_tex(this->brush);

    if (!ss.cache) {
      ss.cache = MEM_new<StrokeCache>(__func__);
      const BrushStrokeMode stroke_mode = BrushStrokeMode(RNA_enum_get(op->ptr, "mode"));
      const bool pen_flip = RNA_boolean_get(op->ptr, "pen_flip");

      StrokeToggleSettings toggle_settings;

      toggle_settings.invert = stroke_mode == BrushStrokeMode::Invert || pen_flip;
      ss.cache->toggle_settings = toggle_settings;

      /* TODO: Further toggle support */
    }

    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    BKE_sculptsession_update_for_edit(depsgraph, &ob, true);

    ED_paint_brush_type_update_sticky_shading_color(C, &ob);
  }

  std::optional<float3> get_location(float2 mouse, bool force_original) override;
  bool test_start(wmOperator *op, float2 mouse) override;
  void update_step(wmOperator *op, const StrokeStep &stroke_step) override;
  void redraw(bool final) override;
  bool test_cancel() override;
  void done(bool is_cancel, bool stroke_started) override;
};

std::optional<float3> ExperimentalTexturePaintStroke::get_location(float2 mouse,
                                                                   bool force_original)
{
  return stroke_get_location_bvh(
      *this->depsgraph, this->vc, *this->paint, this->brush, mouse, force_original);
}
bool ExperimentalTexturePaintStroke::test_start(wmOperator *op, float2 mouse)
{
  if (!stroke_get_location_bvh(
          *this->depsgraph, this->vc, *this->paint, this->brush, mouse, false))
  {
    return false;
  }

  SculptSession &ss = *this->object->runtime->sculpt_session;
  StrokeCache &cache = *ss.cache;
  stroke_cache_common_init(this->vc, *this->paint, *this->brush, *this->object, mouse);
  cache.accum = true;

  std::unique_ptr<TexturePaintData> texture_paint_data = std::make_unique<TexturePaintData>();
  texture_paint_data->image_data = ImageData::init_active_image(*this->object, *settings_);
  if (!texture_paint_data->image_data) {
    BLI_assert(0);
    return false;
  }
  mode_data_ = std::move(texture_paint_data);

  if (brush_type_is_paint(this->brush->sculpt_brush_type)) {
    BKE_curvemapping_init(this->brush->curve_rand_hue);
    BKE_curvemapping_init(this->brush->curve_rand_saturation);
    BKE_curvemapping_init(this->brush->curve_rand_value);
  }

  cursor_geometry_info_update(*this->depsgraph, *paint, nullptr, this->vc, base_, mouse, false);

  ED_image_undo_push_begin(op->type->name, PaintMode::Texture3D);

  return true;
};

static void stroke_cache_update(ViewContext & /*vc*/,
                                Paint &paint,
                                Object &object,
                                const PaintStroke::StrokeStep &stroke_step)
{
  bke::PaintRuntime &paint_runtime = *paint.runtime;
  SculptSession &ss = *object.runtime->sculpt_session;
  StrokeCache &cache = *ss.cache;
  Brush &brush = *BKE_paint_brush(&paint);

  if (stroke_is_first_brush_step_of_symmetry_pass(cache) ||
      brush.stroke_method != BRUSH_STROKE_ANCHORED)
  {
    cache.location = stroke_step.location;
  }

  cache.mouse = stroke_step.mouse;
  cache.mouse_event = stroke_step.mouse_event;

  if (paint_supports_dynamic_size(brush, PaintMode::Texture3D) || cache.first_time) {
    cache.pressure = stroke_step.pressure;
  }

  cache.tilt = stroke_step.tilt;

  if (stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    cache.initial_radius = object_space_radius_get(*cache.vc, paint, brush, cache.location);

    if (!BKE_brush_use_locked_size(&paint, &brush)) {
      BKE_brush_unprojected_size_set(&paint, &brush, cache.initial_radius * 2.0f);
    }
  }

  if (BKE_brush_use_size_pressure(&brush) &&
      paint_supports_dynamic_size(brush, PaintMode::Texture3D))
  {
    const float pressure_eval = BKE_curvemapping_evaluateF(brush.curve_size, 0, cache.pressure);
    cache.radius = cache.initial_radius * pressure_eval;
  }
  else if (brush.stroke_method == BRUSH_STROKE_ANCHORED) {
    cache.radius = paint_calc_object_space_radius(
        *cache.vc, cache.location, paint_runtime.pixel_radius);
  }
  else {
    cache.radius = cache.initial_radius;
  }
  cache.radius_squared = cache.radius * cache.radius;

  cache.hardness = brush.hardness;
  /* TODO: Extend the brush "capabilities" checks to handle multi-mode */
  if (brush.paint_flags & BRUSH_PAINT_HARDNESS_PRESSURE) {
    cache.hardness *= BKE_curvemapping_evaluateF(brush.curve_hardness, 0, cache.pressure);
  }

  /* TODO: Brush delta */

  cache.special_rotation = paint_runtime.brush_rotation;
}

static void do_brush_action(const Depsgraph &depsgraph,
                            const Scene &scene,
                            const Brush &brush,
                            Object &ob,
                            PaintModeData *paint_mode_data)
{
  PRF_scope(ProfileCategory::Editor);
  /* TODO: Dynamic brush name */
  ImagePaintSettings &image_paint_settings = scene.toolsettings->imapaint;
  TexturePaintData *mode_data = static_cast<TexturePaintData *>(paint_mode_data);
  SculptSession &ss = *ob.runtime->sculpt_session;
  IndexMaskMemory memory;

  bke::pbvh::build_pixels(
      depsgraph, ob, *mode_data->image_data->image, mode_data->image_data->image_user_get());

  const IndexMask node_mask = gather_brush_nodes(ob, brush, memory, node_fully_masked_or_hidden);

  /* Only act if some verts are inside the brush area. */
  if (node_mask.is_empty()) {
    return;
  }

/* TODO: Automasking support */
#if 0
  if (auto_mask::is_enabled(image_paint_settings.paint, ob, &brush)) {
    auto_mask::Cache &cache = auto_mask::stroke_cache_ensure(depsgraph, image_paint_settings.paint,
  &brush, ob); if (cache.settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
      cache.calc_cavity_factor(depsgraph, ob, node_mask);
    }
  }
#endif

  /* TODO: Sculpt normal */
  /* TODO: Brush local mat */
  /* TODO: Cube tip */

  /* Main brush action */
  switch (brush.image_brush_type) {
    case IMAGE_PAINT_BRUSH_TYPE_DRAW:
      do_3d_image_paint_brush(
          depsgraph, image_paint_settings.paint, brush, ob, *mode_data->image_data, node_mask);
      break;
    default:
      /* TODO: Implement the rest of them... */
      BLI_assert(0);
      break;
  }

  /* Update average stroke position. */
  const float3 world_location = math::project_point(ob.object_to_world(), ss.cache->location);

  bke::paint::stroke_track_location(image_paint_settings.paint, world_location);
}

void ExperimentalTexturePaintStroke::update_step(wmOperator * /*op*/, const StrokeStep &step)
{
  Object &object = *this->object;
  SculptSession &ss = *object.runtime->sculpt_session;
  StrokeCache &cache = *ss.cache;
  TexturePaintData &mode_data = *static_cast<TexturePaintData *>(mode_data_.get());

  cache.stroke_distance = this->stroke_distance();
  stroke_cache_update(this->vc, *this->paint, object, step);

  /* TODO: 'restore' support */

  const bke::PaintRuntime &paint_runtime = *this->paint->runtime;
  const float overlap = paint_runtime.overlap_factor;
  const float pressure = BKE_brush_use_alpha_pressure(brush) ?
                             BKE_curvemapping_evaluateF(brush->curve_strength, 0, cache.pressure) :
                             1.0f;
  /* TODO: Remove hardcoded square pressure, this should be controlled by the user. */
  cache.base_brush_strength = pressure * pressure * overlap;

  do_symmetrical_brush_actions_with_tiling_and_feathering(
      *this->depsgraph, *this->scene, *this->paint, object, do_brush_action, &mode_data);
  cache.first_time = false;

  if (this->vc.rv3d) {
    /* Mark for faster 3D viewport redraws. */
    this->vc.rv3d->rflag |= RV3D_PAINTING;
  }
  ED_region_tag_redraw(this->vc.region);
}
void ExperimentalTexturePaintStroke::redraw(bool /*final*/) {}
bool ExperimentalTexturePaintStroke::test_cancel()
{
  return true;
}
void ExperimentalTexturePaintStroke::done(bool is_cancel, bool stroke_started)
{
  Object &ob = *this->object;
  SculptSession &ss = *ob.runtime->sculpt_session;

  paint_brush_exit_tex(this->brush);

  if (!ss.cache) {
    return;
  }

  if (this->vc.rv3d) {
    this->vc.rv3d->rflag &= ~RV3D_PAINTING;
  }

  MEM_delete(ss.cache);
  ss.cache = nullptr;

  if (!is_cancel && stroke_started) {
    ED_image_undo_push_end();
  }

  if (stroke_started) {
    TexturePaintData *mode_data = static_cast<TexturePaintData *>(mode_data_.get());
    Image *image = mode_data->image_data->image;

    WM_event_add_notifier(this->evil_C, NC_OBJECT | ND_DRAW, &ob);
    WM_event_add_notifier(this->evil_C, NC_IMAGE | NA_PAINTING, image);

    DEG_id_tag_update(&image->id, ID_RECALC_SYNC_TO_EVAL);
  }
}

static PaintStroke *create_paint_stroke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (CTX_wm_region_view3d(C)) {
    auto switch_mode = BrushSwitchMode(RNA_enum_get(op->ptr, "brush_toggle"));

    const Paint &paint = *BKE_paint_get_active_from_context(C);
    const Brush &brush = *BKE_paint_brush_for_read(&paint);
    if (USER_EXPERIMENTAL_TEST(&U, use_3d_texture_paint) &&
        bke::brush::implements_3d_texture_paint(brush) && switch_mode == BrushSwitchMode::None)
    {
      return MEM_new<ExperimentalTexturePaintStroke>(__func__, C, op, event);
    }
    return MEM_new<TexturePaintStroke>(__func__, C, op, event);
  }
  return MEM_new<ImagePaintStroke>(__func__, C, op, event);
}

static PaintStroke *get_paint_stroke(bContext *C, wmOperator *op)
{
  if (CTX_wm_region_view3d(C)) {
    auto switch_mode = BrushSwitchMode(RNA_enum_get(op->ptr, "brush_toggle"));

    const Paint &paint = *BKE_paint_get_active_from_context(C);
    const Brush &brush = *BKE_paint_brush_for_read(&paint);
    if (USER_EXPERIMENTAL_TEST(&U, use_3d_texture_paint) &&
        bke::brush::implements_3d_texture_paint(brush) && switch_mode == BrushSwitchMode::None)
    {
      return static_cast<ExperimentalTexturePaintStroke *>(op->customdata);
    }
    return static_cast<TexturePaintStroke *>(op->customdata);
  }
  return static_cast<ImagePaintStroke *>(op->customdata);
}

static bool should_check_before_stroke_creation(bContext *C, wmOperator *op)
{
  if (CTX_wm_region_view3d(C)) {
    auto switch_mode = BrushSwitchMode(RNA_enum_get(op->ptr, "brush_toggle"));

    const Paint &paint = *BKE_paint_get_active_from_context(C);
    const Brush &brush = *BKE_paint_brush_for_read(&paint);
    if (USER_EXPERIMENTAL_TEST(&U, use_3d_texture_paint) &&
        bke::brush::implements_3d_texture_paint(brush) && switch_mode == BrushSwitchMode::None)
    {
      return true;
    }
  }
  return false;
}

static bool check_preconditions(bContext *C, wmOperator *op)
{
  const Main &bmain = *CTX_data_main(C);
  Scene &scene = *CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(bmain, &scene, view_layer);
  Object &ob = *BKE_view_layer_active_object_get(view_layer);

  bool uvs, mat, tex, stencil;
  if (!ED_paint_proj_mesh_data_check(scene, ob, &uvs, &mat, &tex, &stencil)) {
    ED_paint_data_warning(op->reports, uvs, mat, tex, stencil);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, nullptr);
    return false;
  }
  return true;
}

static wmOperatorStatus paint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (should_check_before_stroke_creation(C, op)) {
    if (!check_preconditions(C, op)) {
      return OPERATOR_CANCELLED;
    }
  }
  PaintStroke *stroke = create_paint_stroke(C, op, event);

  op->customdata = stroke;

  const wmOperatorStatus retval = op->type->modal(C, op, event);
  OPERATOR_RETVAL_CHECK(retval);

  if (retval == OPERATOR_FINISHED) {
    stroke = get_paint_stroke(C, op);
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
  if (should_check_before_stroke_creation(C, op)) {
    if (!check_preconditions(C, op)) {
      return OPERATOR_CANCELLED;
    }
  }

  PaintStroke *stroke = create_paint_stroke(C, op, nullptr);
  op->customdata = stroke;

  wmOperatorStatus ret_val = stroke->exec(C, op);

  MEM_delete(stroke);

  return ret_val;
}

static wmOperatorStatus paint_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  PaintStroke *stroke = get_paint_stroke(C, op);
  const wmOperatorStatus retval = stroke->modal(C, op, event);

  if (ELEM(retval, OPERATOR_FINISHED, OPERATOR_CANCELLED)) {
    MEM_delete(stroke);
    op->customdata = nullptr;
  }

  return retval;
}

static void paint_cancel(bContext *C, wmOperator *op)
{
  PaintStroke *stroke = get_paint_stroke(C, op);
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
  RNA_def_boolean(
      ot->srna,
      "override_location",
      false,
      "Override Location",
      "Override the given \"location\" array by recalculating object space positions from the "
      "provided \"mouse_event\" positions");
}

}  // namespace blender
