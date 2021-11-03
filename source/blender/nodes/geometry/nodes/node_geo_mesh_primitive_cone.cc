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

#include "BKE_material.h"
#include "BKE_mesh.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

#include <cmath>

namespace blender::nodes {

static void geo_node_mesh_primitive_cone_declare(NodeDeclarationBuilder &b)
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
  b.add_output<decl::Bool>(N_("Top")).field_source();
  b.add_output<decl::Bool>(N_("Bottom")).field_source();
  b.add_output<decl::Bool>(N_("Side")).field_source();
}

static void geo_node_mesh_primitive_cone_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryMeshCone *node_storage = (NodeGeometryMeshCone *)MEM_callocN(
      sizeof(NodeGeometryMeshCone), __func__);

  node_storage->fill_type = GEO_NODE_MESH_CIRCLE_FILL_NGON;

  node->storage = node_storage;
}

static void geo_node_mesh_primitive_cone_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *vertices_socket = (bNodeSocket *)node->inputs.first;
  bNodeSocket *rings_socket = vertices_socket->next;
  bNodeSocket *fill_subdiv_socket = rings_socket->next;

  const NodeGeometryMeshCone &storage = *(const NodeGeometryMeshCone *)node->storage;
  const GeometryNodeMeshCircleFillType fill_type =
      static_cast<const GeometryNodeMeshCircleFillType>(storage.fill_type);
  const bool has_fill = fill_type != GEO_NODE_MESH_CIRCLE_FILL_NONE;
  nodeSetSocketAvailability(fill_subdiv_socket, has_fill);
}

static void geo_node_mesh_primitive_cone_layout(uiLayout *layout,
                                                bContext *UNUSED(C),
                                                PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "fill_type", 0, nullptr, ICON_NONE);
}

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

static void calculate_cone_vertices(const MutableSpan<MVert> &verts, const ConeConfig &config)
{
  Array<float2> circle(config.circle_segments);
  const float angle_delta = 2.0f * (M_PI / static_cast<float>(config.circle_segments));
  float angle = 0.0f;
  for (const int i : IndexRange(config.circle_segments)) {
    circle[i].x = std::cos(angle);
    circle[i].y = std::sin(angle);
    angle += angle_delta;
  }

  int vert_index = 0;

  /* Top cone tip or triangle fan center. */
  if (config.top_has_center_vert) {
    copy_v3_fl3(verts[vert_index++].co, 0.0f, 0.0f, config.height);
  }

  /* Top fill including the outer edge of the fill. */
  if (!config.top_is_point) {
    const float top_fill_radius_delta = config.radius_top /
                                        static_cast<float>(config.fill_segments);
    for (const int i : IndexRange(config.fill_segments)) {
      const float top_fill_radius = top_fill_radius_delta * (i + 1);
      for (const int j : IndexRange(config.circle_segments)) {
        const float x = circle[j].x * top_fill_radius;
        const float y = circle[j].y * top_fill_radius;
        copy_v3_fl3(verts[vert_index++].co, x, y, config.height);
      }
    }
  }

  /* Rings along the side. */
  const float side_radius_delta = (config.radius_bottom - config.radius_top) /
                                  static_cast<float>(config.side_segments);
  const float height_delta = 2.0f * config.height / static_cast<float>(config.side_segments);
  for (const int i : IndexRange(config.side_segments - 1)) {
    const float ring_radius = config.radius_top + (side_radius_delta * (i + 1));
    const float ring_height = config.height - (height_delta * (i + 1));
    for (const int j : IndexRange(config.circle_segments)) {
      const float x = circle[j].x * ring_radius;
      const float y = circle[j].y * ring_radius;
      copy_v3_fl3(verts[vert_index++].co, x, y, ring_height);
    }
  }

  /* Bottom fill including the outer edge of the fill. */
  if (!config.bottom_is_point) {
    const float bottom_fill_radius_delta = config.radius_bottom /
                                           static_cast<float>(config.fill_segments);
    for (const int i : IndexRange(config.fill_segments)) {
      const float bottom_fill_radius = config.radius_bottom - (i * bottom_fill_radius_delta);
      for (const int j : IndexRange(config.circle_segments)) {
        const float x = circle[j].x * bottom_fill_radius;
        const float y = circle[j].y * bottom_fill_radius;
        copy_v3_fl3(verts[vert_index++].co, x, y, -config.height);
      }
    }
  }

  /* Bottom cone tip or triangle fan center. */
  if (config.bottom_has_center_vert) {
    copy_v3_fl3(verts[vert_index++].co, 0.0f, 0.0f, -config.height);
  }
}

static void calculate_cone_edges(const MutableSpan<MEdge> &edges, const ConeConfig &config)
{
  int edge_index = 0;

  /* Edges for top cone tip or triangle fan */
  if (config.top_has_center_vert) {
    for (const int i : IndexRange(config.circle_segments)) {
      MEdge &edge = edges[edge_index++];
      edge.v1 = config.first_vert;
      edge.v2 = config.first_ring_verts_start + i;
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
    }
  }

  /* Rings and connecting edges between the rings. */
  for (const int i : IndexRange(config.tot_edge_rings)) {
    const int this_ring_vert_start = config.first_ring_verts_start + (i * config.circle_segments);
    const int next_ring_vert_start = this_ring_vert_start + config.circle_segments;
    /* Edge rings. */
    for (const int j : IndexRange(config.circle_segments)) {
      MEdge &edge = edges[edge_index++];
      edge.v1 = this_ring_vert_start + j;
      edge.v2 = this_ring_vert_start + ((j + 1) % config.circle_segments);
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
    }
    if (i == config.tot_edge_rings - 1) {
      /* There is one fewer ring of connecting edges. */
      break;
    }
    /* Connecting edges. */
    for (const int j : IndexRange(config.circle_segments)) {
      MEdge &edge = edges[edge_index++];
      edge.v1 = this_ring_vert_start + j;
      edge.v2 = next_ring_vert_start + j;
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
    }
  }

  /* Edges for bottom triangle fan or tip. */
  if (config.bottom_has_center_vert) {
    for (const int i : IndexRange(config.circle_segments)) {
      MEdge &edge = edges[edge_index++];
      edge.v1 = config.last_ring_verts_start + i;
      edge.v2 = config.last_vert;
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
    }
  }
}

static void calculate_cone_faces(const MutableSpan<MLoop> &loops,
                                 const MutableSpan<MPoly> &polys,
                                 const ConeConfig &config)
{
  int loop_index = 0;
  int poly_index = 0;

  if (config.top_has_center_vert) {
    /* Top cone tip or center triangle fan in the fill. */
    const int top_center_vert = 0;
    const int top_fan_edges_start = 0;

    for (const int i : IndexRange(config.circle_segments)) {
      MPoly &poly = polys[poly_index++];
      poly.loopstart = loop_index;
      poly.totloop = 3;

      MLoop &loop_a = loops[loop_index++];
      loop_a.v = config.first_ring_verts_start + i;
      loop_a.e = config.first_ring_edges_start + i;
      MLoop &loop_b = loops[loop_index++];
      loop_b.v = config.first_ring_verts_start + ((i + 1) % config.circle_segments);
      loop_b.e = top_fan_edges_start + ((i + 1) % config.circle_segments);
      MLoop &loop_c = loops[loop_index++];
      loop_c.v = top_center_vert;
      loop_c.e = top_fan_edges_start + i;
    }
  }
  else if (config.fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
    /* Center n-gon in the fill. */
    MPoly &poly = polys[poly_index++];
    poly.loopstart = loop_index;
    poly.totloop = config.circle_segments;
    for (const int i : IndexRange(config.circle_segments)) {
      MLoop &loop = loops[loop_index++];
      loop.v = i;
      loop.e = i;
    }
  }

  /* Quads connect one edge ring to the next one. */
  if (config.tot_quad_rings > 0) {
    for (const int i : IndexRange(config.tot_quad_rings)) {
      const int this_ring_vert_start = config.first_ring_verts_start +
                                       (i * config.circle_segments);
      const int next_ring_vert_start = this_ring_vert_start + config.circle_segments;

      const int this_ring_edges_start = config.first_ring_edges_start +
                                        (i * 2 * config.circle_segments);
      const int next_ring_edges_start = this_ring_edges_start + (2 * config.circle_segments);
      const int ring_connections_start = this_ring_edges_start + config.circle_segments;

      for (const int j : IndexRange(config.circle_segments)) {
        MPoly &poly = polys[poly_index++];
        poly.loopstart = loop_index;
        poly.totloop = 4;

        MLoop &loop_a = loops[loop_index++];
        loop_a.v = this_ring_vert_start + j;
        loop_a.e = ring_connections_start + j;
        MLoop &loop_b = loops[loop_index++];
        loop_b.v = next_ring_vert_start + j;
        loop_b.e = next_ring_edges_start + j;
        MLoop &loop_c = loops[loop_index++];
        loop_c.v = next_ring_vert_start + ((j + 1) % config.circle_segments);
        loop_c.e = ring_connections_start + ((j + 1) % config.circle_segments);
        MLoop &loop_d = loops[loop_index++];
        loop_d.v = this_ring_vert_start + ((j + 1) % config.circle_segments);
        loop_d.e = this_ring_edges_start + j;
      }
    }
  }

  if (config.bottom_has_center_vert) {
    /* Bottom cone tip or center triangle fan in the fill. */
    for (const int i : IndexRange(config.circle_segments)) {
      MPoly &poly = polys[poly_index++];
      poly.loopstart = loop_index;
      poly.totloop = 3;

      MLoop &loop_a = loops[loop_index++];
      loop_a.v = config.last_ring_verts_start + i;
      loop_a.e = config.last_fan_edges_start + i;
      MLoop &loop_b = loops[loop_index++];
      loop_b.v = config.last_vert;
      loop_b.e = config.last_fan_edges_start + (i + 1) % config.circle_segments;
      MLoop &loop_c = loops[loop_index++];
      loop_c.v = config.last_ring_verts_start + (i + 1) % config.circle_segments;
      loop_c.e = config.last_ring_edges_start + i;
    }
  }
  else if (config.fill_type == GEO_NODE_MESH_CIRCLE_FILL_NGON) {
    /* Center n-gon in the fill. */
    MPoly &poly = polys[poly_index++];
    poly.loopstart = loop_index;
    poly.totloop = config.circle_segments;

    for (const int i : IndexRange(config.circle_segments)) {
      /* Go backwards to reverse surface normal. */
      MLoop &loop = loops[loop_index++];
      loop.v = config.last_vert - i;
      loop.e = config.last_edge - ((i + 1) % config.circle_segments);
    }
  }
}

static void calculate_selection_outputs(Mesh *mesh,
                                        const ConeConfig &config,
                                        ConeAttributeOutputs &attribute_outputs)
{
  MeshComponent mesh_component;
  mesh_component.replace(mesh, GeometryOwnershipType::Editable);

  /* Populate "Top" selection output. */
  if (attribute_outputs.top_id) {
    const bool face = !config.top_is_point && config.fill_type != GEO_NODE_MESH_CIRCLE_FILL_NONE;
    OutputAttribute_Typed<bool> attribute = mesh_component.attribute_try_get_for_output_only<bool>(
        attribute_outputs.top_id.get(), face ? ATTR_DOMAIN_FACE : ATTR_DOMAIN_POINT);
    MutableSpan<bool> selection = attribute.as_span();

    if (config.top_is_point) {
      selection[config.first_vert] = true;
    }
    else {
      selection.slice(0, face ? config.top_faces_len : config.circle_segments).fill(true);
    }
    attribute.save();
  }

  /* Populate "Bottom" selection output. */
  if (attribute_outputs.bottom_id) {
    const bool face = !config.bottom_is_point &&
                      config.fill_type != GEO_NODE_MESH_CIRCLE_FILL_NONE;
    OutputAttribute_Typed<bool> attribute = mesh_component.attribute_try_get_for_output_only<bool>(
        attribute_outputs.bottom_id.get(), face ? ATTR_DOMAIN_FACE : ATTR_DOMAIN_POINT);
    MutableSpan<bool> selection = attribute.as_span();

    if (config.bottom_is_point) {
      selection[config.last_vert] = true;
    }
    else {
      selection
          .slice(config.bottom_faces_start,
                 face ? config.bottom_faces_len : config.circle_segments)
          .fill(true);
    }
    attribute.save();
  }

  /* Populate "Side" selection output. */
  if (attribute_outputs.side_id) {
    OutputAttribute_Typed<bool> attribute = mesh_component.attribute_try_get_for_output_only<bool>(
        attribute_outputs.side_id.get(), ATTR_DOMAIN_FACE);
    MutableSpan<bool> selection = attribute.as_span();

    selection.slice(config.side_faces_start, config.side_faces_len).fill(true);
    attribute.save();
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
static void calculate_cone_uvs(Mesh *mesh, const ConeConfig &config)
{
  MeshComponent mesh_component;
  mesh_component.replace(mesh, GeometryOwnershipType::Editable);
  OutputAttribute_Typed<float2> uv_attribute =
      mesh_component.attribute_try_get_for_output_only<float2>("uv_map", ATTR_DOMAIN_CORNER);
  MutableSpan<float2> uvs = uv_attribute.as_span();

  Array<float2> circle(config.circle_segments);
  float angle = 0.0f;
  const float angle_delta = 2.0f * M_PI / static_cast<float>(config.circle_segments);
  for (const int i : IndexRange(config.circle_segments)) {
    circle[i].x = std::cos(angle) * 0.225f;
    circle[i].y = std::sin(angle) * 0.225f;
    angle += angle_delta;
  }

  int loop_index = 0;

  /* Left circle of the UV representing the top fill or top cone tip. */
  if (config.top_is_point || config.fill_type != GEO_NODE_MESH_CIRCLE_FILL_NONE) {
    const float2 center_left(0.25f, 0.25f);
    const float radius_factor_delta = 1.0f / (config.top_is_point ?
                                                  static_cast<float>(config.side_segments) :
                                                  static_cast<float>(config.fill_segments));
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
    const float x_delta = 1.0f / static_cast<float>(config.circle_segments);
    const float y_delta = (1.0f - bottom) / static_cast<float>(config.side_segments);

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
                                                  static_cast<float>(config.side_segments) :
                                                  static_cast<float>(config.fill_segments));
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

  uv_attribute.save();
}

static Mesh *create_vertex_mesh()
{
  /* Returns a mesh with a single vertex at the origin. */
  Mesh *mesh = BKE_mesh_new_nomain(1, 0, 0, 0, 0);
  copy_v3_fl3(mesh->mvert[0].co, 0.0f, 0.0f, 0.0f);
  const short up[3] = {0, 0, SHRT_MAX};
  copy_v3_v3_short(mesh->mvert[0].no, up);
  return mesh;
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
    const float z_delta = -2.0f * config.height / static_cast<float>(config.side_segments);
    const float3 start(0.0f, 0.0f, config.height);
    const float3 delta(0.0f, 0.0f, z_delta);
    return create_line_mesh(start, delta, config.tot_verts);
  }

  Mesh *mesh = BKE_mesh_new_nomain(
      config.tot_verts, config.tot_edges, 0, config.tot_corners, config.tot_faces);
  BKE_id_material_eval_ensure_default_slot(&mesh->id);

  MutableSpan<MVert> verts{mesh->mvert, mesh->totvert};
  MutableSpan<MLoop> loops{mesh->mloop, mesh->totloop};
  MutableSpan<MEdge> edges{mesh->medge, mesh->totedge};
  MutableSpan<MPoly> polys{mesh->mpoly, mesh->totpoly};

  calculate_cone_vertices(verts, config);
  calculate_cone_edges(edges, config);
  calculate_cone_faces(loops, polys, config);
  calculate_cone_uvs(mesh, config);
  calculate_selection_outputs(mesh, config, attribute_outputs);

  BKE_mesh_normals_tag_dirty(mesh);

  return mesh;
}

static void geo_node_mesh_primitive_cone_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const NodeGeometryMeshCone &storage = *(const NodeGeometryMeshCone *)node.storage;
  const GeometryNodeMeshCircleFillType fill_type = (const GeometryNodeMeshCircleFillType)
                                                       storage.fill_type;

  auto return_default = [&]() {
    params.set_output("Top", fn::make_constant_field<bool>(false));
    params.set_output("Bottom", fn::make_constant_field<bool>(false));
    params.set_output("Side", fn::make_constant_field<bool>(false));
    params.set_output("Mesh", GeometrySet());
  };

  const int circle_segments = params.extract_input<int>("Vertices");
  if (circle_segments < 3) {
    params.error_message_add(NodeWarningType::Info, TIP_("Vertices must be at least 3"));
    return return_default();
  }

  const int side_segments = params.extract_input<int>("Side Segments");
  if (side_segments < 1) {
    params.error_message_add(NodeWarningType::Info, TIP_("Side Segments must be at least 1"));
    return return_default();
  }

  const bool no_fill = fill_type == GEO_NODE_MESH_CIRCLE_FILL_NONE;
  const int fill_segments = no_fill ? 1 : params.extract_input<int>("Fill Segments");
  if (fill_segments < 1) {
    params.error_message_add(NodeWarningType::Info, TIP_("Fill Segments must be at least 1"));
    return return_default();
  }

  const float radius_top = params.extract_input<float>("Radius Top");
  const float radius_bottom = params.extract_input<float>("Radius Bottom");
  const float depth = params.extract_input<float>("Depth");

  ConeAttributeOutputs attribute_outputs;
  if (params.output_is_required("Top")) {
    attribute_outputs.top_id = StrongAnonymousAttributeID("top_selection");
  }
  if (params.output_is_required("Bottom")) {
    attribute_outputs.bottom_id = StrongAnonymousAttributeID("bottom_selection");
  }
  if (params.output_is_required("Side")) {
    attribute_outputs.side_id = StrongAnonymousAttributeID("side_selection");
  }

  Mesh *mesh = create_cylinder_or_cone_mesh(radius_top,
                                            radius_bottom,
                                            depth,
                                            circle_segments,
                                            side_segments,
                                            fill_segments,
                                            fill_type,
                                            attribute_outputs);

  /* Transform the mesh so that the base of the cone is at the origin. */
  BKE_mesh_translate(mesh, float3(0.0f, 0.0f, depth * 0.5f), false);

  if (attribute_outputs.top_id) {
    params.set_output("Top",
                      AnonymousAttributeFieldInput::Create<bool>(
                          std::move(attribute_outputs.top_id), params.attribute_producer_name()));
  }
  if (attribute_outputs.bottom_id) {
    params.set_output(
        "Bottom",
        AnonymousAttributeFieldInput::Create<bool>(std::move(attribute_outputs.bottom_id),
                                                   params.attribute_producer_name()));
  }
  if (attribute_outputs.side_id) {
    params.set_output("Side",
                      AnonymousAttributeFieldInput::Create<bool>(
                          std::move(attribute_outputs.side_id), params.attribute_producer_name()));
  }

  params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_primitive_cone()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_CONE, "Cone", NODE_CLASS_GEOMETRY, 0);
  node_type_init(&ntype, blender::nodes::geo_node_mesh_primitive_cone_init);
  node_type_update(&ntype, blender::nodes::geo_node_mesh_primitive_cone_update);
  node_type_storage(
      &ntype, "NodeGeometryMeshCone", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_mesh_primitive_cone_exec;
  ntype.draw_buttons = blender::nodes::geo_node_mesh_primitive_cone_layout;
  ntype.declare = blender::nodes::geo_node_mesh_primitive_cone_declare;
  nodeRegisterType(&ntype);
}
