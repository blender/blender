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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Nathan Letwory.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/MeshImporter.cpp
 *  \ingroup collada
 */


#include <algorithm>

#if !defined(WIN32) || defined(FREE_WINDOWS)
#include <iostream>
#endif

/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"

#include "COLLADAFWMeshPrimitive.h"
#include "COLLADAFWMeshVertexData.h"
#include "COLLADAFWPolygons.h"

extern "C" {
	#include "BKE_blender.h"
	#include "BKE_customdata.h"
	#include "BKE_displist.h"
	#include "BKE_global.h"
	#include "BKE_library.h"
	#include "BKE_main.h"
	#include "BKE_material.h"
	#include "BKE_mesh.h"
	#include "BKE_object.h"

	#include "BLI_listbase.h"
	#include "BLI_math.h"
	#include "BLI_string.h"
	#include "BLI_edgehash.h"

	#include "MEM_guardedalloc.h"
}

#include "ArmatureImporter.h"
#include "MeshImporter.h"
#include "collada_utils.h"

// get node name, or fall back to original id if not present (name is optional)
template<class T>
static const std::string bc_get_dae_name(T *node)
{
	return node->getName().size() ? node->getName(): node->getOriginalId();
}

static const char *bc_primTypeToStr(COLLADAFW::MeshPrimitive::PrimitiveType type)
{
	switch (type) {
		case COLLADAFW::MeshPrimitive::LINES:
			return "LINES";
		case COLLADAFW::MeshPrimitive::LINE_STRIPS:
			return "LINESTRIPS";
		case COLLADAFW::MeshPrimitive::POLYGONS:
			return "POLYGONS";
		case COLLADAFW::MeshPrimitive::POLYLIST:
			return "POLYLIST";
		case COLLADAFW::MeshPrimitive::TRIANGLES:
			return "TRIANGLES";
		case COLLADAFW::MeshPrimitive::TRIANGLE_FANS:
			return "TRIANGLE_FANS";
		case COLLADAFW::MeshPrimitive::TRIANGLE_STRIPS:
			return "TRIANGLE_FANS";
		case COLLADAFW::MeshPrimitive::POINTS:
			return "POINTS";
		case COLLADAFW::MeshPrimitive::UNDEFINED_PRIMITIVE_TYPE:
			return "UNDEFINED_PRIMITIVE_TYPE";
	}
	return "UNKNOWN";
}

static const char *bc_geomTypeToStr(COLLADAFW::Geometry::GeometryType type)
{
	switch (type) {
		case COLLADAFW::Geometry::GEO_TYPE_MESH:
			return "MESH";
		case COLLADAFW::Geometry::GEO_TYPE_SPLINE:
			return "SPLINE";
		case COLLADAFW::Geometry::GEO_TYPE_CONVEX_MESH:
			return "CONVEX_MESH";
		case COLLADAFW::Geometry::GEO_TYPE_UNKNOWN:
		default:
			return "UNKNOWN";
	}
}


UVDataWrapper::UVDataWrapper(COLLADAFW::MeshVertexData& vdata) : mVData(&vdata)
{
}

#ifdef COLLADA_DEBUG
void WVDataWrapper::print()
{
	fprintf(stderr, "UVs:\n");
	switch (mVData->getType()) {
		case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT:
		{
			COLLADAFW::ArrayPrimitiveType<float> *values = mVData->getFloatValues();
			if (values->getCount()) {
				for (int i = 0; i < values->getCount(); i += 2) {
					fprintf(stderr, "%.1f, %.1f\n", (*values)[i], (*values)[i + 1]);
				}
			}
		}
		break;
		case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE:
		{
			COLLADAFW::ArrayPrimitiveType<double> *values = mVData->getDoubleValues();
			if (values->getCount()) {
				for (int i = 0; i < values->getCount(); i += 2) {
					fprintf(stderr, "%.1f, %.1f\n", (float)(*values)[i], (float)(*values)[i + 1]);
				}
			}
		}
		break;
	}
	fprintf(stderr, "\n");
}
#endif

void UVDataWrapper::getUV(int uv_index, float *uv)
{
	int stride = mVData->getStride(0);
	if (stride == 0) stride = 2;

	switch (mVData->getType()) {
		case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT:
		{
			COLLADAFW::ArrayPrimitiveType<float> *values = mVData->getFloatValues();
			if (values->empty()) return;
			uv[0] = (*values)[uv_index * stride];
			uv[1] = (*values)[uv_index * stride + 1];
			
		}
		break;
		case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE:
		{
			COLLADAFW::ArrayPrimitiveType<double> *values = mVData->getDoubleValues();
			if (values->empty()) return;
			uv[0] = (float)(*values)[uv_index * stride];
			uv[1] = (float)(*values)[uv_index * stride + 1];
			
		}
		break;
		case COLLADAFW::MeshVertexData::DATA_TYPE_UNKNOWN:
		default:
			fprintf(stderr, "MeshImporter.getUV(): unknown data type\n");
	}
}

MeshImporter::MeshImporter(UnitConverter *unitconv, ArmatureImporter *arm, Scene *sce) : unitconverter(unitconv), scene(sce), armature_importer(arm) {
}

void MeshImporter::set_poly_indices(MPoly *mpoly, MLoop *mloop, int loop_index, unsigned int *indices, int loop_count)
{
	mpoly->loopstart = loop_index;
	mpoly->totloop   = loop_count;

	for (int index=0; index < loop_count; index++) {
		mloop->v = indices[index];
		mloop++;
	}
}

void MeshImporter::set_face_uv(MLoopUV *mloopuv, UVDataWrapper &uvs,
                               int start_index, COLLADAFW::IndexList& index_list, int count)
{
	// per face vertex indices, this means for quad we have 4 indices, not 8
	COLLADAFW::UIntValuesArray& indices = index_list.getIndices();

	for (int index = 0; index < count; index++) {
		int uv_index = indices[index+start_index];
		uvs.getUV(uv_index, mloopuv[index].uv);
	}
}

#ifdef COLLADA_DEBUG
void MeshImporter::print_index_list(COLLADAFW::IndexList& index_list)
{
	fprintf(stderr, "Index list for \"%s\":\n", index_list.getName().c_str());
	for (int i = 0; i < index_list.getIndicesCount(); i += 2) {
		fprintf(stderr, "%u, %u\n", index_list.getIndex(i), index_list.getIndex(i + 1));
	}
	fprintf(stderr, "\n");
}
#endif

bool MeshImporter::is_nice_mesh(COLLADAFW::Mesh *mesh)  // checks if mesh has supported primitive types: lines, polylist, triangles, triangle_fans
{
	COLLADAFW::MeshPrimitiveArray& prim_arr = mesh->getMeshPrimitives();

	const std::string &name = bc_get_dae_name(mesh);
	
	for (unsigned i = 0; i < prim_arr.getCount(); i++) {
		
		COLLADAFW::MeshPrimitive *mp = prim_arr[i];
		COLLADAFW::MeshPrimitive::PrimitiveType type = mp->getPrimitiveType();

		const char *type_str = bc_primTypeToStr(type);
		
		// OpenCollada passes POLYGONS type for <polylist>
		if (type == COLLADAFW::MeshPrimitive::POLYLIST || type == COLLADAFW::MeshPrimitive::POLYGONS) {

			COLLADAFW::Polygons *mpvc = (COLLADAFW::Polygons *)mp;
			COLLADAFW::Polygons::VertexCountArray& vca = mpvc->getGroupedVerticesVertexCountArray();
			
			for (unsigned int j = 0; j < vca.getCount(); j++) {
				int count = vca[j];
				if (count < 3) {
					fprintf(stderr, "Primitive %s in %s has at least one face with vertex count < 3\n",
					        type_str, name.c_str());
					return false;
				}
			}
				
		}

		else if (type == COLLADAFW::MeshPrimitive::LINES) {
			// TODO: Add Checker for line syntax here
		}

		else if (type != COLLADAFW::MeshPrimitive::TRIANGLES && type != COLLADAFW::MeshPrimitive::TRIANGLE_FANS) {
			fprintf(stderr, "Primitive type %s is not supported.\n", type_str);
			return false;
		}
	}
	
	if (mesh->getPositions().empty()) {
		fprintf(stderr, "Mesh %s has no vertices.\n", name.c_str());
		return false;
	}

	return true;
}

void MeshImporter::read_vertices(COLLADAFW::Mesh *mesh, Mesh *me)
{
	// vertices
	COLLADAFW::MeshVertexData& pos = mesh->getPositions();
	int stride = pos.getStride(0);
	if (stride == 0) stride = 3;
	
	me->totvert = mesh->getPositions().getFloatValues()->getCount() / stride;
	me->mvert = (MVert *)CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, me->totvert);

	MVert *mvert;
	int i;

	for (i = 0, mvert = me->mvert; i < me->totvert; i++, mvert++) {
		get_vector(mvert->co, pos, i, stride);
	}
}


// =====================================================================
// condition 1: The Primitive has normals
// condition 2: The number of normals equals the number of faces.
// return true if both conditions apply.
// return false otherwise.
// =====================================================================
bool MeshImporter::primitive_has_useable_normals(COLLADAFW::MeshPrimitive *mp) {

	bool has_useable_normals = false;

	int normals_count = mp->getNormalIndices().getCount();
	if (normals_count > 0) {
		int index_count   = mp->getPositionIndices().getCount();
		if (index_count == normals_count) 
			has_useable_normals = true;
		else {
			fprintf(stderr,
			        "Warning: Number of normals %d is different from the number of vertices %d, skipping normals\n",
			        normals_count, index_count);
		}
	}

	return has_useable_normals;

}

// =====================================================================
// Assume that only TRIANGLES, TRIANGLE_FANS, POLYLIST and POLYGONS
// have faces. (to be verified)
// =====================================================================
bool MeshImporter::primitive_has_faces(COLLADAFW::MeshPrimitive *mp) {

	bool has_faces = false;
	int type = mp->getPrimitiveType();
	switch (type) {
		case COLLADAFW::MeshPrimitive::TRIANGLES:
		case COLLADAFW::MeshPrimitive::TRIANGLE_FANS:
		case COLLADAFW::MeshPrimitive::POLYLIST:
		case COLLADAFW::MeshPrimitive::POLYGONS:
		{
			has_faces = true;
			break;
		}
		default: {
			has_faces = false; 
			break;
		}
	}
	return has_faces;
}

// =================================================================
// Return the number of faces by summing up
// the facecounts of the parts.
// hint: This is done because mesh->getFacesCount() does
// count loose edges as extra faces, which is not what we want here.
// =================================================================
void MeshImporter::allocate_poly_data(COLLADAFW::Mesh *collada_mesh, Mesh *me)
{
	COLLADAFW::MeshPrimitiveArray& prim_arr = collada_mesh->getMeshPrimitives();
	int total_poly_count = 0;
	int total_loop_count = 0;

	// collect edge_count and face_count from all parts
	for (int i = 0; i < prim_arr.getCount(); i++) {
		COLLADAFW::MeshPrimitive *mp = prim_arr[i];
		int type = mp->getPrimitiveType();
		switch (type) {
			case COLLADAFW::MeshPrimitive::TRIANGLES:
			case COLLADAFW::MeshPrimitive::TRIANGLE_FANS:
			case COLLADAFW::MeshPrimitive::POLYLIST:
			case COLLADAFW::MeshPrimitive::POLYGONS:
			{
				COLLADAFW::Polygons *mpvc = (COLLADAFW::Polygons *)mp;
				size_t prim_poly_count    = mpvc->getFaceCount();

				size_t prim_loop_count    = 0;
				for (int index=0; index < prim_poly_count; index++) {
					prim_loop_count += get_vertex_count(mpvc, index);
				}

				total_poly_count += prim_poly_count;
				total_loop_count += prim_loop_count;
				break;
			}
			default:
				break;
		}
	}

	// Add the data containers
	if (total_poly_count > 0) {
		me->totpoly = total_poly_count;
		me->totloop = total_loop_count;
		me->mpoly   = (MPoly *)CustomData_add_layer(&me->pdata, CD_MPOLY, CD_CALLOC, NULL, me->totpoly);
		me->mloop   = (MLoop *)CustomData_add_layer(&me->ldata, CD_MLOOP, CD_CALLOC, NULL, me->totloop);

		unsigned int totuvset = collada_mesh->getUVCoords().getInputInfosArray().getCount();
		for (int i = 0; i < totuvset; i++) {
			if (collada_mesh->getUVCoords().getLength(i) == 0) {
				totuvset = 0;
				break;
			}
		}

		if (totuvset > 0) {
			for (int i = 0; i < totuvset; i++) {
				COLLADAFW::MeshVertexData::InputInfos *info = collada_mesh->getUVCoords().getInputInfosArray()[i];
				COLLADAFW::String &uvname = info->mName;
				// Allocate space for UV_data
				CustomData_add_layer_named(&me->pdata, CD_MTEXPOLY, CD_DEFAULT, NULL, me->totpoly, uvname.c_str());
				CustomData_add_layer_named(&me->ldata, CD_MLOOPUV, CD_DEFAULT, NULL, me->totloop, uvname.c_str());
			}
			// activate the first uv map
			me->mtpoly  = (MTexPoly *)CustomData_get_layer_n(&me->pdata, CD_MTEXPOLY, 0);
			me->mloopuv = (MLoopUV *) CustomData_get_layer_n(&me->ldata, CD_MLOOPUV, 0);
		}
	}
}

unsigned int MeshImporter::get_vertex_count(COLLADAFW::Polygons *mp, int index) {
	int type = mp->getPrimitiveType();
	int result;
	switch (type) {
		case COLLADAFW::MeshPrimitive::TRIANGLES:
		case COLLADAFW::MeshPrimitive::TRIANGLE_FANS:
		{
			result = 3;
			break;
		}
		case COLLADAFW::MeshPrimitive::POLYLIST:
		case COLLADAFW::MeshPrimitive::POLYGONS:
		{
			result = mp->getGroupedVerticesVertexCountArray()[index];
			break;
		}
		default:
		{
			result = -1;
			break;
		}
	}
	return result;
}


unsigned int MeshImporter::get_loose_edge_count(COLLADAFW::Mesh *mesh) {
	COLLADAFW::MeshPrimitiveArray& prim_arr = mesh->getMeshPrimitives();
	int loose_edge_count = 0;

	// collect edge_count and face_count from all parts
	for (int i = 0; i < prim_arr.getCount(); i++) {
		COLLADAFW::MeshPrimitive *mp = prim_arr[i];
		int type = mp->getPrimitiveType();
		switch (type) {
			case COLLADAFW::MeshPrimitive::LINES:
			{
				size_t prim_totface = mp->getFaceCount();
				loose_edge_count += prim_totface;
				break;
			}
			default:
				break;
		}
	}
	return loose_edge_count;
}

// =================================================================
// This functin is copied from source/blender/editors/mesh/mesh_data.c
//
// TODO: (As discussed with sergey-) :
// Maybe move this function to blenderkernel/intern/mesh.c 
// and add definition to BKE_mesh.c
// =================================================================
void MeshImporter::mesh_add_edges(Mesh *mesh, int len)
{
	CustomData edata;
	MEdge *medge;
	int i, totedge;

	if (len == 0)
		return;

	totedge = mesh->totedge + len;

	/* update customdata  */
	CustomData_copy(&mesh->edata, &edata, CD_MASK_MESH, CD_DEFAULT, totedge);
	CustomData_copy_data(&mesh->edata, &edata, 0, 0, mesh->totedge);

	if (!CustomData_has_layer(&edata, CD_MEDGE))
		CustomData_add_layer(&edata, CD_MEDGE, CD_CALLOC, NULL, totedge);

	CustomData_free(&mesh->edata, mesh->totedge);
	mesh->edata = edata;
	BKE_mesh_update_customdata_pointers(mesh, false); /* new edges don't change tessellation */

	/* set default flags */
	medge = &mesh->medge[mesh->totedge];
	for (i = 0; i < len; i++, medge++)
		medge->flag = ME_EDGEDRAW | ME_EDGERENDER | SELECT;

	mesh->totedge = totedge;
}

// =================================================================
// Read all loose edges.
// Important: This function assumes that all edges from existing 
// faces have allready been generated and added to me->medge
// So this function MUST be called after read_faces() (see below)
// =================================================================
void MeshImporter::read_lines(COLLADAFW::Mesh *mesh, Mesh *me)
{
	unsigned int loose_edge_count = get_loose_edge_count(mesh);
	if (loose_edge_count > 0) {

		unsigned int face_edge_count  = me->totedge;
		/* unsigned int total_edge_count = loose_edge_count + face_edge_count; */ /* UNUSED */
		
		mesh_add_edges(me, loose_edge_count);
		MEdge *med = me->medge + face_edge_count;

		COLLADAFW::MeshPrimitiveArray& prim_arr = mesh->getMeshPrimitives();

		for (int i = 0; i < prim_arr.getCount(); i++) {
			
			COLLADAFW::MeshPrimitive *mp = prim_arr[i];

			int type = mp->getPrimitiveType();
			if (type == COLLADAFW::MeshPrimitive::LINES) {
				unsigned int edge_count  = mp->getFaceCount();
				unsigned int *indices    = mp->getPositionIndices().getData();
				
				for (int i = 0; i < edge_count; i++, med++) {
					med->bweight = 0;
					med->crease  = 0;
					med->flag   |= ME_LOOSEEDGE;
					med->v1      = indices[2 * i];
					med->v2      = indices[2 * i + 1];
				}
			}
		}

	}
}


// =======================================================================
// Read all faces from TRIANGLES, TRIANGLE_FANS, POLYLIST, POLYGON
// Important: This function MUST be called before read_lines() 
// Otherwise we will loose all edges from faces (see read_lines() above)
//
// TODO: import uv set names
// ========================================================================
void MeshImporter::read_polys(COLLADAFW::Mesh *collada_mesh, Mesh *me)
{
	unsigned int i;
	
	allocate_poly_data(collada_mesh, me);

	UVDataWrapper uvs(collada_mesh->getUVCoords());

	MPoly *mpoly = me->mpoly;
	MLoop *mloop = me->mloop;
	int loop_index = 0;

	MaterialIdPrimitiveArrayMap mat_prim_map;

	COLLADAFW::MeshPrimitiveArray& prim_arr = collada_mesh->getMeshPrimitives();
	COLLADAFW::MeshVertexData& nor = collada_mesh->getNormals();

	for (i = 0; i < prim_arr.getCount(); i++) {
		
		COLLADAFW::MeshPrimitive *mp = prim_arr[i];

		// faces
		size_t prim_totpoly            = mp->getFaceCount();
		unsigned int *position_indices = mp->getPositionIndices().getData();
		unsigned int *normal_indices   = mp->getNormalIndices().getData();

		bool mp_has_normals = primitive_has_useable_normals(mp);
		bool mp_has_faces   = primitive_has_faces(mp);

		int collada_meshtype = mp->getPrimitiveType();
		
		// since we cannot set mpoly->mat_nr here, we store a portion of me->mpoly in Primitive
		Primitive prim = {mpoly, 0};
		COLLADAFW::IndexListArray& index_list_array = mp->getUVCoordIndicesArray();

		// If MeshPrimitive is TRIANGLE_FANS we split it into triangles
		// The first trifan vertex will be the first vertex in every triangle
		// XXX The proper function of TRIANGLE_FANS is not tested!!!
		// XXX In particular the handling of the normal_indices looks very wrong to me
		if (collada_meshtype == COLLADAFW::MeshPrimitive::TRIANGLE_FANS) {
			unsigned grouped_vertex_count = mp->getGroupedVertexElementsCount();
			for (unsigned int group_index = 0; group_index < grouped_vertex_count; group_index++) {
				unsigned int first_vertex = position_indices[0]; // Store first trifan vertex
				unsigned int first_normal = normal_indices[0]; // Store first trifan vertex normal
				unsigned int vertex_count = mp->getGroupedVerticesVertexCount(group_index);

				for (unsigned int vertex_index = 0; vertex_index < vertex_count - 2; vertex_index++) {
					// For each triangle store indeces of its 3 vertices
					unsigned int triangle_vertex_indices[3] = {first_vertex, position_indices[1], position_indices[2]};
					set_poly_indices(mpoly, mloop, loop_index, triangle_vertex_indices, 3);

					if (mp_has_normals) {  // vertex normals, same inplementation as for the triangles
						// the same for vertces normals
						unsigned int vertex_normal_indices[3] = {first_normal, normal_indices[1], normal_indices[2]};
						if (!is_flat_face(vertex_normal_indices, nor, 3))
							mpoly->flag |= ME_SMOOTH;
						normal_indices++;
					}
				
					mpoly++;
					mloop += 3;
					loop_index += 3;
					prim.totpoly++;

				}

				// Moving cursor  to the next triangle fan.
				if (mp_has_normals)
					normal_indices += 2;

				position_indices +=  2;
			}
		}

		if (collada_meshtype == COLLADAFW::MeshPrimitive::POLYLIST ||
			collada_meshtype == COLLADAFW::MeshPrimitive::POLYGONS ||
			collada_meshtype == COLLADAFW::MeshPrimitive::TRIANGLES) {
			COLLADAFW::Polygons *mpvc = (COLLADAFW::Polygons *)mp;
			unsigned int start_index = 0;

			for (unsigned int j = 0; j < prim_totpoly; j++) {
				
				// Vertices in polygon:
				int vcount = get_vertex_count(mpvc, j);
				set_poly_indices(mpoly, mloop, loop_index, position_indices, vcount);


				for (unsigned int uvset_index = 0; uvset_index < index_list_array.getCount(); uvset_index++) {
					// get mtface by face index and uv set index
					COLLADAFW::IndexList& index_list = *index_list_array[uvset_index];
					MLoopUV  *mloopuv = (MLoopUV  *)CustomData_get_layer_named(&me->ldata, CD_MLOOPUV, index_list.getName().c_str());
					if (mloopuv == NULL) {
						fprintf(stderr, "Collada import: Mesh [%s] : Unknown reference to TEXCOORD [#%s].", me->id.name, index_list.getName().c_str() );
					}
					else {
						set_face_uv(mloopuv+loop_index, uvs, start_index, *index_list_array[uvset_index], vcount);
					}
				}

				if (mp_has_normals) {
					if (!is_flat_face(normal_indices, nor, vcount))
						mpoly->flag |= ME_SMOOTH;
				}
				
				mpoly++;
				mloop += vcount;
				loop_index += vcount;
				start_index += vcount;
				prim.totpoly++;

				if (mp_has_normals)
					normal_indices += vcount;

				position_indices += vcount;
			}
		}

		else if (collada_meshtype == COLLADAFW::MeshPrimitive::LINES) {
			continue; // read the lines later after all the rest is done
		}

		if (mp_has_faces)
			mat_prim_map[mp->getMaterialId()].push_back(prim);
	}

	geom_uid_mat_mapping_map[collada_mesh->getUniqueId()] = mat_prim_map;
}

void MeshImporter::get_vector(float v[3], COLLADAFW::MeshVertexData& arr, int i, int stride)
{
	i *= stride;
	
	switch (arr.getType()) {
		case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT:
		{
			COLLADAFW::ArrayPrimitiveType<float> *values = arr.getFloatValues();
			if (values->empty()) return;

			v[0] = (*values)[i++];
			v[1] = (*values)[i++];
			if (stride>=3) {
				v[2] = (*values)[i];
			}
			else {
				v[2] = 0.0f;
			}

		}
		break;
		case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE:
		{
			COLLADAFW::ArrayPrimitiveType<double> *values = arr.getDoubleValues();
			if (values->empty()) return;

			v[0] = (float)(*values)[i++];
			v[1] = (float)(*values)[i++];
			if (stride >= 3) {
				v[2] = (float)(*values)[i];
			}
			else {
				v[2] = 0.0f;
			}
		}
		break;
		default:
			break;
	}
}

bool MeshImporter::is_flat_face(unsigned int *nind, COLLADAFW::MeshVertexData& nor, int count)
{
	float a[3], b[3];

	get_vector(a, nor, *nind, 3);
	normalize_v3(a);

	nind++;

	for (int i = 1; i < count; i++, nind++) {
		get_vector(b, nor, *nind, 3);
		normalize_v3(b);

		float dp = dot_v3v3(a, b);

		if (dp < 0.99999f || dp > 1.00001f)
			return false;
	}

	return true;
}


void MeshImporter::bmeshConversion()
{
	for (std::map<COLLADAFW::UniqueId, Mesh *>::iterator m = uid_mesh_map.begin();
	     m != uid_mesh_map.end(); ++m)
	{
		if ((*m).second) {
			Mesh *me = (*m).second;
			BKE_mesh_tessface_clear(me);
			BKE_mesh_calc_normals(me);
			//BKE_mesh_validate(me, 1);
		}
	}
}


Object *MeshImporter::get_object_by_geom_uid(const COLLADAFW::UniqueId& geom_uid)
{
	if (uid_object_map.find(geom_uid) != uid_object_map.end())
		return uid_object_map[geom_uid];
	return NULL;
}

Mesh *MeshImporter::get_mesh_by_geom_uid(const COLLADAFW::UniqueId& mesh_uid)
{
	if (uid_mesh_map.find(mesh_uid) != uid_mesh_map.end())
		return uid_mesh_map[mesh_uid];
	return NULL;
}

std::string *MeshImporter::get_geometry_name(const std::string &mesh_name)
{
	if (this->mesh_geom_map.find(mesh_name) != this->mesh_geom_map.end())
		return &this->mesh_geom_map[mesh_name];
	return NULL;
}

MTex *MeshImporter::assign_textures_to_uvlayer(COLLADAFW::TextureCoordinateBinding &ctexture,
                                               Mesh *me, TexIndexTextureArrayMap& texindex_texarray_map,
                                               MTex *color_texture)
{
	const COLLADAFW::TextureMapId texture_index = ctexture.getTextureMapId();
	size_t setindex = ctexture.getSetIndex();
	std::string uvname = ctexture.getSemantic();
	
	if (setindex == -1) return NULL;
	
	const CustomData *data = &me->fdata;
	int layer_index = CustomData_get_layer_index(data, CD_MTFACE);

	if (layer_index == -1) return NULL;

	CustomDataLayer *cdl = &data->layers[layer_index + setindex];
	
	/* set uvname to bind_vertex_input semantic */
	BLI_strncpy(cdl->name, uvname.c_str(), sizeof(cdl->name));

	if (texindex_texarray_map.find(texture_index) == texindex_texarray_map.end()) {
		
		fprintf(stderr, "Cannot find texture array by texture index.\n");
		return color_texture;
	}
	
	std::vector<MTex *> textures = texindex_texarray_map[texture_index];
	
	std::vector<MTex *>::iterator it;
	
	for (it = textures.begin(); it != textures.end(); it++) {
		
		MTex *texture = *it;
		
		if (texture) {
			BLI_strncpy(texture->uvname, uvname.c_str(), sizeof(texture->uvname));
			if (texture->mapto == MAP_COL) color_texture = texture;
		}
	}
	return color_texture;
}

/**
 * this function checks if both objects have the same
 * materials assigned to Object (in the same order)
 * returns true if condition matches, otherwise false;
 **/
static bool bc_has_same_material_configuration(Object *ob1, Object *ob2)
{
	if (ob1->totcol != ob2->totcol) return false; // not same number of materials
	if (ob1->totcol == 0) return false; // no material at all
	
	for (int index=0; index < ob1->totcol; index++) {
		if (ob1->matbits[index] != ob2->matbits[index]) return false; // shouldn't happen
		if (ob1->matbits[index] == 0) return false; // shouldn't happen
		if (ob1->mat[index] != ob2->mat[index]) return false; // different material assignment
	}
	return true;
}


/**
 *
 * Caution here: This code assumes tha all materials are assigned to Object
 * and no material is assigned to Data.
 * That is true right after the objects have been imported.
 *
 **/
static void bc_copy_materials_to_data(Object *ob, Mesh *me)
{
	for (int index = 0; index < ob->totcol; index++) {
		ob->matbits[index] = 0;
		me->mat[index] = ob->mat[index];
	}
}

/**
 *
 * Remove all references to materials from the object
 *
 **/
static void bc_remove_materials_from_object(Object *ob, Mesh *me)
{
	for (int index = 0; index < ob->totcol; index++) {
		ob->matbits[index] = 0;
		ob->mat[index] = NULL;
	}
}

/**
 * Returns the list of Users of the given Mesh object.
 * Note: This function uses the object user flag to control
 * which objects have already been processed.
 **/
std::vector<Object *> MeshImporter::get_all_users_of(Mesh *reference_mesh)
{
	std::vector<Object *> mesh_users;
	for (std::vector<Object *>::iterator it = imported_objects.begin();
	     it != imported_objects.end(); ++it)
	{
		Object *ob = (*it);
		if (bc_is_marked(ob)) {
			bc_remove_mark(ob);
			Mesh *me = (Mesh *) ob->data;
			if (me == reference_mesh)
				mesh_users.push_back(ob);
		}
	}
	return mesh_users;
}

/**
 *
 * During import all materials have been assigned to Object.
 * Now we iterate over the imported objects and optimize
 * the assignments as follows:
 *
 * for each imported geometry:
 *     if number of users is 1:
 *         get the user (object)
 *         move the materials from Object to Data
 *     else:
 *         determine which materials are assigned to the first user
 *         check if all other users have the same materials in the same order
 *         if the check is positive:
 *             Add the materials of the first user to the geometry
 *             adjust all other users accordingly.
 *
 **/
void MeshImporter::optimize_material_assignements()
{
	for (std::vector<Object *>::iterator it = imported_objects.begin();
	     it != imported_objects.end(); ++it)
	{
		Object *ob = (*it);
		Mesh *me = (Mesh *) ob->data;
		if (me->id.us==1) {
			bc_copy_materials_to_data(ob, me);
			bc_remove_materials_from_object(ob, me);
			bc_remove_mark(ob);
		}
		else if (me->id.us > 1)
		{
			bool can_move = true;
			std::vector<Object *> mesh_users = get_all_users_of(me);
			if (mesh_users.size() > 1)
			{
				Object *ref_ob = mesh_users[0];
				for (int index = 1; index < mesh_users.size(); index++) {
					if (!bc_has_same_material_configuration(ref_ob, mesh_users[index])) {
						can_move = false;
						break;
					}
				}
				if (can_move) {
					bc_copy_materials_to_data(ref_ob, me);
					for (int index = 0; index < mesh_users.size(); index++) {
						Object *object = mesh_users[index];
						bc_remove_materials_from_object(object, me);
						bc_remove_mark(object);
					}
				}
			}
		}
	}
}

/**
 * We do not know in advance which objects will share geometries.
 * And we do not know either if the objects which share geometries
 * come along with different materials. So we first create the objects
 * and assign the materials to Object, then in a later cleanup we decide
 * which materials shall be moved to the created geometries. Also see
 * optimize_material_assignements() above.
 */
MTFace *MeshImporter::assign_material_to_geom(COLLADAFW::MaterialBinding cmaterial,
                                              std::map<COLLADAFW::UniqueId, Material *>& uid_material_map,
                                              Object *ob, const COLLADAFW::UniqueId *geom_uid,
                                              char *layername, MTFace *texture_face,
                                              std::map<Material *, TexIndexTextureArrayMap>& material_texture_mapping_map, short mat_index)
{
	MTex *color_texture = NULL;
	Mesh *me = (Mesh *)ob->data;
	const COLLADAFW::UniqueId& ma_uid = cmaterial.getReferencedMaterial();
	
	// do we know this material?
	if (uid_material_map.find(ma_uid) == uid_material_map.end()) {
		
		fprintf(stderr, "Cannot find material by UID.\n");
		return NULL;
	}

	// first time we get geom_uid, ma_uid pair. Save for later check.
	materials_mapped_to_geom.insert(std::pair<COLLADAFW::UniqueId, COLLADAFW::UniqueId>(*geom_uid, ma_uid));
	
	Material *ma = uid_material_map[ma_uid];

	// Attention! This temporaly assigns material to object on purpose!
	// See note above.
	ob->actcol=0;
	assign_material(ob, ma, mat_index + 1, BKE_MAT_ASSIGN_OBJECT); 
	
	COLLADAFW::TextureCoordinateBindingArray& tex_array = 
	    cmaterial.getTextureCoordinateBindingArray();
	TexIndexTextureArrayMap texindex_texarray_map = material_texture_mapping_map[ma];
	unsigned int i;
	// loop through <bind_vertex_inputs>
	for (i = 0; i < tex_array.getCount(); i++) {
		
		color_texture = assign_textures_to_uvlayer(tex_array[i], me, texindex_texarray_map,
		                                            color_texture);
	}
	
	// set texture face
	if (color_texture &&
	    strlen((color_texture)->uvname) &&
	    strcmp(layername, color_texture->uvname) != 0) {
		texture_face = (MTFace *)CustomData_get_layer_named(&me->fdata, CD_MTFACE,
		                                                    color_texture->uvname);
		strcpy(layername, color_texture->uvname);
	}
	
	MaterialIdPrimitiveArrayMap& mat_prim_map = geom_uid_mat_mapping_map[*geom_uid];
	COLLADAFW::MaterialId mat_id = cmaterial.getMaterialId();
	
	// assign material indices to mesh faces
	if (mat_prim_map.find(mat_id) != mat_prim_map.end()) {
		
		std::vector<Primitive>& prims = mat_prim_map[mat_id];
		
		std::vector<Primitive>::iterator it;
		
		for (it = prims.begin(); it != prims.end(); it++) {
			Primitive& prim = *it;
			MPoly *mpoly = prim.mpoly;

			for (i = 0; i < prim.totpoly; i++, mpoly++) {
				mpoly->mat_nr = mat_index;
				// bind texture images to faces
				if (texture_face && color_texture) {
					texture_face->tpage = (Image *)color_texture->tex->ima;
					texture_face++;
				}
			}
		}
	}	
	return texture_face;
}

Object *MeshImporter::create_mesh_object(COLLADAFW::Node *node, COLLADAFW::InstanceGeometry *geom,
                                         bool isController,
                                         std::map<COLLADAFW::UniqueId, Material *>& uid_material_map,
                                         std::map<Material *, TexIndexTextureArrayMap>& material_texture_mapping_map)
{
	const COLLADAFW::UniqueId *geom_uid = &geom->getInstanciatedObjectId();
	
	// check if node instanciates controller or geometry
	if (isController) {
		
		geom_uid = armature_importer->get_geometry_uid(*geom_uid);
		
		if (!geom_uid) {
			fprintf(stderr, "Couldn't find a mesh UID by controller's UID.\n");
			return NULL;
		}
	}
	else {
		
		if (uid_mesh_map.find(*geom_uid) == uid_mesh_map.end()) {
			// this could happen if a mesh was not created
			// (e.g. if it contains unsupported geometry)
			fprintf(stderr, "Couldn't find a mesh by UID.\n");
			return NULL;
		}
	}
	if (!uid_mesh_map[*geom_uid]) return NULL;
	
	// name Object
	const std::string& id = node->getName().size() ? node->getName() : node->getOriginalId();
	const char *name = (id.length()) ? id.c_str() : NULL;
	
	// add object
	Object *ob = bc_add_object(scene, OB_MESH, name);
	bc_set_mark(ob); // used later for material assignement optimization


	// store object pointer for ArmatureImporter
	uid_object_map[*geom_uid] = ob;
	imported_objects.push_back(ob);
	
	// replace ob->data freeing the old one
	Mesh *old_mesh = (Mesh *)ob->data;
	Mesh *new_mesh = uid_mesh_map[*geom_uid];

	BKE_mesh_assign_object(ob, new_mesh);
	BKE_mesh_calc_normals(new_mesh);

	if (old_mesh->id.us == 0) BKE_libblock_free(G.main, old_mesh);
	
	char layername[100];
	layername[0] = '\0';
	MTFace *texture_face = NULL;
	
	COLLADAFW::MaterialBindingArray& mat_array =
	    geom->getMaterialBindings();
	
	// loop through geom's materials
	for (unsigned int i = 0; i < mat_array.getCount(); i++) {
		
		if (mat_array[i].getReferencedMaterial().isValid()) {
			texture_face = assign_material_to_geom(mat_array[i], uid_material_map, ob, geom_uid,
			                                       layername, texture_face,
			                                       material_texture_mapping_map, i);
		}
		else {
			fprintf(stderr, "invalid referenced material for %s\n", mat_array[i].getName().c_str());
		}
	}

	return ob;
}

// create a mesh storing a pointer in a map so it can be retrieved later by geometry UID
bool MeshImporter::write_geometry(const COLLADAFW::Geometry *geom)
{

	if (geom->getType() != COLLADAFW::Geometry::GEO_TYPE_MESH) {
		// TODO: report warning
		fprintf(stderr, "Mesh type %s is not supported\n", bc_geomTypeToStr(geom->getType()));
		return true;
	}
	
	COLLADAFW::Mesh *mesh = (COLLADAFW::Mesh *)geom;
	
	if (!is_nice_mesh(mesh)) {
		fprintf(stderr, "Ignoring mesh %s\n", bc_get_dae_name(mesh).c_str());
		return true;
	}
	
	const std::string& str_geom_id = mesh->getName().size() ? mesh->getName() : mesh->getOriginalId();
	Mesh *me = BKE_mesh_add(G.main, (char *)str_geom_id.c_str());
	me->id.us--; // is already 1 here, but will be set later in BKE_mesh_assign_object

	// store the Mesh pointer to link it later with an Object
	// mesh_geom_map needed to map mesh to its geometry name (for shape key naming)
	this->uid_mesh_map[mesh->getUniqueId()] = me;
	this->mesh_geom_map[std::string(me->id.name)] = str_geom_id;
	
	read_vertices(mesh, me);
	read_polys(mesh, me);
	BKE_mesh_calc_edges(me, false, false);

	// read_lines() must be called after the face edges have been generated.
	// Oterwise the loose edges will be silently deleted again.
	read_lines(mesh, me);
	return true;
}
