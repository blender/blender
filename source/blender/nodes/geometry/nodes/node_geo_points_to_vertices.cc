/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BLI_task.hh"

#include "BKE_attribute_math.hh"
#include "BKE_mesh.h"

#include "node_geometry_util.hh"

using blender::Array;

namespace blender::nodes {

static void geo_node_points_to_vertices_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Points").supported_type(GEO_COMPONENT_TYPE_POINT_CLOUD);
  b.add_input<decl::Bool>("Selection").default_value(true).supports_field().hide_value();
  b.add_output<decl::Geometry>("Mesh");
}

template<typename T>
static void copy_attribute_to_vertices(const Span<T> src, const IndexMask mask, MutableSpan<T> dst)
{
  for (const int i : mask.index_range()) {
    dst[i] = src[mask[i]];
  }
}

/* One improvement would be to move the attribute arrays directly to the mesh when possible. */
static void geometry_set_points_to_vertices(GeometrySet &geometry_set,
                                            Field<bool> &selection_field)
{
  const PointCloudComponent *point_component =
      geometry_set.get_component_for_read<PointCloudComponent>();
  if (point_component == nullptr) {
    geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
    return;
  }

  GeometryComponentFieldContext field_context{*point_component, ATTR_DOMAIN_POINT};
  const int domain_size = point_component->attribute_domain_size(ATTR_DOMAIN_POINT);
  if (domain_size == 0) {
    geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
    return;
  }

  fn::FieldEvaluator selection_evaluator{field_context, domain_size};
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
    const CustomDataType data_type = entry.value.data_type;
    GVArrayPtr src = point_component->attribute_get_for_read(
        attribute_id, ATTR_DOMAIN_POINT, data_type);
    OutputAttribute dst = mesh_component.attribute_try_get_for_output_only(
        attribute_id, ATTR_DOMAIN_POINT, data_type);
    if (dst && src) {
      attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
        using T = decltype(dummy);
        GVArray_Typed<T> src_typed{*src};
        VArray_Span<T> src_typed_span{*src_typed};
        copy_attribute_to_vertices(src_typed_span, selection, dst.as_span().typed<T>());
      });
      dst.save();
    }
  }

  geometry_set.keep_only({GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_INSTANCES});
}

static void geo_node_points_to_vertices_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    geometry_set_points_to_vertices(geometry_set, selection_field);
  });

  params.set_output("Mesh", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_points_to_vertices()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_POINTS_TO_VERTICES, "Points to Vertices", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_points_to_vertices_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_points_to_vertices_exec;
  nodeRegisterType(&ntype);
}
