/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.hh"

#include "BKE_mesh.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_face_is_planar_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Threshold")
      .default_value(0.01f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .supports_field()
      .description(
          "The distance a point can be from the surface before the face is no longer "
          "considered planar");
  b.add_output<decl::Bool>("Planar")
      .translation_context(BLT_I18NCONTEXT_ID_NODETREE)
      .field_source();
}

class PlanarFieldInput final : public bke::MeshFieldInput {
 private:
  Field<float> threshold_;

 public:
  PlanarFieldInput(Field<float> threshold)
      : bke::MeshFieldInput(CPPType::get<bool>(), "Planar"), threshold_(threshold)
  {
    category_ = Category::Generated;
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
        if (dot > max) {
          max = dot;
        }
        if (dot < min) {
          min = dot;
        }
      }
      return max - min < thresholds[i] / 2.0f;
    };

    return mesh.attributes().adapt_domain<bool>(
        VArray<bool>::ForFunc(faces.size(), planar_fn), AttrDomain::Face, domain);
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    threshold_.node().for_each_field_input_recursive(fn);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 2356235652;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const PlanarFieldInput *>(&other) != nullptr;
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Face;
  }
};

static void geo_node_exec(GeoNodeExecParams params)
{
  Field<float> threshold = params.extract_input<Field<float>>("Threshold");
  Field<bool> planar_field{std::make_shared<PlanarFieldInput>(threshold)};
  params.set_output("Planar", std::move(planar_field));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_MESH_FACE_IS_PLANAR, "Is Face Planar", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = geo_node_exec;
  ntype.declare = node_declare;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_mesh_face_is_planar_cc
