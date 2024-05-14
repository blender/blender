/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "DNA_pointcloud_types.h"

#include "BKE_customdata.hh"
#include "BKE_mesh.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_points_to_vertices_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Points").supported_type(GeometryComponent::Type::PointCloud);
  b.add_input<decl::Bool>("Selection").default_value(true).field_on_all().hide_value();
  b.add_output<decl::Geometry>("Mesh").propagate_all();
}

/* One improvement would be to move the attribute arrays directly to the mesh when possible. */
static void geometry_set_points_to_vertices(
    GeometrySet &geometry_set,
    Field<bool> &selection_field,
    const AnonymousAttributePropagationInfo &propagation_info)
{
  const PointCloud *points = geometry_set.get_pointcloud();
  if (points == nullptr) {
    geometry_set.remove_geometry_during_modify();
    return;
  }
  if (points->totpoint == 0) {
    geometry_set.remove_geometry_during_modify();
    return;
  }

  const bke::PointCloudFieldContext field_context{*points};
  fn::FieldEvaluator selection_evaluator{field_context, points->totpoint};
  selection_evaluator.add(selection_field);
  selection_evaluator.evaluate();
  const IndexMask selection = selection_evaluator.get_evaluated_as_mask(0);

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation({GeometryComponent::Type::PointCloud},
                                                 GeometryComponent::Type::Mesh,
                                                 false,
                                                 propagation_info,
                                                 attributes);

  Mesh *mesh;
  if (selection.size() == points->totpoint) {
    /* Create a mesh without positions so the attribute can be shared. */
    mesh = BKE_mesh_new_nomain(0, 0, 0, 0);
    CustomData_free_layer_named(&mesh->vert_data, "position", mesh->verts_num);
    mesh->verts_num = selection.size();
  }
  else {
    mesh = BKE_mesh_new_nomain(selection.size(), 0, 0, 0);
  }

  const AttributeAccessor src_attributes = points->attributes();
  MutableAttributeAccessor dst_attributes = mesh->attributes_for_write();

  for (MapItem<AttributeIDRef, AttributeKind> entry : attributes.items()) {
    const AttributeIDRef id = entry.key;
    const eCustomDataType data_type = entry.value.data_type;
    const GAttributeReader src = src_attributes.lookup(id);
    if (selection.size() == points->totpoint && src.sharing_info && src.varray.is_span()) {
      const bke::AttributeInitShared init(src.varray.get_internal_span().data(),
                                          *src.sharing_info);
      dst_attributes.add(id, AttrDomain::Point, data_type, init);
    }
    else {
      GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
          id, AttrDomain::Point, data_type);
      array_utils::gather(src.varray, selection, dst.span);
      dst.finish();
    }
  }

  mesh->tag_loose_edges_none();
  mesh->tag_overlapping_none();

  geometry_set.replace_mesh(mesh);
  geometry_set.keep_only_during_modify({GeometryComponent::Type::Mesh});
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    geometry_set_points_to_vertices(
        geometry_set, selection_field, params.get_output_propagation_info("Mesh"));
  });

  params.set_output("Mesh", std::move(geometry_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_POINTS_TO_VERTICES, "Points to Vertices", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_points_to_vertices_cc
