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

#ifndef DT_COMPLEX_H
#define DT_COMPLEX_H

#include <algorithm>

#include "MT_Transform.h"
#include "DT_VertexBase.h"

#include "DT_Shape.h"
#include "DT_CBox.h"
#include "DT_BBoxTree.h"

class DT_Convex;

class DT_Complex : public DT_Shape  {
public:
	DT_Complex(const DT_VertexBase *base);
	virtual ~DT_Complex();
	
	void finish(DT_Count n, const DT_Convex *p[]);
    
	virtual DT_ShapeType getType() const { return COMPLEX; }

    virtual MT_BBox bbox(const MT_Transform& t, MT_Scalar margin) const;

	virtual bool ray_cast(const MT_Point3& source, const MT_Point3& target, 
						  MT_Scalar& lambda, MT_Vector3& normal) const; 

	void refit();
	

    friend bool intersect(const DT_Complex& a, const MT_Transform& a2w, MT_Scalar a_margin, 
		                  const DT_Convex& b, MT_Vector3& v);
    
    friend bool intersect(const DT_Complex& a, const MT_Transform& a2w, MT_Scalar a_margin, 
		                  const DT_Complex& b, const MT_Transform& b2w, MT_Scalar b_margin,
                          MT_Vector3& v);
   
    friend bool common_point(const DT_Complex& a, const MT_Transform& a2w, MT_Scalar a_margin, 
		                     const DT_Convex& b, MT_Vector3& v, MT_Point3& pa, MT_Point3& pb);
    
    friend bool common_point(const DT_Complex& a, const MT_Transform& a2w, MT_Scalar a_margin, 
		                     const DT_Complex& b, const MT_Transform& b2w, MT_Scalar b_margin,
                             MT_Vector3& v, MT_Point3& pa, MT_Point3& pb);
    
    friend bool penetration_depth(const DT_Complex& a, const MT_Transform& a2w, MT_Scalar a_margin, 
								  const DT_Convex& b, MT_Scalar b_margin, MT_Vector3& v, MT_Point3& pa, MT_Point3& pb);
    
    friend bool penetration_depth(const DT_Complex& a, const MT_Transform& a2w, MT_Scalar a_margin, 
								  const DT_Complex& b, const MT_Transform& b2w, MT_Scalar b_margin,
								  MT_Vector3& v, MT_Point3& pa, MT_Point3& pb);

    friend MT_Scalar closest_points(const DT_Complex& a, const MT_Transform& a2w, MT_Scalar a_margin, 
		                            const DT_Convex& b, MT_Point3& pa, MT_Point3& pb);
    
    friend MT_Scalar closest_points(const DT_Complex& a, const MT_Transform& a2w, MT_Scalar a_margin, 
		                            const DT_Complex& b, const MT_Transform& b2w, MT_Scalar b_margin,
									MT_Point3& pa, MT_Point3& pb);

	const DT_VertexBase   *m_base;
	DT_Count               m_count;
	const DT_Convex      **m_leaves;
	DT_BBoxNode           *m_nodes;
	DT_CBox                m_cbox;
	DT_BBoxTree::NodeType  m_type;
};

#endif



