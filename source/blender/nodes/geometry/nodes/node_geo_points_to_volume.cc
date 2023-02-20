/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/LevelSetUtil.h>
#  include <openvdb/tools/ParticlesToLevelSet.h>
#endif

#include "BLI_bounds.hh"

#include "node_geometry_util.hh"

#include "BKE_lib_id.h"
#include "BKE_volume.h"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_geo_points_to_volume_cc {

NODE_STORAGE_FUNCS(NodeGeometryPointsToVolume)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Points"));
  b.add_input<decl::Float>(N_("Density")).default_value(1.0f).min(0.0f);
  b.add_input<decl::Float>(N_("Voxel Size"))
      .default_value(0.3f)
      .min(0.01f)
      .subtype(PROP_DISTANCE)
      .make_available([](bNode &node) {
        node_storage(node).resolution_mode = GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE;
      });
  b.add_input<decl::Float>(N_("Voxel Amount"))
      .default_value(64.0f)
      .min(0.0f)
      .make_available([](bNode &node) {
        node_storage(node).resolution_mode = GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT;
      });
  b.add_input<decl::Float>(N_("Radius"))
      .default_value(0.5f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .field_on_all();
  b.add_output<decl::Geometry>(N_("Volume"));
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "resolution_mode", 0, IFACE_("Resolution"), ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryPointsToVolume *data = MEM_cnew<NodeGeometryPointsToVolume>(__func__);
  data->resolution_mode = GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryPointsToVolume &storage = node_storage(*node);
  bNodeSocket *voxel_size_socket = nodeFindSocket(node, SOCK_IN, "Voxel Size");
  bNodeSocket *voxel_amount_socket = nodeFindSocket(node, SOCK_IN, "Voxel Amount");
  nodeSetSocketAvailability(ntree,
                            voxel_amount_socket,
                            storage.resolution_mode ==
                                GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT);
  nodeSetSocketAvailability(ntree,
                            voxel_size_socket,
                            storage.resolution_mode ==
                                GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE);
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
    return size_t(positions.size());
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
  const NodeGeometryPointsToVolume &storage = node_storage(params.node());

  if (storage.resolution_mode == GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE) {
    return params.get_input<float>("Voxel Size");
  }

  if (positions.is_empty()) {
    return 0.0f;
  }

  const Bounds<float3> bounds = *bounds::min_max(positions);

  const float voxel_amount = params.get_input<float>("Voxel Amount");
  if (voxel_amount <= 1) {
    return 0.0f;
  }

  /* The voxel size adapts to the final size of the volume. */
  const float diagonal = math::distance(bounds.min, bounds.max);
  const float extended_diagonal = diagonal + 2.0f * radius;
  const float voxel_size = extended_diagonal / voxel_amount;
  return voxel_size;
}

static void gather_point_data_from_component(GeoNodeExecParams &params,
                                             const GeometryComponent &component,
                                             Vector<float3> &r_positions,
                                             Vector<float> &r_radii)
{
  if (component.is_empty()) {
    return;
  }
  VArray<float3> positions = component.attributes()->lookup_or_default<float3>(
      "position", ATTR_DOMAIN_POINT, {0, 0, 0});

  Field<float> radius_field = params.get_input<Field<float>>("Radius");
  bke::GeometryFieldContext field_context{component, ATTR_DOMAIN_POINT};
  const int domain_num = component.attribute_domain_size(ATTR_DOMAIN_POINT);

  r_positions.resize(r_positions.size() + domain_num);
  positions.materialize(r_positions.as_mutable_span().take_back(domain_num));

  r_radii.resize(r_radii.size() + domain_num);
  fn::FieldEvaluator evaluator{field_context, domain_num};
  evaluator.add_with_destination(radius_field, r_radii.as_mutable_span().take_back(domain_num));
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

  for (const GeometryComponentType type :
       {GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_POINT_CLOUD, GEO_COMPONENT_TYPE_CURVE}) {
    if (r_geometry_set.has(type)) {
      gather_point_data_from_component(
          params, *r_geometry_set.get_component_for_read(type), positions, radii);
    }
  }

  if (positions.is_empty()) {
    return;
  }

  const float max_radius = *std::max_element(radii.begin(), radii.end());
  const float voxel_size = compute_voxel_size(params, positions, max_radius);
  const double determinant = std::pow(double(voxel_size), 3.0);
  if (!BKE_volume_grid_determinant_valid(determinant)) {
    return;
  }

  Volume *volume = reinterpret_cast<Volume *>(BKE_id_new_nomain(ID_VO, nullptr));

  const float density = params.get_input<float>("Density");
  convert_to_grid_index_space(voxel_size, positions, radii);
  openvdb::FloatGrid::Ptr new_grid = generate_volume_from_points(positions, radii, density);
  new_grid->transform().postScale(voxel_size);
  BKE_volume_grid_add_vdb(*volume, "density", std::move(new_grid));

  r_geometry_set.keep_only_during_modify({GEO_COMPONENT_TYPE_VOLUME});
  r_geometry_set.replace_volume(volume);
}
#endif

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    initialize_volume_component_from_points(params, geometry_set);
  });
  params.set_output("Volume", std::move(geometry_set));
#else
  params.set_default_remaining_outputs();
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenVDB"));
#endif
}

}  // namespace blender::nodes::node_geo_points_to_volume_cc

void register_node_type_geo_points_to_volume()
{
  namespace file_ns = blender::nodes::node_geo_points_to_volume_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_POINTS_TO_VOLUME, "Points to Volume", NODE_CLASS_GEOMETRY);
  node_type_storage(&ntype,
                    "NodeGeometryPointsToVolume",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  node_type_size(&ntype, 170, 120, 700);
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
