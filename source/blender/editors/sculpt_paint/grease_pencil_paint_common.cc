/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"

#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_brush_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "grease_pencil_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

Vector<ed::greasepencil::MutableDrawingInfo> get_drawings_for_stroke_operation(const bContext &C)
{
  using namespace blender::bke::greasepencil;

  const Scene &scene = *CTX_data_scene(&C);
  Object &ob_orig = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob_orig.data);

  /* Apply to all editable drawings. */
  return ed::greasepencil::retrieve_editable_drawings_with_falloff(scene, grease_pencil);
}

Vector<ed::greasepencil::MutableDrawingInfo> get_drawings_with_masking_for_stroke_operation(
    const bContext &C)
{
  using namespace blender::bke::greasepencil;

  const Scene &scene = *CTX_data_scene(&C);
  const ToolSettings &ts = *CTX_data_tool_settings(&C);
  Object &ob_orig = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob_orig.data);

  const bool active_layer_masking = (ts.gp_sculpt.flag &
                                     GP_SCULPT_SETT_FLAG_AUTOMASK_LAYER_ACTIVE) != 0;

  if (active_layer_masking) {
    /* Apply only to the drawing at the current frame of the active layer. */
    if (!grease_pencil.has_active_layer()) {
      return {};
    }
    const Layer &active_layer = *grease_pencil.get_active_layer();
    return ed::greasepencil::retrieve_editable_drawings_from_layer_with_falloff(
        scene, grease_pencil, active_layer);
  }

  /* Apply to all editable drawings. */
  return ed::greasepencil::retrieve_editable_drawings_with_falloff(scene, grease_pencil);
}

void init_brush(Brush &brush)
{
  if (brush.gpencil_settings == nullptr) {
    BKE_brush_init_gpencil_settings(&brush);
  }
  BLI_assert(brush.gpencil_settings != nullptr);
  BKE_curvemapping_init(brush.curve_distance_falloff);
  BKE_curvemapping_init(brush.gpencil_settings->curve_strength);
  BKE_curvemapping_init(brush.gpencil_settings->curve_sensitivity);
  BKE_curvemapping_init(brush.gpencil_settings->curve_jitter);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_pressure);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_strength);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_uv);
  BKE_curvemapping_init(brush.curve_rand_hue);
  BKE_curvemapping_init(brush.curve_rand_saturation);
  BKE_curvemapping_init(brush.curve_rand_value);
}

float brush_radius(const Paint &paint, const Brush &brush, const float pressure = 1.0f)
{
  float radius = BKE_brush_radius_get(&paint, &brush);
  if (BKE_brush_use_size_pressure(&brush)) {
    radius *= BKE_curvemapping_evaluateF(brush.gpencil_settings->curve_sensitivity, 0, pressure);
  }
  return radius;
}

float brush_point_influence(const Paint &paint,
                            const Brush &brush,
                            const float2 &co,
                            const InputSample &sample,
                            const float multi_frame_falloff)
{
  const float radius = brush_radius(paint, brush, sample.pressure);
  /* Basic strength factor from brush settings. */
  const float brush_pressure = BKE_brush_use_alpha_pressure(&brush) ? sample.pressure : 1.0f;
  const float influence_base = BKE_brush_alpha_get(&paint, &brush) * brush_pressure *
                               multi_frame_falloff;

  /* Distance falloff. */
  const float distance = math::distance(sample.mouse_position, co);
  /* Apply Brush curve. */
  const float brush_falloff = BKE_brush_curve_strength(&brush, distance, radius);

  return influence_base * brush_falloff;
}

float closest_distance_to_surface_2d(const float2 pt, const Span<float2> verts)
{
  int j = verts.size() - 1;
  bool isect = false;
  float distance = FLT_MAX;
  for (int i = 0; i < verts.size(); i++) {
    /* Based on implementation of #isect_point_poly_v2. */
    if (((verts[i].y > pt.y) != (verts[j].y > pt.y)) &&
        (pt.x <
         (verts[j].x - verts[i].x) * (pt.y - verts[i].y) / (verts[j].y - verts[i].y) + verts[i].x))
    {
      isect = !isect;
    }
    distance = math::min(distance, math::distance(pt, verts[i]));
    j = i;
  }
  return isect ? 0.0f : distance;
}

float brush_fill_influence(const Paint &paint,
                           const Brush &brush,
                           const Span<float2> fill_positions,
                           const InputSample &sample,
                           const float multi_frame_falloff)
{
  const float radius = brush_radius(paint, brush, sample.pressure);
  /* Basic strength factor from brush settings. */
  const float brush_pressure = BKE_brush_use_alpha_pressure(&brush) ? sample.pressure : 1.0f;
  const float influence_base = BKE_brush_alpha_get(&paint, &brush) * brush_pressure *
                               multi_frame_falloff;

  /* Distance falloff. */
  const float distance = closest_distance_to_surface_2d(sample.mouse_position, fill_positions);
  /* Apply Brush curve. */
  const float brush_falloff = BKE_brush_curve_strength(&brush, distance, radius);

  return influence_base * brush_falloff;
}

IndexMask brush_point_influence_mask(const Paint &paint,
                                     const Brush &brush,
                                     const float2 &mouse_position,
                                     const float pressure,
                                     const float multi_frame_falloff,
                                     const IndexMask &selection,
                                     const Span<float2> view_positions,
                                     Vector<float> &influences,
                                     IndexMaskMemory &memory)
{
  if (selection.is_empty()) {
    return {};
  }

  const float radius = brush_radius(paint, brush, pressure);
  const float radius_squared = radius * radius;
  const float brush_pressure = BKE_brush_use_alpha_pressure(&brush) ? pressure : 1.0f;
  const float influence_base = BKE_brush_alpha_get(&paint, &brush) * brush_pressure *
                               multi_frame_falloff;
  const int2 mval_i = int2(math::round(mouse_position));

  Array<float> all_influences(selection.min_array_size());
  const IndexMask influence_mask = IndexMask::from_predicate(
      selection, GrainSize(4096), memory, [&](const int point) {
        /* Distance falloff. */
        const float distance_squared = math::distance_squared(int2(view_positions[point]), mval_i);
        if (distance_squared > radius_squared) {
          all_influences[point] = 0.0f;
          return false;
        }
        /* Apply Brush curve. */
        const float brush_falloff = BKE_brush_curve_strength(
            &brush, math::sqrt(distance_squared), radius);
        all_influences[point] = influence_base * brush_falloff;
        return all_influences[point] > 0.0f;
      });
  influences.resize(influence_mask.size());
  array_utils::gather(all_influences.as_span(), influence_mask, influences.as_mutable_span());

  return influence_mask;
}

bool brush_using_vertex_color(const GpPaint *gp_paint, const Brush *brush)
{
  const int brush_draw_mode = brush->gpencil_settings->brush_draw_mode;
  const bool brush_use_pinned_mode = (brush_draw_mode != GP_BRUSH_MODE_ACTIVE);
  if (brush_use_pinned_mode) {
    return (brush_draw_mode == GP_BRUSH_MODE_VERTEXCOLOR);
  }
  return (gp_paint->mode == GPPAINT_FLAG_USE_VERTEXCOLOR);
}

bool is_brush_inverted(const Brush &brush, const BrushStrokeMode stroke_mode)
{
  /* The basic setting is the brush's setting. During runtime, the user can hold down the Ctrl key
   * to invert the basic behavior. */
  return bool(brush.flag & BRUSH_DIR_IN) ^ (stroke_mode == BrushStrokeMode::BRUSH_STROKE_INVERT);
}

DeltaProjectionFunc get_screen_projection_fn(const GreasePencilStrokeParams &params,
                                             const Object &object,
                                             const bke::greasepencil::Layer &layer)
{
  const float4x4 view_to_world = float4x4(params.rv3d.viewinv);
  const float4x4 layer_to_world = layer.to_world_space(object);
  const float4x4 world_to_layer = math::invert(layer_to_world);

  auto screen_to_world = [=](const float3 &world_pos, const float2 &screen_delta) {
    const float zfac = ED_view3d_calc_zfac(&params.rv3d, world_pos);
    float3 world_delta;
    ED_view3d_win_to_delta(&params.region, screen_delta, zfac, world_delta);
    return world_delta;
  };

  float3 world_normal;
  switch (params.toolsettings.gp_sculpt.lock_axis) {
    case GP_LOCKAXIS_VIEW: {
      world_normal = view_to_world.z_axis();
      break;
    }
    case GP_LOCKAXIS_X: {
      world_normal = layer_to_world.x_axis();
      break;
    }
    case GP_LOCKAXIS_Y: {
      world_normal = layer_to_world.y_axis();
      break;
    }
    case GP_LOCKAXIS_Z: {
      world_normal = layer_to_world.z_axis();
      break;
    }
    case GP_LOCKAXIS_CURSOR: {
      world_normal = params.scene.cursor.matrix<float3x3>().z_axis();
      break;
    }
    default: {
      BLI_assert_unreachable();
      return [](const float3 &, const float2 &) { return float3(); };
    }
  }

  return [=](const float3 &position, const float2 &screen_delta) {
    const float3 world_pos = math::transform_point(layer_to_world, position);
    const float3 world_delta = screen_to_world(world_pos, screen_delta);
    const float3 layer_delta = math::transform_direction(
        world_to_layer, world_delta - world_normal * math::dot(world_delta, world_normal));
    return position + layer_delta;
  };
}

float3 compute_orig_delta(const DeltaProjectionFunc &projection_fn,
                          const bke::crazyspace::GeometryDeformation &deformation,
                          const int index,
                          const float2 &screen_delta)
{
  const float3 old_position_eval = deformation.positions[index];
  const float3 new_position_eval = projection_fn(old_position_eval, screen_delta);
  const float3 translation_eval = new_position_eval - old_position_eval;
  const float3 translation_orig = deformation.translation_from_deformed_to_original(
      index, translation_eval);
  return translation_orig;
}

GreasePencilStrokeParams GreasePencilStrokeParams::from_context(
    const Scene &scene,
    Depsgraph &depsgraph,
    ARegion &region,
    RegionView3D &rv3d,
    Object &object,
    const int layer_index,
    const int frame_number,
    const float multi_frame_falloff,
    bke::greasepencil::Drawing &drawing)
{
  Object &ob_eval = *DEG_get_evaluated(&depsgraph, &object);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  const bke::greasepencil::Layer &layer = grease_pencil.layer(layer_index);
  return {*scene.toolsettings,
          region,
          rv3d,
          scene,
          object,
          ob_eval,
          layer,
          layer_index,
          frame_number,
          multi_frame_falloff,
          drawing};
}

IndexMask point_mask_for_stroke_operation(const GreasePencilStrokeParams &params,
                                          const bool use_selection_masking,
                                          IndexMaskMemory &memory)
{
  return use_selection_masking ? ed::greasepencil::retrieve_editable_and_selected_points(
                                     params.ob_orig, params.drawing, params.layer_index, memory) :
                                 ed::greasepencil::retrieve_editable_points(
                                     params.ob_orig, params.drawing, params.layer_index, memory);
}

IndexMask curve_mask_for_stroke_operation(const GreasePencilStrokeParams &params,
                                          const bool use_selection_masking,
                                          IndexMaskMemory &memory)
{
  return use_selection_masking ? ed::greasepencil::retrieve_editable_and_selected_strokes(
                                     params.ob_orig, params.drawing, params.layer_index, memory) :
                                 ed::greasepencil::retrieve_editable_strokes(
                                     params.ob_orig, params.drawing, params.layer_index, memory);
}

IndexMask fill_mask_for_stroke_operation(const GreasePencilStrokeParams &params,
                                         const bool use_selection_masking,
                                         IndexMaskMemory &memory)
{
  return use_selection_masking ? ed::greasepencil::retrieve_editable_and_selected_fill_strokes(
                                     params.ob_orig, params.drawing, params.layer_index, memory) :
                                 params.drawing.strokes().curves_range();
}

bke::crazyspace::GeometryDeformation get_drawing_deformation(
    const GreasePencilStrokeParams &params)
{
  return bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
      &params.ob_eval, params.ob_orig, params.drawing);
}

Array<float2> calculate_view_positions(const GreasePencilStrokeParams &params,
                                       const IndexMask &selection)
{
  bke::crazyspace::GeometryDeformation deformation = get_drawing_deformation(params);

  Array<float2> view_positions(deformation.positions.size());

  /* Compute screen space positions. */
  const float4x4 transform = params.layer.to_world_space(params.ob_eval);
  selection.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
    eV3DProjStatus result = ED_view3d_project_float_global(
        &params.region,
        math::transform_point(transform, deformation.positions[point_i]),
        view_positions[point_i],
        V3D_PROJ_TEST_NOP);
    if (result != V3D_PROJ_RET_OK) {
      view_positions[point_i] = float2(0);
    }
  });

  return view_positions;
}

Array<float> calculate_view_radii(const GreasePencilStrokeParams &params,
                                  const IndexMask &selection)
{
  const RegionView3D *rv3d = static_cast<RegionView3D *>(params.region.regiondata);
  bke::crazyspace::GeometryDeformation deformation = get_drawing_deformation(params);

  const VArray<float> radii = params.drawing.radii();
  Array<float> view_radii(radii.size());
  /* Compute screen space radii. */
  const float4x4 transform = params.layer.to_world_space(params.ob_eval);
  selection.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
    const float pixel_size = ED_view3d_pixel_size(
        rv3d, math::transform_point(transform, deformation.positions[point_i]));
    view_radii[point_i] = radii[point_i] / pixel_size;
  });

  return view_radii;
}

bool do_vertex_color_points(const Brush &brush)
{
  return brush.gpencil_settings != nullptr &&
         ELEM(brush.gpencil_settings->vertex_mode, GPPAINT_MODE_STROKE, GPPAINT_MODE_BOTH);
}

bool do_vertex_color_fill(const Brush &brush)
{
  return brush.gpencil_settings != nullptr &&
         ELEM(brush.gpencil_settings->vertex_mode, GPPAINT_MODE_FILL, GPPAINT_MODE_BOTH);
}

bool GreasePencilStrokeOperationCommon::is_inverted(const Brush &brush) const
{
  return is_brush_inverted(brush, this->stroke_mode);
}

float2 GreasePencilStrokeOperationCommon::mouse_delta(const InputSample &input_sample) const
{
  return input_sample.mouse_position - this->prev_mouse_position;
}

void GreasePencilStrokeOperationCommon::foreach_editable_drawing_with_automask(
    const bContext &C,
    FunctionRef<bool(const GreasePencilStrokeParams &params, const IndexMask &point_mask)> fn)
    const
{
  using namespace blender::bke::greasepencil;

  const Scene &scene = *CTX_data_scene(&C);
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(&C);
  ARegion &region = *CTX_wm_region(&C);
  RegionView3D &rv3d = *CTX_wm_region_view3d(&C);
  Object &object = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  std::atomic<bool> changed = false;
  const Vector<MutableDrawingInfo> drawings = get_drawings_with_masking_for_stroke_operation(C);
  for (const int64_t i : drawings.index_range()) {
    const MutableDrawingInfo &info = drawings[i];
    const AutoMaskingInfo &auto_mask_info = this->auto_masking_info_per_drawing[i];
    GreasePencilStrokeParams params = GreasePencilStrokeParams::from_context(
        scene,
        depsgraph,
        region,
        rv3d,
        object,
        info.layer_index,
        info.frame_number,
        info.multi_frame_falloff,
        info.drawing);

    if (fn(params, auto_mask_info.point_mask)) {
      changed = true;
    }
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &grease_pencil);
  }
}

void GreasePencilStrokeOperationCommon::foreach_editable_drawing_with_automask(
    const bContext &C,
    FunctionRef<bool(const GreasePencilStrokeParams &params,
                     const IndexMask &point_mask,
                     const DeltaProjectionFunc &projection_fn)> fn) const
{
  using namespace blender::bke::greasepencil;

  const Scene &scene = *CTX_data_scene(&C);
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(&C);
  ARegion &region = *CTX_wm_region(&C);
  RegionView3D &rv3d = *CTX_wm_region_view3d(&C);
  Object &object = *CTX_data_active_object(&C);
  Object &object_eval = *DEG_get_evaluated(&depsgraph, &object);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  std::atomic<bool> changed = false;
  const Vector<MutableDrawingInfo> drawings = get_drawings_with_masking_for_stroke_operation(C);
  threading::parallel_for_each(drawings.index_range(), [&](const int i) {
    const MutableDrawingInfo &info = drawings[i];
    const Layer &layer = grease_pencil.layer(info.layer_index);
    const AutoMaskingInfo &auto_mask_info = this->auto_masking_info_per_drawing[i];
    const GreasePencilStrokeParams params = GreasePencilStrokeParams::from_context(
        scene,
        depsgraph,
        region,
        rv3d,
        object,
        info.layer_index,
        info.frame_number,
        info.multi_frame_falloff,
        info.drawing);

    const DeltaProjectionFunc projection_fn = get_screen_projection_fn(params, object_eval, layer);
    if (fn(params, auto_mask_info.point_mask, projection_fn)) {
      changed = true;
    }
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &grease_pencil);
  }
}

void GreasePencilStrokeOperationCommon::foreach_editable_drawing(
    const bContext &C, FunctionRef<bool(const GreasePencilStrokeParams &params)> fn) const
{
  using namespace blender::bke::greasepencil;

  const Scene &scene = *CTX_data_scene(&C);
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(&C);
  ARegion &region = *CTX_wm_region(&C);
  RegionView3D &rv3d = *CTX_wm_region_view3d(&C);
  Object &object = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = get_drawings_for_stroke_operation(C);
  for (const int64_t i : drawings.index_range()) {
    const MutableDrawingInfo &info = drawings[i];
    GreasePencilStrokeParams params = GreasePencilStrokeParams::from_context(
        scene,
        depsgraph,
        region,
        rv3d,
        object,
        info.layer_index,
        info.frame_number,
        info.multi_frame_falloff,
        info.drawing);
    if (fn(params)) {
      changed = true;
    }
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &grease_pencil);
  }
}

void GreasePencilStrokeOperationCommon::foreach_editable_drawing(
    const bContext &C,
    FunctionRef<bool(const GreasePencilStrokeParams &params,
                     const DeltaProjectionFunc &projection_fn)> fn) const
{
  using namespace blender::bke::greasepencil;

  const Scene &scene = *CTX_data_scene(&C);
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(&C);
  ARegion &region = *CTX_wm_region(&C);
  RegionView3D &rv3d = *CTX_wm_region_view3d(&C);
  Object &object = *CTX_data_active_object(&C);
  Object &object_eval = *DEG_get_evaluated(&depsgraph, &object);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = get_drawings_for_stroke_operation(C);
  for (const int64_t i : drawings.index_range()) {
    const MutableDrawingInfo &info = drawings[i];
    const Layer &layer = grease_pencil.layer(info.layer_index);
    GreasePencilStrokeParams params = GreasePencilStrokeParams::from_context(
        scene,
        depsgraph,
        region,
        rv3d,
        object,
        info.layer_index,
        info.frame_number,
        info.multi_frame_falloff,
        info.drawing);

    const DeltaProjectionFunc projection_fn = get_screen_projection_fn(params, object_eval, layer);
    if (fn(params, projection_fn)) {
      changed = true;
    }
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &grease_pencil);
  }
}

void GreasePencilStrokeOperationCommon::foreach_editable_drawing(
    const bContext &C,
    const GrainSize grain_size,
    FunctionRef<bool(const GreasePencilStrokeParams &params)> fn) const
{
  using namespace blender::bke::greasepencil;

  const Scene &scene = *CTX_data_scene(&C);
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(&C);
  ARegion &region = *CTX_wm_region(&C);
  RegionView3D &rv3d = *CTX_wm_region_view3d(&C);
  Object &object = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  std::atomic<bool> changed = false;
  const Vector<MutableDrawingInfo> drawings = get_drawings_for_stroke_operation(C);
  threading::parallel_for(drawings.index_range(), grain_size.value, [&](const IndexRange range) {
    for (const int64_t i : range) {
      const MutableDrawingInfo &info = drawings[i];
      GreasePencilStrokeParams params = GreasePencilStrokeParams::from_context(
          scene,
          depsgraph,
          region,
          rv3d,
          object,
          info.layer_index,
          info.frame_number,
          info.multi_frame_falloff,
          info.drawing);
      if (fn(params)) {
        changed = true;
      }
    }
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &grease_pencil);
  }
}

void GreasePencilStrokeOperationCommon::init_stroke(const bContext &C,
                                                    const InputSample &start_sample)
{
  Paint &paint = *BKE_paint_get_active_from_context(&C);
  Brush &brush = *BKE_paint_brush(&paint);

  init_brush(brush);

  this->start_mouse_position = start_sample.mouse_position;
  this->prev_mouse_position = start_sample.mouse_position;
}

void GreasePencilStrokeOperationCommon::init_auto_masking(const bContext &C,
                                                          const InputSample &start_sample)
{
  const Scene &scene = *CTX_data_scene(&C);
  ARegion &region = *CTX_wm_region(&C);
  RegionView3D &rv3d = *CTX_wm_region_view3d(&C);
  Object &object = *CTX_data_active_object(&C);
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(&C);

  const eGP_Sculpt_SelectMaskFlag sculpt_selection_flag = eGP_Sculpt_SelectMaskFlag(
      scene.toolsettings->gpencil_selectmode_sculpt);
  const bool use_sculpt_selection_masking = (sculpt_selection_flag &
                                             (GP_SCULPT_MASK_SELECTMODE_POINT |
                                              GP_SCULPT_MASK_SELECTMODE_STROKE |
                                              GP_SCULPT_MASK_SELECTMODE_SEGMENT)) != 0;

  const eGP_Sculpt_SettingsFlag sculpt_settings_flag = eGP_Sculpt_SettingsFlag(
      scene.toolsettings->gp_sculpt.flag);
  const bool use_auto_mask_stroke = (sculpt_settings_flag & GP_SCULPT_SETT_FLAG_AUTOMASK_STROKE);
  const bool use_auto_mask_layer = (sculpt_settings_flag &
                                    GP_SCULPT_SETT_FLAG_AUTOMASK_LAYER_STROKE);
  const bool use_auto_mask_material = (sculpt_settings_flag &
                                       GP_SCULPT_SETT_FLAG_AUTOMASK_MATERIAL_STROKE);
  const bool use_auto_mask_active_material = (sculpt_settings_flag &
                                              GP_SCULPT_SETT_FLAG_AUTOMASK_MATERIAL_ACTIVE);

  const float stroke_distance_threshold = 20.0f;
  const int2 mval_i = int2(math::round(start_sample.mouse_position));
  const int active_material_index = math::max(object.actcol - 1, 0);

  const Vector<MutableDrawingInfo> drawings = get_drawings_with_masking_for_stroke_operation(C);
  this->auto_masking_info_per_drawing.reinitialize(drawings.size());

  VectorSet<int> masked_layer_indices;
  VectorSet<int> masked_material_indices;
  for (const int drawing_i : drawings.index_range()) {
    const MutableDrawingInfo &drawing_info = drawings[drawing_i];
    AutoMaskingInfo &automask_info = this->auto_masking_info_per_drawing[drawing_i];
    GreasePencilStrokeParams params = GreasePencilStrokeParams::from_context(
        scene,
        depsgraph,
        region,
        rv3d,
        object,
        drawing_info.layer_index,
        drawing_info.frame_number,
        drawing_info.multi_frame_falloff,
        drawing_info.drawing);
    automask_info.point_mask = point_mask_for_stroke_operation(
        params, use_sculpt_selection_masking, automask_info.memory);
    if (automask_info.point_mask.is_empty()) {
      continue;
    }

    const bke::CurvesGeometry &curves = drawing_info.drawing.strokes();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    const bke::AttributeAccessor attributes = curves.attributes();

    if (use_auto_mask_active_material) {
      IndexMaskMemory memory;
      const VArraySpan<int> materials = *attributes.lookup_or_default<int>(
          "material_index", bke::AttrDomain::Point, 0);
      const IndexMask active_material_mask = IndexMask::from_predicate(
          curves.points_range(), GrainSize(4096), memory, [&](const int64_t point_i) {
            return active_material_index == materials[point_i];
          });
      automask_info.point_mask = IndexMask::from_intersection(
          automask_info.point_mask, active_material_mask, automask_info.memory);
      if (automask_info.point_mask.is_empty()) {
        continue;
      }
    }

    if (use_auto_mask_layer || use_auto_mask_stroke || use_auto_mask_material) {
      Array<float2> view_positions = calculate_view_positions(params, automask_info.point_mask);

      IndexMaskMemory memory;
      const IndexMask stroke_selection = curve_mask_for_stroke_operation(
          params, use_sculpt_selection_masking, memory);
      const IndexMask strokes_under_brush = IndexMask::from_predicate(
          stroke_selection, GrainSize(512), memory, [&](const int curve_i) {
            for (const int point_i : points_by_curve[curve_i]) {
              const float distance = math::distance(mval_i, int2(view_positions[point_i]));
              if (distance <= stroke_distance_threshold) {
                return true;
              }
            }
            return false;
          });

      if (use_auto_mask_layer && !strokes_under_brush.is_empty()) {
        masked_layer_indices.add(drawing_info.layer_index);
      }

      if (use_auto_mask_stroke) {
        automask_info.point_mask = IndexMask::from_intersection(
            automask_info.point_mask,
            IndexMask::from_ranges(curves.points_by_curve(), strokes_under_brush, memory),
            automask_info.memory);
      }

      if (use_auto_mask_material) {
        const VArraySpan<int> material_indices = *attributes.lookup_or_default<int>(
            "material_index", bke::AttrDomain::Curve, 0);
        strokes_under_brush.foreach_index(
            [&](const int curve_i) { masked_material_indices.add(material_indices[curve_i]); });
      }
    }
  }

  /* When we mask by the initial strokes under the cursor, the other masking options don't affect
   * the resulting mask. So we can skip the second loop. */
  if (use_auto_mask_stroke) {
    return;
  }

  threading::parallel_for_each(drawings.index_range(), [&](const int drawing_i) {
    const MutableDrawingInfo &drawing_info = drawings[drawing_i];
    AutoMaskingInfo &automask_info = this->auto_masking_info_per_drawing[drawing_i];

    if (use_auto_mask_layer && !masked_layer_indices.contains(drawing_info.layer_index)) {
      automask_info.point_mask = {};
      return;
    }

    if (use_auto_mask_material) {
      const bke::CurvesGeometry &curves = drawing_info.drawing.strokes();
      const VArraySpan<int> material_indices = *curves.attributes().lookup_or_default<int>(
          "material_index", bke::AttrDomain::Curve, 0);
      IndexMaskMemory memory;
      const IndexMask masked_curves = IndexMask::from_predicate(
          curves.curves_range(), GrainSize(1024), memory, [&](const int curve_i) {
            return masked_material_indices.contains(material_indices[curve_i]);
          });

      automask_info.point_mask = IndexMask::from_intersection(
          automask_info.point_mask,
          IndexMask::from_ranges(curves.points_by_curve(), masked_curves, memory),
          automask_info.memory);
    }
  });
}

void GreasePencilStrokeOperationCommon::stroke_extended(const InputSample &extension_sample)
{
  this->prev_mouse_position = extension_sample.mouse_position;
}

}  // namespace blender::ed::sculpt_paint::greasepencil
