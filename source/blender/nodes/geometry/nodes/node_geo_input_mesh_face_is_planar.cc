/* SPDX-License-Identifier: GPL-2.0-or-later */

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

class PlanarFieldInput final : public GeometryFieldInput {
 private:
  Field<float> threshold_;

 public:
  PlanarFieldInput(Field<float> threshold)
      : GeometryFieldInput(CPPType::get<bool>(), "Planar"), threshold_(threshold)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const eAttrDomain domain,
                                 [[maybe_unused]] IndexMask mask) const final
  {
    if (component.type() != GEO_COMPONENT_TYPE_MESH) {
      return {};
    }

    const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
    const Mesh *mesh = mesh_component.get_for_read();
    if (mesh == nullptr) {
      return {};
    }

    GeometryComponentFieldContext context{mesh_component, ATTR_DOMAIN_FACE};
    fn::FieldEvaluator evaluator{context, mesh->totpoly};
    evaluator.add(threshold_);
    evaluator.evaluate();
    const VArray<float> thresholds = evaluator.get_evaluated<float>(0);

    Span<float3> poly_normals{(float3 *)BKE_mesh_poly_normals_ensure(mesh), mesh->totpoly};

    auto planar_fn = [mesh, thresholds, poly_normals](const int i_poly) -> bool {
      if (mesh->mpoly[i_poly].totloop <= 3) {
        return true;
      }
      const int loopstart = mesh->mpoly[i_poly].loopstart;
      const int loops = mesh->mpoly[i_poly].totloop;
      Span<MLoop> poly_loops(&mesh->mloop[loopstart], loops);
      float3 reference_normal = poly_normals[i_poly];

      float min = FLT_MAX;
      float max = -FLT_MAX;

      for (const int i_loop : poly_loops.index_range()) {
        const float3 vert = mesh->mvert[poly_loops[i_loop].v].co;
        float dot = math::dot(reference_normal, vert);
        if (dot > max) {
          max = dot;
        }
        if (dot < min) {
          min = dot;
        }
      }
      return max - min < thresholds[i_poly] / 2.0f;
    };

    return component.attributes()->adapt_domain<bool>(
        VArray<bool>::ForFunc(mesh->totpoly, planar_fn), ATTR_DOMAIN_FACE, domain);
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
