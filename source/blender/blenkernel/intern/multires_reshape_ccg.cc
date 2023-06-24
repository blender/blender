/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "multires_reshape.hh"

#include <cstring>

#include "BLI_utildefines.h"

#include "BKE_ccg.h"
#include "BKE_subdiv_ccg.h"

bool multires_reshape_assign_final_coords_from_ccg(const MultiresReshapeContext *reshape_context,
                                                   SubdivCCG *subdiv_ccg)
{
  CCGKey reshape_level_key;
  BKE_subdiv_ccg_key(&reshape_level_key, subdiv_ccg, reshape_context->reshape.level);

  const int reshape_grid_size = reshape_context->reshape.grid_size;
  const float reshape_grid_size_1_inv = 1.0f / (float(reshape_grid_size) - 1.0f);

  int num_grids = subdiv_ccg->num_grids;
  for (int grid_index = 0; grid_index < num_grids; ++grid_index) {
    CCGElem *ccg_grid = subdiv_ccg->grids[grid_index];
    for (int y = 0; y < reshape_grid_size; ++y) {
      const float v = float(y) * reshape_grid_size_1_inv;
      for (int x = 0; x < reshape_grid_size; ++x) {
        const float u = float(x) * reshape_grid_size_1_inv;

        GridCoord grid_coord;
        grid_coord.grid_index = grid_index;
        grid_coord.u = u;
        grid_coord.v = v;

        ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(
            reshape_context, &grid_coord);

        BLI_assert(grid_element.displacement != nullptr);
        memcpy(grid_element.displacement,
               CCG_grid_elem_co(&reshape_level_key, ccg_grid, x, y),
               sizeof(float[3]));

        /* NOTE: The sculpt mode might have SubdivCCG's data out of sync from what is stored in
         * the original object. This happens upon the following scenario:
         *
         *  - User enters sculpt mode of the default cube object.
         *  - Sculpt mode creates new `layer`
         *  - User does some strokes.
         *  - User used undo until sculpt mode is exited.
         *
         * In an ideal world the sculpt mode will take care of keeping CustomData and CCG layers in
         * sync by doing proper pushes to a local sculpt undo stack.
         *
         * Since the proper solution needs time to be implemented, consider the target object
         * the source of truth of which data layers are to be updated during reshape. This means,
         * for example, that if the undo system says object does not have paint mask layer, it is
         * not to be updated.
         *
         * This is a fragile logic, and is only working correctly because the code path is only
         * used by sculpt changes. In other use cases the code might not catch inconsistency and
         * silently do wrong decision. */
        /* NOTE: There is a known bug in Undo code that results in first Sculpt step
         * after a Memfile one to never be undone (see #83806). This might be the root cause of
         * this inconsistency. */
        if (reshape_level_key.has_mask && grid_element.mask != nullptr) {
          *grid_element.mask = *CCG_grid_elem_mask(&reshape_level_key, ccg_grid, x, y);
        }
      }
    }
  }

  return true;
}
