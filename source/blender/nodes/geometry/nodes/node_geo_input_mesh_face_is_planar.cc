/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_face_is_planar_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Threshold")
      .field_source()
      .default_value(0.01f)
      .subtype(PROP_DISTANCE)
      .supports_field()
      .description(N_("The distance a point can be from the surface before the face is no longer "
                      "considered planar"))
      .min(0.0f);
  b.add_output<decl::Bool>("Planar").field_source();
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
                                 const eAttrDomain domain,
                                 IndexMask /*mask*/) const final
  {
    const Span<float3> positions = mesh.vert_positions();
    const Span<MPoly> polys = mesh.polys();
    const Span<MLoop> loops = mesh.loops();
    const Span<float3> poly_normals = mesh.poly_normals();

    bke::MeshFieldContext context{mesh, ATTR_DOMAIN_FACE};
    fn::FieldEvaluator evaluator{context, polys.size()};
    evaluator.add(threshold_);
    evaluator.evaluate();
    const VArray<float> thresholds = evaluator.get_evaluated<float>(0);

    auto planar_fn = [positions, polys, loops, thresholds, poly_normals](const int i) -> bool {
      const MPoly &poly = polys[i];
      if (poly.totloop <= 3) {
        return true;
      }
      const Span<MLoop> poly_loops = loops.slice(poly.loopstart, poly.totloop);
      const float3 &reference_normal = poly_normals[i];

      float min = FLT_MAX;
      float max = -FLT_MAX;

      for (const int i_loop : poly_loops.index_range()) {
        const float3 &vert = positions[poly_loops[i_loop].v];
        float dot = math::dot(reference_normal, vert);
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
        VArray<bool>::ForFunc(polys.size(), planar_fn), ATTR_DOMAIN_FACE, domain);
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

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_FACE;
  }
};

static void geo_node_exec(GeoNodeExecParams params)
{
  Field<float> threshold = params.extract_input<Field<float>>("Threshold");
  Field<bool> planar_field{std::make_shared<PlanarFieldInput>(threshold)};
  params.set_output("Planar", std::move(planar_field));
}

}  // namespace blender::nodes::node_geo_input_mesh_face_is_planar_cc

void register_node_type_geo_input_mesh_face_is_planar()
{
  namespace file_ns = blender::nodes::node_geo_input_mesh_face_is_planar_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_MESH_FACE_IS_PLANAR, "Face is Planar", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::geo_node_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
