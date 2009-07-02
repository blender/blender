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

#include "DT_Object.h"
#include "DT_AlgoTable.h"
#include "DT_Convex.h" 
#include "DT_Complex.h" 
#include "DT_LineSegment.h" 
#include "DT_Transform.h"
#include "DT_Minkowski.h"
#include "DT_Sphere.h"

void DT_Object::setBBox() 
{
	m_bbox = m_shape.bbox(m_xform, m_margin); 
	DT_Vector3 min, max;
	m_bbox.getMin().getValue(min);
	m_bbox.getMax().getValue(max);
	
	T_ProxyList::const_iterator it;
	for (it = m_proxies.begin(); it != m_proxies.end(); ++it) 
	{
		BP_SetBBox(*it, min, max);
	}
}

bool DT_Object::ray_cast(const MT_Point3& source, const MT_Point3& target, 
						 MT_Scalar& lambda, MT_Vector3& normal) const 
{	
	MT_Transform inv_xform = m_xform.inverse();
	MT_Point3 local_source = inv_xform(source);
	MT_Point3 local_target = inv_xform(target);
	MT_Vector3 local_normal;

	bool result = m_shape.ray_cast(local_source, local_target, lambda, local_normal);
    	
	if (result) 
	{
		normal = local_normal * inv_xform.getBasis();
      MT_Scalar len = normal.length();
		if (len > MT_Scalar(0.0))
      {
         normal /= len;
      }
	}

	return result;
}


typedef AlgoTable<Intersect> IntersectTable;
typedef AlgoTable<Common_point> Common_pointTable;
typedef AlgoTable<Penetration_depth> Penetration_depthTable;
typedef AlgoTable<Closest_points> Closest_pointsTable;


bool intersectConvexConvex(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
						   const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
                           MT_Vector3& v) 
{
	DT_Transform ta(a2w, (const DT_Convex&)a);
	DT_Transform tb(b2w, (const DT_Convex&)b);
    return intersect((a_margin > MT_Scalar(0.0) ? static_cast<const DT_Convex&>(DT_Minkowski(ta, DT_Sphere(a_margin))) : static_cast<const DT_Convex&>(ta)), 
			         (b_margin > MT_Scalar(0.0) ? static_cast<const DT_Convex&>(DT_Minkowski(tb, DT_Sphere(b_margin))) : static_cast<const DT_Convex&>(tb)), v);
}

bool intersectComplexConvex(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
						    const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
                            MT_Vector3& v) 
{
	if (a.getType() == COMPLEX)
	{
		DT_Transform tb(b2w, (const DT_Convex&)b);
		return intersect((const DT_Complex&)a, a2w, a_margin, 
		             (b_margin > MT_Scalar(0.0) ? static_cast<const DT_Convex&>(DT_Minkowski(tb, DT_Sphere(b_margin))) : static_cast<const DT_Convex&>(tb)), v);
	}
	
	bool r = intersectComplexConvex(b, b2w, b_margin, a, a2w, a_margin, v);
	v *= -1.;
	return r;
}

bool intersectComplexComplex(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
							 const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
                             MT_Vector3& v) 
{
    return intersect((const DT_Complex&)a, a2w, a_margin, 
					 (const DT_Complex&)b, b2w, b_margin, v);
}

IntersectTable *intersectInitialize() 
{
    IntersectTable *p = new IntersectTable;
    p->addEntry(COMPLEX, COMPLEX, intersectComplexComplex);
    p->addEntry(COMPLEX, CONVEX, intersectComplexConvex);
    p->addEntry(CONVEX, CONVEX, intersectConvexConvex);
    return p;
}

bool intersect(const DT_Object& a, const DT_Object& b, MT_Vector3& v) 
{
    static IntersectTable *intersectTable = intersectInitialize();
    Intersect intersect = intersectTable->lookup(a.getType(), b.getType());
    return intersect(a.m_shape, a.m_xform, a.m_margin, 
		             b.m_shape, b.m_xform, b.m_margin, v);
}

bool common_pointConvexConvex(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
							  const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
							  MT_Vector3& v, MT_Point3& pa, MT_Point3& pb) 
{
	DT_Transform ta(a2w, (const DT_Convex&)a);
	DT_Transform tb(b2w, (const DT_Convex&)b);
    return common_point((a_margin > MT_Scalar(0.0) ? static_cast<const DT_Convex&>(DT_Minkowski(ta, DT_Sphere(a_margin))) : static_cast<const DT_Convex&>(ta)), 
						(b_margin > MT_Scalar(0.0) ? static_cast<const DT_Convex&>(DT_Minkowski(tb, DT_Sphere(b_margin))) : static_cast<const DT_Convex&>(tb)), v, pa, pb);
}

bool common_pointComplexConvex(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
							   const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
							   MT_Vector3& v, MT_Point3& pa, MT_Point3& pb) 
{
	if (a.getType() == COMPLEX)
	{
		DT_Transform tb(b2w, (const DT_Convex&)b);
		return common_point((const DT_Complex&)a, a2w, a_margin,
					(b_margin > MT_Scalar(0.0) ? static_cast<const DT_Convex&>(DT_Minkowski(tb, DT_Sphere(b_margin))) : static_cast<const DT_Convex&>(tb)), v, pa, pb);
	}
	
	bool r = common_pointComplexConvex(b, b2w, b_margin, a, a2w, a_margin, v, pb, pa);
	v *= -1.;
	return r;
}

bool common_pointComplexComplex(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
								const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
								MT_Vector3& v, MT_Point3& pa, MT_Point3& pb) 
{
    return common_point((const DT_Complex&)a, a2w, a_margin, 
						(const DT_Complex&)b, b2w, b_margin, v, pa, pb);
}

Common_pointTable *common_pointInitialize() 
{
    Common_pointTable *p = new Common_pointTable;
    p->addEntry(COMPLEX, COMPLEX, common_pointComplexComplex);
    p->addEntry(COMPLEX, CONVEX, common_pointComplexConvex);
    p->addEntry(CONVEX, CONVEX, common_pointConvexConvex);
    return p;
}

bool common_point(const DT_Object& a, const DT_Object& b, MT_Vector3& v, MT_Point3& pa, MT_Point3& pb) 
{
    static Common_pointTable *common_pointTable = common_pointInitialize();
    Common_point common_point = common_pointTable->lookup(a.getType(), b.getType());
    return common_point(a.m_shape, a.m_xform, a.m_margin, 
						b.m_shape, b.m_xform, b.m_margin, v, pa, pb);
}



bool penetration_depthConvexConvex(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
								   const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
                                   MT_Vector3& v, MT_Point3& pa, MT_Point3& pb) 
{
    return hybrid_penetration_depth(DT_Transform(a2w, (const DT_Convex&)a), a_margin, 
									DT_Transform(b2w, (const DT_Convex&)b), b_margin, v, pa, pb);
}

bool penetration_depthComplexConvex(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
									const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
                                    MT_Vector3& v, MT_Point3& pa, MT_Point3& pb) 
{
    if (a.getType() == COMPLEX)
    	return penetration_depth((const DT_Complex&)a, a2w, a_margin,
							 DT_Transform(b2w, (const DT_Convex&)b), b_margin, v, pa, pb);

    bool r = penetration_depthComplexConvex(b, b2w, b_margin, a, a2w, a_margin, v, pb, pa);
    v *= -1.;
    return r;
}

bool penetration_depthComplexComplex(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
									 const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
                                     MT_Vector3& v, MT_Point3& pa, MT_Point3& pb) 
{
    return penetration_depth((const DT_Complex&)a, a2w, a_margin, (const DT_Complex&)b, b2w, b_margin, v, pa, pb);
}

Penetration_depthTable *penetration_depthInitialize() 
{
    Penetration_depthTable *p = new Penetration_depthTable;
    p->addEntry(COMPLEX, COMPLEX, penetration_depthComplexComplex);
    p->addEntry(COMPLEX, CONVEX, penetration_depthComplexConvex);
    p->addEntry(CONVEX, CONVEX, penetration_depthConvexConvex);
    return p;
}

bool penetration_depth(const DT_Object& a, const DT_Object& b, MT_Vector3& v, MT_Point3& pa, MT_Point3& pb) 
{
    static Penetration_depthTable *penetration_depthTable = penetration_depthInitialize();
    Penetration_depth penetration_depth = penetration_depthTable->lookup(a.getType(), b.getType());
    return penetration_depth(a.m_shape, a.m_xform, a.m_margin, 
		                     b.m_shape, b.m_xform, b.m_margin, v, pa, pb);
}


MT_Scalar closest_pointsConvexConvex(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
									 const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
									 MT_Point3& pa, MT_Point3& pb)
{
	DT_Transform ta(a2w, (const DT_Convex&)a);
	DT_Transform tb(b2w, (const DT_Convex&)b);
    return closest_points((a_margin > MT_Scalar(0.0) ? static_cast<const DT_Convex&>(DT_Minkowski(ta, DT_Sphere(a_margin))) : static_cast<const DT_Convex&>(ta)), 
						  (b_margin > MT_Scalar(0.0) ? static_cast<const DT_Convex&>(DT_Minkowski(tb, DT_Sphere(b_margin))) : static_cast<const DT_Convex&>(tb)), MT_INFINITY, pa, pb);
}

MT_Scalar closest_pointsComplexConvex(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
									  const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
									  MT_Point3& pa, MT_Point3& pb)
{
    if (a.getType() == COMPLEX)
    {
	DT_Transform tb(b2w, (const DT_Convex&)b);
	return closest_points((const DT_Complex&)a, a2w, a_margin,
							(b_margin > MT_Scalar(0.0) ? static_cast<const DT_Convex&>(DT_Minkowski(tb, DT_Sphere(b_margin))) : static_cast<const DT_Convex&>(tb)), pa, pb);
    }
    
    return closest_pointsComplexConvex(b, b2w, b_margin, a, a2w, a_margin, pb, pa);
}

MT_Scalar closest_pointsComplexComplex(const DT_Shape& a, const MT_Transform& a2w, MT_Scalar a_margin,
									   const DT_Shape& b, const MT_Transform& b2w, MT_Scalar b_margin,
									   MT_Point3& pa, MT_Point3& pb) 
{
    return closest_points((const DT_Complex&)a, a2w, a_margin, 
						  (const DT_Complex&)b, b2w, b_margin, pa, pb);
}

Closest_pointsTable *closest_pointsInitialize()
{
    Closest_pointsTable *p = new Closest_pointsTable;
    p->addEntry(COMPLEX, COMPLEX, closest_pointsComplexComplex);
    p->addEntry(COMPLEX, CONVEX, closest_pointsComplexConvex);
    p->addEntry(CONVEX, CONVEX, closest_pointsConvexConvex);
    return p;
}

MT_Scalar closest_points(const DT_Object& a, const DT_Object& b,
						 MT_Point3& pa, MT_Point3& pb) 
{
    static Closest_pointsTable *closest_pointsTable = closest_pointsInitialize();
    Closest_points closest_points = closest_pointsTable->lookup(a.getType(), b.getType());
    return closest_points(a.m_shape, a.m_xform, a.m_margin, 
						  b.m_shape, b.m_xform, b.m_margin, pa, pb);
}

