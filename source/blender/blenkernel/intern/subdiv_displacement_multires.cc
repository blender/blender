/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>

#include "BKE_subdiv.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"
#include "BLI_offset_indices.hh"

#include "BKE_customdata.hh"
#include "BKE_multires.hh"
#include "BKE_subdiv_eval.hh"

#include "MEM_guardedalloc.h"

namespace blender::bke::subdiv {

struct PolyCornerIndex {
  int face_index;
  int corner;
};

struct MultiresDisplacementData {
  Subdiv *subdiv = nullptr;
  int grid_size = 0;
  /* Mesh is used to read external displacement. */
  Mesh *mesh = nullptr;
  const MultiresModifierData *mmd = nullptr;
  OffsetIndices<int> faces = {};
  const MDisps *mdisps = nullptr;
  /* Indexed by PTEX face index, contains face/corner which corresponds
   * to it.
   *
   * NOTE: For quad face this is an index of first corner only, since
   * there we only have one PTEX. */
  Array<PolyCornerIndex> ptex_face_corner = {};
  /* Indexed by coarse face index, returns first PTEX face index corresponding
   * to that coarse face. */
  int *face_ptex_offset = nullptr;
  /* Sanity check, is used in debug builds.
   * Controls that initialize() was called prior to eval_displacement(). */
  bool is_initialized = false;
};

/* Denotes which grid to use to average value of the displacement read from the
 * grid which corresponds to the PTEX face. */
enum class AverageWith : int8_t {
  None,
  All,
  Prev,
  Next,
};

static int displacement_get_grid_and_coord(const Displacement &displacement,
                                           const int ptex_face_index,
                                           const float u,
                                           const float v,
                                           const MDisps **r_displacement_grid,
                                           float &r_grid_u,
                                           float &r_grid_v)
{
  const MultiresDisplacementData &data = *static_cast<MultiresDisplacementData *>(
      displacement.user_data);
  const PolyCornerIndex &face_corner = data.ptex_face_corner[ptex_face_index];
  const IndexRange face = data.faces[face_corner.face_index];
  const int start_grid_index = face.start() + face_corner.corner;
  int corner = 0;
  if (face.size() == 4) {
    float corner_u, corner_v;
    corner = rotate_quad_to_corner(u, v, &corner_u, &corner_v);
    *r_displacement_grid = &data.mdisps[start_grid_index + corner];
    ptex_face_uv_to_grid_uv(corner_u, corner_v, &r_grid_u, &r_grid_v);
  }
  else {
    *r_displacement_grid = &data.mdisps[start_grid_index];
    ptex_face_uv_to_grid_uv(u, v, &r_grid_u, &r_grid_v);
  }
  return corner;
}

static const MDisps *displacement_get_other_grid(const Displacement &displacement,
                                                 const int ptex_face_index,
                                                 const int corner,
                                                 const int corner_delta)
{
  MultiresDisplacementData &data = *static_cast<MultiresDisplacementData *>(
      displacement.user_data);
  const PolyCornerIndex &face_corner = data.ptex_face_corner[ptex_face_index];
  const IndexRange face = data.faces[face_corner.face_index];
  const int effective_corner = (face.size() == 4) ? corner : face_corner.corner;
  const int next_corner = (effective_corner + corner_delta + face.size()) % face.size();
  return &data.mdisps[face[next_corner]];
}

BLI_INLINE AverageWith read_displacement_grid(const MDisps &displacement_grid,
                                              const int grid_size,
                                              const float grid_u,
                                              const float grid_v,
                                              float3 &r_tangent_D)
{
  if (displacement_grid.disps == nullptr) {
    r_tangent_D = float3(0.0f);
    return AverageWith::None;
  }
  const int x = roundf(grid_u * (grid_size - 1));
  const int y = roundf(grid_v * (grid_size - 1));
  r_tangent_D = displacement_grid.disps[y * grid_size + x];
  if (x == 0 && y == 0) {
    return AverageWith::All;
  }
  if (x == 0) {
    return AverageWith::Prev;
  }
  if (y == 0) {
    return AverageWith::Next;
  }
  return AverageWith::None;
}

static void average_convert_grid_coord_to_ptex(const int num_corners,
                                               const int corner,
                                               const float grid_u,
                                               const float grid_v,
                                               float &r_ptex_face_u,
                                               float &r_ptex_face_v)
{
  if (num_corners == 4) {
    rotate_grid_to_quad(corner, grid_u, grid_v, &r_ptex_face_u, &r_ptex_face_v);
  }
  else {
    grid_uv_to_ptex_face_uv(grid_u, grid_v, &r_ptex_face_u, &r_ptex_face_v);
  }
}

static void average_construct_tangent_matrix(Subdiv &subdiv,
                                             const int num_corners,
                                             const int ptex_face_index,
                                             const int corner,
                                             const float u,
                                             const float v,
                                             float3x3 &r_tangent_matrix)
{
  const bool is_quad = num_corners == 4;
  const int quad_corner = is_quad ? corner : 0;
  float3 dummy_P;
  float3 dPdu;
  float3 dPdv;
  eval_limit_point_and_derivatives(&subdiv, ptex_face_index, u, v, dummy_P, dPdu, dPdv);
  BKE_multires_construct_tangent_matrix(r_tangent_matrix, dPdu, dPdv, quad_corner);
}

static void average_read_displacement_tangent(const MultiresDisplacementData &data,
                                              const MDisps &other_displacement_grid,
                                              const float grid_u,
                                              const float grid_v,
                                              float3 &r_tangent_D)
{
  read_displacement_grid(other_displacement_grid, data.grid_size, grid_u, grid_v, r_tangent_D);
}

static void average_read_displacement_object(const MultiresDisplacementData &data,
                                             const MDisps &displacement_grid,
                                             const float grid_u,
                                             const float grid_v,
                                             const int ptex_face_index,
                                             const int corner_index,
                                             float3 &r_D)
{
  const PolyCornerIndex &face_corner = data.ptex_face_corner[ptex_face_index];
  const int num_corners = data.faces[face_corner.face_index].size();
  /* Get (u, v) coordinate within the other PTEX face which corresponds to
   * the grid coordinates. */
  float u, v;
  average_convert_grid_coord_to_ptex(num_corners, corner_index, grid_u, grid_v, u, v);
  /* Construct tangent matrix which corresponds to partial derivatives
   * calculated for the other PTEX face. */
  float3x3 tangent_matrix;
  average_construct_tangent_matrix(
      *data.subdiv, num_corners, ptex_face_index, corner_index, u, v, tangent_matrix);
  /* Read displacement from other grid in a tangent space. */
  float3 tangent_D;
  average_read_displacement_tangent(data, displacement_grid, grid_u, grid_v, tangent_D);
  /* Convert displacement to object space. */
  r_D = math::transform_direction(tangent_matrix, tangent_D);
}

static void average_get_other_ptex_and_corner(const MultiresDisplacementData &data,
                                              const int ptex_face_index,
                                              const int corner,
                                              const int corner_delta,
                                              int &r_other_ptex_face_index,
                                              int &r_other_corner_index)
{
  const PolyCornerIndex &face_corner = data.ptex_face_corner[ptex_face_index];
  const int face_index = face_corner.face_index;
  const int num_corners = data.faces[face_corner.face_index].size();
  const bool is_quad = (num_corners == 4);
  const int start_ptex_face_index = data.face_ptex_offset[face_index];
  r_other_corner_index = (corner + corner_delta + num_corners) % num_corners;
  r_other_ptex_face_index = is_quad ? start_ptex_face_index :
                                      start_ptex_face_index + r_other_corner_index;
}

/* NOTE: Grid coordinates are relative to the other grid already. */
static void average_with_other(const Displacement &displacement,
                               const int ptex_face_index,
                               const int corner,
                               const float grid_u,
                               const float grid_v,
                               const int corner_delta,
                               float3 &r_D)
{
  const MultiresDisplacementData &data = *static_cast<MultiresDisplacementData *>(
      displacement.user_data);
  const MDisps other_displacement_grid = *displacement_get_other_grid(
      displacement, ptex_face_index, corner, corner_delta);
  int other_ptex_face_index, other_corner_index;
  average_get_other_ptex_and_corner(
      data, ptex_face_index, corner, corner_delta, other_ptex_face_index, other_corner_index);
  /* Get displacement in object space. */
  float3 other_D;
  average_read_displacement_object(data,
                                   other_displacement_grid,
                                   grid_u,
                                   grid_v,
                                   other_ptex_face_index,
                                   other_corner_index,
                                   other_D);
  /* Average result with the other displacement vector. */
  r_D += other_D;
  r_D *= 0.5f;
}

static void average_with_all(const Displacement &displacement,
                             const int ptex_face_index,
                             const int corner,
                             const float /*grid_u*/,
                             const float /*grid_v*/,
                             float3 &r_D)
{
  const MultiresDisplacementData &data = *static_cast<MultiresDisplacementData *>(
      displacement.user_data);
  const PolyCornerIndex &face_corner = data.ptex_face_corner[ptex_face_index];
  const int num_corners = data.faces[face_corner.face_index].size();
  for (int corner_delta = 1; corner_delta < num_corners; corner_delta++) {
    average_with_other(displacement, ptex_face_index, corner, 0.0f, 0.0f, corner_delta, r_D);
  }
}

static void average_with_next(const Displacement &displacement,
                              const int ptex_face_index,
                              const int corner,
                              const float grid_u,
                              const float /*grid_v*/,
                              float3 &r_D)
{
  average_with_other(displacement, ptex_face_index, corner, 0.0f, grid_u, 1, r_D);
}

static void average_with_prev(const Displacement &displacement,
                              const int ptex_face_index,
                              const int corner,
                              const float /*grid_u*/,
                              const float grid_v,
                              float3 &r_D)
{
  average_with_other(displacement, ptex_face_index, corner, grid_v, 0.0f, -1, r_D);
}

static void average_displacement(const Displacement &displacement,
                                 const AverageWith average_with,
                                 const int ptex_face_index,
                                 const int corner,
                                 const float grid_u,
                                 const float grid_v,
                                 float3 &r_D)
{
  switch (average_with) {
    case AverageWith::All:
      average_with_all(displacement, ptex_face_index, corner, grid_u, grid_v, r_D);
      break;
    case AverageWith::Prev:
      average_with_prev(displacement, ptex_face_index, corner, grid_u, grid_v, r_D);
      break;
    case AverageWith::Next:
      average_with_next(displacement, ptex_face_index, corner, grid_u, grid_v, r_D);
      break;
    case AverageWith::None:
      break;
  }
}

static int displacement_get_face_corner(const MultiresDisplacementData &data,
                                        const int ptex_face_index,
                                        const float u,
                                        const float v)
{
  const PolyCornerIndex &face_corner = data.ptex_face_corner[ptex_face_index];
  const int num_corners = data.faces[face_corner.face_index].size();
  const bool is_quad = (num_corners == 4);
  if (is_quad) {
    float dummy_corner_u, dummy_corner_v;
    return rotate_quad_to_corner(u, v, &dummy_corner_u, &dummy_corner_v);
  }

  return face_corner.corner;
}

static void initialize(Displacement *displacement)
{
  MultiresDisplacementData &data = *static_cast<MultiresDisplacementData *>(
      displacement->user_data);
  multiresModifier_ensure_external_read(data.mesh, data.mmd);
  data.is_initialized = true;
}

static void eval_displacement(Displacement *displacement,
                              const int ptex_face_index,
                              const float u,
                              const float v,
                              const float3 &dPdu,
                              const float3 &dPdv,
                              float3 &r_D)
{
  MultiresDisplacementData &data = *static_cast<MultiresDisplacementData *>(
      displacement->user_data);
  BLI_assert(data.is_initialized);
  const int grid_size = data.grid_size;
  /* Get displacement in tangent space. */
  const MDisps *displacement_grid;
  float grid_u, grid_v;
  const int corner_of_quad = displacement_get_grid_and_coord(
      *displacement, ptex_face_index, u, v, &displacement_grid, grid_u, grid_v);
  /* Read displacement from the current displacement grid and see if any
   * averaging is needed. */
  float3 tangent_D;
  const AverageWith average_with = read_displacement_grid(
      *displacement_grid, grid_size, grid_u, grid_v, tangent_D);
  /* Convert it to the object space. */
  float3x3 tangent_matrix;
  BKE_multires_construct_tangent_matrix(tangent_matrix, dPdu, dPdv, corner_of_quad);

  r_D = math::transform_direction(tangent_matrix, tangent_D);
  /* For the boundary points of grid average two (or all) neighbor grids. */
  const int corner = displacement_get_face_corner(data, ptex_face_index, u, v);
  average_displacement(*displacement, average_with, ptex_face_index, corner, grid_u, grid_v, r_D);
}

static void free_displacement(Displacement *displacement)
{
  MultiresDisplacementData *data = static_cast<MultiresDisplacementData *>(
      displacement->user_data);
  MEM_delete(data);
}

/* TODO(sergey): This seems to be generally used information, which almost
 * worth adding to a subdiv itself, with possible cache of the value. */
static int count_num_ptex_faces(const Mesh &mesh)
{
  int num_ptex_faces = 0;
  const OffsetIndices faces = mesh.faces();
  for (int face_index = 0; face_index < mesh.faces_num; face_index++) {
    num_ptex_faces += (faces[face_index].size() == 4) ? 1 : faces[face_index].size();
  }
  return num_ptex_faces;
}

static void displacement_data_init_mapping(Displacement &displacement, const Mesh &mesh)
{
  MultiresDisplacementData &data = *static_cast<MultiresDisplacementData *>(
      displacement.user_data);
  const OffsetIndices faces = mesh.faces();
  const int num_ptex_faces = count_num_ptex_faces(mesh);
  data.ptex_face_corner.reinitialize(num_ptex_faces);
  /* Fill in offsets. */
  int ptex_face_index = 0;
  MutableSpan<PolyCornerIndex> ptex_face_corner = data.ptex_face_corner.as_mutable_span();
  for (int face_index = 0; face_index < mesh.faces_num; face_index++) {
    const IndexRange face = faces[face_index];
    if (face.size() == 4) {
      ptex_face_corner[ptex_face_index].face_index = face_index;
      ptex_face_corner[ptex_face_index].corner = 0;
      ptex_face_index++;
    }
    else {
      for (int corner = 0; corner < face.size(); corner++) {
        ptex_face_corner[ptex_face_index].face_index = face_index;
        ptex_face_corner[ptex_face_index].corner = corner;
        ptex_face_index++;
      }
    }
  }
}

static void displacement_init_data(Displacement &displacement,
                                   Subdiv &subdiv,
                                   Mesh &mesh,
                                   const MultiresModifierData &mmd)
{
  MultiresDisplacementData &data = *static_cast<MultiresDisplacementData *>(
      displacement.user_data);
  data.subdiv = &subdiv;
  data.grid_size = grid_size_from_level(mmd.totlvl);
  data.mesh = &mesh;
  data.mmd = &mmd;
  data.faces = mesh.faces();
  data.mdisps = static_cast<const MDisps *>(CustomData_get_layer(&mesh.corner_data, CD_MDISPS));
  data.face_ptex_offset = face_ptex_offset_get(&subdiv);
  data.is_initialized = false;
  displacement_data_init_mapping(displacement, mesh);
}

static void displacement_init_functions(Displacement *displacement)
{
  displacement->initialize = initialize;
  displacement->eval_displacement = eval_displacement;
  displacement->free = free_displacement;
}

void displacement_attach_from_multires(Subdiv *subdiv, Mesh *mesh, const MultiresModifierData *mmd)
{
  /* Make sure we don't have previously assigned displacement. */
  displacement_detach(subdiv);
  /* It is possible to have mesh without CD_MDISPS layer. Happens when using
   * dynamic topology. */
  if (!CustomData_has_layer(&mesh->corner_data, CD_MDISPS)) {
    return;
  }
  /* Allocate all required memory. */
  Displacement *displacement = MEM_callocN<Displacement>("multires displacement");
  displacement->user_data = MEM_new<MultiresDisplacementData>("multires displacement data");
  displacement_init_data(*displacement, *subdiv, *mesh, *mmd);
  displacement_init_functions(displacement);
  /* Finish. */
  subdiv->displacement_evaluator = displacement;
}

}  // namespace blender::bke::subdiv
