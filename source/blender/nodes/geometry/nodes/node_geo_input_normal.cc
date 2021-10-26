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

#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.h"
#include "BKE_spline.hh"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_input_normal_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Normal").field_source();
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

static void calculate_bezier_normals(const BezierSpline &spline, MutableSpan<float3> normals)
{
  Span<int> offsets = spline.control_point_offsets();
  Span<float3> evaluated_normals = spline.evaluated_normals();
  for (const int i : IndexRange(spline.size())) {
    normals[i] = evaluated_normals[offsets[i]];
  }
}

static void calculate_poly_normals(const PolySpline &spline, MutableSpan<float3> normals)
{
  normals.copy_from(spline.evaluated_normals());
}

/**
 * Because NURBS control points are not necessarily on the path, the normal at the control points
 * is not well defined, so create a temporary poly spline to find the normals. This requires extra
 * copying currently, but may be more efficient in the future if attributes have some form of CoW.
 */
static void calculate_nurbs_normals(const NURBSpline &spline, MutableSpan<float3> normals)
{
  PolySpline poly_spline;
  poly_spline.resize(spline.size());
  poly_spline.positions().copy_from(spline.positions());
  normals.copy_from(poly_spline.evaluated_normals());
}

static Array<float3> curve_normal_point_domain(const CurveEval &curve)
{
  Span<SplinePtr> splines = curve.splines();
  Array<int> offsets = curve.control_point_offsets();
  const int total_size = offsets.last();
  Array<float3> normals(total_size);

  threading::parallel_for(splines.index_range(), 128, [&](IndexRange range) {
    for (const int i : range) {
      const Spline &spline = *splines[i];
      MutableSpan spline_normals{normals.as_mutable_span().slice(offsets[i], spline.size())};
      switch (splines[i]->type()) {
        case Spline::Type::Bezier:
          calculate_bezier_normals(static_cast<const BezierSpline &>(spline), spline_normals);
          break;
        case Spline::Type::Poly:
          calculate_poly_normals(static_cast<const PolySpline &>(spline), spline_normals);
          break;
        case Spline::Type::NURBS:
          calculate_nurbs_normals(static_cast<const NURBSpline &>(spline), spline_normals);
          break;
      }
    }
  });
  return normals;
}

static const GVArray *construct_curve_normal_gvarray(const CurveComponent &component,
                                                     const AttributeDomain domain,
                                                     ResourceScope &scope)
{
  const CurveEval *curve = component.get_for_read();
  if (curve == nullptr) {
    return nullptr;
  }

  if (domain == ATTR_DOMAIN_POINT) {
    const Span<SplinePtr> splines = curve->splines();

    /* Use a reference to evaluated normals if possible to avoid an allocation and a copy.
     * This is only possible when there is only one poly spline. */
    if (splines.size() == 1 && splines.first()->type() == Spline::Type::Poly) {
      const PolySpline &spline = static_cast<PolySpline &>(*splines.first());
      return &scope.construct<fn::GVArray_For_Span<float3>>(spline.evaluated_normals());
    }

    Array<float3> normals = curve_normal_point_domain(*curve);
    return &scope.construct<fn::GVArray_For_ArrayContainer<Array<float3>>>(std::move(normals));
  }

  if (domain == ATTR_DOMAIN_CURVE) {
    Array<float3> point_normals = curve_normal_point_domain(*curve);
    GVArrayPtr gvarray = std::make_unique<fn::GVArray_For_ArrayContainer<Array<float3>>>(
        std::move(point_normals));
    GVArrayPtr spline_normals = component.attribute_try_adapt_domain(
        std::move(gvarray), ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE);
    return scope.add_value(std::move(spline_normals)).get();
  }

  return nullptr;
}

class NormalFieldInput final : public fn::FieldInput {
 public:
  NormalFieldInput() : fn::FieldInput(CPPType::get<float3>(), "Normal node")
  {
    category_ = Category::Generated;
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
        const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
        return construct_curve_normal_gvarray(curve_component, domain, scope);
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
