//
//  Filename         : OccluderSource.h
//  Author(s)        : Alexander Beels
//  Purpose          : Class to define a cell grid surrounding
//                     the projected image of a scene
//  Date of creation : 2010-12-21
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

#ifndef OCCLUDERSOURCE_H
#define OCCLUDERSOURCE_H

#include "../winged_edge/WEdge.h"
#include "../geometry/GridHelpers.h"

class OccluderSource {
	// Disallow copying and assignment
	OccluderSource (const OccluderSource& other);
	OccluderSource& operator= (const OccluderSource& other);

public:
	OccluderSource (const GridHelpers::Transform& transform, WingedEdge& we);
	virtual ~OccluderSource();

	void begin();
	virtual bool next();
	bool isValid();

	WFace* getWFace();
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
};

#endif // OCCLUDERSOURCE_H
