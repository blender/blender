#ifndef CSG_BlenderVProp_H
#define CSG_BlenderVProp_H
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

// A vertex property that stores a CSG_IFaceVertexData structure defined
// in the interface between the CSG module and blender CSG_Interface.h!

#include "CSG_Interface.h"
#include "MT_Scalar.h"

class BlenderVProp
{
public :
	// You must set the interpolation function ptr
	// before using this class.

	static CSG_InterpolateUserFaceVertexDataFunc InterpFunc;

private :

	CSG_IFaceVertexData m_data;

public :

	BlenderVProp(const int& vIndex)
	{
		m_data.m_vertexIndex = vIndex;
	}

	BlenderVProp(
		const int& vIndex, 
		const BlenderVProp& p1, 
		const BlenderVProp& p2, 
		const MT_Scalar& epsilon
	){
		m_data.m_vertexIndex = vIndex;
		InterpFunc(&(p1.m_data),&(p2.m_data),&m_data,epsilon);
	}


	BlenderVProp(
	) {};

	// Default copy constructor and assignment operator are fine.

	// Support conversion to an integer
	///////////////////////////////////
	operator int(
	) const { 
		return m_data.m_vertexIndex;
	}

	// and assignment from an integer.
	//////////////////////////////////
		BlenderVProp& 
	operator = (
		int i
	) { 
		m_data.m_vertexIndex = i;
		return *this;
	}

	// return a reference to our data
	const CSG_IFaceVertexData& Data() const {
		return m_data;
	}
	
	CSG_IFaceVertexData& Data() {
		return m_data;
	}


};

#endif