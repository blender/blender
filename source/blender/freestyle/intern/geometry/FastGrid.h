/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the bounding box of the scene
 */

#include "Grid.h"

namespace Freestyle {

/** Class to define a regular grid used for ray casting computations
 *  We don't use a hash-table here. The grid is explicitly stored for faster computations.
 *  However, this might result in significant increase in memory usage
 *  (compared to the regular grid).
 */
class FastGrid : public Grid {
 public:
  FastGrid() : Grid()
  {
    _cells = nullptr;
    _cells_size = 0;
  }

  virtual ~FastGrid()
  {
    clear();
  }

  /**
   * clears the grid
   * Deletes all the cells, clears the hash-table, resets size, size of cell, number of cells.
   */
  virtual void clear();

  /** Sets the different parameters of the grid
   *    orig
   *      The grid origin
   *    size
   *      The grid's dimensions
   *    nb
   *      The number of cells of the grid
   */
  virtual void configure(const Vec3r &orig, const Vec3r &size, uint nb);

  /** returns the cell whose coordinates are passed as argument */
  Cell *getCell(const Vec3u &p);

  /** Fills the case p with the cell iCell */
  virtual void fillCell(const Vec3u &p, Cell &cell);

 protected:
  Cell **_cells;
  uint _cells_size;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:FastGrid")
#endif
};

} /* namespace Freestyle */
