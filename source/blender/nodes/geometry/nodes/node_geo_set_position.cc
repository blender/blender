/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_position_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Vector>("Position").implicit_field_on_all(implicit_field_inputs::position);
  b.add_input<decl::Vector>("Offset").subtype(PROP_TRANSLATION).field_on_all();
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

constexpr GrainSize grain_size{10000};

static bool check_positions_are_original(const AttributeAccessor &attributes,
                                         const VArray<float3> &in_positions)
{
  const bke::AttributeReader positions_read_only = attributes.lookup<float3>("position");
  if (positions_read_only.varray.is_span() && in_positions.is_span()) {
    return positions_read_only.varray.get_internal_span().data() ==
           in_positions.get_internal_span().data();
  }
  return false;
}

static void write_offset_positions(const bool positions_are_original,
                                   const IndexMask &selection,
                                   const VArray<float3> &in_positions,
                                   const VArray<float3> &in_offsets,
                                   VMutableArray<float3> &out_positions)
{
  if (positions_are_original) {
    if (const std::optional<float3> offset = in_offsets.get_if_single()) {
      if (math::is_zero(*offset)) {
        return;
      }
    }
  }

  MutableVArraySpan<float3> out_positions_span = out_positions;
  if (positions_are_original) {
    devirtualize_varray(in_offsets, [&](const auto in_offsets) {
      selection.foreach_index_optimized<int>(
          grain_size, [&](const int i) { out_positions_span[i] += in_offsets[i]; });
    });
  }
  else {
    devirtualize_varray2(
        in_positions, in_offsets, [&](const auto in_positions, const auto in_offsets) {
          selection.foreach_index_optimized<int>(grain_size, [&](const int i) {
            out_positions_span[i] = in_positions[i] + in_offsets[i];
          });
        });
  }
  out_positions_span.save();
}

static void set_computed_position_and_offset(GeometryComponent &component,
                                             const VArray<float3> &in_positions,
                                             const VArray<float3> &in_offsets,
                                             const IndexMask &selection,
                                             MutableAttributeAccessor attributes)
{
  /* Optimize the case when `in_positions` references the original positions array. */
  switch (component.type()) {
    case GeometryComponent::Type::Curve: {
      if (attributes.contains("handle_right") && attributes.contains("handle_left")) {
        CurveComponent &curve_component = static_cast<CurveComponent &>(component);
        Curves &curves_id = *curve_component.get_for_write();
        bke::CurvesGeometry &curves = curves_id.geometry.wrap();
        SpanAttributeWriter<float3> handle_right_attribute =
            attributes.lookup_or_add_for_write_span<float3>("handle_right", AttrDomain::Point);
        SpanAttributeWriter<float3> handle_left_attribute =
            attributes.lookup_or_add_for_write_span<float3>("handle_left", AttrDomain::Point);

        AttributeWriter<float3> positions = attributes.lookup_for_write<float3>("position");
        MutableVArraySpan<float3> out_positions_span = positions.varray;
        devirtualize_varray2(
            in_positions, in_offsets, [&](const auto in_positions, const auto in_offsets) {
              selection.foreach_index_optimized<int>(grain_size, [&](const int i) {
                const float3 new_position = in_positions[i] + in_offsets[i];
                const float3 delta = new_position - out_positions_span[i];
                handle_right_attribute.span[i] += delta;
                handle_left_attribute.span[i] += delta;
                out_positions_span[i] = new_position;
              });
            });

        out_positions_span.save();
        positions.finish();
        handle_right_attribute.finish();
        handle_left_attribute.finish();

        /* Automatic Bezier handles must be recalculated based on the new positions. */
        curves.calculate_bezier_auto_handles();
        break;
      }
      AttributeWriter<float3> positions = attributes.lookup_for_write<float3>("position");
      write_offset_positions(check_positions_are_original(attributes, in_positions),
                             selection,
                             in_positions,
                             in_offsets,
                             positions.varray);
      positions.finish();
      break;
    }
    case GeometryComponent::Type::Instance: {
      /* Special case for "position" which is no longer an attribute on instances. */
      auto &instances_component = reinterpret_cast<bke::InstancesComponent &>(component);
      bke::Instances &instances = *instances_component.get_for_write();
      VMutableArray<float3> positions = bke::instance_position_varray_for_write(instances);
      write_offset_positions(false, selection, in_positions, in_offsets, positions);
      break;
    }
    default: {
      AttributeWriter<float3> positions = attributes.lookup_for_write<float3>("position");
      write_offset_positions(check_positions_are_original(attributes, in_positions),
                             selection,
                             in_positions,
                             in_offsets,
                             positions.varray);
      positions.finish();
      break;
    }
  }
}

static void set_position_in_grease_pencil(GreasePencilComponent &grease_pencil_component,
                                          const Field<bool> &selection_field,
                                          const Field<float3> &position_field,
                                          const Field<float3> &offset_field)
{
  using namespace blender::bke::greasepencil;
  GreasePencil &grease_pencil = *grease_pencil_component.get_for_write();
  /* Set position for each layer. */
  for (const int layer_index : grease_pencil.layers().index_range()) {
    Drawing *drawing = bke::greasepencil::get_eval_grease_pencil_layer_drawing_for_write(
        grease_pencil, layer_index);
    if (drawing == nullptr || drawing->strokes().points_num() == 0) {
      continue;
    }
    bke::GreasePencilLayerFieldContext field_context(
        grease_pencil, AttrDomain::Point, layer_index);
    fn::FieldEvaluator evaluator{field_context, drawing->strokes().points_num()};
    evaluator.set_selection(selection_field);
    evaluator.add(position_field);
    evaluator.add(offset_field);
    evaluator.evaluate();

    const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
    if (selection.is_empty()) {
      continue;
    }

    MutableAttributeAccessor attributes = drawing->strokes_for_write().attributes_for_write();
    const VArray<float3> positions_input = evaluator.get_evaluated<float3>(0);
    const VArray<float3> offsets_input = evaluator.get_evaluated<float3>(1);
    set_computed_position_and_offset(
        grease_pencil_component, positions_input, offsets_input, selection, attributes);
    drawing->tag_positions_changed();
  }
}

static void set_position_in_component(GeometrySet &geometry,
                                      GeometryComponent::Type component_type,
                                      const Field<bool> &selection_field,
                                      const Field<float3> &position_field,
                                      const Field<float3> &offset_field)
{
  const GeometryComponent &component = *geometry.get_component(component_type);
  const AttrDomain domain = component.type() == GeometryComponent::Type::Instance ?
                                AttrDomain::Instance :
                                AttrDomain::Point;
  const int domain_size = component.attribute_domain_size(domain);
  if (domain_size == 0) {
    return;
  }

  bke::GeometryFieldContext field_context{component, domain};
  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);
  evaluator.add(position_field);
  evaluator.add(offset_field);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.is_empty()) {
    return;
  }

  GeometryComponent &mutable_component = geometry.get_component_for_write(component_type);
  MutableAttributeAccessor attributes = *mutable_component.attributes_for_write();
  const VArray<float3> positions_input = evaluator.get_evaluated<float3>(0);
  const VArray<float3> offsets_input = evaluator.get_evaluated<float3>(1);
  set_computed_position_and_offset(
      mutable_component, positions_input, offsets_input, selection, attributes);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float3> offset_field = params.extract_input<Field<float3>>("Offset");
  Field<float3> position_field = params.extract_input<Field<float3>>("Position");

  if (geometry.has_grease_pencil()) {
    set_position_in_grease_pencil(geometry.get_component_for_write<GreasePencilComponent>(),
                                  selection_field,
                                  position_field,
                                  offset_field);
  }

  for (const GeometryComponent::Type type : {GeometryComponent::Type::Mesh,
                                             GeometryComponent::Type::PointCloud,
                                             GeometryComponent::Type::Curve,
                                             GeometryComponent::Type::Instance})
  {
    if (geometry.has(type)) {
      set_position_in_component(geometry, type, selection_field, position_field, offset_field);
    }
  }

  params.set_output("Geometry", std::move(geometry));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_POSITION, "Set Position", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_position_cc
