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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "carve-util.h"
#include "carve-capi.h"

#include <cfloat>
#include <carve/csg.hpp>
#include <carve/csg_triangulator.hpp>
#include <carve/rtree.hpp>

using carve::csg::Intersections;
using carve::geom::aabb;
using carve::geom::RTreeNode;
using carve::geom3d::Vector;
using carve::math::Matrix3;
using carve::mesh::Face;
using carve::mesh::MeshSet;
using carve::triangulate::tri_idx;
using carve::triangulate::triangulate;

typedef std::map< MeshSet<3>::mesh_t*, RTreeNode<3, Face<3> *> * > RTreeCache;
typedef std::map< MeshSet<3>::mesh_t*, bool > IntersectCache;

namespace {

// Functions adopted from BLI_math.h to use Carve Vector and Matrix.

void transpose_m3__bli(double mat[3][3])
{
	double t;

	t = mat[0][1];
	mat[0][1] = mat[1][0];
	mat[1][0] = t;
	t = mat[0][2];
	mat[0][2] = mat[2][0];
	mat[2][0] = t;
	t = mat[1][2];
	mat[1][2] = mat[2][1];
	mat[2][1] = t;
}

void ortho_basis_v3v3_v3__bli(double r_n1[3], double r_n2[3], const double n[3])
{
	const double eps = FLT_EPSILON;
	const double f = (n[0] * n[0]) + (n[1] * n[1]);

	if (f > eps) {
		const double d = 1.0f / sqrt(f);

		r_n1[0] =  n[1] * d;
		r_n1[1] = -n[0] * d;
		r_n1[2] =  0.0f;
		r_n2[0] = -n[2] * r_n1[1];
		r_n2[1] =  n[2] * r_n1[0];
		r_n2[2] =  n[0] * r_n1[1] - n[1] * r_n1[0];
	}
	else {
		/* degenerate case */
		r_n1[0] = (n[2] < 0.0f) ? -1.0f : 1.0f;
		r_n1[1] = r_n1[2] = r_n2[0] = r_n2[2] = 0.0f;
		r_n2[1] = 1.0f;
	}
}

void axis_dominant_v3_to_m3__bli(Matrix3 *r_mat, const Vector &normal)
{
	memcpy(r_mat->m[2], normal.v, sizeof(double[3]));
	ortho_basis_v3v3_v3__bli(r_mat->m[0], r_mat->m[1], r_mat->m[2]);

	transpose_m3__bli(r_mat->m);
}


void meshset_minmax(const MeshSet<3> *mesh,
                    Vector *min,
                    Vector *max)
{
	for (size_t i = 0; i < mesh->vertex_storage.size(); ++i) {
		min->x = std::min(min->x, mesh->vertex_storage[i].v.x);
		min->y = std::min(min->y, mesh->vertex_storage[i].v.y);
		min->z = std::min(min->z, mesh->vertex_storage[i].v.z);
		max->x = std::max(max->x, mesh->vertex_storage[i].v.x);
		max->y = std::max(max->y, mesh->vertex_storage[i].v.y);
		max->z = std::max(max->z, mesh->vertex_storage[i].v.z);
	}
}

}  // namespace

void carve_getRescaleMinMax(const MeshSet<3> *left,
                            const MeshSet<3> *right,
                            Vector *min,
                            Vector *max)
{
	min->x = max->x = left->vertex_storage[0].v.x;
	min->y = max->y = left->vertex_storage[0].v.y;
	min->z = max->z = left->vertex_storage[0].v.z;

	meshset_minmax(left, min, max);
	meshset_minmax(right, min, max);

	// Make sure we don't scale object with zero scale.
	if (std::abs(min->x - max->x) < carve::EPSILON) {
		min->x = -1.0;
		max->x = 1.0;
	}
	if (std::abs(min->y - max->y) < carve::EPSILON) {
		min->y = -1.0;
		max->y = 1.0;
	}
	if (std::abs(min->z - max->z) < carve::EPSILON) {
		min->z = -1.0;
		max->z = 1.0;
	}
}

namespace {

struct UnionIntersectionContext {
	VertexAttrsCallback vertex_attr_callback;
	void *user_data;
};

void copyMeshes(const std::vector<MeshSet<3>::mesh_t*> &meshes,
                std::vector<MeshSet<3>::mesh_t*> *new_meshes)
{
	std::vector<MeshSet<3>::mesh_t*>::const_iterator it = meshes.begin();
	new_meshes->reserve(meshes.size());
	for (; it != meshes.end(); it++) {
		MeshSet<3>::mesh_t *mesh = *it;
		MeshSet<3>::mesh_t *new_mesh = new MeshSet<3>::mesh_t(mesh->faces);

		new_meshes->push_back(new_mesh);
	}
}

struct NewMeshMapping {
	std::map<const MeshSet<3>::edge_t*, MeshSet<3>::vertex_t*> orig_edge_vert;
};

void prepareNewMeshMapping(const std::vector<MeshSet<3>::mesh_t*> &meshes,
                           NewMeshMapping *mapping)
{
	for (size_t m = 0; m < meshes.size(); ++m) {
		MeshSet<3>::mesh_t *mesh = meshes[m];
		for (size_t f = 0; f < mesh->faces.size(); ++f) {
			MeshSet<3>::face_t *face = mesh->faces[f];
			MeshSet<3>::edge_t *edge = face->edge;
			do {
				mapping->orig_edge_vert[edge] = edge->vert;
				edge = edge->next;
			} while (edge != face->edge);
		}
	}
}

void runNewMeshSetHooks(UnionIntersectionContext *ctx,
                        NewMeshMapping *mapping,
                        MeshSet<3> *mesh_set)
{
	for (size_t m = 0; m < mesh_set->meshes.size(); ++m) {
		MeshSet<3>::mesh_t *mesh = mesh_set->meshes[m];
		for (size_t f = 0; f < mesh->faces.size(); ++f) {
			MeshSet<3>::face_t *face = mesh->faces[f];
			MeshSet<3>::edge_t *edge = face->edge;
			do {
				const MeshSet<3>::vertex_t *orig_vert = mapping->orig_edge_vert[edge];
				const MeshSet<3>::vertex_t *new_vert = edge->vert;
				ctx->vertex_attr_callback(orig_vert, new_vert, ctx->user_data);
				edge = edge->next;
			} while (edge != face->edge);
		}
	}
}

MeshSet<3> *newMeshSetFromMeshesWithAttrs(
    UnionIntersectionContext *ctx,
    std::vector<MeshSet<3>::mesh_t*> &meshes)
{
	NewMeshMapping mapping;
	prepareNewMeshMapping(meshes, &mapping);
	MeshSet<3> *mesh_set = new MeshSet<3>(meshes);
	runNewMeshSetHooks(ctx, &mapping, mesh_set);
	return mesh_set;
}


MeshSet<3> *meshSetFromMeshes(UnionIntersectionContext *ctx,
                              const std::vector<MeshSet<3>::mesh_t*> &meshes)
{
	std::vector<MeshSet<3>::mesh_t*> new_meshes;
	copyMeshes(meshes, &new_meshes);
	return newMeshSetFromMeshesWithAttrs(ctx, new_meshes);
}

MeshSet<3> *meshSetFromTwoMeshes(UnionIntersectionContext *ctx,
                                 const std::vector<MeshSet<3>::mesh_t*> &left_meshes,
                                 const std::vector<MeshSet<3>::mesh_t*> &right_meshes)
{
	std::vector<MeshSet<3>::mesh_t*> new_meshes;
	copyMeshes(left_meshes, &new_meshes);
	copyMeshes(right_meshes, &new_meshes);
	return newMeshSetFromMeshesWithAttrs(ctx, new_meshes);
}

bool checkEdgeFaceIntersections_do(Intersections &intersections,
                                   MeshSet<3>::face_t *face_a,
                                   MeshSet<3>::edge_t *edge_b)
{
	if (intersections.intersects(edge_b, face_a))
		return true;

	carve::mesh::MeshSet<3>::vertex_t::vector_t _p;
	if (face_a->simpleLineSegmentIntersection(carve::geom3d::LineSegment(edge_b->v1()->v, edge_b->v2()->v), _p))
		return true;

	return false;
}

bool checkEdgeFaceIntersections(Intersections &intersections,
                                MeshSet<3>::face_t *face_a,
                                MeshSet<3>::face_t *face_b)
{
	MeshSet<3>::edge_t *edge_b;

	edge_b = face_b->edge;
	do {
		if (checkEdgeFaceIntersections_do(intersections, face_a, edge_b))
			return true;
		edge_b = edge_b->next;
	} while (edge_b != face_b->edge);

	return false;
}

inline bool facesAreCoplanar(const MeshSet<3>::face_t *a, const MeshSet<3>::face_t *b)
{
	carve::geom3d::Ray temp;
	// XXX: Find a better definition. This may be a source of problems
	// if floating point inaccuracies cause an incorrect answer.
	return !carve::geom3d::planeIntersection(a->plane, b->plane, temp);
}

bool checkMeshSetInterseciton_do(Intersections &intersections,
                                 const RTreeNode<3, Face<3> *> *a_node,
                                 const RTreeNode<3, Face<3> *> *b_node,
                                 bool descend_a = true)
{
	if (!a_node->bbox.intersects(b_node->bbox)) {
		return false;
	}

	if (a_node->child && (descend_a || !b_node->child)) {
		for (RTreeNode<3, Face<3> *> *node = a_node->child; node; node = node->sibling) {
			if (checkMeshSetInterseciton_do(intersections, node, b_node, false)) {
				return true;
			}
		}
	}
	else if (b_node->child) {
		for (RTreeNode<3, Face<3> *> *node = b_node->child; node; node = node->sibling) {
			if (checkMeshSetInterseciton_do(intersections, a_node, node, true)) {
				return true;
			}
		}
	}
	else {
		for (size_t i = 0; i < a_node->data.size(); ++i) {
			MeshSet<3>::face_t *fa = a_node->data[i];
			aabb<3> aabb_a = fa->getAABB();
			if (aabb_a.maxAxisSeparation(b_node->bbox) > carve::EPSILON) {
				continue;
			}

			for (size_t j = 0; j < b_node->data.size(); ++j) {
				MeshSet<3>::face_t *fb = b_node->data[j];
				aabb<3> aabb_b = fb->getAABB();
				if (aabb_b.maxAxisSeparation(aabb_a) > carve::EPSILON) {
					continue;
				}

				std::pair<double, double> a_ra = fa->rangeInDirection(fa->plane.N, fa->edge->vert->v);
				std::pair<double, double> b_ra = fb->rangeInDirection(fa->plane.N, fa->edge->vert->v);
				if (carve::rangeSeparation(a_ra, b_ra) > carve::EPSILON) {
					continue;
				}

				std::pair<double, double> a_rb = fa->rangeInDirection(fb->plane.N, fb->edge->vert->v);
				std::pair<double, double> b_rb = fb->rangeInDirection(fb->plane.N, fb->edge->vert->v);
				if (carve::rangeSeparation(a_rb, b_rb) > carve::EPSILON) {
					continue;
				}

				if (!facesAreCoplanar(fa, fb)) {
					if (checkEdgeFaceIntersections(intersections, fa, fb)) {
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool checkMeshSetInterseciton(RTreeNode<3, Face<3> *> *rtree_a, RTreeNode<3, Face<3> *> *rtree_b)
{
	Intersections intersections;
	return checkMeshSetInterseciton_do(intersections, rtree_a, rtree_b);
}

void getIntersectedOperandMeshes(std::vector<MeshSet<3>::mesh_t*> *meshes,
                                 const MeshSet<3>::aabb_t &otherAABB,
                                 std::vector<MeshSet<3>::mesh_t*> *operandMeshes,
                                 RTreeCache *rtree_cache,
                                 IntersectCache *intersect_cache)
{
	std::vector<MeshSet<3>::mesh_t*>::iterator it = meshes->begin();
	std::vector< RTreeNode<3, Face<3> *> *> meshRTree;

	while (it != meshes->end()) {
		MeshSet<3>::mesh_t *mesh = *it;
		bool isAdded = false;

		RTreeNode<3, Face<3> *> *rtree;
		bool intersects;

		RTreeCache::iterator rtree_found = rtree_cache->find(mesh);
		if (rtree_found != rtree_cache->end()) {
			rtree = rtree_found->second;
		}
		else {
			rtree = RTreeNode<3, Face<3> *>::construct_STR(mesh->faces.begin(), mesh->faces.end(), 4, 4);
			(*rtree_cache)[mesh] = rtree;
		}

		IntersectCache::iterator intersect_found = intersect_cache->find(mesh);
		if (intersect_found != intersect_cache->end()) {
			intersects = intersect_found->second;
		}
		else {
			intersects = rtree->bbox.intersects(otherAABB);
			(*intersect_cache)[mesh] = intersects;
		}

		if (intersects) {
			bool isIntersect = false;

			std::vector<MeshSet<3>::mesh_t*>::iterator operand_it = operandMeshes->begin();
			std::vector<RTreeNode<3, Face<3> *> *>::iterator tree_it = meshRTree.begin();
			for (; operand_it!=operandMeshes->end(); operand_it++, tree_it++) {
				RTreeNode<3, Face<3> *> *operandRTree = *tree_it;

				if (checkMeshSetInterseciton(rtree, operandRTree)) {
					isIntersect = true;
					break;
				}
			}

			if (!isIntersect) {
				operandMeshes->push_back(mesh);
				meshRTree.push_back(rtree);

				it = meshes->erase(it);
				isAdded = true;
			}
		}

		if (!isAdded) {
			//delete rtree;
			it++;
		}
	}

	std::vector<RTreeNode<3, Face<3> *> *>::iterator tree_it = meshRTree.begin();
	for (; tree_it != meshRTree.end(); tree_it++) {
		//delete *tree_it;
	}
}

MeshSet<3> *getIntersectedOperand(UnionIntersectionContext *ctx,
                                  std::vector<MeshSet<3>::mesh_t*> *meshes,
                                  const MeshSet<3>::aabb_t &otherAABB,
                                  RTreeCache *rtree_cache,
                                  IntersectCache *intersect_cache)
{
	std::vector<MeshSet<3>::mesh_t*> operandMeshes;
	getIntersectedOperandMeshes(meshes, otherAABB, &operandMeshes, rtree_cache, intersect_cache);

	if (operandMeshes.size() == 0)
		return NULL;

	return meshSetFromMeshes(ctx, operandMeshes);
}

MeshSet<3> *unionIntersectingMeshes(carve::csg::CSG *csg,
                                    MeshSet<3> *poly,
                                    const MeshSet<3> *other_poly,
                                    const MeshSet<3>::aabb_t &otherAABB,
                                    VertexAttrsCallback vertex_attr_callback,
                                    UnionIntersectionsCallback callback,
                                    void *user_data)
{
	if (poly->meshes.size() <= 1) {
		return poly;
	}

	std::vector<MeshSet<3>::mesh_t*> orig_meshes =
			std::vector<MeshSet<3>::mesh_t*>(poly->meshes.begin(), poly->meshes.end());

	RTreeCache rtree_cache;
	IntersectCache intersect_cache;

	UnionIntersectionContext ctx;
	ctx.vertex_attr_callback = vertex_attr_callback;
	ctx.user_data = user_data;

	MeshSet<3> *left = getIntersectedOperand(&ctx,
	                                         &orig_meshes,
	                                         otherAABB,
	                                         &rtree_cache,
	                                         &intersect_cache);

	if (!left) {
		// No maniforlds which intersects another object at all.
		return poly;
	}

	while (orig_meshes.size()) {
		MeshSet<3> *right = getIntersectedOperand(&ctx,
		                                          &orig_meshes,
		                                          otherAABB,
		                                          &rtree_cache,
		                                          &intersect_cache);

		if (!right) {
			// No more intersecting manifolds which intersects other object
			break;
		}

		try {
			if (left->meshes.size()==0) {
				delete left;

				left = right;
			}
			else {
				MeshSet<3> *result = csg->compute(left, right,
				                                  carve::csg::CSG::UNION,
				                                  NULL, carve::csg::CSG::CLASSIFY_EDGE);

				callback(result, other_poly, user_data);
				delete left;
				delete right;

				left = result;
			}
		}
		catch (carve::exception e) {
			std::cerr << "CSG failed, exception " << e.str() << std::endl;

			MeshSet<3> *result = meshSetFromTwoMeshes(&ctx,
			                                          left->meshes,
			                                          right->meshes);

			callback(result, other_poly, user_data);

			delete left;
			delete right;

			left = result;
		}
		catch (...) {
			delete left;
			delete right;

			throw "Unknown error in Carve library";
		}
	}

	for (RTreeCache::iterator it = rtree_cache.begin();
	     it != rtree_cache.end();
	     it++)
	{
		delete it->second;
	}

	// Append all meshes which doesn't have intersection with another operand as-is.
	if (orig_meshes.size()) {
		MeshSet<3> *result = meshSetFromTwoMeshes(&ctx,
		                                          left->meshes,
		                                          orig_meshes);

		delete left;
		left = result;
	}

	return left;
}

}  // namespace

// TODO(sergey): This function is to be totally re-implemented to make it
// more clear what's going on and hopefully optimize it as well.
void carve_unionIntersections(carve::csg::CSG *csg,
                              MeshSet<3> **left_r,
                              MeshSet<3> **right_r,
                              VertexAttrsCallback vertex_attr_callback,
                              UnionIntersectionsCallback callback,
                              void *user_data)
{
	MeshSet<3> *left = *left_r, *right = *right_r;

	if (left->meshes.size() == 1 && right->meshes.size() == 0) {
		return;
	}

	MeshSet<3>::aabb_t leftAABB = left->getAABB();
	MeshSet<3>::aabb_t rightAABB = right->getAABB();;

	left = unionIntersectingMeshes(csg, left, right, rightAABB,
	                               vertex_attr_callback, callback, user_data);
	right = unionIntersectingMeshes(csg, right, left, leftAABB,
	                                vertex_attr_callback, callback, user_data);

	if (left != *left_r) {
		delete *left_r;
	}

	if (right != *right_r) {
		delete *right_r;
	}

	*left_r = left;
	*right_r = right;
}

static inline void add_newell_cross_v3_v3v3(const Vector &v_prev,
                                            const Vector &v_curr,
                                            Vector *n)
{
	(*n)[0] += (v_prev[1] - v_curr[1]) * (v_prev[2] + v_curr[2]);
	(*n)[1] += (v_prev[2] - v_curr[2]) * (v_prev[0] + v_curr[0]);
	(*n)[2] += (v_prev[0] - v_curr[0]) * (v_prev[1] + v_curr[1]);
}

// Axis matrix is being set for non-flat ngons only.
bool carve_checkPolyPlanarAndGetNormal(const std::vector<MeshSet<3>::vertex_t> &vertex_storage,
                                       const int verts_per_poly,
                                       const int *verts_of_poly,
                                       Matrix3 *axis_matrix_r)
{
	if (verts_per_poly == 3) {
		// Triangles are always planar.
		return true;
	}
	else if (verts_per_poly == 4) {
		// Presumably faster than using generig n-gon check for quads.

		const Vector &v1 = vertex_storage[verts_of_poly[0]].v,
		             &v2 = vertex_storage[verts_of_poly[1]].v,
		             &v3 = vertex_storage[verts_of_poly[2]].v,
		             &v4 = vertex_storage[verts_of_poly[3]].v;

		Vector vec1, vec2, vec3, cross;

		vec1 = v2 - v1;
		vec2 = v4 - v1;
		vec3 = v3 - v1;

		cross = carve::geom::cross(vec1, vec2);

		double production = carve::geom::dot(cross, vec3);

		// TODO(sergey): Check on whether we could have length-independent
		// magnitude here.
		double magnitude = 1e-3 * cross.length2();

		return fabs(production) < magnitude;
	}
	else {
		const Vector *vert_prev = &vertex_storage[verts_of_poly[verts_per_poly - 1]].v;
		const Vector *vert_curr = &vertex_storage[verts_of_poly[0]].v;

		Vector normal = carve::geom::VECTOR(0.0, 0.0, 0.0);
		for (int i = 0; i < verts_per_poly; i++) {
			add_newell_cross_v3_v3v3(*vert_prev, *vert_curr, &normal);
			vert_prev = vert_curr;
			vert_curr = &vertex_storage[verts_of_poly[(i + 1) % verts_per_poly]].v;
		}

		if (normal.length2() < FLT_EPSILON) {
			// Degenerated face, couldn't triangulate properly anyway.
			return true;
		}
		else {
			double magnitude = normal.length2();

			normal.normalize();
			axis_dominant_v3_to_m3__bli(axis_matrix_r, normal);

			Vector first_projected = *axis_matrix_r * vertex_storage[verts_of_poly[0]].v;
			double min_z = first_projected[2], max_z = first_projected[2];

			for (int i = 1; i < verts_per_poly; i++) {
				const Vector &vertex = vertex_storage[verts_of_poly[i]].v;
				Vector projected = *axis_matrix_r * vertex;
				if (projected[2] < min_z) {
					min_z = projected[2];
				}
				if (projected[2] > max_z) {
					max_z = projected[2];
				}
			}

			if (std::abs(min_z - max_z) > FLT_EPSILON * magnitude) {
				return false;
			}
		}

		return true;
	}

	return false;
}

namespace {

int triangulateNGon_carveTriangulator(const std::vector<MeshSet<3>::vertex_t> &vertex_storage,
                                      const int verts_per_poly,
                                      const int *verts_of_poly,
                                      const Matrix3 &axis_matrix,
                                      std::vector<tri_idx> *triangles)
{
	// Project vertices to 2D plane.
	Vector projected;
	std::vector<carve::geom::vector<2> > poly_2d;
	poly_2d.reserve(verts_per_poly);
	for (int i = 0; i < verts_per_poly; ++i) {
		projected = axis_matrix * vertex_storage[verts_of_poly[i]].v;
		poly_2d.push_back(carve::geom::VECTOR(projected[0], projected[1]));
	}

	carve::triangulate::triangulate(poly_2d, *triangles);
	carve::triangulate::improve(poly_2d, *triangles);

	return triangles->size();
}

int triangulateNGon_importerTriangulator(struct ImportMeshData *import_data,
                                         CarveMeshImporter *mesh_importer,
                                         const std::vector<MeshSet<3>::vertex_t> &vertex_storage,
                                         const int verts_per_poly,
                                         const int *verts_of_poly,
                                         const Matrix3 &axis_matrix,
                                         std::vector<tri_idx> *triangles)
{
	typedef float Vector2D[2];
	typedef unsigned int Triangle[3];

	// Project vertices to 2D plane.
	Vector2D *poly_2d = new Vector2D[verts_per_poly];
	Vector projected;
	for (int i = 0; i < verts_per_poly; ++i) {
		projected = axis_matrix * vertex_storage[verts_of_poly[i]].v;
		poly_2d[i][0] = projected[0];
		poly_2d[i][1] = projected[1];
	}

	Triangle *api_triangles = new Triangle[verts_per_poly - 2];
	int num_triangles =
		mesh_importer->triangulate2DPoly(import_data,
		                                 poly_2d,
		                                 verts_per_poly,
		                                 api_triangles);

	triangles->reserve(num_triangles);
	for (int i = 0; i < num_triangles; ++i) {
		triangles->push_back(tri_idx(api_triangles[i][0],
			                         api_triangles[i][1],
			                         api_triangles[i][2]));
	}

	delete [] poly_2d;
	delete [] api_triangles;

	return num_triangles;
}

template <typename T>
void sortThreeNumbers(T &a, T &b, T &c)
{
	if (a > b)
		std::swap(a, b);
	if (b > c)
		std::swap(b, c);
	if (a > b)
		std::swap(a, b);
}

bool pushTriangle(int v1, int v2, int v3,
                  std::vector<int> *face_indices,
                  TrianglesStorage *triangles_storage)
{

	tri_idx triangle(v1, v2, v3);
	sortThreeNumbers(triangle.a, triangle.b, triangle.c);

	assert(triangle.a < triangle.b);
	assert(triangle.b < triangle.c);

	if (triangles_storage->find(triangle) == triangles_storage->end()) {
		face_indices->push_back(v1);
		face_indices->push_back(v2);
		face_indices->push_back(v3);

		triangles_storage->insert(triangle);
		return true;
	}
	else {
		return false;
	}
}

}  // namespace

int carve_triangulatePoly(struct ImportMeshData *import_data,
                          CarveMeshImporter *mesh_importer,
                          const std::vector<MeshSet<3>::vertex_t> &vertex_storage,
                          const int verts_per_poly,
                          const int *verts_of_poly,
                          const Matrix3 &axis_matrix,
                          std::vector<int> *face_indices,
                          TrianglesStorage *triangles_storage)
{
	int num_triangles = 0;

	assert(verts_per_poly > 3);

	if (verts_per_poly == 4) {
		// Quads we triangulate by 1-3 diagonal, it is an original behavior
		// of boolean modifier.
		//
		// TODO(sergey): Consider using shortest diagonal here. However
		// display code in Blende use static 1-3 split, so some experiments
		// are needed here.
		if (pushTriangle(verts_of_poly[0],
		                 verts_of_poly[1],
		                 verts_of_poly[2],
		                 face_indices,
		                 triangles_storage))
		{
			num_triangles++;
		}

		if (pushTriangle(verts_of_poly[0],
		                 verts_of_poly[2],
		                 verts_of_poly[3],
		                 face_indices,
		                 triangles_storage))
		{
			num_triangles++;
		}
	}
	else {
		std::vector<tri_idx> triangles;
		triangles.reserve(verts_per_poly - 2);

		// Make triangulator callback optional so we could do some tests
		// in the future.
		if (mesh_importer->triangulate2DPoly) {
			triangulateNGon_importerTriangulator(import_data,
			                                     mesh_importer,
			                                     vertex_storage,
			                                     verts_per_poly,
			                                     verts_of_poly,
			                                     axis_matrix,
			                                     &triangles);
		}
		else {
			triangulateNGon_carveTriangulator(vertex_storage,
			                                  verts_per_poly,
			                                  verts_of_poly,
			                                  axis_matrix,
			                                  &triangles);
		}

		for (int i = 0; i < triangles.size(); ++i) {
			int v1 = triangles[i].a,
			    v2 = triangles[i].b,
			    v3 = triangles[i].c;

			// Sanity check of the triangle.
			assert(v1 != v2);
			assert(v1 != v3);
			assert(v2 != v3);
			assert(v1 < verts_per_poly);
			assert(v2 < verts_per_poly);
			assert(v3 < verts_per_poly);

			if (pushTriangle(verts_of_poly[v1],
			                 verts_of_poly[v2],
			                 verts_of_poly[v3],
			                 face_indices,
			                 triangles_storage))
			{
				num_triangles++;
			}
		}
	}

	return num_triangles;
}
