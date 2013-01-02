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

#ifndef __FREESTYLE_GRID_DENSITY_PROVIDER_H__
#define __FREESTYLE_GRID_DENSITY_PROVIDER_H__

/** \file blender/freestyle/intern/view_map/GridDensityProvider.h
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the projected image of a scene
 *  \author Alexander Beels
 *  \date 2011-2-5
 */

#include <stdexcept>
#include <memory>

#include "OccluderSource.h"

#include "../geometry/BBox.h"

class GridDensityProvider
{
	// Disallow copying and assignment
	GridDensityProvider(const GridDensityProvider& other);
	GridDensityProvider& operator=(const GridDensityProvider& other);

public:
	GridDensityProvider(OccluderSource& source) : source(source) {}

	virtual ~GridDensityProvider() {};

	float cellSize()
	{
		return _cellSize;
	}

	unsigned cellsX()
	{
		return _cellsX;
	}

	unsigned cellsY()
	{
		return _cellsY;
	}

	float cellOrigin(int index)
	{
		if (index < 2) {
			return _cellOrigin[index];
		}
		else {
			throw new out_of_range("GridDensityProvider::cellOrigin can take only indexes of 0 or 1.");
		}
	}

	static void calculateOptimalProscenium(OccluderSource& source, real proscenium[4])
	{
		source.begin();
		if (source.isValid()) {
			const Vec3r& initialPoint = source.getGridSpacePolygon().getVertices()[0];
			proscenium[0] = proscenium[1] = initialPoint[0];
			proscenium[2] = proscenium[3] = initialPoint[1];
			while (source.isValid()) {
				GridHelpers::expandProscenium (proscenium, source.getGridSpacePolygon());
				source.next();
			}
		}
		cout << "Proscenium: (" << proscenium[0] << ", " << proscenium[1] << ", " << proscenium[2]
		     << ", " << proscenium[3] << ")" << endl;
	}

	static void calculateQuickProscenium(const GridHelpers::Transform& transform, const BBox<Vec3r>& bbox,
	                                     real proscenium[4])
	{
		real z;
		// We want to use the z-coordinate closest to the camera to determine the proscenium face
		if (::fabs(bbox.getMin()[2]) < ::fabs(bbox.getMax()[2])) {
			z = bbox.getMin()[2];
		}
		else {
			z = bbox.getMax()[2];
		}
		// Now calculate the proscenium according to the min and max values of the x and y coordinates
		Vec3r minPoint = transform(Vec3r(bbox.getMin()[0], bbox.getMin()[1], z));
		Vec3r maxPoint = transform(Vec3r(bbox.getMax()[0], bbox.getMax()[1], z));
		cout << "Bounding box: " << minPoint << " to " << maxPoint << endl;
		proscenium[0] = std::min(minPoint[0], maxPoint[0]);
		proscenium[1] = std::max(minPoint[0], maxPoint[0]);
		proscenium[2] = std::min(minPoint[1], maxPoint[1]);
		proscenium[3] = std::max(minPoint[1], maxPoint[1]);
		cout << "Proscenium  : " << proscenium[0] << ", " << proscenium[1] << ", " << proscenium[2] << ", "
		     << proscenium[3] << endl;
	}

protected:
	OccluderSource& source;
	unsigned _cellsX, _cellsY;
	float _cellSize;
	float _cellOrigin[2];
};

class GridDensityProviderFactory
{
	// Disallow copying and assignment
	GridDensityProviderFactory (const GridDensityProviderFactory& other);
	GridDensityProviderFactory& operator= (const GridDensityProviderFactory& other);

public:
	GridDensityProviderFactory() {}

	virtual auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source, const real proscenium[4]) = 0;

	virtual auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox,
	                                                             const GridHelpers::Transform& transform) = 0;

	virtual auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source) = 0;

	virtual ~GridDensityProviderFactory () {}
};

#endif // __FREESTYLE_GRID_DENSITY_PROVIDER_H__
