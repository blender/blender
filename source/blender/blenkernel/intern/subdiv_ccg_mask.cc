/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>

#include "BKE_subdiv_ccg.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"

#include "BKE_customdata.hh"
#include "BKE_subdiv.hh"

#include "MEM_guardedalloc.h"

struct PolyCornerIndex {
  int face_index;
  int corner;
};

struct GridPaintMaskData {
  // int grid_size;
  blender::OffsetIndices<int> faces;
  const GridPaintMask *grid_paint_mask;
  /* Indexed by ptex face index, contains face/corner which corresponds
   * to it.
   *
   * NOTE: For quad face this is an index of first corner only, since
   * there we only have one ptex.
   */
  PolyCornerIndex *ptex_face_corner;
};

static int mask_get_grid_and_coord(SubdivCCGMaskEvaluator *mask_evaluator,
                                   const int ptex_face_index,
                                   const float u,
                                   const float v,
                                   const GridPaintMask **r_mask_grid,
                                   float *grid_u,
                                   float *grid_v)
{
  GridPaintMaskData *data = static_cast<GridPaintMaskData *>(mask_evaluator->user_data);
  const PolyCornerIndex *poly_corner = &data->ptex_face_corner[ptex_face_index];
  const blender::IndexRange face = data->faces[poly_corner->face_index];
  const int start_grid_index = face.start() + poly_corner->corner;
  int corner = 0;
  if (face.size() == 4) {
    float corner_u, corner_v;
    corner = BKE_subdiv_rotate_quad_to_corner(u, v, &corner_u, &corner_v);
    *r_mask_grid = &data->grid_paint_mask[start_grid_index + corner];
    BKE_subdiv_ptex_face_uv_to_grid_uv(corner_u, corner_v, grid_u, grid_v);
  }
  else {
    *r_mask_grid = &data->grid_paint_mask[start_grid_index];
    BKE_subdiv_ptex_face_uv_to_grid_uv(u, v, grid_u, grid_v);
  }
  return corner;
}

BLI_INLINE float read_mask_grid(const GridPaintMask *mask_grid,
                                const float grid_u,
                                const float grid_v)
{
  if (mask_grid->data == nullptr) {
    return 0;
  }
  const int grid_size = BKE_subdiv_grid_size_from_level(mask_grid->level);
  const int x = roundf(grid_u * (grid_size - 1));
  const int y = roundf(grid_v * (grid_size - 1));
  return mask_grid->data[y * grid_size + x];
}

static float eval_mask(SubdivCCGMaskEvaluator *mask_evaluator,
                       const int ptex_face_index,
                       const float u,
                       const float v)
{
  const GridPaintMask *mask_grid;
  float grid_u, grid_v;
  mask_get_grid_and_coord(mask_evaluator, ptex_face_index, u, v, &mask_grid, &grid_u, &grid_v);
  return read_mask_grid(mask_grid, grid_u, grid_v);
}

static void free_mask_data(SubdivCCGMaskEvaluator *mask_evaluator)
{
  GridPaintMaskData *data = static_cast<GridPaintMaskData *>(mask_evaluator->user_data);
  MEM_freeN(data->ptex_face_corner);
  MEM_freeN(data);
}

/* TODO(sergey): This seems to be generally used information, which almost
 * worth adding to a subdiv itself, with possible cache of the value.
 */
static int count_num_ptex_faces(const Mesh *mesh)
{
  int num_ptex_faces = 0;
  const blender::OffsetIndices faces = mesh->faces();
  for (const int face_index : faces.index_range()) {
    num_ptex_faces += (faces[face_index].size() == 4) ? 1 : faces[face_index].size();
  }
  return num_ptex_faces;
}

static void mask_data_init_mapping(SubdivCCGMaskEvaluator *mask_evaluator, const Mesh *mesh)
{
  GridPaintMaskData *data = static_cast<GridPaintMaskData *>(mask_evaluator->user_data);
  const blender::OffsetIndices faces = mesh->faces();
  const int num_ptex_faces = count_num_ptex_faces(mesh);
  /* Allocate memory. */
  data->ptex_face_corner = static_cast<PolyCornerIndex *>(
      MEM_malloc_arrayN(num_ptex_faces, sizeof(*data->ptex_face_corner), __func__));
  /* Fill in offsets. */
  int ptex_face_index = 0;
  PolyCornerIndex *ptex_face_corner = data->ptex_face_corner;
  for (const int face_index : faces.index_range()) {
    const blender::IndexRange face = faces[face_index];
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

static void mask_init_data(SubdivCCGMaskEvaluator *mask_evaluator, const Mesh *mesh)
{
  GridPaintMaskData *data = static_cast<GridPaintMaskData *>(mask_evaluator->user_data);
  data->faces = mesh->faces();
  data->grid_paint_mask = static_cast<const GridPaintMask *>(
      CustomData_get_layer(&mesh->corner_data, CD_GRID_PAINT_MASK));
  mask_data_init_mapping(mask_evaluator, mesh);
}

static void mask_init_functions(SubdivCCGMaskEvaluator *mask_evaluator)
{
  mask_evaluator->eval_mask = eval_mask;
  mask_evaluator->free = free_mask_data;
}

bool BKE_subdiv_ccg_mask_init_from_paint(SubdivCCGMaskEvaluator *mask_evaluator, const Mesh *mesh)
{
  if (!CustomData_get_layer(&mesh->corner_data, CD_GRID_PAINT_MASK)) {
    return false;
  }
  /* Allocate all required memory. */
  mask_evaluator->user_data = MEM_new<GridPaintMaskData>("mask from grid data");
  mask_init_data(mask_evaluator, mesh);
  mask_init_functions(mask_evaluator);
  return true;
}
