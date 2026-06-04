/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "BLI_math_vector.hh"

#include "DNA_mesh_types.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_face_is_planar_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Threshold"_ustr)
      .default_value(0.01f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .structure_type(StructureType::Field)
      .description(
          "The distance a point can be from the surface before the face is no longer "
          "considered planar");
  b.add_output<decl::Bool>("Planar"_ustr)
      .translation_context(BLT_I18NCONTEXT_ID_NODETREE)
      .structure_type(StructureType::Field);
}

class PlanarFieldInput final : public bke::MeshFieldInput {
 private:
  Field<float> threshold_;

 public:
  PlanarFieldInput(Field<float> threshold)
      : bke::MeshFieldInput(CPPType::get<bool>(), "Planar"), threshold_(threshold)
  {
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    const Span<float3> positions = mesh.vert_positions();
    const OffsetIndices faces = mesh.faces();
    const Span<int> corner_verts = mesh.corner_verts();
    const Span<float3> face_normals = mesh.face_normals();

    const bke::MeshFieldContext context{mesh, AttrDomain::Face};
    fn::FieldEvaluator evaluator{context, faces.size()};
    evaluator.add(threshold_);
    evaluator.evaluate();
    const VArray<float> thresholds = evaluator.get_evaluated<float>(0);

    auto planar_fn =
        [positions, faces, corner_verts, thresholds, face_normals](const int i) -> bool {
      const IndexRange face = faces[i];
      if (face.size() <= 3) {
        return true;
      }
      const float3 &reference_normal = face_normals[i];

      float min = FLT_MAX;
      float max = -FLT_MAX;

      for (const int vert : corner_verts.slice(face)) {
        float dot = math::dot(reference_normal, positions[vert]);
        max = std::max(dot, max);
        min = std::min(dot, min);
      }
      return max - min < thresholds[i] / 2.0f;
    };

    return mesh.attributes().adapt_domain<bool>(
        VArray<bool>::from_func(faces.size(), planar_fn), AttrDomain::Face, domain);
  }

  void foreach_recursive_field(FunctionRef<void(const GField &)> fn) const override
  {
    fn(threshold_);
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
    hash.add(deep_hash_cache.ensure(threshold_));
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

static void geo_node_exec(GeoNodeExecParams params)
{
  Field<float> threshold = params.extract_input<Field<float>>("Threshold"_ustr);
  params.set_output("Planar"_ustr, Field<bool>::from_input<PlanarFieldInput>(threshold));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(
      &ntype, "GeometryNodeInputMeshFaceIsPlanar"_ustr, GEO_NODE_INPUT_MESH_FACE_IS_PLANAR);
  ntype.ui_name = "Is Face Planar";
  ntype.ui_description =
      "Retrieve whether all triangles in a face are on the same plane, i.e. whether they have the "
      "same normal";
  ntype.enum_name_legacy = "MESH_FACE_IS_PLANAR";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = geo_node_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_mesh_face_is_planar_cc
