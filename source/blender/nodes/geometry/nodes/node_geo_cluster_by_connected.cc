/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_disjoint_set.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "BKE_geometry_fields.hh"
#include "BKE_mesh.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_cluster_by_connected_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>("Selection"_ustr)
      .default_value(true)
      .hide_value()
      .structure_type(StructureType::Field);
  b.add_input<decl::Vector>("Position"_ustr)
      .default_input_type(NODE_DEFAULT_INPUT_POSITION_FIELD)
      .structure_type(StructureType::Field);
  b.add_input<decl::Float>("Distance"_ustr).default_value(0.001f).min(0.0f).subtype(PROP_DISTANCE);

  b.add_output<decl::Int>("Cluster ID"_ustr)
      .structure_type(StructureType::Field)
      .propagate_references();
}

class ClusterByConnectedFieldInput final : public bke::GeometryFieldInput {
 private:
  Field<bool> selection_field_;
  Field<float3> position_field_;
  float min_distance_;

 public:
  ClusterByConnectedFieldInput(Field<bool> selection_field,
                               Field<float3> position_field,
                               float min_distance)
      : bke::GeometryFieldInput(CPPType::get<int>(), "Cluster by Connected"),
        selection_field_(std::move(selection_field)),
        position_field_(std::move(position_field)),
        min_distance_(min_distance)
  {
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const override
  {
    if (context.type() != bke::GeometryComponent::Type::Mesh) {
      return fn::IndexFieldInput::get_index_varray(mask);
    }
    const Mesh &mesh = *context.mesh();
    const Span<int2> edges = mesh.edges();

    const bke::MeshFieldContext edge_context(mesh, AttrDomain::Edge);
    fn::FieldEvaluator edge_evaluator(edge_context, mesh.edges_num);
    edge_evaluator.set_selection(selection_field_);
    edge_evaluator.evaluate();
    const IndexMask selection = edge_evaluator.get_evaluated_selection_as_mask();
    if (selection.is_empty()) {
      return fn::IndexFieldInput::get_index_varray(mask);
    }

    const bke::MeshFieldContext vert_context(mesh, AttrDomain::Point);
    fn::FieldEvaluator point_evaluator(vert_context, mesh.verts_num);
    point_evaluator.add(position_field_);
    point_evaluator.evaluate();
    const VArraySpan<float3> position = point_evaluator.get_evaluated<float3>(0);

    DisjointSet<int> vertex_cluster(mesh.verts_num);

    Array<float3> vert_cluster_center(mesh.verts_num);
    array_utils::copy(position, vert_cluster_center.as_mutable_span());
    Array<int> vert_cluster_size(mesh.verts_num, 1);

    selection.foreach_index([&](const int edge_i) {
      const int2 edge = edges[edge_i];
      const int2 edge_clusters(vertex_cluster.find_root(edge[0]),
                               vertex_cluster.find_root(edge[1]));
      if (edge_clusters[0] == edge_clusters[1]) {
        return;
      }

      const float3 vert_a_cluster = vert_cluster_center[edge_clusters[0]];
      const float3 vert_b_cluster = vert_cluster_center[edge_clusters[1]];
      const float distance = math::distance(vert_a_cluster, vert_b_cluster);
      if (distance >= min_distance_) {
        return;
      }

      /* Use original vertices instead of already found roots to keep path folding optimization. */
      const int new_cluster_root = vertex_cluster.join(edge[0], edge[1]);

      const int new_cluster_size = vert_cluster_size[edge_clusters[0]] +
                                   vert_cluster_size[edge_clusters[1]];
      const float3 new_cluster_center = math::interpolate(vert_a_cluster,
                                                          vert_b_cluster,
                                                          vert_cluster_size[edge_clusters[1]] /
                                                              float(new_cluster_size));

      vert_cluster_center[new_cluster_root] = new_cluster_center;
      vert_cluster_size[new_cluster_root] = new_cluster_size;
    });

    Array<int> cluster_indices(mesh.verts_num);
    threading::parallel_for(IndexRange(mesh.verts_num), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        cluster_indices[i] = std::as_const(vertex_cluster).find_root(i);
      }
    });

    return mesh.attributes().adapt_domain<int>(
        VArray<int>::from_container(std::move(cluster_indices)),
        AttrDomain::Point,
        context.domain());
  }

  void foreach_recursive_field(FunctionRef<void(const GField &)> fn) const override
  {
    fn(selection_field_);
    fn(position_field_);
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
    hash.add(deep_hash_cache.ensure(selection_field_));
    hash.add(deep_hash_cache.ensure(position_field_));
    hash.add(min_distance_);
  }

  std::optional<AttrDomain> preferred_domain(
      const GeometryComponent & /*component*/) const override
  {
    return AttrDomain::Point;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Cluster ID"_ustr,
                    Field<int>::from_input<ClusterByConnectedFieldInput>(
                        params.extract_input<Field<bool>>("Selection"_ustr),
                        params.extract_input<Field<float3>>("Position"_ustr),
                        params.extract_input<float>("Distance"_ustr)));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeClusterByConnected"_ustr);
  ntype.ui_name = "Cluster by Connected";
  ntype.ui_description =
      "Group mesh vertices connected by edges when they are within a specified distance";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.default_width = bke::NodeWidth::_160;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_cluster_by_connected_cc
