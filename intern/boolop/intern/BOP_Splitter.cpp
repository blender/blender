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
 
#include "BOP_Splitter.h"
#include "BOP_Tag.h"

#include <iostream>
using namespace std;

/**
 * Returns the split point resulting from intersect a plane and a mesh face  
 * according to its specified relative edge.
 * @param plane split plane
 * @param m mesh
 * @param f face
 * @param e relative edge index
 * @return intersection point
 */
MT_Point3 BOP_splitEdge(MT_Plane3 plane, BOP_Mesh *m, BOP_Face *f, unsigned int e)
{
	int v1 = -1, v2 = -1;
  
	switch(e) {
	case 1:
		v1 = f->getVertex(0);
		v2 = f->getVertex(1);
		break;
	case 2:
		v1 = f->getVertex(1);
		v2 = f->getVertex(2);
		break;
	case 3:
		v1 = f->getVertex(2);
		v2 = f->getVertex(0);
		break;
	default:
		// wrong relative edge index!
		break;
	}
  
	MT_Point3 p1 = m->getVertex(v1)->getPoint();
	MT_Point3 p2 = m->getVertex(v2)->getPoint();
	return BOP_intersectPlane(plane,p1,p2);
}

/**
 * Returns the segment resulting from intersect a plane and a mesh face.
 * @param plane split plane
 * @param m mesh
 * @param f face
 * @return segment if there is intersection, NULL otherwise
 */
BOP_Segment BOP_splitFace(MT_Plane3 plane, BOP_Mesh *m, BOP_Face *f)
{    
	BOP_Vertex *v1 = m->getVertex(f->getVertex(0));
	BOP_Vertex *v2 = m->getVertex(f->getVertex(1));
	BOP_Vertex *v3 = m->getVertex(f->getVertex(2));

	// Classify face vertices
	BOP_TAG tag1 = BOP_createTAG(BOP_classify(v1->getPoint(),plane));
	BOP_TAG tag2 = BOP_createTAG(BOP_classify(v2->getPoint(),plane));
	BOP_TAG tag3 = BOP_createTAG(BOP_classify(v3->getPoint(),plane));
  
	// Classify face according to its vertices classification
	BOP_TAG tag = BOP_createTAG(tag1,tag2,tag3);
  
	BOP_Segment s;

	switch(tag) {
	case IN_IN_IN : 
	case OUT_OUT_OUT :
	case ON_ON_ON :
		s.m_cfg1 = s.m_cfg2 = BOP_Segment::createUndefinedCfg();        
		break;
    
	case ON_OUT_OUT :
	case ON_IN_IN :
		s.m_v1 = f->getVertex(0);
		s.m_cfg1 = BOP_Segment::createVertexCfg(1);
		s.m_cfg2 = BOP_Segment::createUndefinedCfg();
		break;
    
	case OUT_ON_OUT :
	case IN_ON_IN :
		s.m_v1 = f->getVertex(1); 
		s.m_cfg1 = BOP_Segment::createVertexCfg(2);
		s.m_cfg2 = BOP_Segment::createUndefinedCfg();
		break;
    
	case OUT_OUT_ON :      
	case IN_IN_ON :
		s.m_v1 = f->getVertex(2); 
		s.m_cfg1 = BOP_Segment::createVertexCfg(3);
		s.m_cfg2 = BOP_Segment::createUndefinedCfg();
		break;
    
	case ON_ON_IN :
	case ON_ON_OUT :
		s.m_v1 = f->getVertex(0); 
		s.m_v2 = f->getVertex(1);
		s.m_cfg1 = BOP_Segment::createVertexCfg(1);
		s.m_cfg2 = BOP_Segment::createVertexCfg(2);
		break;
    
	case ON_OUT_ON :        
	case ON_IN_ON :
		s.m_v1 = f->getVertex(0); 
		s.m_v2 = f->getVertex(2);
		s.m_cfg1 = BOP_Segment::createVertexCfg(1);
		s.m_cfg2 = BOP_Segment::createVertexCfg(3);
		break;
    
	case OUT_ON_ON :
	case IN_ON_ON :
		s.m_v1 = f->getVertex(1); 
		s.m_v2 = f->getVertex(2);
		s.m_cfg1 = BOP_Segment::createVertexCfg(2);
		s.m_cfg2 = BOP_Segment::createVertexCfg(3);
		break;
    
	case IN_OUT_ON :
	case OUT_IN_ON :
		s.m_v2 = f->getVertex(2);
		s.m_cfg1 = BOP_Segment::createEdgeCfg(1);
		s.m_cfg2 = BOP_Segment::createVertexCfg(3);
	    break;
    
	case IN_ON_OUT :
	case OUT_ON_IN :
		s.m_v1 = f->getVertex(1);
		s.m_cfg1 = BOP_Segment::createVertexCfg(2);
		s.m_cfg2 = BOP_Segment::createEdgeCfg(3);
		break;
    
	case ON_IN_OUT :
	case ON_OUT_IN :
		s.m_v1 = f->getVertex(0);
		s.m_cfg1 = BOP_Segment::createVertexCfg(1);
		s.m_cfg2 = BOP_Segment::createEdgeCfg(2);
		break;
    
	case OUT_IN_IN :
	case IN_OUT_OUT :
		s.m_cfg1 = BOP_Segment::createEdgeCfg(1);
		s.m_cfg2 = BOP_Segment::createEdgeCfg(3);
		break;
    
	case OUT_IN_OUT :
	case IN_OUT_IN :
		s.m_cfg1 = BOP_Segment::createEdgeCfg(1);
		s.m_cfg2 = BOP_Segment::createEdgeCfg(2);
		break;
    
	case OUT_OUT_IN :
	case IN_IN_OUT :
		s.m_cfg1 = BOP_Segment::createEdgeCfg(2);
		s.m_cfg2 = BOP_Segment::createEdgeCfg(3);
		break;
    
	default:
		// wrong TAG!
		break;
	}

	return s;
}
