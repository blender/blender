#ifndef CSG_CVertex_H
#define CSG_CVertex_H
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
#include "CSG_IndexDefs.h"
#include "CSG_Vertex.h"
// This class extends an existing vertex by connecting
// them with their polygons through the PIndexList member.
//
// This extra information allows us to perform local 
// mesh topology queries
//
// These queries are availble through the ConnectedMesh class.

class CVertex : public VertexBase
{
private : 	

	// polygons using this vertex
	PIndexList m_polygons;

public :

	CVertex()
	:VertexBase(),m_polygons()
	{
	};
	
	// have to change VertexBase and all functions to 
	// use Pos() rather than m_pos;

	CVertex(const VertexBase& vertex)
	:VertexBase(vertex),m_polygons()
	{}

	// Value type model
	///////////////////
	CVertex(const CVertex& other)
	: VertexBase(other), m_polygons(other.m_polygons)
	{}

	CVertex& operator = (const CVertex& other) 
	{
		m_pos = other.Pos();
		m_vertexMap = other.m_vertexMap;
		m_polygons = other.m_polygons;

		return *this;
	}


	CVertex& operator = (const VertexBase& other)
	{
		m_pos= other.Pos();
		return *this;
	}


	~CVertex(){};
	
	// Our special connected vertex functions.
	//////////////////////////////////////////
	
	const PIndexList& Polys() const { return m_polygons;}
	PIndexList& Polys() { return m_polygons;}

	int & operator[] (const int & i) { return m_polygons[i];}
	
	const int & operator[] (const int& i) const { return m_polygons[i];}

	void AddPoly(int polyIndex) {m_polygons.push_back(polyIndex);}

	void RemovePolygon(int polyIndex) 
	{
		PIndexIt foundIt = std::find(m_polygons.begin(),m_polygons.end(),polyIndex);
		if (foundIt != m_polygons.end()) {
			std::swap(m_polygons.back(), *foundIt);
			m_polygons.pop_back();
		}
	}
};


#endif
