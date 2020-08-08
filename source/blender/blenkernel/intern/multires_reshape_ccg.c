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
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "multires_reshape.h"

#include <string.h>

#include "BLI_utildefines.h"

#include "BKE_ccg.h"
#include "BKE_subdiv_ccg.h"

bool multires_reshape_assign_final_coords_from_ccg(const MultiresReshapeContext *reshape_context,
                                                   struct SubdivCCG *subdiv_ccg)
{
  CCGKey reshape_level_key;
  BKE_subdiv_ccg_key(&reshape_level_key, subdiv_ccg, reshape_context->reshape.level);

  const int reshape_grid_size = reshape_context->reshape.grid_size;
  const float reshape_grid_size_1_inv = 1.0f / (((float)reshape_grid_size) - 1.0f);

  int num_grids = subdiv_ccg->num_grids;
  for (int grid_index = 0; grid_index < num_grids; ++grid_index) {
    CCGElem *ccg_grid = subdiv_ccg->grids[grid_index];
    for (int y = 0; y < reshape_grid_size; ++y) {
      const float v = (float)y * reshape_grid_size_1_inv;
      for (int x = 0; x < reshape_grid_size; ++x) {
        const float u = (float)x * reshape_grid_size_1_inv;

        GridCoord grid_coord;
        grid_coord.grid_index = grid_index;
        grid_coord.u = u;
        grid_coord.v = v;

        ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(
            reshape_context, &grid_coord);

        BLI_assert(grid_element.displacement != NULL);
        memcpy(grid_element.displacement,
               CCG_grid_elem_co(&reshape_level_key, ccg_grid, x, y),
               sizeof(float[3]));

        if (reshape_level_key.has_mask) {
          BLI_assert(grid_element.mask != NULL);
          *grid_element.mask = *CCG_grid_elem_mask(&reshape_level_key, ccg_grid, x, y);
        }
      }
    }
  }

  return true;
}
