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
#include <algorithm>
#include <iterator>
#include "CSG_SplitFunction.h"

// Define functors things that bind a function to an object,
// for the templated split polygon code.

template <typename CMesh> class SplitFunctionBindor
{
private :
	CMesh& m_mesh;

public :

	SplitFunctionBindor(CMesh& mesh):m_mesh(mesh) {};

	void DisconnectPolygon(int polyIndex){
		m_mesh.DisconnectPolygon(polyIndex);
	}

	void ConnectPolygon(int polygonIndex) {
		m_mesh.ConnectPolygon(polygonIndex);
	}
	
	void InsertVertexAlongEdge(int lastIndex,int newIndex,const CMesh::VProp& prop) {
		m_mesh.InsertVertexAlongEdge(lastIndex,newIndex,prop);
	}

	~SplitFunctionBindor(){};
};


template <typename TMesh> 
	void 
ConnectedMeshWrapper<TMesh>::
BuildVertexPolyLists(
) {
	int i;
	for (i=0; i < Polys().size(); i++) 
	{
		ConnectPolygon(i);
	}
}

template <typename TMesh> 
	void 
ConnectedMeshWrapper<TMesh>::
DisconnectPolygon(
	int polyIndex
){
	const Polygon& poly = Polys()[polyIndex];

	int j;
	for (j=0;j<poly.Verts().size(); j++) 
	{
		Verts()[poly[j]].RemovePolygon(polyIndex);
	}
}
	
template <typename TMesh> 
	void 
ConnectedMeshWrapper<TMesh>::
ConnectPolygon(
	int polyIndex
){
	const Polygon& poly = Polys()[polyIndex];

	int j;
	for (j=0;j<poly.Verts().size(); j++) 
	{
		Verts()[poly[j]].AddPoly(polyIndex);
	}
}

template <typename TMesh> 
	void 
ConnectedMeshWrapper<TMesh>::
EdgePolygons(
	int v1, 
	int v2, 
	PIndexList& polys
) {
	++m_uniqueEdgeTestId;

	Vertex& vb1 = Verts()[v1];
	int i;
	for (i=0;i < vb1.Polys().size(); ++i)
	{
		Polys()[vb1[i]].Classification() = m_uniqueEdgeTestId;
	}

	Vertex& vb2 = Verts()[v2];
	int j;
	for (j=0;j < vb2.Polys().size(); ++j)
	{
		if (Polys()[vb2[j]].Classification() == m_uniqueEdgeTestId) 
		{
			polys.push_back(vb2[j]);
		}
	}
}

template <typename TMesh> 
	void 
ConnectedMeshWrapper<TMesh>::
InsertVertexAlongEdge(
	int v1,
	int v2, 
	const VProp& prop
) {

	PIndexList npolys;
	EdgePolygons(v1,v2,npolys);

	// iterate through the neighbouting polygons of
	// this edge and insert the vertex into the polygon
	
	int newVertex = int(prop);

	int i;
	for (i=0;i < npolys.size(); i++) 
	{
		// find the first vertex index in this polygon
		Polygon::TVPropList& polyVerts = Polys()[npolys[i]].Verts();

		Polygon::TVPropIt v1pos = std::find(polyVerts.begin(),polyVerts.end(),v1);
	
		// this should never be false!
		if (v1pos != polyVerts.end())
		{	
			// v2 must be either side of this pos
			Polygon::TVPropIt prevPos = (v1pos == polyVerts.begin()) ? polyVerts.end()-1 : v1pos-1;
			Polygon::TVPropIt nextPos = (v1pos == polyVerts.end()-1) ? polyVerts.begin() : v1pos+1;

			if (*prevPos == v2) {
				polyVerts.insert(v1pos,prop);
			} else 
			if (*nextPos == v2) {
				polyVerts.insert(nextPos,prop);
			} else {
				//assert(false);
			}
	
			Verts()[newVertex].AddPoly(npolys[i]);
		} else {
			assert(false);
		}
	}	
}


template <typename TMesh> 
	void 
ConnectedMeshWrapper<TMesh>::
SplitPolygon(
	const int p1Index,
	const MT_Plane3& plane,
	int& inPiece,
	int& outPiece,
	const MT_Scalar onEpsilon
){

	SplitFunctionBindor<MyType> functionBindor(*this);

	SplitFunction<MyType,SplitFunctionBindor<MyType> > splitFunction(*this,functionBindor);
	splitFunction.SplitPolygon(p1Index,plane,inPiece,outPiece,onEpsilon);
}	
		
#if 0	
template <class TPolygon, class TVertex> 
	void 
Mesh::
printMesh(
	std::ostream& stream
) {

	int i;
	for (i =0; i < m_polys.size(); i++) 
	{
		std::ostream_iterator<int> streamIt(stream," ");
		std::copy(m_polys[i].Verts().begin(),m_polys[i].Verts().end(),streamIt);
		stream << "\n";
	}
}

#endif




