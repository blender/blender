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

#include "DT_Facet.h"

bool DT_Facet::link(int edge0, DT_Facet *facet, int edge1) 
{
    m_adjFacets[edge0] = facet;
    m_adjEdges[edge0] = edge1;
    facet->m_adjFacets[edge1] = this;
    facet->m_adjEdges[edge1] = edge0;

    bool b = m_indices[edge0] == facet->m_indices[incMod3(edge1)] &&
	m_indices[incMod3(edge0)] == facet->m_indices[edge1];
    return b;
}

bool DT_Facet::computeClosest(const MT_Vector3 *verts)
{
    const MT_Vector3& p0 = verts[m_indices[0]]; 

    MT_Vector3 v1 = verts[m_indices[1]] - p0;
    MT_Vector3 v2 = verts[m_indices[2]] - p0;
    MT_Scalar v1dv1 = v1.length2();
    MT_Scalar v1dv2 = v1.dot(v2);
    MT_Scalar v2dv2 = v2.length2();
    MT_Scalar p0dv1 = p0.dot(v1); 
    MT_Scalar p0dv2 = p0.dot(v2);
    
    m_det = v1dv1 * v2dv2 - v1dv2 * v1dv2; // non-negative
    m_lambda1 = p0dv2 * v1dv2 - p0dv1 * v2dv2;
    m_lambda2 = p0dv1 * v1dv2 - p0dv2 * v1dv1; 
    
    if (m_det > MT_Scalar(0.0)) {	
	m_closest = p0 + (m_lambda1 * v1 + m_lambda2 * v2) / m_det;
	m_dist2 = m_closest.length2();
	return true;
    }
    
    return false;
} 

void DT_Facet::silhouette(int index, const MT_Vector3& w, 
			  DT_EdgeBuffer& edgeBuffer) 
{
    if (!m_obsolete) {
		if (m_closest.dot(w) < m_dist2) {
			edgeBuffer.push_back(DT_Edge(this, index));
		}	
	else {
	    m_obsolete = true; // Facet is visible 
	    int next = incMod3(index);
	    m_adjFacets[next]->silhouette(m_adjEdges[next], w, edgeBuffer);
	    next = incMod3(next);
	    m_adjFacets[next]->silhouette(m_adjEdges[next], w, edgeBuffer);
	}
    }
}


