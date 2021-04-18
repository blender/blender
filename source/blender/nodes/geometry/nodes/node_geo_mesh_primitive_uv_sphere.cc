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

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_mesh_primitive_uv_sphere_in[] = {
    {SOCK_INT, N_("Segments"), 32, 0.0f, 0.0f, 0.0f, 3, 1024},
    {SOCK_INT, N_("Rings"), 16, 0.0f, 0.0f, 0.0f, 2, 1024},
    {SOCK_FLOAT, N_("Radius"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX, PROP_DISTANCE},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_mesh_primitive_uv_sphere_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static int sphere_vert_total(const int segments, const int rings)
{
  return segments * (rings - 1) + 2;
}

static int sphere_edge_total(const int segments, const int rings)
{
  return segments * (rings * 2 - 1);
}

static int sphere_corner_total(const int segments, const int rings)
{
  const int quad_corners = 4 * segments * (rings - 2);
  const int tri_corners = 3 * segments * 2;
  return quad_corners + tri_corners;
}

static int sphere_face_total(const int segments, const int rings)
{
  const int quads = segments * (rings - 2);
  const int triangles = segments * 2;
  return quads + triangles;
}

static void calculate_sphere_vertex_data(MutableSpan<MVert> verts,
                                         const float radius,
                                         const int segments,
                                         const int rings)
{
  const float delta_theta = M_PI / rings;
  const float delta_phi = (2 * M_PI) / segments;

  copy_v3_v3(verts[0].co, float3(0.0f, 0.0f, radius));
  normal_float_to_short_v3(verts[0].no, float3(0.0f, 0.0f, 1.0f));

  int vert_index = 1;
  float theta = delta_theta;
  for (const int UNUSED(ring) : IndexRange(rings - 1)) {
    float phi = 0.0f;
    const float z = cosf(theta);
    for (const int UNUSED(segment) : IndexRange(segments)) {
      const float sin_theta = std::sin(theta);
      const float x = sin_theta * std::cos(phi);
      const float y = sin_theta * std::sin(phi);
      copy_v3_v3(verts[vert_index].co, float3(x, y, z) * radius);
      normal_float_to_short_v3(verts[vert_index].no, float3(x, y, z));
      phi += delta_phi;
      vert_index++;
    }
    theta += delta_theta;
  }

  copy_v3_v3(verts.last().co, float3(0.0f, 0.0f, -radius));
  normal_float_to_short_v3(verts.last().no, float3(0.0f, 0.0f, -1.0f));
}

static void calculate_sphere_edge_indices(MutableSpan<MEdge> edges,
                                          const int segments,
                                          const int rings)
{
  int edge_index = 0;

  /* Add the edges connecting the top vertex to the first ring. */
  const int first_vert_ring_index_start = 1;
  for (const int segment : IndexRange(segments)) {
    MEdge &edge = edges[edge_index++];
    edge.v1 = 0;
    edge.v2 = first_vert_ring_index_start + segment;
    edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
  }

  int ring_vert_index_start = 1;
  for (const int ring : IndexRange(rings - 1)) {
    const int next_ring_vert_index_start = ring_vert_index_start + segments;

    /* Add the edges running along each ring. */
    for (const int segment : IndexRange(segments)) {
      MEdge &edge = edges[edge_index++];
      edge.v1 = ring_vert_index_start + segment;
      edge.v2 = ring_vert_index_start + ((segment + 1) % segments);
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
    }

    /* Add the edges connecting to the next ring. */
    if (ring < rings - 2) {
      for (const int segment : IndexRange(segments)) {
        MEdge &edge = edges[edge_index++];
        edge.v1 = ring_vert_index_start + segment;
        edge.v2 = next_ring_vert_index_start + segment;
        edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
      }
    }
    ring_vert_index_start += segments;
  }

  /* Add the edges connecting the last ring to the bottom vertex. */
  const int last_vert_index = sphere_vert_total(segments, rings) - 1;
  const int last_vert_ring_start = last_vert_index - segments;
  for (const int segment : IndexRange(segments)) {
    MEdge &edge = edges[edge_index++];
    edge.v1 = last_vert_index;
    edge.v2 = last_vert_ring_start + segment;
    edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
  }
}

static void calculate_sphere_faces(MutableSpan<MLoop> loops,
                                   MutableSpan<MPoly> polys,
                                   const int segments,
                                   const int rings)
{
  int loop_index = 0;
  int poly_index = 0;

  /* Add the triangles connected to the top vertex. */
  const int first_vert_ring_index_start = 1;
  for (const int segment : IndexRange(segments)) {
    MPoly &poly = polys[poly_index++];
    poly.loopstart = loop_index;
    poly.totloop = 3;
    MLoop &loop_a = loops[loop_index++];
    loop_a.v = 0;
    loop_a.e = segment;
    MLoop &loop_b = loops[loop_index++];
    loop_b.v = first_vert_ring_index_start + segment;
    loop_b.e = segments + segment;
    MLoop &loop_c = loops[loop_index++];
    loop_c.v = first_vert_ring_index_start + (segment + 1) % segments;
    loop_c.e = (segment + 1) % segments;
  }

  int ring_vert_index_start = 1;
  int ring_edge_index_start = segments;
  for (const int UNUSED(ring) : IndexRange(1, rings - 2)) {
    const int next_ring_vert_index_start = ring_vert_index_start + segments;
    const int next_ring_edge_index_start = ring_edge_index_start + segments * 2;
    const int ring_vertical_edge_index_start = ring_edge_index_start + segments;

    for (const int segment : IndexRange(segments)) {
      MPoly &poly = polys[poly_index++];
      poly.loopstart = loop_index;
      poly.totloop = 4;

      MLoop &loop_a = loops[loop_index++];
      loop_a.v = ring_vert_index_start + segment;
      loop_a.e = ring_vertical_edge_index_start + segment;
      MLoop &loop_b = loops[loop_index++];
      loop_b.v = next_ring_vert_index_start + segment;
      loop_b.e = next_ring_edge_index_start + segment;
      MLoop &loop_c = loops[loop_index++];
      loop_c.v = next_ring_vert_index_start + (segment + 1) % segments;
      loop_c.e = ring_vertical_edge_index_start + (segment + 1) % segments;
      MLoop &loop_d = loops[loop_index++];
      loop_d.v = ring_vert_index_start + (segment + 1) % segments;
      loop_d.e = ring_edge_index_start + segment;
    }
    ring_vert_index_start += segments;
    ring_edge_index_start += segments * 2;
  }

  /* Add the triangles connected to the bottom vertex. */
  const int last_edge_ring_start = segments * (rings - 2) * 2 + segments;
  const int bottom_edge_fan_start = last_edge_ring_start + segments;
  const int last_vert_index = sphere_vert_total(segments, rings) - 1;
  const int last_vert_ring_start = last_vert_index - segments;
  for (const int segment : IndexRange(segments)) {
    MPoly &poly = polys[poly_index++];
    poly.loopstart = loop_index;
    poly.totloop = 3;

    MLoop &loop_a = loops[loop_index++];
    loop_a.v = last_vert_index;
    loop_a.e = bottom_edge_fan_start + (segment + 1) % segments;
    MLoop &loop_b = loops[loop_index++];
    loop_b.v = last_vert_ring_start + (segment + 1) % segments;
    loop_b.e = last_edge_ring_start + segment;
    MLoop &loop_c = loops[loop_index++];
    loop_c.v = last_vert_ring_start + segment;
    loop_c.e = bottom_edge_fan_start + segment;
  }
}

static void calculate_sphere_uvs(Mesh *mesh, const float segments, const float rings)
{
  MeshComponent mesh_component;
  mesh_component.replace(mesh, GeometryOwnershipType::Editable);
  OutputAttribute_Typed<float2> uv_attribute =
      mesh_component.attribute_try_get_for_output_only<float2>("uv_map", ATTR_DOMAIN_CORNER);
  MutableSpan<float2> uvs = uv_attribute.as_span();

  int loop_index = 0;
  const float dy = 1.0f / rings;

  for (const int i_segment : IndexRange(segments)) {
    const float segment = static_cast<float>(i_segment);
    uvs[loop_index++] = float2((segment + 0.5f) / segments, 0.0f);
    uvs[loop_index++] = float2(segment / segments, dy);
    uvs[loop_index++] = float2((segment + 1.0f) / segments, dy);
  }

  for (const int i_ring : IndexRange(1, rings - 2)) {
    const float ring = static_cast<float>(i_ring);
    for (const int i_segment : IndexRange(segments)) {
      const float segment = static_cast<float>(i_segment);
      uvs[loop_index++] = float2(segment / segments, ring / rings);
      uvs[loop_index++] = float2(segment / segments, (ring + 1.0f) / rings);
      uvs[loop_index++] = float2((segment + 1.0f) / segments, (ring + 1.0f) / rings);
      uvs[loop_index++] = float2((segment + 1.0f) / segments, ring / rings);
    }
  }

  for (const int i_segment : IndexRange(segments)) {
    const float segment = static_cast<float>(i_segment);
    uvs[loop_index++] = float2((segment + 0.5f) / segments, 1.0f);
    uvs[loop_index++] = float2((segment + 1.0f) / segments, 1.0f - dy);
    uvs[loop_index++] = float2(segment / segments, 1.0f - dy);
  }

  uv_attribute.save();
}

static Mesh *create_uv_sphere_mesh(const float radius, const int segments, const int rings)
{
  Mesh *mesh = BKE_mesh_new_nomain(sphere_vert_total(segments, rings),
                                   sphere_edge_total(segments, rings),
                                   0,
                                   sphere_corner_total(segments, rings),
                                   sphere_face_total(segments, rings));
  MutableSpan<MVert> verts{mesh->mvert, mesh->totvert};
  MutableSpan<MLoop> loops{mesh->mloop, mesh->totloop};
  MutableSpan<MEdge> edges{mesh->medge, mesh->totedge};
  MutableSpan<MPoly> polys{mesh->mpoly, mesh->totpoly};

  calculate_sphere_vertex_data(verts, radius, segments, rings);

  calculate_sphere_edge_indices(edges, segments, rings);

  calculate_sphere_faces(loops, polys, segments, rings);

  calculate_sphere_uvs(mesh, segments, rings);

  BLI_assert(BKE_mesh_is_valid(mesh));

  return mesh;
}

static void geo_node_mesh_primitive_uv_sphere_exec(GeoNodeExecParams params)
{
  const int segments_num = params.extract_input<int>("Segments");
  const int rings_num = params.extract_input<int>("Rings");
  if (segments_num < 3 || rings_num < 2) {
    params.set_output("Geometry", GeometrySet());
    return;
  }

  const float radius = params.extract_input<float>("Radius");

  Mesh *mesh = create_uv_sphere_mesh(radius, segments_num, rings_num);
  params.set_output("Geometry", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_primitive_uv_sphere()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_MESH_PRIMITIVE_UV_SPHERE, "UV Sphere", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(
      &ntype, geo_node_mesh_primitive_uv_sphere_in, geo_node_mesh_primitive_uv_sphere_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_mesh_primitive_uv_sphere_exec;
  nodeRegisterType(&ntype);
}
