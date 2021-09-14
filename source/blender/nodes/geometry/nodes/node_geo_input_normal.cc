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

namespace blender::nodes {

static void geo_node_input_normal_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Normal");
}

static GVArrayPtr mesh_face_normals(const Mesh &mesh,
                                    const Span<MVert> verts,
                                    const Span<MPoly> polys,
                                    const Span<MLoop> loops,
                                    const IndexMask mask)
{
  /* Use existing normals to avoid unnecessarily recalculating them, if possible. */
  if (!(mesh.runtime.cd_dirty_poly & CD_MASK_NORMAL) &&
      CustomData_has_layer(&mesh.pdata, CD_NORMAL)) {
    const void *data = CustomData_get_layer(&mesh.pdata, CD_NORMAL);

    return std::make_unique<fn::GVArray_For_Span<float3>>(
        Span<float3>((const float3 *)data, polys.size()));
  }

  auto normal_fn = [verts, polys, loops](const int i) -> float3 {
    float3 normal;
    const MPoly &poly = polys[i];
    BKE_mesh_calc_poly_normal(&poly, &loops[poly.loopstart], verts.data(), normal);
    return normal;
  };

  return std::make_unique<
      fn::GVArray_For_EmbeddedVArray<float3, VArray_For_Func<float3, decltype(normal_fn)>>>(
      mask.min_array_size(), mask.min_array_size(), normal_fn);
}

static GVArrayPtr mesh_vertex_normals(const Mesh &mesh,
                                      const Span<MVert> verts,
                                      const Span<MPoly> polys,
                                      const Span<MLoop> loops,
                                      const IndexMask mask)
{
  /* Use existing normals to avoid unnecessarily recalculating them, if possible. */
  if (!(mesh.runtime.cd_dirty_vert & CD_MASK_NORMAL) &&
      CustomData_has_layer(&mesh.vdata, CD_NORMAL)) {
    const void *data = CustomData_get_layer(&mesh.pdata, CD_NORMAL);

    return std::make_unique<fn::GVArray_For_Span<float3>>(
        Span<float3>((const float3 *)data, mesh.totvert));
  }

  /* If the normals are dirty, they must be recalculated for the output of this node's field
   * source. Ideally vertex normals could be calculated lazily on a const mesh, but that's not
   * possible at the moment, so we take ownership of the results. Sadly we must also create a copy
   * of MVert to use the mesh normals API. This can be improved by adding mutex-protected lazy
   * calculation of normals on meshes.
   *
   * Use mask.min_array_size() to avoid calculating a final chunk of data if possible. */
  Array<MVert> temp_verts(verts);
  Array<float3> normals(verts.size()); /* Use full size for accumulation from faces. */
  BKE_mesh_calc_normals_poly_and_vertex(temp_verts.data(),
                                        mask.min_array_size(),
                                        loops.data(),
                                        loops.size(),
                                        polys.data(),
                                        polys.size(),
                                        nullptr,
                                        (float(*)[3])normals.data());

  return std::make_unique<fn::GVArray_For_ArrayContainer<Array<float3>>>(std::move(normals));
}

static const GVArray *construct_mesh_normals_gvarray(const MeshComponent &mesh_component,
                                                     const Mesh &mesh,
                                                     const IndexMask mask,
                                                     const AttributeDomain domain,
                                                     ResourceScope &scope)
{
  Span<MVert> verts{mesh.mvert, mesh.totvert};
  Span<MEdge> edges{mesh.medge, mesh.totedge};
  Span<MPoly> polys{mesh.mpoly, mesh.totpoly};
  Span<MLoop> loops{mesh.mloop, mesh.totloop};

  switch (domain) {
    case ATTR_DOMAIN_FACE: {
      return scope.add_value(mesh_face_normals(mesh, verts, polys, loops, mask)).get();
    }
    case ATTR_DOMAIN_POINT: {
      return scope.add_value(mesh_vertex_normals(mesh, verts, polys, loops, mask)).get();
    }
    case ATTR_DOMAIN_EDGE: {
      /* In this case, start with vertex normals and convert to the edge domain, since the
       * conversion from edges to vertices is very simple. Use the full mask since the edges
       * might use the vertex normal from any index. */
      GVArrayPtr vert_normals = mesh_vertex_normals(
          mesh, verts, polys, loops, IndexRange(verts.size()));
      Span<float3> vert_normals_span = vert_normals->get_internal_span().typed<float3>();
      Array<float3> edge_normals(mask.min_array_size());

      /* Use "manual" domain interpolation instead of the GeometryComponent API to avoid
       * calculating unnecessary values and to allow normalizing the result much more simply. */
      for (const int i : mask) {
        const MEdge &edge = edges[i];
        edge_normals[i] = float3::interpolate(
                              vert_normals_span[edge.v1], vert_normals_span[edge.v2], 0.5f)
                              .normalized();
      }

      return &scope.construct<fn::GVArray_For_ArrayContainer<Array<float3>>>(
          std::move(edge_normals));
    }
    case ATTR_DOMAIN_CORNER: {
      /* The normals on corners are just the mesh's face normals, so start with the face normal
       * array and copy the face normal for each of its corners. */
      GVArrayPtr face_normals = mesh_face_normals(
          mesh, verts, polys, loops, IndexRange(polys.size()));

      /* In this case using the mesh component's generic domain interpolation is fine, the data
       * will still be normalized, since the face normal is just copied to every corner. */
      GVArrayPtr loop_normals = mesh_component.attribute_try_adapt_domain(
          std::move(face_normals), ATTR_DOMAIN_FACE, ATTR_DOMAIN_CORNER);
      return scope.add_value(std::move(loop_normals)).get();
    }
    default:
      return nullptr;
  }
}

class NormalFieldInput final : public fn::FieldInput {
 public:
  NormalFieldInput() : fn::FieldInput(CPPType::get<float3>(), "Normal")
  {
  }

  const GVArray *get_varray_for_context(const fn::FieldContext &context,
                                        IndexMask mask,
                                        ResourceScope &scope) const final
  {
    if (const GeometryComponentFieldContext *geometry_context =
            dynamic_cast<const GeometryComponentFieldContext *>(&context)) {

      const GeometryComponent &component = geometry_context->geometry_component();
      const AttributeDomain domain = geometry_context->domain();

      if (component.type() == GEO_COMPONENT_TYPE_MESH) {
        const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
        const Mesh *mesh = mesh_component.get_for_read();
        if (mesh == nullptr) {
          return nullptr;
        }

        return construct_mesh_normals_gvarray(mesh_component, *mesh, mask, domain, scope);
      }
      if (component.type() == GEO_COMPONENT_TYPE_CURVE) {
        /* TODO: Add curve normals support. */
        return nullptr;
      }
    }
    return nullptr;
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 669605641;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const NormalFieldInput *>(&other) != nullptr;
  }
};

static void geo_node_input_normal_exec(GeoNodeExecParams params)
{
  Field<float3> normal_field{std::make_shared<NormalFieldInput>()};
  params.set_output("Normal", std::move(normal_field));
}

}  // namespace blender::nodes

void register_node_type_geo_input_normal()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_NORMAL, "Normal", NODE_CLASS_INPUT, 0);
  ntype.geometry_node_execute = blender::nodes::geo_node_input_normal_exec;
  ntype.declare = blender::nodes::geo_node_input_normal_declare;
  nodeRegisterType(&ntype);
}
