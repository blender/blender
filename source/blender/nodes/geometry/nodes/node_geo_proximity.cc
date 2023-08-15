/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.h"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

#include "DNA_mesh_types.h"

#include "BKE_bvhutils.h"
#include "BKE_geometry_set.hh"

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_proximity_cc {

NODE_STORAGE_FUNCS(NodeGeometryProximity)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Target").only_realized_data().supported_type(
      {GeometryComponent::Type::Mesh, GeometryComponent::Type::PointCloud});
  b.add_input<decl::Vector>("Source Position").implicit_field(implicit_field_inputs::position);
  b.add_output<decl::Vector>("Position").dependent_field().reference_pass_all();
  b.add_output<decl::Float>("Distance").dependent_field().reference_pass_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "target_element", UI_ITEM_NONE, "", ICON_NONE);
}

static void geo_proximity_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryProximity *node_storage = MEM_cnew<NodeGeometryProximity>(__func__);
  node_storage->target_element = GEO_NODE_PROX_TARGET_FACES;
  node->storage = node_storage;
}

static bool calculate_mesh_proximity(const VArray<float3> &positions,
                                     const IndexMask &mask,
                                     const Mesh &mesh,
                                     const GeometryNodeProximityTargetType type,
                                     const MutableSpan<float> r_distances,
                                     const MutableSpan<float3> r_locations)
{
  BVHTreeFromMesh bvh_data;
  switch (type) {
    case GEO_NODE_PROX_TARGET_POINTS:
      BKE_bvhtree_from_mesh_get(&bvh_data, &mesh, BVHTREE_FROM_VERTS, 2);
      break;
    case GEO_NODE_PROX_TARGET_EDGES:
      BKE_bvhtree_from_mesh_get(&bvh_data, &mesh, BVHTREE_FROM_EDGES, 2);
      break;
    case GEO_NODE_PROX_TARGET_FACES:
      BKE_bvhtree_from_mesh_get(&bvh_data, &mesh, BVHTREE_FROM_LOOPTRI, 2);
      break;
  }

  if (bvh_data.tree == nullptr) {
    return false;
  }

  mask.foreach_index(GrainSize(512), [&](const int index) {
    BVHTreeNearest nearest;
    copy_v3_fl(nearest.co, FLT_MAX);
    nearest.index = -1;

    /* Use the distance to the last found point as upper bound to speedup the bvh lookup. */
    nearest.dist_sq = math::distance_squared(float3(nearest.co), positions[index]);

    BLI_bvhtree_find_nearest(
        bvh_data.tree, positions[index], &nearest, bvh_data.nearest_callback, &bvh_data);

    if (nearest.dist_sq < r_distances[index]) {
      r_distances[index] = nearest.dist_sq;
      if (!r_locations.is_empty()) {
        r_locations[index] = nearest.co;
      }
    }
  });

  free_bvhtree_from_mesh(&bvh_data);
  return true;
}

static bool calculate_pointcloud_proximity(const VArray<float3> &positions,
                                           const IndexMask &mask,
                                           const PointCloud &pointcloud,
                                           MutableSpan<float> r_distances,
                                           MutableSpan<float3> r_locations)
{
  BVHTreeFromPointCloud bvh_data;
  const BVHTree *tree = BKE_bvhtree_from_pointcloud_get(&bvh_data, &pointcloud, 2);
  if (tree == nullptr) {
    return false;
  }

  mask.foreach_index(GrainSize(512), [&](const int index) {
    BVHTreeNearest nearest;
    copy_v3_fl(nearest.co, FLT_MAX);
    nearest.index = -1;

    /* Use the distance to the closest point in the mesh to speedup the pointcloud bvh lookup.
     * This is ok because we only need to find the closest point in the pointcloud if it's
     * closer than the mesh. */
    nearest.dist_sq = r_distances[index];

    BLI_bvhtree_find_nearest(
        bvh_data.tree, positions[index], &nearest, bvh_data.nearest_callback, &bvh_data);

    if (nearest.dist_sq < r_distances[index]) {
      r_distances[index] = nearest.dist_sq;
      if (!r_locations.is_empty()) {
        r_locations[index] = nearest.co;
      }
    }
  });

  free_bvhtree_from_pointcloud(&bvh_data);
  return true;
}

class ProximityFunction : public mf::MultiFunction {
 private:
  GeometrySet target_;
  GeometryNodeProximityTargetType type_;

 public:
  ProximityFunction(GeometrySet target, GeometryNodeProximityTargetType type)
      : target_(std::move(target)), type_(type)
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Geometry Proximity", signature};
      builder.single_input<float3>("Source Position");
      builder.single_output<float3>("Position", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float>("Distance");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &src_positions = params.readonly_single_input<float3>(0,
                                                                               "Source Position");
    MutableSpan<float3> positions = params.uninitialized_single_output_if_required<float3>(
        1, "Position");
    /* Make sure there is a distance array, used for finding the smaller distance when there are
     * multiple components. Theoretically it would be possible to avoid using the distance array
     * when there is only one component. However, this only adds an allocation and a single float
     * comparison per vertex, so it's likely not worth it. */
    MutableSpan<float> distances = params.uninitialized_single_output<float>(2, "Distance");

    index_mask::masked_fill(distances, FLT_MAX, mask);

    bool success = false;
    if (target_.has_mesh()) {
      success |= calculate_mesh_proximity(
          src_positions, mask, *target_.get_mesh(), type_, distances, positions);
    }

    if (target_.has_pointcloud() && type_ == GEO_NODE_PROX_TARGET_POINTS) {
      success |= calculate_pointcloud_proximity(
          src_positions, mask, *target_.get_pointcloud(), distances, positions);
    }

    if (!success) {
      if (!positions.is_empty()) {
        index_mask::masked_fill(positions, float3(0), mask);
      }
      if (!distances.is_empty()) {
        index_mask::masked_fill(distances, 0.0f, mask);
      }
      return;
    }

    if (params.single_output_is_required(2, "Distance")) {
      mask.foreach_index_optimized<int>(
          GrainSize(2048), [&](const int j) { distances[j] = std::sqrt(distances[j]); });
    }
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet target = params.extract_input<GeometrySet>("Target");
  target.ensure_owns_direct_data();

  if (!target.has_mesh() && !target.has_pointcloud()) {
    params.set_default_remaining_outputs();
    return;
  }

  const NodeGeometryProximity &storage = node_storage(params.node());
  Field<float3> position_field = params.extract_input<Field<float3>>("Source Position");

  auto proximity_fn = std::make_unique<ProximityFunction>(
      std::move(target), GeometryNodeProximityTargetType(storage.target_element));
  auto proximity_op = FieldOperation::Create(std::move(proximity_fn), {std::move(position_field)});

  params.set_output("Position", Field<float3>(proximity_op, 0));
  params.set_output("Distance", Field<float>(proximity_op, 1));
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem target_element_items[] = {
      {GEO_NODE_PROX_TARGET_POINTS,
       "POINTS",
       ICON_NONE,
       "Points",
       "Calculate the proximity to the target's points (faster than the other modes)"},
      {GEO_NODE_PROX_TARGET_EDGES,
       "EDGES",
       ICON_NONE,
       "Edges",
       "Calculate the proximity to the target's edges"},
      {GEO_NODE_PROX_TARGET_FACES,
       "FACES",
       ICON_NONE,
       "Faces",
       "Calculate the proximity to the target's faces"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "target_element",
                    "Target Geometry",
                    "Element of the target geometry to calculate the distance from",
                    target_element_items,
                    NOD_storage_enum_accessors(target_element),
                    GEO_NODE_PROX_TARGET_FACES);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_PROXIMITY, "Geometry Proximity", NODE_CLASS_GEOMETRY);
  ntype.initfunc = geo_proximity_init;
  node_type_storage(
      &ntype, "NodeGeometryProximity", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_proximity_cc
