/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_topology_face_of_corner_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Corner Index"_ustr)
      .implicit_field(NODE_DEFAULT_INPUT_INDEX_FIELD)
      .description("The corner to retrieve data from. Defaults to the corner from the context")
      .structure_type(StructureType::Field);
  b.add_output<decl::Int>("Face Index"_ustr)
      .field_source_reference_all()
      .description("The index of the face the corner is a part of");
  b.add_output<decl::Int>("Index in Face"_ustr)
      .field_source_reference_all()
      .description("The index of the corner starting from the first corner in the face");
}

class CornerFaceIndexInput final : public bke::MeshFieldInput {
 public:
  CornerFaceIndexInput() : bke::MeshFieldInput(CPPType::get<int>(), "Corner Face Index") {}

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != AttrDomain::Corner) {
      return {};
    }
    return VArray<int>::from_span(mesh.corner_to_face_map());
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep & /*deep_hash_cache*/) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }
};

class CornerIndexInFaceInput final : public bke::MeshFieldInput {
 public:
  CornerIndexInFaceInput() : bke::MeshFieldInput(CPPType::get<int>(), "Corner Index In Face") {}

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != AttrDomain::Corner) {
      return {};
    }
    const OffsetIndices faces = mesh.faces();
    const Span<int> corner_to_face = mesh.corner_to_face_map();
    return VArray<int>::from_func(mesh.corners_num, [faces, corner_to_face](const int corner) {
      const int face_i = corner_to_face[corner];
      return corner - faces[face_i].start();
    });
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep & /*deep_hash_cache*/) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return AttrDomain::Corner;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> corner_index = params.extract_input<Field<int>>("Corner Index"_ustr);
  if (params.output_is_required("Face Index"_ustr)) {
    params.set_output(
        "Face Index"_ustr,
        Field<int>::from_input<bke::EvaluateAtIndexInput>(
            corner_index, Field<int>::from_input<CornerFaceIndexInput>(), AttrDomain::Corner));
  }
  if (params.output_is_required("Index in Face"_ustr)) {
    params.set_output(
        "Index in Face"_ustr,
        Field<int>::from_input<bke::EvaluateAtIndexInput>(
            corner_index, Field<int>::from_input<CornerIndexInFaceInput>(), AttrDomain::Corner));
  }
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, "GeometryNodeFaceOfCorner"_ustr, GEO_NODE_MESH_TOPOLOGY_FACE_OF_CORNER);
  ntype.ui_name = "Face of Corner";
  ntype.ui_description = "Retrieve the face each face corner is part of";
  ntype.enum_name_legacy = "FACE_OF_CORNER";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_topology_face_of_corner_cc
