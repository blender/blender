#ifndef __VERTEX_H
#define __VERTEX_H
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

#include "MT_Point3.h"
#include <algorithm>
#include "CSG_IndexDefs.h"


class VertexBase
{
protected:

	// Map to this vertex position in the classified Mesh
	// or -1 if this vertex has not yet been used.
	int m_vertexMap;

	MT_Point3 m_pos;
	
public :

	VertexBase():m_vertexMap(-1) {};

	// Regular vertex model
	//////////////////////
	const MT_Point3& Pos() const { return m_pos;}
	MT_Point3& Pos() {return m_pos;}

	int & VertexMap() { return m_vertexMap;}
	const int & VertexMap() const { return m_vertexMap;}
 
	~VertexBase(){};
};

#endif


