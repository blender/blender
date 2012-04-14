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
using namespace carve::geom;
typedef unsigned int uint;

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

static bool isQuadPlanar(carve::geom3d::Vector &v1, carve::geom3d::Vector &v2,
                         carve::geom3d::Vector &v3, carve::geom3d::Vector &v4)
{
	carve::geom3d::Vector vec1, vec2, vec3, cross;

	vec1 = v2 - v1;
	vec2 = v4 - v1;
	vec3 = v3 - v1;

	cross = carve::geom::cross(vec1, vec2);

	float production = carve::geom::dot(cross, vec3);
	float magnitude = 1e-6 * cross.length();

	return fabs(production) < magnitude;
}

static bool isFacePlanar(CSG_IFace &face, std::vector<carve::geom3d::Vector> &vertices)
{
	if (face.vertex_number == 4) {
		return isQuadPlanar(vertices[face.vertex_index[0]], vertices[face.vertex_index[1]],
		                    vertices[face.vertex_index[2]], vertices[face.vertex_index[3]]);
	}

	return true;
}

static void Carve_copyMeshes(std::vector<MeshSet<3>::mesh_t*> &meshes, std::vector<MeshSet<3>::mesh_t*> &new_meshes)
{
	std::vector<MeshSet<3>::mesh_t*>::iterator it = meshes.begin();

	for(; it!=meshes.end(); it++) {
		MeshSet<3>::mesh_t *mesh = *it;
		MeshSet<3>::mesh_t *new_mesh = new MeshSet<3>::mesh_t(mesh->faces);

		new_meshes.push_back(new_mesh);
	}
}

static MeshSet<3> *Carve_meshSetFromMeshes(std::vector<MeshSet<3>::mesh_t*> &meshes)
{
	std::vector<MeshSet<3>::mesh_t*> new_meshes;

	Carve_copyMeshes(meshes, new_meshes);

	return new MeshSet<3>(new_meshes);
}

static MeshSet<3> *Carve_meshSetFromTwoMeshes(std::vector<MeshSet<3>::mesh_t*> &left_meshes,
                                              std::vector<MeshSet<3>::mesh_t*> &right_meshes)
{
	std::vector<MeshSet<3>::mesh_t*> new_meshes;

	Carve_copyMeshes(left_meshes, new_meshes);
	Carve_copyMeshes(right_meshes, new_meshes);

	return new MeshSet<3>(new_meshes);
}

static bool Carve_checkEdgeFaceIntersections_do(carve::csg::Intersections &intersections,
                                                MeshSet<3>::face_t *face_a, MeshSet<3>::edge_t *edge_b)
{
	if(intersections.intersects(edge_b, face_a))
		return true;

	carve::mesh::MeshSet<3>::vertex_t::vector_t _p;
	if(face_a->simpleLineSegmentIntersection(carve::geom3d::LineSegment(edge_b->v1()->v, edge_b->v2()->v), _p))
		return true;

	return false;
}

static bool Carve_checkEdgeFaceIntersections(carve::csg::Intersections &intersections,
                                             MeshSet<3>::face_t *face_a, MeshSet<3>::face_t *face_b)
{
	MeshSet<3>::edge_t *edge_b;

	edge_b = face_b->edge;
	do {
		if(Carve_checkEdgeFaceIntersections_do(intersections, face_a, edge_b))
			return true;
		edge_b = edge_b->next;
	} while (edge_b != face_b->edge);

	return false;
}

static inline bool Carve_facesAreCoplanar(const MeshSet<3>::face_t *a, const MeshSet<3>::face_t *b)
{
  carve::geom3d::Ray temp;
  // XXX: Find a better definition. This may be a source of problems
  // if floating point inaccuracies cause an incorrect answer.
  return !carve::geom3d::planeIntersection(a->plane, b->plane, temp);
}

static bool Carve_checkMeshSetInterseciton_do(carve::csg::Intersections &intersections,
                                              const RTreeNode<3, Face<3> *> *a_node,
                                              const RTreeNode<3, Face<3> *> *b_node,
                                              bool descend_a = true)
{
	if(!a_node->bbox.intersects(b_node->bbox))
		return false;

	if(a_node->child && (descend_a || !b_node->child)) {
		for(RTreeNode<3, Face<3> *> *node = a_node->child; node; node = node->sibling) {
			if(Carve_checkMeshSetInterseciton_do(intersections, node, b_node, false))
				return true;
		}
	}
	else if(b_node->child) {
		for(RTreeNode<3, Face<3> *> *node = b_node->child; node; node = node->sibling) {
			if(Carve_checkMeshSetInterseciton_do(intersections, a_node, node, true))
				return true;
		}
	}
	else {
		for(size_t i = 0; i < a_node->data.size(); ++i) {
			MeshSet<3>::face_t *fa = a_node->data[i];
			aabb<3> aabb_a = fa->getAABB();
			if(aabb_a.maxAxisSeparation(b_node->bbox) > carve::EPSILON) continue;

			for(size_t j = 0; j < b_node->data.size(); ++j) {
				MeshSet<3>::face_t *fb = b_node->data[j];
				aabb<3> aabb_b = fb->getAABB();
				if(aabb_b.maxAxisSeparation(aabb_a) > carve::EPSILON) continue;

				std::pair<double, double> a_ra = fa->rangeInDirection(fa->plane.N, fa->edge->vert->v);
				std::pair<double, double> b_ra = fb->rangeInDirection(fa->plane.N, fa->edge->vert->v);
				if(carve::rangeSeparation(a_ra, b_ra) > carve::EPSILON) continue;

				std::pair<double, double> a_rb = fa->rangeInDirection(fb->plane.N, fb->edge->vert->v);
				std::pair<double, double> b_rb = fb->rangeInDirection(fb->plane.N, fb->edge->vert->v);
				if(carve::rangeSeparation(a_rb, b_rb) > carve::EPSILON) continue;

				if(!Carve_facesAreCoplanar(fa, fb)) {
					if(Carve_checkEdgeFaceIntersections(intersections, fa, fb)) {
						return true;
					}
				}
			}
		}
	}

	return false;
}

static bool Carve_checkMeshSetInterseciton(RTreeNode<3, Face<3> *> *rtree_a, RTreeNode<3, Face<3> *> *rtree_b)
{
	carve::csg::Intersections intersections;

	return Carve_checkMeshSetInterseciton_do(intersections, rtree_a, rtree_b);
}

static void Carve_getIntersectedOperandMeshes(std::vector<MeshSet<3>::mesh_t*> &meshes, MeshSet<3>::aabb_t &otherAABB,
                                              std::vector<MeshSet<3>::mesh_t*> &operandMeshes)
{
	std::vector<MeshSet<3>::mesh_t*>::iterator it = meshes.begin();
	std::vector< RTreeNode<3, Face<3> *> *> meshRTree;

	while(it != meshes.end()) {
		MeshSet<3>::mesh_t *mesh = *it;
		bool isAdded = false;

		RTreeNode<3, Face<3> *> *rtree = RTreeNode<3, Face<3> *>::construct_STR(mesh->faces.begin(), mesh->faces.end(), 4, 4);

		if (rtree->bbox.intersects(otherAABB)) {
			bool isIntersect = false;

			std::vector<MeshSet<3>::mesh_t*>::iterator operand_it = operandMeshes.begin();
			std::vector<RTreeNode<3, Face<3> *> *>::iterator tree_it = meshRTree.begin();
			for(; operand_it!=operandMeshes.end(); operand_it++, tree_it++) {
				RTreeNode<3, Face<3> *> *operandRTree = *tree_it;

				if(Carve_checkMeshSetInterseciton(rtree, operandRTree)) {
					isIntersect = true;
					break;
				}
			}

			if(!isIntersect) {
				operandMeshes.push_back(mesh);
				meshRTree.push_back(rtree);

				it = meshes.erase(it);
				isAdded = true;
			}
		}

		if (!isAdded) {
			delete rtree;
			it++;
		}
	}

	std::vector<RTreeNode<3, Face<3> *> *>::iterator tree_it = meshRTree.begin();
	for(; tree_it != meshRTree.end(); tree_it++) {
		delete *tree_it;
	}
}

static MeshSet<3> *Carve_getIntersectedOperand(std::vector<MeshSet<3>::mesh_t*> &meshes, MeshSet<3>::aabb_t &otherAABB)
{
	std::vector<MeshSet<3>::mesh_t*> operandMeshes;
	Carve_getIntersectedOperandMeshes(meshes, otherAABB, operandMeshes);

	if (operandMeshes.size() == 0)
		return NULL;

	return Carve_meshSetFromMeshes(operandMeshes);
}

static MeshSet<3> *Carve_unionIntersectingMeshes(MeshSet<3> *poly,
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

	MeshSet<3> *left = Carve_getIntersectedOperand(orig_meshes, otherAABB);

	if (!left) {
		/* no maniforlds which intersects another object at all */
		return poly;
	}

	while(orig_meshes.size()) {
		MeshSet<3> *right = Carve_getIntersectedOperand(orig_meshes, otherAABB);

		if (!right) {
			/* no more intersecting manifolds which intersects other object */
			break;
		}

		try {
			if(left->meshes.size()==0) {
				delete left;

				left = right;
			}
			else {
				MeshSet<3> *result = csg.compute(left, right, carve::csg::CSG::UNION, NULL, carve::csg::CSG::CLASSIFY_EDGE);

				delete left;
				delete right;

				left = result;
			}
		}
		catch(carve::exception e) {
			std::cerr << "CSG failed, exception " << e.str() << std::endl;

			MeshSet<3> *result = Carve_meshSetFromTwoMeshes(left->meshes, right->meshes);

			delete left;
			delete right;

			left = result;
		}
		catch(...) {
			delete left;
			delete right;

			throw "Unknown error in Carve library";
		}
	}

	/* append all meshes which doesn't have intersection with another operand as-is */
	if (orig_meshes.size()) {
		MeshSet<3> *result = Carve_meshSetFromTwoMeshes(left->meshes, orig_meshes);

		delete left;

		return result;
	}

	return left;
}

static void Carve_unionIntersections(MeshSet<3> **left_r, MeshSet<3> **right_r,
                                     carve::interpolate::FaceAttr<uint> &oface_num)
{
	MeshSet<3> *left, *right;

	MeshSet<3>::aabb_t leftAABB = (*left_r)->getAABB();
	MeshSet<3>::aabb_t rightAABB = (*right_r)->getAABB();

	left = Carve_unionIntersectingMeshes(*left_r, rightAABB, oface_num);
	right = Carve_unionIntersectingMeshes(*right_r, leftAABB, oface_num);

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
		vertices.push_back(VECTOR(vertex.position[0],
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

static double triangleArea(carve::geom3d::Vector &v1, carve::geom3d::Vector &v2, carve::geom3d::Vector &v3)
{
	carve::geom3d::Vector a = v2 - v1;
	carve::geom3d::Vector b = v3 - v1;

	return carve::geom::cross(a, b).length();
}

static bool checkValidQuad(std::vector<MeshSet<3>::vertex_t> &vertex_storage, uint quad[4])
{
	carve::geom3d::Vector &v1 = vertex_storage[quad[0]].v;
	carve::geom3d::Vector &v2 = vertex_storage[quad[1]].v;
	carve::geom3d::Vector &v3 = vertex_storage[quad[2]].v;
	carve::geom3d::Vector &v4 = vertex_storage[quad[3]].v;

#if 0
	/* disabled for now to prevent initially non-planar be triangulated
	 * in theory this might cause some artifacts if intersections happens by non-planar
	 * non-concave quad, but in practice it's acceptable */
	if (!isQuadPlanar(v1, v2, v3, v4)) {
		/* non-planar faces better not be merged because of possible differences in triangulation
		 * of non-planar faces in opengl and renderer */
		return false;
	}
#endif

	carve::geom3d::Vector edges[4];
	carve::geom3d::Vector normal;
	bool normal_set = false;

	edges[0] = v2 - v1;
	edges[1] = v3 - v2;
	edges[2] = v4 - v3;
	edges[3] = v1 - v4;

	for (int i = 0; i < 4; i++) {
		int n = i + 1;

		if (n == 4)
			n = 0;

		carve::geom3d::Vector current_normal = carve::geom::cross(edges[i], edges[n]);

		if (current_normal.length() > DBL_EPSILON) {
			if (!normal_set) {
				normal = current_normal;
				normal_set = true;
			}
			else if (carve::geom::dot(normal, current_normal) < 0) {
				return false;
			}
		}
	}

	if (!normal_set) {
		/* normal wasn't set means face is degraded and better merge it in such way */
		return false;
	}

	double area = triangleArea(v1, v2, v3) + triangleArea(v1, v3, v4);
	if (area <= DBL_EPSILON)
		return false;

	return true;
}

// check whether two faces share an edge, and if so merge them
static uint quadMerge(std::map<MeshSet<3>::vertex_t*, uint> *vertexToIndex_map,
					  std::vector<MeshSet<3>::vertex_t> &vertex_storage,
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

		return checkValidQuad(vertex_storage, quad);
	}
	else if (v1[n1] == v2[p2]) {
		quad[0] = v1[current];
		quad[1] = v2[n2];
		quad[2] = v1[n1];
		quad[3] = v1[p1];

		return checkValidQuad(vertex_storage, quad);
	}

	return 0;
}

static bool Carve_checkDegeneratedFace(MeshSet<3>::face_t *face)
{
	/* only tris and quads for now */
	if (face->n_edges == 3) {
		return triangleArea(face->edge->prev->vert->v, face->edge->vert->v, face->edge->next->vert->v) < DBL_EPSILON;
	}
	else if (face->n_edges == 4) {
		return triangleArea(face->edge->vert->v, face->edge->next->vert->v, face->edge->next->next->vert->v) +
		       triangleArea(face->edge->prev->vert->v, face->edge->vert->v, face->edge->next->next->vert->v) < DBL_EPSILON;
	}

	return false;
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
	std::vector<std::vector<uint> > vi(poly->vertex_storage.size());
	std::vector<std::vector<uint> > ofaces(num_origfaces);
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
					result = quadMerge(&vertexToIndex_map, poly->vertex_storage, f, f2, v, quadverts);
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

			bool degenerativeFace = false;

			if (!result) {
				/* merged triangles are already checked for degenerative quad */
				degenerativeFace = Carve_checkDegeneratedFace(f);
			}

			if (!degenerativeFace) {
				// add all information except vertices to the output mesh
				outputMesh->FaceSet().push_back(BSP_MFace());
				BSP_MFace& outFace = outputMesh->FaceSet().back();
				outFace.m_verts.clear();
				outFace.m_plane.setValue(f->plane.N.v);
				outFace.m_orig_face = orig;

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
	MeshSet<3> *left, *right, *output = NULL;
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

	// prepare operands for actual boolean operation. it's needed because operands might consist of
	// several intersecting meshes and in case if another operands intersect an edge loop of intersecting that
	// meshes tessellation of operation result can't be done properly. the only way to make such situations
	// working is to union intersecting meshes of the same operand
	Carve_unionIntersections(&left, &right, oface_num);

	if(left->meshes.size() == 0 || right->meshes.size()==0) {
		// normally sohuldn't happen (zero-faces objects are handled by modifier itself), but
		// unioning intersecting meshes which doesn't have consistent normals might lead to
		// empty result which wouldn't work here

		delete left;
		delete right;

		return BOP_ERROR;
	}

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
