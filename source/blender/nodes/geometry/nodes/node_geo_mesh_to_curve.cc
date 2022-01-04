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

#include "GEO_mesh_to_curve.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_to_curve_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_mesh()) {
      geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
      return;
    }

    const MeshComponent &component = *geometry_set.get_component_for_read<MeshComponent>();
    GeometryComponentFieldContext context{component, ATTR_DOMAIN_EDGE};
    fn::FieldEvaluator evaluator{context, component.attribute_domain_size(ATTR_DOMAIN_EDGE)};
    evaluator.add(params.get_input<Field<bool>>("Selection"));
    evaluator.evaluate();
    const IndexMask selection = evaluator.get_evaluated_as_mask(0);
    if (selection.size() == 0) {
      geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
      return;
    }

    std::unique_ptr<CurveEval> curve = geometry::mesh_to_curve_convert(component, selection);
    geometry_set.replace_curve(curve.release());
    geometry_set.keep_only({GEO_COMPONENT_TYPE_CURVE, GEO_COMPONENT_TYPE_INSTANCES});
  });

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_mesh_to_curve_cc

void register_node_type_geo_mesh_to_curve()
{
  namespace file_ns = blender::nodes::node_geo_mesh_to_curve_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_TO_CURVE, "Mesh to Curve", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
