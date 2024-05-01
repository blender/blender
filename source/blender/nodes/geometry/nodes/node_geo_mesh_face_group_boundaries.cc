/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <atomic>

#include "BKE_mesh.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_face_group_boundaries_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Face Group ID", "Face Set")
      .default_value(0)
      .hide_value()
      .supports_field()
      .description(
          "An identifier for the group of each face. All contiguous faces with the "
          "same value are in the same region");
  b.add_output<decl::Bool>("Boundary Edges")
      .field_source_reference_all()
      .description("The edges that lie on the boundaries between the different face groups");
}

class BoundaryFieldInput final : public bke::MeshFieldInput {
 private:
  const Field<int> face_set_;

 public:
  BoundaryFieldInput(const Field<int> face_set)
      : bke::MeshFieldInput(CPPType::get<bool>(), "Face Group Boundaries"), face_set_(face_set)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    const bke::MeshFieldContext face_context{mesh, AttrDomain::Face};
    FieldEvaluator face_evaluator{face_context, mesh.faces_num};
    face_evaluator.add(face_set_);
    face_evaluator.evaluate();
    const VArray<int> faces_group_id = face_evaluator.get_evaluated<int>(0);
    if (faces_group_id.is_single()) {
      return {};
    }

    Array<bool> boundary(mesh.edges_num, false);

    Array<std::atomic<int>> edge_states(mesh.edges_num);
    /* State is index of face or one of invalid values: */
    static constexpr int no_face_yet = -1;
    static constexpr int is_boundary = -2;

    threading::parallel_for(edge_states.index_range(), 4096, [&](const IndexRange range) {
      for (std::atomic<int> &v : edge_states.as_mutable_span().slice(range)) {
        v.store(no_face_yet, std::memory_order_relaxed);
      }
    });

    const GroupedSpan<int> face_edges(mesh.face_offsets(), mesh.corner_edges());
    threading::parallel_for(face_edges.index_range(), 2048, [&](const IndexRange range) {
      for (const int face_i : range) {
        const int group_id = faces_group_id[face_i];
        for (const int edge_i : face_edges[face_i]) {
          std::atomic<int> &edge_state = edge_states[edge_i];
          while (true) {
            int edge_state_value = edge_state.load(std::memory_order_relaxed);
            switch (edge_state_value) {
              case is_boundary:
                break;
              case no_face_yet: {
                if (edge_state.compare_exchange_weak(edge_state_value,
                                                     face_i,
                                                     std::memory_order_relaxed,
                                                     std::memory_order_relaxed))
                {
                  break;
                }
                continue;
              }
              default: {
                if (faces_group_id[edge_state_value] == group_id) {
                  break;
                }
                if (edge_state.compare_exchange_weak(
                        edge_state_value, is_boundary, std::memory_order_release))
                {
                  boundary[edge_i] = true;
                  break;
                }
                continue;
              }
            }
            break;
          }
        }
      }
    });
    return mesh.attributes().adapt_domain<bool>(
        VArray<bool>::ForContainer(std::move(boundary)), AttrDomain::Edge, domain);
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    face_set_.node().for_each_field_input_recursive(fn);
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Edge;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> face_set_field = params.extract_input<Field<int>>("Face Set");
  Field<bool> face_set_boundaries{std::make_shared<BoundaryFieldInput>(face_set_field)};
  params.set_output("Boundary Edges", std::move(face_set_boundaries));
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_MESH_FACE_GROUP_BOUNDARIES, "Face Group Boundaries", NODE_CLASS_INPUT);
  bke::node_type_size_preset(&ntype, bke::eNodeSizePreset::Middle);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_face_group_boundaries_cc
