//implementation of MT_Line3
/////////////////////////////
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

#include "MT_Line3.h"

MT_Line3::MT_Line3()
:
	m_origin(0,0,0),
	m_dir(1,0,0)
{
	m_bounds[0] = false;
	m_bounds[1] = false;

	m_params[0] = 0;
	m_params[1] = 1;
}


MT_Line3::MT_Line3(
	const MT_Point3& p1, 
	const MT_Point3& p2
):
	m_origin(p1),
	m_dir(p2-p1)
{
	m_bounds[0] = true;
	m_bounds[1] = true;
	m_params[0] = 0;
	m_params[1] = 1;
}
	
// construct infinite line from p1 in direction v
MT_Line3::MT_Line3(
	const MT_Point3& p1, 
	const MT_Vector3& v
):
	m_origin(p1),
	m_dir(v)
{
	m_bounds[0] = false;
	m_bounds[1] = false;
	m_params[0] = 0;
	m_params[1] = 1;
}

MT_Line3::MT_Line3(
	const MT_Point3& p1, 
	const MT_Vector3& v, 
	bool bound1, 
	bool bound2
):
	m_origin(p1),
	m_dir(v)
{
	m_bounds[0] = bound1;
	m_bounds[1] = bound2;
	m_params[0] = 0;
	m_params[1] = 1;
}	


