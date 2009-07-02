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

#ifndef DT_CONVEX_H
#define DT_CONVEX_H

#include "DT_Shape.h"

#include "MT_Vector3.h"
#include "MT_Point3.h"

#include "MT_Matrix3x3.h"
#include "MT_Transform.h"

class DT_Convex : public DT_Shape {
public:
    virtual ~DT_Convex() {}
	virtual DT_ShapeType getType() const { return CONVEX; } 
    
	virtual MT_Scalar supportH(const MT_Vector3& v) const {	return v.dot(support(v)); }
    virtual MT_Point3 support(const MT_Vector3& v) const = 0;
	virtual MT_BBox bbox() const;
    virtual MT_BBox bbox(const MT_Matrix3x3& basis) const;
    virtual MT_BBox bbox(const MT_Transform& t, MT_Scalar margin = MT_Scalar(0.0)) const;
	virtual bool ray_cast(const MT_Point3& source, const MT_Point3& target, MT_Scalar& param, MT_Vector3& normal) const;
	
protected:
	DT_Convex() {}
};


bool intersect(const DT_Convex& a, const DT_Convex& b, MT_Vector3& v);

bool common_point(const DT_Convex& a, const DT_Convex& b, MT_Vector3& v, MT_Point3& pa, MT_Point3& pb);

MT_Scalar closest_points(const DT_Convex&, const DT_Convex&, MT_Scalar max_dist2, MT_Point3& pa, MT_Point3& pb);

bool penetration_depth(const DT_Convex& a, const DT_Convex& b, MT_Vector3& v, MT_Point3& pa, MT_Point3& pb);

bool hybrid_penetration_depth(const DT_Convex& a, MT_Scalar a_margin, 
							  const DT_Convex& b, MT_Scalar b_margin,
                              MT_Vector3& v, MT_Point3& pa, MT_Point3& pb);

#endif
