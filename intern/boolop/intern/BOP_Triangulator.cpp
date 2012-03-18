/*
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file boolop/intern/BOP_Triangulator.cpp
 *  \ingroup boolopintern
 */

 
#include "BOP_Triangulator.h"
#include <iostream>

void BOP_addFace(BOP_Mesh* mesh, BOP_Faces *faces, BOP_Face* face, BOP_TAG tag);
void BOP_splitQuad(BOP_Mesh* mesh, MT_Plane3 plane, BOP_Index v1, BOP_Index v2, BOP_Index v3, BOP_Index v4, 
				   BOP_Face* triangles[], BOP_Index original);
BOP_Index BOP_getTriangleVertex(BOP_Mesh* mesh, BOP_Index v1, BOP_Index v2, BOP_Index v3, BOP_Index v4);
BOP_Index BOP_getNearestVertex(BOP_Mesh* mesh, BOP_Index u, BOP_Index v1, BOP_Index v2);
bool BOP_isInsideCircle(BOP_Mesh* mesh, BOP_Index v1, BOP_Index v2, BOP_Index v3, BOP_Index v4, BOP_Index v5);
bool BOP_isInsideCircle(BOP_Mesh* mesh, BOP_Index v1, BOP_Index v2, BOP_Index v3, BOP_Index w);
void BOP_triangulateC_split(BOP_Mesh* mesh, BOP_Faces* faces, BOP_Face* face, 
							BOP_Index v1, BOP_Index v2, BOP_Index v3, BOP_Index v4, BOP_Index v5);
void BOP_triangulateD_split(BOP_Mesh* mesh, BOP_Faces* faces, BOP_Face* face, 
							BOP_Index v1, BOP_Index v2, BOP_Index v3, BOP_Index v4, BOP_Index v5);

/**
 * Triangulates the face in two new faces by splitting one edge.
 *
 *         *
 *        /|\
 *       / | \
 *      /  |  \
 *     /   |   \
 *    /    |    \
 *   *-----x-----*
 *
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faces set of faces that contains face and will contains new faces
 * @param face input face to be triangulate
 * @param v vertex index that intersects the edge
 * @param e relative edge index used to triangulate the face
 */


void BOP_triangulateA(BOP_Mesh *mesh, BOP_Faces *faces, BOP_Face * face, BOP_Index v, unsigned int e)
{
	BOP_Face *face1, *face2;
	if (e == 1) {
		face1 = new BOP_Face3(face->getVertex(0), v, face->getVertex(2), face->getPlane(),
							  face->getOriginalFace());
		face2 = new BOP_Face3(v, face->getVertex(1), face->getVertex(2), face->getPlane(),
							  face->getOriginalFace());
	}
	else if (e == 2) {
		face1 = new BOP_Face3(face->getVertex(0), face->getVertex(1), v, face->getPlane(),
							  face->getOriginalFace());
		face2 = new BOP_Face3(face->getVertex(0), v, face->getVertex(2), face->getPlane(),
							  face->getOriginalFace());
	}
	else if (e == 3) {
		face1 = new BOP_Face3(face->getVertex(0), face->getVertex(1), v, face->getPlane(),
							  face->getOriginalFace());
		face2 = new BOP_Face3(face->getVertex(1), face->getVertex(2), v, face->getPlane(),
							  face->getOriginalFace());
	}
	else {
		return;
	}
	
	BOP_addFace(mesh, faces, face1, face->getTAG());
	BOP_addFace(mesh, faces, face2, face->getTAG());
	face1->setSplit(face->getSplit());
	face2->setSplit(face->getSplit());
	
	face->setTAG(BROKEN);
	face->freeBBox();
}

/**
 * Triangulates the face in three new faces by one inner point.
 *
 *         *
 *        / \
 *       /   \
 *      /     \
 *     /   x   \
 *    /         \
 *   *-----------*
 *
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faces set of faces that contains face and will contains new faces
 * @param face input face to be triangulate
 * @param v vertex index that lays inside face
 */
void BOP_triangulateB(BOP_Mesh* mesh, BOP_Faces* faces, BOP_Face* face, BOP_Index v)
{
	BOP_Face *face1 = new BOP_Face3(face->getVertex(0), face->getVertex(1), v, face->getPlane(),
									face->getOriginalFace());
	BOP_Face *face2 = new BOP_Face3(face->getVertex(1), face->getVertex(2), v, face->getPlane(),
									face->getOriginalFace());
	BOP_Face *face3 = new BOP_Face3(face->getVertex(2), face->getVertex(0), v, face->getPlane(),
									face->getOriginalFace());
  
	BOP_addFace(mesh,faces,face1,face->getTAG());
	BOP_addFace(mesh,faces,face2,face->getTAG());
	BOP_addFace(mesh,faces,face3,face->getTAG());
	face1->setSplit(face->getSplit());
	face2->setSplit(face->getSplit());
	face3->setSplit(face->getSplit());
	face->setTAG(BROKEN);
	face->freeBBox();
}


/**
 * Triangulates the face in five new faces by two inner points.
 *
 *         *
 *        / \
 *       /   \
 *      /     \
 *     /  x x  \
 *    /         \
 *   *-----------*
 *
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faces set of faces that contains face and will contains new faces
 * @param face input face to be triangulate
 * @param v1 first vertex index that lays inside face
 * @param v2 second vertex index that lays inside face
 */
void BOP_triangulateC(BOP_Mesh* mesh, BOP_Faces* faces, BOP_Face* face, BOP_Index v1, BOP_Index v2)
{
	if (!BOP_isInsideCircle(mesh, face->getVertex(0), v1, v2, face->getVertex(1), face->getVertex(2))) {
		BOP_triangulateC_split(mesh, faces, face, face->getVertex(0), face->getVertex(1), 
							   face->getVertex(2), v1, v2);
	}
	else if (!BOP_isInsideCircle(mesh, face->getVertex(1), v1, v2, face->getVertex(0), face->getVertex(2))) {
		BOP_triangulateC_split(mesh, faces, face, face->getVertex(1), face->getVertex(2), 
							   face->getVertex(0), v1, v2);
	}
	else if (!BOP_isInsideCircle(mesh, face->getVertex(2), v1, v2, face->getVertex(0), face->getVertex(1))) {
		BOP_triangulateC_split(mesh, faces, face, face->getVertex(2), face->getVertex(0), 
							   face->getVertex(1), v1, v2);
	}
	else {
		BOP_triangulateC_split(mesh, faces, face, face->getVertex(2), face->getVertex(0),
							   face->getVertex(1), v1, v2);
	}  
}

/**
 * Triangulates the face (v1,v2,v3) in five new faces by two inner points (v4,v5), where
 * v1 v4 v5 defines the nice triangle and v4 v5 v2 v3 defines the quad to be tessellated.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faces set of faces that contains face and will contains new faces
 * @param face input face to be triangulate
 * @param v1 first vertex index that defines the original triangle
 * @param v2 second vertex index that defines the original triangle
 * @param v3 third vertex index that defines the original triangle
 * @param v4 first vertex index that lays inside face
 * @param v5 second vertex index that lays inside face
 */
void BOP_triangulateC_split(BOP_Mesh* mesh, BOP_Faces* faces, BOP_Face* face, 
							BOP_Index v1, BOP_Index v2, BOP_Index v3, BOP_Index v4, BOP_Index v5)
{
	BOP_Index v = BOP_getTriangleVertex(mesh, v1, v2, v4, v5);
	BOP_Index w = (v == v4 ? v5 : v4);
	BOP_Face *face1 = new BOP_Face3(v1, v, w, face->getPlane(), face->getOriginalFace());
	BOP_Face *face2 = new BOP_Face3(v1, v2, v, face->getPlane(), face->getOriginalFace());
	BOP_Face *face3 = new BOP_Face3(v1, w, v3, face->getPlane(), face->getOriginalFace());
	
	// v1 v w defines the nice triangle in the correct order
	// v1 v2 v defines one lateral triangle in the correct order
	// v1 w v3 defines the other lateral triangle in the correct order
	// w v v2 v3 defines the quad in the correct order
	
	BOP_addFace(mesh, faces, face1, face->getTAG());
	BOP_addFace(mesh, faces, face2, face->getTAG());
	BOP_addFace(mesh, faces, face3, face->getTAG());

	face1->setSplit(face->getSplit());
	face2->setSplit(face->getSplit());
	face3->setSplit(face->getSplit());
	
	BOP_Face *faces45[2];
	
	BOP_splitQuad(mesh, face->getPlane(), v2, v3, w, v, faces45, face->getOriginalFace());
	BOP_addFace(mesh, faces, faces45[0], face->getTAG());
	BOP_addFace(mesh, faces, faces45[1], face->getTAG());
	faces45[0]->setSplit(face->getSplit());
	faces45[1]->setSplit(face->getSplit());
	
	face->setTAG(BROKEN); 
	face->freeBBox();
}


/**
 * Triangulates the face in three new faces by splitting twice an edge.
 *
 *         *
 *        / \
 *       /   \
 *      /     \
 *     /       \
 *    /         \
 *   *---x---x---*
 *
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faces set of faces that contains face and will contains new faces
 * @param face input face to be triangulate
 * @param v1 first vertex index that intersects the edge
 * @param v2 second vertex index that intersects the edge
 * @param e relative edge index used to triangulate the face
 */
void BOP_triangulateD(BOP_Mesh* mesh, BOP_Faces* faces, BOP_Face* face, BOP_Index v1, 
					  BOP_Index v2, unsigned int e)
{
	if (e == 1) {
		BOP_triangulateD_split(mesh, faces, face, face->getVertex(0), face->getVertex(1),
							   face->getVertex(2), v1, v2);
	}
	else if (e == 2) {
		BOP_triangulateD_split(mesh, faces, face, face->getVertex(1), face->getVertex(2),
							   face->getVertex(0), v1, v2);
	}
	else if (e == 3) {
		BOP_triangulateD_split(mesh, faces, face, face->getVertex(2), face->getVertex(0),
							   face->getVertex(1), v1, v2);
	}
}

/**
 * Triangulates the face (v1,v2,v3) in three new faces by splitting twice an edge.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faces set of faces that contains face and will contains new faces
 * @param face input face to be triangulate
 * @param v1 first vertex index that defines the original triangle
 * @param v2 second vertex index that defines the original triangle
 * @param v3 third vertex index that defines the original triangle
 * @param v4 first vertex index that lays on the edge
 * @param v5 second vertex index that lays on the edge
 */
void BOP_triangulateD_split(BOP_Mesh* mesh, BOP_Faces* faces, BOP_Face* face, 
							BOP_Index v1, BOP_Index v2, BOP_Index v3, BOP_Index v4, BOP_Index v5)
{
	BOP_Index v = BOP_getNearestVertex(mesh, v1, v4, v5);
	BOP_Index w = (v == v4 ? v5 : v4);
	BOP_Face *face1 = new BOP_Face3(v1, v, v3, face->getPlane(), face->getOriginalFace());
	BOP_Face *face2 = new BOP_Face3(v, w, v3, face->getPlane(), face->getOriginalFace());
	BOP_Face *face3 = new BOP_Face3(w, v2, v3, face->getPlane(), face->getOriginalFace());
	
	BOP_addFace(mesh, faces, face1, face->getTAG());
	BOP_addFace(mesh, faces, face2, face->getTAG());
	BOP_addFace(mesh, faces, face3, face->getTAG());
	face1->setSplit(face->getSplit());
	face2->setSplit(face->getSplit());
	face3->setSplit(face->getSplit());

	face->setTAG(BROKEN);
	face->freeBBox();
}


/**
 * Triangulates the face in three new faces by splitting two edges.
 *
 *         *
 *        / \
 *       /   \
 *      x     x
 *     /       \
 *    /         \
 *   *-----------*
 *
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faces set of faces that contains face and will contains new faces
 * @param face input face to be triangulate
 * @param v1 vertex index that intersects the first edge
 * @param v1 vertex index that intersects the second edge
 * @param e1 first relative edge index used to triangulate the face
 * @param e2 second relative edge index used to triangulate the face
 */
void BOP_triangulateE(BOP_Mesh* mesh, BOP_Faces* faces, BOP_Face* face, 
					  BOP_Index v1, BOP_Index v2, unsigned int e1, unsigned int e2)
{
	// Sort the edges to reduce the cases
	if (e1 > e2) {
		unsigned int aux = e1;
		e1 = e2;
		e2 = aux;
		aux = v1;
		v1 = v2;
		v2 = aux;
	}
	// e1 < e2!
	BOP_Face *face1;
	BOP_Face *faces23[2];
	if (e1 == 1 && e2 == 2) {
		// the vertex is 2
		face1 = new BOP_Face3(face->getVertex(1), v2, v1, face->getPlane(), 
							  face->getOriginalFace());
		BOP_splitQuad(mesh, face->getPlane(), face->getVertex(2), face->getVertex(0), v1, v2,
					  faces23, face->getOriginalFace());
	}
	else if (e1 == 1 && e2 == 3) {
		// the vertex is 1
		face1 = new BOP_Face3(face->getVertex(0), v1, v2, face->getPlane(), 
							  face->getOriginalFace());
		BOP_splitQuad(mesh, face->getPlane(), face->getVertex(1), face->getVertex(2), v2, v1, 
					  faces23, face->getOriginalFace());
	}
	else if (e1 == 2 && e2 == 3) {
		// the vertex is 3
		face1 = new BOP_Face3(face->getVertex(2), v2, v1, face->getPlane(), 
							  face->getOriginalFace());
		BOP_splitQuad(mesh, face->getPlane(), face->getVertex(0), face->getVertex(1), v1, v2, 
					  faces23, face->getOriginalFace());
	}
	else {
		return;
	}
	
	BOP_addFace(mesh, faces, face1, face->getTAG());
	BOP_addFace(mesh, faces, faces23[0], face->getTAG());
	BOP_addFace(mesh, faces, faces23[1], face->getTAG());
	face1->setSplit(face->getSplit());
	faces23[0]->setSplit(face->getSplit());
	faces23[1]->setSplit(face->getSplit());
	face->setTAG(BROKEN);
	face->freeBBox();
}

/**
 * Triangulates the face in four new faces by one edge and one inner point.
 *
 *         *
 *        / \
 *       /   \
 *      x  x  \
 *     /       \
 *    /         \
 *   *-----------*
 *
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faces set of faces that contains face and will contains new faces
 * @param face input face to be triangulate
 * @param v1 vertex index that lays inside face
 * @param v2 vertex index that intersects the edge
 * @param e relative edge index used to triangulate the face
 */
void BOP_triangulateF(BOP_Mesh* mesh, BOP_Faces* faces, BOP_Face* face, 
					  BOP_Index v1, BOP_Index v2, unsigned int e)
{
	BOP_Face *faces12[2];
	BOP_Face *faces34[2];
	if (e == 1) {      
		BOP_splitQuad(mesh, face->getPlane(), face->getVertex(2), face->getVertex(0), v2, v1,
					  faces12, face->getOriginalFace());
		BOP_splitQuad(mesh, face->getPlane(), face->getVertex(1), face->getVertex(2), v1, v2,
					  faces34, face->getOriginalFace());
	}
	else if (e == 2) {
		BOP_splitQuad(mesh, face->getPlane(), face->getVertex(0), face->getVertex(1), v2, v1,
					  faces12, face->getOriginalFace());
		BOP_splitQuad(mesh, face->getPlane(), face->getVertex(2), face->getVertex(0), v1, v2,
					  faces34, face->getOriginalFace());
	}
	else if (e==3) {
		BOP_splitQuad(mesh, face->getPlane(), face->getVertex(1), face->getVertex(2), v2, v1,
					  faces12, face->getOriginalFace());
		BOP_splitQuad(mesh, face->getPlane(), face->getVertex(0), face->getVertex(1), v1, v2,
					  faces34, face->getOriginalFace());
	}
	else {
		return;
	}
	
	BOP_addFace(mesh, faces, faces12[0], face->getTAG());
	BOP_addFace(mesh, faces, faces12[1], face->getTAG());
	BOP_addFace(mesh, faces, faces34[0], face->getTAG());
	BOP_addFace(mesh, faces, faces34[1], face->getTAG());
	faces12[0]->setSplit(face->getSplit());
	faces12[1]->setSplit(face->getSplit());
	faces34[0]->setSplit(face->getSplit());
	faces34[1]->setSplit(face->getSplit());
	
	face->setTAG(BROKEN);
	face->freeBBox();
}

/**
 * Adds the new face into the faces set and the mesh and sets it a new tag.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param faces set of faces that contains oldFace
 * @param face input face to be added
 * @param tag tag of the new face
 */
void BOP_addFace(BOP_Mesh* mesh, BOP_Faces* faces, BOP_Face* face, BOP_TAG tag)
{
	BOP_Index av1 = face->getVertex(0);
	BOP_Index av2 = face->getVertex(1);
	BOP_Index av3 = face->getVertex(2);

	/*
	 * Before adding a new face to the face list, be sure it's not
	 * already there.  Duplicate faces have been found to cause at
	 * least two instances of infinite loops.  Also, some faces are
	 * created which have the same vertex twice.  Don't add these either.
	 *
	 * When someone has more time to look into this issue, it's possible
	 * this code may be removed again.
	 */
	if( av1==av2 || av2==av3  || av3==av1 ) return;

	for(unsigned int idxFace=0;idxFace<faces->size();idxFace++) {
		BOP_Face *faceA = (*faces)[idxFace];
		BOP_Index bv1 = faceA->getVertex(0);
		BOP_Index bv2 = faceA->getVertex(1);
		BOP_Index bv3 = faceA->getVertex(2);

		if( ( av1==bv1 && av2==bv2 && av3==bv3 ) ||
	        ( av1==bv1 && av2==bv3 && av3==bv2 ) ||
	        ( av1==bv2 && av2==bv1 && av3==bv3 ) ||
	        ( av1==bv2 && av2==bv3 && av3==bv1 ) ||
	        ( av1==bv3 && av2==bv2 && av3==bv1 ) ||
	        ( av1==bv3 && av2==bv1 && av3==bv3 ) )
			return;
	}

	face->setTAG(tag);    
	faces->push_back(face);
	mesh->addFace(face);
}

/**
 * Computes the best quad triangulation.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param plane plane used to create the news faces
 * @param v1 first vertex index
 * @param v2 second vertex index
 * @param v3 third vertex index
 * @param v4 fourth vertex index
 * @param triangles array of faces where the new two faces will be saved
 * @param original face index to the new faces
 */
void BOP_splitQuad(BOP_Mesh* mesh, MT_Plane3 plane, BOP_Index v1, BOP_Index v2, 
				   BOP_Index v3, BOP_Index v4, BOP_Face* triangles[], BOP_Index original)
{
	MT_Point3 p1 = mesh->getVertex(v1)->getPoint();
	MT_Point3 p2 = mesh->getVertex(v2)->getPoint();
	MT_Point3 p3 = mesh->getVertex(v3)->getPoint();
	MT_Point3 p4 = mesh->getVertex(v4)->getPoint();
	
	int res = BOP_concave(p1,p2,p3,p4);
	
	if (res==0) {
		MT_Plane3 plane1(p1, p2, p3);
		MT_Plane3 plane2(p1, p3, p4);
		
		if (BOP_isInsideCircle(mesh, v1, v2, v4, v3) && 
			BOP_orientation(plane1, plane) &&
			BOP_orientation(plane2, plane)) {
			triangles[0] = new BOP_Face3(v1, v2, v3, plane, original);
			triangles[1] = new BOP_Face3(v1, v3, v4, plane, original);
		}
		else {
			triangles[0] = new BOP_Face3(v1, v2, v4, plane, original);
			triangles[1] = new BOP_Face3(v2, v3, v4, plane, original);
		}
	}
	else if (res==-1) {
		triangles[0] = new BOP_Face3(v1, v2, v4, plane, original);
		triangles[1] = new BOP_Face3(v2, v3, v4, plane, original);
	}
	else {
		triangles[0] = new BOP_Face3(v1, v2, v3, plane, original);
		triangles[1] = new BOP_Face3(v1, v3, v4, plane, original);
	}
}

/**
 * Returns the vertex (v3 or v4) that splits the quad (v1,v2,v3,v4) in the best pair of triangles.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param v1 first vertex index
 * @param v2 second vertex index
 * @param v3 third vertex index
 * @param v4 fourth vertex index
 * @return v3 if the best split triangles are (v1,v2,v3) and (v1,v3,v4), v4 otherwise
 */
BOP_Index BOP_getTriangleVertex(BOP_Mesh* mesh, BOP_Index v1, BOP_Index v2, BOP_Index v3, BOP_Index v4)
{
	if (BOP_isInsideCircle(mesh, v1, v2, v4, v3)) {
		return v3;
	}
	return v4; 
}

/**
 * Returns which of vertex v1 or v2 is nearest to u.
 * @param mesh mesh that contains the faces, edges and vertices
 * @param u reference vertex index
 * @param v1 first vertex index
 * @param v2 second vertex index
 * @return the nearest vertex index
 */
BOP_Index BOP_getNearestVertex(BOP_Mesh* mesh, BOP_Index u, BOP_Index v1, BOP_Index v2)
{
	MT_Point3 q = mesh->getVertex(u)->getPoint();
	MT_Point3 p1 = mesh->getVertex(v1)->getPoint();
	MT_Point3 p2 = mesh->getVertex(v2)->getPoint();
	if (BOP_comp(q.distance(p1), q.distance(p2)) > 0) return v2;
	else return v1;
}

/**
 * Computes if vertexs v4 and v5 are not inside the circle defined by v1,v2,v3 (seems to be a nice triangle)
 * @param mesh mesh that contains the faces, edges and vertices
 * @param v1 first vertex index
 * @param v2 second vertex index
 * @param v3 third vertex index
 * @param v4 fourth vertex index
 * @param v5 five vertex index
 * @return if v1,v2,v3 defines a nice triangle against v4,v5
 */
bool BOP_isInsideCircle(BOP_Mesh* mesh, BOP_Index v1, BOP_Index v2, BOP_Index v3, BOP_Index v4, BOP_Index v5)
{
	return BOP_isInsideCircle(mesh->getVertex(v1)->getPoint(),
							  mesh->getVertex(v2)->getPoint(),
							  mesh->getVertex(v3)->getPoint(),
							  mesh->getVertex(v4)->getPoint(),
							  mesh->getVertex(v5)->getPoint());
}

/**
 * Computes if vertex w is not inside the circle defined by v1,v2,v3 (seems to be a nice triangle)
 * @param mesh mesh that contains the faces, edges and vertices
 * @param v1 first vertex index
 * @param v2 second vertex index
 * @param v3 third vertex index
 * @param w fourth vertex index
 * @return if v1,v2,v3 defines a nice triangle against w
 */
bool BOP_isInsideCircle(BOP_Mesh* mesh, BOP_Index v1, BOP_Index v2, BOP_Index v3, BOP_Index w) 
{
	return BOP_isInsideCircle(mesh->getVertex(v1)->getPoint(),
							  mesh->getVertex(v2)->getPoint(),
							  mesh->getVertex(v3)->getPoint(),
							  mesh->getVertex(w)->getPoint());
}
