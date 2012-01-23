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
 * The Original Code is Copyright (C) 2010 by the Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Ken Hughes,
 *                 Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file boolop/intern/BOP_CarveInterface.cpp
 *  \ingroup boolopintern
 */

#include "../extern/BOP_Interface.h"
#include "../../bsp/intern/BSP_CSGMesh_CFIterator.h"

#include <carve/csg_triangulator.hpp>
#include <carve/interpolator.hpp>
#include <carve/rescale.hpp>

#include <iostream>

using namespace carve::mesh;
typedef unsigned int uint;

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

static int isFacePlanar(CSG_IFace &face, std::vector<carve::geom3d::Vector> &vertices)
{
	carve::geom3d::Vector v1, v2, v3, cross;

	if (face.vertex_number == 4) {
		v1 = vertices[face.vertex_index[1]] - vertices[face.vertex_index[0]];
		v2 = vertices[face.vertex_index[3]] - vertices[face.vertex_index[0]];
		v3 = vertices[face.vertex_index[2]] - vertices[face.vertex_index[0]];

		cross = carve::geom::cross(v1, v2);

		float production = carve::geom::dot(cross, v3);
		float magnitude = 1e-6 * cross.length();

		return fabs(production) < magnitude;
	}

	return 1;
}

static MeshSet<3> *Carve_meshSetFromMeshes(std::vector<MeshSet<3>::mesh_t*> &meshes)
{
	std::vector<MeshSet<3>::mesh_t*> new_meshes;

	std::vector<MeshSet<3>::mesh_t*>::iterator it = meshes.begin();
	for(; it!=meshes.end(); it++) {
		MeshSet<3>::mesh_t *mesh = *it;
		MeshSet<3>::mesh_t *new_mesh = new MeshSet<3>::mesh_t(mesh->faces);

		new_meshes.push_back(new_mesh);
	}

	return new MeshSet<3>(new_meshes);
}

static void Carve_getIntersectedOperandMeshes(std::vector<MeshSet<3>::mesh_t*> &meshes,
                                              std::vector<MeshSet<3>::aabb_t> &precomputedAABB,
                                              MeshSet<3>::aabb_t &otherAABB,
                                              std::vector<MeshSet<3>::mesh_t*> &operandMeshes)
{
	std::vector<MeshSet<3>::mesh_t*>::iterator it = meshes.begin();
	std::vector<MeshSet<3>::aabb_t>::iterator aabb_it = precomputedAABB.begin();
	std::vector<MeshSet<3>::aabb_t> usedAABB;

	while(it != meshes.end()) {
		MeshSet<3>::mesh_t *mesh = *it;
		MeshSet<3>::aabb_t aabb = mesh->getAABB();
		bool isIntersect = false;

		std::vector<MeshSet<3>::aabb_t>::iterator used_it = usedAABB.begin();
		for(; used_it!=usedAABB.end(); used_it++) {
			MeshSet<3>::aabb_t usedAABB = *used_it;

			if(usedAABB.intersects(aabb) && usedAABB.intersects(otherAABB)) {
				isIntersect = true;
				break;
			}
		}

		if(!isIntersect) {
			operandMeshes.push_back(mesh);
			usedAABB.push_back(aabb);

			it = meshes.erase(it);
			aabb_it = precomputedAABB.erase(aabb_it);
		}
		else {
			it++;
			aabb_it++;
		}
	}
}

static MeshSet<3> *Carve_getIntersectedOperand(std::vector<MeshSet<3>::mesh_t*> &meshes,
                                               std::vector<MeshSet<3>::aabb_t> &precomputedAABB,
                                               MeshSet<3>::aabb_t &otherAABB)
{
	std::vector<MeshSet<3>::mesh_t*> operandMeshes;
	Carve_getIntersectedOperandMeshes(meshes, precomputedAABB, otherAABB, operandMeshes);

	return Carve_meshSetFromMeshes(operandMeshes);
}

static MeshSet<3> *Carve_unionIntersectingMeshes(MeshSet<3> *poly,
                                                 std::vector<MeshSet<3>::aabb_t> &precomputedAABB,
                                                 MeshSet<3>::aabb_t &otherAABB,
                                                 carve::interpolate::FaceAttr<uint> &oface_num)
{
	if(poly->meshes.size()<=1)
		return poly;

	carve::csg::CSG csg;

	oface_num.installHooks(csg);
	csg.hooks.registerHook(new carve::csg::CarveTriangulator, carve::csg::CSG::Hooks::PROCESS_OUTPUT_FACE_BIT);

	std::vector<MeshSet<3>::mesh_t*> orig_meshes =
			std::vector<MeshSet<3>::mesh_t*>(poly->meshes.begin(), poly->meshes.end());

	MeshSet<3> *left = Carve_getIntersectedOperand(orig_meshes, precomputedAABB, otherAABB);

	while(orig_meshes.size()) {
		MeshSet<3> *right = Carve_getIntersectedOperand(orig_meshes, precomputedAABB, otherAABB);

		try {
			MeshSet<3> *result = csg.compute(left, right, carve::csg::CSG::UNION, NULL, carve::csg::CSG::CLASSIFY_EDGE);

			delete left;
			delete right;

			left = result;
		}
		catch(carve::exception e) {
			std::cerr << "CSG failed, exception " << e.str() << std::endl;

			delete right;
		}
		catch(...) {
			delete left;
			delete right;

			throw "Unknown error in Carve library";
		}
	}

	return left;
}

static MeshSet<3>::aabb_t Carve_computeAABB(MeshSet<3> *poly,
                                            std::vector<MeshSet<3>::aabb_t> &precomputedAABB)
{
	MeshSet<3>::aabb_t overallAABB;
	std::vector<MeshSet<3>::mesh_t*>::iterator it = poly->meshes.begin();

	for(; it!=poly->meshes.end(); it++) {
		MeshSet<3>::aabb_t aabb;
		MeshSet<3>::mesh_t *mesh = *it;

		aabb = mesh->getAABB();
		precomputedAABB.push_back(aabb);

		overallAABB.unionAABB(aabb);
	}

	return overallAABB;
}

static void Carve_prepareOperands(MeshSet<3> **left_r, MeshSet<3> **right_r,
                                  carve::interpolate::FaceAttr<uint> &oface_num)
{
	MeshSet<3> *left, *right;

	std::vector<MeshSet<3>::aabb_t> left_precomputedAABB;
	std::vector<MeshSet<3>::aabb_t> right_precomputedAABB;

	MeshSet<3>::aabb_t leftAABB = Carve_computeAABB(*left_r, left_precomputedAABB);
	MeshSet<3>::aabb_t rightAABB = Carve_computeAABB(*right_r, right_precomputedAABB);

	left = Carve_unionIntersectingMeshes(*left_r, left_precomputedAABB, rightAABB, oface_num);
	right = Carve_unionIntersectingMeshes(*right_r, right_precomputedAABB, leftAABB, oface_num);

	if(left != *left_r)
		delete *left_r;

	if(right != *right_r)
		delete *right_r;

	*left_r = left;
	*right_r = right;
}

static MeshSet<3> *Carve_addMesh(CSG_FaceIteratorDescriptor &face_it,
                                 CSG_VertexIteratorDescriptor &vertex_it,
                                 carve::interpolate::FaceAttr<uint> &oface_num,
                                 uint &num_origfaces)
{
	CSG_IVertex vertex;
	std::vector<carve::geom3d::Vector> vertices;

	while (!vertex_it.Done(vertex_it.it)) {
		vertex_it.Fill(vertex_it.it,&vertex);
		vertices.push_back(carve::geom::VECTOR(vertex.position[0],
		                                       vertex.position[1],
		                                       vertex.position[2]));
		vertex_it.Step(vertex_it.it);
	}

	CSG_IFace face;
	std::vector<int> f;
	int numfaces = 0;

	// now for the polygons.
	// we may need to decalare some memory for user defined face properties.

	std::vector<int> forig;
	while (!face_it.Done(face_it.it)) {
		face_it.Fill(face_it.it,&face);

		if (isFacePlanar(face, vertices)) {
			f.push_back(face.vertex_number);
			f.push_back(face.vertex_index[0]);
			f.push_back(face.vertex_index[1]);
			f.push_back(face.vertex_index[2]);

			if (face.vertex_number == 4)
				f.push_back(face.vertex_index[3]);

			forig.push_back(face.orig_face);
			++numfaces;
			face_it.Step(face_it.it);
			++num_origfaces;
		}
		else {
			f.push_back(3);
			f.push_back(face.vertex_index[0]);
			f.push_back(face.vertex_index[1]);
			f.push_back(face.vertex_index[2]);

			forig.push_back(face.orig_face);
			++numfaces;

			if (face.vertex_number == 4) {
				f.push_back(3);
				f.push_back(face.vertex_index[0]);
				f.push_back(face.vertex_index[2]);
				f.push_back(face.vertex_index[3]);

				forig.push_back(face.orig_face);
				++numfaces;
			}

			face_it.Step(face_it.it);
			++num_origfaces;
		}
	}

	MeshSet<3> *poly = new MeshSet<3> (vertices, numfaces, f);

	uint i;
	MeshSet<3>::face_iter face_iter = poly->faceBegin();
	for (i = 0; face_iter != poly->faceEnd(); ++face_iter, ++i) {
		MeshSet<3>::face_t *face = *face_iter;
		oface_num.setAttribute(face, forig[i]);
	}

	return poly;
}

// check whether two faces share an edge, and if so merge them
static uint quadMerge(std::map<MeshSet<3>::vertex_t*, uint> *vertexToIndex_map,
                      MeshSet<3>::face_t *f1, MeshSet<3>::face_t *f2,
                      uint v, uint quad[4])
{
	uint current, n1, p1, n2, p2;
	uint v1[3];
	uint v2[3];

	// get the vertex indices for each face
	v1[0] = vertexToIndex_map->find(f1->edge->vert)->second;
	v1[1] = vertexToIndex_map->find(f1->edge->next->vert)->second;
	v1[2] = vertexToIndex_map->find(f1->edge->next->next->vert)->second;

	v2[0] = vertexToIndex_map->find(f2->edge->vert)->second;
	v2[1] = vertexToIndex_map->find(f2->edge->next->vert)->second;
	v2[2] = vertexToIndex_map->find(f2->edge->next->next->vert)->second;

	// locate the current vertex we're examining, and find the next and
	// previous vertices based on the face windings
	if (v1[0] == v) {current = 0; p1 = 2; n1 = 1;}
	else if (v1[1] == v) {current = 1; p1 = 0; n1 = 2;}
	else {current = 2; p1 = 1; n1 = 0;}

	if (v2[0] == v) {p2 = 2; n2 = 1;}
	else if (v2[1] == v) {p2 = 0; n2 = 2;}
	else {p2 = 1; n2 = 0;}

	// if we find a match, place indices into quad in proper order and return
	// success code
	if (v1[p1] == v2[n2]) {
		quad[0] = v1[current];
		quad[1] = v1[n1];
		quad[2] = v1[p1];
		quad[3] = v2[p2];

		return 1;
	}
	else if (v1[n1] == v2[p2]) {
		quad[0] = v1[current];
		quad[1] = v2[n2];
		quad[2] = v1[n1];
		quad[3] = v1[p1];

		return 1;
	}

	return 0;
}

static BSP_CSGMesh *Carve_exportMesh(MeshSet<3>* &poly, carve::interpolate::FaceAttr<uint> &oface_num,
                                     uint num_origfaces)
{
	uint i;
	BSP_CSGMesh *outputMesh = BSP_CSGMesh::New();

	if (outputMesh == NULL)
		return NULL;

	std::vector<BSP_MVertex> *vertices = new std::vector<BSP_MVertex>;

	outputMesh->SetVertices(vertices);

	std::map<MeshSet<3>::vertex_t*, uint> vertexToIndex_map;
	std::vector<MeshSet<3>::vertex_t>::iterator it = poly->vertex_storage.begin();
	for (i = 0; it != poly->vertex_storage.end(); ++i, ++it) {
		MeshSet<3>::vertex_t *vertex = &(*it);
		vertexToIndex_map[vertex] = i;
	}

	for (i = 0; i < poly->vertex_storage.size(); ++i ) {
		BSP_MVertex outVtx(MT_Point3 (poly->vertex_storage[i].v[0],
		                              poly->vertex_storage[i].v[1],
		                              poly->vertex_storage[i].v[2]));
		outVtx.m_edges.clear();
		outputMesh->VertexSet().push_back(outVtx);
	}

	// build vectors of faces for each original face and each vertex 
	std::vector<std::vector<uint>> vi(poly->vertex_storage.size());
	std::vector<std::vector<uint>> ofaces(num_origfaces);
	MeshSet<3>::face_iter face_iter = poly->faceBegin();
	for (i = 0; face_iter != poly->faceEnd(); ++face_iter, ++i) {
		MeshSet<3>::face_t *f = *face_iter;
		ofaces[oface_num.getAttribute(f)].push_back(i);
		MeshSet<3>::face_t::edge_iter_t edge_iter = f->begin();
		for (; edge_iter != f->end(); ++edge_iter) {
			int index = vertexToIndex_map[edge_iter->vert];
			vi[index].push_back(i);
		}
	}

	uint quadverts[4] = {0, 0, 0, 0};
	// go over each set of faces which belong to an original face
	std::vector< std::vector<uint> >::const_iterator fii;
	uint orig = 0;
	for (fii=ofaces.begin(); fii!=ofaces.end(); ++fii, ++orig) {
		std::vector<uint> fl = *fii;
		// go over a single set from an original face
		while (fl.size() > 0) {
			// remove one new face
			uint findex = fl.back();
			fl.pop_back();

			MeshSet<3>::face_t *f = *(poly->faceBegin() + findex);

			// add all information except vertices to the output mesh
			outputMesh->FaceSet().push_back(BSP_MFace());
			BSP_MFace& outFace = outputMesh->FaceSet().back();
			outFace.m_verts.clear();
			outFace.m_plane.setValue(f->plane.N.v);
			outFace.m_orig_face = orig;

			// for each vertex of this face, check other faces containing
			// that vertex to see if there is a neighbor also belonging to
			// the original face
			uint result = 0;

			MeshSet<3>::face_t::edge_iter_t edge_iter = f->begin();
			for (; edge_iter != f->end(); ++edge_iter) {
				int v = vertexToIndex_map[edge_iter->vert];
				for (uint pos2=0; !result && pos2 < vi[v].size();pos2++) {

					// if we find the current face, ignore it
					uint otherf = vi[v][pos2];
					if (findex == otherf)
						continue;

					MeshSet<3>::face_t *f2 = *(poly->faceBegin() + otherf);

					// if other face doesn't have the same original face,
					// ignore it also
					uint other_orig = oface_num.getAttribute(f2);
					if (orig != other_orig)
						continue;

					// if, for some reason, we don't find the other face in
					// the current set of faces, ignore it
					uint other_index = 0;
					while (other_index < fl.size() && fl[other_index] != otherf) ++other_index;
					if (other_index == fl.size()) continue;

					// see if the faces share an edge
					result = quadMerge(&vertexToIndex_map, f, f2, v, quadverts);
					// if faces can be merged, then remove the other face
					// from the current set
					if (result) {
						uint replace = fl.back();
						fl.pop_back();
						if(otherf != replace)
							fl[other_index] = replace;
					}
				}
			}

			// if we merged faces, use the list of common vertices; otherwise
			// use the faces's vertices
			if (result) {
				// make quat using verts stored in result
				outFace.m_verts.push_back(quadverts[0]);
				outFace.m_verts.push_back(quadverts[1]);
				outFace.m_verts.push_back(quadverts[2]);
				outFace.m_verts.push_back(quadverts[3]);
			} else {
				MeshSet<3>::face_t::edge_iter_t edge_iter = f->begin();
				for (; edge_iter != f->end(); ++edge_iter) {
					//int index = ofacevert_num.getAttribute(f, edge_iter.idx());
					int index = vertexToIndex_map[edge_iter->vert];
					outFace.m_verts.push_back( index );
				}
			}
		}
	}

	// Build the mesh edges using topological informtion
	outputMesh->BuildEdges();
	
	return outputMesh;
}

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
	carve::csg::CSG::OP op;
	MeshSet<3> *left, *right, *output;
	carve::csg::CSG csg;
	carve::geom3d::Vector min, max;
	carve::interpolate::FaceAttr<uint> oface_num;
	uint num_origfaces = 0;

	switch (opType) {
		case BOP_UNION:
			op = carve::csg::CSG::UNION;
			break;
		case BOP_INTERSECTION:
			op = carve::csg::CSG::INTERSECTION;
			break;
		case BOP_DIFFERENCE:
			op = carve::csg::CSG::A_MINUS_B;
			break;
		default:
			return BOP_ERROR;
	}

	left = Carve_addMesh(obAFaces, obAVertices, oface_num, num_origfaces );
	right = Carve_addMesh(obBFaces, obBVertices, oface_num, num_origfaces );

	Carve_prepareOperands(&left, &right, oface_num);

	min.x = max.x = left->vertex_storage[0].v.x;
	min.y = max.y = left->vertex_storage[0].v.y;
	min.z = max.z = left->vertex_storage[0].v.z;
	for (uint i = 1; i < left->vertex_storage.size(); ++i) {
		min.x = MIN(min.x,left->vertex_storage[i].v.x);
		min.y = MIN(min.y,left->vertex_storage[i].v.y);
		min.z = MIN(min.z,left->vertex_storage[i].v.z);
		max.x = MAX(max.x,left->vertex_storage[i].v.x);
		max.y = MAX(max.y,left->vertex_storage[i].v.y);
		max.z = MAX(max.z,left->vertex_storage[i].v.z);
	}
	for (uint i = 0; i < right->vertex_storage.size(); ++i) {
		min.x = MIN(min.x,right->vertex_storage[i].v.x);
		min.y = MIN(min.y,right->vertex_storage[i].v.y);
		min.z = MIN(min.z,right->vertex_storage[i].v.z);
		max.x = MAX(max.x,right->vertex_storage[i].v.x);
		max.y = MAX(max.y,right->vertex_storage[i].v.y);
		max.z = MAX(max.z,right->vertex_storage[i].v.z);
	}

	carve::rescale::rescale scaler(min.x, min.y, min.z, max.x, max.y, max.z);
	carve::rescale::fwd fwd_r(scaler);
	carve::rescale::rev rev_r(scaler); 

	left->transform(fwd_r);
	right->transform(fwd_r);

	csg.hooks.registerHook(new carve::csg::CarveTriangulator, carve::csg::CSG::Hooks::PROCESS_OUTPUT_FACE_BIT);

	oface_num.installHooks(csg);

	try {
		output = csg.compute(left, right, op, NULL, carve::csg::CSG::CLASSIFY_EDGE);
	}
	catch(carve::exception e) {
		std::cerr << "CSG failed, exception " << e.str() << std::endl;
	}
	catch(...) {
		delete left;
		delete right;

		throw "Unknown error in Carve library";
	}

	delete left;
	delete right;

	if(!output)
		return BOP_ERROR;

	output->transform(rev_r);

	*outputMesh = Carve_exportMesh(output, oface_num, num_origfaces);
	delete output;

	return BOP_OK;
}
