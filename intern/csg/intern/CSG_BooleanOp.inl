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
#include "CSG_Math.h"
#include "CSG_BBoxTree.h"
#include "CSG_TreeQueries.h"

using namespace std;

template <typename CMesh, typename TMesh>
	void 
BooleanOp<CMesh,TMesh>::
BuildSplitGroup(
	const TMesh& meshA,
	const TMesh& meshB,
	const BBoxTree& treeA,
	const BBoxTree& treeB,
	OverlapTable& aOverlapsB,
	OverlapTable& bOverlapsA
) {
	
	aOverlapsB = OverlapTable(meshB.Polys().size());
	bOverlapsA = OverlapTable(meshA.Polys().size());

	// iterate through the polygons of A and then B
	// and mark each 	
	TreeIntersector<TMesh>(treeA,treeB,&aOverlapsB,&bOverlapsA,&meshA,&meshB);

}		

template <typename CMesh, typename TMesh>
	void 
BooleanOp<CMesh,TMesh>::
PartitionMesh(
	CMesh & mesh, 
	const TMesh & mesh2,
	const OverlapTable& table
) {
	
	// iterate through the overlap table.
	int i;
	
	MT_Scalar onEpsilon(1e-4);

	for (i = 0; i < table.size(); i++)
	{
		if (table[i].size())
		{
			// current list of fragments - initially contains
			// just the to-be-split polygon index 
			PIndexList fragments;
			fragments.push_back(i);

			// iterate through the splitting polygons.
			int j;
			for (j =0 ; j <table[i].size(); ++j) 
			{
				PIndexList newFragments;

				// find the splitting plane
				MT_Plane3 splitPlane = mesh2.Polys()[table[i][j]].Plane();
				
				// iterate through the current fragments and split them	
				// with the new plane, adding the resulting fragments to 
				// the newFragments list.
				int k;
				for (k = 0; k < fragments.size(); ++k) 
				{
					int newInFragment;
					int newOutFragment;

					CMesh::TGBinder pg1(mesh,fragments[k]);
					TMesh::TGBinder pg2(mesh2,table[i][j]);
	
					const MT_Plane3& fragPlane = mesh.Polys()[fragments[k]].Plane();

					if (CSG_PolygonIntersector<CMesh::TGBinder,TMesh::TGBinder >::
						IntersectPolygons(pg1,pg2,fragPlane,splitPlane)) 
					{
						mesh.SplitPolygon(fragments[k],splitPlane,newInFragment,newOutFragment,onEpsilon);

						if (-1 != newInFragment) newFragments.push_back(newInFragment);
						if (-1 != newOutFragment) newFragments.push_back(newOutFragment);
					} else {
						// this fragment was not split by this polygon but it may be split by a subsequent 
						// polygon in the list
						newFragments.push_back(fragments[k]);
					}

				}	
							
				fragments = newFragments;
			}
		}
	}
}
	
template <typename CMesh, typename TMesh>
	void
BooleanOp<CMesh,TMesh>::
ClassifyMesh(
	const TMesh& meshA,
	const BBoxTree& aTree,
	CMesh& meshB
)  {

	// walk through all of the polygons of meshB. Create a 
	// ray in the direction of the polygons normal emintaing from the
	// mid point of the polygon
	
	// Do a ray test with all of the polygons of MeshA 
	// Find the nearest intersection point and record the polygon index

	// If there were no intersections then the ray is outside.
	// If there was an intersection and the dot product of the ray and normal
	// of the intersected polygon from meshA is +ve then we are on the inside
	// else outside.

	int i;
	for (i = 0; i < meshB.Polys().size(); i++) 
	{
		CMesh::TGBinder pg(meshB,i);

		MT_Line3 midPointRay = CSG_Math<CMesh::TGBinder >::PolygonMidPointRay(pg,meshB.Polys()[i].Plane());
		
		MT_Line3 midPointXRay(midPointRay.Origin(),MT_Vector3(1,0,0)); 	

		int aPolyIndex(-1);

		RayTreeIntersector<TMesh>(aTree,&meshA,midPointXRay,aPolyIndex);

		if (-1 != aPolyIndex)
		{
			if (meshA.Polys()[aPolyIndex].Plane().signedDistance(midPointXRay.Origin()) < 0)
			{
				meshB.Polys()[i].Classification()= 1;
			} else {
				meshB.Polys()[i].Classification()= 2;
			}
		} else {
			meshB.Polys()[i].Classification()= 2;
		}
	}
}		
		
	
template <typename CMesh, typename TMesh>
	void
BooleanOp<CMesh,TMesh>::
ExtractClassification(
	CMesh& meshA,
	TMesh& newMesh,
	int classification,
	bool reverse
){

	int i;
	for (i = 0; i < meshA.Polys().size(); ++i)
	{
		CMesh::Polygon& meshAPolygon = meshA.Polys()[i];
		if (meshAPolygon.Classification() == classification) 
		{
			newMesh.Polys().push_back(meshAPolygon);	
			TMesh::Polygon& newPolygon = newMesh.Polys().back();

			if (reverse) newPolygon.Reverse();

			// iterate through the vertices of the new polygon
			// and find a new place for them in the new mesh (if they arent already there)

			int j;
			for (j=0; j< newPolygon.Size(); j++) 
			{
				if (meshA.Verts()[newPolygon[j]].VertexMap() == -1) 
				{
					// this is the first time we have visited this vertex 
					// copy it over to the new mesh.
					newMesh.Verts().push_back(meshA.Verts()[newPolygon[j]]);
					// and record it's position in the new mesh for the next time we visit it.
					meshA.Verts()[newPolygon[j]].VertexMap() = newMesh.Verts().size() -1;
				}
				newPolygon.VertexProps(j) = meshA.Verts()[newPolygon[j]].VertexMap();
			}
		}
	}
}




















	











