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

#include "BLI_map.hh"
#include "BLI_math_matrix.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_mesh_primitive_plane_in[] = {
    {SOCK_FLOAT, N_("Size"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX, PROP_DISTANCE},
    {SOCK_INT, N_("Vertices X"), 10, 0.0f, 0.0f, 0.0f, 2, 1000},
    {SOCK_INT, N_("Vertices Y"), 10, 0.0f, 0.0f, 0.0f, 2, 1000},
    {SOCK_VECTOR, N_("Location"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX, PROP_TRANSLATION},
    {SOCK_VECTOR, N_("Rotation"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX, PROP_EULER},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_mesh_primitive_plane_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static void calculate_uvs(Mesh *mesh, Span<MVert> verts, Span<MLoop> loops, const float size)
{
  MeshComponent mesh_component;
  mesh_component.replace(mesh, GeometryOwnershipType::Editable);
  OutputAttributePtr uv_attribute = mesh_component.attribute_try_get_for_output(
      "uv", ATTR_DOMAIN_CORNER, CD_PROP_FLOAT2, nullptr);
  MutableSpan<float2> uvs = uv_attribute->get_span_for_write_only<float2>();

  for (const int i : loops.index_range()) {
    const float3 &co = verts[loops[i].v].co;
    uvs[i].x = (co.x + size) / (size * 2.0f);
    uvs[i].y = (co.y + size) / (size * 2.0f);
  }

  uv_attribute.apply_span_and_save();
}

static Mesh *create_plane_mesh(const int verts_x, const int verts_y, const float size)
{
  const int edges_x = verts_x - 1;
  const int edges_y = verts_y - 1;
  Mesh *mesh = BKE_mesh_new_nomain(verts_x * verts_y,
                                   edges_x * verts_y + edges_y * verts_x,
                                   0,
                                   edges_x * edges_y * 4,
                                   edges_x * edges_y);
  MutableSpan<MVert> verts{mesh->mvert, mesh->totvert};
  MutableSpan<MLoop> loops{mesh->mloop, mesh->totloop};
  MutableSpan<MEdge> edges{mesh->medge, mesh->totedge};
  MutableSpan<MPoly> polys{mesh->mpoly, mesh->totpoly};

  {
    const float dx = size * 2.0f / edges_x;
    const float dy = size * 2.0f / edges_y;
    float x = -size;
    for (const int x_index : IndexRange(verts_x)) {
      float y = -size;
      for (const int y_index : IndexRange(verts_y)) {
        const int vert_index = x_index * verts_y + y_index;
        verts[vert_index].co[0] = x;
        verts[vert_index].co[1] = y;
        verts[vert_index].co[2] = 0.0f;
        y += dy;
      }
      x += dx;
    }
  }

  /* Point all vertex normals in the up direction. */
  const short up_normal[3] = {0, 0, SHRT_MAX};
  for (MVert &vert : verts) {
    copy_v3_v3_short(vert.no, up_normal);
  }

  /* Build the horizontal edges in the X direction. */
  const int y_edges_start = 0;
  int edge_index = 0;
  for (const int x : IndexRange(verts_x)) {
    for (const int y : IndexRange(edges_y)) {
      const int vert_index = x * verts_y + y;
      MEdge &edge = edges[edge_index++];
      edge.v1 = vert_index;
      edge.v2 = vert_index + 1;
    }
  }

  /* Build the vertical edges in the Y direction. */
  const int x_edges_start = edge_index;
  for (const int y : IndexRange(verts_y)) {
    for (const int x : IndexRange(edges_x)) {
      const int vert_index = x * verts_y + y;
      MEdge &edge = edges[edge_index++];
      edge.v1 = vert_index;
      edge.v2 = vert_index + verts_y;
    }
  }

  int loop_index = 0;
  int poly_index = 0;
  for (const int x : IndexRange(edges_x)) {
    for (const int y : IndexRange(edges_y)) {
      MPoly &poly = polys[poly_index++];
      poly.loopstart = loop_index;
      poly.totloop = 4;
      const int vert_index = x * verts_y + y;

      MLoop &loop_a = loops[loop_index++];
      loop_a.v = vert_index;
      loop_a.e = x_edges_start + edges_x * y + x;
      MLoop &loop_b = loops[loop_index++];
      loop_b.v = vert_index + verts_y;
      loop_b.e = y_edges_start + edges_y * (x + 1) + y;
      MLoop &loop_c = loops[loop_index++];
      loop_c.v = vert_index + verts_y + 1;
      loop_c.e = x_edges_start + edges_x * (y + 1) + x;
      MLoop &loop_d = loops[loop_index++];
      loop_d.v = vert_index + 1;
      loop_d.e = y_edges_start + edges_y * x + y;
    }
  }

  calculate_uvs(mesh, verts, loops, size);

  return mesh;
}

static void geo_node_mesh_primitive_plane_exec(GeoNodeExecParams params)
{
  const float size = params.extract_input<float>("Size");
  const int verts_x = params.extract_input<int>("Vertices X");
  const int verts_y = params.extract_input<int>("Vertices Y");
  const float3 location = params.extract_input<float3>("Location");
  const float3 rotation = params.extract_input<float3>("Rotation");
  if (verts_x < 2 || verts_y < 2) {
    params.set_output("Geometry", GeometrySet());
    return;
  }

  Mesh *mesh = create_plane_mesh(verts_x, verts_y, size);
  BLI_assert(BKE_mesh_is_valid(mesh));

  transform_mesh(mesh, location, rotation, float3(1));

  params.set_output("Geometry", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_primitive_plane()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_PLANE, "Plane", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(
      &ntype, geo_node_mesh_primitive_plane_in, geo_node_mesh_primitive_plane_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_mesh_primitive_plane_exec;
  nodeRegisterType(&ntype);
}
