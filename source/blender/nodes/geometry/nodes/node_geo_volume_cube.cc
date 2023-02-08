/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/Dense.h>
#  include <openvdb/tools/LevelSetUtil.h>
#  include <openvdb/tools/ParticlesToLevelSet.h>
#endif

#include "node_geometry_util.hh"

#include "DNA_mesh_types.h"

#include "BLI_task.hh"

#include "BKE_geometry_set.hh"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_volume.h"

namespace blender::nodes::node_geo_volume_cube_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Density"))
      .description(N_("Volume density per voxel"))
      .supports_field()
      .default_value(1.0f);
  b.add_input<decl::Float>(N_("Background"))
      .description(N_("Value for voxels outside of the cube"));

  b.add_input<decl::Vector>(N_("Min"))
      .description(N_("Minimum boundary of volume"))
      .default_value(float3(-1.0f));
  b.add_input<decl::Vector>(N_("Max"))
      .description(N_("Maximum boundary of volume"))
      .default_value(float3(1.0f));

  b.add_input<decl::Int>(N_("Resolution X"))
      .description(N_("Number of voxels in the X axis"))
      .default_value(32)
      .min(2);
  b.add_input<decl::Int>(N_("Resolution Y"))
      .description(N_("Number of voxels in the Y axis"))
      .default_value(32)
      .min(2);
  b.add_input<decl::Int>(N_("Resolution Z"))
      .description(N_("Number of voxels in the Z axis"))
      .default_value(32)
      .min(2);

  b.add_output<decl::Geometry>(N_("Volume"));
}

static float map(const float x,
                 const float in_min,
                 const float in_max,
                 const float out_min,
                 const float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

class Grid3DFieldContext : public FieldContext {
 private:
  int3 resolution_;
  float3 bounds_min_;
  float3 bounds_max_;

 public:
  Grid3DFieldContext(const int3 resolution, const float3 bounds_min, const float3 bounds_max)
      : resolution_(resolution), bounds_min_(bounds_min), bounds_max_(bounds_max)
  {
  }

  int64_t points_num() const
  {
    return int64_t(resolution_.x) * int64_t(resolution_.y) * int64_t(resolution_.z);
  }

  GVArray get_varray_for_input(const FieldInput &field_input,
                               const IndexMask /*mask*/,
                               ResourceScope & /*scope*/) const
  {
    const bke::AttributeFieldInput *attribute_field_input =
        dynamic_cast<const bke::AttributeFieldInput *>(&field_input);
    if (attribute_field_input == nullptr) {
      return {};
    }
    if (attribute_field_input->attribute_name() != "position") {
      return {};
    }

    Array<float3> positions(this->points_num());

    threading::parallel_for(IndexRange(resolution_.x), 1, [&](const IndexRange x_range) {
      /* Start indexing at current X slice. */
      int64_t index = x_range.start() * resolution_.y * resolution_.z;
      for (const int64_t x_i : x_range) {
        const float x = map(x_i, 0.0f, resolution_.x - 1, bounds_min_.x, bounds_max_.x);
        for (const int64_t y_i : IndexRange(resolution_.y)) {
          const float y = map(y_i, 0.0f, resolution_.y - 1, bounds_min_.y, bounds_max_.y);
          for (const int64_t z_i : IndexRange(resolution_.z)) {
            const float z = map(z_i, 0.0f, resolution_.z - 1, bounds_min_.z, bounds_max_.z);
            positions[index] = float3(x, y, z);
            index++;
          }
        }
      }
    });
    return VArray<float3>::ForContainer(std::move(positions));
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const float3 bounds_min = params.extract_input<float3>("Min");
  const float3 bounds_max = params.extract_input<float3>("Max");

  const int3 resolution = int3(params.extract_input<int>("Resolution X"),
                               params.extract_input<int>("Resolution Y"),
                               params.extract_input<int>("Resolution Z"));

  if (resolution.x < 2 || resolution.y < 2 || resolution.z < 2) {
    params.error_message_add(NodeWarningType::Error, TIP_("Resolution must be greater than 1"));
    params.set_default_remaining_outputs();
    return;
  }

  if (bounds_min.x == bounds_max.x || bounds_min.y == bounds_max.y ||
      bounds_min.z == bounds_max.z) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("Bounding box volume must be greater than 0"));
    params.set_default_remaining_outputs();
    return;
  }

  const double3 scale_fac = double3(bounds_max - bounds_min) / double3(resolution - 1);
  if (!BKE_volume_grid_determinant_valid(scale_fac.x * scale_fac.y * scale_fac.z)) {
    params.error_message_add(NodeWarningType::Warning,
                             TIP_("Volume scale is lower than permitted by OpenVDB"));
    params.set_default_remaining_outputs();
    return;
  }

  Field<float> input_field = params.extract_input<Field<float>>("Density");

  /* Evaluate input field on a 3D grid. */
  Grid3DFieldContext context(resolution, bounds_min, bounds_max);
  FieldEvaluator evaluator(context, context.points_num());
  Array<float> densities(context.points_num());
  evaluator.add_with_destination(std::move(input_field), densities.as_mutable_span());
  evaluator.evaluate();

  /* Store resulting values in openvdb grid. */
  const float background = params.extract_input<float>("Background");
  openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create(background);
  grid->setGridClass(openvdb::GRID_FOG_VOLUME);

  openvdb::tools::Dense<float, openvdb::tools::LayoutZYX> dense_grid{
      openvdb::math::CoordBBox({0, 0, 0}, {resolution.x - 1, resolution.y - 1, resolution.z - 1}),
      densities.data()};
  openvdb::tools::copyFromDense(dense_grid, *grid, 0.0f);

  grid->transform().preTranslate(openvdb::math::Vec3<float>(-0.5f));
  grid->transform().postScale(openvdb::math::Vec3<double>(scale_fac.x, scale_fac.y, scale_fac.z));
  grid->transform().postTranslate(
      openvdb::math::Vec3<float>(bounds_min.x, bounds_min.y, bounds_min.z));

  Volume *volume = reinterpret_cast<Volume *>(BKE_id_new_nomain(ID_VO, nullptr));
  BKE_volume_grid_add_vdb(*volume, "density", std::move(grid));

  GeometrySet r_geometry_set;
  r_geometry_set.replace_volume(volume);
  params.set_output("Volume", r_geometry_set);
#else
  params.set_default_remaining_outputs();
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenVDB"));
#endif
}

}  // namespace blender::nodes::node_geo_volume_cube_cc

void register_node_type_geo_volume_cube()
{
  namespace file_ns = blender::nodes::node_geo_volume_cube_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_VOLUME_CUBE, "Volume Cube", NODE_CLASS_GEOMETRY);

  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
