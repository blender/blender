/*
The following BBox class is a lightly modfied version of what
is found in Free Solid 2.0
*/

/*
  SOLID - Software Library for Interference Detection
  Copyright (C) 1997-1998  Gino van den Bergen

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

  Please send remarks, questions and bug reports to gino@win.tue.nl,
  or write to:
  Gino van den Bergen
  Department of Mathematics and Computing Science
  Eindhoven University of Technology
  P.O. Box 513, 5600 MB Eindhoven, The Netherlands
*/

#ifndef _BBOX_H_
#define _BBOX_H_

#include "MT_Point3.h"
#include "MT_Vector3.h"
#include "MT_MinMax.h"

class BBox {
public:
	BBox() {} 

	BBox(
		const MT_Point3& mini, 
		const MT_Point3& maxi
	) { SetValue(mini,maxi); }

		const MT_Point3& 
	Center(
	) const { 
		return m_center; 
	}

		const MT_Vector3& 
	Extent(
	) const { 
		return m_extent; 
	}
  
		MT_Point3& 
	Center(
	) { 
		return m_center; 
	}

		MT_Vector3& 
	Extent(
	) { 
		return m_extent; 
	}

		void 
	SetValue(
		const MT_Point3& mini,
		const MT_Point3& maxi
	) { 
		m_extent = (maxi-mini)/2;
		m_center = mini+m_extent;
	}

		void 
	Enclose(
		const BBox& a, 
		const BBox& b
	) {
		MT_Point3 lower(
			MT_min(a.Lower(0), b.Lower(0)),
			MT_min(a.Lower(1), b.Lower(1)),
			MT_min(a.Lower(2), b.Lower(2))
		);
		MT_Point3 upper(
			MT_max(a.Upper(0), b.Upper(0)),
			MT_max(a.Upper(1), b.Upper(1)),
			MT_max(a.Upper(2), b.Upper(2))
		);
		SetValue(lower, upper);
	}

		void 
	SetEmpty() { 
		m_center.setValue(0, 0, 0); 
		m_extent.setValue(-MT_INFINITY,-MT_INFINITY,-MT_INFINITY);
	}

		void 
	Include (
		const MT_Point3& p
	) {
		MT_Point3 lower(
			MT_min(Lower(0), p[0]),
			MT_min(Lower(1), p[1]),
			MT_min(Lower(2), p[2])
		);
		MT_Point3 upper(
			MT_max(Upper(0), p[0]),
			MT_max(Upper(1), p[1]),
			MT_max(Upper(2), p[2])
		);
		SetValue(lower, upper);
	}

		void 
	Include (
		const BBox& b
	) { 
		Enclose(*this, b); 
	}

		MT_Scalar 
	Lower(
		int i
	) const { 
		return m_center[i] - m_extent[i]; 
	}
		MT_Scalar 
	Upper(
		int i
	) const { 
		return m_center[i] + m_extent[i]; 
	}

		MT_Point3
	Lower(
	) const { 
		return m_center - m_extent; 
	}
		MT_Point3 
	Upper(
	) const { 
		return m_center + m_extent; 
	}

		MT_Scalar 
	Size(
	) const { 
		return MT_max(MT_max(m_extent[0], m_extent[1]), m_extent[2]); 
	}

		int 
	LongestAxis(
	) const { 
		return m_extent.closestAxis(); 
	}

		bool
	IntersectXRay(
		const MT_Point3& xBase
	) const {
		if (xBase[0] <= Upper(0))
		{
			if (xBase[1] <= Upper(1) && xBase[1] >= Lower(1))
			{
				if (xBase[2] <= Upper(2) && xBase[2] >= Lower(2))
				{
					return true;
				}
			}
		}
		return false;
	}
 



	friend bool intersect(const BBox& a, const BBox& b);

private:
	MT_Point3 m_center;
	MT_Vector3 m_extent;
};

inline 
	bool 
intersect(
	const BBox& a, 
	const BBox& b
) {
	return 
		MT_abs(a.m_center[0] - b.m_center[0]) <= a.m_extent[0] + b.m_extent[0] &&
		MT_abs(a.m_center[1] - b.m_center[1]) <= a.m_extent[1] + b.m_extent[1] &&
		MT_abs(a.m_center[2] - b.m_center[2]) <= a.m_extent[2] + b.m_extent[2];
}

#endif


