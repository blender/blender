/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to fill in a grid from a SceneGraph (uses only the WingedEdge structures)
 */

#include "WEdge.h"

#include "../geometry/Grid.h"
#include "../geometry/Polygon.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class WFillGrid {
 public:
  inline WFillGrid(Grid *grid = nullptr, WingedEdge *winged_edge = nullptr)
  {
    _winged_edge = winged_edge;
    _grid = grid;
    _polygon_id = 0;
  }

  virtual ~WFillGrid() {}

  void fillGrid();

  /** Accessors */
  WingedEdge *getWingedEdge()
  {
    return _winged_edge;
  }

  Grid *getGrid()
  {
    return _grid;
  }

  /** Modifiers */
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
  uint _polygon_id;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WFillGrid")
#endif
};

} /* namespace Freestyle */
