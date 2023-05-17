/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.hh"

#include "BLT_translation.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_face_area_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Area")
      .translation_context(BLT_I18NCONTEXT_AMOUNT)
      .field_source()
      .description("The surface area of each of the mesh's faces");
}

static VArray<float> construct_face_area_varray(const Mesh &mesh, const eAttrDomain domain)
{
  const Span<float3> positions = mesh.vert_positions();
  const OffsetIndices polys = mesh.polys();
  const Span<int> corner_verts = mesh.corner_verts();

  auto area_fn = [positions, polys, corner_verts](const int i) -> float {
    return bke::mesh::poly_area_calc(positions, corner_verts.slice(polys[i]));
  };

  return mesh.attributes().adapt_domain<float>(
      VArray<float>::ForFunc(polys.size(), area_fn), ATTR_DOMAIN_FACE, domain);
}

class FaceAreaFieldInput final : public bke::MeshFieldInput {
 public:
  FaceAreaFieldInput() : bke::MeshFieldInput(CPPType::get<float>(), "Face Area Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask /*mask*/) const final
  {
    return construct_face_area_varray(mesh, domain);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 1346334523;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const FaceAreaFieldInput *>(&other) != nullptr;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_FACE;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Area", Field<float>(std::make_shared<FaceAreaFieldInput>()));
}

}  // namespace blender::nodes::node_geo_input_mesh_face_area_cc

void register_node_type_geo_input_mesh_face_area()
{
  namespace file_ns = blender::nodes::node_geo_input_mesh_face_area_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_INPUT_MESH_FACE_AREA, "Face Area", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
