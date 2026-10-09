/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "multires_reshape.hh"

#include "BLI_math_vector_c.hh"
#include "BLI_task.hh"

#include "BKE_subdiv.hh"
#include "BKE_subdiv_mesh.hh"
#include "BKE_subdiv_mesh_layout.hh"

namespace blender {

struct MultiresReshapeAssignVertcosContext {
  const MultiresReshapeContext *reshape_context;

  Span<float3> positions;
};

/**
 * Set single displacement grid value at a reshape level to a corresponding vertex coordinate.
 * This function will be called for every side of a boundary grid points for inner coordinates.
 */
static void assign_grid_coord_from_vert(
    const MultiresReshapeAssignVertcosContext *reshape_vertcos_context,
    const GridCoord *grid_coord,
    const int subdiv_vert_index)
{
  const float3 &coordinate = reshape_vertcos_context->positions[subdiv_vert_index];

  ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(
      reshape_vertcos_context->reshape_context, grid_coord);
  BLI_assert(grid_element.displacement != nullptr);
  *grid_element.displacement = coordinate;
}

/* TODO(sergey): De-duplicate with similar function in multires_reshape_smooth.cc */
static void assign_ptex_coord_from_vert(
    const MultiresReshapeAssignVertcosContext *reshape_vertcos_context,
    const bke::subdiv::PtexCoord &coord,
    const int subdiv_vert_index)
{
  const MultiresReshapeContext *reshape_context = reshape_vertcos_context->reshape_context;

  PTexCoord ptex_coord{};
  ptex_coord.ptex_face_index = coord.ptex_face_index;
  ptex_coord.u = coord.uv.x;
  ptex_coord.v = coord.uv.y;
  const GridCoord grid_coord = multires_reshape_ptex_coord_to_grid(reshape_context, &ptex_coord);
  const int face_index = multires_reshape_grid_to_face_index(reshape_context,
                                                             grid_coord.grid_index);

  const int num_corners = reshape_context->base_faces[face_index].size();
  const int start_grid_index = reshape_context->base_faces[face_index].start();
  const int corner = grid_coord.grid_index - start_grid_index;

  if (grid_coord.u == 0.0f && grid_coord.v == 0.0f) {
    for (int current_corner = 0; current_corner < num_corners; ++current_corner) {
      GridCoord corner_grid_coord = grid_coord;
      corner_grid_coord.grid_index = start_grid_index + current_corner;
      assign_grid_coord_from_vert(reshape_vertcos_context, &corner_grid_coord, subdiv_vert_index);
    }
    return;
  }

  assign_grid_coord_from_vert(reshape_vertcos_context, &grid_coord, subdiv_vert_index);

  if (grid_coord.u == 0.0f) {
    GridCoord prev_grid_coord;
    prev_grid_coord.grid_index = start_grid_index + ((corner + num_corners - 1) % num_corners);
    prev_grid_coord.u = grid_coord.v;
    prev_grid_coord.v = 0.0f;

    assign_grid_coord_from_vert(reshape_vertcos_context, &prev_grid_coord, subdiv_vert_index);
  }

  if (grid_coord.v == 0.0f) {
    GridCoord next_grid_coord;
    next_grid_coord.grid_index = start_grid_index + ((corner + 1) % num_corners);
    next_grid_coord.u = 0.0f;
    next_grid_coord.v = grid_coord.u;

    assign_grid_coord_from_vert(reshape_vertcos_context, &next_grid_coord, subdiv_vert_index);
  }
}

bool multires_reshape_assign_final_coords_from_vertcos(
    const MultiresReshapeContext *reshape_context, const Span<float3> positions)
{
  MultiresReshapeAssignVertcosContext reshape_vertcos_context{};
  reshape_vertcos_context.reshape_context = reshape_context;
  reshape_vertcos_context.positions = positions;

  const bke::subdiv::MeshLayout layout = bke::subdiv::build_mesh_layout(
      *reshape_context->base_mesh, (1 << reshape_context->reshape.level) + 1);
  if (layout.verts_num != positions.size()) {
    return false;
  }

  multires_reshape_foreach_boundary_vert_ptex_coord(
      reshape_context,
      layout,
      [&](const bke::subdiv::PtexCoord &coord, const int /*base_vert*/, const int subdiv_vert) {
        assign_ptex_coord_from_vert(&reshape_vertcos_context, coord, subdiv_vert);
      });

  const OffsetIndices<int> inner_verts_by_face = layout.inner_verts_by_face();
  const Span<int> face_ptex_offset = bke::subdiv::face_ptex_offset_get(reshape_context->subdiv);
  threading::parallel_for(
      reshape_context->base_faces.index_range(),
      layout.base_face_grain_size(),
      [&](const IndexRange range) {
        const IndexRange chunk_verts = inner_verts_by_face[range];
        Array<bke::subdiv::PtexCoord> ptex_coords(chunk_verts.size());
        bke::subdiv::fill_inner_vert_ptex_coords(
            layout, *reshape_context->base_mesh, face_ptex_offset, range, ptex_coords);
        for (const int i : ptex_coords.index_range()) {
          assign_ptex_coord_from_vert(
              &reshape_vertcos_context, ptex_coords[i], layout.inner_verts_start + chunk_verts[i]);
        }
      });
  return true;
}

}  // namespace blender
