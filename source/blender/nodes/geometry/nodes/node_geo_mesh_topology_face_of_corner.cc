/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_topology_face_of_corner_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Corner Index")
      .implicit_field(implicit_field_inputs::index)
      .description("The corner to retrieve data from. Defaults to the corner from the context");
  b.add_output<decl::Int>("Face Index")
      .field_source_reference_all()
      .description("The index of the face the corner is a part of");
  b.add_output<decl::Int>("Index in Face")
      .field_source_reference_all()
      .description("The index of the corner starting from the first corner in the face");
}

class CornerFaceIndexInput final : public bke::MeshFieldInput {
 public:
  CornerFaceIndexInput() : bke::MeshFieldInput(CPPType::get<int>(), "Corner Face Index")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != ATTR_DOMAIN_CORNER) {
      return {};
    }
    return VArray<int>::ForContainer(bke::mesh::build_loop_to_face_map(mesh.faces()));
  }

  uint64_t hash() const final
  {
    return 2348712958475728;
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    return dynamic_cast<const CornerFaceIndexInput *>(&other) != nullptr;
  }
};

class CornerIndexInFaceInput final : public bke::MeshFieldInput {
 public:
  CornerIndexInFaceInput() : bke::MeshFieldInput(CPPType::get<int>(), "Corner Index In Face")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != ATTR_DOMAIN_CORNER) {
      return {};
    }
    const OffsetIndices faces = mesh.faces();
    Array<int> loop_to_face_map = bke::mesh::build_loop_to_face_map(faces);
    return VArray<int>::ForFunc(
        mesh.totloop, [faces, loop_to_face_map = std::move(loop_to_face_map)](const int corner_i) {
          const int face_i = loop_to_face_map[corner_i];
          return corner_i - faces[face_i].start();
        });
  }

  uint64_t hash() const final
  {
    return 97837176448;
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    return dynamic_cast<const CornerIndexInFaceInput *>(&other) != nullptr;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return ATTR_DOMAIN_CORNER;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> corner_index = params.extract_input<Field<int>>("Corner Index");
  if (params.output_is_required("Face Index")) {
    params.set_output("Face Index",
                      Field<int>(std::make_shared<EvaluateAtIndexInput>(
                          corner_index,
                          Field<int>(std::make_shared<CornerFaceIndexInput>()),
                          ATTR_DOMAIN_CORNER)));
  }
  if (params.output_is_required("Index in Face")) {
    params.set_output("Index in Face",
                      Field<int>(std::make_shared<EvaluateAtIndexInput>(
                          corner_index,
                          Field<int>(std::make_shared<CornerIndexInFaceInput>()),
                          ATTR_DOMAIN_CORNER)));
  }
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_MESH_TOPOLOGY_FACE_OF_CORNER, "Face of Corner", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_topology_face_of_corner_cc
