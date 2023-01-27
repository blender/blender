/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DEG_depsgraph_query.h"

#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_curves.hh"
#include "BKE_mesh.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_position_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().field_on_all();
  b.add_input<decl::Vector>(N_("Position")).implicit_field_on_all(implicit_field_inputs::position);
  b.add_input<decl::Vector>(N_("Offset")).field_on_all().subtype(PROP_TRANSLATION);
  b.add_output<decl::Geometry>(N_("Geometry")).propagate_all();
}

static void set_computed_position_and_offset(GeometryComponent &component,
                                             const VArray<float3> &in_positions,
                                             const VArray<float3> &in_offsets,
                                             const IndexMask selection)
{
  MutableAttributeAccessor attributes = *component.attributes_for_write();
  const VArray<float3> positions_read_only = attributes.lookup<float3>("position");

  if (in_positions.is_same(positions_read_only)) {
    if (const std::optional<float3> offset = in_offsets.get_if_single()) {
      if (math::is_zero(*offset)) {
        return;
      }
    }
  }
  const int grain_size = 10000;

  switch (component.type()) {
    case GEO_COMPONENT_TYPE_CURVE: {
      if (attributes.contains("handle_right") && attributes.contains("handle_left")) {
        CurveComponent &curve_component = static_cast<CurveComponent &>(component);
        Curves &curves_id = *curve_component.get_for_write();
        bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id.geometry);
        SpanAttributeWriter<float3> handle_right_attribute =
            attributes.lookup_or_add_for_write_span<float3>("handle_right", ATTR_DOMAIN_POINT);
        SpanAttributeWriter<float3> handle_left_attribute =
            attributes.lookup_or_add_for_write_span<float3>("handle_left", ATTR_DOMAIN_POINT);

        AttributeWriter<float3> positions = attributes.lookup_for_write<float3>("position");
        MutableVArraySpan<float3> out_positions_span = positions.varray;
        devirtualize_varray2(
            in_positions, in_offsets, [&](const auto in_positions, const auto in_offsets) {
              threading::parallel_for(
                  selection.index_range(), grain_size, [&](const IndexRange range) {
                    for (const int i : selection.slice(range)) {
                      const float3 new_position = in_positions[i] + in_offsets[i];
                      const float3 delta = new_position - out_positions_span[i];
                      handle_right_attribute.span[i] += delta;
                      handle_left_attribute.span[i] += delta;
                      out_positions_span[i] = new_position;
                    }
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
      ATTR_FALLTHROUGH;
    }
    default: {
      AttributeWriter<float3> positions = attributes.lookup_for_write<float3>("position");
      MutableVArraySpan<float3> out_positions_span = positions.varray;
      if (in_positions.is_same(positions_read_only)) {
        devirtualize_varray(in_offsets, [&](const auto in_offsets) {
          threading::parallel_for(
              selection.index_range(), grain_size, [&](const IndexRange range) {
                for (const int i : selection.slice(range)) {
                  out_positions_span[i] += in_offsets[i];
                }
              });
        });
      }
      else {
        devirtualize_varray2(
            in_positions, in_offsets, [&](const auto in_positions, const auto in_offsets) {
              threading::parallel_for(
                  selection.index_range(), grain_size, [&](const IndexRange range) {
                    for (const int i : selection.slice(range)) {
                      out_positions_span[i] = in_positions[i] + in_offsets[i];
                    }
                  });
            });
      }
      out_positions_span.save();
      positions.finish();
      break;
    }
  }
}

static void set_position_in_component(GeometryComponent &component,
                                      const Field<bool> &selection_field,
                                      const Field<float3> &position_field,
                                      const Field<float3> &offset_field)
{
  eAttrDomain domain = component.type() == GEO_COMPONENT_TYPE_INSTANCES ? ATTR_DOMAIN_INSTANCE :
                                                                          ATTR_DOMAIN_POINT;
  bke::GeometryFieldContext field_context{component, domain};
  const int domain_size = component.attribute_domain_size(domain);
  if (domain_size == 0) {
    return;
  }

  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);
  evaluator.add(position_field);
  evaluator.add(offset_field);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  const VArray<float3> positions_input = evaluator.get_evaluated<float3>(0);
  const VArray<float3> offsets_input = evaluator.get_evaluated<float3>(1);
  set_computed_position_and_offset(component, positions_input, offsets_input, selection);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float3> offset_field = params.extract_input<Field<float3>>("Offset");
  Field<float3> position_field = params.extract_input<Field<float3>>("Position");

  for (const GeometryComponentType type : {GEO_COMPONENT_TYPE_MESH,
                                           GEO_COMPONENT_TYPE_POINT_CLOUD,
                                           GEO_COMPONENT_TYPE_CURVE,
                                           GEO_COMPONENT_TYPE_INSTANCES}) {
    if (geometry.has(type)) {
      set_position_in_component(
          geometry.get_component_for_write(type), selection_field, position_field, offset_field);
    }
  }

  params.set_output("Geometry", std::move(geometry));
}

}  // namespace blender::nodes::node_geo_set_position_cc

void register_node_type_geo_set_position()
{
  namespace file_ns = blender::nodes::node_geo_set_position_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_POSITION, "Set Position", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
