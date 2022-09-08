/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_pointcloud.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

using blender::Array;

namespace blender::nodes::node_geo_mesh_to_points_cc {

NODE_STORAGE_FUNCS(NodeGeometryMeshToPoints)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).supports_field().hide_value();
  b.add_input<decl::Vector>(N_("Position")).implicit_field();
  b.add_input<decl::Float>(N_("Radius"))
      .default_value(0.05f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .supports_field();
  b.add_output<decl::Geometry>(N_("Points"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryMeshToPoints *data = MEM_cnew<NodeGeometryMeshToPoints>(__func__);
  data->mode = GEO_NODE_MESH_TO_POINTS_VERTICES;
  node->storage = data;
}

static void materialize_compressed_to_uninitialized_threaded(const GVArray &src,
                                                             const IndexMask mask,
                                                             GMutableSpan dst)
{
  BLI_assert(src.type() == dst.type());
  BLI_assert(mask.size() == dst.size());
  threading::parallel_for(mask.index_range(), 4096, [&](IndexRange range) {
    src.materialize_compressed_to_uninitialized(mask.slice(range), dst.slice(range).data());
  });
}

static void geometry_set_mesh_to_points(GeometrySet &geometry_set,
                                        Field<float3> &position_field,
                                        Field<float> &radius_field,
                                        Field<bool> &selection_field,
                                        const eAttrDomain domain)
{
  const Mesh *mesh = geometry_set.get_mesh_for_read();
  if (mesh == nullptr) {
    geometry_set.remove_geometry_during_modify();
    return;
  }
  const int domain_size = bke::mesh_attributes(*mesh).domain_size(domain);
  if (domain_size == 0) {
    geometry_set.remove_geometry_during_modify();
    return;
  }
  bke::MeshFieldContext field_context{*mesh, domain};
  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);
  /* Evaluating directly into the point cloud doesn't work because we are not using the full
   * "min_array_size" array but compressing the selected elements into the final array with no
   * gaps. */
  evaluator.add(position_field);
  evaluator.add(radius_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(selection.size());
  geometry_set.replace_pointcloud(pointcloud);
  MutableAttributeAccessor dst_attributes = bke::pointcloud_attributes_for_write(*pointcloud);

  GSpanAttributeWriter position = dst_attributes.lookup_or_add_for_write_only_span(
      "position", ATTR_DOMAIN_POINT, CD_PROP_FLOAT3);
  materialize_compressed_to_uninitialized_threaded(
      evaluator.get_evaluated(0), selection, position.span);
  position.finish();

  GSpanAttributeWriter radius = dst_attributes.lookup_or_add_for_write_only_span(
      "radius", ATTR_DOMAIN_POINT, CD_PROP_FLOAT);
  materialize_compressed_to_uninitialized_threaded(
      evaluator.get_evaluated(1), selection, radius.span);
  radius.finish();

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation(
      {GEO_COMPONENT_TYPE_MESH}, GEO_COMPONENT_TYPE_POINT_CLOUD, false, attributes);
  attributes.remove("position");

  const AttributeAccessor src_attributes = bke::mesh_attributes(*mesh);

  for (Map<AttributeIDRef, AttributeKind>::Item entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    const eCustomDataType data_type = entry.value.data_type;
    GVArray src = src_attributes.lookup_or_default(attribute_id, domain, data_type);
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        attribute_id, ATTR_DOMAIN_POINT, data_type);
    if (dst && src) {
      materialize_compressed_to_uninitialized_threaded(src, selection, dst.span);
      dst.finish();
    }
  }

  geometry_set.keep_only_during_modify({GEO_COMPONENT_TYPE_POINT_CLOUD});
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
  Field<float3> position = params.extract_input<Field<float3>>("Position");
  Field<float> radius = params.extract_input<Field<float>>("Radius");
  Field<bool> selection = params.extract_input<Field<bool>>("Selection");

  /* Use another multi-function operation to make sure the input radius is greater than zero.
   * TODO: Use mutable multi-function once that is supported. */
  static fn::CustomMF_SI_SO<float, float> max_zero_fn(
      __func__,
      [](float value) { return std::max(0.0f, value); },
      fn::CustomMF_presets::AllSpanOrSingle());
  auto max_zero_op = std::make_shared<FieldOperation>(
      FieldOperation(max_zero_fn, {std::move(radius)}));
  Field<float> positive_radius(std::move(max_zero_op), 0);

  const NodeGeometryMeshToPoints &storage = node_storage(params.node());
  const GeometryNodeMeshToPointsMode mode = (GeometryNodeMeshToPointsMode)storage.mode;

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    switch (mode) {
      case GEO_NODE_MESH_TO_POINTS_VERTICES:
        geometry_set_mesh_to_points(
            geometry_set, position, positive_radius, selection, ATTR_DOMAIN_POINT);
        break;
      case GEO_NODE_MESH_TO_POINTS_EDGES:
        geometry_set_mesh_to_points(
            geometry_set, position, positive_radius, selection, ATTR_DOMAIN_EDGE);
        break;
      case GEO_NODE_MESH_TO_POINTS_FACES:
        geometry_set_mesh_to_points(
            geometry_set, position, positive_radius, selection, ATTR_DOMAIN_FACE);
        break;
      case GEO_NODE_MESH_TO_POINTS_CORNERS:
        geometry_set_mesh_to_points(
            geometry_set, position, positive_radius, selection, ATTR_DOMAIN_CORNER);
        break;
    }
  });

  params.set_output("Points", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_mesh_to_points_cc

void register_node_type_geo_mesh_to_points()
{
  namespace file_ns = blender::nodes::node_geo_mesh_to_points_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_TO_POINTS, "Mesh to Points", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  node_type_init(&ntype, file_ns::node_init);
  ntype.draw_buttons = file_ns::node_layout;
  node_type_storage(
      &ntype, "NodeGeometryMeshToPoints", node_free_standard_storage, node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
