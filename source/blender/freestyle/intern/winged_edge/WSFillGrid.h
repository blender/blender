/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FREESTYLE_WS_FILL_GRID_H__
#define __FREESTYLE_WS_FILL_GRID_H__

/** \file blender/freestyle/intern/winged_edge/WSFillGrid.h
 *  \ingroup freestyle
 *  \brief Class to fill in a grid from a SceneGraph (uses only the WingedEdge structures)
 *  \author Stephane Grabli
 *  \date 03/05/2003
 */

#include "WEdge.h"

#include "../geometry/Grid.h"
#include "../geometry/Polygon.h"

class LIB_WINGED_EDGE_EXPORT WSFillGrid
{
public:
	inline WSFillGrid(Grid *grid = NULL, WingedEdge *winged_edge = NULL)
	{
		_winged_edge = winged_edge;
		_grid = grid;
		_polygon_id = 0;
	}

	virtual ~WSFillGrid() {}

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
		if (winged_edge)
			_winged_edge = winged_edge;
	}

	void setGrid(Grid *grid)
	{
		if (grid)
			_grid = grid;
	}

private:
	Grid *_grid;
	WingedEdge *_winged_edge;
	unsigned _polygon_id;
};

#endif // __FREESTYLE_WS_FILL_GRID_H__