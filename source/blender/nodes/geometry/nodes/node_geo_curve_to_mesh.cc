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

#include "BLI_array.hh"
#include "BLI_float4x4.hh"
#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_spline.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_curve_to_mesh_in[] = {
    {SOCK_GEOMETRY, N_("Curve")},
    {SOCK_GEOMETRY, N_("Profile Curve")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_curve_to_mesh_out[] = {
    {SOCK_GEOMETRY, N_("Mesh")},
    {-1, ""},
};

namespace blender::nodes {

static void vert_extrude_to_mesh_data(const Spline &spline,
                                      const float3 profile_vert,
                                      MutableSpan<MVert> r_verts,
                                      MutableSpan<MEdge> r_edges,
                                      int vert_offset,
                                      int edge_offset)
{
  Span<float3> positions = spline.evaluated_positions();

  for (const int i : IndexRange(positions.size() - 1)) {
    MEdge &edge = r_edges[edge_offset++];
    edge.v1 = vert_offset + i;
    edge.v2 = vert_offset + i + 1;
    edge.flag = ME_LOOSEEDGE;
  }

  if (spline.is_cyclic() && spline.evaluated_edges_size() > 1) {
    MEdge &edge = r_edges[edge_offset++];
    edge.v1 = vert_offset;
    edge.v2 = vert_offset + positions.size() - 1;
    edge.flag = ME_LOOSEEDGE;
  }

  for (const int i : positions.index_range()) {
    MVert &vert = r_verts[vert_offset++];
    copy_v3_v3(vert.co, positions[i] + profile_vert);
  }
}

static void mark_edges_sharp(MutableSpan<MEdge> edges)
{
  for (MEdge &edge : edges) {
    edge.flag |= ME_SHARP;
  }
}

static void spline_extrude_to_mesh_data(const Spline &spline,
                                        const Spline &profile_spline,
                                        MutableSpan<MVert> r_verts,
                                        MutableSpan<MEdge> r_edges,
                                        MutableSpan<MLoop> r_loops,
                                        MutableSpan<MPoly> r_polys,
                                        int vert_offset,
                                        int edge_offset,
                                        int loop_offset,
                                        int poly_offset)
{
  const int spline_vert_len = spline.evaluated_points_size();
  const int spline_edge_len = spline.evaluated_edges_size();
  const int profile_vert_len = profile_spline.evaluated_points_size();
  const int profile_edge_len = profile_spline.evaluated_edges_size();
  if (spline_vert_len == 0) {
    return;
  }

  if (profile_vert_len == 1) {
    vert_extrude_to_mesh_data(spline,
                              profile_spline.evaluated_positions()[0],
                              r_verts,
                              r_edges,
                              vert_offset,
                              edge_offset);
    return;
  }

  /* Add the edges running along the length of the curve, starting at each profile vertex. */
  const int spline_edges_start = edge_offset;
  for (const int i_profile : IndexRange(profile_vert_len)) {
    for (const int i_ring : IndexRange(spline_edge_len)) {
      const int i_next_ring = (i_ring == spline_vert_len - 1) ? 0 : i_ring + 1;

      const int ring_vert_offset = vert_offset + profile_vert_len * i_ring;
      const int next_ring_vert_offset = vert_offset + profile_vert_len * i_next_ring;

      MEdge &edge = r_edges[edge_offset++];
      edge.v1 = ring_vert_offset + i_profile;
      edge.v2 = next_ring_vert_offset + i_profile;
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
    }
  }

  /* Add the edges running along each profile ring. */
  const int profile_edges_start = edge_offset;
  for (const int i_ring : IndexRange(spline_vert_len)) {
    const int ring_vert_offset = vert_offset + profile_vert_len * i_ring;

    for (const int i_profile : IndexRange(profile_edge_len)) {
      const int i_next_profile = (i_profile == profile_vert_len - 1) ? 0 : i_profile + 1;

      MEdge &edge = r_edges[edge_offset++];
      edge.v1 = ring_vert_offset + i_profile;
      edge.v2 = ring_vert_offset + i_next_profile;
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
    }
  }

  /* Calculate poly and corner indices. */
  for (const int i_ring : IndexRange(spline_edge_len)) {
    const int i_next_ring = (i_ring == spline_vert_len - 1) ? 0 : i_ring + 1;

    const int ring_vert_offset = vert_offset + profile_vert_len * i_ring;
    const int next_ring_vert_offset = vert_offset + profile_vert_len * i_next_ring;

    const int ring_edge_start = profile_edges_start + profile_edge_len * i_ring;
    const int next_ring_edge_offset = profile_edges_start + profile_edge_len * i_next_ring;

    for (const int i_profile : IndexRange(profile_edge_len)) {
      const int i_next_profile = (i_profile == profile_vert_len - 1) ? 0 : i_profile + 1;

      const int spline_edge_start = spline_edges_start + spline_edge_len * i_profile;
      const int next_spline_edge_start = spline_edges_start + spline_edge_len * i_next_profile;

      MPoly &poly = r_polys[poly_offset++];
      poly.loopstart = loop_offset;
      poly.totloop = 4;
      poly.flag = ME_SMOOTH;

      MLoop &loop_a = r_loops[loop_offset++];
      loop_a.v = ring_vert_offset + i_profile;
      loop_a.e = ring_edge_start + i_profile;
      MLoop &loop_b = r_loops[loop_offset++];
      loop_b.v = ring_vert_offset + i_next_profile;
      loop_b.e = next_spline_edge_start + i_ring;
      MLoop &loop_c = r_loops[loop_offset++];
      loop_c.v = next_ring_vert_offset + i_next_profile;
      loop_c.e = next_ring_edge_offset + i_profile;
      MLoop &loop_d = r_loops[loop_offset++];
      loop_d.v = next_ring_vert_offset + i_profile;
      loop_d.e = spline_edge_start + i_ring;
    }
  }

  /* Calculate the positions of each profile ring profile along the spline. */
  Span<float3> positions = spline.evaluated_positions();
  Span<float3> tangents = spline.evaluated_tangents();
  Span<float3> normals = spline.evaluated_normals();
  Span<float3> profile_positions = profile_spline.evaluated_positions();

  GVArray_Typed<float> radii = spline.interpolate_to_evaluated(spline.radii());
  for (const int i_ring : IndexRange(spline_vert_len)) {
    float4x4 point_matrix = float4x4::from_normalized_axis_data(
        positions[i_ring], normals[i_ring], tangents[i_ring]);

    point_matrix.apply_scale(radii[i_ring]);

    for (const int i_profile : IndexRange(profile_vert_len)) {
      MVert &vert = r_verts[vert_offset++];
      copy_v3_v3(vert.co, point_matrix * profile_positions[i_profile]);
    }
  }

  /* Mark edge loops from sharp vector control points sharp. */
  if (profile_spline.type() == Spline::Type::Bezier) {
    const BezierSpline &bezier_spline = static_cast<const BezierSpline &>(profile_spline);
    Span<int> control_point_offsets = bezier_spline.control_point_offsets();
    for (const int i : IndexRange(bezier_spline.size())) {
      if (bezier_spline.point_is_sharp(i)) {
        mark_edges_sharp(r_edges.slice(
            spline_edges_start + spline_edge_len * control_point_offsets[i], spline_edge_len));
      }
    }
  }
}

static inline int spline_extrude_vert_size(const Spline &curve, const Spline &profile)
{
  return curve.evaluated_points_size() * profile.evaluated_points_size();
}

static inline int spline_extrude_edge_size(const Spline &curve, const Spline &profile)
{
  /* Add the ring edges, with one ring for every curve vertex, and the edge loops
   * that run along the length of the curve, starting on the first profile. */
  return curve.evaluated_points_size() * profile.evaluated_edges_size() +
         curve.evaluated_edges_size() * profile.evaluated_points_size();
}

static inline int spline_extrude_loop_size(const Spline &curve, const Spline &profile)
{
  return curve.evaluated_edges_size() * profile.evaluated_edges_size() * 4;
}

static inline int spline_extrude_poly_size(const Spline &curve, const Spline &profile)
{
  return curve.evaluated_edges_size() * profile.evaluated_edges_size();
}

struct ResultOffsets {
  Array<int> vert;
  Array<int> edge;
  Array<int> loop;
  Array<int> poly;
};
static ResultOffsets calculate_result_offsets(Span<SplinePtr> profiles, Span<SplinePtr> curves)
{
  const int total = profiles.size() * curves.size();
  Array<int> vert(total + 1);
  Array<int> edge(total + 1);
  Array<int> loop(total + 1);
  Array<int> poly(total + 1);

  int mesh_index = 0;
  int vert_offset = 0;
  int edge_offset = 0;
  int loop_offset = 0;
  int poly_offset = 0;
  for (const int i_spline : curves.index_range()) {
    for (const int i_profile : profiles.index_range()) {
      vert[mesh_index] = vert_offset;
      edge[mesh_index] = edge_offset;
      loop[mesh_index] = loop_offset;
      poly[mesh_index] = poly_offset;
      vert_offset += spline_extrude_vert_size(*curves[i_spline], *profiles[i_profile]);
      edge_offset += spline_extrude_edge_size(*curves[i_spline], *profiles[i_profile]);
      loop_offset += spline_extrude_loop_size(*curves[i_spline], *profiles[i_profile]);
      poly_offset += spline_extrude_poly_size(*curves[i_spline], *profiles[i_profile]);
      mesh_index++;
    }
  }
  vert.last() = vert_offset;
  edge.last() = edge_offset;
  loop.last() = loop_offset;
  poly.last() = poly_offset;

  return {std::move(vert), std::move(edge), std::move(loop), std::move(poly)};
}

/**
 * \note Normal calculation is by far the slowest part of calculations relating to the result mesh.
 * Although it would be a sensible decision to use the better topology information available while
 * generating the mesh to also generate the normals, that work may wasted if the output mesh is
 * changed anyway in a way that affects the normals. So currently this code uses the safer /
 * simpler solution of not calculating normals.
 */
static Mesh *curve_to_mesh_calculate(const CurveEval &curve, const CurveEval &profile)
{
  Span<SplinePtr> profiles = profile.splines();
  Span<SplinePtr> curves = curve.splines();

  const ResultOffsets offsets = calculate_result_offsets(profiles, curves);
  if (offsets.vert.last() == 0) {
    return nullptr;
  }

  Mesh *mesh = BKE_mesh_new_nomain(
      offsets.vert.last(), offsets.edge.last(), 0, offsets.loop.last(), offsets.poly.last());
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  mesh->flag |= ME_AUTOSMOOTH;
  mesh->smoothresh = DEG2RADF(180.0f);
  mesh->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  mesh->runtime.cd_dirty_poly |= CD_MASK_NORMAL;

  threading::parallel_for(curves.index_range(), 128, [&](IndexRange curves_range) {
    for (const int i_spline : curves_range) {
      const int spline_start_index = i_spline * profiles.size();
      threading::parallel_for(profiles.index_range(), 128, [&](IndexRange profiles_range) {
        for (const int i_profile : profiles_range) {
          const int i_mesh = spline_start_index + i_profile;
          spline_extrude_to_mesh_data(*curves[i_spline],
                                      *profiles[i_profile],
                                      {mesh->mvert, mesh->totvert},
                                      {mesh->medge, mesh->totedge},
                                      {mesh->mloop, mesh->totloop},
                                      {mesh->mpoly, mesh->totpoly},
                                      offsets.vert[i_mesh],
                                      offsets.edge[i_mesh],
                                      offsets.loop[i_mesh],
                                      offsets.poly[i_mesh]);
        }
      });
    }
  });

  return mesh;
}

static CurveEval get_curve_single_vert()
{
  CurveEval curve;
  std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();
  spline->add_point(float3(0), 0, 0.0f);
  curve.add_spline(std::move(spline));

  return curve;
}

static void geo_node_curve_to_mesh_exec(GeoNodeExecParams params)
{
  GeometrySet curve_set = params.extract_input<GeometrySet>("Curve");
  GeometrySet profile_set = params.extract_input<GeometrySet>("Profile Curve");

  curve_set = bke::geometry_set_realize_instances(curve_set);
  profile_set = bke::geometry_set_realize_instances(profile_set);

  /* NOTE: Theoretically an "is empty" check would be more correct for errors. */
  if (profile_set.has_mesh() && !profile_set.has_curve()) {
    params.error_message_add(NodeWarningType::Warning,
                             TIP_("No curve data available in profile input"));
  }

  if (!curve_set.has_curve()) {
    if (curve_set.has_mesh()) {
      params.error_message_add(NodeWarningType::Warning,
                               TIP_("No curve data available in curve input"));
    }
    params.set_output("Mesh", GeometrySet());
    return;
  }

  const CurveEval *profile_curve = profile_set.get_curve_for_read();

  static const CurveEval vert_curve = get_curve_single_vert();

  Mesh *mesh = curve_to_mesh_calculate(*curve_set.get_curve_for_read(),
                                       (profile_curve == nullptr) ? vert_curve : *profile_curve);
  params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_to_mesh()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CURVE_TO_MESH, "Curve to Mesh", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_curve_to_mesh_in, geo_node_curve_to_mesh_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_to_mesh_exec;
  nodeRegisterType(&ntype);
}
