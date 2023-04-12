/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation */

/** \file
 * \ingroup bke
 */

#include <cmath>

#include "BKE_subdiv.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_mesh.hh"
#include "BKE_multires.h"
#include "BKE_subdiv_eval.h"

#include "MEM_guardedalloc.h"

struct PolyCornerIndex {
  int poly_index;
  int corner;
};

struct MultiresDisplacementData {
  Subdiv *subdiv;
  int grid_size;
  /* Mesh is used to read external displacement. */
  Mesh *mesh;
  const MultiresModifierData *mmd;
  blender::OffsetIndices<int> polys;
  const MDisps *mdisps;
  /* Indexed by PTEX face index, contains polygon/corner which corresponds
   * to it.
   *
   * NOTE: For quad polygon this is an index of first corner only, since
   * there we only have one PTEX. */
  PolyCornerIndex *ptex_poly_corner;
  /* Indexed by coarse face index, returns first PTEX face index corresponding
   * to that coarse face. */
  int *face_ptex_offset;
  /* Sanity check, is used in debug builds.
   * Controls that initialize() was called prior to eval_displacement(). */
  bool is_initialized;
};

/* Denotes which grid to use to average value of the displacement read from the
 * grid which corresponds to the PTEX face. */
typedef enum eAverageWith {
  AVERAGE_WITH_NONE,
  AVERAGE_WITH_ALL,
  AVERAGE_WITH_PREV,
  AVERAGE_WITH_NEXT,
} eAverageWith;

static int displacement_get_grid_and_coord(SubdivDisplacement *displacement,
                                           const int ptex_face_index,
                                           const float u,
                                           const float v,
                                           const MDisps **r_displacement_grid,
                                           float *grid_u,
                                           float *grid_v)
{
  MultiresDisplacementData *data = static_cast<MultiresDisplacementData *>(
      displacement->user_data);
  const PolyCornerIndex *poly_corner = &data->ptex_poly_corner[ptex_face_index];
  const blender::IndexRange poly = data->polys[poly_corner->poly_index];
  const int start_grid_index = poly.start() + poly_corner->corner;
  int corner = 0;
  if (poly.size() == 4) {
    float corner_u, corner_v;
    corner = BKE_subdiv_rotate_quad_to_corner(u, v, &corner_u, &corner_v);
    *r_displacement_grid = &data->mdisps[start_grid_index + corner];
    BKE_subdiv_ptex_face_uv_to_grid_uv(corner_u, corner_v, grid_u, grid_v);
  }
  else {
    *r_displacement_grid = &data->mdisps[start_grid_index];
    BKE_subdiv_ptex_face_uv_to_grid_uv(u, v, grid_u, grid_v);
  }
  return corner;
}

static const MDisps *displacement_get_other_grid(SubdivDisplacement *displacement,
                                                 const int ptex_face_index,
                                                 const int corner,
                                                 const int corner_delta)
{
  MultiresDisplacementData *data = static_cast<MultiresDisplacementData *>(
      displacement->user_data);
  const PolyCornerIndex *poly_corner = &data->ptex_poly_corner[ptex_face_index];
  const blender::IndexRange poly = data->polys[poly_corner->poly_index];
  const int effective_corner = (poly.size() == 4) ? corner : poly_corner->corner;
  const int next_corner = (effective_corner + corner_delta + poly.size()) % poly.size();
  return &data->mdisps[poly[next_corner]];
}

BLI_INLINE eAverageWith read_displacement_grid(const MDisps *displacement_grid,
                                               const int grid_size,
                                               const float grid_u,
                                               const float grid_v,
                                               float r_tangent_D[3])
{
  if (displacement_grid->disps == nullptr) {
    zero_v3(r_tangent_D);
    return AVERAGE_WITH_NONE;
  }
  const int x = roundf(grid_u * (grid_size - 1));
  const int y = roundf(grid_v * (grid_size - 1));
  copy_v3_v3(r_tangent_D, displacement_grid->disps[y * grid_size + x]);
  if (x == 0 && y == 0) {
    return AVERAGE_WITH_ALL;
  }
  if (x == 0) {
    return AVERAGE_WITH_PREV;
  }
  if (y == 0) {
    return AVERAGE_WITH_NEXT;
  }
  return AVERAGE_WITH_NONE;
}

static void average_convert_grid_coord_to_ptex(const int num_corners,
                                               const int corner,
                                               const float grid_u,
                                               const float grid_v,
                                               float *r_ptex_face_u,
                                               float *r_ptex_face_v)
{
  if (num_corners == 4) {
    BKE_subdiv_rotate_grid_to_quad(corner, grid_u, grid_v, r_ptex_face_u, r_ptex_face_v);
  }
  else {
    BKE_subdiv_grid_uv_to_ptex_face_uv(grid_u, grid_v, r_ptex_face_u, r_ptex_face_v);
  }
}

static void average_construct_tangent_matrix(Subdiv *subdiv,
                                             const int num_corners,
                                             const int ptex_face_index,
                                             const int corner,
                                             const float u,
                                             const float v,
                                             float r_tangent_matrix[3][3])
{
  const bool is_quad = num_corners == 4;
  const int quad_corner = is_quad ? corner : 0;
  float dummy_P[3], dPdu[3], dPdv[3];
  BKE_subdiv_eval_limit_point_and_derivatives(subdiv, ptex_face_index, u, v, dummy_P, dPdu, dPdv);
  BKE_multires_construct_tangent_matrix(r_tangent_matrix, dPdu, dPdv, quad_corner);
}

static void average_read_displacement_tangent(MultiresDisplacementData *data,
                                              const MDisps *other_displacement_grid,
                                              const float grid_u,
                                              const float grid_v,
                                              float r_tangent_D[3])
{
  read_displacement_grid(other_displacement_grid, data->grid_size, grid_u, grid_v, r_tangent_D);
}

static void average_read_displacement_object(MultiresDisplacementData *data,
                                             const MDisps *displacement_grid,
                                             const float grid_u,
                                             const float grid_v,
                                             const int ptex_face_index,
                                             const int corner_index,
                                             float r_D[3])
{
  const PolyCornerIndex *poly_corner = &data->ptex_poly_corner[ptex_face_index];
  const int num_corners = data->polys[poly_corner->poly_index].size();
  /* Get (u, v) coordinate within the other PTEX face which corresponds to
   * the grid coordinates. */
  float u, v;
  average_convert_grid_coord_to_ptex(num_corners, corner_index, grid_u, grid_v, &u, &v);
  /* Construct tangent matrix which corresponds to partial derivatives
   * calculated for the other PTEX face. */
  float tangent_matrix[3][3];
  average_construct_tangent_matrix(
      data->subdiv, num_corners, ptex_face_index, corner_index, u, v, tangent_matrix);
  /* Read displacement from other grid in a tangent space. */
  float tangent_D[3];
  average_read_displacement_tangent(data, displacement_grid, grid_u, grid_v, tangent_D);
  /* Convert displacement to object space. */
  mul_v3_m3v3(r_D, tangent_matrix, tangent_D);
}

static void average_get_other_ptex_and_corner(MultiresDisplacementData *data,
                                              const int ptex_face_index,
                                              const int corner,
                                              const int corner_delta,
                                              int *r_other_ptex_face_index,
                                              int *r_other_corner_index)
{
  const PolyCornerIndex *poly_corner = &data->ptex_poly_corner[ptex_face_index];
  const int poly_index = poly_corner->poly_index;
  const int num_corners = data->polys[poly_corner->poly_index].size();
  const bool is_quad = (num_corners == 4);
  const int start_ptex_face_index = data->face_ptex_offset[poly_index];
  *r_other_corner_index = (corner + corner_delta + num_corners) % num_corners;
  *r_other_ptex_face_index = is_quad ? start_ptex_face_index :
                                       start_ptex_face_index + *r_other_corner_index;
}

/* NOTE: Grid coordinates are relative to the other grid already. */
static void average_with_other(SubdivDisplacement *displacement,
                               const int ptex_face_index,
                               const int corner,
                               const float grid_u,
                               const float grid_v,
                               const int corner_delta,
                               float r_D[3])
{
  MultiresDisplacementData *data = static_cast<MultiresDisplacementData *>(
      displacement->user_data);
  const MDisps *other_displacement_grid = displacement_get_other_grid(
      displacement, ptex_face_index, corner, corner_delta);
  int other_ptex_face_index, other_corner_index;
  average_get_other_ptex_and_corner(
      data, ptex_face_index, corner, corner_delta, &other_ptex_face_index, &other_corner_index);
  /* Get displacement in object space. */
  float other_D[3];
  average_read_displacement_object(data,
                                   other_displacement_grid,
                                   grid_u,
                                   grid_v,
                                   other_ptex_face_index,
                                   other_corner_index,
                                   other_D);
  /* Average result with the other displacement vector. */
  add_v3_v3(r_D, other_D);
  mul_v3_fl(r_D, 0.5f);
}

static void average_with_all(SubdivDisplacement *displacement,
                             const int ptex_face_index,
                             const int corner,
                             const float /*grid_u*/,
                             const float /*grid_v*/,
                             float r_D[3])
{
  MultiresDisplacementData *data = static_cast<MultiresDisplacementData *>(
      displacement->user_data);
  const PolyCornerIndex *poly_corner = &data->ptex_poly_corner[ptex_face_index];
  const int num_corners = data->polys[poly_corner->poly_index].size();
  for (int corner_delta = 1; corner_delta < num_corners; corner_delta++) {
    average_with_other(displacement, ptex_face_index, corner, 0.0f, 0.0f, corner_delta, r_D);
  }
}

static void average_with_next(SubdivDisplacement *displacement,
                              const int ptex_face_index,
                              const int corner,
                              const float grid_u,
                              const float /*grid_v*/,
                              float r_D[3])
{
  average_with_other(displacement, ptex_face_index, corner, 0.0f, grid_u, 1, r_D);
}

static void average_with_prev(SubdivDisplacement *displacement,
                              const int ptex_face_index,
                              const int corner,
                              const float /*grid_u*/,
                              const float grid_v,
                              float r_D[3])
{
  average_with_other(displacement, ptex_face_index, corner, grid_v, 0.0f, -1, r_D);
}

static void average_displacement(SubdivDisplacement *displacement,
                                 eAverageWith average_with,
                                 const int ptex_face_index,
                                 const int corner,
                                 const float grid_u,
                                 const float grid_v,
                                 float r_D[3])
{
  switch (average_with) {
    case AVERAGE_WITH_ALL:
      average_with_all(displacement, ptex_face_index, corner, grid_u, grid_v, r_D);
      break;
    case AVERAGE_WITH_PREV:
      average_with_prev(displacement, ptex_face_index, corner, grid_u, grid_v, r_D);
      break;
    case AVERAGE_WITH_NEXT:
      average_with_next(displacement, ptex_face_index, corner, grid_u, grid_v, r_D);
      break;
    case AVERAGE_WITH_NONE:
      break;
  }
}

static int displacement_get_face_corner(MultiresDisplacementData *data,
                                        const int ptex_face_index,
                                        const float u,
                                        const float v)
{
  const PolyCornerIndex *poly_corner = &data->ptex_poly_corner[ptex_face_index];
  const int num_corners = data->polys[poly_corner->poly_index].size();
  const bool is_quad = (num_corners == 4);
  if (is_quad) {
    float dummy_corner_u, dummy_corner_v;
    return BKE_subdiv_rotate_quad_to_corner(u, v, &dummy_corner_u, &dummy_corner_v);
  }

  return poly_corner->corner;
}

static void initialize(SubdivDisplacement *displacement)
{
  MultiresDisplacementData *data = static_cast<MultiresDisplacementData *>(
      displacement->user_data);
  multiresModifier_ensure_external_read(data->mesh, data->mmd);
  data->is_initialized = true;
}

static void eval_displacement(SubdivDisplacement *displacement,
                              const int ptex_face_index,
                              const float u,
                              const float v,
                              const float dPdu[3],
                              const float dPdv[3],
                              float r_D[3])
{
  MultiresDisplacementData *data = static_cast<MultiresDisplacementData *>(
      displacement->user_data);
  BLI_assert(data->is_initialized);
  const int grid_size = data->grid_size;
  /* Get displacement in tangent space. */
  const MDisps *displacement_grid;
  float grid_u, grid_v;
  const int corner_of_quad = displacement_get_grid_and_coord(
      displacement, ptex_face_index, u, v, &displacement_grid, &grid_u, &grid_v);
  /* Read displacement from the current displacement grid and see if any
   * averaging is needed. */
  float tangent_D[3];
  eAverageWith average_with = read_displacement_grid(
      displacement_grid, grid_size, grid_u, grid_v, tangent_D);
  /* Convert it to the object space. */
  float tangent_matrix[3][3];
  BKE_multires_construct_tangent_matrix(tangent_matrix, dPdu, dPdv, corner_of_quad);
  mul_v3_m3v3(r_D, tangent_matrix, tangent_D);
  /* For the boundary points of grid average two (or all) neighbor grids. */
  const int corner = displacement_get_face_corner(data, ptex_face_index, u, v);
  average_displacement(displacement, average_with, ptex_face_index, corner, grid_u, grid_v, r_D);
}

static void free_displacement(SubdivDisplacement *displacement)
{
  MultiresDisplacementData *data = static_cast<MultiresDisplacementData *>(
      displacement->user_data);
  MEM_freeN(data->ptex_poly_corner);
  MEM_freeN(data);
}

/* TODO(sergey): This seems to be generally used information, which almost
 * worth adding to a subdiv itself, with possible cache of the value. */
static int count_num_ptex_faces(const Mesh *mesh)
{
  int num_ptex_faces = 0;
  const blender::OffsetIndices polys = mesh->polys();
  for (int poly_index = 0; poly_index < mesh->totpoly; poly_index++) {
    num_ptex_faces += (polys[poly_index].size() == 4) ? 1 : polys[poly_index].size();
  }
  return num_ptex_faces;
}

static void displacement_data_init_mapping(SubdivDisplacement *displacement, const Mesh *mesh)
{
  MultiresDisplacementData *data = static_cast<MultiresDisplacementData *>(
      displacement->user_data);
  const blender::OffsetIndices polys = mesh->polys();
  const int num_ptex_faces = count_num_ptex_faces(mesh);
  /* Allocate memory. */
  data->ptex_poly_corner = static_cast<PolyCornerIndex *>(
      MEM_malloc_arrayN(num_ptex_faces, sizeof(*data->ptex_poly_corner), "PTEX poly corner"));
  /* Fill in offsets. */
  int ptex_face_index = 0;
  PolyCornerIndex *ptex_poly_corner = data->ptex_poly_corner;
  for (int poly_index = 0; poly_index < mesh->totpoly; poly_index++) {
    const blender::IndexRange poly = polys[poly_index];
    if (poly.size() == 4) {
      ptex_poly_corner[ptex_face_index].poly_index = poly_index;
      ptex_poly_corner[ptex_face_index].corner = 0;
      ptex_face_index++;
    }
    else {
      for (int corner = 0; corner < poly.size(); corner++) {
        ptex_poly_corner[ptex_face_index].poly_index = poly_index;
        ptex_poly_corner[ptex_face_index].corner = corner;
        ptex_face_index++;
      }
    }
  }
}

static void displacement_init_data(SubdivDisplacement *displacement,
                                   Subdiv *subdiv,
                                   Mesh *mesh,
                                   const MultiresModifierData *mmd)
{
  MultiresDisplacementData *data = static_cast<MultiresDisplacementData *>(
      displacement->user_data);
  data->subdiv = subdiv;
  data->grid_size = BKE_subdiv_grid_size_from_level(mmd->totlvl);
  data->mesh = mesh;
  data->mmd = mmd;
  data->polys = mesh->polys();
  data->mdisps = static_cast<const MDisps *>(CustomData_get_layer(&mesh->ldata, CD_MDISPS));
  data->face_ptex_offset = BKE_subdiv_face_ptex_offset_get(subdiv);
  data->is_initialized = false;
  displacement_data_init_mapping(displacement, mesh);
}

static void displacement_init_functions(SubdivDisplacement *displacement)
{
  displacement->initialize = initialize;
  displacement->eval_displacement = eval_displacement;
  displacement->free = free_displacement;
}

void BKE_subdiv_displacement_attach_from_multires(Subdiv *subdiv,
                                                  Mesh *mesh,
                                                  const MultiresModifierData *mmd)
{
  /* Make sure we don't have previously assigned displacement. */
  BKE_subdiv_displacement_detach(subdiv);
  /* It is possible to have mesh without CD_MDISPS layer. Happens when using
   * dynamic topology. */
  if (!CustomData_has_layer(&mesh->ldata, CD_MDISPS)) {
    return;
  }
  /* Allocate all required memory. */
  SubdivDisplacement *displacement = MEM_cnew<SubdivDisplacement>("multires displacement");
  displacement->user_data = MEM_callocN(sizeof(MultiresDisplacementData),
                                        "multires displacement data");
  displacement_init_data(displacement, subdiv, mesh, mmd);
  displacement_init_functions(displacement);
  /* Finish. */
  subdiv->displacement_evaluator = displacement;
}
