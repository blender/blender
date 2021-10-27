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

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/LevelSetUtil.h>
#  include <openvdb/tools/ParticlesToLevelSet.h>
#endif

#include "node_geometry_util.hh"

#include "BKE_lib_id.h"
#include "BKE_volume.h"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes {

static void geo_node_points_to_volume_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Points").supported_type(GEO_COMPONENT_TYPE_POINT_CLOUD);
  b.add_input<decl::Float>("Density").default_value(1.0f).min(0.0f);
  b.add_input<decl::Float>("Voxel Size").default_value(0.3f).min(0.01f).subtype(PROP_DISTANCE);
  b.add_input<decl::Float>("Voxel Amount").default_value(64.0f).min(0.0f);
  b.add_input<decl::Float>("Radius")
      .default_value(0.5f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .supports_field();
  b.add_output<decl::Geometry>("Volume");
}

static void geo_node_points_to_volume_layout(uiLayout *layout,
                                             bContext *UNUSED(C),
                                             PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "resolution_mode", 0, IFACE_("Resolution"), ICON_NONE);
}

static void geo_node_points_to_volume_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryPointsToVolume *data = (NodeGeometryPointsToVolume *)MEM_callocN(
      sizeof(NodeGeometryPointsToVolume), __func__);
  data->resolution_mode = GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT;
  node->storage = data;
}

static void geo_node_points_to_volume_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryPointsToVolume *data = (NodeGeometryPointsToVolume *)node->storage;
  bNodeSocket *voxel_size_socket = nodeFindSocket(node, SOCK_IN, "Voxel Size");
  bNodeSocket *voxel_amount_socket = nodeFindSocket(node, SOCK_IN, "Voxel Amount");
  nodeSetSocketAvailability(voxel_amount_socket,
                            data->resolution_mode ==
                                GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT);
  nodeSetSocketAvailability(
      voxel_size_socket, data->resolution_mode == GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE);
}

#ifdef WITH_OPENVDB
namespace {
/* Implements the interface required by #openvdb::tools::ParticlesToLevelSet. */
struct ParticleList {
  using PosType = openvdb::Vec3R;

  Span<float3> positions;
  Span<float> radii;

  size_t size() const
  {
    return (size_t)positions.size();
  }

  void getPos(size_t n, openvdb::Vec3R &xyz) const
  {
    xyz = &positions[n].x;
  }

  void getPosRad(size_t n, openvdb::Vec3R &xyz, openvdb::Real &radius) const
  {
    xyz = &positions[n].x;
    radius = radii[n];
  }
};
}  // namespace

static openvdb::FloatGrid::Ptr generate_volume_from_points(const Span<float3> positions,
                                                           const Span<float> radii,
                                                           const float density)
{
  /* Create a new grid that will be filled. #ParticlesToLevelSet requires the background value to
   * be positive. It will be set to zero later on. */
  openvdb::FloatGrid::Ptr new_grid = openvdb::FloatGrid::create(1.0f);

  /* Create a narrow-band level set grid based on the positions and radii. */
  openvdb::tools::ParticlesToLevelSet op{*new_grid};
  /* Don't ignore particles based on their radius. */
  op.setRmin(0.0f);
  op.setRmax(FLT_MAX);
  ParticleList particles{positions, radii};
  op.rasterizeSpheres(particles);
  op.finalize();

  /* Convert the level set to a fog volume. This also sets the background value to zero. Inside the
   * fog there will be a density of 1. */
  openvdb::tools::sdfToFogVolume(*new_grid);

  /* Take the desired density into account. */
  openvdb::tools::foreach (new_grid->beginValueOn(),
                           [&](const openvdb::FloatGrid::ValueOnIter &iter) {
                             iter.modifyValue([&](float &value) { value *= density; });
                           });
  return new_grid;
}

static float compute_voxel_size(const GeoNodeExecParams &params,
                                Span<float3> positions,
                                const float radius)
{
  const NodeGeometryPointsToVolume &storage =
      *(const NodeGeometryPointsToVolume *)params.node().storage;

  if (storage.resolution_mode == GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE) {
    return params.get_input<float>("Voxel Size");
  }

  if (positions.is_empty()) {
    return 0.0f;
  }

  float3 min, max;
  INIT_MINMAX(min, max);
  minmax_v3v3_v3_array(min, max, (float(*)[3])positions.data(), positions.size());

  const float voxel_amount = params.get_input<float>("Voxel Amount");
  if (voxel_amount <= 1) {
    return 0.0f;
  }

  /* The voxel size adapts to the final size of the volume. */
  const float diagonal = float3::distance(min, max);
  const float extended_diagonal = diagonal + 2.0f * radius;
  const float voxel_size = extended_diagonal / voxel_amount;
  return voxel_size;
}

static void gather_point_data_from_component(GeoNodeExecParams &params,
                                             const GeometryComponent &component,
                                             Vector<float3> &r_positions,
                                             Vector<float> &r_radii)
{
  GVArray_Typed<float3> positions = component.attribute_get_for_read<float3>(
      "position", ATTR_DOMAIN_POINT, {0, 0, 0});

  Field<float> radius_field = params.get_input<Field<float>>("Radius");
  GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_POINT};
  const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_POINT);

  r_positions.resize(r_positions.size() + domain_size);
  positions->materialize(r_positions.as_mutable_span().take_back(domain_size));

  r_radii.resize(r_radii.size() + domain_size);
  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.add_with_destination(radius_field, r_radii.as_mutable_span().take_back(domain_size));
  evaluator.evaluate();
}

static void convert_to_grid_index_space(const float voxel_size,
                                        MutableSpan<float3> positions,
                                        MutableSpan<float> radii)
{
  const float voxel_size_inv = 1.0f / voxel_size;
  for (const int i : positions.index_range()) {
    positions[i] *= voxel_size_inv;
    /* Better align generated grid with source points. */
    positions[i] -= float3(0.5f);
    radii[i] *= voxel_size_inv;
  }
}

static void initialize_volume_component_from_points(GeoNodeExecParams &params,
                                                    GeometrySet &r_geometry_set)
{
  Vector<float3> positions;
  Vector<float> radii;

  if (r_geometry_set.has<MeshComponent>()) {
    gather_point_data_from_component(
        params, *r_geometry_set.get_component_for_read<MeshComponent>(), positions, radii);
  }
  if (r_geometry_set.has<PointCloudComponent>()) {
    gather_point_data_from_component(
        params, *r_geometry_set.get_component_for_read<PointCloudComponent>(), positions, radii);
  }
  if (r_geometry_set.has<CurveComponent>()) {
    gather_point_data_from_component(
        params, *r_geometry_set.get_component_for_read<CurveComponent>(), positions, radii);
  }

  const float max_radius = *std::max_element(radii.begin(), radii.end());
  const float voxel_size = compute_voxel_size(params, positions, max_radius);
  if (voxel_size == 0.0f || positions.is_empty()) {
    return;
  }

  Volume *volume = (Volume *)BKE_id_new_nomain(ID_VO, nullptr);
  BKE_volume_init_grids(volume);

  VolumeGrid *c_density_grid = BKE_volume_grid_add(volume, "density", VOLUME_GRID_FLOAT);
  openvdb::FloatGrid::Ptr density_grid = openvdb::gridPtrCast<openvdb::FloatGrid>(
      BKE_volume_grid_openvdb_for_write(volume, c_density_grid, false));

  const float density = params.get_input<float>("Density");
  convert_to_grid_index_space(voxel_size, positions, radii);
  openvdb::FloatGrid::Ptr new_grid = generate_volume_from_points(positions, radii, density);
  /* This merge is cheap, because the #density_grid is empty. */
  density_grid->merge(*new_grid);
  density_grid->transform().postScale(voxel_size);
  r_geometry_set.keep_only({GEO_COMPONENT_TYPE_VOLUME, GEO_COMPONENT_TYPE_INSTANCES});
  r_geometry_set.replace_volume(volume);
}
#endif

static void geo_node_points_to_volume_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");

#ifdef WITH_OPENVDB
  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    initialize_volume_component_from_points(params, geometry_set);
  });
  params.set_output("Volume", std::move(geometry_set));
#else
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenVDB"));
  params.set_output("Volume", GeometrySet());
#endif
}

}  // namespace blender::nodes

void register_node_type_geo_points_to_volume()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_POINTS_TO_VOLUME, "Points to Volume", NODE_CLASS_GEOMETRY, 0);
  node_type_storage(&ntype,
                    "NodeGeometryPointsToVolume",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  node_type_size(&ntype, 170, 120, 700);
  node_type_init(&ntype, blender::nodes::geo_node_points_to_volume_init);
  node_type_update(&ntype, blender::nodes::geo_node_points_to_volume_update);
  ntype.declare = blender::nodes::geo_node_points_to_volume_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_points_to_volume_exec;
  ntype.draw_buttons = blender::nodes::geo_node_points_to_volume_layout;
  nodeRegisterType(&ntype);
}
