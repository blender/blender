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

#ifndef __FREESTYLE_HEURISTIC_GRID_DENSITY_PROVIDER_FACTORY_H__
#define __FREESTYLE_HEURISTIC_GRID_DENSITY_PROVIDER_FACTORY_H__

/** \file blender/freestyle/intern/view_map/HeuristicGridDensityProviderFactory.h
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the projected image of a scene
 *  \author Alexander Beels
 *  \date 2011-2-8
 */

//#include <memory> // provided by GridDensityProvider.h

#include "AverageAreaGridDensityProvider.h"
//#include "GridDensityProvider.h" // provided by *GridDensityProvider.h below
#include "Pow23GridDensityProvider.h"

namespace Freestyle {

class HeuristicGridDensityProviderFactory : public GridDensityProviderFactory
{
public:
	HeuristicGridDensityProviderFactory(real sizeFactor, unsigned numFaces);
	~HeuristicGridDensityProviderFactory();

	auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source, const real proscenium[4]);
	auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox,
	                                                     const GridHelpers::Transform& transform);
	auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source);

protected:
	real sizeFactor;
	unsigned numFaces;
};

} /* namespace Freestyle */

#endif // __FREESTYLE_HEURISTIC_GRID_DENSITY_PROVIDER_FACTORY_H__
