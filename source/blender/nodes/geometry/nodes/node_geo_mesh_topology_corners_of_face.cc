/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_mesh.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_topology_corners_of_face_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Face Index")
      .implicit_field(implicit_field_inputs::index)
      .description("The face to retrieve data from. Defaults to the face from the context");
  b.add_input<decl::Float>("Weights").supports_field().hide_value().description(
      "Values used to sort the face's corners. Uses indices by default");
  b.add_input<decl::Int>("Sort Index")
      .min(0)
      .supports_field()
      .description("Which of the sorted corners to output");
  b.add_output<decl::Int>("Corner Index")
      .field_source_reference_all()
      .description("A corner of the face, chosen by the sort index");
  b.add_output<decl::Int>("Total").field_source().reference_pass({0}).description(
      "The number of corners in the face");
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
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
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

    const bke::MeshFieldContext corner_context{mesh, ATTR_DOMAIN_CORNER};
    fn::FieldEvaluator corner_evaluator{corner_context, mesh.totloop};
    corner_evaluator.add(sort_weight_);
    corner_evaluator.evaluate();
    const VArray<float> all_sort_weights = corner_evaluator.get_evaluated<float>(0);
    const bool use_sorting = !all_sort_weights.is_single();

    Array<int> corner_of_face(mask.min_array_size());
    mask.foreach_segment(GrainSize(1024), [&](const IndexMaskSegment segment) {
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

          /* Sort a separate array of compressed indices corresponding to the compressed weights.
           * This allows using `materialize_compressed` to avoid virtual function call overhead
           * when accessing values in the sort weights. However, it means a separate array of
           * indices within the compressed array is necessary for sorting. */
          sort_indices.reinitialize(corners.size());
          std::iota(sort_indices.begin(), sort_indices.end(), 0);
          std::stable_sort(sort_indices.begin(), sort_indices.end(), [&](int a, int b) {
            return sort_weights[a] < sort_weights[b];
          });
          corner_of_face[selection_i] = corners[sort_indices[index_in_sort_wrapped]];
        }
        else {
          corner_of_face[selection_i] = corners[index_in_sort_wrapped];
        }
      }
    });

    return VArray<int>::ForContainer(std::move(corner_of_face));
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    face_index_.node().for_each_field_input_recursive(fn);
    sort_index_.node().for_each_field_input_recursive(fn);
    sort_weight_.node().for_each_field_input_recursive(fn);
  }

  uint64_t hash() const final
  {
    return 6927982716657;
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    if (const auto *typed = dynamic_cast<const CornersOfFaceInput *>(&other)) {
      return typed->face_index_ == face_index_ && typed->sort_index_ == sort_index_ &&
             typed->sort_weight_ == sort_weight_;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return ATTR_DOMAIN_FACE;
  }
};

class CornersOfFaceCountInput final : public bke::MeshFieldInput {
 public:
  CornersOfFaceCountInput() : bke::MeshFieldInput(CPPType::get<int>(), "Face Corner Count")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != ATTR_DOMAIN_FACE) {
      return {};
    }
    const OffsetIndices faces = mesh.faces();
    return VArray<int>::ForFunc(mesh.faces_num,
                                [faces](const int64_t i) { return faces[i].size(); });
  }

  uint64_t hash() const final
  {
    return 8345908765432698;
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    return dynamic_cast<const CornersOfFaceCountInput *>(&other) != nullptr;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return ATTR_DOMAIN_FACE;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> face_index = params.extract_input<Field<int>>("Face Index");
  if (params.output_is_required("Total")) {
    params.set_output("Total",
                      Field<int>(std::make_shared<EvaluateAtIndexInput>(
                          face_index,
                          Field<int>(std::make_shared<CornersOfFaceCountInput>()),
                          ATTR_DOMAIN_FACE)));
  }
  if (params.output_is_required("Corner Index")) {
    params.set_output("Corner Index",
                      Field<int>(std::make_shared<CornersOfFaceInput>(
                          face_index,
                          params.extract_input<Field<int>>("Sort Index"),
                          params.extract_input<Field<float>>("Weights"))));
  }
}

}  // namespace blender::nodes::node_geo_mesh_topology_corners_of_face_cc

void register_node_type_geo_mesh_topology_corners_of_face()
{
  namespace file_ns = blender::nodes::node_geo_mesh_topology_corners_of_face_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_MESH_TOPOLOGY_CORNERS_OF_FACE, "Corners of Face", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
