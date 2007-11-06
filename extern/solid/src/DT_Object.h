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

#ifndef DT_OBJECT_H
#define DT_OBJECT_H

#include <vector>

#include "SOLID.h"
#include "SOLID_broad.h"

#include "MT_Transform.h"
#include "MT_Quaternion.h"
#include "MT_BBox.h"
#include "DT_Shape.h"

class DT_Convex;

class DT_Object {
public:
    DT_Object(void *client_object, const DT_Shape& shape) :
		m_client_object(client_object),
		m_shape(shape), 
		m_margin(MT_Scalar(0.0))
	{
		m_xform.setIdentity();
		setBBox();
	}

	void setMargin(MT_Scalar margin) 
	{ 
		m_margin = margin; 
		setBBox();
	}

	void setScaling(const MT_Vector3& scaling)
	{
        m_xform.scale(scaling);
        setBBox();
    }

    void setPosition(const MT_Point3& pos) 
	{ 
        m_xform.setOrigin(pos);
        setBBox();
    }
    
    void setOrientation(const MT_Quaternion& orn)
	{
		m_xform.setRotation(orn);
		setBBox();
    }

	void setMatrix(const float *m) 
	{
        m_xform.setValue(m);
		assert(m_xform.getBasis().determinant() != MT_Scalar(0.0));
        setBBox();
    }

    void setMatrix(const double *m)
	{
        m_xform.setValue(m);
		assert(m_xform.getBasis().determinant() != MT_Scalar(0.0));
        setBBox();
    }

    void getMatrix(float *m) const
	{
        m_xform.getValue(m);
    }

    void getMatrix(double *m) const 
	{
        m_xform.getValue(m);
    }

	void setBBox();

	const MT_BBox& getBBox() const { return m_bbox; }	
	
    DT_ResponseClass getResponseClass() const { return m_responseClass; }
    
	void setResponseClass(DT_ResponseClass responseClass) 
	{ 
		m_responseClass = responseClass;
	}

    DT_ShapeType getType() const { return m_shape.getType(); }

    void *getClientObject() const { return m_client_object; }

	bool ray_cast(const MT_Point3& source, const MT_Point3& target, 
				  MT_Scalar& param, MT_Vector3& normal) const; 

	void addProxy(BP_ProxyHandle proxy) { m_proxies.push_back(proxy); }

	void removeProxy(BP_ProxyHandle proxy) 
	{ 
		T_ProxyList::iterator it = std::find(m_proxies.begin(), m_proxies.end(), proxy);
		if (it != m_proxies.end()) {
			m_proxies.erase(it);
		}
	}


	friend bool intersect(const DT_Object&, const DT_Object&, MT_Vector3& v);
	
	friend bool common_point(const DT_Object&, const DT_Object&, MT_Vector3&, 
							 MT_Point3&, MT_Point3&);
	
	friend bool penetration_depth(const DT_Object&, const DT_Object&, 
								  MT_Vector3&, MT_Point3&, MT_Point3&);
	
	friend MT_Scalar closest_points(const DT_Object&, const DT_Object&, 
									MT_Point3&, MT_Point3&);

private:
	typedef std::vector<BP_ProxyHandle> T_ProxyList;

	void              *m_client_object;
	DT_ResponseClass   m_responseClass;
    const DT_Shape&    m_shape;
    MT_Scalar          m_margin;
	MT_Transform       m_xform;
	T_ProxyList		   m_proxies;
	MT_BBox            m_bbox;
};

#endif







