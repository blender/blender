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
 */

#ifndef __FASTGRID_H__
#define __FASTGRID_H__

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the bounding box of the scene
 */

#include "Grid.h"

namespace Freestyle {

/*! Class to define a regular grid used for ray casting computations
 *  We don't use a hashtable here. The grid is explicitly stored for faster computations.
 *  However, this might result in significant increase in memory usage
 *  (compared to the regular grid).
 */
class FastGrid : public Grid {
 public:
  FastGrid() : Grid()
  {
    _cells = NULL;
    _cells_size = 0;
  }

  virtual ~FastGrid()
  {
    clear();
  }

  /*!
   * clears the grid
   * Deletes all the cells, clears the hashtable, resets size, size of cell, number of cells.
   */
  virtual void clear();

  /*! Sets the different parameters of the grid
   *    orig
   *      The grid origin
   *    size
   *      The grid's dimensions
   *    nb
   *      The number of cells of the grid
   */
  virtual void configure(const Vec3r &orig, const Vec3r &size, unsigned nb);

  /*! returns the cell whose coordinates are passed as argument */
  Cell *getCell(const Vec3u &p);

  /*! Fills the case p with the cell iCell */
  virtual void fillCell(const Vec3u &p, Cell &cell);

 protected:
  Cell **_cells;
  unsigned _cells_size;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:FastGrid")
#endif
};

} /* namespace Freestyle */

#endif  // __FASTGRID_H__
