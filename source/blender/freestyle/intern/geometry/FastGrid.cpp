/* SPDX-FileCopyrightText: 2009-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the bounding box of the scene
 */

#include <cstdlib>

#include "FastGrid.h"

#include "BKE_global.h"
#include "BLI_utildefines.h"

namespace Freestyle {

void FastGrid::clear()
{
  if (!_cells) {
    return;
  }

  for (uint i = 0; i < _cells_size; i++) {
    if (_cells[i]) {
      delete _cells[i];
    }
  }
  delete[] _cells;
  _cells = nullptr;
  _cells_size = 0;

  Grid::clear();
}

void FastGrid::configure(const Vec3r &orig, const Vec3r &size, uint nb)
{
  Grid::configure(orig, size, nb);
  _cells_size = _cells_nb[0] * _cells_nb[1] * _cells_nb[2];
  _cells = new Cell *[_cells_size];
  memset(_cells, 0, _cells_size * sizeof(*_cells));
}

Cell *FastGrid::getCell(const Vec3u &p)
{
#if 0
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << _cells << " " << p << " " << _cells_nb[0] << "-" << _cells_nb[1] << "-" << _cells_nb[2]
         << " " << _cells_size << endl;
  }
#endif
  BLI_assert_msg(_cells, "_cells is a null pointer");
  BLI_assert((_cells_nb[0] * (p[2] * _cells_nb[1] + p[1]) + p[0]) < _cells_size);
  BLI_assert(p[0] < _cells_nb[0]);
  BLI_assert(p[1] < _cells_nb[1]);
  BLI_assert(p[2] < _cells_nb[2]);
  return _cells[_cells_nb[0] * (p[2] * _cells_nb[1] + p[1]) + p[0]];
}

void FastGrid::fillCell(const Vec3u &p, Cell &cell)
{
  BLI_assert_msg(_cells, "_cells is a null pointer");
  BLI_assert((_cells_nb[0] * (p[2] * _cells_nb[1] + p[1]) + p[0]) < _cells_size);
  BLI_assert(p[0] < _cells_nb[0]);
  BLI_assert(p[1] < _cells_nb[1]);
  BLI_assert(p[2] < _cells_nb[2]);
  _cells[_cells_nb[0] * (p[2] * _cells_nb[1] + p[1]) + p[0]] = &cell;
}

} /* namespace Freestyle */
