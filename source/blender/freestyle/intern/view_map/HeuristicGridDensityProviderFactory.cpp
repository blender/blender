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

/** \file blender/freestyle/intern/view_map/HeuristicGridDensityProviderFactory.cpp
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the projected image of a scene
 *  \author Alexander Beels
 *  \date 2011-2-8
 */

#include "HeuristicGridDensityProviderFactory.h"

namespace Freestyle {

HeuristicGridDensityProviderFactory::HeuristicGridDensityProviderFactory(real sizeFactor, unsigned numFaces)
: sizeFactor(sizeFactor), numFaces(numFaces)
{
}

HeuristicGridDensityProviderFactory::~HeuristicGridDensityProviderFactory() {}

auto_ptr<GridDensityProvider>
HeuristicGridDensityProviderFactory::newGridDensityProvider(OccluderSource& source, const real proscenium[4])
{
	auto_ptr<AverageAreaGridDensityProvider> avg(new AverageAreaGridDensityProvider(source, proscenium, sizeFactor));
	auto_ptr<Pow23GridDensityProvider> p23(new Pow23GridDensityProvider(source, proscenium, numFaces));
	if (avg->cellSize() > p23->cellSize()) {
		return (auto_ptr<GridDensityProvider>) p23;
	}
	else {
		return (auto_ptr<GridDensityProvider>) avg;
	}
}

auto_ptr<GridDensityProvider>
HeuristicGridDensityProviderFactory::newGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox,
                                                            const GridHelpers::Transform& transform)
{
	auto_ptr<AverageAreaGridDensityProvider> avg(new AverageAreaGridDensityProvider(source, bbox,
	                                                                                transform, sizeFactor));
	auto_ptr<Pow23GridDensityProvider> p23(new Pow23GridDensityProvider(source, bbox, transform, numFaces));
	if (avg->cellSize() > p23->cellSize()) {
		return (auto_ptr<GridDensityProvider>) p23;
	}
	else {
		return (auto_ptr<GridDensityProvider>) avg;
	}
}

auto_ptr<GridDensityProvider> HeuristicGridDensityProviderFactory::newGridDensityProvider(OccluderSource& source)
{
	real proscenium[4];
	GridDensityProvider::calculateOptimalProscenium(source, proscenium);
	auto_ptr<AverageAreaGridDensityProvider> avg(new AverageAreaGridDensityProvider(source, proscenium, sizeFactor));
	auto_ptr<Pow23GridDensityProvider> p23(new Pow23GridDensityProvider(source, proscenium, numFaces));
	if (avg->cellSize() > p23->cellSize()) {
		return (auto_ptr<GridDensityProvider>) p23;
	}
	else {
		return (auto_ptr<GridDensityProvider>) avg;
	}
}

} /* namespace Freestyle */
