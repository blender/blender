#ifndef CSG_ConnectedMesh_H
#define CSG_ConnectedMesh_H
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

#include "CSG_GeometryBinder.h"
#include "MT_Plane3.h"

template <typename TMesh> class ConnectedMeshWrapper
{
public :

	typedef TMesh::Polygon Polygon;
	typedef TMesh::Vertex Vertex;

	typedef TMesh::Polygon::TVProp VProp;

	typedef TMesh::VLIST VLIST;
	typedef TMesh::PLIST PLIST;

	typedef PolygonGeometry<ConnectedMeshWrapper> TGBinder;

	typedef ConnectedMeshWrapper<TMesh> MyType;

private :
	TMesh& m_mesh;

	unsigned int m_uniqueEdgeTestId;	

public :

	// Mesh Template interface
	//////////////////////////
	VLIST& Verts() {return m_mesh.Verts();}	
	const VLIST& Verts() const {return m_mesh.Verts();}	

	PLIST& Polys() {return m_mesh.Polys();}
	const PLIST& Polys() const {return m_mesh.Polys();}

	// Mesh Wrapper functions
	/////////////////////////	

	ConnectedMeshWrapper(TMesh& mesh):
		m_mesh(mesh),m_uniqueEdgeTestId(0) {};
	
	void BuildVertexPolyLists();

	void DisconnectPolygon(int polyIndex);

	void ConnectPolygon(int polyIndex);

	//return the polygons neibouring the given edge.
	void EdgePolygons(int v1, int v2, PIndexList& polys);

	void InsertVertexAlongEdge(int v1,int v2, const VProp& prop);

		void 
	SplitPolygon(
		const int p1Index,
		const MT_Plane3& plane,
		int& inPiece,
		int& outPiece,
		const MT_Scalar onEpsilon
	);

	~ConnectedMeshWrapper(){};
};

#include "CSG_ConnectedMeshWrapper.inl"

#endif

	



	
