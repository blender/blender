#ifndef CSG_GEOMETRY_BINDER_H
#define CSG_GEOMETRY_BINDER_H
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

// This class binds the geometry of a polygon
// to the polygon itself it provides just one
// operator [int i] that returns the vertex position
// of the ith vertex in the polygon. 

// Its a model of a geometry binder primarily used by the CSG_Math
// template class to compute geometric information about a mesh.

#include "MT_Point3.h"


template <typename TMesh> class PolygonGeometry
{
public :
	
	typedef typename TMesh::Polygon TPolygon;

	PolygonGeometry(const TMesh& mesh, int pIndex)
	: m_poly(mesh.Polys()[pIndex]),m_mesh(mesh)
	{};

	PolygonGeometry(const TMesh& mesh,const TPolygon& poly)
	: m_poly(poly),m_mesh(mesh)
	{};

	const MT_Point3& operator[] (int i) const  {
		return m_mesh.Verts()[m_poly[i]].Pos();
	};

	int Size() const { return m_poly.Size();}

	~PolygonGeometry(){};
	
private :

	PolygonGeometry(const PolygonGeometry& other) {};
	PolygonGeometry& operator = (PolygonGeometry& other) { return *this;}

	const TMesh& m_mesh;
	const TPolygon& m_poly;	

};

#endif





