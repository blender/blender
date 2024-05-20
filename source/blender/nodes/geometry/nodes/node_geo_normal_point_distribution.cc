/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_pointcloud.hh"

#include "node_geometry_util.hh"

#include <random>

namespace blender::nodes::node_geo_normal_point_distribution_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Count").default_value(100).min(0).field_on_all();
  b.add_input<decl::Int>("Seed");
  b.add_input<decl::Vector>("Mean")
      .default_value({0.0f, 0.0f, 0.0f})
      .compositor_expects_single_value();
  b.add_input<decl::Vector>("Stdev")
      .default_value({1.0f, 1.0f, 1.0f})
      .compositor_expects_single_value();

  b.add_output<decl::Geometry>("Points").propagate_all();
}


/**
 * Random Sample a Multi-variate Normal Distribution with three dimensions.
 * Use the standard library for the underlying normal distribution and random engine implementation.
 */
static void sample_normally_distributed_points(
    float3 mean, float3 stdev, const int count, const int seed, Vector<float3> &out_positions)
{
  std::normal_distribution<float> dist_x{mean.x, stdev.x};
  std::normal_distribution<float> dist_y{mean.y, stdev.y};
  std::normal_distribution<float> dist_z{mean.z, stdev.z};
  std::mt19937 engine(seed);
  for (int i = 0; i < count; i++) {
    float3 point_pos;
    point_pos.x = dist_x(engine);
    point_pos.y = dist_y(engine);
    point_pos.z = dist_z(engine);
    out_positions.append(point_pos);
  }
}

static PointCloud *point_cloud_with_normal_distribution_calculate(float3 mean,
                                                                  float3 stdev,
                                                                  const int seed,
                                                                  const int count)
{
  Vector<float3> positions;
  sample_normally_distributed_points(mean, stdev, count, seed, positions);
  PointCloud *pointcloud = BKE_pointcloud_new_nomain(positions.size());
  pointcloud->positions_for_write().copy_from(positions);
  return pointcloud;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const int count = params.get_input<int>("Count");
  if (count <= 0) {
    params.set_output("Points", GeometrySet());
    return;
  }

  const int seed = params.get_input<int>("Seed") * 5383843;
  const float3 mean = params.get_input<float3>("Mean");
  const float3 stdev = params.get_input<float3>("Stdev");

  auto point_cloud = point_cloud_with_normal_distribution_calculate(mean, stdev, seed, count);

  params.set_output("Points", GeometrySet::from_pointcloud(point_cloud));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype,
                     GEO_NODE_NORMAL_POINT_DISTRIBUTION,
                     "Normal Point Distribution",
                     NODE_CLASS_GEOMETRY);
  blender::bke::node_type_size(&ntype, 170, 100, 320);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_normal_point_distribution_cc
