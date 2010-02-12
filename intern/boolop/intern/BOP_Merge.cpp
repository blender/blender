/**
 *
 * $Id$
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
 
#include "BOP_Merge.h"

#ifdef BOP_ORIG_MERGE

#ifdef _MSC_VER
#if _MSC_VER < 1300
#include <list>
#endif
#endif

/**
 * SINGLETON (use method BOP_Merge.getInstance).
 */
BOP_Merge BOP_Merge::SINGLETON;

/**
 * Simplifies a mesh, merging its faces.
 * @param m mesh
 * @param v index of the first mergeable vertex (can be removed by merge) 
 */
void BOP_Merge::mergeFaces(BOP_Mesh *m, BOP_Index v)
{
	m_mesh = m;
	m_firstVertex = v;

	bool cont = false;

	// Merge faces
	mergeFaces();

	do {
		// Add quads ...
		cont = createQuads();
		if (cont) {
			// ... and merge new faces
			cont = mergeFaces();
		}
		// ... until the merge is not succesful
	} while(cont);
}

/**
 * Simplifies a mesh, merging its faces.
 */
bool BOP_Merge::mergeFaces()
{
	BOP_Indexs mergeVertices;
	BOP_Vertexs vertices = m_mesh->getVertexs();
	BOP_IT_Vertexs v = vertices.begin();
	const BOP_IT_Vertexs verticesEnd = vertices.end();

	// Advance to first mergeable vertex
	advance(v,m_firstVertex);
	BOP_Index pos = m_firstVertex;

	// Add unbroken vertices to the list
	while(v!=verticesEnd) {
		if ((*v)->getTAG() != BROKEN) mergeVertices.push_back(pos);
		v++;pos++;
	}

	// Merge faces with that vertices
	return mergeFaces(mergeVertices);
}


/**
 * Simplifies a mesh, merging the faces with the specified vertices.
 * @param mergeVertices vertices to test
 * @return true if a face merge was performed
 */
bool BOP_Merge::mergeFaces(BOP_Indexs &mergeVertices)
{
	// Check size > 0!
	if (mergeVertices.size() == 0) return false;

	// New faces added by merge
	BOP_Faces newFaces;

	// Old faces removed by merge
	BOP_Faces oldFaces;

	// Get the first vertex index and add it to 
	// the current pending vertices to merge
	BOP_Index v = mergeVertices[0];
	BOP_Indexs pendingVertices;
	pendingVertices.push_back(v);

	// Get faces with index v that come from the same original face
	BOP_LFaces facesByOriginalFace;
	getFaces(facesByOriginalFace,v);

	bool merged = true;

	// Check it has any unbroken face
	if (facesByOriginalFace.size()==0) {
		// v has not any unbroken face (so it's a new BROKEN vertex)
		(m_mesh->getVertex(v))->setTAG(BROKEN);
		merged = false;
	}

	// Merge vertex faces	
	const BOP_IT_LFaces facesEnd = facesByOriginalFace.end();
	
	for(BOP_IT_LFaces facesByOriginalFaceX = facesByOriginalFace.begin();
		(facesByOriginalFaceX != facesEnd)&&merged;
		facesByOriginalFaceX++) {		
			merged = mergeFaces((*facesByOriginalFaceX),oldFaces,newFaces,pendingVertices,v);
	}

	// Check if the are some pendingVertices to merge
	if (pendingVertices.size() > 1 && merged) {
		// There are pending vertices that we need to merge in order to merge v ...
		for(unsigned int i=1;i<pendingVertices.size() && merged;i++) 
			merged = mergeFaces(oldFaces,newFaces,pendingVertices,pendingVertices[i]);
	}

	// If merge was succesful ...
	if (merged) {
		// Set old faces to BROKEN...
	  const BOP_IT_Faces oldFacesEnd = oldFaces.end();
		for(BOP_IT_Faces face=oldFaces.begin();face!=oldFacesEnd;face++) 
			(*face)->setTAG(BROKEN);

		// ... and add merged faces (that are the new merged faces without pending vertices)
		const BOP_IT_Faces newFacesEnd = newFaces.end();
		for(BOP_IT_Faces newFace=newFaces.begin();newFace!=newFacesEnd;newFace++) {
			m_mesh->addFace(*newFace);
			// Also, add new face vertices to the queue of vertices to merge if they weren't
			for(BOP_Index i = 0;i<(*newFace)->size();i++) {
				BOP_Index vertexIndex = (*newFace)->getVertex(i);
				if (vertexIndex >= m_firstVertex && !containsIndex(mergeVertices,vertexIndex))
					mergeVertices.push_back(vertexIndex);
			}
		}
		// Set the merged vertices to BROKEN ...
		const BOP_IT_Indexs pendingEnd = pendingVertices.end();
		for(BOP_IT_Indexs pendingVertex = pendingVertices.begin(); pendingVertex != pendingEnd;pendingVertex++) {
			BOP_Index pV = *pendingVertex;
			m_mesh->getVertex(pV)->setTAG(BROKEN);
			// ... and remove them from mergeVertices queue
			const BOP_IT_Indexs mergeEnd = mergeVertices.end();
			for(BOP_IT_Indexs mergeVertex = mergeVertices.begin(); mergeVertex != mergeEnd;mergeVertex++) {
				BOP_Index mV = *mergeVertex;
				if (mV == pV) {
					mergeVertices.erase(mergeVertex);
					break;
				}
			}
		}
	}
	else {
		// The merge  was not succesful, remove the vertex frome merge vertices queue
		mergeVertices.erase(mergeVertices.begin());
		
		// free the not used newfaces
		const BOP_IT_Faces newFacesEnd = newFaces.end();
		for(BOP_IT_Faces newFace=newFaces.begin();newFace!=newFacesEnd;newFace++) {
			delete (*newFace);
		}
	}

	// Invoke mergeFaces and return the merge result
	return (mergeFaces(mergeVertices) || merged);
}


/**
 * Simplifies a mesh, merging the faces with vertex v that come from the same face.
 * @param oldFaces sequence of old mesh faces obtained from the merge
 * @param newFaces sequence of new mesh faces obtained from the merge
 * @param vertices sequence of indexs (v1 ... vi = v ... vn) where :
 *   v is the current vertex to test,
 *   vj (j < i) are tested vertices, 
 *   vk (k >= i) are vertices required to test to merge vj
 * (so if a vertex vk can't be merged, the merge is not possible).
 * @return true if the vertex v was 'merged' (obviously it could require to test
 * some new vertices that will be added to the vertices list)
 */
bool BOP_Merge::mergeFaces(BOP_Faces &oldFaces, BOP_Faces &newFaces, BOP_Indexs &vertices, BOP_Index v) {

	bool merged = true;

	// Get faces with v that come from the same original face, (without the already 'merged' from vertices)
	BOP_LFaces facesByOriginalFace;
	getFaces(facesByOriginalFace,vertices,v);
  
	if (facesByOriginalFace.size()==0) {
		// All the faces with this vertex were already merged!!!
		return true;
	}
	else {
		// Merge faces
	  const BOP_IT_LFaces facesEnd = facesByOriginalFace.end();
		for(BOP_IT_LFaces facesByOriginalFaceX = facesByOriginalFace.begin();
			(facesByOriginalFaceX != facesEnd)&&merged;
			facesByOriginalFaceX++) {
				merged = mergeFaces((*facesByOriginalFaceX),oldFaces,newFaces,vertices,v);
		}
	}
	return merged;
}


/**
 * Merge a set of faces removing the vertex index v.
 * @param faces set of faces
 * @param oldFaces set of old faces obtained from the merge
 * @param newFaces set of new faces obtained from the merge
 * @param vertices sequence of indexs (v1 ... vi = v ... vn) where :
 *   v is the current vertex to test,
 *   vj (j < i) are tested vertices, 
 *   vk (k >= i) are vertices required to test to merge vj
 * (so if a vertex vk can't be merged, the merge is not possible).
 * @param v vertex index
 * @return true if the merge is succesful, false otherwise
 */
bool BOP_Merge::mergeFaces(BOP_Faces &faces, BOP_Faces &oldFaces, BOP_Faces &newFaces, BOP_Indexs &vertices, BOP_Index v)
{

	bool merged = false;

	if (faces.size() == 2) {
		// Merge a pair of faces into a new face without v
		BOP_Face *faceI = faces[0];
		BOP_Face *faceJ = faces[1];
		BOP_Face *faceK = mergeFaces(faceI,faceJ,vertices,v);
		if (faceK != NULL) {
			newFaces.push_back(faceK);
			oldFaces.push_back(faceI);
			oldFaces.push_back(faceJ);
			merged = true;
		} 
		else merged = false;
	}
	else if (faces.size() == 4) {
		// Merge two pair of faces into a new pair without v
		// First we try to perform a simplify merge to avoid more pending vertices
		// (for example, if we have two triangles and two quads it will be better
		// to do 3+4 and 3+4 than 3+3 and 4+4)
		BOP_Face *oldFace1 = faces[0];
		BOP_Face *oldFace2, *newFace1;
		unsigned int indexJ = 1;
		while (indexJ < faces.size() && !merged) {
			oldFace2 = faces[indexJ];
			newFace1 = mergeFaces(oldFace1,oldFace2,v);
			if (newFace1 != NULL) merged = true;
			else indexJ++;
		}
		if (merged) {
			// Merge the other pair of faces 
			unsigned int indexK, indexL;
			if (indexJ == 1) {indexK = 2;indexL = 3;}
			else if (indexJ == 2) {indexK = 1;indexL = 3;}
			else {indexK = 1;indexL = 2;}
			BOP_Face *oldFace3 = faces[indexK];
			BOP_Face *oldFace4 = faces[indexL];
			unsigned int oldSize = vertices.size();
			BOP_Face *newFace2 = mergeFaces(oldFace3,oldFace4,vertices,v);
			if (newFace2 != NULL) {
				newFaces.push_back(newFace1);
				newFaces.push_back(newFace2);
				oldFaces.push_back(oldFace1);
				oldFaces.push_back(oldFace2);
				oldFaces.push_back(oldFace3);
				oldFaces.push_back(oldFace4);
				merged = true;
			}
			else {
				// Undo all changes
				delete newFace1;
				merged = false;
				unsigned int count = vertices.size() - oldSize;
				if (count != 0)
					vertices.erase(vertices.end() - count, vertices.end());
			}
		} 		
		if (!merged) {
			// Try a complete merge
			merged = true;
			while (faces.size()>0 && merged) {
				indexJ = 1;
				BOP_Face *faceI = faces[0];
				merged = false;
				while (indexJ < faces.size()) {
					BOP_Face *faceJ = faces[indexJ];
					BOP_Face *faceK = mergeFaces(faceI,faceJ,vertices,v);
					if (faceK != NULL) {
						// faceK = faceI + faceJ and it does not include v!
						faces.erase(faces.begin()+indexJ,faces.begin()+(indexJ+1));
						faces.erase(faces.begin(),faces.begin()+1);
						newFaces.push_back(faceK);
						oldFaces.push_back(faceI);
						oldFaces.push_back(faceJ);
						merged = true;
						break;
					}
					else indexJ++;
				}
			}
		}
	}
	else merged = false; // there are N=1 or N=3 or N>4 faces!

	// Return merge result
	return merged;
}

/**
 * Returns a new quad from the merge of two faces (one quad and one triangle) 
 * that share the vertex v and come from the same original face.
 * @param faceI mesh face (quad or triangle) with index v
 * @param faceJ mesh face (quad or triangle) with index v
 * @param v vertex index shared by both faces
 * @return if the merge is possible, a new quad without v
 */
BOP_Face* BOP_Merge::mergeFaces(BOP_Face *faceI, BOP_Face *faceJ, BOP_Index v)
{
	if (faceI->size() == 3) {
		if (faceJ->size() == 4)
			return mergeFaces((BOP_Face4*)faceJ,(BOP_Face3*)faceI,v);
	}
	else if (faceI->size() == 4) {
		if (faceJ->size() == 3)
			return mergeFaces((BOP_Face4*)faceI,(BOP_Face3*)faceJ,v);
	}
	return NULL;
}

/**
 * Returns a new face from the merge of two faces (quads or triangles) that
 * share te vertex v and come from the same original face.
 * @param faceI mesh face (quad or triangle) with index v
 * @param faceJ mesh face (quad or triangle) with index v
 * @param pending vector with pending vertices (required to merge two quads into 
 * a new quad or one quad and one triangle into a new triangle; these merges 
 * suppose to remove two vertexs, v and its neighbour, that will be a pending 
 * vertex to merge if it wasn't)
 * @param v vertex index shared by both faces
 * @return if the merge is possible, a new face without v
 */
BOP_Face* BOP_Merge::mergeFaces(BOP_Face *faceI, BOP_Face *faceJ, BOP_Indexs &pending, BOP_Index v)
{
	if (faceI->size() == 3) {
		if (faceJ->size() == 3)
			return mergeFaces((BOP_Face3*)faceI,(BOP_Face3*)faceJ,v);
		else if (faceJ->size() == 4)
			return mergeFaces((BOP_Face4*)faceJ,(BOP_Face3*)faceI,pending,v);
	}
	else if (faceI->size() == 4) {
		if (faceJ->size() == 3)
			return mergeFaces((BOP_Face4*)faceI,(BOP_Face3*)faceJ,pending,v);
		else if (faceJ->size() == 4)
			return mergeFaces((BOP_Face4*)faceI,(BOP_Face4*)faceJ,pending,v);
	}
	return NULL;
}

/** 
 * Returns a new triangle from the merge of two triangles that share the vertex
 * v and come from the same original face.
 * @param faceI mesh triangle
 * @param faceJ mesh triangle
 * @param v vertex index shared by both triangles
 * @return If the merge is possible, a new triangle without v
 */
BOP_Face* BOP_Merge::mergeFaces(BOP_Face3 *faceI, BOP_Face3 *faceJ, BOP_Index v)
{
	BOP_Face *faceK = NULL;

	// Get faces data
	BOP_Index prevI, nextI, prevJ, nextJ;	
	faceI->getNeighbours(v,prevI,nextI);
	faceJ->getNeighbours(v,prevJ,nextJ);
	MT_Point3 vertex = m_mesh->getVertex(v)->getPoint();
	MT_Point3 vPrevI = m_mesh->getVertex(prevI)->getPoint();
	MT_Point3 vNextI = m_mesh->getVertex(nextI)->getPoint();
	MT_Point3 vPrevJ = m_mesh->getVertex(prevJ)->getPoint();
	MT_Point3 vNextJ = m_mesh->getVertex(nextJ)->getPoint();

	// Merge test
	if (prevI == nextJ) {
		// Both faces share the edge (prevI,v) == (v,nextJ)
		if (BOP_between(vertex,vNextI,vPrevJ)) {
			faceK = new BOP_Face3(prevI,prevJ,nextI,faceI->getPlane(),faceI->getOriginalFace());
			faceK->setTAG(faceI->getTAG());
		}
	}
	else if (nextI == prevJ) {
		// Both faces share the edge (v,nextI) == (prevJ,v)
		if (BOP_between(vertex,vPrevI,vNextJ)) {
			faceK = new BOP_Face3(prevI,nextJ,nextI,faceI->getPlane(),faceI->getOriginalFace());
			faceK->setTAG(faceI->getTAG());
		}
	}
	return faceK;
}

/** 
 * Returns a new quad from the merge of one quad and one triangle that share 
 * the vertex v and come from the same original face.
 * @param faceI mesh quad
 * @param faceJ mesh triangle
 * @param v vertex index shared by both faces
 * @return If the merge is possible, a new quad without v
 */
BOP_Face* BOP_Merge::mergeFaces(BOP_Face4 *faceI, BOP_Face3 *faceJ, BOP_Index v)
{
	BOP_Face *faceK = NULL;

	// Get faces data
	BOP_Index prevI, nextI, opp, prevJ, nextJ;
	faceI->getNeighbours(v,prevI,nextI,opp);
	faceJ->getNeighbours(v,prevJ,nextJ);
	MT_Point3 vertex = m_mesh->getVertex(v)->getPoint();
	MT_Point3 vOpp = m_mesh->getVertex(opp)->getPoint();
	MT_Point3 vPrevI = m_mesh->getVertex(prevI)->getPoint();
	MT_Point3 vNextI = m_mesh->getVertex(nextI)->getPoint();
	MT_Point3 vPrevJ = m_mesh->getVertex(prevJ)->getPoint();
	MT_Point3 vNextJ = m_mesh->getVertex(nextJ)->getPoint();

	// Merge test
	if (prevI == nextJ) {
		if (BOP_between(vertex,vNextI,vPrevJ) && !BOP_collinear(vPrevJ,vPrevI,vOpp)
			&& BOP_convex(vOpp,vPrevI,vPrevJ,vNextI)) {
			faceK = new BOP_Face4(opp,prevI,prevJ,nextI,faceI->getPlane(),faceI->getOriginalFace());
			faceK->setTAG(faceI->getTAG());
		}
	}
	else if (nextI == prevJ) {
		if (BOP_between(vertex,vPrevI,vNextJ) && !BOP_collinear(vNextJ,vNextI,vOpp)
			&& BOP_convex(vOpp,vPrevI,vNextJ,vNextI)) {
			faceK = new BOP_Face4(opp,prevI,nextJ,nextI,faceI->getPlane(),faceI->getOriginalFace());
			faceK->setTAG(faceI->getTAG());
		}
	}
	return faceK;
}

/** 
 * Returns a new face (quad or triangle) from the merge of one quad and one 
 * triangle that share the vertex v and come from the same original face.
 * @param faceI mesh quad
 * @param faceJ mesh triangle
 * @param pending vector with pending vertices (required to merge one quad 
 * and one triangle into a new triangle; it supposes to remove two vertexs,
 * v and its neighbour, that will be a new pending vertex if it wasn't)
 * @param v vertex index shared by both faces
 * @return If the merge is possible, a new face without v
 */
BOP_Face* BOP_Merge::mergeFaces(BOP_Face4 *faceI, BOP_Face3 *faceJ, BOP_Indexs &pending, BOP_Index v)
{
	BOP_Face *faceK = NULL;

	// Get faces data
	BOP_Index prevI, nextI, opp, prevJ, nextJ;
	faceI->getNeighbours(v,prevI,nextI,opp);
	faceJ->getNeighbours(v,prevJ,nextJ);
	MT_Point3 vertex = m_mesh->getVertex(v)->getPoint();
	MT_Point3 vOpp = m_mesh->getVertex(opp)->getPoint();
	MT_Point3 vPrevI = m_mesh->getVertex(prevI)->getPoint();
	MT_Point3 vNextI = m_mesh->getVertex(nextI)->getPoint();
	MT_Point3 vPrevJ = m_mesh->getVertex(prevJ)->getPoint();
	MT_Point3 vNextJ = m_mesh->getVertex(nextJ)->getPoint();

	// Merge test
	if (prevI == nextJ) {
		if (BOP_between(vertex,vNextI,vPrevJ)) {
			if (!BOP_collinear(vPrevJ,vPrevI,vOpp) && BOP_convex(vOpp,vPrevI,vPrevJ,vNextI)) {
				// The result is a new quad
				faceK = new BOP_Face4(opp,prevI,prevJ,nextI,faceI->getPlane(),faceI->getOriginalFace());
				faceK->setTAG(faceI->getTAG());
			}
			else if (BOP_between(vPrevI,vPrevJ,vOpp)) {
				// The result is a triangle (only if prevI can be merged)
				if (prevI < m_firstVertex) return NULL; // It can't be merged
				faceK = new BOP_Face3(nextI,opp,prevJ,faceI->getPlane(),faceI->getOriginalFace());
				faceK->setTAG(faceI->getTAG());
				if (!containsIndex(pending, prevI)) pending.push_back(prevI);
			}
		}
	}
	else if (nextI == prevJ) {
		if (BOP_between(vertex,vPrevI,vNextJ)) {
			if (!BOP_collinear(vNextJ,vNextI,vOpp) && BOP_convex(vOpp,vPrevI,vNextJ,vNextI)) {
				// The result is a new quad
				faceK = new BOP_Face4(opp,prevI,nextJ,nextI,faceI->getPlane(),faceI->getOriginalFace());
				faceK->setTAG(faceI->getTAG());
			}
			else if (BOP_between(vNextI,vOpp,vNextJ)) {
				// The result is a triangle (only if nextI can be merged)
				if (nextI < m_firstVertex) return NULL;
				faceK = new BOP_Face3(prevI,nextJ,opp,faceI->getPlane(),faceI->getOriginalFace());
				faceK->setTAG(faceI->getTAG());
				if (!containsIndex(pending, nextI)) pending.push_back(nextI);
			}
		}
	}
	return faceK;
}

/** 
 * Returns a new quad from the merge of two quads that share 
 * the vertex v and come from the same original face.
 * @param faceI mesh quad
 * @param faceJ mesh quad
 * @param pending vector with pending vertices (required to merge the two
 * quads supposes to remove two vertexs, v and its neighbour, 
 * that will be a new pending vertex if it wasn't)
 * @param v vertex index shared by both quads
 * @return If the merge is possible, a new quad without v
 */
BOP_Face* BOP_Merge::mergeFaces(BOP_Face4 *faceI, BOP_Face4 *faceJ, BOP_Indexs &pending, BOP_Index v)
{
	BOP_Face *faceK = NULL;

	// Get faces data
	BOP_Index prevI, nextI, oppI, prevJ, nextJ, oppJ;
	faceI->getNeighbours(v,prevI,nextI,oppI);
	faceJ->getNeighbours(v,prevJ,nextJ,oppJ);
	MT_Point3 vertex = m_mesh->getVertex(v)->getPoint();
	MT_Point3 vPrevI = m_mesh->getVertex(prevI)->getPoint();
	MT_Point3 vNextI = m_mesh->getVertex(nextI)->getPoint();
	MT_Point3 vOppI = m_mesh->getVertex(oppI)->getPoint();
	MT_Point3 vPrevJ = m_mesh->getVertex(prevJ)->getPoint();
	MT_Point3 vNextJ = m_mesh->getVertex(nextJ)->getPoint();
	MT_Point3 vOppJ = m_mesh->getVertex(oppJ)->getPoint();

	// Merge test
	if (prevI == nextJ) {
		// prevI/nextJ will be a new vertex required to merge
		if (prevI < m_firstVertex) return NULL; // It can't be merged
		if (BOP_between(vertex,vPrevJ,vNextI) && BOP_between(vNextJ,vOppJ,vOppI)) {
			faceK = new BOP_Face4(oppJ,prevJ,nextI,oppI,faceI->getPlane(),faceI->getOriginalFace());
			faceK->setTAG(faceI->getTAG());
			// We add prevI to the pending list if it wasn't yet
			if (!containsIndex(pending, prevI)) pending.push_back(prevI);
		}
	}
	else if (nextI == prevJ) {
		// nextI/prevJ will be a new vertex required to merge
		if (nextI < m_firstVertex) return NULL; // It can't be merged
		if (BOP_between(vertex,vPrevI,vNextJ) && BOP_between(vNextI,vOppI,vOppJ)) {
			faceK = new BOP_Face4(oppI,prevI,nextJ,oppJ,faceI->getPlane(),faceI->getOriginalFace());
			faceK->setTAG(faceI->getTAG());
			// Add nextI to the pending list if it wasn't yet
			if (!containsIndex(pending, nextI)) pending.push_back(nextI);
		}
	}
	return faceK;
}


/** 
 * Simplifies the mesh, merging the pairs of triangles that come frome the
 * same original face and define a quad.
 * @return true if a quad was added, false otherwise
 */
bool BOP_Merge::createQuads()
{
  
	BOP_Faces quads;
	
	// Get mesh faces
	BOP_Faces faces = m_mesh->getFaces();

	
    // Merge mesh triangles
	const BOP_IT_Faces facesIEnd = (faces.end()-1);
	const BOP_IT_Faces facesJEnd = faces.end();
	for(BOP_IT_Faces faceI=faces.begin();faceI!=facesIEnd;faceI++) {
		if ((*faceI)->getTAG() == BROKEN || (*faceI)->size() != 3) continue;
		for(BOP_IT_Faces faceJ=(faceI+1);faceJ!=facesJEnd;faceJ++) {
			if ((*faceJ)->getTAG() == BROKEN || (*faceJ)->size() != 3 ||
				(*faceJ)->getOriginalFace() != (*faceI)->getOriginalFace()) continue;

			// Test if both triangles share a vertex index
			BOP_Index v;
			bool found = false;
			for(unsigned int i=0;i<3 && !found;i++) {
				v = (*faceI)->getVertex(i);
				found = (*faceJ)->containsVertex(v);
				
			}
			if (!found) continue;

			BOP_Face *faceK = createQuad((BOP_Face3*)*faceI,(BOP_Face3*)*faceJ,v);
			if (faceK != NULL) {
				// Set triangles to BROKEN
				(*faceI)->setTAG(BROKEN);
				(*faceJ)->setTAG(BROKEN);
				quads.push_back(faceK);
				break;
			}
		}
	}

    // Add quads to mesh
	const BOP_IT_Faces quadsEnd = quads.end();
	for(BOP_IT_Faces quad=quads.begin();quad!=quadsEnd;quad++) m_mesh->addFace(*quad);
	return (quads.size() > 0);
}

/** 
 * Returns a new quad (convex) from the merge of two triangles that share the
 * vertex index v.
 * @param faceI mesh triangle
 * @param faceJ mesh triangle
 * @param v vertex index shared by both triangles
 * @return a new convex quad if the merge is possible
 */
BOP_Face* BOP_Merge::createQuad(BOP_Face3 *faceI, BOP_Face3 *faceJ, BOP_Index v)
{
	BOP_Face *faceK = NULL;

	// Get faces data
	BOP_Index prevI, nextI, prevJ, nextJ;
	faceI->getNeighbours(v,prevI,nextI);
	faceJ->getNeighbours(v,prevJ,nextJ);
	MT_Point3 vertex = m_mesh->getVertex(v)->getPoint();
	MT_Point3 vPrevI = m_mesh->getVertex(prevI)->getPoint();
	MT_Point3 vNextI = m_mesh->getVertex(nextI)->getPoint();
	MT_Point3 vPrevJ = m_mesh->getVertex(prevJ)->getPoint();
	MT_Point3 vNextJ = m_mesh->getVertex(nextJ)->getPoint();

	// Quad test
	if (prevI == nextJ) {
		if (!BOP_collinear(vNextI,vertex,vPrevJ) && !BOP_collinear(vNextI,vPrevI,vPrevJ) &&
			BOP_convex(vertex,vNextI,vPrevI,vPrevJ)) {
				faceK = new BOP_Face4(v,nextI,prevI,prevJ,faceI->getPlane(),faceI->getOriginalFace());
				faceK->setTAG(faceI->getTAG());
		}
	}
	else if (nextI == prevJ) {
		if (!BOP_collinear(vPrevI,vertex,vNextJ) && !BOP_collinear(vPrevI,vNextI,vNextJ) &&
			BOP_convex(vertex,vNextJ,vNextI,vPrevI)) {
				faceK = new BOP_Face4(v,nextJ,nextI,prevI,faceI->getPlane(),faceI->getOriginalFace());
				faceK->setTAG(faceI->getTAG());
			}
	}
	return faceK;
}

/**
 * Returns if a index is inside a set of indexs.
 * @param indexs set of indexs
 * @param i index
 * @return true if the index is inside the set, false otherwise
 */
bool BOP_Merge::containsIndex(BOP_Indexs indexs, BOP_Index i)
{
  const BOP_IT_Indexs indexsEnd = indexs.end();
	for(BOP_IT_Indexs it=indexs.begin();it!=indexsEnd;it++) {
		if (*it == i) return true;
	}
	return false;
}

/**
 * Creates a list of lists L1, L2, ... LN where
 *   LX = mesh faces with vertex v that come from the same original face
 * @param facesByOriginalFace list of faces lists
 * @param v vertex index
 */
void BOP_Merge::getFaces(BOP_LFaces &facesByOriginalFace, BOP_Index v)
{
	// Get edges with vertex v
	BOP_Indexs edgeIndexs = m_mesh->getVertex(v)->getEdges();
	const BOP_IT_Indexs edgeEnd = edgeIndexs.end();
	for(BOP_IT_Indexs edgeIndex = edgeIndexs.begin();edgeIndex != edgeEnd;edgeIndex++) {	
		// Foreach edge, add its no broken faces to the output list
		BOP_Edge* edge = m_mesh->getEdge(*edgeIndex);
		BOP_Indexs faceIndexs = edge->getFaces();
		const BOP_IT_Indexs faceEnd = faceIndexs.end();
		for(BOP_IT_Indexs faceIndex=faceIndexs.begin();faceIndex!=faceEnd;faceIndex++) {
			BOP_Face* face = m_mesh->getFace(*faceIndex);
			if (face->getTAG() != BROKEN) {
				bool found = false;
				// Search if we already have created a list for the 
				// faces that come from the same original face
				const BOP_IT_LFaces lfEnd = facesByOriginalFace.end();
				for(BOP_IT_LFaces facesByOriginalFaceX=facesByOriginalFace.begin();
				facesByOriginalFaceX!=lfEnd; facesByOriginalFaceX++) {
					if (((*facesByOriginalFaceX)[0])->getOriginalFace() == face->getOriginalFace()) {
						// Search that the face has not been added to the list before
						for(unsigned int i = 0;i<(*facesByOriginalFaceX).size();i++) {
							if ((*facesByOriginalFaceX)[i] == face) {
								found = true;
								break;
							}
						}
						if (!found) {
							// Add the face to the list
						  if (face->getTAG()==OVERLAPPED) facesByOriginalFaceX->insert(facesByOriginalFaceX->begin(),face);
						  else facesByOriginalFaceX->push_back(face);
						  found = true;
						}
						break;
					}
				}
				if (!found) {
					// Create a new list and add the current face
					BOP_Faces facesByOriginalFaceX;
					facesByOriginalFaceX.push_back(face);
					facesByOriginalFace.push_back(facesByOriginalFaceX);
				}
			}
		}
	}
}

/**
 * Creates a list of lists L1, L2, ... LN where
 *   LX = mesh faces with vertex v that come from the same original face
 *        and without any of the vertices that appear before v in vertices
 * @param facesByOriginalFace list of faces lists
 * @param vertices vector with vertices indexs that contains v
 * @param v vertex index
 */
void BOP_Merge::getFaces(BOP_LFaces &facesByOriginalFace, BOP_Indexs vertices, BOP_Index v)
{
	// Get edges with vertex v
	BOP_Indexs edgeIndexs = m_mesh->getVertex(v)->getEdges();
	const BOP_IT_Indexs edgeEnd = edgeIndexs.end();
	for(BOP_IT_Indexs edgeIndex = edgeIndexs.begin();edgeIndex != edgeEnd;edgeIndex++) {	
		// Foreach edge, add its no broken faces to the output list
		BOP_Edge* edge = m_mesh->getEdge(*edgeIndex);
		BOP_Indexs faceIndexs = edge->getFaces();
		const BOP_IT_Indexs faceEnd = faceIndexs.end();
		for(BOP_IT_Indexs faceIndex=faceIndexs.begin();faceIndex!=faceEnd;faceIndex++) {
			BOP_Face* face = m_mesh->getFace(*faceIndex);
			if (face->getTAG() != BROKEN) {
				// Search if the face contains any of the forbidden vertices
				bool found = false;
				for(BOP_IT_Indexs vertex = vertices.begin();*vertex!= v;vertex++) {
					if (face->containsVertex(*vertex)) {
						// face contains a forbidden vertex!
						found = true;
						break;
				}
			}
			if (!found) {
				// Search if we already have created a list with the 
				// faces that come from the same original face
			  const BOP_IT_LFaces lfEnd = facesByOriginalFace.end();
				for(BOP_IT_LFaces facesByOriginalFaceX=facesByOriginalFace.begin();
					facesByOriginalFaceX!=lfEnd; facesByOriginalFaceX++) {
					if (((*facesByOriginalFaceX)[0])->getOriginalFace() == face->getOriginalFace()) {
						// Search that the face has not been added to the list before
						for(unsigned int i = 0;i<(*facesByOriginalFaceX).size();i++) {
							if ((*facesByOriginalFaceX)[i] == face) {
								found = true;
								break;
							}
						}
						if (!found) {
						  // Add face to the list
						  if (face->getTAG()==OVERLAPPED) facesByOriginalFaceX->insert(facesByOriginalFaceX->begin(),face);
						  else facesByOriginalFaceX->push_back(face);
						  found = true;
						}
						break;
					}
				}
				if (!found) {
					// Create a new list and add the current face
					BOP_Faces facesByOriginalFaceX;
					facesByOriginalFaceX.push_back(face);
					facesByOriginalFace.push_back(facesByOriginalFaceX);
				}
			}
		}
	}
	}
}

#endif  /* BOP_ORIG_MERGE */
