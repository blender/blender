/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_material.h"
#include "BKE_mesh.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

#include <cmath>

namespace blender::nodes {

struct ConeConfig {
  float radius_top;
  float radius_bottom;
  float height;
  int circle_segments;
  int side_segments;
  int fill_segments;
  GeometryNodeMeshCircleFillType fill_type;

  bool top_is_point;
  bool bottom_is_point;
  /* The cone tip and a triangle fan filling are topologically identical.
   * This simplifies the logic in some cases. */
  bool top_has_center_vert;
  bool bottom_has_center_vert;

  /* Helpful quantities. */
  int tot_quad_rings;
  int tot_edge_rings;
  int tot_verts;
  int tot_edges;
  int tot_corners;
  int tot_faces;

  /* Helpful vertex indices. */
  int first_vert;
  int first_ring_verts_start;
  int last_ring_verts_start;
  int last_vert;

  /* Helpful edge indices. */
  int first_ring_edges_start;
  int last_ring_edges_start;
  int last_fan_edges_start;
  int last_edge;

  /* Helpful face indices. */
  int top_faces_start;
  int top_faces_len;
  int side_faces_start;
  int side_faces_len;
  int bottom_faces_start;
  int bottom_faces_len;

  ConeConfig(float radius_top,
             float radius_bottom,
             float depth,
             int circle_segments,
             int side_segments,
             int fill_segments,
             GeometryNodeMeshCircleFillType fill_type)
      : radius_top(radius_top),
        radius_bottom(radius_bottom),
        height(0.5f * depth),
        circle_segments(circle_segments),
        side_segments(side_segments),
        fill_segments(fill_segments),
        fill_type(fill_type)
  {
    this->top_is_point = this->radius_top == 0.0f;
    this->bottom_is_point = this->radius_bottom == 0.0f;
    this->top_has_center_vert = this->top_is_point ||
                                this->fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN;
    this->bottom_has_center_vert = this->bottom_is_point ||
                                   this->fill_type == GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN;

    this->tot_quad_rings = this->calculate_total_quad_rings();
    this->tot_edge_rings = this->calculate_total_edge_rings();
    this->tot_verts = this->calculate_total_verts();
    this->tot_edges = this->calculate_total_edges();
    this->tot_corners = this->calculate_total_corners();

    this->first_vert = 0;
    this->first_ring_verts_start = this->top_has_center_vert ? 1 : first_vert;
    this->last_vert = this->tot_verts - 1;
    this->last_ring_verts_start = this->last_vert - this->circle_segments;

    this->first_ring_edges_start = this->top_has_center_vert ? this->circle_segments : 0;
    this->last_ring_edges_start = this->first_ring_edges_start +
                                  this->tot_quad_rings * this->circle_segments * 2;
    this->last_fan_edges_start = this->tot_edges - this->circle_segments;
    this->last_edge = this->tot_edges - 1;

    this->top_faces_start = 0;
    if (!this->top_is_point) {
      this->top_faces_len = (fill_segments - 1) * circle_segments;
      this->top_faces_len += this->top_has_center_vert ? circle_segments : 0;
      this->top_faces_len += this->fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON ? 1 : 0;
    }
    else {
      this->top_faces_len = 0;
    }

    this->side_faces_start = this->top_faces_len;
    if (this->top_is_point && this->bottom_is_point) {
      this->side_faces_len = 0;
    }
    else {
      this->side_faces_len = side_segments * circle_segments;
    }

    if (!this->bottom_is_point) {
      this->bottom_faces_len = (fill_segments - 1) * circle_segments;
      this->bottom_faces_len += this->bottom_has_center_vert ? circle_segments : 0;
      this->bottom_faces_len += this->fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON ? 1 : 0;
    }
    else {
      this->bottom_faces_len = 0;
    }
    this->bottom_faces_start = this->side_faces_start + this->side_faces_len;

    this->tot_faces = this->top_faces_len + this->side_faces_len + this->bottom_faces_len;
  }

 private:
  int calculate_total_quad_rings();
  int calculate_total_edge_rings();
  int calculate_total_verts();
  int calculate_total_edges();
  int calculate_total_corners();
};

int ConeConfig::calculate_total_quad_rings()
{
  if (top_is_point && bottom_is_point) {
    return 0;
  }

  int quad_rings = 0;

  if (!top_is_point) {
    quad_rings += fill_segments - 1;
  }

  quad_rings += (!top_is_point && !bottom_is_point) ? side_segments : (side_segments - 1);

  if (!bottom_is_point) {
    quad_rings += fill_segments - 1;
  }

  return quad_rings;
}

int ConeConfig::calculate_total_edge_rings()
{
  if (top_is_point && bottom_is_point) {
    return 0;
  }

  int edge_rings = 0;

  if (!top_is_point) {
    edge_rings += fill_segments;
  }

  edge_rings += side_segments - 1;

  if (!bottom_is_point) {
    edge_rings += fill_segments;
  }

  return edge_rings;
}

int ConeConfig::calculate_total_verts()
{
  if (top_is_point && bottom_is_point) {
    return side_segments + 1;
  }

  int vert_total = 0;

  if (top_has_center_vert) {
    vert_total++;
  }

  if (!top_is_point) {
    vert_total += circle_segments * fill_segments;
  }

  vert_total += circle_segments * (side_segments - 1);

  if (!bottom_is_point) {
    vert_total += circle_segments * fill_segments;
  }

  if (bottom_has_center_vert) {
    vert_total++;
  }

  return vert_total;
}

int ConeConfig::calculate_total_edges()
{
  if (top_is_point && bottom_is_point) {
    return side_segments;
  }

  int edge_total = 0;
  if (top_has_center_vert) {
    edge_total += circle_segments;
  }

  edge_total += circle_segments * (tot_quad_rings * 2 + 1);

  if (bottom_has_center_vert) {
    edge_total += circle_segments;
  }

  return edge_total;
}

int ConeConfig::calculate_total_corners()
{
  if (top_is_point && bottom_is_point) {
    return 0;
  }

  int corner_total = 0;

  if (top_has_center_vert) {
    corner_total += (circle_segments * 3);
  }
  else if (!top_is_point && fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
    corner_total += circle_segments;
  }

  corner_total += tot_quad_rings * (circle_segments * 4);

  if (bottom_has_center_vert) {
    corner_total += (circle_segments * 3);
  }
  else if (!bottom_is_point && fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
    corner_total += circle_segments;
  }

  return corner_total;
}

static void calculate_cone_verts(const ConeConfig &config, MutableSpan<float3> positions)
{
  Array<float2> circle(config.circle_segments);
  const float angle_delta = 2.0f * (M_PI / float(config.circle_segments));
  float angle = 0.0f;
  for (const int i : IndexRange(config.circle_segments)) {
    circle[i].x = std::cos(angle);
    circle[i].y = std::sin(angle);
    angle += angle_delta;
  }

  int vert_index = 0;

  /* Top cone tip or triangle fan center. */
  if (config.top_has_center_vert) {
    positions[vert_index++] = {0.0f, 0.0f, config.height};
  }

  /* Top fill including the outer edge of the fill. */
  if (!config.top_is_point) {
    const float top_fill_radius_delta = config.radius_top / float(config.fill_segments);
    for (const int i : IndexRange(config.fill_segments)) {
      const float top_fill_radius = top_fill_radius_delta * (i + 1);
      for (const int j : IndexRange(config.circle_segments)) {
        const float x = circle[j].x * top_fill_radius;
        const float y = circle[j].y * top_fill_radius;
        positions[vert_index++] = {x, y, config.height};
      }
    }
  }

  /* Rings along the side. */
  const float side_radius_delta = (config.radius_bottom - config.radius_top) /
                                  float(config.side_segments);
  const float height_delta = 2.0f * config.height / float(config.side_segments);
  for (const int i : IndexRange(config.side_segments - 1)) {
    const float ring_radius = config.radius_top + (side_radius_delta * (i + 1));
    const float ring_height = config.height - (height_delta * (i + 1));
    for (const int j : IndexRange(config.circle_segments)) {
      const float x = circle[j].x * ring_radius;
      const float y = circle[j].y * ring_radius;
      positions[vert_index++] = {x, y, ring_height};
    }
  }

  /* Bottom fill including the outer edge of the fill. */
  if (!config.bottom_is_point) {
    const float bottom_fill_radius_delta = config.radius_bottom / float(config.fill_segments);
    for (const int i : IndexRange(config.fill_segments)) {
      const float bottom_fill_radius = config.radius_bottom - (i * bottom_fill_radius_delta);
      for (const int j : IndexRange(config.circle_segments)) {
        const float x = circle[j].x * bottom_fill_radius;
        const float y = circle[j].y * bottom_fill_radius;
        positions[vert_index++] = {x, y, -config.height};
      }
    }
  }

  /* Bottom cone tip or triangle fan center. */
  if (config.bottom_has_center_vert) {
    positions[vert_index++] = {0.0f, 0.0f, -config.height};
  }
}

static void calculate_cone_edges(const ConeConfig &config, MutableSpan<int2> edges)
{
  int edge_index = 0;

  /* Edges for top cone tip or triangle fan */
  if (config.top_has_center_vert) {
    for (const int i : IndexRange(config.circle_segments)) {
      int2 &edge = edges[edge_index++];
      edge[0] = config.first_vert;
      edge[1] = config.first_ring_verts_start + i;
    }
  }

  /* Rings and connecting edges between the rings. */
  for (const int i : IndexRange(config.tot_edge_rings)) {
    const int this_ring_vert_start = config.first_ring_verts_start + (i * config.circle_segments);
    const int next_ring_vert_start = this_ring_vert_start + config.circle_segments;
    /* Edge rings. */
    for (const int j : IndexRange(config.circle_segments)) {
      int2 &edge = edges[edge_index++];
      edge[0] = this_ring_vert_start + j;
      edge[1] = this_ring_vert_start + ((j + 1) % config.circle_segments);
    }
    if (i == config.tot_edge_rings - 1) {
      /* There is one fewer ring of connecting edges. */
      break;
    }
    /* Connecting edges. */
    for (const int j : IndexRange(config.circle_segments)) {
      int2 &edge = edges[edge_index++];
      edge[0] = this_ring_vert_start + j;
      edge[1] = next_ring_vert_start + j;
    }
  }

  /* Edges for bottom triangle fan or tip. */
  if (config.bottom_has_center_vert) {
    for (const int i : IndexRange(config.circle_segments)) {
      int2 &edge = edges[edge_index++];
      edge[0] = config.last_ring_verts_start + i;
      edge[1] = config.last_vert;
    }
  }
}

static void calculate_cone_faces(const ConeConfig &config,
                                 MutableSpan<int> corner_verts,
                                 MutableSpan<int> corner_edges,
                                 MutableSpan<int> poly_sizes)
{
  int rings_poly_start = 0;
  int rings_loop_start = 0;
  if (config.top_has_center_vert) {
    rings_poly_start = config.circle_segments;
    rings_loop_start = config.circle_segments * 3;

    poly_sizes.take_front(config.circle_segments).fill(3);

    /* Top cone tip or center triangle fan in the fill. */
    const int top_center_vert = 0;
    const int top_fan_edges_start = 0;

    for (const int i : IndexRange(config.circle_segments)) {
      const int loop_start = i * 3;

      corner_verts[loop_start + 0] = config.first_ring_verts_start + i;
      corner_edges[loop_start + 0] = config.first_ring_edges_start + i;

      corner_verts[loop_start + 1] = config.first_ring_verts_start +
                                     ((i + 1) % config.circle_segments);
      corner_edges[loop_start + 1] = top_fan_edges_start + ((i + 1) % config.circle_segments);

      corner_verts[loop_start + 2] = top_center_vert;
      corner_edges[loop_start + 2] = top_fan_edges_start + i;
    }
  }
  else if (config.fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
    rings_poly_start = 1;
    rings_loop_start = config.circle_segments;

    /* Center n-gon in the fill. */
    poly_sizes.first() = config.circle_segments;
    for (const int i : IndexRange(config.circle_segments)) {
      corner_verts[i] = i;
      corner_edges[i] = i;
    }
  }

  /* Quads connect one edge ring to the next one. */
  const int ring_polys_num = config.tot_quad_rings * config.circle_segments;
  poly_sizes.slice(rings_poly_start, ring_polys_num).fill(4);
  for (const int i : IndexRange(config.tot_quad_rings)) {
    const int this_ring_loop_start = rings_loop_start + i * config.circle_segments * 4;
    const int this_ring_vert_start = config.first_ring_verts_start + (i * config.circle_segments);
    const int next_ring_vert_start = this_ring_vert_start + config.circle_segments;

    const int this_ring_edges_start = config.first_ring_edges_start +
                                      (i * 2 * config.circle_segments);
    const int next_ring_edges_start = this_ring_edges_start + (2 * config.circle_segments);
    const int ring_connections_start = this_ring_edges_start + config.circle_segments;

    for (const int j : IndexRange(config.circle_segments)) {
      const int loop_start = this_ring_loop_start + j * 4;

      corner_verts[loop_start + 0] = this_ring_vert_start + j;
      corner_edges[loop_start + 0] = ring_connections_start + j;

      corner_verts[loop_start + 1] = next_ring_vert_start + j;
      corner_edges[loop_start + 1] = next_ring_edges_start + j;

      corner_verts[loop_start + 2] = next_ring_vert_start + ((j + 1) % config.circle_segments);
      corner_edges[loop_start + 2] = ring_connections_start + ((j + 1) % config.circle_segments);

      corner_verts[loop_start + 3] = this_ring_vert_start + ((j + 1) % config.circle_segments);
      corner_edges[loop_start + 3] = this_ring_edges_start + j;
    }
  }

  const int bottom_poly_start = rings_poly_start + ring_polys_num;
  const int bottom_loop_start = rings_loop_start + ring_polys_num * 4;

  if (config.bottom_has_center_vert) {
    poly_sizes.slice(bottom_poly_start, config.circle_segments).fill(3);

    /* Bottom cone tip or center triangle fan in the fill. */
    for (const int i : IndexRange(config.circle_segments)) {
      const int loop_start = bottom_loop_start + i * 3;

      corner_verts[loop_start + 0] = config.last_ring_verts_start + i;
      corner_edges[loop_start + 0] = config.last_fan_edges_start + i;

      corner_verts[loop_start + 1] = config.last_vert;
      corner_edges[loop_start + 1] = config.last_fan_edges_start +
                                     (i + 1) % config.circle_segments;

      corner_verts[loop_start + 2] = config.last_ring_verts_start +
                                     (i + 1) % config.circle_segments;
      corner_edges[loop_start + 2] = config.last_ring_edges_start + i;
    }
  }
  else if (config.fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
    /* Center n-gon in the fill. */
    poly_sizes[bottom_poly_start] = config.circle_segments;
    for (const int i : IndexRange(config.circle_segments)) {
      /* Go backwards to reverse surface normal. */
      corner_verts[bottom_loop_start + i] = config.last_vert - i;
      corner_edges[bottom_loop_start + i] = config.last_edge - ((i + 1) % config.circle_segments);
    }
  }
}

static void calculate_selection_outputs(const ConeConfig &config,
                                        const ConeAttributeOutputs &attribute_outputs,
                                        MutableAttributeAccessor attributes)
{
  /* Populate "Top" selection output. */
  if (attribute_outputs.top_id) {
    const bool face = !config.top_is_point && config.fill_type != GEO_NODE_MESH_CIRCLE_FILL_NONE;
    SpanAttributeWriter<bool> selection = attributes.lookup_or_add_for_write_span<bool>(
        attribute_outputs.top_id.get(), face ? ATTR_DOMAIN_FACE : ATTR_DOMAIN_POINT);

    if (config.top_is_point) {
      selection.span[config.first_vert] = true;
    }
    else {
      selection.span.slice(0, face ? config.top_faces_len : config.circle_segments).fill(true);
    }
    selection.finish();
  }

  /* Populate "Bottom" selection output. */
  if (attribute_outputs.bottom_id) {
    const bool face = !config.bottom_is_point &&
                      config.fill_type != GEO_NODE_MESH_CIRCLE_FILL_NONE;
    SpanAttributeWriter<bool> selection = attributes.lookup_or_add_for_write_span<bool>(
        attribute_outputs.bottom_id.get(), face ? ATTR_DOMAIN_FACE : ATTR_DOMAIN_POINT);

    if (config.bottom_is_point) {
      selection.span[config.last_vert] = true;
    }
    else if (face) {
      selection.span.slice(config.bottom_faces_start, config.bottom_faces_len).fill(true);
    }
    else {
      selection.span.slice(config.last_ring_verts_start + 1, config.circle_segments).fill(true);
    }
    selection.finish();
  }

  /* Populate "Side" selection output. */
  if (attribute_outputs.side_id) {
    SpanAttributeWriter<bool> selection = attributes.lookup_or_add_for_write_span<bool>(
        attribute_outputs.side_id.get(), ATTR_DOMAIN_FACE);

    selection.span.slice(config.side_faces_start, config.side_faces_len).fill(true);
    selection.finish();
  }
}

/**
 * If the top is the cone tip or has a fill, it is unwrapped into a circle in the
 * lower left quadrant of the UV.
 * Likewise, if the bottom is the cone tip or has a fill, it is unwrapped into a circle
 * in the lower right quadrant of the UV.
 * If the mesh is a truncated cone or a cylinder, the side faces are unwrapped into
 * a rectangle that fills the top half of the UV (or the entire UV, if there are no fills).
 */
static void calculate_cone_uvs(const ConeConfig &config,
                               Mesh *mesh,
                               const AttributeIDRef &uv_map_id)
{
  MutableAttributeAccessor attributes = mesh->attributes_for_write();

  SpanAttributeWriter<float2> uv_attribute = attributes.lookup_or_add_for_write_only_span<float2>(
      uv_map_id, ATTR_DOMAIN_CORNER);
  MutableSpan<float2> uvs = uv_attribute.span;

  Array<float2> circle(config.circle_segments);
  float angle = 0.0f;
  const float angle_delta = 2.0f * M_PI / float(config.circle_segments);
  for (const int i : IndexRange(config.circle_segments)) {
    circle[i].x = std::cos(angle) * 0.225f;
    circle[i].y = std::sin(angle) * 0.225f;
    angle += angle_delta;
  }

  int loop_index = 0;

  /* Left circle of the UV representing the top fill or top cone tip. */
  if (config.top_is_point || config.fill_type != GEO_NODE_MESH_CIRCLE_FILL_NONE) {
    const float2 center_left(0.25f, 0.25f);
    const float radius_factor_delta = 1.0f / (config.top_is_point ? float(config.side_segments) :
                                                                    float(config.fill_segments));
    const int left_circle_segment_count = config.top_is_point ? config.side_segments :
                                                                config.fill_segments;

    if (config.top_has_center_vert) {
      /* Cone tip itself or triangle fan center of the fill. */
      for (const int i : IndexRange(config.circle_segments)) {
        uvs[loop_index++] = radius_factor_delta * circle[i] + center_left;
        uvs[loop_index++] = radius_factor_delta * circle[(i + 1) % config.circle_segments] +
                            center_left;
        uvs[loop_index++] = center_left;
      }
    }
    else if (!config.top_is_point && config.fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
      /* N-gon at the center of the fill. */
      for (const int i : IndexRange(config.circle_segments)) {
        uvs[loop_index++] = radius_factor_delta * circle[i] + center_left;
      }
    }
    /* The rest of the top fill is made out of quad rings. */
    for (const int i : IndexRange(1, left_circle_segment_count - 1)) {
      const float inner_radius_factor = i * radius_factor_delta;
      const float outer_radius_factor = (i + 1) * radius_factor_delta;
      for (const int j : IndexRange(config.circle_segments)) {
        uvs[loop_index++] = inner_radius_factor * circle[j] + center_left;
        uvs[loop_index++] = outer_radius_factor * circle[j] + center_left;
        uvs[loop_index++] = outer_radius_factor * circle[(j + 1) % config.circle_segments] +
                            center_left;
        uvs[loop_index++] = inner_radius_factor * circle[(j + 1) % config.circle_segments] +
                            center_left;
      }
    }
  }

  if (!config.top_is_point && !config.bottom_is_point) {
    /* Mesh is a truncated cone or cylinder. The sides are unwrapped into a rectangle. */
    const float bottom = (config.fill_type == GEO_NODE_MESH_CIRCLE_FILL_NONE) ? 0.0f : 0.5f;
    const float x_delta = 1.0f / float(config.circle_segments);
    const float y_delta = (1.0f - bottom) / float(config.side_segments);

    for (const int i : IndexRange(config.side_segments)) {
      for (const int j : IndexRange(config.circle_segments)) {
        uvs[loop_index++] = float2(j * x_delta, i * y_delta + bottom);
        uvs[loop_index++] = float2(j * x_delta, (i + 1) * y_delta + bottom);
        uvs[loop_index++] = float2((j + 1) * x_delta, (i + 1) * y_delta + bottom);
        uvs[loop_index++] = float2((j + 1) * x_delta, i * y_delta + bottom);
      }
    }
  }

  /* Right circle of the UV representing the bottom fill or bottom cone tip. */
  if (config.bottom_is_point || config.fill_type != GEO_NODE_MESH_CIRCLE_FILL_NONE) {
    const float2 center_right(0.75f, 0.25f);
    const float radius_factor_delta = 1.0f / (config.bottom_is_point ?
                                                  float(config.side_segments) :
                                                  float(config.fill_segments));
    const int right_circle_segment_count = config.bottom_is_point ? config.side_segments :
                                                                    config.fill_segments;

    /* The bottom circle has to be created outside in to match the loop order. */
    for (const int i : IndexRange(right_circle_segment_count - 1)) {
      const float outer_radius_factor = 1.0f - i * radius_factor_delta;
      const float inner_radius_factor = 1.0f - (i + 1) * radius_factor_delta;
      for (const int j : IndexRange(config.circle_segments)) {
        uvs[loop_index++] = outer_radius_factor * circle[j] + center_right;
        uvs[loop_index++] = inner_radius_factor * circle[j] + center_right;
        uvs[loop_index++] = inner_radius_factor * circle[(j + 1) % config.circle_segments] +
                            center_right;
        uvs[loop_index++] = outer_radius_factor * circle[(j + 1) % config.circle_segments] +
                            center_right;
      }
    }

    if (config.bottom_has_center_vert) {
      /* Cone tip itself or triangle fan center of the fill. */
      for (const int i : IndexRange(config.circle_segments)) {
        uvs[loop_index++] = radius_factor_delta * circle[i] + center_right;
        uvs[loop_index++] = center_right;
        uvs[loop_index++] = radius_factor_delta * circle[(i + 1) % config.circle_segments] +
                            center_right;
      }
    }
    else if (!config.bottom_is_point && config.fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
      /* N-gon at the center of the fill. */
      for (const int i : IndexRange(config.circle_segments)) {
        /* Go backwards because of reversed face normal. */
        uvs[loop_index++] = radius_factor_delta * circle[config.circle_segments - 1 - i] +
                            center_right;
      }
    }
  }

  uv_attribute.finish();
}

static Mesh *create_vertex_mesh()
{
  /* Returns a mesh with a single vertex at the origin. */
  Mesh *mesh = BKE_mesh_new_nomain(1, 0, 0, 0);
  mesh->vert_positions_for_write().first() = float3(0);
  return mesh;
}

Bounds<float3> calculate_bounds_radial_primitive(const float radius_top,
                                                 const float radius_bottom,
                                                 const int segments,
                                                 const float height)
{
  const float radius = std::max(radius_top, radius_bottom);
  const float delta_phi = (2.0f * M_PI) / float(segments);

  const float x_max = radius;
  const float x_min = std::cos(std::round(0.5f * segments) * delta_phi) * radius;
  const float y_max = std::sin(std::round(0.25f * segments) * delta_phi) * radius;
  const float y_min = -y_max;

  const float3 bounds_min(x_min, y_min, -height);
  const float3 bounds_max(x_max, y_max, height);

  return {bounds_min, bounds_max};
}

static Bounds<float3> calculate_bounds_cylinder(const ConeConfig &config)
{
  return calculate_bounds_radial_primitive(
      config.radius_top, config.radius_bottom, config.circle_segments, config.height);
}

Mesh *create_cylinder_or_cone_mesh(const float radius_top,
                                   const float radius_bottom,
                                   const float depth,
                                   const int circle_segments,
                                   const int side_segments,
                                   const int fill_segments,
                                   const GeometryNodeMeshCircleFillType fill_type,
                                   ConeAttributeOutputs &attribute_outputs)
{
  const ConeConfig config(
      radius_top, radius_bottom, depth, circle_segments, side_segments, fill_segments, fill_type);

  /* Handle the case of a line / single point before everything else to avoid
   * the need to check for it later. */
  if (config.top_is_point && config.bottom_is_point) {
    if (config.height == 0.0f) {
      return create_vertex_mesh();
    }
    const float z_delta = -2.0f * config.height / float(config.side_segments);
    const float3 start(0.0f, 0.0f, config.height);
    const float3 delta(0.0f, 0.0f, z_delta);
    return create_line_mesh(start, delta, config.tot_verts);
  }

  Mesh *mesh = BKE_mesh_new_nomain(
      config.tot_verts, config.tot_edges, config.tot_faces, config.tot_corners);
  BKE_id_material_eval_ensure_default_slot(&mesh->id);

  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  MutableSpan<int2> edges = mesh->edges_for_write();
  MutableSpan<int> poly_offsets = mesh->poly_offsets_for_write();
  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();
  MutableSpan<int> corner_edges = mesh->corner_edges_for_write();
  BKE_mesh_smooth_flag_set(mesh, false);

  calculate_cone_verts(config, positions);
  calculate_cone_edges(config, edges);
  calculate_cone_faces(config, corner_verts, corner_edges, poly_offsets.drop_back(1));
  offset_indices::accumulate_counts_to_offsets(poly_offsets);
  if (attribute_outputs.uv_map_id) {
    calculate_cone_uvs(config, mesh, attribute_outputs.uv_map_id.get());
  }
  calculate_selection_outputs(config, attribute_outputs, mesh->attributes_for_write());

  mesh->tag_loose_verts_none();
  mesh->loose_edges_tag_none();
  mesh->bounds_set_eager(calculate_bounds_cylinder(config));

  return mesh;
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_mesh_primitive_cone_cc {

NODE_STORAGE_FUNCS(NodeGeometryMeshCone)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Vertices"))
      .default_value(32)
      .min(3)
      .max(512)
      .description(N_("Number of points on the circle at the top and bottom"));
  b.add_input<decl::Int>(N_("Side Segments"))
      .default_value(1)
      .min(1)
      .max(512)
      .description(N_("The number of edges running vertically along the side of the cone"));
  b.add_input<decl::Int>(N_("Fill Segments"))
      .default_value(1)
      .min(1)
      .max(512)
      .description(N_("Number of concentric rings used to fill the round face"));
  b.add_input<decl::Float>(N_("Radius Top"))
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Radius of the top circle of the cone"));
  b.add_input<decl::Float>(N_("Radius Bottom"))
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Radius of the bottom circle of the cone"));
  b.add_input<decl::Float>(N_("Depth"))
      .default_value(2.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Height of the generated cone"));
  b.add_output<decl::Geometry>(N_("Mesh"));
  b.add_output<decl::Bool>(N_("Top")).field_on_all();
  b.add_output<decl::Bool>(N_("Bottom")).field_on_all();
  b.add_output<decl::Bool>(N_("Side")).field_on_all();
  b.add_output<decl::Vector>(N_("UV Map")).field_on_all();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryMeshCone *node_storage = MEM_cnew<NodeGeometryMeshCone>(__func__);

  node_storage->fill_type = GEO_NODE_MESH_CIRCLE_FILL_NGON;

  node->storage = node_storage;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *vertices_socket = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *rings_socket = vertices_socket->next;
  bNodeSocket *fill_subdiv_socket = rings_socket->next;

  const NodeGeometryMeshCone &storage = node_storage(*node);
  const GeometryNodeMeshCircleFillType fill = (GeometryNodeMeshCircleFillType)storage.fill_type;
  const bool has_fill = fill != GEO_NODE_MESH_CIRCLE_FILL_NONE;
  nodeSetSocketAvailability(ntree, fill_subdiv_socket, has_fill);
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "fill_type", 0, nullptr, ICON_NONE);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryMeshCone &storage = node_storage(params.node());
  const GeometryNodeMeshCircleFillType fill = (GeometryNodeMeshCircleFillType)storage.fill_type;

  const int circle_segments = params.extract_input<int>("Vertices");
  if (circle_segments < 3) {
    params.error_message_add(NodeWarningType::Info, TIP_("Vertices must be at least 3"));
    params.set_default_remaining_outputs();
    return;
  }

  const int side_segments = params.extract_input<int>("Side Segments");
  if (side_segments < 1) {
    params.error_message_add(NodeWarningType::Info, TIP_("Side Segments must be at least 1"));
    params.set_default_remaining_outputs();
    return;
  }

  const bool no_fill = fill == GEO_NODE_MESH_CIRCLE_FILL_NONE;
  const int fill_segments = no_fill ? 1 : params.extract_input<int>("Fill Segments");
  if (fill_segments < 1) {
    params.error_message_add(NodeWarningType::Info, TIP_("Fill Segments must be at least 1"));
    params.set_default_remaining_outputs();
    return;
  }

  const float radius_top = params.extract_input<float>("Radius Top");
  const float radius_bottom = params.extract_input<float>("Radius Bottom");
  const float depth = params.extract_input<float>("Depth");

  ConeAttributeOutputs attribute_outputs;
  attribute_outputs.top_id = params.get_output_anonymous_attribute_id_if_needed("Top");
  attribute_outputs.bottom_id = params.get_output_anonymous_attribute_id_if_needed("Bottom");
  attribute_outputs.side_id = params.get_output_anonymous_attribute_id_if_needed("Side");
  attribute_outputs.uv_map_id = params.get_output_anonymous_attribute_id_if_needed("UV Map");

  Mesh *mesh = create_cylinder_or_cone_mesh(radius_top,
                                            radius_bottom,
                                            depth,
                                            circle_segments,
                                            side_segments,
                                            fill_segments,
                                            fill,
                                            attribute_outputs);

  /* Transform the mesh so that the base of the cone is at the origin. */
  BKE_mesh_translate(mesh, float3(0.0f, 0.0f, depth * 0.5f), false);

  params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes::node_geo_mesh_primitive_cone_cc

void register_node_type_geo_mesh_primitive_cone()
{
  namespace file_ns = blender::nodes::node_geo_mesh_primitive_cone_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_CONE, "Cone", NODE_CLASS_GEOMETRY);
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  node_type_storage(
      &ntype, "NodeGeometryMeshCone", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
