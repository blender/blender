/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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

class AngleFieldInput final : public GeometryFieldInput {
 public:
  AngleFieldInput() : GeometryFieldInput(CPPType::get<float>(), "Unsigned Angle Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() != GEO_COMPONENT_TYPE_MESH) {
      return {};
    }

    const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
    const Mesh *mesh = mesh_component.get_for_read();
    if (mesh == nullptr) {
      return {};
    }

    Span<MPoly> polys{mesh->mpoly, mesh->totpoly};
    Span<MLoop> loops{mesh->mloop, mesh->totloop};
    Array<EdgeMapEntry> edge_map = create_edge_map(polys, loops, mesh->totedge);

    auto angle_fn = [edge_map, polys, loops, mesh](const int i) -> float {
      if (edge_map[i].face_count != 2) {
        return 0.0f;
      }
      const MPoly &mpoly_1 = polys[edge_map[i].face_index_1];
      const MPoly &mpoly_2 = polys[edge_map[i].face_index_2];
      float3 normal_1, normal_2;
      BKE_mesh_calc_poly_normal(&mpoly_1, &loops[mpoly_1.loopstart], mesh->mvert, normal_1);
      BKE_mesh_calc_poly_normal(&mpoly_2, &loops[mpoly_2.loopstart], mesh->mvert, normal_2);
      return angle_normalized_v3v3(normal_1, normal_2);
    };

    VArray<float> angles = VArray<float>::ForFunc(mesh->totedge, angle_fn);
    return component.attribute_try_adapt_domain<float>(
        std::move(angles), ATTR_DOMAIN_EDGE, domain);
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
};

class SignedAngleFieldInput final : public GeometryFieldInput {
 public:
  SignedAngleFieldInput() : GeometryFieldInput(CPPType::get<float>(), "Signed Angle Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() != GEO_COMPONENT_TYPE_MESH) {
      return {};
    }

    const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
    const Mesh *mesh = mesh_component.get_for_read();
    if (mesh == nullptr) {
      return {};
    }

    Span<MPoly> polys{mesh->mpoly, mesh->totpoly};
    Span<MLoop> loops{mesh->mloop, mesh->totloop};
    Array<EdgeMapEntry> edge_map = create_edge_map(polys, loops, mesh->totedge);

    auto angle_fn = [edge_map, polys, loops, mesh](const int i) -> float {
      if (edge_map[i].face_count != 2) {
        return 0.0f;
      }
      const MPoly &mpoly_1 = polys[edge_map[i].face_index_1];
      const MPoly &mpoly_2 = polys[edge_map[i].face_index_2];

      /* Find the normals of the 2 polys. */
      float3 poly_1_normal, poly_2_normal;
      BKE_mesh_calc_poly_normal(&mpoly_1, &loops[mpoly_1.loopstart], mesh->mvert, poly_1_normal);
      BKE_mesh_calc_poly_normal(&mpoly_2, &loops[mpoly_2.loopstart], mesh->mvert, poly_2_normal);

      /* Find the centerpoint of the axis edge */
      const float3 edge_centerpoint = (float3(mesh->mvert[mesh->medge[i].v1].co) +
                                       float3(mesh->mvert[mesh->medge[i].v2].co)) *
                                      0.5f;

      /* Get the centerpoint of poly 2 and subtract the edge centerpoint to get a tangent
       * normal for poly 2. */
      float3 poly_center_2;
      BKE_mesh_calc_poly_center(&mpoly_2, &loops[mpoly_2.loopstart], mesh->mvert, poly_center_2);
      const float3 poly_2_tangent = math::normalize(poly_center_2 - edge_centerpoint);
      const float concavity = math::dot(poly_1_normal, poly_2_tangent);

      /* Get the unsigned angle between the two polys */
      const float angle = angle_normalized_v3v3(poly_1_normal, poly_2_normal);

      if (angle == 0.0f || angle == 2.0f * M_PI || concavity < 0) {
        return angle;
      }
      return -angle;
    };

    VArray<float> angles = VArray<float>::ForFunc(mesh->totedge, angle_fn);
    return component.attribute_try_adapt_domain<float>(
        std::move(angles), ATTR_DOMAIN_EDGE, domain);
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
