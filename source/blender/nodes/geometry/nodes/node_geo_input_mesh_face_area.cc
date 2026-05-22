/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_face_area_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Area"_ustr)
      .translation_context(BLT_I18NCONTEXT_AMOUNT)
      .structure_type(StructureType::Field)
      .description("The surface area of each of the mesh's faces");
}

static VArray<float> construct_face_area_varray(const Mesh &mesh, const AttrDomain domain)
{
  const Span<float3> positions = mesh.vert_positions();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  auto area_fn = [positions, faces, corner_verts](const int i) -> float {
    return bke::mesh::face_area_calc(positions, corner_verts.slice(faces[i]));
  };

  return mesh.attributes().adapt_domain<float>(
      VArray<float>::from_func(faces.size(), area_fn), AttrDomain::Face, domain);
}

class FaceAreaFieldInput final : public bke::MeshFieldInput {
 public:
  FaceAreaFieldInput() : bke::MeshFieldInput(CPPType::get<float>(), "Face Area Field") {}

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    return construct_face_area_varray(mesh, domain);
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep & /*deep_hash_cache*/) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Face;
  }

  bke::NativeFieldDomain native_domain_info(const Mesh & /*mesh*/) const override
  {
    return bke::NativeFieldDomain::Domain{AttrDomain::Face};
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Area"_ustr, Field<float>::from_input<FaceAreaFieldInput>());
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeInputMeshFaceArea"_ustr, GEO_NODE_INPUT_MESH_FACE_AREA);
  ntype.ui_name = "Face Area";
  ntype.ui_description = "Calculate the surface area of a mesh's faces";
  ntype.enum_name_legacy = "MESH_FACE_AREA";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_mesh_face_area_cc
