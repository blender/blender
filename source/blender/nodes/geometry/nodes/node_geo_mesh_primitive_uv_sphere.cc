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
  b.add_output<decl::Vector>(N_("UV Map")).field_on_all();
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
BLI_NOINLINE static void calculate_sphere_vertex_data(MutableSpan<float3> positions,
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

  positions[0] = float3(0.0f, 0.0f, radius);
  vert_normals.first() = float3(0.0f, 0.0f, 1.0f);

  int vert_index = 1;
  for (const int ring : IndexRange(1, rings - 1)) {
    const float theta = ring * delta_theta;
    const float sin_theta = std::sin(theta);
    const float z = std::cos(theta);
    for (const int segment : IndexRange(1, segments)) {
      const float x = sin_theta * segment_cosines[segment];
      const float y = sin_theta * segment_sines[segment];
      positions[vert_index] = float3(x, y, z) * radius;
      vert_normals[vert_index] = float3(x, y, z);
      vert_index++;
    }
  }

  positions.last() = float3(0.0f, 0.0f, -radius);
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
    edge.flag = ME_EDGEDRAW;
  }

  int ring_vert_index_start = 1;
  for (const int ring : IndexRange(rings - 1)) {
    const int next_ring_vert_index_start = ring_vert_index_start + segments;

    /* Add the edges running along each ring. */
    for (const int segment : IndexRange(segments)) {
      MEdge &edge = edges[edge_index++];
      edge.v1 = ring_vert_index_start + segment;
      edge.v2 = ring_vert_index_start + ((segment + 1) % segments);
      edge.flag = ME_EDGEDRAW;
    }

    /* Add the edges connecting to the next ring. */
    if (ring < rings - 2) {
      for (const int segment : IndexRange(segments)) {
        MEdge &edge = edges[edge_index++];
        edge.v1 = ring_vert_index_start + segment;
        edge.v2 = next_ring_vert_index_start + segment;
        edge.flag = ME_EDGEDRAW;
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
    edge.flag = ME_EDGEDRAW;
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
  auto segment_next_or_first = [&](const int segment) {
    return segment == segments - 1 ? 0 : segment + 1;
  };

  /* Add the triangles connected to the top vertex. */
  const int first_vert_ring_start = 1;
  for (const int segment : IndexRange(segments)) {
    const int loop_start = segment * 3;
    const int segment_next = segment_next_or_first(segment);

    loops[loop_start + 0].v = 0;
    loops[loop_start + 0].e = segment;

    loops[loop_start + 1].v = first_vert_ring_start + segment;
    loops[loop_start + 1].e = segments + segment;

    loops[loop_start + 2].v = first_vert_ring_start + segment_next;
    loops[loop_start + 2].e = segment_next;
  }

  const int rings_vert_start = 1;
  const int rings_edge_start = segments;
  const int rings_loop_start = segments * 3;
  for (const int ring : IndexRange(1, rings - 2)) {
    const int ring_vert_start = rings_vert_start + (ring - 1) * segments;
    const int ring_edge_start = rings_edge_start + (ring - 1) * segments * 2;
    const int ring_loop_start = rings_loop_start + (ring - 1) * segments * 4;

    const int next_ring_vert_start = ring_vert_start + segments;
    const int next_ring_edge_start = ring_edge_start + segments * 2;
    const int ring_vertical_edge_start = ring_edge_start + segments;

    for (const int segment : IndexRange(segments)) {
      const int loop_start = ring_loop_start + segment * 4;
      const int segment_next = segment_next_or_first(segment);

      loops[loop_start + 0].v = ring_vert_start + segment;
      loops[loop_start + 0].e = ring_vertical_edge_start + segment;

      loops[loop_start + 1].v = next_ring_vert_start + segment;
      loops[loop_start + 1].e = next_ring_edge_start + segment;

      loops[loop_start + 2].v = next_ring_vert_start + segment_next;
      loops[loop_start + 2].e = ring_vertical_edge_start + segment_next;

      loops[loop_start + 3].v = ring_vert_start + segment_next;
      loops[loop_start + 3].e = ring_edge_start + segment;
    }
  }

  /* Add the triangles connected to the bottom vertex. */
  const int bottom_loop_start = rings_loop_start + segments * (rings - 2) * 4;
  const int last_edge_ring_start = segments * (rings - 2) * 2 + segments;
  const int bottom_edge_fan_start = last_edge_ring_start + segments;
  const int last_vert_index = sphere_vert_total(segments, rings) - 1;
  const int last_vert_ring_start = last_vert_index - segments;
  for (const int segment : IndexRange(segments)) {
    const int loop_start = bottom_loop_start + segment * 3;
    const int segment_next = segment_next_or_first(segment);

    loops[loop_start + 0].v = last_vert_index;
    loops[loop_start + 0].e = bottom_edge_fan_start + segment_next;

    loops[loop_start + 1].v = last_vert_ring_start + segment_next;
    loops[loop_start + 1].e = last_edge_ring_start + segment;

    loops[loop_start + 2].v = last_vert_ring_start + segment;
    loops[loop_start + 2].e = bottom_edge_fan_start + segment;
  }
}

BLI_NOINLINE static void calculate_sphere_uvs(Mesh *mesh,
                                              const float segments,
                                              const float rings,
                                              const AttributeIDRef &uv_map_id)
{
  MutableAttributeAccessor attributes = mesh->attributes_for_write();

  SpanAttributeWriter<float2> uv_attribute = attributes.lookup_or_add_for_write_only_span<float2>(
      uv_map_id, ATTR_DOMAIN_CORNER);
  MutableSpan<float2> uvs = uv_attribute.span;

  const float dy = 1.0f / rings;

  const float segments_inv = 1.0f / segments;

  for (const int i_segment : IndexRange(segments)) {
    const int loop_start = i_segment * 3;
    const float segment = float(i_segment);
    uvs[loop_start + 0] = float2((segment + 0.5f) * segments_inv, 0.0f);
    uvs[loop_start + 1] = float2(segment * segments_inv, dy);
    uvs[loop_start + 2] = float2((segment + 1.0f) * segments_inv, dy);
  }

  const int rings_loop_start = segments * 3;
  for (const int i_ring : IndexRange(1, rings - 2)) {
    const int ring_loop_start = rings_loop_start + (i_ring - 1) * segments * 4;
    const float ring = float(i_ring);
    for (const int i_segment : IndexRange(segments)) {
      const int loop_start = ring_loop_start + i_segment * 4;
      const float segment = float(i_segment);
      uvs[loop_start + 0] = float2(segment * segments_inv, ring / rings);
      uvs[loop_start + 1] = float2(segment * segments_inv, (ring + 1.0f) / rings);
      uvs[loop_start + 2] = float2((segment + 1.0f) * segments_inv, (ring + 1.0f) / rings);
      uvs[loop_start + 3] = float2((segment + 1.0f) * segments_inv, ring / rings);
    }
  }

  const int bottom_loop_start = rings_loop_start + segments * (rings - 2) * 4;
  for (const int i_segment : IndexRange(segments)) {
    const int loop_start = bottom_loop_start + i_segment * 3;
    const float segment = float(i_segment);
    uvs[loop_start + 0] = float2((segment + 0.5f) * segments_inv, 1.0f);
    uvs[loop_start + 1] = float2((segment + 1.0f) * segments_inv, 1.0f - dy);
    uvs[loop_start + 2] = float2(segment * segments_inv, 1.0f - dy);
  }

  uv_attribute.finish();
}

static Mesh *create_uv_sphere_mesh(const float radius,
                                   const int segments,
                                   const int rings,
                                   const AttributeIDRef &uv_map_id)
{
  Mesh *mesh = BKE_mesh_new_nomain(sphere_vert_total(segments, rings),
                                   sphere_edge_total(segments, rings),
                                   0,
                                   sphere_corner_total(segments, rings),
                                   sphere_face_total(segments, rings));
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  MutableSpan<MEdge> edges = mesh->edges_for_write();
  MutableSpan<MPoly> polys = mesh->polys_for_write();
  MutableSpan<MLoop> loops = mesh->loops_for_write();

  threading::parallel_invoke(
      1024 < segments * rings,
      [&]() {
        MutableSpan vert_normals{
            reinterpret_cast<float3 *>(BKE_mesh_vertex_normals_for_write(mesh)), mesh->totvert};
        calculate_sphere_vertex_data(positions, vert_normals, radius, segments, rings);
        BKE_mesh_vertex_normals_clear_dirty(mesh);
      },
      [&]() { calculate_sphere_edge_indices(edges, segments, rings); },
      [&]() { calculate_sphere_faces(polys, segments); },
      [&]() { calculate_sphere_corners(loops, segments, rings); },
      [&]() {
        if (uv_map_id) {
          calculate_sphere_uvs(mesh, segments, rings, uv_map_id);
        }
      });

  mesh->loose_edges_tag_none();

  BLI_assert(BKE_mesh_is_valid(mesh));

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

  AutoAnonymousAttributeID uv_map_id = params.get_output_anonymous_attribute_id_if_needed(
      "UV Map");

  Mesh *mesh = create_uv_sphere_mesh(radius, segments_num, rings_num, uv_map_id.get());
  params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));
  if (uv_map_id) {
    params.set_output("UV Map",
                      AnonymousAttributeFieldInput::Create<float3>(
                          std::move(uv_map_id), params.attribute_producer_name()));
  }
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
