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

#ifndef __FREESTYLE_ARBITRARY_GRID_DENSITY_PROVIDER_H__
#define __FREESTYLE_ARBITRARY_GRID_DENSITY_PROVIDER_H__

/** \file blender/freestyle/intern/view_map/ArbitraryGridDensityProvider.h
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the projected image of a scene
 *  \author Alexander Beels
 *  \date 2011-2-5
 */

#include "GridDensityProvider.h"

namespace Freestyle {

class ArbitraryGridDensityProvider : public GridDensityProvider
{
	// Disallow copying and assignment
	ArbitraryGridDensityProvider(const ArbitraryGridDensityProvider& other);
	ArbitraryGridDensityProvider& operator=(const ArbitraryGridDensityProvider& other);

public:
	ArbitraryGridDensityProvider(OccluderSource& source, const real proscenium[4], unsigned numCells);
	ArbitraryGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox,
	                             const GridHelpers::Transform& transform, unsigned numCells);
	ArbitraryGridDensityProvider(OccluderSource& source, unsigned numCells);
	virtual ~ArbitraryGridDensityProvider();

protected:
	unsigned numCells;

private:
	void initialize (const real proscenium[4]);
};

class ArbitraryGridDensityProviderFactory : public GridDensityProviderFactory
{
public:
	ArbitraryGridDensityProviderFactory(unsigned numCells);
	~ArbitraryGridDensityProviderFactory();

	auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source, const real proscenium[4]);
	auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox,
	                                                     const GridHelpers::Transform& transform);
	auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source);

protected:
	unsigned numCells;
};

} /* namespace Freestyle */

#endif // __FREESTYLE_ARBITRARY_GRID_DENSITY_PROVIDER_H__
