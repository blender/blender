#ifndef CSG_MESH_WRAPPER_H
#define CSG_MESH_WRAPPER_H
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

// This class wraps a mesh providing 
// simple mesh functions required by the CSG library.
// TMesh is a model of a mesh and as such should have the 
// following public typedefs and functions
//
// TMesh::Vertex the vertex type 
// TMesh::Polygon the polygon type.
// TMesh::VLIST an stl container of Vertex objects
// TMesh::PLIST an stl container of Polygon objects
//
//	VLIST& Verts();	
//	const VLIST& Verts();
//
//	PLIST& Polys();
//	const PLIST& Polys();

#include "CSG_GeometryBinder.h"
#include "MT_Transform.h"
#include "CSG_BBox.h"

#include <vector>

template <typename TMesh> class MeshWrapper
{
public :

	typedef TMesh::Polygon Polygon;
	typedef TMesh::Vertex Vertex;

	typedef TMesh::VLIST VLIST;
	typedef TMesh::PLIST PLIST;

	typedef PolygonGeometry<MeshWrapper> TGBinder;
	
	typedef MeshWrapper<TMesh> MyType;

private :
	TMesh& m_mesh;
	
public :

	// Mesh Template interface
	//////////////////////////

	VLIST& Verts() {return m_mesh.Verts();}	
	const VLIST& Verts() const {return m_mesh.Verts();}	

	PLIST& Polys() {return m_mesh.Polys();}
	const PLIST& Polys() const {return m_mesh.Polys();}

	// Mesh Wrapper functions
	/////////////////////////	
	
	MeshWrapper(TMesh& mesh)
	: m_mesh(mesh)
	{}

	void ComputePlanes();
	
	void BurnTransform(const MT_Transform& t);

	BBox ComputeBBox() const;

	// Triangulate this mesh - does not preserve vertex->polygon information.
	void Triangulate();

	// Split the polygon at position p1Index in the mesh mesh1 into 2 pieces. 
	// Remove the polygon p1 and add the 2 pieces (along with the 2 new vertices)
	// to the mesh. Returns the index of the 2 new pieces in the mesh.(either of [but never both] which can be
	// -1 if there was nothing to split.

	void SplitPolygon(
		const int p1Index,
		const MT_Plane3& plane,
		int& inPiece,
		int& outPiece,
		const MT_Scalar onEpsilon
	);	


	~MeshWrapper(){};
	
};

#include "CSG_MeshWrapper.inl"

#endif