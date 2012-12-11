//
//  Filename         : WSFillGrid.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to fill in a grid from a SceneGraph
//                     (uses only the WingedEdge structures)
//  Date of creation : 03/05/2003
//
///////////////////////////////////////////////////////////////////////////////


//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef  WS_FILL_GRID_H
# define WS_FILL_GRID_H

# include "../geometry/Grid.h" 
# include "../geometry/Polygon.h"
# include "WEdge.h"

class LIB_WINGED_EDGE_EXPORT WSFillGrid
{
public:

  inline WSFillGrid(Grid* grid = 0, WingedEdge* winged_edge = 0) {
    _winged_edge = winged_edge;
    _grid = grid;
    _polygon_id = 0;
  }

  virtual ~WSFillGrid() {}

  void fillGrid();

  /*! Accessors */
  WingedEdge* getWingedEdge() {
    return _winged_edge;
  }

  Grid* getGrid() {
    return _grid;
  }

  /*! Modifiers */
  void setWingedEdge(WingedEdge* winged_edge) {
    if (winged_edge)
      _winged_edge = winged_edge;
  }

  void setGrid(Grid* grid) {
    if (grid)
      _grid = grid;
  }

private:

  Grid*		_grid;
  WingedEdge*	_winged_edge;
  unsigned	_polygon_id;
};

#endif // WS_FILL_GRID_H
