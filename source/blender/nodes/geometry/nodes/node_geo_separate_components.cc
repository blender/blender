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

namespace blender::nodes {

static void geo_node_join_geometry_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_output<decl::Geometry>(N_("Mesh"));
  b.add_output<decl::Geometry>(N_("Point Cloud"));
  b.add_output<decl::Geometry>(N_("Curve"));
  b.add_output<decl::Geometry>(N_("Volume"));
  b.add_output<decl::Geometry>(N_("Instances"));
}

static void geo_node_separate_components_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  GeometrySet meshes;
  GeometrySet point_clouds;
  GeometrySet volumes;
  GeometrySet curves;
  GeometrySet instances;

  if (geometry_set.has<MeshComponent>()) {
    meshes.add(*geometry_set.get_component_for_read<MeshComponent>());
  }
  if (geometry_set.has<PointCloudComponent>()) {
    point_clouds.add(*geometry_set.get_component_for_read<PointCloudComponent>());
  }
  if (geometry_set.has<CurveComponent>()) {
    curves.add(*geometry_set.get_component_for_read<CurveComponent>());
  }
  if (geometry_set.has<VolumeComponent>()) {
    volumes.add(*geometry_set.get_component_for_read<VolumeComponent>());
  }
  if (geometry_set.has<InstancesComponent>()) {
    instances.add(*geometry_set.get_component_for_read<InstancesComponent>());
  }

  params.set_output("Mesh", meshes);
  params.set_output("Point Cloud", point_clouds);
  params.set_output("Curve", curves);
  params.set_output("Volume", volumes);
  params.set_output("Instances", instances);
}

}  // namespace blender::nodes

void register_node_type_geo_separate_components()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SEPARATE_COMPONENTS, "Separate Components", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_join_geometry_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_separate_components_exec;
  nodeRegisterType(&ntype);
}
