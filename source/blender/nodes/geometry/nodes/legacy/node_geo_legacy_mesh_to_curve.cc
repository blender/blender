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

namespace blender::nodes::node_geo_legacy_mesh_to_curve_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh"));
  b.add_input<decl::String>(N_("Selection"));
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

  geometry_set = geometry::realize_instances_legacy(geometry_set);

  if (!geometry_set.has_mesh()) {
    params.set_default_remaining_outputs();
    return;
  }

  const MeshComponent &component = *geometry_set.get_component_for_read<MeshComponent>();
  const std::string selection_name = params.extract_input<std::string>("Selection");
  if (!selection_name.empty() && !component.attribute_exists(selection_name)) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("No attribute with name \"") + selection_name + "\"");
  }
  VArray<bool> selection = component.attribute_get_for_read<bool>(
      selection_name, ATTR_DOMAIN_EDGE, true);

  Vector<int64_t> selected_edge_indices;
  for (const int64_t i : IndexRange(component.attribute_domain_size(ATTR_DOMAIN_EDGE))) {
    if (selection[i]) {
      selected_edge_indices.append(i);
    }
  }

  if (selected_edge_indices.size() == 0) {
    params.set_default_remaining_outputs();
    return;
  }

  std::unique_ptr<CurveEval> curve = geometry::mesh_to_curve_convert(
      component, IndexMask(selected_edge_indices));

  params.set_output("Curve", GeometrySet::create_with_curve(curve.release()));
}

}  // namespace blender::nodes::node_geo_legacy_mesh_to_curve_cc

void register_node_type_geo_legacy_mesh_to_curve()
{
  namespace file_ns = blender::nodes::node_geo_legacy_mesh_to_curve_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_LEGACY_MESH_TO_CURVE, "Mesh to Curve", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
