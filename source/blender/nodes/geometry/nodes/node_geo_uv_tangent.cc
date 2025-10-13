/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"
#include "BKE_mesh_tangent.hh"

#include "BLI_math_vector.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_uv_tangent_cc {

enum class Method {
  Exact = 0,
  Fast = 1,
};

static EnumPropertyItem method_items[] = {
    {int(Method::Exact),
     "EXACT",
     0,
     N_("Exact"),
     N_("Calculation using the MikkTSpace library, consistent with tangents used elsewhere in "
        "Blender")},
    {int(Method::Fast),
     "FAST",
     0,
     N_("Fast"),
     N_("Significantly faster method that approximates tangents interpolated across face corners "
        "with matching UVs. For a value actually tangential to the surface, use the cross product "
        "with the normal.")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Menu>("Method").static_items(method_items).optional_label();
  b.add_input<decl::Vector>("UV").dimensions(2).subtype(PROP_XYZ).supports_field();
  b.add_output<decl::Vector>("Tangent").field_source_reference_all();
}

static float3 compute_triangle_tangent(const float3 &p1,
                                       const float3 &p2,
                                       const float3 &p3,
                                       const float2 &uv1,
                                       const float2 &uv2,
                                       const float2 &uv3)
{
  const float x1 = p2.x - p1.x;
  const float x2 = p3.x - p1.x;
  const float y1 = p2.y - p1.y;
  const float y2 = p3.y - p1.y;
  const float z1 = p2.z - p1.z;
  const float z2 = p3.z - p1.z;
  const float s1 = uv2.x - uv1.x;
  const float s2 = uv3.x - uv1.x;
  const float t1 = uv2.y - uv1.y;
  const float t2 = uv3.y - uv1.y;
  const float r = 1.0f / (s1 * t2 - s2 * t1);
  const float3 tangent((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);
  return tangent;
}

static void calc_uv_tangents_simple(const Span<float3> positions,
                                    const Span<int> corner_verts,
                                    const Span<int3> corner_tris,
                                    const GroupedSpan<int> vert_to_corners_map,
                                    const Span<float3> uvs,
                                    MutableSpan<float3> r_corner_tangents)
{
  BLI_assert(r_corner_tangents.size() == corner_verts.size());

  /* Compute a tangent vector for each triangle. */
  threading::parallel_for(corner_tris.index_range(), 256, [&](const IndexRange range) {
    for (const int tri_i : range) {
      const int3 &tri = corner_tris[tri_i];
      const float3 tangent = compute_triangle_tangent(positions[corner_verts[tri[0]]],
                                                      positions[corner_verts[tri[1]]],
                                                      positions[corner_verts[tri[2]]],
                                                      uvs[tri[0]].xy(),
                                                      uvs[tri[1]].xy(),
                                                      uvs[tri[2]].xy());
      /* Writing the result separately for every triangle simplifies the next loop. */
      r_corner_tangents[tri[0]] = tangent;
      r_corner_tangents[tri[1]] = tangent;
      r_corner_tangents[tri[2]] = tangent;
    }
  });

  /* Mix the tangent vectors in vertices where multiple corners share the same uv. */
  threading::parallel_for(positions.index_range(), 512, [&](const IndexRange range) {
    struct SharedCorners {
      float2 uv;
      Vector<int, 10> corners;
      float3 tangent_sum = float3(0.0f);
    };
    Vector<SharedCorners> shared_corners;
    for (const int vert : range) {
      const Span<int> corners = vert_to_corners_map[vert];

      shared_corners.clear();
      for (const int corner : corners) {
        const float2 uv = uvs[corner].xy();
        /* This is only the non-interpolated tangent right now. */
        const float3 &tri_tangent = r_corner_tangents[corner];
        bool found = false;
        for (SharedCorners &shared_corner : shared_corners) {
          if (math::distance_manhattan(uv, shared_corner.uv) < 0.00001f) {
            shared_corner.corners.append(corner);
            shared_corner.tangent_sum += tri_tangent;
            found = true;
            break;
          }
        }
        if (!found) {
          shared_corners.append({uv, {corner}, tri_tangent});
        }
      }
      for (const SharedCorners &shared_corner : shared_corners) {
        const float3 tangent = math::normalize(shared_corner.tangent_sum);
        for (const int corner : shared_corner.corners) {
          r_corner_tangents[corner] = tangent;
        }
      }
    }
  });
}

class TangentFieldInput final : public bke::MeshFieldInput {
 private:
  Method method_;
  Field<float3> uv_field_;

 public:
  TangentFieldInput(const Method method, Field<float3> uv)
      : bke::MeshFieldInput(CPPType::get<float3>(), "Tangent Field"),
        method_(method),
        uv_field_(std::move(uv))
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const override
  {
    const bke::AttributeAccessor attributes = mesh.attributes();

    const bke::MeshFieldContext corner_context{mesh, AttrDomain::Corner};
    FieldEvaluator evaluator{corner_context, mesh.corners_num};
    evaluator.add(uv_field_);
    evaluator.evaluate();
    const VArraySpan uvs = evaluator.get_evaluated<float3>(0);

    Array<float3> corner_tangents(mesh.corners_num);
    switch (method_) {
      case Method::Fast: {
        calc_uv_tangents_simple(mesh.vert_positions(),
                                mesh.corner_verts(),
                                mesh.corner_tris(),
                                mesh.vert_to_corner_map(),
                                uvs,
                                corner_tangents);
        break;
      }
      case Method::Exact: {
        const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face",
                                                                bke::AttrDomain::Face);
        Array<float2> uvs_float2(uvs.size());
        threading::parallel_for(corner_tangents.index_range(), 4096, [&](const IndexRange range) {
          for (const int64_t corner : range) {
            uvs_float2[corner] = uvs[corner].xy();
          }
        });
        Array<Array<float4>> mikk_tangents = bke::mesh::calc_uv_tangents(mesh.vert_positions(),
                                                                         mesh.faces(),
                                                                         mesh.corner_verts(),
                                                                         mesh.corner_tris(),
                                                                         mesh.corner_tri_faces(),
                                                                         sharp_faces,
                                                                         mesh.vert_normals(),
                                                                         mesh.face_normals(),
                                                                         mesh.corner_normals(),
                                                                         {uvs_float2});
        threading::parallel_for(corner_tangents.index_range(), 4096, [&](const IndexRange range) {
          for (const int64_t corner : range) {
            corner_tangents[corner] = mikk_tangents[0][corner].xyz();
          }
        });
        break;
      }
    }

    return attributes.adapt_domain(VArray<float3>::from_container(std::move(corner_tangents)),
                                   bke::AttrDomain::Corner,
                                   domain);
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    uv_field_.node().for_each_field_input_recursive(fn);
  }

  bool is_equal_to(const FieldNode &other) const override
  {
    if (const TangentFieldInput *other_endpoint = dynamic_cast<const TangentFieldInput *>(&other))
    {
      return method_ == other_endpoint->method_ && uv_field_ == other_endpoint->uv_field_;
    }
    return false;
  }

  uint64_t hash() const override
  {
    return get_default_hash(method_, uv_field_);
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Corner;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Method method = params.extract_input<Method>("Method");
  Field<float3> uv_field = params.extract_input<Field<float3>>("UV");
  params.set_output("Tangent",
                    Field<float3>(std::make_shared<TangentFieldInput>(method, uv_field)));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeUVTangent");
  ntype.ui_name = "UV Tangent";
  ntype.ui_description = "Generate tangent directions based on a UV map";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_uv_tangent_cc
