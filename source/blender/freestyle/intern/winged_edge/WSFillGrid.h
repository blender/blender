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

#ifndef __FREESTYLE_WS_FILL_GRID_H__
#define __FREESTYLE_WS_FILL_GRID_H__

/** \file
 * \ingroup freestyle
 * \brief Class to fill in a grid from a SceneGraph (uses only the WingedEdge structures)
 */

#include "WEdge.h"

#include "../geometry/Grid.h"
#include "../geometry/Polygon.h"

namespace Freestyle {

class WSFillGrid {
 public:
  inline WSFillGrid(Grid *grid = NULL, WingedEdge *winged_edge = NULL)
  {
    _winged_edge = winged_edge;
    _grid = grid;
    _polygon_id = 0;
  }

  virtual ~WSFillGrid()
  {
  }

  void fillGrid();

  /*! Accessors */
  WingedEdge *getWingedEdge()
  {
    return _winged_edge;
  }

  Grid *getGrid()
  {
    return _grid;
  }

  /*! Modifiers */
  void setWingedEdge(WingedEdge *winged_edge)
  {
    if (winged_edge) {
      _winged_edge = winged_edge;
    }
  }

  void setGrid(Grid *grid)
  {
    if (grid) {
      _grid = grid;
    }
  }

 private:
  Grid *_grid;
  WingedEdge *_winged_edge;
  unsigned _polygon_id;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WSFillGrid")
#endif
};

} /* namespace Freestyle */

#endif  // __FREESTYLE_WS_FILL_GRID_H__
