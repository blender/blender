/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "DNA_mesh_types.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_topology_corners_of_face_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Face Index"_ustr)
      .implicit_field(NODE_DEFAULT_INPUT_INDEX_FIELD)
      .description("The face to retrieve data from. Defaults to the face from the context")
      .structure_type(StructureType::Field);
  b.add_input<decl::Float>("Weights"_ustr)
      .supports_field()
      .hide_value()
      .description("Values used to sort the face's corners. Uses indices by default");
  b.add_input<decl::Int>("Sort Index"_ustr)
      .supports_field()
      .description("Which of the sorted corners to output. Negative indexing is supported");
  b.add_output<decl::Int>("Corner Index"_ustr)
      .field_source_reference_all()
      .description("A corner of the face, chosen by the sort index");
  b.add_output<decl::Int>("Total"_ustr)
      .field_source()
      .reference_pass({0})
      .description("The number of corners in the face");
}

class CornersOfFaceInput final : public bke::MeshFieldInput {
  const Field<int> face_index_;
  const Field<int> sort_index_;
  const Field<float> sort_weight_;

 public:
  CornersOfFaceInput(Field<int> face_index, Field<int> sort_index, Field<float> sort_weight)
      : bke::MeshFieldInput(CPPType::get<int>(), "Corner of Face"),
        face_index_(std::move(face_index)),
        sort_index_(std::move(sort_index)),
        sort_weight_(std::move(sort_weight))
  {
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask &mask) const final
  {
    const OffsetIndices faces = mesh.faces();

    const bke::MeshFieldContext context{mesh, domain};
    fn::FieldEvaluator evaluator{context, &mask};
    evaluator.add(face_index_);
    evaluator.add(sort_index_);
    evaluator.evaluate();
    const VArray<int> face_indices = evaluator.get_evaluated<int>(0);
    const VArray<int> indices_in_sort = evaluator.get_evaluated<int>(1);

    const bke::MeshFieldContext corner_context{mesh, AttrDomain::Corner};
    fn::FieldEvaluator corner_evaluator{corner_context, mesh.corners_num};
    corner_evaluator.add(sort_weight_);
    corner_evaluator.evaluate();
    const VArray<float> all_sort_weights = corner_evaluator.get_evaluated<float>(0);
    const bool use_sorting = !all_sort_weights.is_single();

    Array<int> corner_of_face(mask.min_array_size());
    mask.foreach_segment(
        [&](const IndexMaskSegment segment) {
          /* Reuse arrays to avoid allocation. */
          Array<float> sort_weights;
          Array<int> sort_indices;

          for (const int selection_i : segment) {
            const int face_i = face_indices[selection_i];
            const int index_in_sort = indices_in_sort[selection_i];
            if (!faces.index_range().contains(face_i)) {
              corner_of_face[selection_i] = 0;
              continue;
            }

            const IndexRange corners = faces[face_i];

            const int index_in_sort_wrapped = mod_i(index_in_sort, corners.size());
            if (use_sorting) {
              /* Retrieve the weights for each corner. */
              sort_weights.reinitialize(corners.size());
              all_sort_weights.materialize_compressed(IndexMask(corners),
                                                      sort_weights.as_mutable_span());

              /* Sort a separate array of compressed indices corresponding to the compressed
               * weights. This allows using `materialize_compressed` to avoid virtual function call
               * overhead when accessing values in the sort weights. However, it means a separate
               * array of indices within the compressed array is necessary for sorting. */
              sort_indices.reinitialize(corners.size());
              array_utils::fill_index_range<int>(sort_indices);
              std::stable_sort(sort_indices.begin(), sort_indices.end(), [&](int a, int b) {
                return sort_weights[a] < sort_weights[b];
              });
              corner_of_face[selection_i] = corners[sort_indices[index_in_sort_wrapped]];
            }
            else {
              corner_of_face[selection_i] = corners[index_in_sort_wrapped];
            }
          }
        },
        exec_mode::grain_size(1024));

    return VArray<int>::from_container(std::move(corner_of_face));
  }

  void foreach_recursive_field(FunctionRef<void(const GField &)> fn) const override
  {
    fn(face_index_);
    fn(sort_index_);
    fn(sort_weight_);
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const final
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
    hash.add(deep_hash_cache.ensure(face_index_));
    hash.add(deep_hash_cache.ensure(sort_index_));
    hash.add(deep_hash_cache.ensure(sort_weight_));
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return AttrDomain::Face;
  }
};

class CornersOfFaceCountInput final : public bke::MeshFieldInput {
 public:
  CornersOfFaceCountInput() : bke::MeshFieldInput(CPPType::get<int>(), "Face Corner Count") {}

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != AttrDomain::Face) {
      return {};
    }
    const OffsetIndices faces = mesh.faces();
    return VArray<int>::from_func(mesh.faces_num,
                                  [faces](const int64_t i) { return faces[i].size(); });
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep & /*deep_hash_cache*/) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return AttrDomain::Face;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> face_index = params.extract_input<Field<int>>("Face Index"_ustr);
  if (params.output_is_required("Total"_ustr)) {
    params.set_output(
        "Total"_ustr,
        Field<int>::from_input<bke::EvaluateAtIndexInput>(
            face_index, Field<int>::from_input<CornersOfFaceCountInput>(), AttrDomain::Face));
  }
  if (params.output_is_required("Corner Index"_ustr)) {
    params.set_output("Corner Index"_ustr,
                      Field<int>::from_input<CornersOfFaceInput>(
                          face_index,
                          params.extract_input<Field<int>>("Sort Index"_ustr),
                          params.extract_input<Field<float>>("Weights"_ustr)));
  }
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, "GeometryNodeCornersOfFace"_ustr, GEO_NODE_MESH_TOPOLOGY_CORNERS_OF_FACE);
  ntype.ui_name = "Corners of Face";
  ntype.ui_description = "Retrieve corners that make up a face";
  ntype.enum_name_legacy = "CORNERS_OF_FACE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_topology_corners_of_face_cc
