#ifndef __POLYGON_H
#define __POLYGON_H
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

#include <algorithm>
#include "MT_Plane3.h"
#include "MT_Point3.h"

template <typename AVProp, typename AFProp> class PolygonBase
{
public :

	// The per vertex properties
	typedef AVProp TVProp;

	// The per face properties
	typedef AFProp TFProp;

	typedef std::vector<TVProp> TVPropList;
	typedef TVPropList::iterator TVPropIt;

	// Functions required by the CSG library
	////////////////////////////////////////

	PolygonBase():m_fProp(){}

	const TVPropList& Verts() const { return m_verts;}
	TVPropList& Verts() { return m_verts;}

	int Size() const { return m_verts.size();}

	int operator[](int i) const {return m_verts[i];}

	const TVProp& VertexProps(int i) const { return m_verts[i];}
	TVProp& VertexProps(int i) { return m_verts[i];}

	void SetPlane(const MT_Plane3& plane) { m_plane = plane;}

	const MT_Plane3& Plane() const { return m_plane;}
	MT_Vector3 Normal() const { return m_plane.Normal();}

	int & Classification() { return m_classification;}
	const int& Classification() const { return m_classification;}

	// Reverse this polygon.
	void Reverse() 
	{
		std::reverse(m_verts.begin(),m_verts.end());
		m_plane.Invert();
	}

	// Our functions
	////////////////

	TFProp& FProp(){ return m_fProp;}
	const TFProp& FProp() const { return m_fProp;}

	~PolygonBase() {}
		
private :

	TVPropList m_verts;
	MT_Plane3 m_plane;

	TFProp m_fProp;

	// gross waste of bits! 1 = in, 2 = out;
	int m_classification;

};	

#endif