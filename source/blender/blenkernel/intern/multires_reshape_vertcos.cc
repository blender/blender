/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup bke
 */

#include "multires_reshape.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math_vector.h"

#include "BKE_subdiv_foreach.hh"
#include "BKE_subdiv_mesh.hh"

struct MultiresReshapeAssignVertcosContext {
  const MultiresReshapeContext *reshape_context;

  const float (*vert_coords)[3];
  int num_vert_coords;
};

/**
 * Set single displacement grid value at a reshape level to a corresponding vertex coordinate.
 * This function will be called for every side of a boundary grid points for inner coordinates.
 */
static void multires_reshape_vertcos_foreach_single_vertex(
    const SubdivForeachContext *foreach_context,
    const GridCoord *grid_coord,
    const int subdiv_vertex_index)
{
  MultiresReshapeAssignVertcosContext *reshape_vertcos_context =
      static_cast<MultiresReshapeAssignVertcosContext *>(foreach_context->user_data);
  const float *coordinate = reshape_vertcos_context->vert_coords[subdiv_vertex_index];

  ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(
      reshape_vertcos_context->reshape_context, grid_coord);
  BLI_assert(grid_element.displacement != nullptr);
  copy_v3_v3(grid_element.displacement, coordinate);
}

/* TODO(sergey): De-duplicate with similar function in multires_reshape_smooth.cc */
static void multires_reshape_vertcos_foreach_vertex(const SubdivForeachContext *foreach_context,
                                                    const PTexCoord *ptex_coord,
                                                    const int subdiv_vertex_index)
{
  const MultiresReshapeAssignVertcosContext *reshape_vertcos_context =
      static_cast<MultiresReshapeAssignVertcosContext *>(foreach_context->user_data);
  const MultiresReshapeContext *reshape_context = reshape_vertcos_context->reshape_context;

  const GridCoord grid_coord = multires_reshape_ptex_coord_to_grid(reshape_context, ptex_coord);
  const int face_index = multires_reshape_grid_to_face_index(reshape_context,
                                                             grid_coord.grid_index);

  const int num_corners = reshape_context->base_polys[face_index].size();
  const int start_grid_index = reshape_context->face_start_grid_index[face_index];
  const int corner = grid_coord.grid_index - start_grid_index;

  if (grid_coord.u == 0.0f && grid_coord.v == 0.0f) {
    for (int current_corner = 0; current_corner < num_corners; ++current_corner) {
      GridCoord corner_grid_coord = grid_coord;
      corner_grid_coord.grid_index = start_grid_index + current_corner;
      multires_reshape_vertcos_foreach_single_vertex(
          foreach_context, &corner_grid_coord, subdiv_vertex_index);
    }
    return;
  }

  multires_reshape_vertcos_foreach_single_vertex(
      foreach_context, &grid_coord, subdiv_vertex_index);

  if (grid_coord.u == 0.0f) {
    GridCoord prev_grid_coord;
    prev_grid_coord.grid_index = start_grid_index + ((corner + num_corners - 1) % num_corners);
    prev_grid_coord.u = grid_coord.v;
    prev_grid_coord.v = 0.0f;

    multires_reshape_vertcos_foreach_single_vertex(
        foreach_context, &prev_grid_coord, subdiv_vertex_index);
  }

  if (grid_coord.v == 0.0f) {
    GridCoord next_grid_coord;
    next_grid_coord.grid_index = start_grid_index + ((corner + 1) % num_corners);
    next_grid_coord.u = 0.0f;
    next_grid_coord.v = grid_coord.u;

    multires_reshape_vertcos_foreach_single_vertex(
        foreach_context, &next_grid_coord, subdiv_vertex_index);
  }
}

/* SubdivForeachContext::topology_info() */
static bool multires_reshape_vertcos_foreach_topology_info(
    const SubdivForeachContext *foreach_context,
    const int num_vertices,
    const int /*num_edges*/,
    const int /*num_loops*/,
    const int /*num_polygons*/,
    const int * /*subdiv_polygon_offset*/)
{
  MultiresReshapeAssignVertcosContext *reshape_vertcos_context =
      static_cast<MultiresReshapeAssignVertcosContext *>(foreach_context->user_data);
  if (num_vertices != reshape_vertcos_context->num_vert_coords) {
    return false;
  }
  return true;
}

/* SubdivForeachContext::vertex_inner() */
static void multires_reshape_vertcos_foreach_vertex_inner(
    const SubdivForeachContext *foreach_context,
    void * /*tls_v*/,
    const int ptex_face_index,
    const float ptex_face_u,
    const float ptex_face_v,
    const int /*coarse_face_index*/,
    const int /*coarse_face_corner*/,
    const int subdiv_vertex_index)
{
  PTexCoord ptex_coord{};
  ptex_coord.ptex_face_index = ptex_face_index;
  ptex_coord.u = ptex_face_u;
  ptex_coord.v = ptex_face_v;
  multires_reshape_vertcos_foreach_vertex(foreach_context, &ptex_coord, subdiv_vertex_index);
}

/* SubdivForeachContext::vertex_every_corner() */
static void multires_reshape_vertcos_foreach_vertex_every_corner(
    const SubdivForeachContext *foreach_context,
    void * /*tls_v*/,
    const int ptex_face_index,
    const float ptex_face_u,
    const float ptex_face_v,
    const int /*coarse_vertex_index*/,
    const int /*coarse_face_index*/,
    const int /*coarse_face_corner*/,
    const int subdiv_vertex_index)
{
  PTexCoord ptex_coord{};
  ptex_coord.ptex_face_index = ptex_face_index;
  ptex_coord.u = ptex_face_u;
  ptex_coord.v = ptex_face_v;
  multires_reshape_vertcos_foreach_vertex(foreach_context, &ptex_coord, subdiv_vertex_index);
}

/* SubdivForeachContext::vertex_every_edge() */
static void multires_reshape_vertcos_foreach_vertex_every_edge(
    const SubdivForeachContext *foreach_context,
    void * /*tls_v*/,
    const int ptex_face_index,
    const float ptex_face_u,
    const float ptex_face_v,
    const int /*coarse_edge_index*/,
    const int /*coarse_face_index*/,
    const int /*coarse_face_corner*/,
    const int subdiv_vertex_index)
{
  PTexCoord ptex_coord{};
  ptex_coord.ptex_face_index = ptex_face_index;
  ptex_coord.u = ptex_face_u;
  ptex_coord.v = ptex_face_v;
  multires_reshape_vertcos_foreach_vertex(foreach_context, &ptex_coord, subdiv_vertex_index);
}

bool multires_reshape_assign_final_coords_from_vertcos(
    const MultiresReshapeContext *reshape_context,
    const float (*vert_coords)[3],
    const int num_vert_coords)
{
  MultiresReshapeAssignVertcosContext reshape_vertcos_context{};
  reshape_vertcos_context.reshape_context = reshape_context;
  reshape_vertcos_context.vert_coords = vert_coords;
  reshape_vertcos_context.num_vert_coords = num_vert_coords;

  SubdivForeachContext foreach_context{};
  foreach_context.topology_info = multires_reshape_vertcos_foreach_topology_info;
  foreach_context.vertex_inner = multires_reshape_vertcos_foreach_vertex_inner;
  foreach_context.vertex_every_edge = multires_reshape_vertcos_foreach_vertex_every_edge;
  foreach_context.vertex_every_corner = multires_reshape_vertcos_foreach_vertex_every_corner;
  foreach_context.user_data = &reshape_vertcos_context;

  SubdivToMeshSettings mesh_settings;
  mesh_settings.resolution = (1 << reshape_context->reshape.level) + 1;
  mesh_settings.use_optimal_display = false;

  return BKE_subdiv_foreach_subdiv_geometry(
      reshape_context->subdiv, &foreach_context, &mesh_settings, reshape_context->base_mesh);
}
