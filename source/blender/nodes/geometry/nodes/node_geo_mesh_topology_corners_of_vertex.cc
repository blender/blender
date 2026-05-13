/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "BLI_array_utils.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_topology_corners_of_vertex_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Vertex Index"_ustr)
      .implicit_field(NODE_DEFAULT_INPUT_INDEX_FIELD)
      .description("The vertex to retrieve data from. Defaults to the vertex from the context")
      .structure_type(StructureType::Field);
  b.add_input<decl::Float>("Weights"_ustr)
      .supports_field()
      .hide_value()
      .description("Values used to sort corners attached to the vertex. Uses indices by default");
  b.add_input<decl::Int>("Sort Index"_ustr)
      .supports_field()
      .description("Which of the sorted corners to output. Negative indexing is supported");
  b.add_output<decl::Int>("Corner Index"_ustr)
      .field_source_reference_all()
      .description("A corner connected to the face, chosen by the sort index");
  b.add_output<decl::Int>("Total"_ustr)
      .field_source()
      .reference_pass({0})
      .description("The number of faces or corners connected to each vertex");
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
    mask.foreach_segment(
        [&](const IndexMaskSegment segment) {
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
              all_sort_weights.materialize_compressed(
                  IndexMask::from_indices<int>(corners, memory), sort_weights.as_mutable_span());

              /* Sort a separate array of compressed indices corresponding to the compressed
               * weights. This allows using `materialize_compressed` to avoid virtual function call
               * overhead when accessing values in the sort weights. However, it means a separate
               * array of indices within the compressed array is necessary for sorting. */
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
        },
        exec_mode::grain_size(1024));

    return VArray<int>::from_container(std::move(corner_of_vertex));
  }

  void foreach_recursive_field(FunctionRef<void(const GField &)> fn) const override
  {
    fn(vert_index_);
    fn(sort_index_);
    fn(sort_weight_);
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
    hash.add(deep_hash_cache.ensure(vert_index_));
    hash.add(deep_hash_cache.ensure(sort_index_));
    hash.add(deep_hash_cache.ensure(sort_weight_));
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return AttrDomain::Point;
  }
};

class CornersOfVertCountInput final : public bke::MeshFieldInput {
 public:
  CornersOfVertCountInput() : bke::MeshFieldInput(CPPType::get<int>(), "Vertex Corner Count") {}

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != AttrDomain::Point) {
      return {};
    }
    Array<int> counts(mesh.verts_num, 0);
    array_utils::count_indices(mesh.corner_verts(), counts);
    return VArray<int>::from_container(std::move(counts));
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep & /*deep_hash_cache*/) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return AttrDomain::Point;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> vert_index = params.extract_input<Field<int>>("Vertex Index"_ustr);
  if (params.output_is_required("Total"_ustr)) {
    params.set_output(
        "Total"_ustr,
        Field<int>::from_input<bke::EvaluateAtIndexInput>(
            vert_index, Field<int>::from_input<CornersOfVertCountInput>(), AttrDomain::Point));
  }
  if (params.output_is_required("Corner Index"_ustr)) {
    params.set_output("Corner Index"_ustr,
                      Field<int>::from_input<CornersOfVertInput>(
                          vert_index,
                          params.extract_input<Field<int>>("Sort Index"_ustr),
                          params.extract_input<Field<float>>("Weights"_ustr)));
  }
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, "GeometryNodeCornersOfVertex"_ustr, GEO_NODE_MESH_TOPOLOGY_CORNERS_OF_VERTEX);
  ntype.ui_name = "Corners of Vertex";
  ntype.ui_description = "Retrieve face corners connected to vertices";
  ntype.enum_name_legacy = "CORNERS_OF_VERTEX";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_topology_corners_of_vertex_cc
