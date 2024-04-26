/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "BLI_index_mask.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_brush_types.h"
#include "DNA_node_tree_interface_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "grease_pencil_intern.hh"

#include <iostream>

namespace blender::ed::sculpt_paint::greasepencil {

Vector<ed::greasepencil::MutableDrawingInfo> get_drawings_for_sculpt(const bContext &C)
{
  using namespace blender::bke::greasepencil;

  const Scene &scene = *CTX_data_scene(&C);
  Object &ob_orig = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob_orig.data);
  Paint &paint = *BKE_paint_get_active_from_context(&C);
  const Brush &brush = *BKE_paint_brush(&paint);
  const bool active_layer_only = ((brush.gpencil_settings->flag & GP_BRUSH_ACTIVE_LAYER_ONLY) !=
                                  0);

  if (active_layer_only) {
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
  BKE_curvemapping_init(brush.gpencil_settings->curve_strength);
  BKE_curvemapping_init(brush.gpencil_settings->curve_sensitivity);
  BKE_curvemapping_init(brush.gpencil_settings->curve_jitter);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_pressure);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_strength);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_uv);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_hue);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_saturation);
  BKE_curvemapping_init(brush.gpencil_settings->curve_rand_value);
}

static float brush_radius(const Scene &scene, const Brush &brush, const float pressure = 1.0f)
{
  float radius = BKE_brush_size_get(&scene, &brush);
  if (BKE_brush_use_size_pressure(&brush)) {
    radius *= BKE_curvemapping_evaluateF(brush.gpencil_settings->curve_sensitivity, 0, pressure);
  }
  return radius;
}

float brush_influence(const Scene &scene,
                      const Brush &brush,
                      const float2 &co,
                      const InputSample &sample,
                      const float multi_frame_falloff)
{
  const float radius = brush_radius(scene, brush, sample.pressure);
  /* Basic strength factor from brush settings. */
  const float brush_pressure = BKE_brush_use_alpha_pressure(&brush) ? sample.pressure : 1.0f;
  const float influence_base = BKE_brush_alpha_get(&scene, &brush) * brush_pressure *
                               multi_frame_falloff;

  /* Distance falloff. */
  const int2 mval_i = int2(math::round(sample.mouse_position));
  const float distance = math::distance(mval_i, int2(co));
  /* Apply Brush curve. */
  const float brush_falloff = BKE_brush_curve_strength(&brush, distance, radius);

  return influence_base * brush_falloff;
}

IndexMask brush_influence_mask(const Scene &scene,
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

  const float radius = brush_radius(scene, brush, pressure);
  const float radius_squared = radius * radius;
  const float brush_pressure = BKE_brush_use_alpha_pressure(&brush) ? pressure : 1.0f;
  const float influence_base = BKE_brush_alpha_get(&scene, &brush) * brush_pressure *
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
  influences.reinitialize(influence_mask.size());
  array_utils::gather(all_influences.as_span(), influence_mask, influences.as_mutable_span());

  return influence_mask;
}

bool is_brush_inverted(const Brush &brush, const BrushStrokeMode stroke_mode)
{
  /* The basic setting is the brush's setting. During runtime, the user can hold down the Ctrl key
   * to invert the basic behavior. */
  return bool(brush.flag & BRUSH_DIR_IN) ^ (stroke_mode == BrushStrokeMode::BRUSH_STROKE_INVERT);
}

GreasePencilStrokeParams GreasePencilStrokeParams::from_context(
    const Scene &scene,
    const Depsgraph &depsgraph,
    const ARegion &region,
    const View3D &view3d,
    Object &object,
    const int layer_index,
    const int frame_number,
    const float multi_frame_falloff,
    bke::greasepencil::Drawing &drawing)
{
  Object &ob_eval = *DEG_get_evaluated_object(&depsgraph, &object);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  const bke::greasepencil::Layer &layer = *grease_pencil.layers()[layer_index];
  ed::greasepencil::DrawingPlacement placement(scene, region, view3d, ob_eval, layer);

  return {*scene.toolsettings,
          region,
          object,
          ob_eval,
          layer,
          layer_index,
          frame_number,
          multi_frame_falloff,
          std::move(placement),
          drawing};
}

IndexMask point_selection_mask(const GreasePencilStrokeParams &params, IndexMaskMemory &memory)
{
  const bool is_masking = GPENCIL_ANY_SCULPT_MASK(
      eGP_Sculpt_SelectMaskFlag(params.toolsettings.gpencil_selectmode_sculpt));
  return (is_masking ? ed::greasepencil::retrieve_editable_and_selected_points(
                           params.ob_eval, params.drawing, memory) :
                       params.drawing.strokes().points_range());
}

bke::crazyspace::GeometryDeformation get_drawing_deformation(
    const GreasePencilStrokeParams &params)
{
  return bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
      &params.ob_eval, params.ob_orig, params.layer_index, params.frame_number);
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

bool GreasePencilStrokeOperationCommon::is_inverted(const Brush &brush) const
{
  return is_brush_inverted(brush, this->stroke_mode);
}

float2 GreasePencilStrokeOperationCommon::mouse_delta(const InputSample &input_sample) const
{
  return input_sample.mouse_position - this->prev_mouse_position;
}

void GreasePencilStrokeOperationCommon::foreach_editable_drawing(
    const bContext &C, FunctionRef<bool(const GreasePencilStrokeParams &params)> fn) const
{
  using namespace blender::bke::greasepencil;

  const Scene &scene = *CTX_data_scene(&C);
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(&C);
  const View3D &view3d = *CTX_wm_view3d(&C);
  const ARegion &region = *CTX_wm_region(&C);
  Object &object = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  std::atomic<bool> changed = false;
  const Vector<MutableDrawingInfo> drawings = get_drawings_for_sculpt(C);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    GreasePencilStrokeParams params = GreasePencilStrokeParams::from_context(
        scene,
        depsgraph,
        region,
        view3d,
        object,
        info.layer_index,
        info.frame_number,
        info.multi_frame_falloff,
        info.drawing);
    if (fn(params)) {
      changed = true;
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

  this->prev_mouse_position = start_sample.mouse_position;
}

void GreasePencilStrokeOperationCommon::stroke_extended(const InputSample &extension_sample)
{
  this->prev_mouse_position = extension_sample.mouse_position;
}

}  // namespace blender::ed::sculpt_paint::greasepencil
