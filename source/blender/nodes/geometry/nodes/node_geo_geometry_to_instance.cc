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

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_geometry_to_instance_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry")).multi_input();
  b.add_output<decl::Geometry>(N_("Instances"));
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Vector<GeometrySet> geometries = params.extract_multi_input<GeometrySet>("Geometry");
  GeometrySet instances_geometry;
  InstancesComponent &instances_component =
      instances_geometry.get_component_for_write<InstancesComponent>();
  for (GeometrySet &geometry : geometries) {
    geometry.ensure_owns_direct_data();
    const int handle = instances_component.add_reference(std::move(geometry));
    instances_component.add_instance(handle, float4x4::identity());
  }
  params.set_output("Instances", std::move(instances_geometry));
}

}  // namespace blender::nodes::node_geo_geometry_to_instance_cc

void register_node_type_geo_geometry_to_instance()
{
  namespace file_ns = blender::nodes::node_geo_geometry_to_instance_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_GEOMETRY_TO_INSTANCE, "Geometry to Instance", NODE_CLASS_GEOMETRY);
  node_type_size(&ntype, 160, 100, 300);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
