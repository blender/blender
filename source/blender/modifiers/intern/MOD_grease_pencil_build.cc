/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_array.hh"
#include "BLI_hash.h"
#include "BLI_rand.h"
#include "BLI_sort.hh"
#include "BLI_task.h"

#include "BLT_translation.hh"

#include "BLO_read_write.hh"

#include "DNA_defaults.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph_query.hh"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "GEO_reorder.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  auto *gpmd = reinterpret_cast<GreasePencilBuildModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(GreasePencilBuildModifierData), modifier);
  modifier::greasepencil::init_influence_data(&gpmd->influence, false);
}

static void copy_data(const ModifierData *md, ModifierData *target, int flags)
{
  const auto *omd = reinterpret_cast<const GreasePencilBuildModifierData *>(md);
  auto *tomd = reinterpret_cast<GreasePencilBuildModifierData *>(target);

  modifier::greasepencil::free_influence_data(&tomd->influence);

  BKE_modifier_copydata_generic(md, target, flags);
  modifier::greasepencil::copy_influence_data(&omd->influence, &tomd->influence, flags);
}

static void free_data(ModifierData *md)
{
  auto *omd = reinterpret_cast<GreasePencilBuildModifierData *>(md);
  modifier::greasepencil::free_influence_data(&omd->influence);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *omd = reinterpret_cast<GreasePencilBuildModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&omd->influence, ob, walk, user_data);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  auto *mmd = reinterpret_cast<GreasePencilBuildModifierData *>(md);
  if (mmd->object != nullptr) {
    DEG_add_object_relation(ctx->node, mmd->object, DEG_OB_COMP_TRANSFORM, "Build Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Build Modifier");
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *mmd = reinterpret_cast<const GreasePencilBuildModifierData *>(md);

  BLO_write_struct(writer, GreasePencilBuildModifierData, mmd);
  modifier::greasepencil::write_influence_data(writer, &mmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilBuildModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &mmd->influence);
}

static Array<int> point_counts_to_keep_concurrent(const bke::CurvesGeometry &curves,
                                                  const IndexMask &selection,
                                                  const int time_alignment,
                                                  const int transition,
                                                  const float factor,
                                                  const bool clamp_points,
                                                  int &r_curves_num,
                                                  int &r_points_num)
{
  const int stroke_count = curves.curves_num();
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();

  curves.ensure_evaluated_lengths();
  float max_length = 0;
  for (const int stroke : curves.curves_range()) {
    const float len = curves.evaluated_length_total_for_curve(stroke, false);
    max_length = math::max(max_length, len);
  }

  float factor_to_keep = transition == MOD_GREASE_PENCIL_BUILD_TRANSITION_GROW ? factor :
                                                                                 1.0f - factor;
  if (clamp_points) {
    r_curves_num = r_points_num = 0;
    factor_to_keep = std::clamp(factor_to_keep, 0.0f, 1.0f);
  }

  auto get_stroke_factor = [&](const float factor, const int index) {
    const float max_factor = max_length / curves.evaluated_length_total_for_curve(index, false);
    if (time_alignment == MOD_GREASE_PENCIL_BUILD_TIMEALIGN_START) {
      if (clamp_points) {
        return std::clamp(factor * max_factor, 0.0f, 1.0f);
      }
      return factor * max_factor;
    }
    if (time_alignment == MOD_GREASE_PENCIL_BUILD_TIMEALIGN_END) {
      const float min_factor = max_factor - 1.0f;
      const float use_factor = factor * max_factor;
      if (clamp_points) {
        return std::clamp(use_factor - min_factor, 0.0f, 1.0f);
      }
      return use_factor - min_factor;
    }
    return 0.0f;
  };

  Array<bool> select(stroke_count);
  selection.to_bools(select.as_mutable_span());
  Array<int> result(stroke_count);
  for (const int curve : curves.curves_range()) {
    const float local_factor = select[curve] ? get_stroke_factor(factor_to_keep, curve) : 1.0f;
    const int num_points = points_by_curve[curve].size() * local_factor;
    result[curve] = num_points;
    if (clamp_points) {
      r_points_num += num_points;
      if (num_points > 0) {
        r_curves_num++;
      }
    }
  }
  return result;
}

static bke::CurvesGeometry build_concurrent(bke::greasepencil::Drawing &drawing,
                                            bke::CurvesGeometry &curves,
                                            const IndexMask &selection,
                                            const int time_alignment,
                                            const int transition,
                                            const float factor,
                                            const float factor_start,
                                            const float factor_opacity,
                                            const float factor_radii,
                                            StringRefNull target_vgname)
{
  int dst_curves_num, dst_points_num;
  const bool has_fade = factor_start != factor;
  const Array<int> point_counts_to_keep = point_counts_to_keep_concurrent(
      curves, selection, time_alignment, transition, factor, true, dst_curves_num, dst_points_num);
  if (dst_curves_num == 0) {
    return {};
  }
  const Array<int> starts_per_curve = has_fade ? point_counts_to_keep_concurrent(curves,
                                                                                 selection,
                                                                                 time_alignment,
                                                                                 transition,
                                                                                 factor_start,
                                                                                 false,
                                                                                 dst_curves_num,
                                                                                 dst_points_num) :
                                                 Array<int>(0);
  const Array<int> ends_per_curve = has_fade ? point_counts_to_keep_concurrent(curves,
                                                                               selection,
                                                                               time_alignment,
                                                                               transition,
                                                                               factor,
                                                                               false,
                                                                               dst_curves_num,
                                                                               dst_points_num) :
                                               Array<int>(0);

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  MutableSpan<float> opacities = drawing.opacities_for_write();
  MutableSpan<float> radii = drawing.radii_for_write();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<float> weights = attributes.lookup_for_write_span<float>(target_vgname);

  const bool is_vanishing = transition == MOD_GREASE_PENCIL_BUILD_TRANSITION_VANISH;

  bke::CurvesGeometry dst_curves(dst_points_num, dst_curves_num);
  Array<int> dst_to_src_point(dst_points_num);
  Array<int> dst_to_src_curve(dst_curves_num);
  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();
  dst_offsets[0] = 0;

  int next_curve = 0;
  int next_point = 0;
  for (const int curve : curves.curves_range()) {
    if (!point_counts_to_keep[curve]) {
      continue;
    }
    const IndexRange points = points_by_curve[curve];
    dst_offsets[next_curve] = point_counts_to_keep[curve];
    const int curve_size = points.size();

    auto get_fade_weight = [&](const int local_index) {
      const float fade_range = std::abs(ends_per_curve[curve] - starts_per_curve[curve]);
      if (is_vanishing) {
        const float factor_from_start = local_index - curve_size + ends_per_curve[curve];
        return 1.0f - std::clamp(factor_from_start / fade_range, 0.0f, 1.0f);
      }
      const float factor_from_start = local_index - starts_per_curve[curve];
      return std::clamp(factor_from_start / fade_range, 0.0f, 1.0f);
    };

    const int extra_offset = is_vanishing ? points.size() - point_counts_to_keep[curve] : 0;
    for (const int stroke_point : IndexRange(point_counts_to_keep[curve])) {
      const int src_point_index = points.first() + extra_offset + stroke_point;
      if (has_fade) {
        const float fade_weight = get_fade_weight(extra_offset + stroke_point);
        opacities[src_point_index] = opacities[src_point_index] *
                                     (1.0f - fade_weight * factor_opacity);
        radii[src_point_index] = radii[src_point_index] * (1.0f - fade_weight * factor_radii);
        if (!weights.span.is_empty()) {
          weights.span[src_point_index] = fade_weight;
        }
      }
      dst_to_src_point[next_point] = src_point_index;
      next_point++;
    }
    dst_to_src_curve[next_curve] = curve;
    next_curve++;
  }
  weights.finish();

  offset_indices::accumulate_counts_to_offsets(dst_offsets);

  const bke::AttributeAccessor src_attributes = curves.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();

  gather_attributes(
      src_attributes, bke::AttrDomain::Point, {}, {}, dst_to_src_point, dst_attributes);
  gather_attributes(
      src_attributes, bke::AttrDomain::Curve, {}, {}, dst_to_src_curve, dst_attributes);

  dst_curves.update_curve_types();

  return dst_curves;
}

static void points_info_sequential(const bke::CurvesGeometry &curves,
                                   const IndexMask &selection,
                                   const int transition,
                                   const float factor,
                                   const bool clamp_points,
                                   int &r_curves_num,
                                   int &r_points_num)
{
  const int stroke_count = curves.curves_num();
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();

  float factor_to_keep = transition == MOD_GREASE_PENCIL_BUILD_TRANSITION_GROW ? factor :
                                                                                 (1.0f - factor);
  if (clamp_points) {
    factor_to_keep = std::clamp(factor_to_keep, 0.0f, 1.0f);
  }

  const bool is_vanishing = transition == MOD_GREASE_PENCIL_BUILD_TRANSITION_VANISH;

  int effective_points_num = 0;
  selection.foreach_index(
      [&](const int index) { effective_points_num += points_by_curve[index].size(); });

  const int untouched_points_num = points_by_curve.total_size() - effective_points_num;
  effective_points_num *= factor_to_keep;
  effective_points_num += untouched_points_num;

  r_points_num = effective_points_num;
  r_curves_num = 0;

  Array<bool> select(stroke_count);
  selection.to_bools(select.as_mutable_span());

  int counted_points_num = 0;
  for (const int i : curves.curves_range()) {
    const int stroke = is_vanishing ? stroke_count - i - 1 : i;
    if (select[stroke] && counted_points_num >= effective_points_num) {
      continue;
    }
    counted_points_num += points_by_curve[stroke].size();
    r_curves_num++;
  }
}

static bke::CurvesGeometry build_sequential(bke::greasepencil::Drawing &drawing,
                                            bke::CurvesGeometry &curves,
                                            const IndexMask &selection,
                                            const int transition,
                                            const float factor,
                                            const float factor_start,
                                            const float factor_opacity,
                                            const float factor_radii,
                                            StringRefNull target_vgname)
{
  const bool has_fade = factor_start != factor;
  int dst_curves_num, dst_points_num;
  int start_points_num, end_points_num, dummy_curves_num;
  points_info_sequential(
      curves, selection, transition, factor, true, dst_curves_num, dst_points_num);

  if (dst_curves_num == 0) {
    return {};
  }

  points_info_sequential(
      curves, selection, transition, factor_start, false, dummy_curves_num, start_points_num);
  points_info_sequential(
      curves, selection, transition, factor, false, dummy_curves_num, end_points_num);

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  MutableSpan<float> opacities = drawing.opacities_for_write();
  MutableSpan<float> radii = drawing.radii_for_write();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<float> weights = attributes.lookup_for_write_span<float>(target_vgname);

  const bool is_vanishing = transition == MOD_GREASE_PENCIL_BUILD_TRANSITION_VANISH;

  bke::CurvesGeometry dst_curves(dst_points_num, dst_curves_num);
  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();
  Array<int> dst_to_src_point(dst_points_num);
  Array<int> dst_to_src_curve(dst_curves_num);

  dst_offsets[0] = 0;

  int next_curve = 1, next_point = 0;
  IndexMaskMemory memory;
  selection.complement(curves.curves_range(), memory).foreach_index([&](const int stroke) {
    for (const int point : points_by_curve[stroke]) {
      dst_to_src_point[next_point] = point;
      next_point++;
    }
    dst_offsets[next_curve] = next_point;
    next_curve++;
  });

  const int stroke_count = curves.curves_num();
  bool done_scanning = false;
  selection.foreach_index([&](const int i) {
    const int stroke = is_vanishing ? stroke_count - i - 1 : i;
    if (done_scanning || next_point >= dst_points_num) {
      done_scanning = true;
      return;
    }

    auto get_fade_weight = [&](const int next_point_count) {
      return std::clamp(float(next_point_count - start_points_num) /
                            float(abs(end_points_num - start_points_num)),
                        0.0f,
                        1.0f);
    };

    const IndexRange points = points_by_curve[stroke];
    for (const int point : points) {
      const int local_index = point - points.first();
      const int src_point_index = is_vanishing ? points.last() - local_index : point;
      dst_to_src_point[next_point] = src_point_index;

      if (has_fade) {
        const float fade_weight = get_fade_weight(next_point);
        opacities[src_point_index] = opacities[src_point_index] *
                                     (1.0f - fade_weight * factor_opacity);
        radii[src_point_index] = radii[src_point_index] * (1.0f - fade_weight * factor_radii);
        if (!weights.span.is_empty()) {
          weights.span[src_point_index] = fade_weight;
        }
      }

      next_point++;
      if (next_point >= dst_points_num) {
        done_scanning = true;
        break;
      }
    }
    dst_offsets[next_curve] = next_point;
    dst_to_src_curve[next_curve - 1] = i;
    next_curve++;
  });
  weights.finish();

  BLI_assert(next_curve == (dst_curves_num + 1));
  BLI_assert(next_point == dst_points_num);

  const bke::AttributeAccessor src_attributes = curves.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();

  gather_attributes(
      src_attributes, bke::AttrDomain::Point, {}, {}, dst_to_src_point, dst_attributes);
  gather_attributes(
      src_attributes, bke::AttrDomain::Curve, {}, {}, dst_to_src_curve, dst_attributes);

  dst_curves.update_curve_types();

  return dst_curves;
}

static bke::CurvesGeometry reorder_strokes(const bke::CurvesGeometry &curves,
                                           const Span<bool> select,
                                           const Object &object,
                                           MutableSpan<bool> r_selection)
{
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const Span<float3> positions = curves.positions();
  const float3 center = object.object_to_world().location();

  struct Pair {
    float value;
    int index;
    bool selected;
  };

  Array<Pair> distances(curves.curves_num());
  for (const int stroke : curves.curves_range()) {
    const IndexRange points = points_by_curve[stroke];
    const float3 p1 = positions[points.first()];
    const float3 p2 = positions[points.last()];
    distances[stroke].value = math::max(math::distance(p1, center), math::distance(p2, center));
    distances[stroke].index = stroke;
    distances[stroke].selected = select[stroke];
  }

  parallel_sort(
      distances.begin(), distances.end(), [](Pair &a, Pair &b) { return a.value < b.value; });

  Array<int> new_order(curves.curves_num());
  for (const int i : curves.curves_range()) {
    new_order[i] = distances[i].index;
    r_selection[i] = distances[i].selected;
  }

  return geometry::reorder_curves_geometry(curves, new_order.as_span(), {});
}

static float get_factor_from_draw_speed(const bke::CurvesGeometry &curves,
                                        const float time_elapsed,
                                        const float speed_fac,
                                        const float max_gap)
{
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const bke::AttributeAccessor attributes = curves.attributes();
  const VArray<float> init_times =
      attributes.lookup_or_default<float>("init_time", bke::AttrDomain::Curve, 0.0f).varray;
  const VArray<float> delta_times =
      attributes.lookup_or_default<float>("delta_time", bke::AttrDomain::Point, 0.0f).varray;

  Array<float> times(curves.points_num());
  float current_time = 0;
  float previous_init_time = init_times[0];
  for (const int curve : curves.curves_range()) {
    if (curve > 0) {
      current_time += math::max(init_times[curve] - previous_init_time, max_gap);
      previous_init_time = init_times[curve];
    }
    for (const int point : points_by_curve[curve]) {
      current_time += delta_times[point];
      times[point] = current_time;
    }
  }
  for (const int point : curves.points_range()) {
    const float limit = time_elapsed * speed_fac;
    if (times[point] >= limit) {
      return math::clamp(float(point) / float(curves.points_num()), 0.0f, 1.0f);
    }
  }
  return 1.0f;
}

static float get_build_factor(const GreasePencilBuildTimeMode time_mode,
                              const int current_frame,
                              const int start_frame,
                              const int length,
                              const float percentage,
                              const bke::CurvesGeometry &curves,
                              const float scene_fps,
                              const float speed_fac,
                              const float max_gap,
                              const float fade)
{
  switch (time_mode) {
    case MOD_GREASE_PENCIL_BUILD_TIMEMODE_FRAMES:
      return math::clamp(float(current_frame - start_frame) / length, 0.0f, 1.0f) * (1.0f + fade);
    case MOD_GREASE_PENCIL_BUILD_TIMEMODE_PERCENTAGE:
      return percentage * (1.0f + fade);
    case MOD_GREASE_PENCIL_BUILD_TIMEMODE_DRAWSPEED:
      return get_factor_from_draw_speed(
                 curves, float(current_frame) / scene_fps, speed_fac, max_gap) *
             (1.0f + fade);
  }
  BLI_assert_unreachable();
  return 0.0f;
}

static void build_drawing(const GreasePencilBuildModifierData &mmd,
                          const Object &ob,
                          bke::greasepencil::Drawing &drawing,
                          const bke::greasepencil::Drawing *previous_drawing,
                          const int current_time,
                          const float scene_fps)
{
  bke::CurvesGeometry &curves = drawing.strokes_for_write();

  if (curves.points_num() == 0) {
    return;
  }

  IndexMaskMemory memory;
  IndexMask selection = modifier::greasepencil::get_filtered_stroke_mask(
      &ob, curves, mmd.influence, memory);

  /* Remove a count of #prev_strokes.  */
  if (mmd.mode == MOD_GREASE_PENCIL_BUILD_MODE_ADDITIVE && previous_drawing != nullptr) {
    const bke::CurvesGeometry &prev_curves = previous_drawing->strokes();
    const int prev_strokes = prev_curves.curves_num();
    const int added_strokes = curves.curves_num() - prev_strokes;
    if (added_strokes > 0) {
      Array<bool> work_on_select(curves.curves_num());
      selection.to_bools(work_on_select.as_mutable_span());
      work_on_select.as_mutable_span().take_front(prev_strokes).fill(false);
      selection = IndexMask::from_bools(work_on_select, memory);
    }
  }

  if (mmd.object) {
    const int curves_num = curves.curves_num();
    Array<bool> select(curves_num), reordered_select(curves_num);
    selection.to_bools(select);
    curves = reorder_strokes(
        curves, select.as_span(), *mmd.object, reordered_select.as_mutable_span());
    selection = IndexMask::from_bools(reordered_select, memory);
  }

  const float fade_factor = ((mmd.flag & MOD_GREASE_PENCIL_BUILD_USE_FADING) != 0) ? mmd.fade_fac :
                                                                                     0.0f;
  float factor = get_build_factor(GreasePencilBuildTimeMode(mmd.time_mode),
                                  current_time,
                                  mmd.start_delay,
                                  mmd.length,
                                  mmd.percentage_fac,
                                  curves,
                                  scene_fps,
                                  mmd.speed_fac,
                                  mmd.speed_maxgap,
                                  fade_factor);
  float factor_start = factor - fade_factor;
  if (mmd.transition != MOD_GREASE_PENCIL_BUILD_TRANSITION_GROW) {
    std::swap(factor, factor_start);
  }

  const float use_time_alignment = mmd.transition != MOD_GREASE_PENCIL_BUILD_TRANSITION_GROW ?
                                       !mmd.time_alignment :
                                       mmd.time_alignment;
  switch (mmd.mode) {
    default:
    case MOD_GREASE_PENCIL_BUILD_MODE_SEQUENTIAL:
      curves = build_sequential(drawing,
                                curves,
                                selection,
                                mmd.transition,
                                factor,
                                factor_start,
                                mmd.fade_opacity_strength,
                                mmd.fade_thickness_strength,
                                mmd.target_vgname);
      break;
    case MOD_GREASE_PENCIL_BUILD_MODE_CONCURRENT:
      curves = build_concurrent(drawing,
                                curves,
                                selection,
                                use_time_alignment,
                                mmd.transition,
                                factor,
                                factor_start,
                                mmd.fade_opacity_strength,
                                mmd.fade_thickness_strength,
                                mmd.target_vgname);
      break;
    case MOD_GREASE_PENCIL_BUILD_MODE_ADDITIVE:
      curves = build_sequential(drawing,
                                curves,
                                selection,
                                mmd.transition,
                                factor,
                                factor_start,
                                mmd.fade_opacity_strength,
                                mmd.fade_thickness_strength,
                                mmd.target_vgname);
      break;
  }

  drawing.tag_topology_changed();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                blender::bke::GeometrySet *geometry_set)
{
  const auto *mmd = reinterpret_cast<GreasePencilBuildModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }

  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  const int eval_frame = grease_pencil.runtime->eval_frame;

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, mmd->influence, mask_memory);
  const Vector<modifier::greasepencil::LayerDrawingInfo> drawing_infos =
      modifier::greasepencil::get_drawing_infos_by_layer(grease_pencil, layer_mask, eval_frame);

  if (mmd->flag & MOD_GREASE_PENCIL_BUILD_RESTRICT_TIME) {
    if (eval_frame < mmd->start_frame || eval_frame > mmd->end_frame) {
      return;
    }
  }

  const Scene &scene = *DEG_get_evaluated_scene(ctx->depsgraph);
  const float scene_fps = float(scene.r.frs_sec) / scene.r.frs_sec_base;
  const Span<const bke::greasepencil::Layer *> layers = grease_pencil.layers();

  threading::parallel_for_each(
      drawing_infos, [&](modifier::greasepencil::LayerDrawingInfo drawing_info) {
        const bke::greasepencil::Drawing *prev_drawing = grease_pencil.get_drawing_at(
            *layers[drawing_info.layer_index], eval_frame - 1);
        build_drawing(
            *mmd, *ctx->object, *drawing_info.drawing, prev_drawing, eval_frame, scene_fps);
      });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  const GreasePencilBuildMode mode = GreasePencilBuildMode(RNA_enum_get(ptr, "mode"));
  GreasePencilBuildTimeMode time_mode = GreasePencilBuildTimeMode(RNA_enum_get(ptr, "time_mode"));

  uiLayoutSetPropSep(layout, true);

  /* First: Build mode and build settings. */
  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, nullptr, ICON_NONE);
  if (mode == MOD_GREASE_PENCIL_BUILD_MODE_SEQUENTIAL) {
    uiItemR(layout, ptr, "transition", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  if (mode == MOD_GREASE_PENCIL_BUILD_MODE_CONCURRENT) {
    /* Concurrent mode doesn't support MOD_GREASE_PENCIL_BUILD_TIMEMODE_DRAWSPEED, so unset it. */
    if (time_mode == MOD_GREASE_PENCIL_BUILD_TIMEMODE_DRAWSPEED) {
      RNA_enum_set(ptr, "time_mode", MOD_GREASE_PENCIL_BUILD_TIMEMODE_FRAMES);
      time_mode = MOD_GREASE_PENCIL_BUILD_TIMEMODE_FRAMES;
    }
    uiItemR(layout, ptr, "transition", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  uiItemS(layout);

  /* Second: Time mode and time settings. */

  uiItemR(layout, ptr, "time_mode", UI_ITEM_NONE, nullptr, ICON_NONE);
  if (mode == MOD_GREASE_PENCIL_BUILD_MODE_CONCURRENT) {
    uiItemR(layout, ptr, "concurrent_time_alignment", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  switch (time_mode) {
    case MOD_GREASE_PENCIL_BUILD_TIMEMODE_DRAWSPEED:
      uiItemR(layout, ptr, "speed_factor", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, ptr, "speed_maxgap", UI_ITEM_NONE, nullptr, ICON_NONE);
      break;
    case MOD_GREASE_PENCIL_BUILD_TIMEMODE_FRAMES:
      uiItemR(layout, ptr, "length", UI_ITEM_NONE, IFACE_("Frames"), ICON_NONE);
      if (mode != MOD_GREASE_PENCIL_BUILD_MODE_ADDITIVE) {
        uiItemR(layout, ptr, "start_delay", UI_ITEM_NONE, nullptr, ICON_NONE);
      }
      break;
    case MOD_GREASE_PENCIL_BUILD_TIMEMODE_PERCENTAGE:
      uiItemR(layout, ptr, "percentage_factor", UI_ITEM_NONE, nullptr, ICON_NONE);
      break;
    default:
      break;
  }
  uiItemS(layout);
  uiItemR(layout, ptr, "object", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (uiLayout *panel = uiLayoutPanelProp(
          C, layout, ptr, "open_frame_range_panel", "Effective Range"))
  {
    uiLayoutSetPropSep(panel, true);
    uiItemR(
        panel, ptr, "use_restrict_frame_range", UI_ITEM_NONE, IFACE_("Custom Range"), ICON_NONE);

    const bool active = RNA_boolean_get(ptr, "use_restrict_frame_range");
    uiLayout *col = uiLayoutColumn(panel, false);
    uiLayoutSetActive(col, active);
    uiItemR(col, ptr, "frame_start", UI_ITEM_NONE, IFACE_("Start"), ICON_NONE);
    uiItemR(col, ptr, "frame_end", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);
  }

  if (uiLayout *panel = uiLayoutPanelProp(C, layout, ptr, "open_fading_panel", "Fading")) {
    uiLayoutSetPropSep(panel, true);
    uiItemR(panel, ptr, "use_fading", UI_ITEM_NONE, IFACE_("Fade"), ICON_NONE);

    const bool active = RNA_boolean_get(ptr, "use_fading");
    uiLayout *col = uiLayoutColumn(panel, false);
    uiLayoutSetActive(col, active);

    uiItemR(col, ptr, "fade_factor", UI_ITEM_NONE, IFACE_("Factor"), ICON_NONE);

    uiLayout *subcol = uiLayoutColumn(col, true);
    uiItemR(subcol, ptr, "fade_thickness_strength", UI_ITEM_NONE, IFACE_("Thickness"), ICON_NONE);
    uiItemR(subcol, ptr, "fade_opacity_strength", UI_ITEM_NONE, IFACE_("Opacity"), ICON_NONE);

    uiItemPointerR(col,
                   ptr,
                   "target_vertex_group",
                   &ob_ptr,
                   "vertex_groups",
                   IFACE_("Weight Output"),
                   ICON_NONE);
  }

  if (uiLayout *influence_panel = uiLayoutPanelProp(
          C, layout, ptr, "open_influence_panel", "Influence"))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
  }

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilBuild, panel_draw);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilBuild = {
    /*idname*/ "GreasePencilBuildModifier",
    /*name*/ N_("Build"),
    /*struct_name*/ "GreasePencilBuildModifierData",
    /*struct_size*/ sizeof(GreasePencilBuildModifierData),
    /*srna*/ &RNA_GreasePencilBuildModifier,
    /*type*/ ModifierTypeType::Nonconstructive,
    /*flags*/
    eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_EnableInEditmode |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_LENGTH,

    /*copy_data*/ blender::copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ blender::modify_geometry_set,

    /*init_data*/ blender::init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ blender::free_data,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ blender::update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ blender::foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ blender::panel_register,
    /*blend_write*/ blender::blend_write,
    /*blend_read*/ blender::blend_read,
};
