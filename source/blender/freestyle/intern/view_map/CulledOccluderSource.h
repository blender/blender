//
//  Filename         : CulledOccluderSource.h
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

#ifndef CULLEDOCCLUDERSOURCE_H
#define CULLEDOCCLUDERSOURCE_H

#include "OccluderSource.h"
#include "ViewMap.h"

class CulledOccluderSource : public OccluderSource {
	// Disallow copying and assignment
	CulledOccluderSource (const CulledOccluderSource& other);
	CulledOccluderSource& operator= (const CulledOccluderSource& other);

public:
	CulledOccluderSource (const GridHelpers::Transform& transform, WingedEdge& we, ViewMap& viewMap, bool extensiveFEdgeSearch = true);
	virtual ~CulledOccluderSource();

	void cullViewEdges(ViewMap& viewMap, bool extensiveFEdgeSearch);

	bool next();

	void getOccluderProscenium(real proscenium[4]);

private:
	bool testCurrent();
	void expandGridSpaceOccluderProscenium(FEdge* fe);

	real occluderProscenium[4];
	real gridSpaceOccluderProscenium[4];

	unsigned long rejected;
	bool gridSpaceOccluderProsceniumInitialized;
};

#endif // CULLEDOCCLUDERSOURCE_H
