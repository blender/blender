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

#ifndef DT_FACET_H
#define DT_FACET_H

#include <string.h>
#include <vector>

#include <MT_Vector3.h>
#include <MT_Point3.h>

class DT_Facet;


class DT_Edge {
public:
    DT_Edge() {}
    DT_Edge(DT_Facet *facet, int index) : 
	m_facet(facet), 
	m_index(index) {}

    DT_Facet *getFacet() const { return m_facet; }
    int       getIndex() const { return m_index; }

    int getSource() const;
    int getTarget() const;

private:    
    DT_Facet *m_facet;
    int       m_index;
};

typedef std::vector<DT_Edge> DT_EdgeBuffer;


class DT_Facet {
public:
    DT_Facet() {}
    DT_Facet(int i0, int i1, int i2) 
	  :	m_obsolete(false) 
    {
		m_indices[0] = i0; 
		m_indices[1] = i1; 
		m_indices[2] = i2;
    }
	
    int operator[](int i) const { return m_indices[i]; } 

    bool link(int edge0, DT_Facet *facet, int edge1);

    
    bool isObsolete() const { return m_obsolete; }
    

    bool computeClosest(const MT_Vector3 *verts);
    
    const MT_Vector3& getClosest() const { return m_closest; } 
    
    bool isClosestInternal() const
	{ 
		return m_lambda1 >= MT_Scalar(0.0) && 
			m_lambda2 >= MT_Scalar(0.0) && 
			m_lambda1 + m_lambda2 <= m_det;
    } 

    MT_Scalar getDist2() const { return m_dist2; }
	
    MT_Point3 getClosestPoint(const MT_Point3 *points) const 
	{
		const MT_Point3& p0 = points[m_indices[0]];
		
		return p0 + (m_lambda1 * (points[m_indices[1]] - p0) + 
					 m_lambda2 * (points[m_indices[2]] - p0)) / m_det;
    }
    
    void silhouette(const MT_Vector3& w, DT_EdgeBuffer& edgeBuffer) 
	{
		edgeBuffer.clear();
		m_obsolete = true;
		m_adjFacets[0]->silhouette(m_adjEdges[0], w, edgeBuffer);
		m_adjFacets[1]->silhouette(m_adjEdges[1], w, edgeBuffer);
		m_adjFacets[2]->silhouette(m_adjEdges[2], w, edgeBuffer);
    }
	
private:
    void silhouette(int index, const MT_Vector3& w, DT_EdgeBuffer& edgeBuffer);
	
    int         m_indices[3];
    bool        m_obsolete;
    DT_Facet   *m_adjFacets[3];
    int         m_adjEdges[3];
	
    MT_Scalar   m_det;
    MT_Scalar   m_lambda1;
    MT_Scalar   m_lambda2;
    MT_Vector3  m_closest;
    MT_Scalar   m_dist2;
};


inline int incMod3(int i) { return ++i % 3; } 

inline int DT_Edge::getSource() const 
{
    return (*m_facet)[m_index];
}

inline int DT_Edge::getTarget() const 
{
    return (*m_facet)[incMod3(m_index)];
}

#endif
