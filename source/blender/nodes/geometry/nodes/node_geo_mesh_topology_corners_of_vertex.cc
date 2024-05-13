/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"

#include "BLI_array_utils.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_topology_corners_of_vertex_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Vertex Index")
      .implicit_field(implicit_field_inputs::index)
      .description("The vertex to retrieve data from. Defaults to the vertex from the context");
  b.add_input<decl::Float>("Weights").supports_field().hide_value().description(
      "Values used to sort corners attached to the vertex. Uses indices by default");
  b.add_input<decl::Int>("Sort Index")
      .min(0)
      .supports_field()
      .description("Which of the sorted corners to output");
  b.add_output<decl::Int>("Corner Index")
      .field_source_reference_all()
      .description("A corner connected to the face, chosen by the sort index");
  b.add_output<decl::Int>("Total").field_source().reference_pass({0}).description(
      "The number of faces or corners connected to each vertex");
}

class CornersOfVertInput final : public bke::MeshFieldInput {
  const Field<int> vert_index_;
  const Field<int> sort_index_;
  const Field<float> sort_weight_;

 public:
  CornersOfVertInput(Field<int> vert_index, Field<int> sort_index, Field<float> sort_weight)
      : bke::MeshFieldInput(CPPType::get<int>(), "Corner of Vertex"),
        vert_index_(std::move(vert_index)),
        sort_index_(std::move(sort_index)),
        sort_weight_(std::move(sort_weight))
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask &mask) const final
  {
    const IndexRange vert_range(mesh.verts_num);
    const GroupedSpan<int> vert_to_corner_map = mesh.vert_to_corner_map();

    const bke::MeshFieldContext context{mesh, domain};
    fn::FieldEvaluator evaluator{context, &mask};
    evaluator.add(vert_index_);
    evaluator.add(sort_index_);
    evaluator.evaluate();
    const VArray<int> vert_indices = evaluator.get_evaluated<int>(0);
    const VArray<int> indices_in_sort = evaluator.get_evaluated<int>(1);

    const bke::MeshFieldContext corner_context{mesh, AttrDomain::Corner};
    fn::FieldEvaluator corner_evaluator{corner_context, mesh.corners_num};
    corner_evaluator.add(sort_weight_);
    corner_evaluator.evaluate();
    const VArray<float> all_sort_weights = corner_evaluator.get_evaluated<float>(0);
    const bool use_sorting = !all_sort_weights.is_single();

    Array<int> corner_of_vertex(mask.min_array_size());
    mask.foreach_segment(GrainSize(1024), [&](const IndexMaskSegment segment) {
      /* Reuse arrays to avoid allocation. */
      Array<float> sort_weights;
      Array<int> sort_indices;

      for (const int selection_i : segment) {
        const int vert_i = vert_indices[selection_i];
        const int index_in_sort = indices_in_sort[selection_i];
        if (!vert_range.contains(vert_i)) {
          corner_of_vertex[selection_i] = 0;
          continue;
        }

        const Span<int> corners = vert_to_corner_map[vert_i];
        if (corners.is_empty()) {
          corner_of_vertex[selection_i] = 0;
          continue;
        }

        const int index_in_sort_wrapped = mod_i(index_in_sort, corners.size());
        if (use_sorting) {
          /* Retrieve a compressed array of weights for each edge. */
          sort_weights.reinitialize(corners.size());
          IndexMaskMemory memory;
          all_sort_weights.materialize_compressed(IndexMask::from_indices<int>(corners, memory),
                                                  sort_weights.as_mutable_span());

          /* Sort a separate array of compressed indices corresponding to the compressed weights.
           * This allows using `materialize_compressed` to avoid virtual function call overhead
           * when accessing values in the sort weights. However, it means a separate array of
           * indices within the compressed array is necessary for sorting. */
          sort_indices.reinitialize(corners.size());
          array_utils::fill_index_range<int>(sort_indices);
          std::stable_sort(sort_indices.begin(), sort_indices.end(), [&](int a, int b) {
            return sort_weights[a] < sort_weights[b];
          });
          corner_of_vertex[selection_i] = corners[sort_indices[index_in_sort_wrapped]];
        }
        else {
          corner_of_vertex[selection_i] = corners[index_in_sort_wrapped];
        }
      }
    });

    return VArray<int>::ForContainer(std::move(corner_of_vertex));
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    vert_index_.node().for_each_field_input_recursive(fn);
    sort_index_.node().for_each_field_input_recursive(fn);
    sort_weight_.node().for_each_field_input_recursive(fn);
  }

  uint64_t hash() const final
  {
    return 3541871368173645;
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    if (const auto *typed = dynamic_cast<const CornersOfVertInput *>(&other)) {
      return typed->vert_index_ == vert_index_ && typed->sort_index_ == sort_index_ &&
             typed->sort_weight_ == sort_weight_;
    }
    return false;
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return AttrDomain::Point;
  }
};

class CornersOfVertCountInput final : public bke::MeshFieldInput {
 public:
  CornersOfVertCountInput() : bke::MeshFieldInput(CPPType::get<int>(), "Vertex Corner Count")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != AttrDomain::Point) {
      return {};
    }
    Array<int> counts(mesh.verts_num, 0);
    array_utils::count_indices(mesh.corner_verts(), counts);
    return VArray<int>::ForContainer(std::move(counts));
  }

  uint64_t hash() const final
  {
    return 253098745374645;
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    return dynamic_cast<const CornersOfVertCountInput *>(&other) != nullptr;
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return AttrDomain::Point;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> vert_index = params.extract_input<Field<int>>("Vertex Index");
  if (params.output_is_required("Total")) {
    params.set_output("Total",
                      Field<int>(std::make_shared<bke::EvaluateAtIndexInput>(
                          vert_index,
                          Field<int>(std::make_shared<CornersOfVertCountInput>()),
                          AttrDomain::Point)));
  }
  if (params.output_is_required("Corner Index")) {
    params.set_output("Corner Index",
                      Field<int>(std::make_shared<CornersOfVertInput>(
                          vert_index,
                          params.extract_input<Field<int>>("Sort Index"),
                          params.extract_input<Field<float>>("Weights"))));
  }
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_MESH_TOPOLOGY_CORNERS_OF_VERTEX, "Corners of Vertex", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_topology_corners_of_vertex_cc
