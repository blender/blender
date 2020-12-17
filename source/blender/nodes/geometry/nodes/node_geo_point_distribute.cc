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

#include "BKE_bvhutils.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_pointcloud.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_point_distribute_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_FLOAT, N_("Distance Min"), 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 100000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Density Max"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 100000.0f, PROP_NONE},
    {SOCK_STRING, N_("Density Attribute")},
    {SOCK_INT, N_("Seed"), 0, 0, 0, 0, -10000, 10000},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_point_distribute_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void node_point_distribute_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock_min_dist = (bNodeSocket *)BLI_findlink(&node->inputs, 1);

  nodeSetSocketAvailability(sock_min_dist, ELEM(node->custom1, GEO_NODE_POINT_DISTRIBUTE_POISSON));
}

namespace blender::nodes {

static Vector<float3> random_scatter_points_from_mesh(const Mesh *mesh,
                                                      const float density,
                                                      const FloatReadAttribute &density_factors,
                                                      Vector<int> &r_ids,
                                                      const int seed)
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

    const int looptri_seed = BLI_hash_int(looptri_index + seed);
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

      /* Build a hash stable even when the mesh is deformed. */
      r_ids.append(((int)(bary_coords.hash()) + looptri_index));
    }
  }

  return points;
}

struct RayCastAll_Data {
  void *bvhdata;

  BVHTree_RayCastCallback raycast_callback;

  /** The original coordinate the result point was projected from. */
  float2 raystart;

  const Mesh *mesh;
  float base_weight;
  FloatReadAttribute *density_factors;
  Vector<float3> *projected_points;
  Vector<int> *stable_ids;
  float cur_point_weight;
};

static void project_2d_bvh_callback(void *userdata,
                                    int index,
                                    const BVHTreeRay *ray,
                                    BVHTreeRayHit *hit)
{
  struct RayCastAll_Data *data = (RayCastAll_Data *)userdata;
  data->raycast_callback(data->bvhdata, index, ray, hit);
  if (hit->index != -1) {
    /* This only updates a cache and can be considered to be logically const. */
    const MLoopTri *looptris = BKE_mesh_runtime_looptri_ensure(const_cast<Mesh *>(data->mesh));
    const MVert *mvert = data->mesh->mvert;

    const MLoopTri &looptri = looptris[index];
    const FloatReadAttribute &density_factors = data->density_factors[0];

    const int v0_index = data->mesh->mloop[looptri.tri[0]].v;
    const int v1_index = data->mesh->mloop[looptri.tri[1]].v;
    const int v2_index = data->mesh->mloop[looptri.tri[2]].v;

    const float v0_density_factor = std::max(0.0f, density_factors[v0_index]);
    const float v1_density_factor = std::max(0.0f, density_factors[v1_index]);
    const float v2_density_factor = std::max(0.0f, density_factors[v2_index]);

    /* Calculate barycentric weights for hit point. */
    float3 weights;
    interp_weights_tri_v3(
        weights, mvert[v0_index].co, mvert[v1_index].co, mvert[v2_index].co, hit->co);

    float point_weight = weights[0] * v0_density_factor + weights[1] * v1_density_factor +
                         weights[2] * v2_density_factor;

    point_weight *= data->base_weight;

    if (point_weight >= FLT_EPSILON && data->cur_point_weight <= point_weight) {
      data->projected_points->append(hit->co);

      /* Build a hash stable even when the mesh is deformed. */
      data->stable_ids->append((int)data->raystart.hash());
    }
  }
}

static Vector<float3> poisson_scatter_points_from_mesh(const Mesh *mesh,
                                                       const float density,
                                                       const float minimum_distance,
                                                       const FloatReadAttribute &density_factors,
                                                       Vector<int> &r_ids,
                                                       const int seed)
{
  Vector<float3> points;

  if (minimum_distance <= FLT_EPSILON || density <= FLT_EPSILON) {
    return points;
  }

  /* Scatter points randomly on the mesh with higher density (5-7) times higher than desired for
   * good quality possion disk distributions. */
  int quality = 5;
  const int output_points_target = 1000;
  points.resize(output_points_target * quality);

  const float required_area = output_points_target *
                              (2.0f * sqrtf(3.0f) * minimum_distance * minimum_distance);
  const float point_scale_multiplier = sqrtf(required_area);

  {
    const int rnd_seed = BLI_hash_int(seed);
    RandomNumberGenerator point_rng(rnd_seed);

    for (int i = 0; i < points.size(); i++) {
      points[i].x = point_rng.get_float() * point_scale_multiplier;
      points[i].y = point_rng.get_float() * point_scale_multiplier;
      points[i].z = 0.0f;
    }
  }

  /* Eliminate the scattered points until we get a possion distribution. */
  Vector<float3> output_points(output_points_target);

  const float3 bounds_max = float3(point_scale_multiplier, point_scale_multiplier, 0);
  poisson_disk_point_elimination(&points, &output_points, 2.0f * minimum_distance, bounds_max);
  Vector<float3> final_points;
  r_ids.reserve(output_points_target);
  final_points.reserve(output_points_target);

  /* Check if we have any points we should remove from the final possion distribition. */
  BVHTreeFromMesh treedata;
  BKE_bvhtree_from_mesh_get(&treedata, const_cast<Mesh *>(mesh), BVHTREE_FROM_LOOPTRI, 2);

  float3 bb_min, bb_max;
  BLI_bvhtree_get_bounding_box(treedata.tree, bb_min, bb_max);

  struct RayCastAll_Data data;
  data.bvhdata = &treedata;
  data.raycast_callback = treedata.raycast_callback;
  data.mesh = mesh;
  data.projected_points = &final_points;
  data.stable_ids = &r_ids;
  data.density_factors = const_cast<FloatReadAttribute *>(&density_factors);
  data.base_weight = std::min(
      1.0f, density / (output_points.size() / (point_scale_multiplier * point_scale_multiplier)));

  const float max_dist = bb_max[2] - bb_min[2] + 2.0f;
  const float3 dir = float3(0, 0, -1);
  float3 raystart;
  raystart.z = bb_max[2] + 1.0f;

  float tile_start_x_coord = bb_min[0];
  int tile_repeat_x = ceilf((bb_max[0] - bb_min[0]) / point_scale_multiplier);

  float tile_start_y_coord = bb_min[1];
  int tile_repeat_y = ceilf((bb_max[1] - bb_min[1]) / point_scale_multiplier);

  for (int x = 0; x < tile_repeat_x; x++) {
    float tile_curr_x_coord = x * point_scale_multiplier + tile_start_x_coord;
    for (int y = 0; y < tile_repeat_y; y++) {
      float tile_curr_y_coord = y * point_scale_multiplier + tile_start_y_coord;
      for (int idx = 0; idx < output_points.size(); idx++) {
        raystart.x = output_points[idx].x + tile_curr_x_coord;
        raystart.y = output_points[idx].y + tile_curr_y_coord;

        data.cur_point_weight = (float)idx / (float)output_points.size();
        data.raystart = raystart;

        BLI_bvhtree_ray_cast_all(
            treedata.tree, raystart, dir, 0.0f, max_dist, project_2d_bvh_callback, &data);
      }
    }
  }

  return final_points;
}

static void geo_node_point_distribute_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  GeometrySet geometry_set_out;

  GeometryNodePointDistributeMethod distribute_method =
      static_cast<GeometryNodePointDistributeMethod>(params.node().custom1);

  if (!geometry_set.has_mesh()) {
    params.set_output("Geometry", std::move(geometry_set_out));
    return;
  }

  const float density = params.extract_input<float>("Density Max");
  const std::string density_attribute = params.extract_input<std::string>("Density Attribute");

  if (density <= 0.0f) {
    params.set_output("Geometry", std::move(geometry_set_out));
    return;
  }

  const MeshComponent &mesh_component = *geometry_set.get_component_for_read<MeshComponent>();
  const Mesh *mesh_in = mesh_component.get_for_read();

  const FloatReadAttribute density_factors = mesh_component.attribute_get_for_read<float>(
      density_attribute, ATTR_DOMAIN_POINT, 1.0f);
  const int seed = params.get_input<int>("Seed");

  Vector<int> stable_ids;
  Vector<float3> points;
  switch (distribute_method) {
    case GEO_NODE_POINT_DISTRIBUTE_RANDOM:
      points = random_scatter_points_from_mesh(
          mesh_in, density, density_factors, stable_ids, seed);
      break;
    case GEO_NODE_POINT_DISTRIBUTE_POISSON:
      const float min_dist = params.extract_input<float>("Distance Min");
      points = poisson_scatter_points_from_mesh(
          mesh_in, density, min_dist, density_factors, stable_ids, seed);
      break;
  }

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(points.size());
  memcpy(pointcloud->co, points.data(), sizeof(float3) * points.size());
  for (const int i : points.index_range()) {
    *(float3 *)(pointcloud->co + i) = points[i];
    pointcloud->radius[i] = 0.05f;
  }

  PointCloudComponent &point_component =
      geometry_set_out.get_component_for_write<PointCloudComponent>();
  point_component.replace(pointcloud);

  Int32WriteAttribute stable_id_attribute = point_component.attribute_try_ensure_for_write(
      "id", ATTR_DOMAIN_POINT, CD_PROP_INT32);
  MutableSpan<int> stable_ids_span = stable_id_attribute.get_span();
  stable_ids_span.copy_from(stable_ids);
  stable_id_attribute.apply_span();

  params.set_output("Geometry", std::move(geometry_set_out));
}
}  // namespace blender::nodes

void register_node_type_geo_point_distribute()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_POINT_DISTRIBUTE, "Point Distribute", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_point_distribute_in, geo_node_point_distribute_out);
  node_type_update(&ntype, node_point_distribute_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_point_distribute_exec;
  nodeRegisterType(&ntype);
}
