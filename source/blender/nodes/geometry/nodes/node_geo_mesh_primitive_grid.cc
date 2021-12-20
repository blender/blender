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

#include "BKE_material.h"
#include "BKE_mesh.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void calculate_uvs(
    Mesh *mesh, Span<MVert> verts, Span<MLoop> loops, const float size_x, const float size_y)
{
  MeshComponent mesh_component;
  mesh_component.replace(mesh, GeometryOwnershipType::Editable);
  OutputAttribute_Typed<float2> uv_attribute =
      mesh_component.attribute_try_get_for_output_only<float2>("uv_map", ATTR_DOMAIN_CORNER);
  MutableSpan<float2> uvs = uv_attribute.as_span();

  const float dx = (size_x == 0.0f) ? 0.0f : 1.0f / size_x;
  const float dy = (size_y == 0.0f) ? 0.0f : 1.0f / size_y;
  threading::parallel_for(loops.index_range(), 1024, [&](IndexRange range) {
    for (const int i : range) {
      const float3 &co = verts[loops[i].v].co;
      uvs[i].x = (co.x + size_x * 0.5f) * dx;
      uvs[i].y = (co.y + size_y * 0.5f) * dy;
    }
  });

  uv_attribute.save();
}

Mesh *create_grid_mesh(const int verts_x,
                       const int verts_y,
                       const float size_x,
                       const float size_y)
{
  BLI_assert(verts_x > 0 && verts_y > 0);
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
    const float dx = edges_x == 0 ? 0.0f : size_x / edges_x;
    const float dy = edges_y == 0 ? 0.0f : size_y / edges_y;
    const float x_shift = edges_x / 2.0f;
    const float y_shift = edges_y / 2.0f;
    threading::parallel_for(IndexRange(verts_x), 512, [&](IndexRange x_range) {
      for (const int x : x_range) {
        const int y_offset = x * verts_y;
        threading::parallel_for(IndexRange(verts_y), 512, [&](IndexRange y_range) {
          for (const int y : y_range) {
            const int vert_index = y_offset + y;
            verts[vert_index].co[0] = (x - x_shift) * dx;
            verts[vert_index].co[1] = (y - y_shift) * dy;
            verts[vert_index].co[2] = 0.0f;
          }
        });
      }
    });
  }

  /* Point all vertex normals in the up direction. */
  {
    const short up_normal[3] = {0, 0, SHRT_MAX};
    for (MVert &vert : verts) {
      copy_v3_v3_short(vert.no, up_normal);
    }
  }

  const int y_edges_start = 0;
  const int x_edges_start = verts_x * edges_y;
  const short edge_flag = (edges_x == 0 || edges_y == 0) ? ME_LOOSEEDGE :
                                                           ME_EDGEDRAW | ME_EDGERENDER;

  /* Build the horizontal edges in the X direction. */
  threading::parallel_for(IndexRange(verts_x), 512, [&](IndexRange x_range) {
    for (const int x : x_range) {
      const int y_vert_offset = x * verts_y;
      const int y_edge_offset = y_edges_start + x * edges_y;
      threading::parallel_for(IndexRange(edges_y), 512, [&](IndexRange y_range) {
        for (const int y : y_range) {
          const int vert_index = y_vert_offset + y;
          MEdge &edge = edges[y_edge_offset + y];
          edge.v1 = vert_index;
          edge.v2 = vert_index + 1;
          edge.flag = edge_flag;
        }
      });
    }
  });

  /* Build the vertical edges in the Y direction. */
  threading::parallel_for(IndexRange(verts_y), 512, [&](IndexRange y_range) {
    for (const int y : y_range) {
      const int x_edge_offset = x_edges_start + y * edges_x;
      threading::parallel_for(IndexRange(edges_x), 512, [&](IndexRange x_range) {
        for (const int x : x_range) {
          const int vert_index = x * verts_y + y;
          MEdge &edge = edges[x_edge_offset + x];
          edge.v1 = vert_index;
          edge.v2 = vert_index + verts_y;
          edge.flag = edge_flag;
        }
      });
    }
  });

  threading::parallel_for(IndexRange(edges_x), 512, [&](IndexRange x_range) {
    for (const int x : x_range) {
      const int y_offset = x * edges_y;
      threading::parallel_for(IndexRange(edges_y), 512, [&](IndexRange y_range) {
        for (const int y : y_range) {
          const int poly_index = y_offset + y;
          const int loop_index = poly_index * 4;
          MPoly &poly = polys[poly_index];
          poly.loopstart = loop_index;
          poly.totloop = 4;
          const int vert_index = x * verts_y + y;

          MLoop &loop_a = loops[loop_index];
          loop_a.v = vert_index;
          loop_a.e = x_edges_start + edges_x * y + x;
          MLoop &loop_b = loops[loop_index + 1];
          loop_b.v = vert_index + verts_y;
          loop_b.e = y_edges_start + edges_y * (x + 1) + y;
          MLoop &loop_c = loops[loop_index + 2];
          loop_c.v = vert_index + verts_y + 1;
          loop_c.e = x_edges_start + edges_x * (y + 1) + x;
          MLoop &loop_d = loops[loop_index + 3];
          loop_d.v = vert_index + 1;
          loop_d.e = y_edges_start + edges_y * x + y;
        }
      });
    }
  });

  if (mesh->totpoly != 0) {
    calculate_uvs(mesh, verts, loops, size_x, size_y);
  }

  return mesh;
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_mesh_primitive_grid_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Size X"))
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Side length of the plane in the X direction"));
  b.add_input<decl::Float>(N_("Size Y"))
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Side length of the plane in the Y direction"));
  b.add_input<decl::Int>(N_("Vertices X"))
      .default_value(3)
      .min(2)
      .max(1000)
      .description(N_("Number of vertices in the X direction"));
  b.add_input<decl::Int>(N_("Vertices Y"))
      .default_value(3)
      .min(2)
      .max(1000)
      .description(N_("Number of vertices in the Y direction"));
  b.add_output<decl::Geometry>(N_("Mesh"));
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const float size_x = params.extract_input<float>("Size X");
  const float size_y = params.extract_input<float>("Size Y");
  const int verts_x = params.extract_input<int>("Vertices X");
  const int verts_y = params.extract_input<int>("Vertices Y");
  if (verts_x < 1 || verts_y < 1) {
    params.set_default_remaining_outputs();
    return;
  }

  Mesh *mesh = create_grid_mesh(verts_x, verts_y, size_x, size_y);
  BKE_id_material_eval_ensure_default_slot(&mesh->id);

  params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes::node_geo_mesh_primitive_grid_cc

void register_node_type_geo_mesh_primitive_grid()
{
  namespace file_ns = blender::nodes::node_geo_mesh_primitive_grid_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_GRID, "Grid", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
