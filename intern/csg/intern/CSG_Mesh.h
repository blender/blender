#ifndef __MESH_H
#define __MESH_H
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
#include <vector>

// Simple vertex polygon container class 
//--------------------------------------

template <typename TPolygon, typename TVertex> class Mesh
{
public :
	typedef std::vector<TVertex> VLIST;
	typedef std::vector<TPolygon> PLIST;	

	typedef TPolygon Polygon;
	typedef TVertex Vertex;
	
	typedef PolygonGeometry<Mesh> TGBinder;

private :

	VLIST m_verts;
	PLIST m_polys;

	MT_Point3 m_bBoxMin;
	MT_Point3 m_bBoxMax;


public :

	Mesh(): 
		m_verts(),
		m_polys()
	{};

	VLIST& Verts() {return m_verts;}	
	const VLIST& Verts() const {return m_verts;}	

	PLIST& Polys() {return m_polys;}
	const PLIST& Polys() const {return m_polys;}
			
	~Mesh() {}
};

#endif
