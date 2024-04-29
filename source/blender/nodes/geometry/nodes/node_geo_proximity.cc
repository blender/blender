/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.h"
#include "BLI_task.hh"

#include "BKE_bvhutils.hh"
#include "BKE_geometry_set.hh"
#include "BKE_mesh.hh"

#include "DNA_pointcloud_types.h"

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_proximity_cc {

NODE_STORAGE_FUNCS(NodeGeometryProximity)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry", "Target")
      .only_realized_data()
      .supported_type({GeometryComponent::Type::Mesh, GeometryComponent::Type::PointCloud});
  b.add_input<decl::Int>("Group ID")
      .hide_value()
      .field_on_all()
      .description(
          "Splits the elements of the input geometry into groups which can be sampled "
          "individually");
  b.add_input<decl::Vector>("Sample Position", "Source Position")
      .implicit_field(implicit_field_inputs::position);
  b.add_input<decl::Int>("Sample Group ID").hide_value().supports_field();
  b.add_output<decl::Vector>("Position").dependent_field({2, 3}).reference_pass_all();
  b.add_output<decl::Float>("Distance").dependent_field({2, 3}).reference_pass_all();
  b.add_output<decl::Bool>("Is Valid")
      .dependent_field({2, 3})
      .description(
          "Whether the sampling was successful. It can fail when the sampled group is empty");
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

class ProximityFunction : public mf::MultiFunction {
 private:
  struct BVHTrees {
    BVHTreeFromMesh mesh_bvh = {};
    BVHTreeFromPointCloud pointcloud_bvh = {};
  };

  GeometrySet target_;
  GeometryNodeProximityTargetType type_;
  Vector<BVHTrees> bvh_trees_;
  VectorSet<int> group_indices_;

 public:
  ProximityFunction(GeometrySet target,
                    GeometryNodeProximityTargetType type,
                    const Field<int> &group_id_field)
      : target_(std::move(target)), type_(type)
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Geometry Proximity", signature};
      builder.single_input<float3>("Source Position");
      builder.single_input<int>("Sample ID");
      builder.single_output<float3>("Position", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float>("Distance", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<bool>("Is Valid", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);

    if (target_.has_pointcloud() && type_ == GEO_NODE_PROX_TARGET_POINTS) {
      const PointCloud &pointcloud = *target_.get_pointcloud();
      this->init_for_pointcloud(pointcloud, group_id_field);
    }
    if (target_.has_mesh()) {
      const Mesh &mesh = *target_.get_mesh();
      this->init_for_mesh(mesh, group_id_field);
    }
  }

  ~ProximityFunction()
  {
    for (BVHTrees &trees : bvh_trees_) {
      if (trees.mesh_bvh.tree) {
        free_bvhtree_from_mesh(&trees.mesh_bvh);
      }
      if (trees.pointcloud_bvh.tree) {
        free_bvhtree_from_pointcloud(&trees.pointcloud_bvh);
      }
    }
  }

  void init_for_pointcloud(const PointCloud &pointcloud, const Field<int> &group_id_field)
  {
    /* Compute group ids. */
    bke::PointCloudFieldContext field_context{pointcloud};
    FieldEvaluator field_evaluator{field_context, pointcloud.totpoint};
    field_evaluator.add(group_id_field);
    field_evaluator.evaluate();
    const VArray<int> group_ids = field_evaluator.get_evaluated<int>(0);

    IndexMaskMemory memory;
    Vector<IndexMask> group_masks = IndexMask::from_group_ids(group_ids, memory, group_indices_);
    const int groups_num = group_masks.size();

    /* Construct BVH tree for each group. */
    bvh_trees_.resize(groups_num);
    threading::parallel_for(
        IndexRange(groups_num),
        512,
        [&](const IndexRange range) {
          for (const int group_i : range) {
            const IndexMask &group_mask = group_masks[group_i];
            if (group_mask.is_empty()) {
              continue;
            }
            BVHTreeFromPointCloud &bvh = bvh_trees_[group_i].pointcloud_bvh;
            BKE_bvhtree_from_pointcloud_get(pointcloud, group_mask, bvh);
          }
        },
        threading::individual_task_sizes(
            [&](const int group_i) { return group_masks[group_i].size(); }, pointcloud.totpoint));
  }

  void init_for_mesh(const Mesh &mesh, const Field<int> &group_id_field)
  {
    /* Compute group ids. */
    const bke::AttrDomain domain = this->get_domain_on_mesh();
    const int domain_size = mesh.attributes().domain_size(domain);
    bke::MeshFieldContext field_context{mesh, domain};
    FieldEvaluator field_evaluator{field_context, domain_size};
    field_evaluator.add(group_id_field);
    field_evaluator.evaluate();
    const VArray<int> group_ids = field_evaluator.get_evaluated<int>(0);

    IndexMaskMemory memory;
    Vector<IndexMask> group_masks = IndexMask::from_group_ids(group_ids, memory, group_indices_);
    const int groups_num = group_masks.size();

    /* Construct BVH tree for each group. */
    bvh_trees_.resize(groups_num);
    threading::parallel_for(
        IndexRange(groups_num),
        512,
        [&](const IndexRange range) {
          for (const int group_i : range) {
            const IndexMask &group_mask = group_masks[group_i];
            if (group_mask.is_empty()) {
              continue;
            }
            BVHTreeFromMesh &bvh = bvh_trees_[group_i].mesh_bvh;
            switch (type_) {
              case GEO_NODE_PROX_TARGET_POINTS: {
                BKE_bvhtree_from_mesh_verts_init(mesh, group_mask, bvh);
                break;
              }
              case GEO_NODE_PROX_TARGET_EDGES: {
                BKE_bvhtree_from_mesh_edges_init(mesh, group_mask, bvh);
                break;
              }
              case GEO_NODE_PROX_TARGET_FACES: {
                BKE_bvhtree_from_mesh_tris_init(mesh, group_mask, bvh);
                break;
              }
            }
          }
        },
        threading::individual_task_sizes(
            [&](const int group_i) { return group_masks[group_i].size(); }, domain_size));
  }

  bke::AttrDomain get_domain_on_mesh() const
  {
    switch (type_) {
      case GEO_NODE_PROX_TARGET_POINTS:
        return bke::AttrDomain::Point;
      case GEO_NODE_PROX_TARGET_EDGES:
        return bke::AttrDomain::Edge;
      case GEO_NODE_PROX_TARGET_FACES:
        return bke::AttrDomain::Face;
    }
    BLI_assert_unreachable();
    return bke::AttrDomain::Point;
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &sample_positions = params.readonly_single_input<float3>(
        0, "Source Position");
    const VArray<int> &sample_ids = params.readonly_single_input<int>(1, "Sample ID");
    MutableSpan<float3> positions = params.uninitialized_single_output_if_required<float3>(
        2, "Position");
    MutableSpan<float> distances = params.uninitialized_single_output_if_required<float>(
        3, "Distance");
    MutableSpan<bool> is_valid_span = params.uninitialized_single_output_if_required<bool>(
        4, "Is Valid");

    mask.foreach_index([&](const int i) {
      const float3 sample_position = sample_positions[i];
      const int sample_id = sample_ids[i];
      const int group_index = group_indices_.index_of_try(sample_id);
      if (group_index == -1) {
        if (!positions.is_empty()) {
          positions[i] = float3(0, 0, 0);
        }
        if (!is_valid_span.is_empty()) {
          is_valid_span[i] = false;
        }
        if (!distances.is_empty()) {
          distances[i] = 0.0f;
        }
        return;
      }
      const BVHTrees &trees = bvh_trees_[group_index];
      BVHTreeNearest nearest;
      /* Take mesh and pointcloud bvh tree into account. The final result is the closer of the two.
       * First first bvhtree query will set `nearest.dist_sq` which is then passed into the second
       * query as a maximum distance. */
      nearest.dist_sq = FLT_MAX;
      if (trees.mesh_bvh.tree != nullptr) {
        BLI_bvhtree_find_nearest(trees.mesh_bvh.tree,
                                 sample_position,
                                 &nearest,
                                 trees.mesh_bvh.nearest_callback,
                                 const_cast<BVHTreeFromMesh *>(&trees.mesh_bvh));
      }
      if (trees.pointcloud_bvh.tree != nullptr) {
        BLI_bvhtree_find_nearest(trees.pointcloud_bvh.tree,
                                 sample_position,
                                 &nearest,
                                 trees.pointcloud_bvh.nearest_callback,
                                 const_cast<BVHTreeFromPointCloud *>(&trees.pointcloud_bvh));
      }

      if (!positions.is_empty()) {
        positions[i] = nearest.co;
      }
      if (!is_valid_span.is_empty()) {
        is_valid_span[i] = true;
      }
      if (!distances.is_empty()) {
        distances[i] = std::sqrt(nearest.dist_sq);
      }
    });
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
  Field<int> group_id_field = params.extract_input<Field<int>>("Group ID");
  Field<float3> position_field = params.extract_input<Field<float3>>("Source Position");
  Field<int> sample_id_field = params.extract_input<Field<int>>("Sample Group ID");

  auto proximity_fn = std::make_unique<ProximityFunction>(
      std::move(target), GeometryNodeProximityTargetType(storage.target_element), group_id_field);
  auto proximity_op = FieldOperation::Create(
      std::move(proximity_fn), {std::move(position_field), std::move(sample_id_field)});

  params.set_output("Position", Field<float3>(proximity_op, 0));
  params.set_output("Distance", Field<float>(proximity_op, 1));
  params.set_output("Is Valid", Field<bool>(proximity_op, 2));
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
