/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_material.h"
#include "BKE_mesh.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void calculate_uvs(Mesh *mesh,
                          Span<float3> positions,
                          Span<MLoop> loops,
                          const float size_x,
                          const float size_y,
                          const AttributeIDRef &uv_map_id)
{
  MutableAttributeAccessor attributes = mesh->attributes_for_write();

  SpanAttributeWriter<float2> uv_attribute = attributes.lookup_or_add_for_write_only_span<float2>(
      uv_map_id, ATTR_DOMAIN_CORNER);

  const float dx = (size_x == 0.0f) ? 0.0f : 1.0f / size_x;
  const float dy = (size_y == 0.0f) ? 0.0f : 1.0f / size_y;
  threading::parallel_for(loops.index_range(), 1024, [&](IndexRange range) {
    for (const int i : range) {
      const float3 &co = positions[loops[i].v];
      uv_attribute.span[i].x = (co.x + size_x * 0.5f) * dx;
      uv_attribute.span[i].y = (co.y + size_y * 0.5f) * dy;
    }
  });

  uv_attribute.finish();
}

Mesh *create_grid_mesh(const int verts_x,
                       const int verts_y,
                       const float size_x,
                       const float size_y,
                       const AttributeIDRef &uv_map_id)
{
  BLI_assert(verts_x > 0 && verts_y > 0);
  const int edges_x = verts_x - 1;
  const int edges_y = verts_y - 1;
  Mesh *mesh = BKE_mesh_new_nomain(verts_x * verts_y,
                                   edges_x * verts_y + edges_y * verts_x,
                                   0,
                                   edges_x * edges_y * 4,
                                   edges_x * edges_y);
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  MutableSpan<MEdge> edges = mesh->edges_for_write();
  MutableSpan<MPoly> polys = mesh->polys_for_write();
  MutableSpan<MLoop> loops = mesh->loops_for_write();

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
            positions[vert_index].x = (x - x_shift) * dx;
            positions[vert_index].y = (y - y_shift) * dy;
            positions[vert_index].z = 0.0f;
          }
        });
      }
    });
  }

  const int y_edges_start = 0;
  const int x_edges_start = verts_x * edges_y;

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

  if (uv_map_id && mesh->totpoly != 0) {
    calculate_uvs(mesh, positions, loops, size_x, size_y, uv_map_id);
  }

  mesh->loose_edges_tag_none();

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
  b.add_output<decl::Vector>(N_("UV Map")).field_on_all();
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

  AutoAnonymousAttributeID uv_map_id = params.get_output_anonymous_attribute_id_if_needed(
      "UV Map");

  Mesh *mesh = create_grid_mesh(verts_x, verts_y, size_x, size_y, uv_map_id.get());
  BKE_id_material_eval_ensure_default_slot(&mesh->id);

  params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));

  if (uv_map_id) {
    params.set_output("UV Map",
                      AnonymousAttributeFieldInput::Create<float3>(
                          std::move(uv_map_id), params.attribute_producer_name()));
  }
}

}  // namespace blender::nodes::node_geo_mesh_primitive_grid_cc

void register_node_type_geo_mesh_primitive_grid()
{
  namespace file_ns = blender::nodes::node_geo_mesh_primitive_grid_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_GRID, "Grid", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
