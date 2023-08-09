/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_topology_offset_corner_in_face_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Corner Index")
      .implicit_field(implicit_field_inputs::index)
      .description("The corner to retrieve data from. Defaults to the corner from the context");
  b.add_input<decl::Int>("Offset").supports_field().description(
      "The number of corners to move around the face before finding the result, "
      "circling around the start of the face if necessary");
  b.add_output<decl::Int>("Corner Index")
      .field_source_reference_all()
      .description("The index of the offset corner");
}

class OffsetCornerInFaceFieldInput final : public bke::MeshFieldInput {
  const Field<int> corner_index_;
  const Field<int> offset_;

 public:
  OffsetCornerInFaceFieldInput(Field<int> corner_index, Field<int> offset)
      : bke::MeshFieldInput(CPPType::get<int>(), "Offset Corner in Face"),
        corner_index_(std::move(corner_index)),
        offset_(std::move(offset))
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask &mask) const final
  {
    const IndexRange corner_range(mesh.totloop);
    const OffsetIndices faces = mesh.faces();

    const bke::MeshFieldContext context{mesh, domain};
    fn::FieldEvaluator evaluator{context, &mask};
    evaluator.add(corner_index_);
    evaluator.add(offset_);
    evaluator.evaluate();
    const VArray<int> corner_indices = evaluator.get_evaluated<int>(0);
    const VArray<int> offsets = evaluator.get_evaluated<int>(1);

    Array<int> loop_to_face_map = bke::mesh::build_loop_to_face_map(faces);

    Array<int> offset_corners(mask.min_array_size());
    mask.foreach_index_optimized<int>(GrainSize(2048), [&](const int selection_i) {
      const int corner_i = corner_indices[selection_i];
      const int offset = offsets[selection_i];
      if (!corner_range.contains(corner_i)) {
        offset_corners[selection_i] = 0;
        return;
      }

      const IndexRange face = faces[loop_to_face_map[corner_i]];
      offset_corners[selection_i] = apply_offset_in_cyclic_range(face, corner_i, offset);
    });

    return VArray<int>::ForContainer(std::move(offset_corners));
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    corner_index_.node().for_each_field_input_recursive(fn);
    offset_.node().for_each_field_input_recursive(fn);
  }

  uint64_t hash() const final
  {
    return get_default_hash(offset_);
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    if (const OffsetCornerInFaceFieldInput *other_field =
            dynamic_cast<const OffsetCornerInFaceFieldInput *>(&other))
    {
      return other_field->corner_index_ == corner_index_ && other_field->offset_ == offset_;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return ATTR_DOMAIN_CORNER;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Corner Index",
                    Field<int>(std::make_shared<OffsetCornerInFaceFieldInput>(
                        params.extract_input<Field<int>>("Corner Index"),
                        params.extract_input<Field<int>>("Offset"))));
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype,
                     GEO_NODE_MESH_TOPOLOGY_OFFSET_CORNER_IN_FACE,
                     "Offset Corner in Face",
                     NODE_CLASS_INPUT);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_topology_offset_corner_in_face_cc
