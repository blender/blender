/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_material.h"
#include "BKE_mesh.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_primitive_uv_sphere_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Segments"))
      .default_value(32)
      .min(3)
      .max(1024)
      .description(N_("Horizontal resolution of the sphere"));
  b.add_input<decl::Int>(N_("Rings"))
      .default_value(16)
      .min(2)
      .max(1024)
      .description(N_("The number of horizontal rings"));
  b.add_input<decl::Float>(N_("Radius"))
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Distance from the generated points to the origin"));
  b.add_output<decl::Geometry>(N_("Mesh"));
}

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

/**
 * Also calculate vertex normals here, since the calculation is trivial, and it allows avoiding the
 * calculation later, if it's necessary. The vertex normals are just the normalized positions.
 */
BLI_NOINLINE static void calculate_sphere_vertex_data(MutableSpan<MVert> verts,
                                                      MutableSpan<float3> vert_normals,
                                                      const float radius,
                                                      const int segments,
                                                      const int rings)
{
  const float delta_theta = M_PI / rings;
  const float delta_phi = (2.0f * M_PI) / segments;

  Array<float, 64> segment_cosines(segments + 1);
  for (const int segment : IndexRange(1, segments)) {
    const float phi = segment * delta_phi;
    segment_cosines[segment] = std::cos(phi);
  }
  Array<float, 64> segment_sines(segments + 1);
  for (const int segment : IndexRange(1, segments)) {
    const float phi = segment * delta_phi;
    segment_sines[segment] = std::sin(phi);
  }

  copy_v3_v3(verts[0].co, float3(0.0f, 0.0f, radius));
  vert_normals.first() = float3(0.0f, 0.0f, 1.0f);

  int vert_index = 1;
  for (const int ring : IndexRange(1, rings - 1)) {
    const float theta = ring * delta_theta;
    const float sin_theta = std::sin(theta);
    const float z = std::cos(theta);
    for (const int segment : IndexRange(1, segments)) {
      const float x = sin_theta * segment_cosines[segment];
      const float y = sin_theta * segment_sines[segment];
      copy_v3_v3(verts[vert_index].co, float3(x, y, z) * radius);
      vert_normals[vert_index] = float3(x, y, z);
      vert_index++;
    }
  }

  copy_v3_v3(verts.last().co, float3(0.0f, 0.0f, -radius));
  vert_normals.last() = float3(0.0f, 0.0f, -1.0f);
}

BLI_NOINLINE static void calculate_sphere_edge_indices(MutableSpan<MEdge> edges,
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

BLI_NOINLINE static void calculate_sphere_faces(MutableSpan<MPoly> polys, const int segments)
{
  int loop_index = 0;

  /* Add the triangles connected to the top vertex. */
  for (MPoly &poly : polys.take_front(segments)) {
    poly.loopstart = loop_index;
    poly.totloop = 3;
    loop_index += 3;
  }

  /* Add the middle quads. */
  for (MPoly &poly : polys.drop_front(segments).drop_back(segments)) {
    poly.loopstart = loop_index;
    poly.totloop = 4;
    loop_index += 4;
  }

  /* Add the triangles connected to the bottom vertex. */
  for (MPoly &poly : polys.take_back(segments)) {
    poly.loopstart = loop_index;
    poly.totloop = 3;
    loop_index += 3;
  }
}

BLI_NOINLINE static void calculate_sphere_corners(MutableSpan<MLoop> loops,
                                                  const int segments,
                                                  const int rings)
{
  int loop_index = 0;
  auto segment_next_or_first = [&](const int segment) {
    return segment == segments - 1 ? 0 : segment + 1;
  };

  /* Add the triangles connected to the top vertex. */
  const int first_vert_ring_index_start = 1;
  for (const int segment : IndexRange(segments)) {
    const int segment_next = segment_next_or_first(segment);

    MLoop &loop_a = loops[loop_index++];
    loop_a.v = 0;
    loop_a.e = segment;
    MLoop &loop_b = loops[loop_index++];
    loop_b.v = first_vert_ring_index_start + segment;
    loop_b.e = segments + segment;
    MLoop &loop_c = loops[loop_index++];
    loop_c.v = first_vert_ring_index_start + segment_next;
    loop_c.e = segment_next;
  }

  int ring_vert_index_start = 1;
  int ring_edge_index_start = segments;
  for ([[maybe_unused]] const int ring : IndexRange(1, rings - 2)) {
    const int next_ring_vert_index_start = ring_vert_index_start + segments;
    const int next_ring_edge_index_start = ring_edge_index_start + segments * 2;
    const int ring_vertical_edge_index_start = ring_edge_index_start + segments;

    for (const int segment : IndexRange(segments)) {
      const int segment_next = segment_next_or_first(segment);

      MLoop &loop_a = loops[loop_index++];
      loop_a.v = ring_vert_index_start + segment;
      loop_a.e = ring_vertical_edge_index_start + segment;
      MLoop &loop_b = loops[loop_index++];
      loop_b.v = next_ring_vert_index_start + segment;
      loop_b.e = next_ring_edge_index_start + segment;
      MLoop &loop_c = loops[loop_index++];
      loop_c.v = next_ring_vert_index_start + segment_next;
      loop_c.e = ring_vertical_edge_index_start + segment_next;
      MLoop &loop_d = loops[loop_index++];
      loop_d.v = ring_vert_index_start + segment_next;
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
    const int segment_next = segment_next_or_first(segment);

    MLoop &loop_a = loops[loop_index++];
    loop_a.v = last_vert_index;
    loop_a.e = bottom_edge_fan_start + segment_next;
    MLoop &loop_b = loops[loop_index++];
    loop_b.v = last_vert_ring_start + segment_next;
    loop_b.e = last_edge_ring_start + segment;
    MLoop &loop_c = loops[loop_index++];
    loop_c.v = last_vert_ring_start + segment;
    loop_c.e = bottom_edge_fan_start + segment;
  }
}

BLI_NOINLINE static void calculate_sphere_uvs(Mesh *mesh, const float segments, const float rings)
{
  MutableAttributeAccessor attributes = bke::mesh_attributes_for_write(*mesh);

  SpanAttributeWriter<float2> uv_attribute = attributes.lookup_or_add_for_write_only_span<float2>(
      "uv_map", ATTR_DOMAIN_CORNER);
  MutableSpan<float2> uvs = uv_attribute.span;

  int loop_index = 0;
  const float dy = 1.0f / rings;

  const float segments_inv = 1.0f / segments;

  for (const int i_segment : IndexRange(segments)) {
    const float segment = static_cast<float>(i_segment);
    uvs[loop_index++] = float2((segment + 0.5f) * segments_inv, 0.0f);
    uvs[loop_index++] = float2(segment * segments_inv, dy);
    uvs[loop_index++] = float2((segment + 1.0f) * segments_inv, dy);
  }

  for (const int i_ring : IndexRange(1, rings - 2)) {
    const float ring = static_cast<float>(i_ring);
    for (const int i_segment : IndexRange(segments)) {
      const float segment = static_cast<float>(i_segment);
      uvs[loop_index++] = float2(segment * segments_inv, ring / rings);
      uvs[loop_index++] = float2(segment * segments_inv, (ring + 1.0f) / rings);
      uvs[loop_index++] = float2((segment + 1.0f) * segments_inv, (ring + 1.0f) / rings);
      uvs[loop_index++] = float2((segment + 1.0f) * segments_inv, ring / rings);
    }
  }

  for (const int i_segment : IndexRange(segments)) {
    const float segment = static_cast<float>(i_segment);
    uvs[loop_index++] = float2((segment + 0.5f) * segments_inv, 1.0f);
    uvs[loop_index++] = float2((segment + 1.0f) * segments_inv, 1.0f - dy);
    uvs[loop_index++] = float2(segment * segments_inv, 1.0f - dy);
  }

  uv_attribute.finish();
}

static Mesh *create_uv_sphere_mesh(const float radius, const int segments, const int rings)
{
  Mesh *mesh = BKE_mesh_new_nomain(sphere_vert_total(segments, rings),
                                   sphere_edge_total(segments, rings),
                                   0,
                                   sphere_corner_total(segments, rings),
                                   sphere_face_total(segments, rings));
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  MutableSpan<MVert> verts = mesh->verts_for_write();
  MutableSpan<MEdge> edges = mesh->edges_for_write();
  MutableSpan<MPoly> polys = mesh->polys_for_write();
  MutableSpan<MLoop> loops = mesh->loops_for_write();

  threading::parallel_invoke(
      1024 < segments * rings,
      [&]() {
        MutableSpan vert_normals{(float3 *)BKE_mesh_vertex_normals_for_write(mesh), mesh->totvert};
        calculate_sphere_vertex_data(verts, vert_normals, radius, segments, rings);
        BKE_mesh_vertex_normals_clear_dirty(mesh);
      },
      [&]() { calculate_sphere_edge_indices(edges, segments, rings); },
      [&]() { calculate_sphere_faces(polys, segments); },
      [&]() { calculate_sphere_corners(loops, segments, rings); },
      [&]() { calculate_sphere_uvs(mesh, segments, rings); });

  return mesh;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const int segments_num = params.extract_input<int>("Segments");
  const int rings_num = params.extract_input<int>("Rings");
  if (segments_num < 3 || rings_num < 2) {
    if (segments_num < 3) {
      params.error_message_add(NodeWarningType::Info, TIP_("Segments must be at least 3"));
    }
    if (rings_num < 3) {
      params.error_message_add(NodeWarningType::Info, TIP_("Rings must be at least 3"));
    }
    params.set_default_remaining_outputs();
    return;
  }

  const float radius = params.extract_input<float>("Radius");

  Mesh *mesh = create_uv_sphere_mesh(radius, segments_num, rings_num);
  params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes::node_geo_mesh_primitive_uv_sphere_cc

void register_node_type_geo_mesh_primitive_uv_sphere()
{
  namespace file_ns = blender::nodes::node_geo_mesh_primitive_uv_sphere_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_UV_SPHERE, "UV Sphere", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
