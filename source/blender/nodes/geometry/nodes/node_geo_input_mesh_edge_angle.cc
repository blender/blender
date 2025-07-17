/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_quaternion.hh"
#include "BLI_math_vector.h"
#include "BLI_ordered_edge.hh"

#include "BKE_mesh.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_edge_angle_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Unsigned Angle")
      .field_source()
      .description(
          "The shortest angle in radians between two faces where they meet at an edge. Flat edges "
          "and Non-manifold edges have an angle of zero. Computing this value is faster than the "
          "signed angle");
  b.add_output<decl::Float>("Signed Angle")
      .field_source()
      .description(
          "The signed angle in radians between two faces where they meet at an edge. Flat edges "
          "and Non-manifold edges have an angle of zero. Concave angles are positive and convex "
          "angles are negative. Computing this value is slower than the unsigned angle");
}

static Array<int2> create_edge_map(const OffsetIndices<int> faces,
                                   const Span<int> corner_edges,
                                   const int total_edges)
{
  Array<int2> edge_map(total_edges, int2(-1));

  for (const int i_face : faces.index_range()) {
    for (const int edge : corner_edges.slice(faces[i_face])) {
      int2 &entry = edge_map[edge];
      if (entry[0] == -1) {
        entry[0] = i_face;
      }
      else if (entry[1] == -1) {
        entry[1] = i_face;
      }
      else {
        entry = int2(-2);
      }
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
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    const Span<float3> positions = mesh.vert_positions();
    const OffsetIndices faces = mesh.faces();
    const Span<int> corner_verts = mesh.corner_verts();
    const Span<int> corner_edges = mesh.corner_edges();
    Array<int2> edge_map = create_edge_map(faces, corner_edges, mesh.edges_num);

    auto angle_fn =
        [edge_map = std::move(edge_map), positions, faces, corner_verts](const int i) -> float {
      if (edge_map[i][0] < 0 || edge_map[i][1] < 0) {
        return 0.0f;
      }
      const IndexRange face_1 = faces[edge_map[i][0]];
      const IndexRange face_2 = faces[edge_map[i][1]];
      const float3 normal_1 = bke::mesh::face_normal_calc(positions, corner_verts.slice(face_1));
      const float3 normal_2 = bke::mesh::face_normal_calc(positions, corner_verts.slice(face_2));
      return angle_normalized_v3v3(normal_1, normal_2);
    };

    VArray<float> angles = VArray<float>::from_func(mesh.edges_num, angle_fn);
    return mesh.attributes().adapt_domain<float>(std::move(angles), AttrDomain::Edge, domain);
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

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Edge;
  }
};

static int find_other_vert_of_edge_triangle(const OffsetIndices<int> faces,
                                            const Span<int> corner_verts,
                                            const Span<int3> corner_tris,
                                            const int face_index,
                                            const int2 edge)
{
  const OrderedEdge ordered_edge(edge);
  for (const int tri_index : bke::mesh::face_triangles_range(faces, face_index)) {
    const int3 &tri = corner_tris[tri_index];
    const int3 vert_tri(corner_verts[tri[0]], corner_verts[tri[1]], corner_verts[tri[2]]);
    if (ordered_edge == OrderedEdge(vert_tri[0], vert_tri[1])) {
      return vert_tri[2];
    }
    if (ordered_edge == OrderedEdge(vert_tri[1], vert_tri[2])) {
      return vert_tri[0];
    }
    if (ordered_edge == OrderedEdge(vert_tri[2], vert_tri[0])) {
      return vert_tri[1];
    }
  }
  BLI_assert_unreachable();
  return -1;
}

class SignedAngleFieldInput final : public bke::MeshFieldInput {
 public:
  SignedAngleFieldInput() : bke::MeshFieldInput(CPPType::get<float>(), "Signed Angle Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    const Span<float3> positions = mesh.vert_positions();
    const Span<int2> edges = mesh.edges();
    const OffsetIndices faces = mesh.faces();
    const Span<int> corner_verts = mesh.corner_verts();
    const Span<int> corner_edges = mesh.corner_edges();
    const Span<int3> corner_tris = mesh.corner_tris();
    Array<int2> edge_map = create_edge_map(faces, corner_edges, mesh.edges_num);

    auto angle_fn =
        [edge_map = std::move(edge_map), positions, edges, faces, corner_verts, corner_tris](
            const int i) -> float {
      if (edge_map[i][0] < 0 || edge_map[i][1] < 0) {
        return 0.0f;
      }
      const int face_index_1 = edge_map[i][0];
      const int face_index_2 = edge_map[i][1];
      const IndexRange face_1 = faces[face_index_1];
      const IndexRange face_2 = faces[face_index_2];

      /* Find the normals of the 2 faces. */
      const float3 face_1_normal = bke::mesh::face_normal_calc(positions,
                                                               corner_verts.slice(face_1));
      const float3 face_2_normal = bke::mesh::face_normal_calc(positions,
                                                               corner_verts.slice(face_2));

      /* Find the centerpoint of the axis edge */
      const float3 edge_centerpoint = math::midpoint(positions[edges[i][0]],
                                                     positions[edges[i][1]]);

      /* Use the third point of the triangle connected to the edge in face 2 to determine a
       * reference point for the concavity test. */
      const int tri_other_vert = find_other_vert_of_edge_triangle(
          faces, corner_verts, corner_tris, face_index_2, edges[i]);
      const float3 face_2_tangent = math::normalize(positions[tri_other_vert] - edge_centerpoint);
      const float concavity = math::dot(face_1_normal, face_2_tangent);

      /* Get the unsigned angle between the two faces */
      const float angle = angle_normalized_v3v3(face_1_normal, face_2_normal);

      if (angle == 0.0f || angle == 2.0f * M_PI || concavity < 0) {
        return angle;
      }
      return -angle;
    };

    VArray<float> angles = VArray<float>::from_func(mesh.edges_num, angle_fn);
    return mesh.attributes().adapt_domain<float>(std::move(angles), AttrDomain::Edge, domain);
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

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Edge;
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

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeInputMeshEdgeAngle", GEO_NODE_INPUT_MESH_EDGE_ANGLE);
  ntype.ui_name = "Edge Angle";
  ntype.ui_description = "The angle between the normals of connected manifold faces";
  ntype.enum_name_legacy = "MESH_EDGE_ANGLE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_mesh_edge_angle_cc
