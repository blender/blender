/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_mesh.hh"
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
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).field_on_all().hide_value();
  b.add_input<decl::Vector>(N_("Position")).implicit_field_on_all(implicit_field_inputs::position);
  b.add_input<decl::Float>(N_("Radius"))
      .default_value(0.05f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .field_on_all();
  b.add_output<decl::Geometry>(N_("Points")).propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryMeshToPoints *data = MEM_cnew<NodeGeometryMeshToPoints>(__func__);
  data->mode = GEO_NODE_MESH_TO_POINTS_VERTICES;
  node->storage = data;
}

static void geometry_set_mesh_to_points(GeometrySet &geometry_set,
                                        const Field<float3> &position_field,
                                        const Field<float> &radius_field,
                                        const Field<bool> &selection_field,
                                        const eAttrDomain domain,
                                        const AnonymousAttributePropagationInfo &propagation_info)
{
  const Mesh *mesh = geometry_set.get_mesh_for_read();
  if (mesh == nullptr) {
    geometry_set.remove_geometry_during_modify();
    return;
  }
  const int domain_size = mesh->attributes().domain_size(domain);
  if (domain_size == 0) {
    geometry_set.remove_geometry_during_modify();
    return;
  }
  const AttributeAccessor src_attributes = mesh->attributes();
  const bke::MeshFieldContext field_context{*mesh, domain};
  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);
  /* Evaluating directly into the point cloud doesn't work because we are not using the full
   * "min_array_size" array but compressing the selected elements into the final array with no
   * gaps. */
  evaluator.add(position_field);
  evaluator.add(radius_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const VArray<float3> positions_eval = evaluator.get_evaluated<float3>(0);
  const VArray<float> radii_eval = evaluator.get_evaluated<float>(1);

  const bool share_arrays = selection.size() == domain_size;
  const bool share_position = share_arrays && positions_eval.is_span() &&
                              positions_eval.get_internal_span().data() ==
                                  mesh->vert_positions().data();

  PointCloud *pointcloud;
  if (share_position) {
    /* Create an empty point cloud so the positions can be shared. */
    pointcloud = BKE_pointcloud_new_nomain(0);
    CustomData_free_layer_named(&pointcloud->pdata, "position", pointcloud->totpoint);
    pointcloud->totpoint = mesh->totvert;
    const bke::AttributeReader src = src_attributes.lookup<float3>("position");
    const bke::AttributeInitShared init(src.varray.get_internal_span().data(), *src.sharing_info);
    pointcloud->attributes_for_write().add<float3>("position", ATTR_DOMAIN_POINT, init);
  }
  else {
    pointcloud = BKE_pointcloud_new_nomain(selection.size());
    array_utils::gather(positions_eval, selection, pointcloud->positions_for_write());
  }

  MutableAttributeAccessor dst_attributes = pointcloud->attributes_for_write();
  GSpanAttributeWriter radius = dst_attributes.lookup_or_add_for_write_only_span(
      "radius", ATTR_DOMAIN_POINT, CD_PROP_FLOAT);
  array_utils::gather(evaluator.get_evaluated(1), selection, radius.span);
  radius.finish();

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation({GEO_COMPONENT_TYPE_MESH},
                                                 GEO_COMPONENT_TYPE_POINT_CLOUD,
                                                 false,
                                                 propagation_info,
                                                 attributes);
  attributes.remove("radius");
  attributes.remove("position");

  for (MapItem<AttributeIDRef, AttributeKind> entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    const eCustomDataType data_type = entry.value.data_type;
    const bke::GAttributeReader src = src_attributes.lookup(attribute_id, domain, data_type);
    if (!src) {
      /* Domain interpolation can fail if the source domain is empty. */
      continue;
    }

    if (share_arrays && src.domain == domain && src.sharing_info && src.varray.is_span()) {
      const bke::AttributeInitShared init(src.varray.get_internal_span().data(),
                                          *src.sharing_info);
      dst_attributes.add(attribute_id, ATTR_DOMAIN_POINT, data_type, init);
    }
    else {
      GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
          attribute_id, ATTR_DOMAIN_POINT, data_type);
      array_utils::gather(src.varray, selection, dst.span);
      dst.finish();
    }
  }

  geometry_set.replace_pointcloud(pointcloud);
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
  static auto max_zero_fn = mf::build::SI1_SO<float, float>(
      __func__,
      [](float value) { return std::max(0.0f, value); },
      mf::build::exec_presets::AllSpanOrSingle());
  const Field<float> positive_radius(FieldOperation::Create(max_zero_fn, {std::move(radius)}), 0);

  const NodeGeometryMeshToPoints &storage = node_storage(params.node());
  const GeometryNodeMeshToPointsMode mode = (GeometryNodeMeshToPointsMode)storage.mode;

  const AnonymousAttributePropagationInfo &propagation_info = params.get_output_propagation_info(
      "Points");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    switch (mode) {
      case GEO_NODE_MESH_TO_POINTS_VERTICES:
        geometry_set_mesh_to_points(geometry_set,
                                    position,
                                    positive_radius,
                                    selection,
                                    ATTR_DOMAIN_POINT,
                                    propagation_info);
        break;
      case GEO_NODE_MESH_TO_POINTS_EDGES:
        geometry_set_mesh_to_points(geometry_set,
                                    position,
                                    positive_radius,
                                    selection,
                                    ATTR_DOMAIN_EDGE,
                                    propagation_info);
        break;
      case GEO_NODE_MESH_TO_POINTS_FACES:
        geometry_set_mesh_to_points(geometry_set,
                                    position,
                                    positive_radius,
                                    selection,
                                    ATTR_DOMAIN_FACE,
                                    propagation_info);
        break;
      case GEO_NODE_MESH_TO_POINTS_CORNERS:
        geometry_set_mesh_to_points(geometry_set,
                                    position,
                                    positive_radius,
                                    selection,
                                    ATTR_DOMAIN_CORNER,
                                    propagation_info);
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
  ntype.initfunc = file_ns::node_init;
  ntype.draw_buttons = file_ns::node_layout;
  node_type_storage(
      &ntype, "NodeGeometryMeshToPoints", node_free_standard_storage, node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
