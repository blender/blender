/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_map.hh"

#include "GEO_foreach_geometry.hh"
#include "GEO_mesh_merge_verts.hh"
#include "GEO_point_merge.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_merge_points_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Geometry"_ustr)
      .supported_type({GeometryComponent::Type::PointCloud, GeometryComponent::Type::Mesh})
      .description("Point cloud or mesh to merge points of");
  b.add_output<decl::Geometry>("Geometry"_ustr).propagate_all_geometry().align_with_previous();
  b.add_input<decl::Bool>("Selection"_ustr)
      .default_value(true)
      .hide_value()
      .evaluated_geometry_field();
  b.add_input<decl::Int>("Merge ID"_ustr)
      .evaluated_geometry_field()
      .default_input_type(NODE_DEFAULT_INPUT_INDEX_FIELD)
      .description("ID of group of the points to merge");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry"_ustr);
  const Field<int> group_id_field = params.extract_input<Field<int>>("Merge ID"_ustr);
  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection"_ustr);
  if (group_id_field.get_input_if<fn::IndexFieldInput>()) {
    params.set_output("Geometry"_ustr, std::move(geometry_set));
    return;
  }

  const AttributeFilter &attribute_filter = params.get_attribute_filter("Geometry"_ustr);

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    if (const PointCloud *pointcloud = geometry_set.get_pointcloud()) {
      const bke::PointCloudFieldContext context(*pointcloud);
      FieldEvaluator evaluator(context, pointcloud->totpoint);
      evaluator.add(group_id_field);
      evaluator.set_selection(selection_field);
      Array<int> masked_group_ids(pointcloud->totpoint);
      evaluator.add_with_destination(group_id_field, masked_group_ids.as_mutable_span());
      evaluator.evaluate();
      const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
      if (selection.is_empty()) {
        return;
      }
      PointCloud *new_points = geometry::merge_points(
          *pointcloud, selection, masked_group_ids, attribute_filter);
      geometry_set.replace_pointcloud(new_points);
    }
    if (const Mesh *mesh = geometry_set.get_mesh()) {
      const bke::MeshFieldContext context(*mesh, AttrDomain::Point);
      FieldEvaluator evaluator(context, mesh->verts_num);
      evaluator.add(group_id_field);
      evaluator.set_selection(selection_field);
      Array<int> masked_group_ids(mesh->verts_num);
      evaluator.add_with_destination(group_id_field, masked_group_ids.as_mutable_span());
      evaluator.evaluate();
      const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
      if (selection.is_empty()) {
        return;
      }
      Mesh *new_mesh = geometry::mesh_merge_verts(
          *mesh, selection, masked_group_ids, attribute_filter);
      geometry_set.replace_mesh(new_mesh);
    }
  });

  params.set_output("Geometry"_ustr, std::move(geometry_set));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeMergePoints"_ustr);
  ntype.ui_name = "Merge Points";
  ntype.ui_description = "Merge points of a point cloud or mesh based on group ID and selection.";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_merge_points_cc
