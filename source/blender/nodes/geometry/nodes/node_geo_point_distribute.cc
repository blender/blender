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

#include "BLI_float3.hh"
#include "BLI_hash.h"
#include "BLI_math_vector.h"
#include "BLI_rand.hh"
#include "BLI_span.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_pointcloud.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_point_distribute_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_FLOAT, N_("Density"), 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 100000.0f, PROP_NONE},
    {SOCK_STRING, N_("Density Attribute")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_point_distribute_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static Vector<float3> scatter_points_from_mesh(const Mesh *mesh,
                                               const float density,
                                               const FloatReadAttribute &density_factors)
{
  /* This only updates a cache and can be considered to be logically const. */
  const MLoopTri *looptris = BKE_mesh_runtime_looptri_ensure(const_cast<Mesh *>(mesh));
  const int looptris_len = BKE_mesh_runtime_looptri_len(mesh);

  Vector<float3> points;

  for (const int looptri_index : IndexRange(looptris_len)) {
    const MLoopTri &looptri = looptris[looptri_index];
    const int v0_index = mesh->mloop[looptri.tri[0]].v;
    const int v1_index = mesh->mloop[looptri.tri[1]].v;
    const int v2_index = mesh->mloop[looptri.tri[2]].v;
    const float3 v0_pos = mesh->mvert[v0_index].co;
    const float3 v1_pos = mesh->mvert[v1_index].co;
    const float3 v2_pos = mesh->mvert[v2_index].co;
    const float v0_density_factor = std::max(0.0f, density_factors[v0_index]);
    const float v1_density_factor = std::max(0.0f, density_factors[v1_index]);
    const float v2_density_factor = std::max(0.0f, density_factors[v2_index]);
    const float looptri_density_factor = (v0_density_factor + v1_density_factor +
                                          v2_density_factor) /
                                         3.0f;
    const float area = area_tri_v3(v0_pos, v1_pos, v2_pos);

    const int looptri_seed = BLI_hash_int(looptri_index);
    RandomNumberGenerator looptri_rng(looptri_seed);

    const float points_amount_fl = area * density * looptri_density_factor;
    const float add_point_probability = fractf(points_amount_fl);
    const bool add_point = add_point_probability > looptri_rng.get_float();
    const int point_amount = (int)points_amount_fl + (int)add_point;

    for (int i = 0; i < point_amount; i++) {
      const float3 bary_coords = looptri_rng.get_barycentric_coordinates();
      float3 point_pos;
      interp_v3_v3v3v3(point_pos, v0_pos, v1_pos, v2_pos, bary_coords);
      points.append(point_pos);
    }
  }

  return points;
}

static void geo_node_point_distribute_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  GeometrySet geometry_set_out;

  if (!geometry_set.has_mesh()) {
    params.set_output("Geometry", std::move(geometry_set_out));
    return;
  }

  const float density = params.extract_input<float>("Density");
  const std::string density_attribute = params.extract_input<std::string>("Density Attribute");

  if (density <= 0.0f) {
    params.set_output("Geometry", std::move(geometry_set_out));
    return;
  }

  const MeshComponent &mesh_component = *geometry_set.get_component_for_read<MeshComponent>();
  const Mesh *mesh_in = mesh_component.get_for_read();

  const FloatReadAttribute density_factors = mesh_component.attribute_get_for_read<float>(
      density_attribute, ATTR_DOMAIN_POINT, 1.0f);

  Vector<float3> points = scatter_points_from_mesh(mesh_in, density, density_factors);

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(points.size());
  memcpy(pointcloud->co, points.data(), sizeof(float3) * points.size());
  for (const int i : points.index_range()) {
    *(float3 *)(pointcloud->co + i) = points[i];
    pointcloud->radius[i] = 0.05f;
  }

  geometry_set_out.replace_pointcloud(pointcloud);
  params.set_output("Geometry", std::move(geometry_set_out));
}
}  // namespace blender::nodes

void register_node_type_geo_point_distribute()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_POINT_DISTRIBUTE, "Point Distribute", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_point_distribute_in, geo_node_point_distribute_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_point_distribute_exec;
  nodeRegisterType(&ntype);
}
