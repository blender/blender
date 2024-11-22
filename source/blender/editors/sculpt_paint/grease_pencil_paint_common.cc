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
#include "DNA_node_tree_interface_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "grease_pencil_intern.hh"

#include <iostream>

namespace blender::ed::sculpt_paint::greasepencil {

Vector<ed::greasepencil::MutableDrawingInfo> get_drawings_for_painting(const bContext &C)
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
  BKE_curvemapping_init(brush.curve);
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

float brush_radius(const Scene &scene, const Brush &brush, const float pressure = 1.0f)
{
  float radius = BKE_brush_size_get(&scene, &brush);
  if (BKE_brush_use_size_pressure(&brush)) {
    radius *= BKE_curvemapping_evaluateF(brush.gpencil_settings->curve_sensitivity, 0, pressure);
  }
  return radius;
}

float brush_point_influence(const Scene &scene,
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

float brush_fill_influence(const Scene &scene,
                           const Brush &brush,
                           const Span<float2> fill_positions,
                           const InputSample &sample,
                           const float multi_frame_falloff)
{
  const float radius = brush_radius(scene, brush, sample.pressure);
  /* Basic strength factor from brush settings. */
  const float brush_pressure = BKE_brush_use_alpha_pressure(&brush) ? sample.pressure : 1.0f;
  const float influence_base = BKE_brush_alpha_get(&scene, &brush) * brush_pressure *
                               multi_frame_falloff;

  /* Distance falloff. */
  const float distance = closest_distance_to_surface_2d(sample.mouse_position, fill_positions);
  /* Apply Brush curve. */
  const float brush_falloff = BKE_brush_curve_strength(&brush, distance, radius);

  return influence_base * brush_falloff;
}

IndexMask brush_point_influence_mask(const Scene &scene,
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
  influences.resize(influence_mask.size());
  array_utils::gather(all_influences.as_span(), influence_mask, influences.as_mutable_span());

  return influence_mask;
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

  switch (params.toolsettings.gp_sculpt.lock_axis) {
    case GP_LOCKAXIS_VIEW: {
      const float3 world_normal = view_to_world.z_axis();
      return [=](const float3 &position, const float2 &screen_delta) {
        const float3 world_pos = math::transform_point(layer_to_world, position);
        const float3 world_delta = screen_to_world(world_pos, screen_delta);
        const float3 layer_delta = math::transform_direction(
            world_to_layer, world_delta - world_normal * math::dot(world_delta, world_normal));
        return position + layer_delta;
      };
    }
    case GP_LOCKAXIS_X: {
      return [=](const float3 &position, const float2 &screen_delta) {
        const float3 world_pos = math::transform_point(layer_to_world, position);
        const float3 world_delta = screen_to_world(world_pos, screen_delta);
        const float3 layer_delta = math::transform_direction(
            world_to_layer, float3(0.0f, world_delta.y, world_delta.z));
        return position + layer_delta;
      };
    }
    case GP_LOCKAXIS_Y: {
      return [=](const float3 &position, const float2 &screen_delta) {
        const float3 world_pos = math::transform_point(layer_to_world, position);
        const float3 world_delta = screen_to_world(world_pos, screen_delta);
        const float3 layer_delta = math::transform_direction(
            world_to_layer, float3(world_delta.x, 0.0f, world_delta.z));
        return position + layer_delta;
      };
    }
    case GP_LOCKAXIS_Z: {
      return [=](const float3 &position, const float2 &screen_delta) {
        const float3 world_pos = math::transform_point(layer_to_world, position);
        const float3 world_delta = screen_to_world(world_pos, screen_delta);
        const float3 layer_delta = math::transform_direction(
            world_to_layer, float3(world_delta.x, world_delta.y, 0.0f));
        return position + layer_delta;
      };
    }
    case GP_LOCKAXIS_CURSOR: {
      const float3 world_normal = params.scene.cursor.matrix<float3x3>().z_axis();
      return [=](const float3 &position, const float2 &screen_delta) {
        const float3 world_pos = math::transform_point(layer_to_world, position);
        const float3 world_delta = screen_to_world(world_pos, screen_delta);
        const float3 layer_delta = math::transform_direction(
            world_to_layer, world_delta - world_normal * math::dot(world_delta, world_normal));
        return position + layer_delta;
      };
    }
  }

  BLI_assert_unreachable();
  return [](const float3 &, const float2 &) { return float3(); };
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
  Object &ob_eval = *DEG_get_evaluated_object(&depsgraph, &object);
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

IndexMask point_selection_mask(const GreasePencilStrokeParams &params,
                               const bool use_masking,
                               IndexMaskMemory &memory)
{

  return (use_masking ? ed::greasepencil::retrieve_editable_and_selected_points(
                            params.ob_orig, params.drawing, params.layer_index, memory) :
                        ed::greasepencil::retrieve_editable_points(
                            params.ob_orig, params.drawing, params.layer_index, memory));
}

IndexMask stroke_selection_mask(const GreasePencilStrokeParams &params,
                                const bool use_masking,
                                IndexMaskMemory &memory)
{

  return (use_masking ? ed::greasepencil::retrieve_editable_and_selected_strokes(
                            params.ob_orig, params.drawing, params.layer_index, memory) :
                        ed::greasepencil::retrieve_editable_strokes(
                            params.ob_orig, params.drawing, params.layer_index, memory));
}

IndexMask fill_selection_mask(const GreasePencilStrokeParams &params,
                              const bool use_masking,
                              IndexMaskMemory &memory)
{
  return (use_masking ? ed::greasepencil::retrieve_editable_and_selected_fill_strokes(
                            params.ob_orig, params.drawing, params.layer_index, memory) :
                        params.drawing.strokes().curves_range());
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

  std::atomic<bool> changed = false;
  const Vector<MutableDrawingInfo> drawings = get_drawings_for_painting(C);
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
  const Vector<MutableDrawingInfo> drawings = get_drawings_for_painting(C);
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
  Object &object_eval = *DEG_get_evaluated_object(&depsgraph, &object);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  std::atomic<bool> changed = false;
  const Vector<MutableDrawingInfo> drawings = get_drawings_for_painting(C);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    const Layer &layer = grease_pencil.layer(info.layer_index);

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
    if (fn(params, projection_fn)) {
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

  this->start_mouse_position = start_sample.mouse_position;
  this->prev_mouse_position = start_sample.mouse_position;
}

void GreasePencilStrokeOperationCommon::stroke_extended(const InputSample &extension_sample)
{
  this->prev_mouse_position = extension_sample.mouse_position;
}

}  // namespace blender::ed::sculpt_paint::greasepencil
