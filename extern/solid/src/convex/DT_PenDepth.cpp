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

#include "DT_PenDepth.h"

#include "MT_Vector3.h"
#include "MT_Point3.h"
#include "MT_Quaternion.h"
#include "DT_Convex.h"
#include "DT_GJK.h"
#include "DT_Facet.h"

//#define DEBUG

const int       MaxSupportPoints = 1000;
const int       MaxFacets         = 2000;

static MT_Point3  pBuf[MaxSupportPoints];
static MT_Point3  qBuf[MaxSupportPoints];
static MT_Vector3 yBuf[MaxSupportPoints];

static DT_Facet facetBuf[MaxFacets];
static int  freeFacet = 0;
static DT_Facet *facetHeap[MaxFacets];
static int  num_facets;

class DT_FacetComp {
public:
    
    bool operator()(const DT_Facet *face1, const DT_Facet *face2) 
	{ 
		return face1->getDist2() > face2->getDist2();
    }
    
} facetComp;

inline DT_Facet *addFacet(int i0, int i1, int i2,
						  MT_Scalar lower2, MT_Scalar upper2) 
{
    assert(i0 != i1 && i0 != i2 && i1 != i2);
    if (freeFacet < MaxFacets)
	{
		DT_Facet *facet = new(&facetBuf[freeFacet++]) DT_Facet(i0, i1, i2);
#ifdef DEBUG
		std::cout << "Facet " << i0 << ' ' << i1 << ' ' << i2;
#endif
		if (facet->computeClosest(yBuf)) 
		{
			if (facet->isClosestInternal() && 
				lower2 <= facet->getDist2() && facet->getDist2() <= upper2) 
			{
				facetHeap[num_facets++] = facet;
				std::push_heap(&facetHeap[0], &facetHeap[num_facets], facetComp);
#ifdef DEBUG
				std::cout << " accepted" << std::endl;
#endif
			}
			else 
			{
#ifdef DEBUG
				std::cout << " rejected, ";
				if (!facet->isClosestInternal()) 
				{
					std::cout << "closest point not internal";
				}
				else if (lower2 > facet->getDist2()) 
				{
					std::cout << "facet is closer than orignal facet";
				}
				else 
				{
					std::cout << "facet is further than upper bound";
				}
				std::cout << std::endl;
#endif
			}
			
			return facet;
		}
    }
    
    return 0;
}

inline bool originInTetrahedron(const MT_Vector3& p1, const MT_Vector3& p2, 
								const MT_Vector3& p3, const MT_Vector3& p4)
{
    MT_Vector3 normal1 = (p2 - p1).cross(p3 - p1);
    MT_Vector3 normal2 = (p3 - p2).cross(p4 - p2);
    MT_Vector3 normal3 = (p4 - p3).cross(p1 - p3);
    MT_Vector3 normal4 = (p1 - p4).cross(p2 - p4);
    
    return 
		normal1.dot(p1) > MT_Scalar(0.0) != normal1.dot(p4) > MT_Scalar(0.0) &&
		normal2.dot(p2) > MT_Scalar(0.0) != normal2.dot(p1) > MT_Scalar(0.0) &&
		normal3.dot(p3) > MT_Scalar(0.0) != normal3.dot(p2) > MT_Scalar(0.0) &&
		normal4.dot(p4) > MT_Scalar(0.0) != normal4.dot(p3) > MT_Scalar(0.0);
}


bool penDepth(const DT_GJK& gjk, const DT_Convex& a, const DT_Convex& b,
			  MT_Vector3& v, MT_Point3& pa, MT_Point3& pb)
{
	
    int num_verts = gjk.getSimplex(pBuf, qBuf, yBuf);
    
    switch (num_verts) 
	{
	case 1:
	    // Touching contact. Yes, we have a collision,
	    // but no penetration.
	    return false;
	case 2:	
	{
	    // We have a line segment inside the Minkowski sum containing the
	    // origin. Blow it up by adding three additional support points.
	    
	    MT_Vector3 dir  = (yBuf[1] - yBuf[0]).normalized();
	    int        axis = dir.furthestAxis();
	    
	    static MT_Scalar sin_60 = MT_sqrt(MT_Scalar(3.0)) * MT_Scalar(0.5);
	    
	    MT_Quaternion rot(dir[0] * sin_60, dir[1] * sin_60, dir[2] * sin_60, MT_Scalar(0.5));
	    MT_Matrix3x3 rot_mat(rot);
	    
	    MT_Vector3 aux1 = dir.cross(MT_Vector3(axis == 0, axis == 1, axis == 2));
	    MT_Vector3 aux2 = rot_mat * aux1;
	    MT_Vector3 aux3 = rot_mat * aux2;
	    
	    pBuf[2] = a.support(aux1);
	    qBuf[2] = b.support(-aux1);
	    yBuf[2] = pBuf[2] - qBuf[2];
	    
	    pBuf[3] = a.support(aux2);
	    qBuf[3] = b.support(-aux2);
	    yBuf[3] = pBuf[3] - qBuf[3];
	    
	    pBuf[4] = a.support(aux3);
	    qBuf[4] = b.support(-aux3);
	    yBuf[4] = pBuf[4] - qBuf[4];
	    
	    if (originInTetrahedron(yBuf[0], yBuf[2], yBuf[3], yBuf[4])) 
		{
			pBuf[1] = pBuf[4];
			qBuf[1] = qBuf[4];
			yBuf[1] = yBuf[4];
	    }
	    else if (originInTetrahedron(yBuf[1], yBuf[2], yBuf[3], yBuf[4])) 
		{
			pBuf[0] = pBuf[4];
			qBuf[0] = qBuf[4];
			yBuf[0] = yBuf[4];
	    } 
	    else 
		{
			// Origin not in initial polytope
			return false;
	    }
	    
	    num_verts = 4;
	    
	    break;
	}
	case 3: 
	{
	    // We have a triangle inside the Minkowski sum containing
	    // the origin. First blow it up.
	    
	    MT_Vector3 v1     = yBuf[1] - yBuf[0];
	    MT_Vector3 v2     = yBuf[2] - yBuf[0];
	    MT_Vector3 vv     = v1.cross(v2);
	    
	    pBuf[3] = a.support(vv);
	    qBuf[3] = b.support(-vv);
	    yBuf[3] = pBuf[3] - qBuf[3];
	    pBuf[4] = a.support(-vv);
	    qBuf[4] = b.support(vv);
	    yBuf[4] = pBuf[4] - qBuf[4];
	    
	   
	    if (originInTetrahedron(yBuf[0], yBuf[1], yBuf[2], yBuf[4])) 
		{
			pBuf[3] = pBuf[4];
			qBuf[3] = qBuf[4];
			yBuf[3] = yBuf[4];
	    }
	    else if (!originInTetrahedron(yBuf[0], yBuf[1], yBuf[2], yBuf[3]))
		{ 
			// Origin not in initial polytope
			return false;
	    }
	    
	    num_verts = 4;
	    
	    break;
	}
    }
    
    // We have a tetrahedron inside the Minkowski sum containing
    // the origin (if GJK did it's job right ;-)
      
    
    if (!originInTetrahedron(yBuf[0], yBuf[1], yBuf[2], yBuf[3])) 
	{
		//	assert(false);
		return false;
	}
    
	num_facets = 0;
    freeFacet = 0;

    DT_Facet *f0 = addFacet(0, 1, 2, MT_Scalar(0.0), MT_INFINITY);
    DT_Facet *f1 = addFacet(0, 3, 1, MT_Scalar(0.0), MT_INFINITY);
    DT_Facet *f2 = addFacet(0, 2, 3, MT_Scalar(0.0), MT_INFINITY);
    DT_Facet *f3 = addFacet(1, 3, 2, MT_Scalar(0.0), MT_INFINITY);
    
    if (!f0 || f0->getDist2() == MT_Scalar(0.0) ||
		!f1 || f1->getDist2() == MT_Scalar(0.0) ||
		!f2 || f2->getDist2() == MT_Scalar(0.0) ||
		!f3 || f3->getDist2() == MT_Scalar(0.0)) 
	{
		return false;
    }
    
    f0->link(0, f1, 2);
    f0->link(1, f3, 2);
    f0->link(2, f2, 0);
    f1->link(0, f2, 2);
    f1->link(1, f3, 0);
    f2->link(1, f3, 1);
    
    if (num_facets == 0) 
	{
		return false;
    }
    
    // at least one facet on the heap.	
    
    DT_EdgeBuffer edgeBuffer(20);

    DT_Facet *facet = 0;
    
    MT_Scalar upper_bound2 = MT_INFINITY; 	
    
    do {
        facet = facetHeap[0];
        std::pop_heap(&facetHeap[0], &facetHeap[num_facets], facetComp);
        --num_facets;
		
		if (!facet->isObsolete()) 
		{
			assert(facet->getDist2() > MT_Scalar(0.0));
			
			if (num_verts == MaxSupportPoints)
			{
#ifdef DEBUG
				std::cout << "Ouch, no convergence!!!" << std::endl;
#endif 
				assert(false);	
				break;
			}
			
			pBuf[num_verts] = a.support(facet->getClosest());
			qBuf[num_verts] = b.support(-facet->getClosest());
			yBuf[num_verts] = pBuf[num_verts] - qBuf[num_verts];
			
			int index = num_verts++;
			MT_Scalar far_dist2 = yBuf[index].dot(facet->getClosest());
			
			// Make sure the support mapping is OK.
			assert(far_dist2 > MT_Scalar(0.0));
			
			GEN_set_min(upper_bound2, far_dist2 * far_dist2 / facet->getDist2());
			
			if (upper_bound2 <= DT_Accuracy::depth_tolerance * facet->getDist2()
#define CHECK_NEW_SUPPORT
#ifdef CHECK_NEW_SUPPORT
				|| yBuf[index] == yBuf[(*facet)[0]] 
				|| yBuf[index] == yBuf[(*facet)[1]]
				|| yBuf[index] == yBuf[(*facet)[2]]
#endif
				) 
			{
				break;
			}
			
			// Compute the silhouette cast by the new vertex
			// Note that the new vertex is on the positive side
			// of the current facet, so the current facet is will
			// not be in the convex hull. Start local search
			// from this facet.
			
			facet->silhouette(yBuf[index], edgeBuffer);
			
			if (edgeBuffer.empty()) 
			{
				return false;
			}
			
			DT_EdgeBuffer::const_iterator it = edgeBuffer.begin();
			DT_Facet *firstFacet = 
				addFacet((*it).getTarget(), (*it).getSource(),
						 index, facet->getDist2(), upper_bound2);
			
			if (!firstFacet) 
			{
				break;
			}
			
			firstFacet->link(0, (*it).getFacet(), (*it).getIndex());
			DT_Facet *lastFacet = firstFacet;
			
			++it;
			for (; it != edgeBuffer.end(); ++it) 
			{
				DT_Facet *newFacet = 
					addFacet((*it).getTarget(), (*it).getSource(),
							 index, facet->getDist2(), upper_bound2);
				
				if (!newFacet) 
				{
					break;
				}
				
				if (!newFacet->link(0, (*it).getFacet(), (*it).getIndex())) 
				{
					break;
				}
				
				if (!newFacet->link(2, lastFacet, 1)) 
				{
					break;
				}
				
				lastFacet = newFacet;				
			}
			if (it != edgeBuffer.end()) 
			{
				break;
			}
			
			firstFacet->link(2, lastFacet, 1);
		}
    }
    while (num_facets > 0 && facetHeap[0]->getDist2() <= upper_bound2);
	
#ifdef DEBUG    
    std::cout << "#facets left = " << num_facets << std::endl;
#endif
    
    v = facet->getClosest();
    pa = facet->getClosestPoint(pBuf);    
    pb = facet->getClosestPoint(qBuf);    
    return true;
}

