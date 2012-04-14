/*
 *
 *
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Marc Freixas, Ken Hughes
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file boolop/intern/BOP_Face2Face.cpp
 *  \ingroup boolopintern
 */

 
#include "BOP_Face2Face.h"
#include "BOP_BBox.h"

// TAGS for segment classification in x-segment creation
// sA -> point of sA
// sB -> point of sB
// sX -> point of sA and SB
#define sA_sB 12
#define sB_sA 21
#define sX_sA 31
#define sA_sX 13
#define sX_sB 32
#define sB_sX 23
#define sX_sX 33
    
#define sA_sA_sB 112
#define sB_sB_sA 221
#define sB_sA_sA 211
#define sA_sB_sB 122
#define sA_sB_sA 121
#define sB_sA_sB 212
#define sA_sX_sB 132
#define sB_sX_sA 231
#define sX_sA_sB 312
#define sX_sB_sA 321
#define sA_sB_sX 123
#define sB_sA_sX 213

#define sA_sA_sB_sB 1122
#define sB_sB_sA_sA 2211
#define sA_sB_sA_sB 1212
#define sB_sA_sB_sA 2121
#define sA_sB_sB_sA 1221
#define sB_sA_sA_sB 2112

void BOP_intersectCoplanarFaces(BOP_Mesh*  mesh, 
								BOP_Faces* facesB, 
								BOP_Face*  faceA, 
								BOP_Face*  faceB, 
								bool       invert);

void BOP_intersectCoplanarFaces(BOP_Mesh*   mesh, 
								BOP_Faces*  facesB, 
								BOP_Face*   faceB, 
								BOP_Segment sA, 
								MT_Plane3   planeA, 
								bool        invert);

void BOP_intersectNonCoplanarFaces(BOP_Mesh*  mesh, 
								   BOP_Faces* facesA,  
								   BOP_Faces* facesB, 
								   BOP_Face*  faceA, 
								   BOP_Face*  faceB);

void BOP_getPoints(BOP_Mesh*    mesh, 
				   BOP_Face*    faceA, 
				   BOP_Segment& sA, 
				   MT_Plane3    planeB, 
				   MT_Point3*   points, 
				   unsigned int*         faces, 
				   unsigned int&         size, 
				   unsigned int         faceValue);

void BOP_mergeSort(MT_Point3 *points, unsigned int *face, unsigned int &size, bool &invertA, bool &invertB);

void BOP_createXS(BOP_Mesh*    mesh, 
				  BOP_Face*    faceA, 
				  BOP_Face*    faceB, 
				  BOP_Segment  sA, 
				  BOP_Segment  sB, 
				  bool         invert, 
				  BOP_Segment* segments);    

void BOP_createXS(BOP_Mesh*    mesh, 
				  BOP_Face*    faceA, 
				  BOP_Face*    faceB, 
				  MT_Plane3    planeA, 
				  MT_Plane3    planeB, 
				  BOP_Segment  sA, 
				  BOP_Segment  sB, 
				  bool         invert, 
				  BOP_Segment* segments);    

BOP_Index BOP_getVertexIndex(BOP_Mesh* mesh, 
					   MT_Point3 point, 
					   unsigned int       cfgA, 
					   unsigned int       cfgB, 
					   BOP_Index       vA, 
					   BOP_Index       vB, 
					   bool      invert);

BOP_Index BOP_getVertexIndex(BOP_Mesh *mesh, MT_Point3 point, unsigned int cfg, BOP_Index v);

void triangulate(BOP_Mesh *mesh,  BOP_Faces *faces, BOP_Face *face, BOP_Segment s);

BOP_Face *BOP_getOppositeFace(BOP_Mesh*  mesh, 
							  BOP_Faces* faces, 
							  BOP_Face*  face, 
							  BOP_Edge*  edge);

bool BOP_overlap(MT_Vector3 normal, 
				 MT_Point3  p1, 
				 MT_Point3  p2, 
				 MT_Point3  p3, 
				 MT_Point3  q1, 
				 MT_Point3  q2, 
				 MT_Point3  q3);

void BOP_mergeVertexs(BOP_Mesh *mesh, unsigned int firstFace);


/**
 * Computes intersections between faces of both lists.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param facesA set of faces from object A
 * @param facesB set of faces from object B
 *
 * Two optimizations were added here:
 *   1) keep the bounding box for a face once it's created; this is
 *      especially important for B faces, since they were being created and
 *      recreated over and over
 *   2) associate a "split" index in the faceB vector with each A face; when
 *      an A face is split, we will not need to recheck any B faces have
 *      already been checked against that original A face
 */

void BOP_Face2Face(BOP_Mesh *mesh, BOP_Faces *facesA, BOP_Faces *facesB)
{
	for(unsigned int idxFaceA=0;idxFaceA<facesA->size();idxFaceA++) {
		BOP_Face *faceA = (*facesA)[idxFaceA];
		MT_Plane3 planeA = faceA->getPlane();
		MT_Point3 p1 = mesh->getVertex(faceA->getVertex(0))->getPoint();
		MT_Point3 p2 = mesh->getVertex(faceA->getVertex(1))->getPoint();
		MT_Point3 p3 = mesh->getVertex(faceA->getVertex(2))->getPoint();

		/* get (or create) bounding box for face A */
		if( faceA->getBBox() == NULL )
			faceA->setBBox(p1,p2,p3);
		BOP_BBox *boxA = faceA->getBBox();

	/* start checking B faces with the previously stored split index */

		for(unsigned int idxFaceB=faceA->getSplit();
		    idxFaceB<facesB->size() && (faceA->getTAG() != BROKEN) && (faceA->getTAG() != PHANTOM);) {
			BOP_Face *faceB = (*facesB)[idxFaceB];
			faceA->setSplit(idxFaceB);
			if ((faceB->getTAG() != BROKEN) && (faceB->getTAG() != PHANTOM)) {

				/* get (or create) bounding box for face B */
				if( faceB->getBBox() == NULL ) {
					faceB->setBBox(mesh->getVertex(faceB->getVertex(0))->getPoint(),
					               mesh->getVertex(faceB->getVertex(1))->getPoint(),
					               mesh->getVertex(faceB->getVertex(2))->getPoint());
				}
				BOP_BBox *boxB = faceB->getBBox();

				if (boxA->intersect(*boxB)) {
					MT_Plane3 planeB = faceB->getPlane();
					if (BOP_containsPoint(planeB,p1) &&
					        BOP_containsPoint(planeB,p2) &&
					        BOP_containsPoint(planeB,p3))
					{
						if (BOP_orientation(planeB,planeA)>0) {
							BOP_intersectCoplanarFaces(mesh,facesB,faceA,faceB,false);
						}
					}
					else {
						BOP_intersectNonCoplanarFaces(mesh,facesA,facesB,faceA,faceB);
					}
				}
			}
			idxFaceB++;
		}
	}
	
	
	// Clean broken faces from facesA
	BOP_IT_Faces it;
	it = facesA->begin();
	while (it != facesA->end()) {
		BOP_Face *face = *it;
		if (face->getTAG() == BROKEN) it = facesA->erase(it);
		else it++;
	}
	/*
	it = facesB->begin();
	while (it != facesB->end()) {
		BOP_Face *face = *it;
		if (face->getTAG() == BROKEN) it = facesB->erase(it);
		else it++;
	}
	*/
}

/**
 * Computes intesections of coplanars faces from object A with faces from object B.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param facesA set of faces from object A
 * @param facesB set of faces from object B
 */
void BOP_sew(BOP_Mesh *mesh, BOP_Faces *facesA, BOP_Faces *facesB)
{
	for(unsigned int idxFaceB = 0; idxFaceB < facesB->size(); idxFaceB++) {
		BOP_Face *faceB = (*facesB)[idxFaceB];
		MT_Plane3 planeB = faceB->getPlane();
		MT_Point3 p1 = mesh->getVertex(faceB->getVertex(0))->getPoint();
		MT_Point3 p2 = mesh->getVertex(faceB->getVertex(1))->getPoint();
		MT_Point3 p3 = mesh->getVertex(faceB->getVertex(2))->getPoint();
		
		for(unsigned int idxFaceA = 0;
			idxFaceA < facesA->size() &&
			faceB->getTAG() != BROKEN &&
			faceB->getTAG() != PHANTOM;
			idxFaceA++) {
			BOP_Face *faceA = (*facesA)[idxFaceA];
			if ((faceA->getTAG() != BROKEN)&&(faceA->getTAG() != PHANTOM)) {
				MT_Plane3 planeA = faceA->getPlane();
				if (BOP_containsPoint(planeA,p1) && 
					BOP_containsPoint(planeA,p2) && 
					BOP_containsPoint(planeA,p3)) {
					if (BOP_orientation(planeA,planeB) > 0) {
						BOP_intersectCoplanarFaces(mesh,facesA,faceB,faceA,true);
					}
				}
			}
		}
	}
}

/**
 * Triangulates faceB using edges of faceA that both are complanars.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param facesB set of faces from object B
 * @param faceA face from object A
 * @param faceB face from object B
 * @param invert indicates if faceA has priority over faceB
 */
void BOP_intersectCoplanarFaces(BOP_Mesh*  mesh,
								BOP_Faces* facesB, 
								BOP_Face*  faceA, 
								BOP_Face*  faceB, 
								bool       invert)
{
	unsigned int oldSize = facesB->size();
	unsigned int originalFaceB = faceB->getOriginalFace();    
	
	MT_Point3 p1 = mesh->getVertex(faceA->getVertex(0))->getPoint();
	MT_Point3 p2 = mesh->getVertex(faceA->getVertex(1))->getPoint();
	MT_Point3 p3 = mesh->getVertex(faceA->getVertex(2))->getPoint();
	
	MT_Vector3 normal(faceA->getPlane().x(),faceA->getPlane().y(),faceA->getPlane().z());
	
	MT_Vector3 p1p2 = p2-p1;
	
	MT_Plane3 plane1((p1p2.cross(normal).normalized()),p1);
	
	BOP_Segment sA;
	sA.m_cfg1 = BOP_Segment::createVertexCfg(1);
	sA.m_v1 = faceA->getVertex(0);
	sA.m_cfg2 = BOP_Segment::createVertexCfg(2);
	sA.m_v2 = faceA->getVertex(1);
	
	BOP_intersectCoplanarFaces(mesh,facesB,faceB,sA,plane1,invert);
	
	MT_Vector3 p2p3 = p3-p2;
	MT_Plane3 plane2((p2p3.cross(normal).normalized()),p2);
	
	sA.m_cfg1 = BOP_Segment::createVertexCfg(2);
	sA.m_v1 = faceA->getVertex(1);
	sA.m_cfg2 = BOP_Segment::createVertexCfg(3);
	sA.m_v2 = faceA->getVertex(2);
  
	if (faceB->getTAG() == BROKEN) {
		for(unsigned int idxFace = oldSize; idxFace < facesB->size(); idxFace++) {
			BOP_Face *face = (*facesB)[idxFace];
			if (face->getTAG() != BROKEN && originalFaceB == face->getOriginalFace())
				BOP_intersectCoplanarFaces(mesh,facesB,face,sA,plane2,invert);
		}
	}
	else {
		BOP_intersectCoplanarFaces(mesh,facesB,faceB,sA,plane2,invert);
	}
  
	MT_Vector3 p3p1 = p1-p3;
	MT_Plane3 plane3((p3p1.cross(normal).safe_normalized()),p3);
	
	sA.m_cfg1 = BOP_Segment::createVertexCfg(3);
	sA.m_v1 = faceA->getVertex(2);
	sA.m_cfg2 = BOP_Segment::createVertexCfg(1);
	sA.m_v2 = faceA->getVertex(0);
  
	if (faceB->getTAG() == BROKEN) {
		for(unsigned int idxFace = oldSize; idxFace < facesB->size(); idxFace++) {
			BOP_Face *face = (*facesB)[idxFace];
			if (face->getTAG() != BROKEN && originalFaceB == face->getOriginalFace())
				BOP_intersectCoplanarFaces(mesh,facesB,face,sA,plane3,invert);
		}
	}
	else {
		BOP_intersectCoplanarFaces(mesh,facesB,faceB,sA,plane3,invert);
	} 
}

/**
 * Triangulates faceB using segment sA and planeA.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param facesB set of faces from object B
 * @param faceB face from object B
 * @param sA segment to intersect with faceB
 * @param planeA plane to intersect with faceB
 * @param invert indicates if sA has priority over faceB
 */
void BOP_intersectCoplanarFaces(BOP_Mesh*   mesh, 
								BOP_Faces*  facesB, 
								BOP_Face*   faceB, 
								BOP_Segment sA, 
								MT_Plane3   planeA, 
								bool        invert)
{
	BOP_Segment sB = BOP_splitFace(planeA,mesh,faceB);

	if (BOP_Segment::isDefined(sB.m_cfg1)) {
		BOP_Segment xSegment[2];
		BOP_createXS(mesh,NULL,faceB,planeA,MT_Plane3(),sA,sB,invert,xSegment);
		if (BOP_Segment::isDefined(xSegment[1].m_cfg1)) {
			unsigned int sizefaces = mesh->getNumFaces();
			triangulate(mesh,facesB,faceB,xSegment[1]);
			BOP_mergeVertexs(mesh,sizefaces);
		}
	}
}

/**
 * Triangulates faceB using edges of faceA that both are not complanars.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param facesB set of faces from object B
 * @param faceA face from object A
 * @param faceB face from object B
 */
void BOP_intersectNonCoplanarFaces(BOP_Mesh *mesh, 
								   BOP_Faces *facesA, 
								   BOP_Faces *facesB, 
								   BOP_Face *faceA, 
								   BOP_Face *faceB)
{
	// Obtain segments of faces A and B from the intersection with their planes
	BOP_Segment sA = BOP_splitFace(faceB->getPlane(),mesh,faceA);
	BOP_Segment sB = BOP_splitFace(faceA->getPlane(),mesh,faceB);
	
	if (BOP_Segment::isDefined(sA.m_cfg1) && BOP_Segment::isDefined(sB.m_cfg1)) {    
		// There is an intesection, build the X-segment
		BOP_Segment xSegment[2];
		BOP_createXS(mesh,faceA,faceB,sA,sB,false,xSegment);
		
		unsigned int sizefaces = mesh->getNumFaces();
		triangulate(mesh,facesA,faceA,xSegment[0]);
		BOP_mergeVertexs(mesh,sizefaces);
	
		sizefaces = mesh->getNumFaces();
		triangulate(mesh,facesB,faceB,xSegment[1]);
		BOP_mergeVertexs(mesh,sizefaces);
	}
}

/**
 * Tests if faces since firstFace have all vertexs non-coincident of colinear, otherwise repairs the mesh.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param firstFace first face index to be tested
 */
void BOP_mergeVertexs(BOP_Mesh *mesh, unsigned int firstFace)
{
	unsigned int numFaces = mesh->getNumFaces();
	for(unsigned int idxFace = firstFace; idxFace < numFaces; idxFace++) {
		BOP_Face *face = mesh->getFace(idxFace);
		if ((face->getTAG() != BROKEN) && (face->getTAG() != PHANTOM)) {
			MT_Point3 vertex1 = mesh->getVertex(face->getVertex(0))->getPoint();
			MT_Point3 vertex2 = mesh->getVertex(face->getVertex(1))->getPoint();
			MT_Point3 vertex3 = mesh->getVertex(face->getVertex(2))->getPoint();
			if (BOP_collinear(vertex1,vertex2,vertex3)) // collinear triangle 
				face->setTAG(PHANTOM);
		}
	}
}

/**
 * Obtains the points of the segment created from the intersection between faceA and planeB.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faceA intersected face
 * @param sA segment of the intersection between  faceA and planeB
 * @param planeB intersected plane
 * @param points array of points where the new points are saved
 * @param faces array of relative face index to the points
 * @param size size of arrays points and faces
 * @param faceValue relative face index of new points
 */
void BOP_getPoints(BOP_Mesh*    mesh, 
				   BOP_Face*    faceA, 
				   BOP_Segment& sA, 
				   MT_Plane3    planeB, 
				   MT_Point3*   points, 
				   unsigned int*         faces, 
				   unsigned int&         size, 
				   unsigned int          faceValue) 
{
	MT_Point3 p1,p2;  
  
	if (BOP_Segment::isDefined(sA.m_cfg1)) {
		if (BOP_Segment::isEdge(sA.m_cfg1)) {
			// the new point becomes of split faceA edge 
			p1 = BOP_splitEdge(planeB,mesh,faceA,BOP_Segment::getEdge(sA.m_cfg1));
		}
		else if (BOP_Segment::isVertex(sA.m_cfg1)) {
			// the new point becomes of vertex faceA
			p1 =  mesh->getVertex(BOP_Segment::getVertex(sA.m_v1))->getPoint();
		}

		if (BOP_Segment::isDefined(sA.m_cfg2)) {
			if (BOP_Segment::isEdge(sA.m_cfg2)) {
				p2 = BOP_splitEdge(planeB,mesh,faceA,BOP_Segment::getEdge(sA.m_cfg2));
			}
			else if (BOP_Segment::isVertex(sA.m_cfg2)) { 
				p2 =  mesh->getVertex(BOP_Segment::getVertex(sA.m_v2))->getPoint();
			}
			points[size] = p1;
			points[size+1] = p2;
			faces[size] = faceValue;
			faces[size+1] = faceValue;
			size += 2;
		}
	
		else {
			points[size] = p1;
			faces[size] = faceValue;
			size++;
		}
	}
}

/**
 * Sorts the colinear points and relative face indices.
 * @param points array of points where the new points are saved
 * @param faces array of relative face index to the points
 * @param size size of arrays points and faces
 * @param invertA indicates if points of same relative face had been exchanged
 */
void BOP_mergeSort(MT_Point3 *points, unsigned int *face, unsigned int &size, bool &invertA, bool &invertB) {
	MT_Point3 sortedPoints[4];
	unsigned int sortedFaces[4], position[4];
	unsigned int i;
	if (size == 2) {

		// Trivial case, only test the merge ...
		if (BOP_fuzzyZero(points[0].distance(points[1]))) {
			face[0] = 3;
			size--;
		}
	}
	else {
		// size is 3 or 4
		// Get segment extreme points
		MT_Scalar maxDistance = -1;
		for(i=0;i<size-1;i++){
			for(unsigned int j=i+1;j<size;j++){
				MT_Scalar distance = points[i].distance(points[j]);
				if (distance > maxDistance){
					maxDistance = distance;
					position[0] = i;
					position[size-1] = j;
				}
			}
		}

		// Get segment inner points
		position[1] = position[2] = size;
		for(i=0;i<size;i++){
			if ((i != position[0]) && (i != position[size-1])){
				if (position[1] == size) position[1] = i;
				else position[2] = i;
			}
		}

		// Get inner points
		if (position[2] < size) {
			MT_Scalar d1 = points[position[1]].distance(points[position[0]]);
			MT_Scalar d2 = points[position[2]].distance(points[position[0]]);
			if (d1 > d2) {
				unsigned int aux = position[1];
				position[1] = position[2];
				position[2] = aux;
			}
		}

		// Sort data
		for(i=0;i<size;i++) {
			sortedPoints[i] = points[position[i]];
			sortedFaces[i] = face[position[i]];
		}

		invertA = false;
		invertB = false;
		if (face[1] == 1) {

			// invertAø?
			for(i=0;i<size;i++) {
				if (position[i] == 1) {
					invertA = true;
					break;
				}
				else if (position[i] == 0) break;
			}

			// invertBø?
			if (size == 4) {
				for(i=0;i<size;i++) {
					if (position[i] == 3) {
						invertB = true;
						break;
					}
					else if (position[i] == 2) break;
				}
			}
		}
		else if (face[1] == 2) {
			// invertBø?
			for(i=0;i<size;i++) {
				if (position[i] == 2) {
					invertB = true;
					break;
				}
				else if (position[i] == 1) break;
			}
		}


		// Merge data
		MT_Scalar d1 = sortedPoints[1].distance(sortedPoints[0]);
		MT_Scalar d2 = sortedPoints[1].distance(sortedPoints[2]);
		if (BOP_fuzzyZero(d1) && sortedFaces[1] != sortedFaces[0]) {
			if (BOP_fuzzyZero(d2) && sortedFaces[1] != sortedFaces[2])  {
				if (d1 < d2) {
					// merge 0 and 1
					sortedFaces[0] = 3;
					for(i = 1; i<size-1;i++) {
						sortedPoints[i] = sortedPoints[i+1];
						sortedFaces[i] = sortedFaces[i+1];
					}
					size--;
					if (size == 3) {
						// merge 1 and 2 ???
						d1 = sortedPoints[1].distance(sortedPoints[2]);
						if (BOP_fuzzyZero(d1) && sortedFaces[1] != sortedFaces[2])  {
							// merge!
							sortedFaces[1] = 3;
							size--;
						}
					}
				}
				else {
					// merge 1 and 2
					sortedFaces[1] = 3;
					for(i = 2; i<size-1;i++) {
						sortedPoints[i] = sortedPoints[i+1];
						sortedFaces[i] = sortedFaces[i+1];
					}
					size--;
				}
			}
			else {
				// merge 0 and 1
				sortedFaces[0] = 3;
				for(i = 1; i<size-1;i++) {
					sortedPoints[i] = sortedPoints[i+1];
					sortedFaces[i] = sortedFaces[i+1];
				}
				size--;
				if (size == 3) {
					// merge 1 i 2 ???
					d1 = sortedPoints[1].distance(sortedPoints[2]);
					if (BOP_fuzzyZero(d1) && sortedFaces[1] != sortedFaces[2])  {
						// merge!
						sortedFaces[1] = 3;
						size--;
					}
				}
			}
		}
		else {
			if (BOP_fuzzyZero(d2) && sortedFaces[1] != sortedFaces[2])  {
				// merge 1 and 2
				sortedFaces[1] = 3;
				for(i = 2; i<size-1;i++) {
					sortedPoints[i] = sortedPoints[i+1];
					sortedFaces[i] = sortedFaces[i+1];
				}
				size--;
			}
			else if (size == 4) {
				d1 = sortedPoints[2].distance(sortedPoints[3]);
				if (BOP_fuzzyZero(d1) && sortedFaces[2] != sortedFaces[3])  {
					// merge 2 and 3
					sortedFaces[2] = 3;
					size--;
				}
			}
		}

		// Merge initial points ...
		for(i=0;i<size;i++) {
			points[i] = sortedPoints[i];
			face[i] = sortedFaces[i];
		}

	}
}


/**
 * Computes the x-segment of two segments (the shared interval). The segments needs to have sA.m_cfg1 > 0 && sB.m_cfg1 > 0 .
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faceA face of object A
 * @param faceB face of object B
 * @param sA segment of intersection between faceA and planeB
 * @param sB segment of intersection between faceB and planeA
 * @param invert indicates if faceA has priority over faceB
 * @param segmemts array of the output x-segments
 */
void BOP_createXS(BOP_Mesh*    mesh,
                  BOP_Face*    faceA,
                  BOP_Face*    faceB,
                  BOP_Segment  sA,
                  BOP_Segment  sB,
                  bool         invert,
                  BOP_Segment* segments) {
	BOP_createXS(mesh, faceA, faceB, faceA->getPlane(), faceB->getPlane(),
	             sA, sB, invert, segments);
}

/**
 * Computes the x-segment of two segments (the shared interval). The segments needs to have sA.m_cfg1 > 0 && sB.m_cfg1 > 0 .
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faceA face of object A
 * @param faceB face of object B
 * @param planeA plane of faceA
 * @param planeB plane of faceB
 * @param sA segment of intersection between faceA and planeB
 * @param sB segment of intersection between faceB and planeA
 * @param invert indicates if faceA has priority over faceB
 * @param segmemts array of the output x-segments
 */
void BOP_createXS(BOP_Mesh*    mesh, 
				  BOP_Face*    faceA, 
				  BOP_Face*    faceB, 
				  MT_Plane3    planeA, 
				  MT_Plane3    planeB, 
				  BOP_Segment  sA, 
				  BOP_Segment  sB, 
				  bool         invert, 
				  BOP_Segment* segments)
{    
	MT_Point3 points[4];  // points of the segments
	unsigned int face[4]; // relative face indexs (1 => faceA, 2 => faceB)
	unsigned int size = 0; 	// size of points and relative face indexs
  
	BOP_getPoints(mesh, faceA, sA, planeB, points, face, size, 1);
	BOP_getPoints(mesh, faceB, sB, planeA, points, face, size, 2);

	bool invertA = false;
	bool invertB = false;
	BOP_mergeSort(points,face,size,invertA,invertB);

	if (invertA) sA.invert();
	if (invertB) sB.invert();
	
	// Compute the configuration label
	unsigned int label = 0;
	for(unsigned int i =0; i < size; i++) {
		label = face[i]+label*10;    
	}

	if (size == 1) {
		// Two coincident points
		segments[0].m_cfg1 = sA.m_cfg1;
		segments[1].m_cfg1 = sB.m_cfg1;
		
		segments[0].m_v1 = BOP_getVertexIndex(mesh, points[0], sA.m_cfg1, sB.m_cfg1,
											  sA.m_v1, sB.m_v1, invert);
		segments[1].m_v1 = segments[0].m_v1;
		segments[0].m_cfg2 = segments[1].m_cfg2 = BOP_Segment::createUndefinedCfg();
	}
	else if (size == 2) {
		switch(label) {
		// Two non-coincident points
		case sA_sB:
		case sB_sA:
			segments[0].m_cfg1 = 
			segments[1].m_cfg1 = 
			segments[0].m_cfg2 = 
			segments[1].m_cfg2 = BOP_Segment::createUndefinedCfg();
			break;
      
		// Two coincident points and one non-coincident of sA
		case sA_sX:
			segments[0].m_cfg1 = sA.m_cfg2;
			segments[1].m_cfg1 = sB.m_cfg1;
			segments[0].m_v1 = BOP_getVertexIndex(mesh, points[1], sA.m_cfg2, sB.m_cfg1,
												  sA.m_v2, sB.m_v1, invert);
			segments[1].m_v1 = segments[0].m_v1;
			
			segments[0].m_cfg2 = BOP_Segment::createUndefinedCfg();
			segments[1].m_cfg2 = BOP_Segment::createUndefinedCfg();
			break;
		case sX_sA:
			segments[0].m_cfg1 = sA.m_cfg1;
			segments[1].m_cfg1 = sB.m_cfg1;
			segments[0].m_v1 = BOP_getVertexIndex(mesh, points[0], sA.m_cfg1, sB.m_cfg1,
												  sA.m_v1, sB.m_v1, invert);
			segments[1].m_v1 = segments[0].m_v1;
			
			segments[0].m_cfg2 = BOP_Segment::createUndefinedCfg();
			segments[1].m_cfg2 = BOP_Segment::createUndefinedCfg();
			break;
      
		// Two coincident points and one non-coincident of sB
		case sB_sX:
			segments[0].m_cfg1 = sA.m_cfg1;
			segments[1].m_cfg1 = sB.m_cfg2;
			segments[0].m_v1 = BOP_getVertexIndex(mesh, points[1], sA.m_cfg1, sB.m_cfg2, 
												  sA.m_v1, sB.m_v2, invert);
			segments[1].m_v1 = segments[0].m_v1;
			
			segments[0].m_cfg2 = BOP_Segment::createUndefinedCfg();
			segments[1].m_cfg2 = BOP_Segment::createUndefinedCfg();
			break;
		case sX_sB:
			segments[0].m_cfg1 = sA.m_cfg1;
			segments[1].m_cfg1 = sB.m_cfg1;
			segments[0].m_v1 = BOP_getVertexIndex(mesh, points[0], sA.m_cfg1, sB.m_cfg1, 
												  sA.m_v1, sB.m_v1, invert);
			segments[1].m_v1 = segments[0].m_v1;
		
			segments[0].m_cfg2 = BOP_Segment::createUndefinedCfg();
			segments[1].m_cfg2 = BOP_Segment::createUndefinedCfg();
			break;
      
		// coincident points 2-2
		case sX_sX:
			segments[0].m_cfg1 = sA.m_cfg1;
			segments[1].m_cfg1 = sB.m_cfg1;          
			segments[0].m_v1 = BOP_getVertexIndex(mesh, points[0], sA.m_cfg1, sB.m_cfg1, 
												  sA.m_v1, sB.m_v1, invert);
			segments[1].m_v1 = segments[0].m_v1;
			
			segments[0].m_cfg2 = sA.m_cfg2;
			segments[1].m_cfg2 = sB.m_cfg2;          
			segments[0].m_v2 = BOP_getVertexIndex(mesh, points[1], sA.m_cfg2, sB.m_cfg2, 
												  sA.m_v2, sB.m_v2, invert);
			segments[1].m_v2 = segments[0].m_v2;
			break;
		
		default:
			break; 
		}
	}
	else if (size == 3) {
		switch(label) {
		case sA_sA_sB:
		case sB_sA_sA:
		case sA_sB_sB:
		case sB_sB_sA:
			segments[0].m_cfg1 = 
			segments[1].m_cfg1 = 
			segments[0].m_cfg2 = 
			segments[1].m_cfg2 = BOP_Segment::createUndefinedCfg();
			break;
      
		case sA_sB_sA:
			segments[1].m_v1 = BOP_getVertexIndex(mesh,points[1],sB.m_cfg1,sB.m_v1);
			segments[1].m_cfg1 = sB.m_cfg1;
			segments[1].m_cfg2 = BOP_Segment::createUndefinedCfg();
			segments[0].m_cfg1 = sA.getConfig();
			segments[0].m_cfg2 = BOP_Segment::createUndefinedCfg();
			segments[0].m_v1 = segments[1].m_v1;
			break;
      
		case sB_sA_sB:
			segments[0].m_v1 = BOP_getVertexIndex(mesh,points[1],sA.m_cfg1,sA.m_v1);
			segments[0].m_cfg1 = sA.m_cfg1;
			segments[0].m_cfg2 = BOP_Segment::createUndefinedCfg();
			segments[1].m_cfg1 = sB.getConfig();
			segments[1].m_cfg2 = BOP_Segment::createUndefinedCfg();
			segments[1].m_v1 = segments[0].m_v1;
			break;
      
		case sA_sX_sB:
			segments[0].m_cfg1 = sA.m_cfg2;
			segments[1].m_cfg1 = sB.m_cfg1;          
			segments[0].m_v1 = BOP_getVertexIndex(mesh, points[1], sA.m_cfg2, sB.m_cfg1, 
												  sA.m_v2, sB.m_v1, invert);
			segments[1].m_v1 = segments[0].m_v1;
			segments[0].m_cfg2 = BOP_Segment::createUndefinedCfg();
			segments[1].m_cfg2 = BOP_Segment::createUndefinedCfg();
			break;
    
		case sB_sX_sA:
			segments[0].m_cfg1 = sA.m_cfg1;
			segments[1].m_cfg1 = sB.m_cfg2;          
			segments[0].m_v1 = BOP_getVertexIndex(mesh, points[1], sA.m_cfg1, sB.m_cfg2, 
												  sA.m_v1, sB.m_v2, invert);
			segments[1].m_v1 = segments[0].m_v1;
			segments[0].m_cfg2 = BOP_Segment::createUndefinedCfg();
			segments[1].m_cfg2 = BOP_Segment::createUndefinedCfg();
			break;
      
		case sX_sA_sB:
			segments[0].m_cfg1 = sA.m_cfg1;
			segments[1].m_cfg1 = sB.m_cfg1;          
			segments[0].m_v1 = BOP_getVertexIndex(mesh, points[0], sA.m_cfg1, sB.m_cfg1,
												  sA.m_v1, sB.m_v1, invert);
			segments[1].m_v1 = segments[0].m_v1;
			
			segments[0].m_cfg2 = sA.m_cfg2;
			segments[1].m_cfg2 = sB.getConfig();
			segments[0].m_v2 = BOP_getVertexIndex(mesh, points[1], sA.m_cfg2, sA.m_v2);
			segments[1].m_v2 = segments[0].m_v2;
			break;
      
		case sX_sB_sA:
			segments[0].m_cfg1 = sA.m_cfg1;
			segments[1].m_cfg1 = sB.m_cfg1;          
			segments[0].m_v1 = BOP_getVertexIndex(mesh, points[0], sA.m_cfg1, sB.m_cfg1,
												  sA.m_v1, sB.m_v1, invert);
			segments[1].m_v1 = segments[0].m_v1;
			
			segments[0].m_cfg2 = sA.getConfig();
			segments[1].m_cfg2 = sB.m_cfg2;
			segments[0].m_v2 = BOP_getVertexIndex(mesh,points[1],sB.m_cfg2,sB.m_v2);
			segments[1].m_v2 = segments[0].m_v2;
			break;
      
		case sA_sB_sX:
			segments[0].m_cfg1 = sA.getConfig();
			segments[1].m_cfg1 = sB.m_cfg1;
			segments[0].m_v1 = BOP_getVertexIndex(mesh,points[1],sB.m_cfg1,sB.m_v1);
			segments[1].m_v1 = segments[0].m_v1;
			
			segments[0].m_cfg2 = sA.m_cfg2;
			segments[1].m_cfg2 = sB.m_cfg2;          
			segments[0].m_v2 = BOP_getVertexIndex(mesh, points[2], sA.m_cfg2, sB.m_cfg2, 
												  sA.m_v2, sB.m_v2, invert);
			segments[1].m_v2 = segments[0].m_v2;
			break;

		case sB_sA_sX:
			segments[0].m_cfg1 = sA.m_cfg1;
			segments[1].m_cfg1 = sB.getConfig();
			segments[0].m_v1 = BOP_getVertexIndex(mesh,points[1],sA.m_cfg1,sA.m_v1);
			segments[1].m_v1 = segments[0].m_v1;
			
			segments[0].m_cfg2 = sA.m_cfg2;
			segments[1].m_cfg2 = sB.m_cfg2;          
			segments[0].m_v2 = BOP_getVertexIndex(mesh, points[2], sA.m_cfg2, sB.m_cfg2,
												  sA.m_v2, sB.m_v2, invert);
			segments[1].m_v2 = segments[0].m_v2;
			break;
      
		default:
			break; 
		}
	}
	else {
		// 4!
		switch(label) {
		case sA_sA_sB_sB:
		case sB_sB_sA_sA:
			segments[0].m_cfg1 = 
			segments[1].m_cfg1 = 
			segments[0].m_cfg2 = 
			segments[1].m_cfg2 = BOP_Segment::createUndefinedCfg();
			break;
      
		case sA_sB_sA_sB:
			segments[0].m_cfg1 = sA.getConfig();
			segments[1].m_cfg1 = sB.m_cfg1;
			segments[0].m_v1 = BOP_getVertexIndex(mesh,points[1],sB.m_cfg1,sB.m_v1);
			segments[1].m_v1 = segments[0].m_v1;
			
			segments[0].m_cfg2 = sA.m_cfg2;
			segments[1].m_cfg2 = sB.getConfig();
			segments[0].m_v2 = BOP_getVertexIndex(mesh,points[2],sA.m_cfg2,sA.m_v2);
			segments[1].m_v2 = segments[0].m_v2;
			break;          
      
		case sB_sA_sB_sA:
			segments[0].m_cfg1 = sA.m_cfg1;
			segments[1].m_cfg1 = sB.getConfig();
			segments[0].m_v1 = BOP_getVertexIndex(mesh,points[1],sA.m_cfg1,sA.m_v1);
			segments[1].m_v1 = segments[0].m_v1;
			
			segments[0].m_cfg2 = sA.getConfig();
			segments[1].m_cfg2 = sB.m_cfg2;
			segments[0].m_v2 = BOP_getVertexIndex(mesh,points[2],sB.m_cfg2,sB.m_v2);
			segments[1].m_v2 = segments[0].m_v2;
			break;          
      
		case sA_sB_sB_sA:
			segments[0].m_cfg1 = sA.getConfig();
			segments[1].m_cfg1 = sB.m_cfg1;
			segments[0].m_v1 = BOP_getVertexIndex(mesh,points[1],sB.m_cfg1,sB.m_v1);
			segments[1].m_v1 = segments[0].m_v1;
			
			segments[0].m_cfg2 = segments[0].m_cfg1;
			segments[1].m_cfg2 = sB.m_cfg2;
			segments[0].m_v2 = BOP_getVertexIndex(mesh,points[2],sB.m_cfg2,sB.m_v2);
			segments[1].m_v2 = segments[0].m_v2;
			break;
      
		case sB_sA_sA_sB:
			segments[0].m_cfg1 = sA.m_cfg1;
			segments[1].m_cfg1 = sB.getConfig();
			segments[0].m_v1 = BOP_getVertexIndex(mesh,points[1],sA.m_cfg1,sA.m_v1);
			segments[1].m_v1 = segments[0].m_v1;
			
			segments[0].m_cfg2 = sA.m_cfg2;
			segments[1].m_cfg2 = segments[1].m_cfg1;
			segments[0].m_v2 = BOP_getVertexIndex(mesh,points[2],sA.m_cfg2,sA.m_v2);
			segments[1].m_v2 = segments[0].m_v2;
			break;
      
		default:
			break; 
		}
	}
	
	segments[0].sort();
	segments[1].sort();
}

/**
 * Computes the vertex index of a point.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param point input point
 * @param cfgA configuration of point on faceA
 * @param cfgB configuration of point on faceB
 * @param vA vertex index of point on faceA
 * @param vB vertex index of point on faceB
 * @param invert indicates if vA has priority over vB
 * @return final vertex index in the mesh
 */
BOP_Index BOP_getVertexIndex(BOP_Mesh* mesh, 
					   MT_Point3 point, 
					   unsigned int       cfgA, 
					   unsigned int       cfgB, 
					   BOP_Index       vA, 
					   BOP_Index       vB, 
					   bool      invert)
{
	if (BOP_Segment::isVertex(cfgA)) { // exists vertex index on A
		if (BOP_Segment::isVertex(cfgB)) { // exists vertex index on B
			// unify vertex indexs
			if (invert)
				return mesh->replaceVertexIndex(vA,vB);
			else 
				return mesh->replaceVertexIndex(vB,vA);
		}
		else
			return vA;
	}
	else {// does not exist vertex index on A
		if (BOP_Segment::isVertex(cfgB)) // exists vertex index on B
			return vB;	
		else {// does not exist vertex index on B
			return mesh->addVertex(point);
		}
	} 
}

/**
 * Computes the vertex index of a point.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param cfg configuration of point
 * @param v vertex index of point
 * @return final vertex index in the mesh
 */
BOP_Index BOP_getVertexIndex(BOP_Mesh *mesh, MT_Point3 point, unsigned int cfg, BOP_Index v)
{
	if (BOP_Segment::isVertex(cfg)) // vertex existent
		return v;	
	else {
		return mesh->addVertex(point);
	}
}

/******************************************************************************/
/*** TRIANGULATE                                                            ***/
/******************************************************************************/

/**
 * Triangulates the input face according to the specified segment.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faces set of faces that contains the original face and the new triangulated faces
 * @param face face to be triangulated
 * @param s segment used to triangulate face
 */
void triangulate(BOP_Mesh *mesh, BOP_Faces *faces, BOP_Face *face, BOP_Segment s)
{
	if (BOP_Segment::isUndefined(s.m_cfg1)) {
		// Nothing to do
	}
	else if (BOP_Segment::isVertex(s.m_cfg1)) {
		// VERTEX(v1) + VERTEX(v2) => nothing to do
	}
	else if (BOP_Segment::isEdge(s.m_cfg1)) {
		if (BOP_Segment::isVertex(s.m_cfg2) || BOP_Segment::isUndefined(s.m_cfg2)) {
			// EDGE(v1) + VERTEX(v2)
			BOP_Edge *edge = mesh->getEdge(face,BOP_Segment::getEdge(s.m_cfg1));
			BOP_triangulateA(mesh,faces,face,s.m_v1,BOP_Segment::getEdge(s.m_cfg1));
			BOP_Face *opposite = BOP_getOppositeFace(mesh,faces,face,edge);
			if (opposite != NULL) {
			  unsigned int e;
			  opposite->getEdgeIndex(edge->getVertex1(), edge->getVertex2(),e);
			  BOP_triangulateA(mesh, faces, opposite, s.m_v1, e);
			}
		}
		else {
			//  EDGE(v1) + EDGE(v2)
			if (BOP_Segment::getEdge(s.m_cfg1) == BOP_Segment::getEdge(s.m_cfg2)) {
				// EDGE(v1) == EDGE(v2)
				BOP_Edge *edge = mesh->getEdge(face,BOP_Segment::getEdge(s.m_cfg1));
				BOP_triangulateD(mesh, faces, face, s.m_v1, s.m_v2,
				                 BOP_Segment::getEdge(s.m_cfg1));
				BOP_Face *opposite = BOP_getOppositeFace(mesh,faces,face,edge);
				if (opposite != NULL) {
					unsigned int e;
					opposite->getEdgeIndex(edge->getVertex1(), edge->getVertex2(),e);
					BOP_triangulateD(mesh, faces, opposite, s.m_v1, s.m_v2, e);
				}
			}
			else { // EDGE(v1) != EDGE(v2)
				BOP_Edge *edge1 = mesh->getEdge(face,BOP_Segment::getEdge(s.m_cfg1));
				BOP_Edge *edge2 = mesh->getEdge(face,BOP_Segment::getEdge(s.m_cfg2));
				BOP_triangulateE(mesh, faces, face, s.m_v1, s.m_v2,
								 BOP_Segment::getEdge(s.m_cfg1),
								 BOP_Segment::getEdge(s.m_cfg2));
				BOP_Face *opposite = BOP_getOppositeFace(mesh,faces,face,edge1);
				if (opposite != NULL) {
				  unsigned int e;
				  opposite->getEdgeIndex(edge1->getVertex1(), edge1->getVertex2(),e);
				  BOP_triangulateA(mesh, faces, opposite, s.m_v1, e);
				}
				opposite = BOP_getOppositeFace(mesh,faces,face,edge2);
				if (opposite != NULL) {
				  unsigned int e;
				  opposite->getEdgeIndex(edge2->getVertex1(), edge2->getVertex2(),e);
				  BOP_triangulateA(mesh, faces, opposite, s.m_v2, e);
				}
			}
		}
	}
	else if (BOP_Segment::isIn(s.m_cfg1)) {
		if (BOP_Segment::isVertex(s.m_cfg2) || BOP_Segment::isUndefined(s.m_cfg2)) {
			// IN(v1) + VERTEX(v2)
			BOP_triangulateB(mesh,faces,face,s.m_v1);
		}
		else if (BOP_Segment::isEdge(s.m_cfg2)) {
			// IN(v1) + EDGE(v2)
			BOP_Edge *edge = mesh->getEdge(face,BOP_Segment::getEdge(s.m_cfg2));
			BOP_triangulateF(mesh,faces,face,s.m_v1,s.m_v2,BOP_Segment::getEdge(s.m_cfg2));
			BOP_Face *opposite = BOP_getOppositeFace(mesh,faces,face,edge);
			if (opposite != NULL) {
			  unsigned int e;
			  opposite->getEdgeIndex(edge->getVertex1(), edge->getVertex2(),e);
			  BOP_triangulateA(mesh, faces, opposite, s.m_v2, e);
			}
		}
		else // IN(v1) + IN(v2)
			BOP_triangulateC(mesh,faces,face,s.m_v1,s.m_v2);
	}
}

/**
 * Returns if a face is in the set of faces.
 * @param faces set of faces
 * @param face face to be searched
 * @return if the face is inside faces
 */
bool BOP_containsFace(BOP_Faces *faces, BOP_Face *face)
{
  const BOP_IT_Faces facesEnd = faces->end();
	for(BOP_IT_Faces it=faces->begin();it!=facesEnd;it++)
	{
		if (*it == face)
		return true;
	}
	
	return false;
}

/**
 * Returns the first face of faces that shares the input edge of face.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faces set of faces
 * @param face input face
 * @param edge face's edge
 * @return first face that shares the edge of input face
 */
BOP_Face *BOP_getOppositeFace(BOP_Mesh*  mesh, 
							  BOP_Faces* faces, 
							  BOP_Face*  face,
							  BOP_Edge*  edge)
{
	if (edge == NULL)
	  return NULL;
  
	BOP_Indexs auxfaces = edge->getFaces();
	const BOP_IT_Indexs auxfacesEnd = auxfaces.end();
	for(BOP_IT_Indexs it = auxfaces.begin(); it != auxfacesEnd; it++) {
		BOP_Face *auxface = mesh->getFace(*it);
		if ((auxface != face) && (auxface->getTAG()!=BROKEN) && 
			BOP_containsFace(faces,auxface)) {
			return auxface;
		}
	}        
  
	return NULL;
}

/******************************************************************************/ 
/***  OVERLAPPING                                                           ***/
/******************************************************************************/ 

/**
 * Removes faces from facesB that are overlapped with anyone from facesA.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param facesA set of faces from object A
 * @param facesB set of faces from object B
 */
void BOP_removeOverlappedFaces(BOP_Mesh *mesh,  BOP_Faces *facesA,  BOP_Faces *facesB)
{
	for(unsigned int i=0;i<facesA->size();i++) {
		BOP_Face *faceI = (*facesA)[i];
		if (faceI->getTAG()==BROKEN) continue;
		bool overlapped = false;
		MT_Point3 p1 = mesh->getVertex(faceI->getVertex(0))->getPoint();
		MT_Point3 p2 = mesh->getVertex(faceI->getVertex(1))->getPoint();
		MT_Point3 p3 = mesh->getVertex(faceI->getVertex(2))->getPoint();
		for(unsigned int j=0;j<facesB->size();) {
			BOP_Face *faceJ = (*facesB)[j];
			if (faceJ->getTAG()!=BROKEN) {
				MT_Plane3 planeJ = faceJ->getPlane();
				if (BOP_containsPoint(planeJ,p1) && BOP_containsPoint(planeJ,p2)
				        && BOP_containsPoint(planeJ,p3))
				{
					MT_Point3 q1 = mesh->getVertex(faceJ->getVertex(0))->getPoint();
					MT_Point3 q2 = mesh->getVertex(faceJ->getVertex(1))->getPoint();
					MT_Point3 q3 = mesh->getVertex(faceJ->getVertex(2))->getPoint();
					if (BOP_overlap(MT_Vector3(planeJ.x(),planeJ.y(),planeJ.z()),
					                p1,p2,p3,q1,q2,q3))
					{
						facesB->erase(facesB->begin()+j,facesB->begin()+(j+1));
						faceJ->setTAG(BROKEN);
						overlapped = true;
					}
					else j++;
				}
				else j++;
			}else j++;
		}
		if (overlapped) faceI->setTAG(OVERLAPPED);
	}
}

/**
 * Computes if triangle p1,p2,p3 is overlapped with triangle q1,q2,q3.
 * @param normal normal of the triangle p1,p2,p3
 * @param p1 point of first triangle
 * @param p2 point of first triangle
 * @param p3 point of first triangle
 * @param q1 point of second triangle
 * @param q2 point of second triangle
 * @param q3 point of second triangle
 * @return if there is overlapping between both triangles
 */
bool BOP_overlap(MT_Vector3 normal, MT_Point3 p1, MT_Point3 p2, MT_Point3 p3, 
				  MT_Point3 q1, MT_Point3 q2, MT_Point3 q3)
{
	MT_Vector3 p1p2 = p2-p1;    
	MT_Plane3 plane1(p1p2.cross(normal),p1);
	
	MT_Vector3 p2p3 = p3-p2;
	MT_Plane3 plane2(p2p3.cross(normal),p2);
	
	MT_Vector3 p3p1 = p1-p3;    
	MT_Plane3 plane3(p3p1.cross(normal),p3);
  
	BOP_TAG tag1 = BOP_createTAG(BOP_classify(q1,plane1));
	BOP_TAG tag2 = BOP_createTAG(BOP_classify(q1,plane2));
	BOP_TAG tag3 = BOP_createTAG(BOP_classify(q1,plane3));
	BOP_TAG tagQ1 = BOP_createTAG(tag1,tag2,tag3);   
	if (tagQ1 == IN_IN_IN) return true;    
  
	tag1 = BOP_createTAG(BOP_classify(q2,plane1));
	tag2 = BOP_createTAG(BOP_classify(q2,plane2));
	tag3 = BOP_createTAG(BOP_classify(q2,plane3));
	BOP_TAG tagQ2 = BOP_createTAG(tag1,tag2,tag3);
	if (tagQ2 == IN_IN_IN) return true;    
	
	tag1 = BOP_createTAG(BOP_classify(q3,plane1));
	tag2 = BOP_createTAG(BOP_classify(q3,plane2));
	tag3 = BOP_createTAG(BOP_classify(q3,plane3));
	BOP_TAG tagQ3 = BOP_createTAG(tag1,tag2,tag3);
	if (tagQ3 == IN_IN_IN) return true;    
  
	if ((tagQ1 & OUT_OUT_OUT) == 0 && (tagQ2 & OUT_OUT_OUT) == 0 && 
		(tagQ3 & OUT_OUT_OUT) == 0) return true;
	else return false;
}
