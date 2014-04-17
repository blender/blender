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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __HASHGRID_H__
#define __HASHGRID_H__

/** \file blender/freestyle/intern/geometry/HashGrid.h
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the bounding box of the scene
 *  \author Stephane Grabli
 *  \date 30/07/2002
 */

#if 0
# if defined(__GNUC__) && (__GNUC__ >= 3)
// hash_map is not part of the C++ standard anymore;
// hash_map.h has been kept though for backward compatibility
#  include <hash_map.h>
# else
#  include <hash_map>
# endif
#endif

#include <map>

#include "Grid.h"

namespace Freestyle {

/*! Defines a hash table used for searching the Cells */
struct GridHasher
{
#define _MUL 950706376UL
#define _MOD 2147483647UL
	inline size_t operator() (const Vec3u& p) const
	{
		size_t res = ((unsigned long) (p[0] * _MUL)) % _MOD;
		res = ((res + (unsigned long) (p[1]) * _MUL)) % _MOD;
		return ((res +(unsigned long) (p[2]) * _MUL)) % _MOD;
	}
#undef _MUL
#undef _MOD
};

/*! Class to define a regular grid used for ray casting computations */
class HashGrid : public Grid
{
public:
	typedef map<Vec3u, Cell*> GridHashTable;

	HashGrid() : Grid() {}

	virtual ~HashGrid()
	{
		clear();
	}

	/*! clears the grid
	 *  Deletes all the cells, clears the hashtable, resets size, size of cell, number of cells.
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
	virtual void configure(const Vec3r& orig, const Vec3r& size, unsigned nb);

	/*! returns the cell whose coordinates are pased as argument */
	virtual Cell *getCell(const Vec3u& p)
	{
		Cell *found_cell = NULL;

		GridHashTable::const_iterator found = _cells.find(p);
		if (found != _cells.end())
			found_cell = (*found).second;
		return found_cell;
	}

	/*! Fills the case p with the cell iCell */
	virtual void fillCell(const Vec3u& p, Cell& cell)
	{
		_cells[p] = &cell;
	}

protected:
	GridHashTable _cells;
};

} /* namespace Freestyle */

#endif // __HASHGRID_H__
