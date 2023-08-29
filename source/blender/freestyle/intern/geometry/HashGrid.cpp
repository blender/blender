/* SPDX-FileCopyrightText: 2012-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the bounding box of the scene
 */

#include "HashGrid.h"

#include "BLI_sys_types.h"

namespace Freestyle {

void HashGrid::clear()
{
  if (!_cells.empty()) {
    for (GridHashTable::iterator it = _cells.begin(); it != _cells.end(); it++) {
      Cell *cell = (*it).second;
      delete cell;
    }
    _cells.clear();
  }

  Grid::clear();
}

void HashGrid::configure(const Vec3r &orig, const Vec3r &size, uint nb)
{
  Grid::configure(orig, size, nb);
}

} /* namespace Freestyle */
