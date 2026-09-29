/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "BKE_bvh.hh"
#include "BKE_geometry_set.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.hh"

#include "DNA_pointcloud_types.h"

#include "NOD_rna_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_proximity_cc {

NODE_STORAGE_FUNCS(NodeGeometryProximity)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry"_ustr, "Target"_ustr)
      .only_realized_data()
      .supported_type({GeometryComponent::Type::Mesh, GeometryComponent::Type::PointCloud})
      .description("Geometry to find the closest point on");
  b.add_input<decl::Int>("Group ID"_ustr)
      .hide_value()
      .evaluated_geometry_field()
      .description(
          "Splits the elements of the input geometry into groups which can be sampled "
          "individually");
  auto &sample_position = b.add_input<decl::Vector>("Sample Position"_ustr, "Source Position"_ustr)
                              .default_input_type(NODE_DEFAULT_INPUT_POSITION_FIELD)
                              .structure_type(StructureType::Dynamic);
  auto &sample_group_id = b.add_input<decl::Int>("Sample Group ID"_ustr)
                              .hide_value()
                              .structure_type(StructureType::Dynamic);

  const std::array<int, 2> dynamic_inputs = {sample_position.index(), sample_group_id.index()};
  b.add_output<decl::Vector>("Position"_ustr)
      .inferred_structure_type(dynamic_inputs)
      .propagate_references(dynamic_inputs);
  b.add_output<decl::Float>("Distance"_ustr)
      .inferred_structure_type(dynamic_inputs)
      .propagate_references(dynamic_inputs);
  b.add_output<decl::Bool>("Is Valid"_ustr)
      .inferred_structure_type(dynamic_inputs)
      .propagate_references(dynamic_inputs)
      .description(
          "Whether the sampling was successful. It can fail when the sampled group is empty");
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "target_element", UI_ITEM_NONE, "", ICON_NONE);
}

static void geo_proximity_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryProximity *node_storage = MEM_new<NodeGeometryProximity>(__func__);
  node_storage->target_element = GEO_NODE_PROX_TARGET_FACES;
  node->storage = node_storage;
}

class ProximityFunction : public mf::MultiFunction {
 private:
  /** A tree for every group of elements with the same ID in one geometry. */
  struct GroupTrees {
    /** Used as a map from group ID to the index in the trees array. */
    VectorSet<int> ids;
    /** Trees for groups that don't contain every element. */
    Array<std::optional<bke::bvh::Tree>> owned_trees;
    /** The tree of every group, either owned or the geometry's cached tree. */
    Array<const bke::bvh::Tree *> trees;

    const bke::bvh::Tree *find(const int id) const
    {
      const int64_t group = this->ids.index_of_try(id);
      return group == -1 ? nullptr : this->trees[group];
    }
  };

  GeometrySet target_;
  Field<int> group_id_field_;
  GeometryNodeProximityTargetType type_;

  mutable CacheMutex mutex_;
  mutable GroupTrees mesh_trees_;
  mutable GroupTrees pointcloud_trees_;

 public:
  ProximityFunction(GeometrySet target,
                    GeometryNodeProximityTargetType type,
                    const Field<int> &group_id_field)
      : target_(std::move(target)), group_id_field_(std::move(group_id_field)), type_(type)
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
  }

  ~ProximityFunction() override = default;

  /**
   * \param cached_tree: The geometry's cached tree, used when a group contains every element.
   * \param build_tree: Build a tree for the elements in the mask.
   */
  static void build_group_trees(const VArray<int> &group_ids,
                                const FunctionRef<const bke::bvh::Tree &()> cached_tree,
                                const FunctionRef<bke::bvh::Tree(const IndexMask &)> build_tree,
                                GroupTrees &r_trees)
  {
    IndexMaskMemory memory;
    const Vector<IndexMask> group_masks = IndexMask::from_group_ids(group_ids, memory);
    const int groups_num = group_masks.size();

    r_trees.ids.reserve(groups_num);
    for (const IndexMask &group_mask : group_masks) {
      r_trees.ids.add_new(group_ids[group_mask.first()]);
    }
    r_trees.owned_trees.reinitialize(groups_num);
    r_trees.trees.reinitialize(groups_num);
    threading::parallel_for(
        IndexRange(groups_num),
        512,
        [&](const IndexRange range) {
          for (const int group_i : range) {
            const IndexMask &group_mask = group_masks[group_i];
            if (group_mask.size() == group_ids.size()) {
              r_trees.trees[group_i] = &cached_tree();
            }
            else {
              r_trees.owned_trees[group_i] = build_tree(group_mask);
              r_trees.trees[group_i] = &*r_trees.owned_trees[group_i];
            }
          }
        },
        threading::individual_task_sizes(
            [&](const int group_i) { return group_masks[group_i].size(); }, group_ids.size()));
  }

  void init_for_pointcloud(const PointCloud &pointcloud, const Field<int> &group_id_field) const
  {
    bke::PointCloudFieldContext field_context{pointcloud};
    FieldEvaluator field_evaluator{field_context, pointcloud.totpoint};
    field_evaluator.add(group_id_field);
    field_evaluator.evaluate();
    const VArray<int> group_ids = field_evaluator.get_evaluated<int>(0);

    build_group_trees(
        group_ids,
        [&]() -> const bke::bvh::Tree & { return pointcloud.bvh_tree(); },
        [&](const IndexMask &mask) {
          return bke::bvh::Tree::from_points(pointcloud.positions(), mask);
        },
        pointcloud_trees_);
  }

  void init_for_mesh(const Mesh &mesh, const Field<int> &group_id_field) const
  {
    const bke::AttrDomain domain = this->get_domain_on_mesh();
    const int domain_size = mesh.attributes().domain_size(domain);
    bke::MeshFieldContext field_context{mesh, domain};
    FieldEvaluator field_evaluator{field_context, domain_size};
    field_evaluator.add(group_id_field);
    field_evaluator.evaluate();
    const VArray<int> group_ids = field_evaluator.get_evaluated<int>(0);

    switch (type_) {
      case GEO_NODE_PROX_TARGET_POINTS:
        build_group_trees(
            group_ids,
            [&]() -> const bke::bvh::Tree & { return mesh.bvh_verts(); },
            [&](const IndexMask &mask) {
              return bke::bvh::Tree::from_points(mesh.vert_positions(), mask);
            },
            mesh_trees_);
        break;
      case GEO_NODE_PROX_TARGET_EDGES:
        build_group_trees(
            group_ids,
            [&]() -> const bke::bvh::Tree & { return mesh.bvh_edges(); },
            [&](const IndexMask &mask) {
              return bke::bvh::Tree::from_edges(mesh.vert_positions(), mesh.edges(), mask);
            },
            mesh_trees_);
        break;
      case GEO_NODE_PROX_TARGET_FACES:
        build_group_trees(
            group_ids,
            [&]() -> const bke::bvh::Tree & { return mesh.bvh_tris(); },
            [&](const IndexMask &mask) { return bke::bvh::Tree::from_tris(mesh, mask, false); },
            mesh_trees_);
        break;
    }
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
      const bke::bvh::Tree *mesh_bvh = mesh_trees_.find(sample_id);
      const bke::bvh::Tree *pointcloud_bvh = pointcloud_trees_.find(sample_id);
      if (!mesh_bvh && !pointcloud_bvh) {
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
      /* Take mesh and pointcloud bvh tree into account. The final result is the closer of the two.
       * The distance from the first query is passed into the second query as a maximum distance,
       * so that the mesh result is kept when both are the same distance away. */
      float3 nearest_position(0.0f);
      float nearest_distance = FLT_MAX;
      if (mesh_bvh) {
        if (const std::optional<bke::bvh::ClosestPointResult> result = mesh_bvh->closest_point(
                sample_position))
        {
          nearest_position = result->position;
          nearest_distance = math::distance(sample_position, result->position);
        }
      }
      if (pointcloud_bvh) {
        if (const std::optional<bke::bvh::ClosestPointResult> result =
                pointcloud_bvh->closest_point(sample_position, nearest_distance))
        {
          nearest_position = result->position;
          nearest_distance = math::distance(sample_position, result->position);
        }
      }

      if (!positions.is_empty()) {
        positions[i] = nearest_position;
      }
      if (!is_valid_span.is_empty()) {
        is_valid_span[i] = true;
      }
      if (!distances.is_empty()) {
        distances[i] = nearest_distance;
      }
    });
  }

  ExecutionHints get_execution_hints() const override
  {
    ExecutionHints hints;
    hints.min_grain_size = 512;
    return hints;
  }

  void hash_unique(UniqueHashBytes &hash) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
    hash.add(target_.get_mesh());
    hash.add(target_.get_pointcloud());
    hash.add(type_);
    fn::FieldHashDeep field_hash;
    hash.add(field_hash.ensure(group_id_field_));
  }

  void prepare_for_execution() const override
  {
    mutex_.ensure([&]() {
      if (target_.has_pointcloud() && type_ == GEO_NODE_PROX_TARGET_POINTS) {
        const PointCloud &pointcloud = *target_.get_pointcloud();
        this->init_for_pointcloud(pointcloud, group_id_field_);
      }
      if (target_.has_mesh()) {
        const Mesh &mesh = *target_.get_mesh();
        this->init_for_mesh(mesh, group_id_field_);
      }
    });
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet target = params.extract_input<GeometrySet>("Target"_ustr);
  target.ensure_owns_direct_data();

  if (!target.has_mesh() && !target.has_pointcloud()) {
    params.set_default_remaining_outputs();
    return;
  }

  const NodeGeometryProximity &storage = node_storage(params.node());
  const auto target_type = GeometryNodeProximityTargetType(storage.target_element);

  Field<int> group_id_field = params.extract_input<Field<int>>("Group ID"_ustr);
  auto sample_position = params.extract_input<bke::SocketValueVariant>("Source Position"_ustr);
  auto sample_group_id = params.extract_input<bke::SocketValueVariant>("Sample Group ID"_ustr);

  std::string error_message;
  bke::SocketValueVariant position;
  bke::SocketValueVariant distance;
  bke::SocketValueVariant is_valid;
  if (!execute_multi_function_on_value_variant(
          std::make_shared<ProximityFunction>(
              std::move(target), target_type, std::move(group_id_field)),
          {&sample_position, &sample_group_id},
          {&position, &distance, &is_valid},
          params.user_data(),
          error_message))
  {
    params.set_default_remaining_outputs();
    params.error_message_add(NodeWarningType::Error, std::move(error_message));
    return;
  }

  params.set_output("Position"_ustr, std::move(position));
  params.set_output("Distance"_ustr, std::move(distance));
  params.set_output("Is Valid"_ustr, std::move(is_valid));
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
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeProximity"_ustr, GEO_NODE_PROXIMITY);
  ntype.ui_name = "Geometry Proximity";
  ntype.ui_description = "Compute the closest location on the target geometry";
  ntype.enum_name_legacy = "PROXIMITY";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.initfunc = geo_proximity_init;
  bke::node_type_storage(
      ntype, "NodeGeometryProximity", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_proximity_cc
