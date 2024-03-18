/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "DNA_grease_pencil_types.h"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

namespace blender::nodes::node_geo_set_material_index_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry")
      .supported_type({GeometryComponent::Type::Mesh, GeometryComponent::Type::GreasePencil});
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Int>("Material Index").min(0).field_on_all();
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void set_material_index_in_geometry(const fn::FieldContext &field_context,
                                           const Field<bool> &selection_field,
                                           const Field<int> &index_field,
                                           MutableAttributeAccessor &attributes,
                                           const AttrDomain domain)
{
  const int domain_size = attributes.domain_size(domain);
  if (domain_size == 0) {
    return;
  }

  const bke::AttributeValidator validator = attributes.lookup_validator("material_index");
  AttributeWriter<int> indices = attributes.lookup_or_add_for_write<int>("material_index", domain);

  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(validator.validate_field_if_necessary(index_field),
                                 indices.varray);
  evaluator.evaluate();
  indices.finish();
}

static void set_material_index_in_grease_pencil(GreasePencil &grease_pencil,
                                                const Field<bool> &selection_field,
                                                const Field<int> &index_field)
{
  using namespace blender::bke::greasepencil;
  for (const int layer_index : grease_pencil.layers().index_range()) {
    Drawing *drawing = get_eval_grease_pencil_layer_drawing_for_write(grease_pencil, layer_index);
    if (drawing == nullptr) {
      continue;
    }
    bke::CurvesGeometry &curves = drawing->strokes_for_write();
    if (curves.curves_num() == 0) {
      continue;
    }

    MutableAttributeAccessor attributes = curves.attributes_for_write();
    const bke::GreasePencilLayerFieldContext field_context{
        grease_pencil, AttrDomain::Curve, layer_index};
    set_material_index_in_geometry(
        field_context, selection_field, index_field, attributes, AttrDomain::Curve);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<int> index_field = params.extract_input<Field<int>>("Material Index");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_mesh()) {
      GeometryComponent &component = geometry_set.get_component_for_write<MeshComponent>();
      const bke::GeometryFieldContext field_context{
          geometry_set.get_component_for_write<MeshComponent>(), AttrDomain::Face};
      MutableAttributeAccessor attributes = *component.attributes_for_write();
      set_material_index_in_geometry(
          field_context, selection_field, index_field, attributes, AttrDomain::Face);
    }
    if (GreasePencil *grease_pencil = geometry_set.get_grease_pencil_for_write()) {
      set_material_index_in_grease_pencil(*grease_pencil, selection_field, index_field);
    }
  });
  params.set_output("Geometry", std::move(geometry_set));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SET_MATERIAL_INDEX, "Set Material Index", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_material_index_cc
