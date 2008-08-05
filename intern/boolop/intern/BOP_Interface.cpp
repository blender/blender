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
 
#include <iostream>
#include <map>
#include "../extern/BOP_Interface.h"
#include "../../bsp/intern/BSP_CSGMesh_CFIterator.h"
#include "BOP_BSPTree.h"
#include "BOP_Mesh.h"
#include "BOP_Face2Face.h"
#include "BOP_Merge.h"
#include "BOP_Merge2.h"
#include "BOP_Chrono.h"

#if defined(BOP_ORIG_MERGE) && defined(BOP_NEW_MERGE) 
#include "../../../source/blender/blenkernel/BKE_global.h"
#endif

BoolOpState BOP_intersectionBoolOp(BOP_Mesh*  meshC,
								   BOP_Faces* facesA,
								   BOP_Faces* facesB,
								   bool       invertMeshA,
								   bool       invertMeshB);
BOP_Face3* BOP_createFace(BOP_Mesh* mesh, 
						  BOP_Index vertex1, 
						  BOP_Index vertex2, 
						  BOP_Index vertex3, 
						  BOP_Index origFace);
void BOP_addMesh(BOP_Mesh*                     mesh,
				 BOP_Faces*                    meshFacesId,
				 CSG_FaceIteratorDescriptor&   face_it,
				 CSG_VertexIteratorDescriptor& vertex_it,
				 bool                          invert);
BSP_CSGMesh* BOP_newEmptyMesh();
BSP_CSGMesh* BOP_exportMesh(BOP_Mesh*                  inputMesh, 
							bool                       invert);
void BOP_meshFilter(BOP_Mesh* meshC, BOP_Faces* faces, BOP_BSPTree* bsp);
void BOP_simplifiedMeshFilter(BOP_Mesh* meshC, BOP_Faces* faces, BOP_BSPTree* bsp, bool inverted);
void BOP_meshClassify(BOP_Mesh* meshC, BOP_Faces* faces, BOP_BSPTree* bsp);

/**
 * Performs a generic booleam operation, the entry point for external modules.
 * @param opType Boolean operation type BOP_INTERSECTION, BOP_UNION, BOP_DIFFERENCE
 * @param outputMesh Output mesh, the final result (the object C)
 * @param obAFaces Object A faces list
 * @param obAVertices Object A vertices list
 * @param obBFaces Object B faces list
 * @param obBVertices Object B vertices list
 * @param interpFunc Interpolating function
 * @return operation state: BOP_OK, BOP_NO_SOLID, BOP_ERROR
 */
BoolOpState BOP_performBooleanOperation(BoolOpType                    opType,
										BSP_CSGMesh**                 outputMesh,
										CSG_FaceIteratorDescriptor    obAFaces,
										CSG_VertexIteratorDescriptor  obAVertices,
										CSG_FaceIteratorDescriptor    obBFaces,
										CSG_VertexIteratorDescriptor  obBVertices)
{
	#ifdef DEBUG
	cout << "BEGIN BOP_performBooleanOperation" << endl;
	#endif

	// Set invert flags depending on boolean operation type:
	// INTERSECTION: A^B = and(A,B)
	// UNION:        A|B = not(and(not(A),not(B)))
	// DIFFERENCE:   A-B = and(A,not(B))
	bool invertMeshA = (opType == BOP_UNION);
	bool invertMeshB = (opType != BOP_INTERSECTION);
	bool invertMeshC = (opType == BOP_UNION);
	
	// Faces list for both objects, used by boolean op.
	BOP_Faces meshAFacesId;
	BOP_Faces meshBFacesId;
	
	// Build C-mesh, the output mesh
	BOP_Mesh meshC;

	// Add A-mesh into C-mesh
	BOP_addMesh(&meshC, &meshAFacesId, obAFaces, obAVertices, invertMeshA);

	// Add B-mesh into C-mesh
	BOP_addMesh(&meshC, &meshBFacesId, obBFaces, obBVertices, invertMeshB);

	// for now, allow operations on non-manifold (non-solid) meshes
#if 0
	if (!meshC.isClosedMesh())
		return BOP_NO_SOLID;
#endif

	// Perform the intersection boolean operation.
	BoolOpState result = BOP_intersectionBoolOp(&meshC, &meshAFacesId, &meshBFacesId, 
												invertMeshA, invertMeshB);

	// Invert the output mesh if is required
	*outputMesh = BOP_exportMesh(&meshC, invertMeshC);

	#ifdef DEBUG
	cout << "END BOP_performBooleanOperation" << endl;
	#endif
	
	return result;
}

/**
 * Computes the intersection boolean operation. Creates a new mesh resulting from 
 * an intersection of two meshes.
 * @param meshC Input & Output mesh
 * @param facesA Mesh A faces list
 * @param facesB Mesh B faces list
 * @param invertMeshA determines if object A is inverted
 * @param invertMeshB determines if object B is inverted
 * @return operation state: BOP_OK, BOP_NO_SOLID, BOP_ERROR
 */
BoolOpState BOP_intersectionBoolOp(BOP_Mesh*  meshC,
								   BOP_Faces* facesA,
								   BOP_Faces* facesB,
								   bool       invertMeshA,
								   bool       invertMeshB)
{
	#ifdef DEBUG
	BOP_Chrono chrono;
	float t = 0.0f;
	float c = 0.0f;
	chrono.start();  
	cout << "---" << endl;
	#endif

	// Create BSPs trees for mesh A & B
	BOP_BSPTree bspA;
	bspA.addMesh(meshC, *facesA);

	BOP_BSPTree bspB;
	bspB.addMesh(meshC, *facesB);

	#ifdef DEBUG
	c = chrono.stamp(); t += c;
	cout << "Create BSP     " << c << endl;	
	#endif

	unsigned int numVertices = meshC->getNumVertexs();
	
	// mesh pre-filter
	BOP_simplifiedMeshFilter(meshC, facesA, &bspB, invertMeshB);
	if ((0.25*facesA->size()) > bspB.getDeep())
	  BOP_meshFilter(meshC, facesA, &bspB);

	BOP_simplifiedMeshFilter(meshC, facesB, &bspA, invertMeshA);
	if ((0.25*facesB->size()) > bspA.getDeep())
	  BOP_meshFilter(meshC, facesB, &bspA);
	
	#ifdef DEBUG
	c = chrono.stamp(); t += c;
	cout << "mesh Filter    " << c << endl;	
	#endif

	// Face 2 Face
	BOP_Face2Face(meshC,facesA,facesB);

	#ifdef DEBUG
	c = chrono.stamp(); t += c;
	cout << "Face2Face      " << c << endl;
	#endif

	// BSP classification
	BOP_meshClassify(meshC,facesA,&bspB);
	BOP_meshClassify(meshC,facesB,&bspA);
	
	#ifdef DEBUG
	c = chrono.stamp(); t += c;
	cout << "Classification " << c << endl;
	#endif
	
	// Process overlapped faces
	BOP_removeOverlappedFaces(meshC,facesA,facesB);
	
	#ifdef DEBUG
	c = chrono.stamp(); t += c;
	cout << "Remove overlap " << c << endl;
	#endif

	// Sew two meshes
	BOP_sew(meshC,facesA,facesB);

	#ifdef DEBUG
	c = chrono.stamp(); t += c;
	cout << "Sew            " << c << endl;
	#endif

	// Merge faces
#ifdef BOP_ORIG_MERGE
#ifndef BOP_NEW_MERGE
	BOP_Merge::getInstance().mergeFaces(meshC,numVertices);
#endif
#endif

#ifdef BOP_NEW_MERGE
#ifndef BOP_ORIG_MERGE
	BOP_Merge2::getInstance().mergeFaces(meshC,numVertices);
#else
	static int state = -1;
	if (G.rt == 100) {
		if( state != 1 ) {
			cout << "Boolean code using old merge technique." << endl;
			state = 1;
		}
		BOP_Merge::getInstance().mergeFaces(meshC,numVertices);
	} else {
		if( state != 0 ) {
			cout << "Boolean code using new merge technique." << endl;
			state = 0;
		}
		BOP_Merge2::getInstance().mergeFaces(meshC,numVertices);
	}
#endif
#endif

	#ifdef DEBUG
	c = chrono.stamp(); t += c;
	cout << "Merge faces    " << c << endl;
	cout << "Total          " << t << endl;
	// Test integrity
	meshC->testMesh();
	#endif
	
	return BOP_OK;
}

/**
 * Preprocess to filter no collisioned faces.
 * @param meshC Input & Output mesh data
 * @param faces Faces list to test
 * @param bsp BSP tree used to filter
 */
void BOP_meshFilter(BOP_Mesh* meshC, BOP_Faces* faces, BOP_BSPTree* bsp)
{
	BOP_IT_Faces it;
	BOP_TAG tag;
	
	it = faces->begin();
	while (it!=faces->end()) {
		BOP_Face *face = *it;
		MT_Point3 p1 = meshC->getVertex(face->getVertex(0))->getPoint();
		MT_Point3 p2 = meshC->getVertex(face->getVertex(1))->getPoint();
		MT_Point3 p3 = meshC->getVertex(face->getVertex(2))->getPoint();
		if ((tag = bsp->classifyFace(p1,p2,p3,face->getPlane()))==OUT||tag==OUTON) {
			face->setTAG(BROKEN);
			it = faces->erase(it);
		}
		else if (tag == IN) {
			it = faces->erase(it);
		}else{
		  it++;
		}
	}
}

/**
 * Pre-process to filter no collisioned faces.
 * @param meshC Input & Output mesh data
 * @param faces Faces list to test
 * @param bsp BSP tree used to filter
 * @param inverted determines if the object is inverted
 */
void BOP_simplifiedMeshFilter(BOP_Mesh* meshC, BOP_Faces* faces, BOP_BSPTree* bsp, bool inverted)
{
	BOP_IT_Faces it;
	
	it = faces->begin();
	while (it!=faces->end()) {
		BOP_Face *face = *it;
		MT_Point3 p1 = meshC->getVertex(face->getVertex(0))->getPoint();
		MT_Point3 p2 = meshC->getVertex(face->getVertex(1))->getPoint();
		MT_Point3 p3 = meshC->getVertex(face->getVertex(2))->getPoint();
		if (bsp->filterFace(p1,p2,p3,face)==OUT) {
			if (!inverted) face->setTAG(BROKEN);
			it = faces->erase(it);
		}
		else {
			it++;
		}
	}
}

/**
 * Process to classify the mesh faces using a bsp tree.
 * @param meshC Input & Output mesh data
 * @param faces Faces list to classify
 * @param bsp BSP tree used to face classify
 */
void BOP_meshClassify(BOP_Mesh* meshC, BOP_Faces* faces, BOP_BSPTree* bsp)
{
	for(BOP_IT_Faces face=faces->begin();face!=faces->end();face++) {
		if ((*face)->getTAG()!=BROKEN) {
			MT_Point3 p1 = meshC->getVertex((*face)->getVertex(0))->getPoint();
			MT_Point3 p2 = meshC->getVertex((*face)->getVertex(1))->getPoint();
			MT_Point3 p3 = meshC->getVertex((*face)->getVertex(2))->getPoint();
			if (bsp->simplifiedClassifyFace(p1,p2,p3,(*face)->getPlane())!=IN) {
				(*face)->setTAG(BROKEN);
			}
		}
	}
}

/**
 * Returns a new mesh triangle.
 * @param meshC Input & Output mesh data
 * @param vertex1 first vertex of the new face
 * @param vertex2 second vertex of the new face
 * @param vertex3 third vertex of the new face
 * @param origFace identifier of the new face
 * @return new the new face
 */
BOP_Face3 *BOP_createFace3(BOP_Mesh* mesh, 
						   BOP_Index       vertex1, 
						   BOP_Index       vertex2, 
						   BOP_Index       vertex3, 
						   BOP_Index       origFace)
{
	MT_Point3 p1 = mesh->getVertex(vertex1)->getPoint();
	MT_Point3 p2 = mesh->getVertex(vertex2)->getPoint();
	MT_Point3 p3 = mesh->getVertex(vertex3)->getPoint();
	MT_Plane3 plane(p1,p2,p3);

	return new BOP_Face3(vertex1, vertex2, vertex3, plane, origFace);
}

/**
 * Adds mesh information into destination mesh.
 * @param mesh input/output mesh, destination for the new mesh data
 * @param meshFacesId output mesh faces, contains an added faces list
 * @param face_it faces iterator
 * @param vertex_it vertices iterator
 * @param inverted if TRUE adding inverted faces, non-inverted otherwise
 */
void BOP_addMesh(BOP_Mesh*                     mesh,
				 BOP_Faces*                    meshFacesId,
				 CSG_FaceIteratorDescriptor&   face_it,
				 CSG_VertexIteratorDescriptor& vertex_it,
				 bool                          invert)
{
	unsigned int vtxIndexOffset = mesh->getNumVertexs();

	// The size of the vertex data array will be at least the number of faces.
	CSG_IVertex vertex;
	while (!vertex_it.Done(vertex_it.it)) {
		vertex_it.Fill(vertex_it.it,&vertex);
		MT_Point3 pos(vertex.position);
		mesh->addVertex(pos);
		vertex_it.Step(vertex_it.it);
	}

	CSG_IFace face;
	
	// now for the polygons.
	// we may need to decalare some memory for user defined face properties.

	BOP_Face3 *newface;
	
	while (!face_it.Done(face_it.it)) {
		face_it.Fill(face_it.it,&face);

		// Let's not rely on quads being coplanar - especially if they 
		// are coming out of that soup of code from blender...
		if (face.vertex_number == 4){
			// QUAD
			if (invert) {
				newface = BOP_createFace3(mesh,
										  face.vertex_index[2] + vtxIndexOffset,
										  face.vertex_index[0] + vtxIndexOffset,
										  face.vertex_index[3] + vtxIndexOffset,
										  face.orig_face);
				meshFacesId->push_back(newface);
				mesh->addFace(newface);
				newface = BOP_createFace3(mesh,
										  face.vertex_index[2] + vtxIndexOffset,
										  face.vertex_index[1] + vtxIndexOffset,
										  face.vertex_index[0] + vtxIndexOffset,
										  face.orig_face);
				meshFacesId->push_back(newface);
				mesh->addFace(newface);
			}
			else {
				newface = BOP_createFace3(mesh,
										 face.vertex_index[0] + vtxIndexOffset,
										 face.vertex_index[2] + vtxIndexOffset,
										 face.vertex_index[3] + vtxIndexOffset,
										 face.orig_face);
				meshFacesId->push_back(newface);
				mesh->addFace(newface);
				newface = BOP_createFace3(mesh,
										 face.vertex_index[0] + vtxIndexOffset,
										 face.vertex_index[1] + vtxIndexOffset,
										 face.vertex_index[2] + vtxIndexOffset,
										 face.orig_face);
				meshFacesId->push_back(newface);
				mesh->addFace(newface);
			}
		}
		else {
			// TRIANGLES
			if (invert) {
				newface = BOP_createFace3(mesh,
										  face.vertex_index[2] + vtxIndexOffset,
										  face.vertex_index[1] + vtxIndexOffset,
										  face.vertex_index[0] + vtxIndexOffset,
										  face.orig_face);
				meshFacesId->push_back(newface);
				mesh->addFace(newface);
			}
			else {
				newface = BOP_createFace3(mesh,
										  face.vertex_index[0] + vtxIndexOffset,
										  face.vertex_index[1] + vtxIndexOffset,
										  face.vertex_index[2] + vtxIndexOffset,
										  face.orig_face);
				meshFacesId->push_back(newface);
				mesh->addFace(newface);
			}
		}
		
		face_it.Step(face_it.it);
	}
}

/**
 * Returns an empty mesh with the specified properties.
 * @return a new empty mesh
 */
BSP_CSGMesh* BOP_newEmptyMesh()
{
	BSP_CSGMesh* mesh = BSP_CSGMesh::New();
	if (mesh == NULL) return mesh;

	vector<BSP_MVertex>* vertices = new vector<BSP_MVertex>;
	
	mesh->SetVertices(vertices);

	return mesh;
}

/**
 * Exports a BOP_Mesh to a BSP_CSGMesh.
 * @param mesh Input mesh
 * @param invert if TRUE export with inverted faces, no inverted otherwise
 * @return the corresponding new BSP_CSGMesh
 */
BSP_CSGMesh* BOP_exportMesh(BOP_Mesh*                  mesh, 
							bool                       invert)
{
	BSP_CSGMesh* outputMesh = BOP_newEmptyMesh();

	if (outputMesh == NULL) return NULL;

	// vtx index dictionary, to translate indeces from input to output.
	map<int,unsigned int> dic;
	map<int,unsigned int>::iterator itDic;

	unsigned int count = 0;

	// Add a new face for each face in the input list
	BOP_Faces faces = mesh->getFaces();
	BOP_Vertexs vertexs = mesh->getVertexs();

	for (BOP_IT_Faces face = faces.begin(); face != faces.end(); face++) {
		if ((*face)->getTAG()!=BROKEN){    
			// Add output face
			outputMesh->FaceSet().push_back(BSP_MFace());
			BSP_MFace& outFace = outputMesh->FaceSet().back();

			// Copy face
			outFace.m_verts.clear();
			outFace.m_plane = (*face)->getPlane();
			outFace.m_orig_face = (*face)->getOriginalFace();

			// invert face if is required
			if (invert) (*face)->invert();
			
			// Add the face vertex if not added yet
			for (unsigned int pos=0;pos<(*face)->size();pos++) {
				BSP_VertexInd outVtxId;
				BOP_Index idVertex = (*face)->getVertex(pos);
				itDic = dic.find(idVertex);
				if (itDic == dic.end()) {
					// The vertex isn't added yet
					outVtxId = BSP_VertexInd(outputMesh->VertexSet().size());
					BSP_MVertex outVtx((mesh->getVertex(idVertex))->getPoint());
					outVtx.m_edges.clear();
					outputMesh->VertexSet().push_back(outVtx);
					dic[idVertex] = outVtxId;
					count++;
				}
				else {
					// The vertex is added
					outVtxId = BSP_VertexInd(itDic->second);
				}

				outFace.m_verts.push_back(outVtxId);
			}
		}
	}
	
	// Build the mesh edges using topological informtion
	outputMesh->BuildEdges();
	
	return outputMesh;
}
