#ifndef __CGS_LINE_H
#define __CGS_LINE_H
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
#include "MT_Vector3.h"

class MT_Line3
{
public :

	MT_Line3();

	// construct closed line segment from p1 -> p2
	MT_Line3(const MT_Point3& p1, const MT_Point3& p2);
	
	// construct infinite line from p1 in direction v
	MT_Line3(const MT_Point3& p1, const MT_Vector3& v);

	MT_Line3(const MT_Point3& p1, const MT_Vector3& v, bool bound1, bool bound2);

	// return an infinite ray from the point p1 in the direction v
	static MT_Line3 InfiniteRay(const MT_Point3& p1, const MT_Vector3& v);

	// return direction of line
	const MT_Vector3& Direction() const { return m_dir;}

	// return a point on the line
	const MT_Point3& Origin() const { return m_origin;}

	bool Bounds(int i) const {
		return (i == 0 ? m_bounds[0] : m_bounds[1]);
	}

	bool & Bounds(int i) {
		return (i == 0 ? m_bounds[0] : m_bounds[1]);
	}

	const MT_Scalar& Param(int i) const {
		return (i == 0 ? m_params[0] : m_params[1]);
	}

	MT_Scalar& Param(int i){
		return (i == 0 ? m_params[0] : m_params[1]);
	}

	// Return the  smallest Vector from the point to the line
	// does not take into account bounds of line.
	MT_Vector3 UnboundSmallestVector(const MT_Point3& point) const
	{
		MT_Vector3 diff(m_origin-point);
		return diff - m_dir * diff.dot(m_dir);
	}

	// Return the closest parameter of the line to the 
	// point.
	MT_Scalar UnboundClosestParameter(const MT_Point3& point) const
	{
		MT_Vector3 diff(m_origin-point);
		return diff.dot(m_dir);
	}

	MT_Scalar UnboundDistance(const MT_Point3& point) const 
	{
		return UnboundSmallestVector(point).length();
	}

	// Return true if the line parameter t is actually within the line bounds.
	bool IsParameterOnLine(const MT_Scalar &t) const 
	{
		return ((m_params[0]-MT_EPSILON < t) || (!m_bounds[0])) && ((m_params[1] > t+MT_EPSILON) || (!m_bounds[1]));
//		return ((m_params[0] < t) || (!m_bounds[0])) && ((m_params[1] > t ) || (!m_bounds[1]));
	} 


private :

	bool m_bounds[2];

	MT_Scalar m_params[2];
	MT_Point3 m_origin;
	MT_Vector3 m_dir;
	
};			
	




#endif