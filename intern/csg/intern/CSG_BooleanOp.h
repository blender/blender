#ifndef CSG_BOOLEANOP_H
#define CSG_BOOLEANOP_H
/*
  CSGLib - Software Library for Constructive Solid Geometry
  Copyright (C) 2003-2004  Laurence Bourn

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  Please send remarks, questions and bug reports to laurencebourn@hotmail.com
*/

#include "CSG_IndexDefs.h"
#include "CSG_BBoxTree.h"

template <typename CMesh, typename TMesh> class BooleanOp
{
public :

	// unimplemented
	////////////////
	BooleanOp();
	BooleanOp(const BooleanOp&);
	BooleanOp& operator = (const BooleanOp&);

	//helpers
	/////////

	static
		void
	BuildSplitGroup(
		const TMesh& meshA,
		const TMesh& meshB,
		const BBoxTree& treeA,
		const BBoxTree& treeB,
		OverlapTable& aOverlapsB,
		OverlapTable& bOverlapsA
	);		

	// Split mesh with mesh2, table is an OverlapTable containing the polygons
	// of mesh2 that intersect with polygons of mesh.
	// if preserve is true this function will aim to reduce the number of 
	// T-junctions created. 

	static
		void	
	PartitionMesh(
		CMesh & mesh, 
		const TMesh & mesh2,
		const OverlapTable& table
	);

	// Classify meshB with respect to meshA, uses a BBox tree of meshA
	// to drastically improve speed!
	static
		void	
	ClassifyMesh(
		const TMesh& meshA,
		const BBoxTree& aTree,
		CMesh& meshB
	);
	
	static
		void
	ExtractClassification(
		CMesh& meshA,
		TMesh& newMesh,
		int classification,
		bool reverse
	);


};

#include "CSG_BooleanOp.inl"

#endif
