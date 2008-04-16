/**
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include "BOP_MathUtils.h"
#include "BOP_BSPNode.h"
#include "MT_assert.h"
#include "MT_MinMax.h"
#include <iostream>
using namespace std;

/**
 * Constructs a new BSP node.
 * @param plane split plane.
 */
BOP_BSPNode::BOP_BSPNode(const MT_Plane3& plane)
{
	m_plane      = plane;
	m_inChild    = NULL;
	m_outChild   = NULL;
	m_deep       = 1;
}

/**
 * Destroys a BSP tree.
 */
BOP_BSPNode::~BOP_BSPNode()
{
	if (m_inChild!=NULL) delete m_inChild;
	if (m_outChild!=NULL) delete m_outChild;
}

/**
 * Adds a new face to this BSP tree.
 * @param pts vector containing face points
 * @param plane face plane.
 */

unsigned int BOP_BSPNode::addFace(BOP_BSPPoints pts,
								  const MT_Plane3& plane )
{
	unsigned int newDeep = 0;
	BOP_TAG tag = ON;

	// find out if any points on the "face" lie in either half-space
	BOP_IT_BSPPoints ptsEnd = pts.end();
	for(BOP_IT_BSPPoints itp=pts.begin();itp!=ptsEnd;itp++){
		tag = (BOP_TAG) ((int) tag | (int)testPoint(*itp));
	}
 
	if (tag == ON) { }		// face lies on hyperplane: do nothing
	else if ((tag & IN) != 0 && (tag & OUT) == 0) {	// face is entirely on inside
		if (m_inChild != NULL)
			newDeep = m_inChild->addFace(pts, plane) + 1;
		else {
			m_inChild = new BOP_BSPNode(plane);
			newDeep = 2;
		}    
	} else if ((tag & OUT) != 0 && (tag & IN) == 0) { // face is entirely on outside
		if (m_outChild != NULL)
			newDeep = m_outChild->addFace(pts, plane) + 1;
		else {
			m_outChild = new BOP_BSPNode(plane);
			newDeep = 2;
		}      
	} else { // face lies in both half-spaces: split it
		BOP_BSPPoints inside, outside;
  		MT_Point3 lpoint= pts[pts.size()-1];
		BOP_TAG ltag = testPoint(lpoint);
		BOP_TAG tstate = ltag;

		// classify each line segment, looking for endpoints which lie on different
		// sides of the hyperplane.

		ptsEnd = pts.end();
		for(BOP_IT_BSPPoints itp=pts.begin();itp!=ptsEnd;itp++){
			MT_Point3 npoint= *itp;
			BOP_TAG ntag = testPoint(npoint);

			if(ltag != ON) {	// last point not on hyperplane
				if(tstate == IN) {
					if (m_inChild != NULL) inside.push_back(lpoint);
				} else {
					if (m_outChild != NULL) outside.push_back(lpoint);
				}
				if(ntag != ON && ntag != tstate) {	// last, self in different half-spaces 
					MT_Point3 mpoint = BOP_intersectPlane( m_plane, lpoint, npoint );
					if (m_inChild != NULL) inside.push_back(mpoint);
					if (m_outChild != NULL) outside.push_back(mpoint);
					tstate = ntag;
				}
			} else {			// last point on hyperplane, so we're switching
								// half-spaces
								// boundary point belong to both faces
				if (m_inChild != NULL) inside.push_back(lpoint);	
				if (m_outChild != NULL) outside.push_back(lpoint);
				tstate = ntag;	// state changes to new point tag
			}
			lpoint = npoint;	// save point, tag for next iteration
			ltag = ntag;
		}

		if (m_inChild != NULL)
			newDeep = m_inChild->addFace(inside, plane) + 1;
		else {
			m_inChild = new BOP_BSPNode(plane);
			newDeep = 2;
		}    
		if (m_outChild != NULL)
			newDeep = MT_max(newDeep, m_outChild->addFace(outside, plane) + 1);
		else {
			m_outChild = new BOP_BSPNode(plane);
			newDeep = MT_max(newDeep,(unsigned int)2);
		}      
	}
	
	// update the deep attribute
	m_deep = MT_max(m_deep,newDeep);
	
	return m_deep;
}

/**
 * Tests the point situation respect the node plane.
 * @param p point to test.
 * @return TAG result: IN, OUT or ON.
 */
BOP_TAG BOP_BSPNode::testPoint(const MT_Point3& p) const
{
  return BOP_createTAG(BOP_classify(p,m_plane));

}

/**
 * Classifies a face using its coordinates and plane.
 * @param p1 first point.
 * @param p2 second point.
 * @param p3 third point.
 * @param plane face plane.
 * @return TAG result: IN, OUT or IN&OUT.
 */
BOP_TAG BOP_BSPNode::classifyFace(const MT_Point3& p1, 
								  const MT_Point3& p2, 
								  const MT_Point3& p3, 
								  const MT_Plane3& plane) const
{
	// local variables
	MT_Point3 auxp1, auxp2;
	BOP_TAG auxtag1, auxtag2, auxtag3;

	switch(BOP_createTAG(testPoint(p1),testPoint(p2),testPoint(p3))) {
		// Classify the face on the IN side
		case IN_IN_IN : 
			return classifyFaceIN(p1, p2, p3, plane);
		case IN_IN_ON :
		case IN_ON_IN :
		case ON_IN_IN :
		case IN_ON_ON :
		case ON_IN_ON :
		case ON_ON_IN :
			return BOP_addON(classifyFaceIN(p1, p2, p3, plane));
	
		// Classify the face on the OUT side
		case OUT_OUT_OUT :
			return classifyFaceOUT(p1, p2, p3, plane);
		case OUT_OUT_ON :
		case OUT_ON_OUT :
		case ON_OUT_OUT :
		case ON_ON_OUT :
		case ON_OUT_ON :
		case OUT_ON_ON :
			return BOP_addON(classifyFaceOUT(p1, p2, p3, plane));
	
		// Classify the ON face depending on it plane normal
		case ON_ON_ON :
			if (hasSameOrientation(plane))
				return BOP_addON(classifyFaceIN(p1, p2, p3, plane));
			else
				return BOP_addON(classifyFaceOUT(p1, p2, p3, plane));

		// Classify the face IN/OUT and one vertex ON
		// becouse only one ON, only one way to subdivide the face
		case IN_OUT_ON :
			auxp1 = BOP_intersectPlane(m_plane, p1, p2);
			auxtag1 = classifyFaceIN( p1,    auxp1 , p3, plane);
			auxtag2 = classifyFaceOUT(auxp1, p2,     p3, plane);
			return (BOP_compTAG(auxtag1,auxtag2)?BOP_addON(auxtag1):INOUT);

		case OUT_IN_ON :
			auxp1 = BOP_intersectPlane(m_plane, p1, p2);
			auxtag1 = classifyFaceOUT(p1,    auxp1, p3, plane);
			auxtag2 = classifyFaceIN( auxp1, p2,    p3, plane);
			return (BOP_compTAG(auxtag1,auxtag2)?BOP_addON(auxtag1):INOUT);

		case IN_ON_OUT :
			auxp1 = BOP_intersectPlane(m_plane, p1, p3);
			auxtag1 = classifyFaceIN( p1, p2, auxp1, plane);
			auxtag2 = classifyFaceOUT(p2, p3, auxp1, plane);
			return (BOP_compTAG(auxtag1,auxtag2)?BOP_addON(auxtag1):INOUT);

		case OUT_ON_IN :
			auxp1 = BOP_intersectPlane(m_plane, p1, p3);
			auxtag1 = classifyFaceOUT(p1, p2, auxp1, plane);
			auxtag2 = classifyFaceIN( p2, p3, auxp1, plane);
			return (BOP_compTAG(auxtag1,auxtag2)?BOP_addON(auxtag1):INOUT);

		case ON_IN_OUT :
			auxp1 = BOP_intersectPlane(m_plane, p2, p3);
			auxtag1 = classifyFaceIN( p1,    p2, auxp1, plane);
			auxtag2 = classifyFaceOUT(auxp1, p3, p1,    plane);
			return (BOP_compTAG(auxtag1,auxtag2)?BOP_addON(auxtag1):INOUT);

		case ON_OUT_IN :
			auxp1 = BOP_intersectPlane(m_plane, p2, p3);
			auxtag1 = classifyFaceOUT(p1,    p2, auxp1, plane);
			auxtag2 = classifyFaceIN( auxp1, p3, p1,    plane);
			return (BOP_compTAG(auxtag1,auxtag2)?BOP_addON(auxtag1):INOUT);

		// Classify IN/OUT face without ON vertices.
		// Two ways to divide the triangle, 
		// will chose the least degenerated sub-triangles.
		case IN_OUT_OUT :
			auxp1 = BOP_intersectPlane(m_plane, p1, p2);
			auxp2 = BOP_intersectPlane(m_plane, p1, p3);
	
			// f1: p1 auxp1 , auxp1 auxp2
			auxtag1 = classifyFaceIN(p1, auxp1, auxp2, plane);
	
			// f2: auxp1 p2 , p2 auxp2;  f3: p2 p3 , p3 auxp2  ||  
			// f2: auxp1 p3, p3 auxp2;   f3: p2 p3 , p3 auxp1
			if (BOP_isInsideCircle(p2, p3, auxp1, auxp2)) {
				auxtag2 = classifyFaceOUT(auxp1, p2, auxp2, plane);
				auxtag3 = classifyFaceOUT(p2,    p3, auxp2, plane);
			}
			else {
				auxtag2 = classifyFaceOUT(auxp1, p3, auxp2, plane);
				auxtag3 = classifyFaceOUT(p2,    p3, auxp1, plane);
			}
			return (BOP_compTAG(auxtag1,auxtag2)&&BOP_compTAG(auxtag2,auxtag3)?auxtag1:INOUT);

		case OUT_IN_IN :
			auxp1 = BOP_intersectPlane(m_plane, p1, p2);
			auxp2 = BOP_intersectPlane(m_plane, p1, p3);
	
			// f1: p1 auxp1 , auxp1 auxp2
			auxtag1 = classifyFaceOUT(p1, auxp1, auxp2, plane);
	
			// f2: auxp1 p2 , p2 auxp2;  f3: p2 p3 , p3 auxp2  ||
			// f2: auxp1 p3, p3 auxp2;  f3: p2 p3 , p3 auxp1
			if (BOP_isInsideCircle(p2, p3, auxp1, auxp2)) {
				auxtag2 = classifyFaceIN(auxp1, p2, auxp2, plane);
				auxtag3 = classifyFaceIN(p2, p3, auxp2, plane);
			}
			else {
				auxtag2 = classifyFaceIN(auxp1, p3, auxp2, plane);
				auxtag3 = classifyFaceIN(p2,    p3, auxp1, plane);
			}
			return (BOP_compTAG(auxtag1,auxtag2)&&BOP_compTAG(auxtag2,auxtag3)?auxtag1:INOUT);

		case OUT_IN_OUT :
			auxp1 = BOP_intersectPlane(m_plane, p2, p1);
			auxp2 = BOP_intersectPlane(m_plane, p2, p3);
	
			// f1: auxp1 p2 , p2 auxp2
			auxtag1 = classifyFaceIN(auxp1, p2, auxp2, plane);
	
			// f2: p1 auxp1 , auxp1 auxp2;  f3: p1 auxp2 , auxp2 p3  ||  
			// f2: p3 auxp1, auxp1 auxp2  f3:p1 auxp1, auxp1 p3
			if (BOP_isInsideCircle(p1, p3, auxp1, auxp2)) {
				auxtag2 = classifyFaceOUT(p1, auxp1, auxp2, plane);
				auxtag3 = classifyFaceOUT(p1, auxp2, p3,    plane);
			}
			else {
				auxtag2 = classifyFaceOUT(p3, auxp1, auxp2, plane);
				auxtag3 = classifyFaceOUT(p1, auxp1, p3,    plane);
			}
			return (BOP_compTAG(auxtag1,auxtag2)&&BOP_compTAG(auxtag2,auxtag3)?auxtag1:INOUT);
    
		case IN_OUT_IN :
			auxp1 = BOP_intersectPlane(m_plane, p2, p1);
			auxp2 = BOP_intersectPlane(m_plane, p2, p3);
	
			// f1: auxp1 p2 , p2 auxp2
			auxtag1 = classifyFaceOUT(auxp1, p2, auxp2, plane);
	
			// f2: p1 auxp1 , auxp1 auxp2;  f3: p1 auxp2 , auxp2 p3  ||  
			// f2: p3 auxp1, auxp1 auxp2  f3:p1 auxp1, auxp1 p3
			if (BOP_isInsideCircle(p1, p3, auxp1, auxp2)) {
				auxtag2 = classifyFaceIN(p1, auxp1, auxp2, plane);
				auxtag3 = classifyFaceIN(p1, auxp2, p3,    plane);
			}
			else {
				auxtag2 = classifyFaceIN(p3, auxp1, auxp2, plane);
				auxtag3 = classifyFaceIN(p1, auxp1, p3,    plane);
			}
			return (BOP_compTAG(auxtag1,auxtag2)&&BOP_compTAG(auxtag2,auxtag3)?auxtag1:INOUT);
	
		case OUT_OUT_IN :
			auxp1 = BOP_intersectPlane(m_plane, p3, p1);
			auxp2 = BOP_intersectPlane(m_plane, p3, p2);
	
			// f1: auxp1 auxp2 , auxp2 p3
			auxtag1 = classifyFaceIN(auxp1, auxp2, p3, plane);
	
			// f2: p1 p2 , p2 auxp2;   f3:p1 auxp2 , auxp2 auxp1  ||
			// f2: p1 p2, p2 auxp1;  f3:p2 auxp2, auxp2 auxp1
			if (BOP_isInsideCircle(p1, p2, auxp1, auxp2)) {
				auxtag2 = classifyFaceOUT(p1, p2,    auxp2, plane);
				auxtag3 = classifyFaceOUT(p1, auxp2, auxp1, plane);
			}
			else {
				auxtag2 = classifyFaceOUT(p1, p2,    auxp1, plane);
				auxtag3 = classifyFaceOUT(p2, auxp2, auxp1, plane);
			}
			return (BOP_compTAG(auxtag1,auxtag2)&&BOP_compTAG(auxtag2,auxtag3)?auxtag1:INOUT);

		case IN_IN_OUT :
			auxp1 = BOP_intersectPlane(m_plane, p3, p1);
			auxp2 = BOP_intersectPlane(m_plane, p3, p2);
	
			// f1: auxp1 auxp2 , auxp2 p3
			auxtag1 = classifyFaceOUT(auxp1, auxp2, p3, plane);
	
			// f2: p1 p2 , p2 auxp2;   f3:p1 auxp2 , auxp2 auxp1  ||
			// f2: p1 p2, p2 auxp1;  f3:p2 auxp2, auxp2 auxp1
			if (BOP_isInsideCircle(p1, p2, auxp1, auxp2)) {
				auxtag2 = classifyFaceIN(p1, p2,    auxp2, plane);
				auxtag3 = classifyFaceIN(p1, auxp2, auxp1, plane);
			}
			else {
				auxtag2 = classifyFaceIN(p1, p2,    auxp1, plane);
				auxtag3 = classifyFaceIN(p2, auxp2, auxp1, plane);
			}
			return (BOP_compTAG(auxtag1,auxtag2)&&BOP_compTAG(auxtag2,auxtag3)?auxtag1:INOUT);

		default:
			return UNCLASSIFIED;
	}
}

/**
 * Classifies a face through IN subtree.
 * @param p1 firts face vertex.
 * @param p2 second face vertex.
 * @param p3 third face vertex.
 * @param plane face plane.
 */
BOP_TAG BOP_BSPNode::classifyFaceIN(const MT_Point3& p1, 
									const MT_Point3& p2, 
									const MT_Point3& p3, 
									const MT_Plane3& plane) const
{
	if (m_inChild != NULL)
		return m_inChild->classifyFace(p1, p2, p3, plane);
	else
		return IN;
}

/**
 * Classifies a face through OUT subtree.
 * @param p1 firts face vertex.
 * @param p2 second face vertex.
 * @param p3 third face vertex.
 * @param plane face plane.
 */
BOP_TAG BOP_BSPNode::classifyFaceOUT(const MT_Point3& p1, 
									 const MT_Point3& p2, 
									 const MT_Point3& p3, 
									 const MT_Plane3& plane) const
{
	if (m_outChild != NULL)
		return m_outChild->classifyFace(p1, p2, p3, plane);
	else
		return OUT;
}

/**
 * Simplified classification (optimized but requires that the face is not 
 * INOUT; only works correctly with faces completely IN or OUT).
 * @param p1 firts face vertex.
 * @param p2 second face vertex.
 * @param p3 third face vertex.
 * @param plane face plane.
 * @return TAG result: IN or OUT.
 */
BOP_TAG BOP_BSPNode::simplifiedClassifyFace(const MT_Point3& p1, 
											const MT_Point3& p2, 
											const MT_Point3& p3, 
											const MT_Plane3& plane) const
{
	MT_Point3 ret[3];

	BOP_TAG tag = BOP_createTAG(testPoint(p1),testPoint(p2),testPoint(p3));

	if ((tag & IN_IN_IN) != 0) {
		if ((tag & OUT_OUT_OUT) != 0) {	    
		  if (splitTriangle(ret,m_plane,p1,p2,p3,tag)<0)
		    return simplifiedClassifyFaceIN(ret[0],ret[1],ret[2],plane);
		  else
		    return simplifiedClassifyFaceOUT(ret[0],ret[1],ret[2],plane);
		}
		else {
			return simplifiedClassifyFaceIN(p1,p2,p3,plane);
		}
	}
	else {
		if ((tag & OUT_OUT_OUT) != 0) {
			return simplifiedClassifyFaceOUT(p1,p2,p3,plane);
		}
		else {
			if (hasSameOrientation(plane)) {
				return simplifiedClassifyFaceIN(p1,p2,p3,plane);
			}
			else {
				return simplifiedClassifyFaceOUT(p1,p2,p3,plane);
			}
		}
	}
	
	return IN;
}

/**
 * Simplified classify through IN subtree.
 * @param p1 firts face vertex.
 * @param p2 second face vertex.
 * @param p3 third face vertex.
 * @param plane face plane.
 */
BOP_TAG BOP_BSPNode::simplifiedClassifyFaceIN(const MT_Point3& p1, 
											  const MT_Point3& p2, 
											  const MT_Point3& p3, 
											  const MT_Plane3& plane) const
{
	if (m_inChild != NULL)
		return m_inChild->simplifiedClassifyFace(p1, p2, p3, plane);
	else
		return IN;
}

/**
 * Simplified classify through OUT subtree.
 * @param p1 firts face vertex.
 * @param p2 second face vertex.
 * @param p3 third face vertex.
 * @param plane face plane.
 */
BOP_TAG BOP_BSPNode::simplifiedClassifyFaceOUT(const MT_Point3& p1, 
											   const MT_Point3& p2, 
											   const MT_Point3& p3, 
											   const MT_Plane3& plane) const
{
	if (m_outChild != NULL)
		return m_outChild->simplifiedClassifyFace(p1, p2, p3, plane);
	else
		return OUT;
}

/**
 * Determine if the input plane have the same orientation of the node plane.
 * @param plane plane to test.
 * @return TRUE if have the same orientation, FALSE otherwise.
 */
bool BOP_BSPNode::hasSameOrientation(const MT_Plane3& plane) const
{
	return (BOP_orientation(m_plane,plane)>0);
}

/**
 * Comparation between both childrens.
 * @return 0 equal deep, 1 inChild more deep than outChild and -1 otherwise.
 */
int BOP_BSPNode::compChildren() const
{
	unsigned int deep1 = (m_inChild == NULL?0:m_inChild->getDeep());
	unsigned int deep2 = (m_outChild == NULL?0:m_outChild->getDeep());
	
	if (deep1 == deep2)
		return 0;
	else if (deep1 < deep2)
		return -1;
	else
		return 1;
}

/**
 * Extract a subtriangle from input triangle, is used for simplified classification.
 * The subtriangle is obtained spliting the input triangle by input plane.
 * @param res output subtriangle result.
 * @param plane spliter plane.
 * @param p1 first triangle point.
 * @param p2 second triangle point.
 * @param p3 third triangle point.
 * @param tag triangle orientation respect the plane.
 */
int BOP_BSPNode::splitTriangle(MT_Point3* res, 
							   const MT_Plane3& plane, 
							   const MT_Point3& p1, 
							   const MT_Point3& p2, 
							   const MT_Point3& p3, 
							   const BOP_TAG tag) const
{
	switch (tag) {
		case IN_OUT_ON :
		  if (compChildren()<0) {
		    // f1: p1 new p3 || new = splitedge(p1,p2)
		    res[0] = p1;
		    res[1] = BOP_intersectPlane( plane, p1, p2 );
		    res[2] = p3;
		    return -1;
		  }else{
		    // f1: p2 new p3 || new = splitedge(p1,p2)
		    res[0] = p2;
		    res[1] = p3;
		    res[2] = BOP_intersectPlane( plane, p1, p2 );
		    return 1;
		  }
		case OUT_IN_ON :
		  if (compChildren()<0) {
		    // f1: p2 new p3 || new = splitedge(p1,p2)
		    res[0] = p2;
		    res[1] = p3;
		    res[2] = BOP_intersectPlane( plane, p1, p2 );
		    return -1;
		  }else{
		    // f1: p1 new p3 || new = splitedge(p1,p2)
		    res[0] = p1;
		    res[1] = BOP_intersectPlane( plane, p1, p2 );
		    res[2] = p3;
		    return 1;
		  }
		case IN_ON_OUT :
		  if (compChildren()<0) {
		    // f1: p1 p2 new || new = splitedge(p1,p3)
		    res[0] = p1;
		    res[1] = p2;
		    res[2] = BOP_intersectPlane( plane, p1, p3 );
		    return -1;
		  }else{
		    // f1: p2 p3 new || new = splitedge(p1,p3)
		    res[0] = p2;
		    res[1] = p3;
		    res[2] = BOP_intersectPlane( plane, p1, p3 );
		    return 1;
		  }
		case OUT_ON_IN :
		  if (compChildren()<0) {
		    // f1: p2 p3 new || new = splitedge(p1,p3)
		    res[0] = p2;
		    res[1] = p3;
		    res[2] = BOP_intersectPlane( plane, p1, p3 );
		    return -1;
		  }else{
		    // f1: p1 p2 new || new = splitedge(p1,p3)
		    res[0] = p1;
		    res[1] = p2;
		    res[2] = BOP_intersectPlane( plane, p1, p3 );
		    return 1;
		  }
		case ON_IN_OUT :
		  if (compChildren()<0) {
		    // f1: p1 p2 new || new = splitedge(p2,p3)
		    res[0] = p1;
		    res[1] = p2;
		    res[2] = BOP_intersectPlane( plane, p2, p3 );
		    return -1;
		  }else{
		    // f1: p1 p3 new || new = splitedge(p2,p3)
		    res[0] = p1;
		    res[1] = BOP_intersectPlane( plane, p2, p3 );
		    res[2] = p3;
		    return 1;
		  }
		case ON_OUT_IN :
		  if (compChildren()<0) {
		    // f1: p1 p2 new || new = splitedge(p2,p3)
		    res[0] = p1;
		    res[1] = BOP_intersectPlane( plane, p2, p3 );
		    res[2] = p3;
		    return -1;
		  }else{
		    // f1: p1 p2 new || new = splitedge(p2,p3)
		    res[0] = p1;
		    res[1] = p2;
		    res[2] = BOP_intersectPlane( plane, p2, p3 );
		    return 1;
		  }
		case IN_OUT_OUT :
		  if (compChildren()<=0) {
		    // f1: p1 new1 new2 || new1 = splitedge(p1,p2) new2 = splitedge(p1,p3)
		    res[0] = p1;
		    res[1] = BOP_intersectPlane( plane, p1, p2 );
		    res[2] = BOP_intersectPlane( plane, p1, p3 );
		    return -1;
		  }else{
		    // f1: p1 new1 new2 || new1 = splitedge(p1,p2) new2 = splitedge(p1,p3)
		    res[0] = BOP_intersectPlane( plane, p1, p2 );
		    res[1] = p2;
		    res[2] = p3;
		    return 1;
		  }
		case OUT_IN_IN :
		  if (compChildren()<0) {
		    // f1: p1 new1 new2 || new1 = splitedge(p1,p2) new2 = splitedge(p1,p3)
		    res[0] = BOP_intersectPlane( plane, p1, p2 );
		    res[1] = p2;
		    res[2] = p3;
		    return -1;
		  }else {
		    // f1: p1 new1 new2 || new1 = splitedge(p1,p2) new2 = splitedge(p1,p3)
		    res[0] = p1;
		    res[1] = BOP_intersectPlane( plane, p1, p2 );
		    res[2] = BOP_intersectPlane( plane, p1, p3 );
		    return 1;
		  }
		case OUT_IN_OUT :
		  if (compChildren()<=0) {
		    // f1: new1 p2 new2 || new1 = splitedge(p2,p1) new2 = splitedge(p2,p3)
		    res[0] = BOP_intersectPlane( plane, p2, p1 );
		    res[1] = p2;
		    res[2] = BOP_intersectPlane( plane, p2, p3 );
		    return -1;
		  }else {
		    // f1: new1 p2 new2 || new1 = splitedge(p2,p1) new2 = splitedge(p2,p3)
		    res[0] = p1;
		    res[1] = BOP_intersectPlane( plane, p2, p1 );
		    res[2] = BOP_intersectPlane( plane, p2, p3 );
		    return 1;
		  }
		case IN_OUT_IN :
		  if (compChildren()<0) {
		    // f1: new1 p2 new2 || new1 = splitedge(p2,p1) new2 = splitedge(p2,p3)
		    res[0] = p1;
		    res[1] = BOP_intersectPlane( plane, p2, p1 );
		    res[2] = BOP_intersectPlane( plane, p2, p3 );
		    return -1;
		  }else{
		    // f1: new1 p2 new2 || new1 = splitedge(p2,p1) new2 = splitedge(p2,p3)
		    res[0] = BOP_intersectPlane( plane, p2, p1 );
		    res[1] = p2;
		    res[2] = BOP_intersectPlane( plane, p2, p3 );
		    return 1;
		  }
		case OUT_OUT_IN :
		  if (compChildren()<=0) {
		    // f1: new1 new2 p2 || new1 = splitedge(p3,p1) new2 = splitedge(p3,p2)
		    res[0] = BOP_intersectPlane( plane, p3, p1 );
		    res[1] = BOP_intersectPlane( plane, p3, p2 );
		    res[2] = p3;
		    return -1;
		  }else{
		    // f1: new1 new2 p2 || new1 = splitedge(p3,p1) new2 = splitedge(p3,p2)
		    res[0] = BOP_intersectPlane( plane, p3, p1 );
		    res[1] = p1;
		    res[2] = p2;
		    return 1;
		  }
		case IN_IN_OUT :
		  if (compChildren()<0) {
		    // f1: new1 new2 p2 || new1 = splitedge(p3,p1) new2 = splitedge(p3,p2)
		    res[0] = BOP_intersectPlane( plane, p3, p1 );
		    res[1] = p1;
		    res[2] = p2;
		    return -1;
		  }else{
		    // f1: new1 new2 p2 || new1 = splitedge(p3,p1) new2 = splitedge(p3,p2)
		    res[0] = BOP_intersectPlane( plane, p3, p1 );
		    res[1] = BOP_intersectPlane( plane, p3, p2 );
		    res[2] = p3;
		    return 1;
		  }
	default:
	  return 0;
	}
}

/**
 * Debug info.
 */
void BOP_BSPNode::print(unsigned int deep)
{
	cout << "(" << deep << "," << m_plane << ")," << endl;
	if (m_inChild != NULL)
		m_inChild->print(deep + 1);
	else
		cout << "(" << deep+1 << ",None)," << endl;
	if (m_outChild != NULL)
		m_outChild->print(deep + 1);
	else
		cout << "(" << deep+1 << ",None)," << endl;
}
