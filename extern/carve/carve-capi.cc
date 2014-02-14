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

#include "carve-capi.h"
#include "carve-util.h"

#include <carve/interpolator.hpp>
#include <carve/rescale.hpp>

using carve::mesh::MeshSet;

typedef std::pair<int, int> OrigIndex;
typedef std::pair<MeshSet<3>::vertex_t *, MeshSet<3>::vertex_t *> VertexPair;
typedef carve::interpolate::VertexAttr<OrigIndex> OrigVertMapping;
typedef carve::interpolate::FaceAttr<OrigIndex> OrigFaceMapping;
typedef carve::interpolate::FaceEdgeAttr<OrigIndex> OrigFaceEdgeMapping;

typedef struct CarveMeshDescr {
	// Stores mesh data itself.
	MeshSet<3> *poly;

	// N-th element of the vector indicates index of an original mesh loop.
	std::vector<int> orig_loop_index_map;

	// N-th element of the vector indicates index of an original mesh poly.
	std::vector<int> orig_poly_index_map;

	// The folloving mapping is only filled in for output mesh.

	// Mapping from the face verts back to original vert index.
	OrigVertMapping orig_vert_mapping;

	// Mapping from the face edges back to (original edge index, original loop index).
	OrigFaceEdgeMapping orig_face_edge_mapping;

	// Mapping from the faces back to original poly index.
	OrigFaceMapping orig_face_mapping;
} CarveMeshDescr;

namespace {

template <typename T1, typename T2>
void edgeIndexMap_put(std::unordered_map<std::pair<T1, T1>, T2> *edge_map,
                      const T1 &v1,
                      const T1 &v2,
                      const T2 &index)
{
	if (v1 < v2) {
		(*edge_map)[std::make_pair(v1, v2)] = index;
	}
	else {
		(*edge_map)[std::make_pair(v2, v1)] = index;
	}
}

template <typename T1, typename T2>
const T2 &edgeIndexMap_get(const std::unordered_map<std::pair<T1, T1>, T2> &edge_map,
                           const T1 &v1,
                           const T1 &v2)
{
	typedef std::unordered_map<std::pair<T1, T1>, T2> Map;
	typename Map::const_iterator found;

	if (v1 < v2) {
		found = edge_map.find(std::make_pair(v1, v2));
	}
	else {
		found = edge_map.find(std::make_pair(v2, v1));
	}

	assert(found != edge_map.end());
	return found->second;
}

template <typename T1, typename T2>
bool edgeIndexMap_get_if_exists(const std::unordered_map<std::pair<T1, T1>, T2> &edge_map,
                                const T1 &v1,
                                const T1 &v2,
                                T2 *out)
{
	typedef std::unordered_map<std::pair<T1, T1>, T2> Map;
	typename Map::const_iterator found;

	if (v1 < v2) {
		found = edge_map.find(std::make_pair(v1, v2));
	}
	else {
		found = edge_map.find(std::make_pair(v2, v1));
	}

	if (found == edge_map.end()) {
		return false;
	}
	*out = found->second;
	return true;
}

template <typename T>
inline int indexOf(const T *element, const std::vector<T> &vector_from)
{
	return element - &vector_from.at(0);
}

void initOrigIndexMeshFaceMapping(CarveMeshDescr *mesh,
                                  int which_mesh,
                                  const std::vector<int> &orig_loop_index_map,
                                  const std::vector<int> &orig_poly_index_map,
                                  OrigVertMapping *orig_vert_mapping,
                                  OrigFaceEdgeMapping *orig_face_edge_mapping,
                                  OrigFaceMapping *orig_face_attr)
{
	MeshSet<3> *poly = mesh->poly;

	std::vector<MeshSet<3>::vertex_t>::iterator vertex_iter =
		poly->vertex_storage.begin();
	for (int i = 0;
	     vertex_iter != poly->vertex_storage.end();
	     ++i, ++vertex_iter)
	{
		MeshSet<3>::vertex_t *vertex = &(*vertex_iter);
		orig_vert_mapping->setAttribute(vertex,
		                                std::make_pair(which_mesh, i));
	}

	MeshSet<3>::face_iter face_iter = poly->faceBegin();
	for (int i = 0, loop_map_index = 0;
	     face_iter != poly->faceEnd();
	     ++face_iter, ++i)
	{
		const MeshSet<3>::face_t *face = *face_iter;

		// Mapping from carve face back to original poly index.
		int orig_poly_index = orig_poly_index_map[i];
		orig_face_attr->setAttribute(face, std::make_pair(which_mesh, orig_poly_index));

		for (MeshSet<3>::face_t::const_edge_iter_t edge_iter = face->begin();
		     edge_iter != face->end();
		     ++edge_iter, ++loop_map_index)
		{
			int orig_loop_index = orig_loop_index_map[loop_map_index];
			if (orig_loop_index != -1) {
				// Mapping from carve face edge back to original loop index.
				orig_face_edge_mapping->setAttribute(face,
				                                     edge_iter.idx(),
				                                     std::make_pair(which_mesh,
				                                                    orig_loop_index));
			}
		}
	}
}

void initOrigIndexMapping(CarveMeshDescr *left_mesh,
                          CarveMeshDescr *right_mesh,
                          OrigVertMapping *orig_vert_mapping,
                          OrigFaceEdgeMapping *orig_face_edge_mapping,
                          OrigFaceMapping *orig_face_mapping)
{
	initOrigIndexMeshFaceMapping(left_mesh,
	                             CARVE_MESH_LEFT,
	                             left_mesh->orig_loop_index_map,
	                             left_mesh->orig_poly_index_map,
	                             orig_vert_mapping,
	                             orig_face_edge_mapping,
	                             orig_face_mapping);

	initOrigIndexMeshFaceMapping(right_mesh,
	                             CARVE_MESH_RIGHT,
	                             right_mesh->orig_loop_index_map,
	                             right_mesh->orig_poly_index_map,
	                             orig_vert_mapping,
	                             orig_face_edge_mapping,
	                             orig_face_mapping);
}

}  // namespace

CarveMeshDescr *carve_addMesh(struct ImportMeshData *import_data,
                              CarveMeshImporter *mesh_importer)
{
#define MAX_STATIC_VERTS 64

	CarveMeshDescr *mesh_descr = new CarveMeshDescr;

	// Import verices from external mesh to Carve.
	int num_verts = mesh_importer->getNumVerts(import_data);
	std::vector<carve::geom3d::Vector> vertices;
	vertices.reserve(num_verts);
	for (int i = 0; i < num_verts; i++) {
		float position[3];
		mesh_importer->getVertCoord(import_data, i, position);
		vertices.push_back(carve::geom::VECTOR(position[0],
		                                       position[1],
		                                       position[2]));
	}

	// Import polys from external mesh to Carve.
	int verts_of_poly_static[MAX_STATIC_VERTS];
	int *verts_of_poly_dynamic = NULL;
	int verts_of_poly_dynamic_size = 0;

	int num_loops = mesh_importer->getNumLoops(import_data);
	int num_polys = mesh_importer->getNumPolys(import_data);
	int loop_index = 0;
	int num_tessellated_polys = 0;
	std::vector<int> face_indices;
	face_indices.reserve(num_loops);
	mesh_descr->orig_loop_index_map.reserve(num_polys);
	mesh_descr->orig_poly_index_map.reserve(num_polys);
	for (int i = 0; i < num_polys; i++) {
		int verts_per_poly =
			mesh_importer->getNumPolyVerts(import_data, i);
		int *verts_of_poly;

		if (verts_per_poly <= MAX_STATIC_VERTS) {
			verts_of_poly = verts_of_poly_static;
		}
		else {
			if (verts_of_poly_dynamic_size < verts_per_poly) {
				if (verts_of_poly_dynamic != NULL) {
					delete [] verts_of_poly_dynamic;
				}
				verts_of_poly_dynamic = new int[verts_per_poly];
				verts_of_poly_dynamic_size = verts_per_poly;
			}
			verts_of_poly = verts_of_poly_dynamic;
		}

		mesh_importer->getPolyVerts(import_data, i, verts_of_poly);

		carve::math::Matrix3 axis_matrix;
		if (!carve_checkPolyPlanarAndGetNormal(vertices,
		                                       verts_per_poly,
		                                       verts_of_poly,
		                                       &axis_matrix)) {
			int num_triangles;

			num_triangles = carve_triangulatePoly(import_data,
			                                      mesh_importer,
			                                      i,
			                                      loop_index,
			                                      vertices,
			                                      verts_per_poly,
			                                      verts_of_poly,
			                                      axis_matrix,
			                                      &face_indices,
			                                      &mesh_descr->orig_loop_index_map,
			                                      &mesh_descr->orig_poly_index_map);

			num_tessellated_polys += num_triangles;
		}
		else {
			face_indices.push_back(verts_per_poly);
			for (int j = 0; j < verts_per_poly; ++j) {
				mesh_descr->orig_loop_index_map.push_back(loop_index++);
				face_indices.push_back(verts_of_poly[j]);
			}
			mesh_descr->orig_poly_index_map.push_back(i);
			num_tessellated_polys++;
		}
	}

	if (verts_of_poly_dynamic != NULL) {
		delete [] verts_of_poly_dynamic;
	}

	mesh_descr->poly = new MeshSet<3> (vertices,
	                                   num_tessellated_polys,
	                                   face_indices);

	return mesh_descr;

#undef MAX_STATIC_VERTS
}

void carve_deleteMesh(CarveMeshDescr *mesh_descr)
{
	delete mesh_descr->poly;
	delete mesh_descr;
}

bool carve_performBooleanOperation(CarveMeshDescr *left_mesh,
                                   CarveMeshDescr *right_mesh,
                                   int operation,
                                   CarveMeshDescr **output_mesh)
{
	*output_mesh = NULL;

	carve::csg::CSG::OP op;
	switch (operation) {
#define OP_CONVERT(the_op) \
		case CARVE_OP_ ## the_op: \
			op = carve::csg::CSG::the_op; \
			break;
		OP_CONVERT(UNION)
		OP_CONVERT(INTERSECTION)
		OP_CONVERT(A_MINUS_B)
		default:
			return false;
#undef OP_CONVERT
	}

	CarveMeshDescr *output_descr = new CarveMeshDescr;
	output_descr->poly = NULL;
	try {
		MeshSet<3> *left = left_mesh->poly, *right = right_mesh->poly;
		carve::geom3d::Vector min, max;

		// TODO(sergey): Make importer/exporter to care about re-scale
		// to save extra mesh iteration here.
		carve_getRescaleMinMax(left, right, &min, &max);

		carve::rescale::rescale scaler(min.x, min.y, min.z, max.x, max.y, max.z);
		carve::rescale::fwd fwd_r(scaler);
		carve::rescale::rev rev_r(scaler);

		left->transform(fwd_r);
		right->transform(fwd_r);

		// Initialize attributes for maping from boolean result mesh back to
		// original geometry indices.
		initOrigIndexMapping(left_mesh, right_mesh,
		                     &output_descr->orig_vert_mapping,
		                     &output_descr->orig_face_edge_mapping,
		                     &output_descr->orig_face_mapping);

		carve::csg::CSG csg;

		output_descr->orig_vert_mapping.installHooks(csg);
		output_descr->orig_face_edge_mapping.installHooks(csg);
		output_descr->orig_face_mapping.installHooks(csg);

		// Prepare operands for actual boolean operation.
		//
		// It's needed because operands might consist of several intersecting
		// meshes and in case of another operands intersect an edge loop of
		// intersecting that meshes tessellation of operation result can't be
		// done properly. The only way to make such situations working is to
		// union intersecting meshes of the same operand.
		carve_unionIntersections(&csg, &left, &right);
		left_mesh->poly = left;
		right_mesh->poly = right;

		if (left->meshes.size() == 0 || right->meshes.size() == 0) {
			// Normally shouldn't happen (zero-faces objects are handled by
			// modifier itself), but unioning intersecting meshes which doesn't
			// have consistent normals might lead to empty result which
			// wouldn't work here.

			return false;
		}

		output_descr->poly = csg.compute(left,
		                                 right,
		                                 op,
		                                 NULL,
		                                 carve::csg::CSG::CLASSIFY_EDGE);
		if (output_descr->poly) {
			output_descr->poly->transform(rev_r);
		}
	}
	catch (carve::exception e) {
		std::cerr << "CSG failed, exception " << e.str() << std::endl;
	}
	catch (...) {
		std::cerr << "Unknown error in Carve library" << std::endl;
	}

	*output_mesh = output_descr;

	return output_descr->poly != NULL;
}

static void exportMesh_handle_edges_list(MeshSet<3> *poly,
                                         const std::vector<MeshSet<3>::edge_t*> &edges,
                                         int start_edge_index,
                                         CarveMeshExporter *mesh_exporter,
                                         struct ExportMeshData *export_data,
                                         const std::unordered_map<VertexPair, OrigIndex> &edge_origindex_map,
                                         std::unordered_map<VertexPair, int> *edge_map)
{
	for (int i = 0, edge_index = start_edge_index;
	     i < edges.size();
	     ++i, ++edge_index)
	{
		MeshSet<3>::edge_t *edge = edges.at(i);
		MeshSet<3>::vertex_t *v1 = edge->vert;
		MeshSet<3>::vertex_t *v2 = edge->next->vert;

		OrigIndex orig_edge_index;

		if (!edgeIndexMap_get_if_exists(edge_origindex_map,
		                                v1,
		                                v2,
		                                &orig_edge_index))
		{
			orig_edge_index.first = CARVE_MESH_NONE;
			orig_edge_index.second = -1;
		}

		mesh_exporter->setEdge(export_data,
		                       edge_index,
		                       indexOf(v1, poly->vertex_storage),
		                       indexOf(v2, poly->vertex_storage),
		                       orig_edge_index.first,
		                       orig_edge_index.second);

		edgeIndexMap_put(edge_map, v1, v2, edge_index);
	}
}

void carve_exportMesh(CarveMeshDescr *mesh_descr,
                      CarveMeshExporter *mesh_exporter,
                      struct ExportMeshData *export_data)
{
	OrigIndex origindex_none = std::make_pair((int)CARVE_MESH_NONE, -1);
	MeshSet<3> *poly = mesh_descr->poly;
	int num_vertices = poly->vertex_storage.size();
	int num_edges = 0, num_loops = 0, num_polys = 0;

	// Count edges from all manifolds.
	for (int i = 0; i < poly->meshes.size(); ++i) {
		carve::mesh::Mesh<3> *mesh = poly->meshes[i];
		num_edges += mesh->closed_edges.size() + mesh->open_edges.size();
	}

	// Count polys and loops from all manifolds.
	for (MeshSet<3>::face_iter face_iter = poly->faceBegin();
	     face_iter != poly->faceEnd();
	     ++face_iter, ++num_polys)
	{
		MeshSet<3>::face_t *face = *face_iter;
		num_loops += face->n_edges;
	}

	// Initialize arrays for geometry in exported mesh.
	mesh_exporter->initGeomArrays(export_data,
	                              num_vertices,
	                              num_edges,
	                              num_loops,
	                              num_polys);

	// Get mapping from edge denoted by vertex pair to original edge index,
	//
	// This is needed because internally Carve interpolates data for per-face
	// edges rather then having some global edge storage.
	std::unordered_map<VertexPair, OrigIndex> edge_origindex_map;
	for (MeshSet<3>::face_iter face_iter = poly->faceBegin();
	     face_iter != poly->faceEnd();
	     ++face_iter)
	{
		MeshSet<3>::face_t *face = *face_iter;
		for (MeshSet<3>::face_t::edge_iter_t edge_iter = face->begin();
		     edge_iter != face->end();
		     ++edge_iter)
		{
			MeshSet<3>::edge_t &edge = *edge_iter;
			int edge_iter_index = edge_iter.idx();

			const OrigIndex &orig_loop_index =
				mesh_descr->orig_face_edge_mapping.getAttribute(face,
				                                                edge_iter_index,
				                                                origindex_none);

			if (orig_loop_index.first != CARVE_MESH_NONE) {
				OrigIndex orig_edge_index;

				orig_edge_index.first = orig_loop_index.first;
				orig_edge_index.second =
					mesh_exporter->mapLoopToEdge(export_data,
					                             orig_loop_index.first,
					                             orig_loop_index.second);

				MeshSet<3>::vertex_t *v1 = edge.vert;
				MeshSet<3>::vertex_t *v2 = edge.next->vert;

				edgeIndexMap_put(&edge_origindex_map, v1, v2, orig_edge_index);
			}
		}
	}

	// Export all the vertices.
	std::vector<MeshSet<3>::vertex_t>::iterator vertex_iter = poly->vertex_storage.begin();
	for (int i = 0; vertex_iter != poly->vertex_storage.end(); ++i, ++vertex_iter) {
		MeshSet<3>::vertex_t *vertex = &(*vertex_iter);

		OrigIndex orig_vert_index =
			mesh_descr->orig_vert_mapping.getAttribute(vertex, origindex_none);

		float coord[3];
		coord[0] = vertex->v[0];
		coord[1] = vertex->v[1];
		coord[2] = vertex->v[2];
		mesh_exporter->setVert(export_data, i, coord,
		                       orig_vert_index.first,
		                       orig_vert_index.second);
	}

	// Export all the edges.
	std::unordered_map<VertexPair, int> edge_map;
	for (int i = 0, edge_index = 0; i < poly->meshes.size(); ++i) {
		carve::mesh::Mesh<3> *mesh = poly->meshes[i];
		// Export closed edges.
		exportMesh_handle_edges_list(poly,
		                             mesh->closed_edges,
		                             edge_index,
		                             mesh_exporter,
		                             export_data,
		                             edge_origindex_map,
		                             &edge_map);
		edge_index += mesh->closed_edges.size();

		// Export open edges.
		exportMesh_handle_edges_list(poly,
		                             mesh->open_edges,
		                             edge_index,
		                             mesh_exporter,
		                             export_data,
		                             edge_origindex_map,
		                             &edge_map);
		edge_index += mesh->open_edges.size();
	}

	// Export all the loops and polys.
	MeshSet<3>::face_iter face_iter = poly->faceBegin();
	for (int loop_index = 0, poly_index = 0;
	     face_iter != poly->faceEnd();
	     ++face_iter, ++poly_index)
	{
		int start_loop_index = loop_index;
		MeshSet<3>::face_t *face = *face_iter;
		const OrigIndex &orig_face_index =
			mesh_descr->orig_face_mapping.getAttribute(face, origindex_none);

		for (MeshSet<3>::face_t::edge_iter_t edge_iter = face->begin();
		     edge_iter != face->end();
		     ++edge_iter, ++loop_index)
		{
			MeshSet<3>::edge_t &edge = *edge_iter;
			const OrigIndex &orig_loop_index =
				mesh_descr->orig_face_edge_mapping.getAttribute(face,
				                                                edge_iter.idx(),
				                                                origindex_none);

			mesh_exporter->setLoop(export_data,
		                           loop_index,
		                           indexOf(edge.vert, poly->vertex_storage),
		                           edgeIndexMap_get(edge_map, edge.vert, edge.next->vert),
		                           orig_loop_index.first,
		                           orig_loop_index.second);
		}

		mesh_exporter->setPoly(export_data,
		                       poly_index, start_loop_index, face->n_edges,
		                       orig_face_index.first, orig_face_index.second);
	}
}
