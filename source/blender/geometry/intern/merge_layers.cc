/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_merge_layers.hh"

#include "BLI_math_matrix.hh"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "GEO_join_geometries.hh"

namespace blender::geometry {

static bke::CurvesGeometry join_curves(const GreasePencil &src_grease_pencil,
                                       const Span<const bke::CurvesGeometry *> all_src_curves,
                                       const Span<float4x4> transforms_to_apply,
                                       const bke::AttributeFilter &attribute_filter)
{
  BLI_assert(all_src_curves.size() == transforms_to_apply.size());
  Vector<bke::GeometrySet> src_geometries(all_src_curves.size());
  for (const int src_curves_i : all_src_curves.index_range()) {
    bke::CurvesGeometry src_curves = *all_src_curves[src_curves_i];
    if (src_curves.is_empty()) {
      continue;
    }
    const float4x4 &transform = transforms_to_apply[src_curves_i];
    src_curves.transform(transform);
    Curves *src_curves_id = bke::curves_new_nomain(std::move(src_curves));
    src_curves_id->mat = static_cast<Material **>(MEM_dupallocN(src_grease_pencil.material_array));
    src_curves_id->totcol = src_grease_pencil.material_array_num;
    src_geometries[src_curves_i].replace_curves(src_curves_id);
  }
  bke::GeometrySet joined_geometry = join_geometries(src_geometries, attribute_filter);
  if (joined_geometry.has_curves()) {
    return joined_geometry.get_curves()->geometry.wrap();
  }
  return {};
}

GreasePencil *merge_layers(const GreasePencil &src_grease_pencil,
                           const Span<Vector<int>> layers_to_merge,
                           const bke::AttributeFilter &attribute_filter)
{
  using namespace bke::greasepencil;

  GreasePencil *new_grease_pencil = BKE_grease_pencil_new_nomain();

  BKE_grease_pencil_copy_parameters(src_grease_pencil, *new_grease_pencil);
  new_grease_pencil->runtime->eval_frame = src_grease_pencil.runtime->eval_frame;

  const int new_layers_num = layers_to_merge.size();
  new_grease_pencil->add_layers_with_empty_drawings_for_eval(new_layers_num);
  Vector<bke::CurvesGeometry *> curves_by_new_layer(new_layers_num);

  for (const int new_layer_i : IndexRange(new_layers_num)) {
    Layer &layer = new_grease_pencil->layer(new_layer_i);
    const Span<int> src_layer_indices = layers_to_merge[new_layer_i];
    BLI_assert(!src_layer_indices.is_empty());
    const int first_src_layer_i = src_layer_indices[0];
    const Layer &first_src_layer = src_grease_pencil.layer(first_src_layer_i);
    layer.set_name(first_src_layer.name());
    layer.opacity = first_src_layer.opacity;
    Drawing *drawing = new_grease_pencil->get_eval_drawing(layer);
    BLI_assert(drawing != nullptr);
    curves_by_new_layer[new_layer_i] = &drawing->strokes_for_write();
  }

  threading::parallel_for(IndexRange(new_layers_num), 32, [&](const IndexRange new_layers_range) {
    for (const int new_layer_i : new_layers_range) {
      Layer &new_layer = new_grease_pencil->layer(new_layer_i);

      const Span<int> src_layer_indices = layers_to_merge[new_layer_i];
      const int first_src_layer_i = src_layer_indices[0];
      const Layer &first_src_layer = src_grease_pencil.layer(first_src_layer_i);

      const float4x4 new_layer_transform = first_src_layer.local_transform();
      new_layer.set_local_transform(new_layer_transform);

      bke::CurvesGeometry &new_curves = *curves_by_new_layer[new_layer_i];

      if (src_layer_indices.size() == 1) {
        /* Optimization for the case if the new layer corresponds to exactly one source layer. */
        if (const Drawing *src_drawing = src_grease_pencil.get_eval_drawing(first_src_layer)) {
          const bke::CurvesGeometry &src_curves = src_drawing->strokes();
          new_curves = src_curves;
        }
        continue;
      }

      /* Needed to transform the positions from all spaces into the same space. */
      const float4x4 new_layer_transform_inv = math::invert(new_layer_transform);

      Vector<const bke::CurvesGeometry *> all_src_curves;
      Vector<float4x4> transforms_to_apply;
      for (const int i : src_layer_indices.index_range()) {
        const int src_layer_i = src_layer_indices[i];
        const Layer &src_layer = src_grease_pencil.layer(src_layer_i);
        if (const Drawing *src_drawing = src_grease_pencil.get_eval_drawing(src_layer)) {
          const bke::CurvesGeometry &src_curves = src_drawing->strokes();
          all_src_curves.append(&src_curves);
          transforms_to_apply.append(new_layer_transform_inv * src_layer.local_transform());
        }
      }
      new_curves = join_curves(
          src_grease_pencil, all_src_curves, transforms_to_apply, attribute_filter);
    }
  });

  const bke::AttributeAccessor src_attributes = src_grease_pencil.attributes();
  bke::MutableAttributeAccessor new_attributes = new_grease_pencil->attributes_for_write();
  src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.data_type == CD_PROP_STRING) {
      return;
    }
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    bke::GAttributeReader src_attribute = iter.get();
    bke::GSpanAttributeWriter new_attribute = new_attributes.lookup_or_add_for_write_only_span(
        iter.name, bke::AttrDomain::Layer, iter.data_type);

    const CPPType &type = new_attribute.span.type();

    bke::attribute_math::convert_to_static_type(type, [&](auto type) {
      using T = decltype(type);
      const VArraySpan<T> src_span = src_attribute.varray.typed<T>();
      MutableSpan<T> new_span = new_attribute.span.typed<T>();

      bke::attribute_math::DefaultMixer<T> mixer(new_span);
      for (const int new_layer_i : IndexRange(new_layers_num)) {
        const Span<int> src_layer_indices = layers_to_merge[new_layer_i];
        for (const int src_layer_i : src_layer_indices) {
          const T &src_value = src_span[src_layer_i];
          mixer.mix_in(new_layer_i, src_value);
        }
      }

      mixer.finalize();
    });

    new_attribute.finish();
  });

  return new_grease_pencil;
}

}  // namespace blender::geometry
