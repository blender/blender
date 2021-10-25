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

#ifndef __FREESTYLE_OCCLUDER_SOURCE_H__
#define __FREESTYLE_OCCLUDER_SOURCE_H__

/** \file blender/freestyle/intern/view_map/OccluderSource.h
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the projected image of a scene
 *  \author Alexander Beels
 *  \date 2010-12-21
 */

#include "../geometry/GridHelpers.h"

#include "../winged_edge/WEdge.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class OccluderSource
{
	// Disallow copying and assignment
	OccluderSource(const OccluderSource& other);
	OccluderSource& operator=(const OccluderSource& other);

public:
	OccluderSource(const GridHelpers::Transform& transform, WingedEdge& we);
	virtual ~OccluderSource();

	void begin();
	virtual bool next();
	bool isValid();

	WFace *getWFace();
	Polygon3r getCameraSpacePolygon();
	Polygon3r& getGridSpacePolygon();

	virtual void getOccluderProscenium(real proscenium[4]);
	virtual real averageOccluderArea();

protected:
	WingedEdge& wingedEdge;
	vector<WShape*>::const_iterator currentShape, shapesEnd;
	vector<WFace*>::const_iterator currentFace, facesEnd;

	bool valid;

	Polygon3r cachedPolygon;
	const GridHelpers::Transform& transform;

	void buildCachedPolygon();

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:OccluderSource")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_OCCLUDER_SOURCE_H__
