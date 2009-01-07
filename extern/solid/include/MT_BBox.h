/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#ifndef MT_BBOX_H
#define MT_BBOX_H

#include "MT_Scalar.h"
#include "MT_Point3.h"
#include "MT_Vector3.h"

#include <MT/Tuple3.h> 
#include "MT_Interval.h" 



class MT_BBox : public MT::Tuple3<MT_Interval> {
public:
    MT_BBox() {}
	MT_BBox(const MT_Point3& p)
		: MT::Tuple3<MT_Interval>(MT_Interval(p[0]), 
				                  MT_Interval(p[1]), 
				                  MT_Interval(p[2]))
	{}
	MT_BBox(const MT_Point3& lb, const MT_Point3& ub)
		: MT::Tuple3<MT_Interval>(MT_Interval(lb[0], ub[0]),
				                  MT_Interval(lb[1], ub[1]),
				                  MT_Interval(lb[2], ub[2]))
	{}
	MT_BBox(const MT_Interval& x, const MT_Interval& y, const MT_Interval& z) 
		: MT::Tuple3<MT_Interval>(x, y, z) 
	{}

	MT_Point3 getMin() const 
	{ 
		return MT_Point3(m_co[0].lower(), m_co[1].lower(), m_co[2].lower());
	}

	MT_Point3 getMax() const 
	{ 
		return MT_Point3(m_co[0].upper(), m_co[1].upper(), m_co[2].upper());
	}
	
	MT_Point3 getCenter() const
	{
		return MT_Point3(MT::median(m_co[0]), MT::median(m_co[1]), MT::median(m_co[2]));
	}

	MT_Vector3 getExtent() const
	{
		return MT_Vector3(MT::width(m_co[0]) * MT_Scalar(0.5), MT::width(m_co[1]) * MT_Scalar(0.5), MT::width(m_co[2]) * MT_Scalar(0.5));
	}

	void extend(const MT_Vector3& v) 
	{
		m_co[0] = MT::widen(m_co[0], v[0]);
		m_co[1] = MT::widen(m_co[1], v[1]);
		m_co[2] = MT::widen(m_co[2], v[2]);
	}

    bool overlaps(const MT_BBox& b) const 
	{
        return MT::overlap(m_co[0], b[0]) &&
			   MT::overlap(m_co[1], b[1]) &&
			   MT::overlap(m_co[2], b[2]);
    }

	bool inside(const MT_BBox& b) const 
	{
        return MT::in(m_co[0], b[0]) &&
			   MT::in(m_co[1], b[1]) &&
			   MT::in(m_co[2], b[2]);
    }

	MT_BBox hull(const MT_BBox& b) const 
	{
		return MT_BBox(MT::hull(m_co[0], b[0]), 
					   MT::hull(m_co[1], b[1]), 
					   MT::hull(m_co[2], b[2])); 
	}

	bool contains(const MT_Point3& p) const 
	{
		return MT::in(p[0], m_co[0]) && MT::in(p[1], m_co[1]) && MT::in(p[2], m_co[2]);
	}
};

inline MT_BBox operator+(const MT_BBox& b1, const MT_BBox& b2) 
{
	return MT_BBox(b1[0] + b2[0], b1[1] + b2[1], b1[2] + b2[2]);
}

inline MT_BBox operator-(const MT_BBox& b1, const MT_BBox& b2) 
{
	return MT_BBox(b1[0] - b2[0], b1[1] - b2[1], b1[2] - b2[2]);
}

#endif


