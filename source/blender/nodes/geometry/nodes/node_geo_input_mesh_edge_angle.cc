/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_edge_angle_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Unsigned Angle"))
      .field_source()
      .description(
          "The shortest angle in radians between two faces where they meet at an edge. Flat edges "
          "and Non-manifold edges have an angle of zero. Computing this value is faster than the "
          "signed angle");
  b.add_output<decl::Float>(N_("Signed Angle"))
      .field_source()
      .description(
          "The signed angle in radians between two faces where they meet at an edge. Flat edges "
          "and Non-manifold edges have an angle of zero. Concave angles are positive and convex "
          "angles are negative. Computing this value is slower than the unsigned angle");
}

struct EdgeMapEntry {
  int face_count;
  int face_index_1;
  int face_index_2;
};

static Array<EdgeMapEntry> create_edge_map(const Span<MPoly> polys,
                                           const Span<MLoop> loops,
                                           const int total_edges)
{
  Array<EdgeMapEntry> edge_map(total_edges, {0, 0, 0});

  for (const int i_poly : polys.index_range()) {
    const MPoly &mpoly = polys[i_poly];
    for (const MLoop &loop : loops.slice(mpoly.loopstart, mpoly.totloop)) {
      EdgeMapEntry &entry = edge_map[loop.e];
      if (entry.face_count == 0) {
        entry.face_index_1 = i_poly;
      }
      else if (entry.face_count == 1) {
        entry.face_index_2 = i_poly;
      }
      entry.face_count++;
    }
  }
  return edge_map;
}

class AngleFieldInput final : public bke::MeshFieldInput {
 public:
  AngleFieldInput() : bke::MeshFieldInput(CPPType::get<float>(), "Unsigned Angle Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask /*mask*/) const final
  {
    const Span<float3> positions = mesh.vert_positions();
    const Span<MPoly> polys = mesh.polys();
    const Span<MLoop> loops = mesh.loops();
    Array<EdgeMapEntry> edge_map = create_edge_map(polys, loops, mesh.totedge);

    auto angle_fn =
        [edge_map = std::move(edge_map), positions, polys, loops](const int i) -> float {
      if (edge_map[i].face_count != 2) {
        return 0.0f;
      }
      const MPoly &mpoly_1 = polys[edge_map[i].face_index_1];
      const MPoly &mpoly_2 = polys[edge_map[i].face_index_2];
      float3 normal_1, normal_2;
      BKE_mesh_calc_poly_normal(&mpoly_1,
                                &loops[mpoly_1.loopstart],
                                reinterpret_cast<const float(*)[3]>(positions.data()),
                                normal_1);
      BKE_mesh_calc_poly_normal(&mpoly_2,
                                &loops[mpoly_2.loopstart],
                                reinterpret_cast<const float(*)[3]>(positions.data()),
                                normal_2);
      return angle_normalized_v3v3(normal_1, normal_2);
    };

    VArray<float> angles = VArray<float>::ForFunc(mesh.totedge, angle_fn);
    return mesh.attributes().adapt_domain<float>(std::move(angles), ATTR_DOMAIN_EDGE, domain);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 32426725235;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const AngleFieldInput *>(&other) != nullptr;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_EDGE;
  }
};

class SignedAngleFieldInput final : public bke::MeshFieldInput {
 public:
  SignedAngleFieldInput() : bke::MeshFieldInput(CPPType::get<float>(), "Signed Angle Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask /*mask*/) const final
  {
    const Span<float3> positions = mesh.vert_positions();
    const Span<MEdge> edges = mesh.edges();
    const Span<MPoly> polys = mesh.polys();
    const Span<MLoop> loops = mesh.loops();
    Array<EdgeMapEntry> edge_map = create_edge_map(polys, loops, mesh.totedge);

    auto angle_fn =
        [edge_map = std::move(edge_map), positions, edges, polys, loops](const int i) -> float {
      if (edge_map[i].face_count != 2) {
        return 0.0f;
      }
      const MPoly &mpoly_1 = polys[edge_map[i].face_index_1];
      const MPoly &mpoly_2 = polys[edge_map[i].face_index_2];

      /* Find the normals of the 2 polys. */
      float3 poly_1_normal, poly_2_normal;
      BKE_mesh_calc_poly_normal(&mpoly_1,
                                &loops[mpoly_1.loopstart],
                                reinterpret_cast<const float(*)[3]>(positions.data()),
                                poly_1_normal);
      BKE_mesh_calc_poly_normal(&mpoly_2,
                                &loops[mpoly_2.loopstart],
                                reinterpret_cast<const float(*)[3]>(positions.data()),
                                poly_2_normal);

      /* Find the centerpoint of the axis edge */
      const float3 edge_centerpoint = (positions[edges[i].v1] + positions[edges[i].v2]) * 0.5f;

      /* Get the centerpoint of poly 2 and subtract the edge centerpoint to get a tangent
       * normal for poly 2. */
      float3 poly_center_2;
      BKE_mesh_calc_poly_center(&mpoly_2,
                                &loops[mpoly_2.loopstart],
                                reinterpret_cast<const float(*)[3]>(positions.data()),
                                poly_center_2);
      const float3 poly_2_tangent = math::normalize(poly_center_2 - edge_centerpoint);
      const float concavity = math::dot(poly_1_normal, poly_2_tangent);

      /* Get the unsigned angle between the two polys */
      const float angle = angle_normalized_v3v3(poly_1_normal, poly_2_normal);

      if (angle == 0.0f || angle == 2.0f * M_PI || concavity < 0) {
        return angle;
      }
      return -angle;
    };

    VArray<float> angles = VArray<float>::ForFunc(mesh.totedge, angle_fn);
    return mesh.attributes().adapt_domain<float>(std::move(angles), ATTR_DOMAIN_EDGE, domain);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 68465416863;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const SignedAngleFieldInput *>(&other) != nullptr;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_EDGE;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  if (params.output_is_required("Unsigned Angle")) {
    Field<float> angle_field{std::make_shared<AngleFieldInput>()};
    params.set_output("Unsigned Angle", std::move(angle_field));
  }
  if (params.output_is_required("Signed Angle")) {
    Field<float> angle_field{std::make_shared<SignedAngleFieldInput>()};
    params.set_output("Signed Angle", std::move(angle_field));
  }
}

}  // namespace blender::nodes::node_geo_input_mesh_edge_angle_cc

void register_node_type_geo_input_mesh_edge_angle()
{
  namespace file_ns = blender::nodes::node_geo_input_mesh_edge_angle_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_INPUT_MESH_EDGE_ANGLE, "Edge Angle", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
