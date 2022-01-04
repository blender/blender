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

namespace blender::nodes::node_geo_bounding_box_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_output<decl::Geometry>(N_("Bounding Box"));
  b.add_output<decl::Vector>(N_("Min"));
  b.add_output<decl::Vector>(N_("Max"));
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  /* Compute the min and max of all realized geometry for the two
   * vector outputs, which are only meant to consider real geometry. */
  float3 min = float3(FLT_MAX);
  float3 max = float3(-FLT_MAX);
  geometry_set.compute_boundbox_without_instances(&min, &max);
  if (min == float3(FLT_MAX)) {
    params.set_output("Min", float3(0));
    params.set_output("Max", float3(0));
  }
  else {
    params.set_output("Min", min);
    params.set_output("Max", max);
  }

  /* Generate the bounding box meshes inside each unique geometry set (including individually for
   * every instance). Because geometry components are reference counted anyway, we can just
   * repurpose the original geometry sets for the output. */
  if (params.output_is_required("Bounding Box")) {
    geometry_set.modify_geometry_sets([&](GeometrySet &sub_geometry) {
      float3 sub_min = float3(FLT_MAX);
      float3 sub_max = float3(-FLT_MAX);

      /* Reuse the min and max calculation if this is the main "real" geometry set. */
      if (&sub_geometry == &geometry_set) {
        sub_min = min;
        sub_max = max;
      }
      else {
        sub_geometry.compute_boundbox_without_instances(&sub_min, &sub_max);
      }

      if (sub_min == float3(FLT_MAX)) {
        sub_geometry.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
      }
      else {
        const float3 scale = sub_max - sub_min;
        const float3 center = sub_min + scale / 2.0f;
        Mesh *mesh = create_cuboid_mesh(scale, 2, 2, 2);
        transform_mesh(*mesh, center, float3(0), float3(1));
        sub_geometry.replace_mesh(mesh);
        sub_geometry.keep_only({GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_INSTANCES});
      }
    });

    params.set_output("Bounding Box", std::move(geometry_set));
  }
}

}  // namespace blender::nodes::node_geo_bounding_box_cc

void register_node_type_geo_bounding_box()
{
  namespace file_ns = blender::nodes::node_geo_bounding_box_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_BOUNDING_BOX, "Bounding Box", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
