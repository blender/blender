/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_attribute_math.hh"
#include "BKE_mesh.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_points_to_vertices_cc {

using blender::Array;

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Points")).supported_type(GEO_COMPONENT_TYPE_POINT_CLOUD);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).supports_field().hide_value();
  b.add_output<decl::Geometry>(N_("Mesh"));
}

/* One improvement would be to move the attribute arrays directly to the mesh when possible. */
static void geometry_set_points_to_vertices(GeometrySet &geometry_set,
                                            Field<bool> &selection_field)
{
  const PointCloudComponent *point_component =
      geometry_set.get_component_for_read<PointCloudComponent>();
  if (point_component == nullptr) {
    geometry_set.remove_geometry_during_modify();
    return;
  }

  GeometryComponentFieldContext field_context{*point_component, ATTR_DOMAIN_POINT};
  const int domain_num = point_component->attribute_domain_size(ATTR_DOMAIN_POINT);
  if (domain_num == 0) {
    geometry_set.remove_geometry_during_modify();
    return;
  }

  fn::FieldEvaluator selection_evaluator{field_context, domain_num};
  selection_evaluator.add(selection_field);
  selection_evaluator.evaluate();
  const IndexMask selection = selection_evaluator.get_evaluated_as_mask(0);

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation(
      {GEO_COMPONENT_TYPE_POINT_CLOUD}, GEO_COMPONENT_TYPE_MESH, false, attributes);

  Mesh *mesh = BKE_mesh_new_nomain(selection.size(), 0, 0, 0, 0);
  geometry_set.replace_mesh(mesh);
  MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();

  for (Map<AttributeIDRef, AttributeKind>::Item entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    const eCustomDataType data_type = entry.value.data_type;
    GVArray src = point_component->attributes()->lookup_or_default(
        attribute_id, ATTR_DOMAIN_POINT, data_type);
    GSpanAttributeWriter dst =
        mesh_component.attributes_for_write()->lookup_or_add_for_write_only_span(
            attribute_id, ATTR_DOMAIN_POINT, data_type);
    if (dst && src) {
      src.materialize_compressed_to_uninitialized(selection, dst.span.data());
      dst.finish();
    }
  }

  geometry_set.keep_only_during_modify({GEO_COMPONENT_TYPE_MESH});
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    geometry_set_points_to_vertices(geometry_set, selection_field);
  });

  params.set_output("Mesh", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_points_to_vertices_cc

void register_node_type_geo_points_to_vertices()
{
  namespace file_ns = blender::nodes::node_geo_points_to_vertices_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_POINTS_TO_VERTICES, "Points to Vertices", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
