/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_index_mask.hh"
#include "BLI_kdtree.h"

#include "BLT_translation.hh"

#include "BLO_read_write.hh"

#include "DNA_defaults.h"
#include "DNA_modifier_types.h"
#include "DNA_screen_types.h"

#include "BKE_curves_utils.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_modifier.hh"

#include "GEO_resample_curves.hh"
#include "GEO_simplify_curves.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_ui_common.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

namespace blender {

static void init_data(ModifierData *md)
{
  auto *gpmd = reinterpret_cast<GreasePencilSimplifyModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(GreasePencilSimplifyModifierData), modifier);
  modifier::greasepencil::init_influence_data(&gpmd->influence, true);
}

static void free_data(ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilSimplifyModifierData *>(md);

  modifier::greasepencil::free_influence_data(&mmd->influence);
}

static void copy_data(const ModifierData *md, ModifierData *target, int flag)
{
  const auto *gmd = reinterpret_cast<const GreasePencilSimplifyModifierData *>(md);
  auto *tgmd = reinterpret_cast<GreasePencilSimplifyModifierData *>(target);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&gmd->influence, &tgmd->influence, flag);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *mmd = reinterpret_cast<const GreasePencilSimplifyModifierData *>(md);

  BLO_write_struct(writer, GreasePencilSimplifyModifierData, mmd);
  modifier::greasepencil::write_influence_data(writer, &mmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilSimplifyModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &mmd->influence);
}

static IndexMask simplify_fixed(const bke::CurvesGeometry &curves,
                                const int step,
                                IndexMaskMemory &memory)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const Array<int> point_to_curve_map = curves.point_to_curve_map();
  return IndexMask::from_predicate(
      curves.points_range(), GrainSize(2048), memory, [&](const int64_t i) {
        const int curve_i = point_to_curve_map[i];
        const IndexRange points = points_by_curve[curve_i];
        if (points.size() <= 2) {
          return true;
        }
        const int local_i = i - points.start();
        return (local_i % int(math::pow(2.0f, float(step - 1))) == 0) || points.last() == i;
      });
}

static int curve_merge_by_distance(const IndexRange points,
                                   const Span<float> distances,
                                   const IndexMask &selection,
                                   const float merge_distance,
                                   MutableSpan<int> r_merge_indices)
{
  /* We use a KDTree_1d here, because we can only merge neighboring points in the curves. */
  KDTree_1d *tree = BLI_kdtree_1d_new(selection.size());
  /* The selection is an IndexMask of the points just in this curve. */
  selection.foreach_index_optimized<int64_t>([&](const int64_t i, const int64_t pos) {
    BLI_kdtree_1d_insert(tree, pos, &distances[i - points.first()]);
  });
  BLI_kdtree_1d_balance(tree);

  Array<int> selection_merge_indices(selection.size(), -1);
  const int duplicate_count = BLI_kdtree_1d_calc_duplicates_fast(
      tree, merge_distance, false, selection_merge_indices.data());
  BLI_kdtree_1d_free(tree);

  array_utils::fill_index_range<int>(r_merge_indices);

  selection.foreach_index([&](const int src_index, const int pos) {
    const int merge_index = selection_merge_indices[pos];
    if (merge_index != -1) {
      const int src_merge_index = selection[merge_index] - points.first();
      r_merge_indices[src_index - points.first()] = src_merge_index;
    }
  });

  return duplicate_count;
}

/* NOTE: The code here is an adapted version of #blender::geometry::point_merge_by_distance. */
static bke::CurvesGeometry curves_merge_by_distance(
    const bke::CurvesGeometry &src_curves,
    const float merge_distance,
    const IndexMask &selection,
    const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  const int src_point_size = src_curves.points_num();
  if (src_point_size == 0) {
    return {};
  }
  const OffsetIndices<int> points_by_curve = src_curves.points_by_curve();
  const VArray<bool> cyclic = src_curves.cyclic();
  src_curves.ensure_evaluated_lengths();

  bke::CurvesGeometry dst_curves = bke::curves::copy_only_curve_domain(src_curves);
  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();

  std::atomic<int> total_duplicate_count = 0;
  Array<Array<int>> merge_indices_per_curve(src_curves.curves_num());
  threading::parallel_for(src_curves.curves_range(), 512, [&](const IndexRange range) {
    for (const int curve_i : range) {
      const IndexRange points = points_by_curve[curve_i];
      merge_indices_per_curve[curve_i].reinitialize(points.size());

      Array<float> distances_along_curve(points.size());
      distances_along_curve.first() = 0.0f;
      const Span<float> lengths = src_curves.evaluated_lengths_for_curve(curve_i, cyclic[curve_i]);
      distances_along_curve.as_mutable_span().drop_front(1).copy_from(lengths);

      MutableSpan<int> merge_indices = merge_indices_per_curve[curve_i].as_mutable_span();
      array_utils::fill_index_range<int>(merge_indices);

      const int duplicate_count = curve_merge_by_distance(points,
                                                          distances_along_curve,
                                                          selection.slice_content(points),
                                                          merge_distance,
                                                          merge_indices);
      /* Write the curve size. The counts will be accumulated to offsets below. */
      dst_offsets[curve_i] = points.size() - duplicate_count;
      total_duplicate_count += duplicate_count;
    }
  });

  const int dst_point_size = src_point_size - total_duplicate_count;
  dst_curves.resize(dst_point_size, src_curves.curves_num());
  offset_indices::accumulate_counts_to_offsets(dst_offsets);

  int merged_points = 0;
  Array<int> src_to_dst_indices(src_point_size);
  for (const int curve_i : src_curves.curves_range()) {
    const IndexRange points = points_by_curve[curve_i];
    const Span<int> merge_indices = merge_indices_per_curve[curve_i].as_span();
    for (const int i : points.index_range()) {
      const int point_i = points.start() + i;
      src_to_dst_indices[point_i] = point_i - merged_points;
      if (merge_indices[i] != i) {
        merged_points++;
      }
    }
  }

  Array<int> point_merge_counts(dst_point_size, 0);
  for (const int curve_i : src_curves.curves_range()) {
    const IndexRange points = points_by_curve[curve_i];
    const Span<int> merge_indices = merge_indices_per_curve[curve_i].as_span();
    for (const int i : points.index_range()) {
      const int merge_index = merge_indices[i];
      const int point_src = points.start() + merge_index;
      const int dst_index = src_to_dst_indices[point_src];
      point_merge_counts[dst_index]++;
    }
  }

  Array<int> map_offsets_data(dst_point_size + 1);
  map_offsets_data.as_mutable_span().drop_back(1).copy_from(point_merge_counts);
  OffsetIndices<int> map_offsets = offset_indices::accumulate_counts_to_offsets(map_offsets_data);

  point_merge_counts.fill(0);

  Array<int> merge_map_indices(src_point_size);
  for (const int curve_i : src_curves.curves_range()) {
    const IndexRange points = points_by_curve[curve_i];
    const Span<int> merge_indices = merge_indices_per_curve[curve_i].as_span();
    for (const int i : points.index_range()) {
      const int point_i = points.start() + i;
      const int merge_index = merge_indices[i];
      const int dst_index = src_to_dst_indices[points.start() + merge_index];
      merge_map_indices[map_offsets[dst_index].first() + point_merge_counts[dst_index]] = point_i;
      point_merge_counts[dst_index]++;
    }
  }

  bke::AttributeAccessor src_attributes = src_curves.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  src_attributes.for_all([&](const bke::AttributeIDRef &id,
                             const bke::AttributeMetaData &meta_data) {
    if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    if (meta_data.domain != bke::AttrDomain::Point) {
      return true;
    }

    bke::GAttributeReader src_attribute = src_attributes.lookup(id);
    bke::attribute_math::convert_to_static_type(src_attribute.varray.type(), [&](auto dummy) {
      using T = decltype(dummy);
      if constexpr (!std::is_void_v<bke::attribute_math::DefaultMixer<T>>) {
        bke::SpanAttributeWriter<T> dst_attribute =
            dst_attributes.lookup_or_add_for_write_only_span<T>(id, bke::AttrDomain::Point);
        VArraySpan<T> src = src_attribute.varray.typed<T>();

        threading::parallel_for(dst_curves.points_range(), 1024, [&](IndexRange range) {
          for (const int dst_point_i : range) {
            /* Create a separate mixer for every point to avoid allocating temporary buffers
             * in the mixer the size of the result curves and to improve memory locality. */
            bke::attribute_math::DefaultMixer<T> mixer{dst_attribute.span.slice(dst_point_i, 1)};

            Span<int> src_merge_indices = merge_map_indices.as_span().slice(
                map_offsets[dst_point_i]);
            for (const int src_point_i : src_merge_indices) {
              mixer.mix_in(0, src[src_point_i]);
            }

            mixer.finalize();
          }
        });

        dst_attribute.finish();
      }
    });
    return true;
  });

  return dst_curves;
}

static void simplify_drawing(const GreasePencilSimplifyModifierData &mmd,
                             const Object &ob,
                             bke::greasepencil::Drawing &drawing)
{
  IndexMaskMemory memory;
  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexMask strokes = modifier::greasepencil::get_filtered_stroke_mask(
      &ob, curves, mmd.influence, memory);
  if (strokes.is_empty()) {
    return;
  }

  switch (mmd.mode) {
    case MOD_GREASE_PENCIL_SIMPLIFY_FIXED: {
      const IndexMask points_to_keep = simplify_fixed(curves, mmd.step, memory);
      if (points_to_keep.is_empty()) {
        drawing.strokes_for_write() = {};
        break;
      }
      if (points_to_keep.size() == curves.points_num()) {
        break;
      }
      drawing.strokes_for_write() = bke::curves_copy_point_selection(curves, points_to_keep, {});
      break;
    }
    case MOD_GREASE_PENCIL_SIMPLIFY_ADAPTIVE: {
      const IndexMask points_to_delete = geometry::simplify_curve_attribute(
          curves.positions(),
          strokes,
          curves.points_by_curve(),
          curves.cyclic(),
          mmd.factor,
          curves.positions(),
          memory);
      drawing.strokes_for_write().remove_points(points_to_delete, {});
      break;
    }
    case MOD_GREASE_PENCIL_SIMPLIFY_SAMPLE: {
      drawing.strokes_for_write() = geometry::resample_to_length(
          curves, strokes, VArray<float>::ForSingle(mmd.length, curves.curves_num()), {});
      break;
    }
    case MOD_GREASE_PENCIL_SIMPLIFY_MERGE: {
      const OffsetIndices points_by_curve = curves.points_by_curve();
      const Array<int> point_to_curve_map = curves.point_to_curve_map();
      const IndexMask points = IndexMask::from_predicate(
          curves.points_range(), GrainSize(2048), memory, [&](const int64_t i) {
            const int curve_i = point_to_curve_map[i];
            const IndexRange points = points_by_curve[curve_i];
            if (points.drop_front(1).drop_back(1).contains(i)) {
              return true;
            }
            return false;
          });
      drawing.strokes_for_write() = curves_merge_by_distance(curves, mmd.distance, points, {});
      break;
    }
    default:
      break;
  }

  drawing.tag_topology_changed();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  const auto *mmd = reinterpret_cast<GreasePencilSimplifyModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }

  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  const int current_frame = grease_pencil.runtime->eval_frame;

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, mmd->influence, mask_memory);
  const Vector<bke::greasepencil::Drawing *> drawings =
      modifier::greasepencil::get_drawings_for_write(grease_pencil, layer_mask, current_frame);

  threading::parallel_for_each(drawings, [&](bke::greasepencil::Drawing *drawing) {
    simplify_drawing(*mmd, *ctx->object, *drawing);
  });
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *mmd = reinterpret_cast<GreasePencilSimplifyModifierData *>(md);

  modifier::greasepencil::foreach_influence_ID_link(&mmd->influence, ob, walk, user_data);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  int mode = RNA_enum_get(ptr, "mode");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (mode == MOD_GREASE_PENCIL_SIMPLIFY_FIXED) {
    uiItemR(layout, ptr, "step", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else if (mode == MOD_GREASE_PENCIL_SIMPLIFY_ADAPTIVE) {
    uiItemR(layout, ptr, "factor", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else if (mode == MOD_GREASE_PENCIL_SIMPLIFY_SAMPLE) {
    uiItemR(layout, ptr, "length", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "sharp_threshold", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else if (mode == MOD_GREASE_PENCIL_SIMPLIFY_MERGE) {
    uiItemR(layout, ptr, "distance", UI_ITEM_NONE, nullptr, ICON_NONE);
  }

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilSimplify, panel_draw);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilSimplify = {
    /*idname*/ "GreasePencilSimplifyModifier",
    /*name*/ N_("Simplify"),
    /*struct_name*/ "GreasePencilSimplifyModifierData",
    /*struct_size*/ sizeof(GreasePencilSimplifyModifierData),
    /*srna*/ &RNA_GreasePencilSimplifyModifier,
    /*type*/ ModifierTypeType::Nonconstructive,
    /*flags*/
    eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_SIMPLIFY,

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
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ blender::foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ blender::panel_register,
    /*blend_write*/ blender::blend_write,
    /*blend_read*/ blender::blend_read,
    /*foreach_cache*/ nullptr,
};
