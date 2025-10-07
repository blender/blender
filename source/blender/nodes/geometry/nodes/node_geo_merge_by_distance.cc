/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "GEO_foreach_geometry.hh"
#include "GEO_mesh_merge_by_distance.hh"
#include "GEO_point_merge_by_distance.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_merge_by_distance_cc {

NODE_STORAGE_FUNCS(NodeGeometryMergeByDistance)

static EnumPropertyItem mode_items[] = {
    {GEO_NODE_MERGE_BY_DISTANCE_MODE_ALL,
     "ALL",
     0,
     N_("All"),
     N_("Merge all close selected points, whether or not they are connected")},
    {GEO_NODE_MERGE_BY_DISTANCE_MODE_CONNECTED,
     "CONNECTED",
     0,
     N_("Connected"),
     N_("Only merge mesh vertices along existing edges. This method can be much faster")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Geometry")
      .supported_type({GeometryComponent::Type::PointCloud, GeometryComponent::Type::Mesh})
      .description("Point cloud or mesh to merge points of");
  b.add_output<decl::Geometry>("Geometry").propagate_all().align_with_previous();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Menu>("Mode").static_items(mode_items).optional_label();
  b.add_input<decl::Float>("Distance").default_value(0.001f).min(0.0f).subtype(PROP_DISTANCE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  /* Still used for forward compatibility. */
  node->storage = MEM_callocN<NodeGeometryMergeByDistance>(__func__);
}

static PointCloud *pointcloud_merge_by_distance(const PointCloud &src_points,
                                                const float merge_distance,
                                                const Field<bool> &selection_field,
                                                const AttributeFilter &attribute_filter)
{
  const bke::PointCloudFieldContext context{src_points};
  FieldEvaluator evaluator{context, src_points.totpoint};
  evaluator.add(selection_field);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_as_mask(0);
  if (selection.is_empty()) {
    return nullptr;
  }

  return geometry::point_merge_by_distance(
      src_points, merge_distance, selection, attribute_filter);
}

static std::optional<Mesh *> mesh_merge_by_distance_connected(const Mesh &mesh,
                                                              const float merge_distance,
                                                              const Field<bool> &selection_field)
{
  Array<bool> selection(mesh.verts_num);
  const bke::MeshFieldContext context{mesh, AttrDomain::Point};
  FieldEvaluator evaluator{context, mesh.verts_num};
  evaluator.add_with_destination(selection_field, selection.as_mutable_span());
  evaluator.evaluate();

  return geometry::mesh_merge_by_distance_connected(mesh, selection, merge_distance, false);
}

static std::optional<Mesh *> mesh_merge_by_distance_all(const Mesh &mesh,
                                                        const float merge_distance,
                                                        const Field<bool> &selection_field)
{
  const bke::MeshFieldContext context{mesh, AttrDomain::Point};
  FieldEvaluator evaluator{context, mesh.verts_num};
  evaluator.add(selection_field);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_as_mask(0);
  if (selection.is_empty()) {
    return std::nullopt;
  }

  return geometry::mesh_merge_by_distance_all(mesh, selection, merge_distance);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const auto mode = params.get_input<GeometryNodeMergeByDistanceMode>("Mode");
  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");
  const float merge_distance = params.extract_input<float>("Distance");

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    if (const PointCloud *pointcloud = geometry_set.get_pointcloud()) {
      PointCloud *result = pointcloud_merge_by_distance(
          *pointcloud, merge_distance, selection, params.get_attribute_filter("Geometry"));
      if (result) {
        geometry_set.replace_pointcloud(result);
      }
    }
    if (const Mesh *mesh = geometry_set.get_mesh()) {
      std::optional<Mesh *> result;
      switch (mode) {
        case GEO_NODE_MERGE_BY_DISTANCE_MODE_ALL:
          result = mesh_merge_by_distance_all(*mesh, merge_distance, selection);
          break;
        case GEO_NODE_MERGE_BY_DISTANCE_MODE_CONNECTED:
          result = mesh_merge_by_distance_connected(*mesh, merge_distance, selection);
          break;
        default:
          BLI_assert_unreachable();
      }
      if (result) {
        geometry_set.replace_mesh(*result);
      }
    }
  });

  params.set_output("Geometry", std::move(geometry_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeMergeByDistance", GEO_NODE_MERGE_BY_DISTANCE);
  ntype.ui_name = "Merge by Distance";
  ntype.ui_description = "Merge vertices or points within a given distance";
  ntype.enum_name_legacy = "MERGE_BY_DISTANCE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(ntype,
                                  "NodeGeometryMergeByDistance",
                                  node_free_standard_storage,
                                  node_copy_standard_storage);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_merge_by_distance_cc
