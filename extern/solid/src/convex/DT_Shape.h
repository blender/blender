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

#ifndef DT_SHAPE_H
#define DT_SHAPE_H

#include <algorithm>

#include "MT_BBox.h"

#include "MT_Transform.h"

class DT_Object;

enum DT_ShapeType {
    COMPLEX,
    CONVEX
};

class DT_Shape {
public:
    virtual ~DT_Shape() {}
    virtual DT_ShapeType getType() const = 0;
	virtual MT_BBox bbox(const MT_Transform& t, MT_Scalar margin) const = 0;
	virtual bool ray_cast(const MT_Point3& source, const MT_Point3& target, MT_Scalar& param, MT_Vector3& normal) const = 0;

protected:
	DT_Shape()  {}
};

typedef bool (*Intersect)(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
						  const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
						  MT_Vector3&);

typedef bool (*Common_point)(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
						     const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
			                 MT_Vector3&, MT_Point3&, MT_Point3&);

typedef bool (*Penetration_depth)(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
						          const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
                                  MT_Vector3&, MT_Point3&, MT_Point3&);

typedef MT_Scalar (*Closest_points)(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
						            const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
									MT_Point3&, MT_Point3&);

#endif





