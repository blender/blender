/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "BKE_curve_to_mesh.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"

#include "UI_resources.hh"

#include "GEO_randomize.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_to_mesh_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(
      {GeometryComponent::Type::Curve, GeometryComponent::Type::GreasePencil});
  b.add_input<decl::Geometry>("Profile Curve")
      .only_realized_data()
      .supported_type(GeometryComponent::Type::Curve);
  b.add_input<decl::Bool>("Fill Caps")
      .description(
          "If the profile spline is cyclic, fill the ends of the generated mesh with N-gons");
  b.add_output<decl::Geometry>("Mesh").propagate_all();
}

static Mesh *curve_to_mesh(const bke::CurvesGeometry &curves,
                           const GeometrySet &profile_set,
                           const bool fill_caps,
                           const AnonymousAttributePropagationInfo &propagation_info)
{
  Mesh *mesh;
  if (profile_set.has_curves()) {
    const Curves *profile_curves = profile_set.get_curves();
    mesh = bke::curve_to_mesh_sweep(
        curves, profile_curves->geometry.wrap(), fill_caps, propagation_info);
  }
  else {
    mesh = bke::curve_to_wire_mesh(curves, propagation_info);
  }
  geometry::debug_randomize_mesh_order(mesh);
  return mesh;
}

static void grease_pencil_to_mesh(GeometrySet &geometry_set,
                                  const GeometrySet &profile_set,
                                  const bool fill_caps,
                                  const AnonymousAttributePropagationInfo &propagation_info)
{
  using namespace blender::bke::greasepencil;

  const GreasePencil &grease_pencil = *geometry_set.get_grease_pencil();
  Array<Mesh *> mesh_by_layer(grease_pencil.layers().size(), nullptr);

  for (const int layer_index : grease_pencil.layers().index_range()) {
    const Drawing *drawing = grease_pencil.get_eval_drawing(*grease_pencil.layer(layer_index));
    if (drawing == nullptr) {
      continue;
    }
    const bke::CurvesGeometry &curves = drawing->strokes();
    mesh_by_layer[layer_index] = curve_to_mesh(curves, profile_set, fill_caps, propagation_info);
  }

  if (mesh_by_layer.is_empty()) {
    return;
  }

  InstancesComponent &instances_component =
      geometry_set.get_component_for_write<InstancesComponent>();
  bke::Instances *instances = instances_component.get_for_write();
  if (instances == nullptr) {
    instances = new bke::Instances();
    instances_component.replace(instances);
  }
  for (Mesh *mesh : mesh_by_layer) {
    if (!mesh) {
      /* Add an empty reference so the number of layers and instances match.
       * This makes it easy to reconstruct the layers afterwards and keep their attributes.
       * Although in this particular case we don't propagate the attributes. */
      const int handle = instances->add_reference(bke::InstanceReference());
      instances->add_instance(handle, float4x4::identity());
      continue;
    }
    GeometrySet temp_set = GeometrySet::from_mesh(mesh);
    const int handle = instances->add_reference(bke::InstanceReference{temp_set});
    instances->add_instance(handle, float4x4::identity());
  }
  GeometrySet::propagate_attributes_from_layer_to_instances(
      geometry_set.get_grease_pencil()->attributes(),
      geometry_set.get_instances_for_write()->attributes_for_write(),
      propagation_info);
  geometry_set.replace_grease_pencil(nullptr);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet curve_set = params.extract_input<GeometrySet>("Curve");
  GeometrySet profile_set = params.extract_input<GeometrySet>("Profile Curve");
  const bool fill_caps = params.extract_input<bool>("Fill Caps");

  bke::GeometryComponentEditData::remember_deformed_positions_if_necessary(curve_set);
  const AnonymousAttributePropagationInfo &propagation_info = params.get_output_propagation_info(
      "Mesh");

  curve_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_curves()) {
      const Curves &curves = *geometry_set.get_curves();
      Mesh *mesh = curve_to_mesh(curves.geometry.wrap(), profile_set, fill_caps, propagation_info);
      geometry_set.replace_mesh(mesh);
    }
    if (geometry_set.has_grease_pencil()) {
      grease_pencil_to_mesh(geometry_set, profile_set, fill_caps, propagation_info);
    }
    geometry_set.keep_only_during_modify({GeometryComponent::Type::Mesh});
  });

  params.set_output("Mesh", std::move(curve_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CURVE_TO_MESH, "Curve to Mesh", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_to_mesh_cc
