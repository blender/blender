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

#ifndef __FREESTYLE_CULLED_OCCLUDER_SOURCE_H__
#define __FREESTYLE_CULLED_OCCLUDER_SOURCE_H__

/** \file blender/freestyle/intern/view_map/CulledOccluderSource.h
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the projected image of a scene
 *  \author Alexander Beels
 *  \date 2010-12-21
 */

#include "OccluderSource.h"
#include "ViewMap.h"

namespace Freestyle {

class CulledOccluderSource : public OccluderSource
{
	// Disallow copying and assignment
	CulledOccluderSource(const CulledOccluderSource& other);
	CulledOccluderSource& operator=(const CulledOccluderSource& other);

public:
	CulledOccluderSource(const GridHelpers::Transform& transform, WingedEdge& we, ViewMap& viewMap,
	                     bool extensiveFEdgeSearch = true);
	virtual ~CulledOccluderSource();

	void cullViewEdges(ViewMap& viewMap, bool extensiveFEdgeSearch);

	bool next();

	void getOccluderProscenium(real proscenium[4]);

private:
	bool testCurrent();
	void expandGridSpaceOccluderProscenium(FEdge *fe);

	real occluderProscenium[4];
	real gridSpaceOccluderProscenium[4];

	unsigned long rejected;
	bool gridSpaceOccluderProsceniumInitialized;
};

} /* namespace Freestyle */

#endif // __FREESTYLE_CULLED_OCCLUDER_SOURCE_H__
