/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_mesh_merge_by_distance.hh"
#include "GEO_point_merge_by_distance.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_merge_by_distance_cc {

NODE_STORAGE_FUNCS(NodeGeometryMergeByDistance)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"))
      .supported_type({GEO_COMPONENT_TYPE_POINT_CLOUD, GEO_COMPONENT_TYPE_MESH});
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_input<decl::Float>(N_("Distance")).default_value(0.001f).min(0.0f).subtype(PROP_DISTANCE);
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryMergeByDistance *data = MEM_cnew<NodeGeometryMergeByDistance>(__func__);
  data->mode = GEO_NODE_MERGE_BY_DISTANCE_MODE_ALL;
  node->storage = data;
}

static PointCloud *pointcloud_merge_by_distance(const PointCloudComponent &src_points,
                                                const float merge_distance,
                                                const Field<bool> &selection_field)
{
  const int src_num = src_points.attribute_domain_size(ATTR_DOMAIN_POINT);
  GeometryComponentFieldContext context{src_points, ATTR_DOMAIN_POINT};
  FieldEvaluator evaluator{context, src_num};
  evaluator.add(selection_field);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_as_mask(0);
  if (selection.is_empty()) {
    return nullptr;
  }

  return geometry::point_merge_by_distance(src_points, merge_distance, selection);
}

static std::optional<Mesh *> mesh_merge_by_distance_connected(const MeshComponent &mesh_component,
                                                              const float merge_distance,
                                                              const Field<bool> &selection_field)
{
  const int src_num = mesh_component.attribute_domain_size(ATTR_DOMAIN_POINT);
  Array<bool> selection(src_num);
  GeometryComponentFieldContext context{mesh_component, ATTR_DOMAIN_POINT};
  FieldEvaluator evaluator{context, src_num};
  evaluator.add_with_destination(selection_field, selection.as_mutable_span());
  evaluator.evaluate();

  const Mesh &mesh = *mesh_component.get_for_read();
  return geometry::mesh_merge_by_distance_connected(mesh, selection, merge_distance, false);
}

static std::optional<Mesh *> mesh_merge_by_distance_all(const MeshComponent &mesh_component,
                                                        const float merge_distance,
                                                        const Field<bool> &selection_field)
{
  const int src_num = mesh_component.attribute_domain_size(ATTR_DOMAIN_POINT);
  GeometryComponentFieldContext context{mesh_component, ATTR_DOMAIN_POINT};
  FieldEvaluator evaluator{context, src_num};
  evaluator.add(selection_field);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_as_mask(0);
  if (selection.is_empty()) {
    return std::nullopt;
  }

  const Mesh &mesh = *mesh_component.get_for_read();
  return geometry::mesh_merge_by_distance_all(mesh, selection, merge_distance);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryMergeByDistance &storage = node_storage(params.node());
  const GeometryNodeMergeByDistanceMode mode = (GeometryNodeMergeByDistanceMode)storage.mode;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");
  const float merge_distance = params.extract_input<float>("Distance");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_pointcloud()) {
      PointCloud *result = pointcloud_merge_by_distance(
          *geometry_set.get_component_for_read<PointCloudComponent>(), merge_distance, selection);
      if (result) {
        geometry_set.replace_pointcloud(result);
      }
    }
    if (geometry_set.has_mesh()) {
      const MeshComponent &component = *geometry_set.get_component_for_read<MeshComponent>();
      std::optional<Mesh *> result;
      switch (mode) {
        case GEO_NODE_MERGE_BY_DISTANCE_MODE_ALL:
          result = mesh_merge_by_distance_all(component, merge_distance, selection);
          break;
        case GEO_NODE_MERGE_BY_DISTANCE_MODE_CONNECTED:
          result = mesh_merge_by_distance_connected(component, merge_distance, selection);
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

}  // namespace blender::nodes::node_geo_merge_by_distance_cc

void register_node_type_geo_merge_by_distance()
{
  namespace file_ns = blender::nodes::node_geo_merge_by_distance_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MERGE_BY_DISTANCE, "Merge by Distance", NODE_CLASS_GEOMETRY);
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(&ntype,
                    "NodeGeometryMergeByDistance",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
