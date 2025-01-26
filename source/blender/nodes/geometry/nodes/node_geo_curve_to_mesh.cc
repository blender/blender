/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "BKE_curve_to_mesh.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"

#include "GEO_join_geometries.hh"
#include "GEO_randomize.hh"

#include "DNA_mesh_types.h"

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
                           const AttributeFilter &attribute_filter)
{
  Mesh *mesh;
  if (profile_set.has_curves()) {
    const Curves *profile_curves = profile_set.get_curves();
    mesh = bke::curve_to_mesh_sweep(
        curves, profile_curves->geometry.wrap(), fill_caps, attribute_filter);
  }
  else {
    mesh = bke::curve_to_wire_mesh(curves, attribute_filter);
  }
  geometry::debug_randomize_mesh_order(mesh);
  return mesh;
}

static void grease_pencil_to_mesh(GeometrySet &geometry_set,
                                  const GeometrySet &profile_set,
                                  const bool fill_caps,
                                  const AttributeFilter &attribute_filter)
{
  using namespace blender::bke::greasepencil;

  const GreasePencil &grease_pencil = *geometry_set.get_grease_pencil();
  Array<Mesh *> mesh_by_layer(grease_pencil.layers().size(), nullptr);

  for (const int layer_index : grease_pencil.layers().index_range()) {
    const Drawing *drawing = grease_pencil.get_eval_drawing(grease_pencil.layer(layer_index));
    if (drawing == nullptr) {
      continue;
    }
    const bke::CurvesGeometry &curves = drawing->strokes();
    mesh_by_layer[layer_index] = curve_to_mesh(curves, profile_set, fill_caps, attribute_filter);
  }

  if (mesh_by_layer.is_empty()) {
    return;
  }

  bke::Instances *instances = new bke::Instances();
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

  bke::copy_attributes(geometry_set.get_grease_pencil()->attributes(),
                       bke::AttrDomain::Layer,
                       bke::AttrDomain::Instance,
                       attribute_filter,
                       instances->attributes_for_write());
  InstancesComponent &dst_component = geometry_set.get_component_for_write<InstancesComponent>();
  GeometrySet new_instances = geometry::join_geometries(
      {GeometrySet::from_instances(dst_component.release()),
       GeometrySet::from_instances(instances)},
      attribute_filter);
  dst_component.replace(new_instances.get_component_for_write<InstancesComponent>().release());
  geometry_set.replace_grease_pencil(nullptr);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet curve_set = params.extract_input<GeometrySet>("Curve");
  GeometrySet profile_set = params.extract_input<GeometrySet>("Profile Curve");
  const bool fill_caps = params.extract_input<bool>("Fill Caps");

  bke::GeometryComponentEditData::remember_deformed_positions_if_necessary(curve_set);
  const AttributeFilter &attribute_filter = params.get_attribute_filter("Mesh");

  curve_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_curves()) {
      const Curves &curves = *geometry_set.get_curves();
      Mesh *mesh = curve_to_mesh(curves.geometry.wrap(), profile_set, fill_caps, attribute_filter);
      if (mesh != nullptr) {
        mesh->mat = static_cast<Material **>(MEM_dupallocN(curves.mat));
        mesh->totcol = curves.totcol;
      }
      geometry_set.replace_mesh(mesh);
    }
    if (geometry_set.has_grease_pencil()) {
      grease_pencil_to_mesh(geometry_set, profile_set, fill_caps, attribute_filter);
    }
    geometry_set.keep_only_during_modify({GeometryComponent::Type::Mesh});
  });

  params.set_output("Mesh", std::move(curve_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeCurveToMesh", GEO_NODE_CURVE_TO_MESH);
  ntype.ui_name = "Curve to Mesh";
  ntype.ui_description =
      "Convert curves into a mesh, optionally with a custom profile shape defined by curves";
  ntype.enum_name_legacy = "CURVE_TO_MESH";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_to_mesh_cc
