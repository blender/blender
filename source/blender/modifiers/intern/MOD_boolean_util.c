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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/modifiers/intern/MOD_boolean_util.c
 *  \ingroup modifiers
 */

#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_polyfill2d.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_material.h"

#include "MOD_boolean_util.h"

#include "carve-capi.h"

/* Adopted from BM_loop_interp_from_face(),
 *
 * Transform matrix is used in cases when target coordinate needs
 * to be converted to source space (namely when interpolating
 * boolean result loops from second operand).
 *
 * TODO(sergey): Consider making it a generic function in DerivedMesh.c.
 */
static void DM_loop_interp_from_poly(DerivedMesh *source_dm,
                                     MVert *source_mverts,
                                     MLoop *source_mloops,
                                     MPoly *source_poly,
                                     DerivedMesh *target_dm,
                                     MVert *target_mverts,
                                     MLoop *target_mloop,
                                     float transform[4][4],
                                     int target_loop_index)
{
	float (*cos_3d)[3] = BLI_array_alloca(cos_3d, source_poly->totloop);
	int *source_indices = BLI_array_alloca(source_indices, source_poly->totloop);
	float *weights = BLI_array_alloca(weights, source_poly->totloop);
	int i;
	int target_vert_index = target_mloop[target_loop_index].v;
	float coord[3];

	for (i = 0; i < source_poly->totloop; ++i) {
		MLoop *mloop = &source_mloops[source_poly->loopstart + i];
		source_indices[i] = source_poly->loopstart + i;
		copy_v3_v3(cos_3d[i], source_mverts[mloop->v].co);
	}

	if (transform) {
		mul_v3_m4v3(coord, transform, target_mverts[target_vert_index].co);
	}
	else {
		copy_v3_v3(coord, target_mverts[target_vert_index].co);
	}

	interp_weights_poly_v3(weights, cos_3d, source_poly->totloop, coord);

	DM_interp_loop_data(source_dm, target_dm, source_indices, weights,
	                    source_poly->totloop, target_loop_index);
}

/* **** Importer from derived mesh to Carve ****  */

typedef struct ImportMeshData {
	DerivedMesh *dm;
	float obmat[4][4];
	MVert *mvert;
	MEdge *medge;
	MLoop *mloop;
	MPoly *mpoly;
} ImportMeshData;

/* Get number of vertices. */
static int importer_GetNumVerts(ImportMeshData *import_data)
{
	DerivedMesh *dm = import_data->dm;
	return dm->getNumVerts(dm);
}

/* Get number of edges. */
static int importer_GetNumEdges(ImportMeshData *import_data)
{
	DerivedMesh *dm = import_data->dm;
	return dm->getNumEdges(dm);
}

/* Get number of loops. */
static int importer_GetNumLoops(ImportMeshData *import_data)
{
	DerivedMesh *dm = import_data->dm;
	return dm->getNumLoops(dm);
}

/* Get number of polys. */
static int importer_GetNumPolys(ImportMeshData *import_data)
{
	DerivedMesh *dm = import_data->dm;
	return dm->getNumPolys(dm);
}

/* Get 3D coordinate of vertex with given index. */
static void importer_GetVertCoord(ImportMeshData *import_data, int vert_index, float coord[3])
{
	MVert *mvert = import_data->mvert;

	BLI_assert(vert_index >= 0 && vert_index < import_data->dm->getNumVerts(import_data->dm));

	mul_v3_m4v3(coord, import_data->obmat, mvert[vert_index].co);
}

/* Get index of vertices which are adjucent to edge specified by it's index. */
static void importer_GetEdgeVerts(ImportMeshData *import_data, int edge_index, int *v1, int *v2)
{
	MEdge *medge = &import_data->medge[edge_index];

	BLI_assert(edge_index >= 0 && edge_index < import_data->dm->getNumEdges(import_data->dm));

	*v1 = medge->v1;
	*v2 = medge->v2;
}

/* Get number of adjucent vertices to the poly specified by it's index. */
static int importer_GetPolyNumVerts(ImportMeshData *import_data, int poly_index)
{
	MPoly *mpoly = import_data->mpoly;

	BLI_assert(poly_index >= 0 && poly_index < import_data->dm->getNumPolys(import_data->dm));

	return mpoly[poly_index].totloop;
}

/* Get list of adjucent vertices to the poly specified by it's index. */
static void importer_GetPolyVerts(ImportMeshData *import_data, int poly_index, int *verts)
{
	MPoly *mpoly = &import_data->mpoly[poly_index];
	MLoop *mloop = import_data->mloop + mpoly->loopstart;
	int i;
	BLI_assert(poly_index >= 0 && poly_index < import_data->dm->getNumPolys(import_data->dm));
	for (i = 0; i < mpoly->totloop; i++, mloop++) {
		verts[i] = mloop->v;
	}
}

// Triangulate 2D polygon.
#if 0
static int importer_triangulate2DPoly(ImportMeshData *UNUSED(import_data),
                                      const float (*vertices)[2], int num_vertices,
                                      unsigned int (*triangles)[3])
{
	// TODO(sergey): Currently import_data is unused but in the future we could
	// put memory arena there which will reduce amount of allocations happening
	// over the triangulation period.
	//
	// However that's not so much straighforward to do it right now because we
	// also are tu consider threaded import/export.

	BLI_assert(num_vertices > 3);

	BLI_polyfill_calc(vertices, num_vertices, triangles);

	return num_vertices - 2;
}
#endif

static CarveMeshImporter MeshImporter = {
	importer_GetNumVerts,
	importer_GetNumEdges,
	importer_GetNumLoops,
	importer_GetNumPolys,
	importer_GetVertCoord,
	importer_GetEdgeVerts,
	importer_GetPolyNumVerts,
	importer_GetPolyVerts,

	/* TODO(sergey): We don't use BLI_polyfill_calc() because it tends
	 * to generate degenerated geometry which is fatal for booleans.
	 *
	 * For now we stick to Carve's triangulation.
	 */
	NULL, /* importer_triangulate2DPoly */
};

/* **** Exporter from Carve to derived mesh ****  */

typedef struct ExportMeshData {
	DerivedMesh *dm;
	float obimat[4][4];
	MVert *mvert;
	MEdge *medge;
	MLoop *mloop;
	MPoly *mpoly;
	int *vert_origindex;
	int *edge_origindex;
	int *poly_origindex;
	int *loop_origindex;

	/* Objects and derived meshes of left and right operands.
	 * Used for custom data merge and interpolation.
	 */
	Object *ob_left;
	Object *ob_right;
	DerivedMesh *dm_left;
	DerivedMesh *dm_right;
	MVert *mvert_left;
	MLoop *mloop_left;
	MPoly *mpoly_left;
	MVert *mvert_right;
	MLoop *mloop_right;
	MPoly *mpoly_right;

	float left_to_right_mat[4][4];

	/* Hash to map materials from right object to result. */
	GHash *material_hash;
} ExportMeshData;

BLI_INLINE Object *which_object(ExportMeshData *export_data, int which_mesh)
{
	Object *object = NULL;
	switch (which_mesh) {
		case CARVE_MESH_LEFT:
			object = export_data->ob_left;
			break;
		case CARVE_MESH_RIGHT:
			object = export_data->ob_right;
			break;
	}
	return object;
}

BLI_INLINE DerivedMesh *which_dm(ExportMeshData *export_data, int which_mesh)
{
	DerivedMesh *dm = NULL;
	switch (which_mesh) {
		case CARVE_MESH_LEFT:
			dm = export_data->dm_left;
			break;
		case CARVE_MESH_RIGHT:
			dm = export_data->dm_right;
			break;
	}
	return dm;
}

BLI_INLINE MVert *which_mvert(ExportMeshData *export_data, int which_mesh)
{
	MVert *mvert = NULL;
	switch (which_mesh) {
		case CARVE_MESH_LEFT:
			mvert = export_data->mvert_left;
			break;
		case CARVE_MESH_RIGHT:
			mvert = export_data->mvert_right;
			break;
	}
	return mvert;
}

BLI_INLINE MLoop *which_mloop(ExportMeshData *export_data, int which_mesh)
{
	MLoop *mloop = NULL;
	switch (which_mesh) {
		case CARVE_MESH_LEFT:
			mloop = export_data->mloop_left;
			break;
		case CARVE_MESH_RIGHT:
			mloop = export_data->mloop_right;
			break;
	}
	return mloop;
}

BLI_INLINE MPoly *which_mpoly(ExportMeshData *export_data, int which_mesh)
{
	MPoly *mpoly = NULL;
	switch (which_mesh) {
		case CARVE_MESH_LEFT:
			mpoly = export_data->mpoly_left;
			break;
		case CARVE_MESH_RIGHT:
			mpoly = export_data->mpoly_right;
			break;
	}
	return mpoly;
}

static void allocate_custom_layers(CustomData *data, int type, int num_elements, int num_layers)
{
	int i;
	for (i = 0; i < num_layers; i++) {
		CustomData_add_layer(data, type, CD_DEFAULT, NULL, num_elements);
	}
}

/* Create new external mesh */
static void exporter_InitGeomArrays(ExportMeshData *export_data,
                                    int num_verts, int num_edges,
                                    int num_loops, int num_polys)
{
	DerivedMesh *dm = CDDM_new(num_verts, num_edges, 0,
	                           num_loops, num_polys);
	DerivedMesh *dm_left = export_data->dm_left,
	            *dm_right = export_data->dm_right;

	/* Mask for custom data layers to be merhed from operands. */
	CustomDataMask merge_mask = CD_MASK_DERIVEDMESH & ~CD_MASK_ORIGINDEX;

	export_data->dm = dm;
	export_data->mvert = dm->getVertArray(dm);
	export_data->medge = dm->getEdgeArray(dm);
	export_data->mloop = dm->getLoopArray(dm);
	export_data->mpoly = dm->getPolyArray(dm);

	/* Allocate layers for UV layers and vertex colors.
	 * Without this interpolation of those data will not happen.
	 */
	allocate_custom_layers(&dm->loopData, CD_MLOOPCOL, num_loops,
	                       CustomData_number_of_layers(&dm_left->loopData, CD_MLOOPCOL));
	allocate_custom_layers(&dm->loopData, CD_MLOOPUV, num_loops,
	                       CustomData_number_of_layers(&dm_left->loopData, CD_MLOOPUV));

	/* Merge custom data layers from operands.
	 *
	 * Will only create custom data layers for all the layers which appears in
	 * the operand. Data for those layers will not be allocated or initialized.
	 */
	CustomData_merge(&dm_left->polyData, &dm->polyData, merge_mask, CD_DEFAULT, num_polys);
	CustomData_merge(&dm_right->polyData, &dm->polyData, merge_mask, CD_DEFAULT, num_polys);

	export_data->vert_origindex = dm->getVertDataArray(dm, CD_ORIGINDEX);
	export_data->edge_origindex = dm->getEdgeDataArray(dm, CD_ORIGINDEX);
	export_data->poly_origindex = dm->getPolyDataArray(dm, CD_ORIGINDEX);
	export_data->loop_origindex = dm->getLoopDataArray(dm, CD_ORIGINDEX);
}

/* Set coordinate of vertex with given index. */
static void exporter_SetVert(ExportMeshData *export_data,
                             int vert_index, float coord[3],
                             int which_orig_mesh, int orig_vert_index)
{
	DerivedMesh *dm = export_data->dm;
	DerivedMesh *dm_orig;
	MVert *mvert = export_data->mvert;

	BLI_assert(vert_index >= 0 && vert_index <= dm->getNumVerts(dm));

	dm_orig = which_dm(export_data, which_orig_mesh);
	if (dm_orig) {
		BLI_assert(orig_vert_index >= 0 && orig_vert_index < dm_orig->getNumVerts(dm_orig));
		CustomData_copy_data(&dm_orig->vertData, &dm->vertData, orig_vert_index, vert_index, 1);
	}

	/* Set original index of the vertex. */
	if (export_data->vert_origindex) {
		if (which_orig_mesh == CARVE_MESH_LEFT) {
			export_data->vert_origindex[vert_index] = orig_vert_index;
		}
		else {
			export_data->vert_origindex[vert_index] = ORIGINDEX_NONE;
		}
	}

	mul_v3_m4v3(mvert[vert_index].co, export_data->obimat, coord);
}

/* Set vertices which are adjucent to the edge specified by it's index. */
static void exporter_SetEdge(ExportMeshData *export_data,
                             int edge_index, int v1, int v2,
                             int which_orig_mesh, int orig_edge_index)
{
	DerivedMesh *dm = export_data->dm;
	MEdge *medge = &export_data->medge[edge_index];
	DerivedMesh *dm_orig;

	BLI_assert(edge_index >= 0 && edge_index < dm->getNumEdges(dm));
	BLI_assert(v1 >= 0 && v1 < dm->getNumVerts(dm));
	BLI_assert(v2 >= 0 && v2 < dm->getNumVerts(dm));

	dm_orig = which_dm(export_data, which_orig_mesh);
	if (dm_orig) {
		BLI_assert(orig_edge_index >= 0 && orig_edge_index < dm_orig->getNumEdges(dm_orig));

		/* Copy all edge layers, including mpoly. */
		CustomData_copy_data(&dm_orig->edgeData, &dm->edgeData, orig_edge_index, edge_index, 1);
	}

	/* Set original index of the edge. */
	if (export_data->edge_origindex) {
		if (which_orig_mesh == CARVE_MESH_LEFT) {
			export_data->edge_origindex[edge_index] = orig_edge_index;
		}
		else {
			export_data->edge_origindex[edge_index] = ORIGINDEX_NONE;
		}
	}

	medge->v1 = v1;
	medge->v2 = v2;

	medge->flag |= ME_EDGEDRAW | ME_EDGERENDER;
}

static void setMPolyMaterial(ExportMeshData *export_data,
                             MPoly *mpoly,
                             int which_orig_mesh)
{
	Object *orig_object;
	GHash *material_hash;
	Material *orig_mat;

	if (which_orig_mesh == CARVE_MESH_LEFT) {
		/* No need to change materian index for faces from left operand */
		return;
	}

	material_hash = export_data->material_hash;
	orig_object = which_object(export_data, which_orig_mesh);

	/* Set material, based on lookup in hash table. */
	orig_mat = give_current_material(orig_object, mpoly->mat_nr + 1);

	if (orig_mat) {
		/* For faces from right operand check if there's requested material
		 * in the left operand. And if it is, use index of that material,
		 * otherwise fallback to first material (material with index=0).
		 */
		if (!BLI_ghash_haskey(material_hash, orig_mat)) {
			int a, mat_nr;;

			mat_nr = 0;
			for (a = 0; a < export_data->ob_left->totcol; a++) {
				if (give_current_material(export_data->ob_left, a + 1) == orig_mat) {
					mat_nr = a;
					break;
				}
			}

			BLI_ghash_insert(material_hash, orig_mat, SET_INT_IN_POINTER(mat_nr));

			mpoly->mat_nr = mat_nr;
		}
		else
			mpoly->mat_nr = GET_INT_FROM_POINTER(BLI_ghash_lookup(material_hash, orig_mat));
	}
	else {
		mpoly->mat_nr = 0;
	}
}

/* Set list of adjucent loops to the poly specified by it's index. */
static void exporter_SetPoly(ExportMeshData *export_data,
                             int poly_index, int start_loop, int num_loops,
                             int which_orig_mesh, int orig_poly_index)
{
	DerivedMesh *dm = export_data->dm;
	MPoly *mpoly = &export_data->mpoly[poly_index];
	DerivedMesh *dm_orig;
	int i;

	/* Poly is always to be either from left or right operand. */
	dm_orig = which_dm(export_data, which_orig_mesh);

	BLI_assert(poly_index >= 0 && poly_index < dm->getNumPolys(dm));
	BLI_assert(start_loop >= 0 && start_loop <= dm->getNumLoops(dm) - num_loops);
	BLI_assert(num_loops >= 3);
	BLI_assert(dm_orig != NULL);
	BLI_assert(orig_poly_index >= 0 && orig_poly_index < dm_orig->getNumPolys(dm_orig));

	/* Copy all poly layers, including mpoly. */
	CustomData_copy_data(&dm_orig->polyData, &dm->polyData, orig_poly_index, poly_index, 1);

	/* Set material of the curren poly.
	 * This would re-map materials from right operand to materials from the
	 * left one as well.
	 */
	setMPolyMaterial(export_data, mpoly, which_orig_mesh);

	/* Set original index of the poly. */
	if (export_data->poly_origindex) {
		if (which_orig_mesh == CARVE_MESH_LEFT) {
			export_data->poly_origindex[poly_index] = orig_poly_index;
		}
		else {
			export_data->poly_origindex[poly_index] = ORIGINDEX_NONE;
		}
	}

	/* Set poly data itself. */
	mpoly->loopstart = start_loop;
	mpoly->totloop = num_loops;

	/* Interpolate data for poly loops. */
	{
		MVert *source_mverts = which_mvert(export_data, which_orig_mesh);
		MLoop *source_mloops = which_mloop(export_data, which_orig_mesh);
		MPoly *source_mpolys = which_mpoly(export_data, which_orig_mesh);
		MPoly *source_poly = &source_mpolys[orig_poly_index];
		MVert *target_mverts = export_data->mvert;
		MLoop *target_mloops = export_data->mloop;
		float (*transform)[4] = NULL;

		if (which_orig_mesh == CARVE_MESH_RIGHT) {
			transform = export_data->left_to_right_mat;
		}

		for (i = 0; i < mpoly->totloop; i++) {
			DM_loop_interp_from_poly(dm_orig,
			                         source_mverts,
			                         source_mloops,
			                         source_poly,
			                         dm,
			                         target_mverts,
			                         target_mloops,
			                         transform,
			                         i + mpoly->loopstart);
		}
	}
}

/* Set list vertex and edge which are adjucent to loop with given index. */
static void exporter_SetLoop(ExportMeshData *export_data,
                             int loop_index, int vertex, int edge,
                             int which_orig_mesh, int orig_loop_index)
{
	DerivedMesh *dm = export_data->dm;
	MLoop *mloop = &export_data->mloop[loop_index];
	DerivedMesh *dm_orig;

	BLI_assert(loop_index >= 0 && loop_index < dm->getNumLoops(dm));
	BLI_assert(vertex >= 0 && vertex < dm->getNumVerts(dm));
	BLI_assert(edge >= 0 && vertex < dm->getNumEdges(dm));

	dm_orig = which_dm(export_data, which_orig_mesh);
	if (dm_orig) {
		BLI_assert(orig_loop_index >= 0 && orig_loop_index < dm_orig->getNumLoops(dm_orig));

		/* Copy all loop layers, including mpoly. */
		CustomData_copy_data(&dm_orig->loopData, &dm->loopData, orig_loop_index, loop_index, 1);
	}

	/* Set original index of the loop. */
	if (export_data->loop_origindex) {
		if (which_orig_mesh == CARVE_MESH_LEFT) {
			export_data->loop_origindex[loop_index] = orig_loop_index;
		}
		else {
			export_data->loop_origindex[loop_index] = ORIGINDEX_NONE;
		}
	}

	mloop->v = vertex;
	mloop->e = edge;
}

/* Edge index from a loop index for a given original mesh. */
static int exporter_MapLoopToEdge(ExportMeshData *export_data,
                                  int which_mesh, int loop_index)
{
	DerivedMesh *dm = which_dm(export_data, which_mesh);
	MLoop *mloop = which_mloop(export_data, which_mesh);

	(void) dm;  /* Unused in release builds. */

	BLI_assert(dm != NULL);
	BLI_assert(loop_index >= 0 && loop_index < dm->getNumLoops(dm));

	return mloop[loop_index].e;
}

static CarveMeshExporter MeshExporter = {
	exporter_InitGeomArrays,
	exporter_SetVert,
	exporter_SetEdge,
	exporter_SetPoly,
	exporter_SetLoop,
	exporter_MapLoopToEdge
};

static int operation_from_optype(int int_op_type)
{
	int operation;

	switch (int_op_type) {
		case 1:
			operation = CARVE_OP_INTERSECTION;
			break;
		case 2:
			operation = CARVE_OP_UNION;
			break;
		case 3:
			operation = CARVE_OP_A_MINUS_B;
			break;
		default:
			BLI_assert(!"Should not happen");
			operation = -1;
			break;
	}

	return operation;
}

static void prepare_import_data(Object *object, DerivedMesh *dm, ImportMeshData *import_data)
{
	import_data->dm = dm;
	copy_m4_m4(import_data->obmat, object->obmat);
	import_data->mvert = dm->getVertArray(dm);
	import_data->medge = dm->getEdgeArray(dm);
	import_data->mloop = dm->getLoopArray(dm);
	import_data->mpoly = dm->getPolyArray(dm);
}

static struct CarveMeshDescr *carve_mesh_from_dm(Object *object, DerivedMesh *dm)
{
	ImportMeshData import_data;
	prepare_import_data(object, dm, &import_data);
	return carve_addMesh(&import_data, &MeshImporter);
}

static void prepare_export_data(Object *object_left, DerivedMesh *dm_left,
                                Object *object_right, DerivedMesh *dm_right,
                                ExportMeshData *export_data)
{
	float object_right_imat[4][4];

	invert_m4_m4(export_data->obimat, object_left->obmat);

	export_data->ob_left = object_left;
	export_data->ob_right = object_right;

	export_data->dm_left = dm_left;
	export_data->dm_right = dm_right;

	export_data->mvert_left = dm_left->getVertArray(dm_left);
	export_data->mloop_left = dm_left->getLoopArray(dm_left);
	export_data->mpoly_left = dm_left->getPolyArray(dm_left);
	export_data->mvert_right = dm_right->getVertArray(dm_right);
	export_data->mloop_right = dm_right->getLoopArray(dm_right);
	export_data->mpoly_right = dm_right->getPolyArray(dm_right);

	export_data->material_hash = BLI_ghash_ptr_new("CSG_mat gh");

	/* Matrix to convert coord from left object's loca; space to
	 * right object's local space.
	 */
	invert_m4_m4(object_right_imat, object_right->obmat);
	mul_m4_m4m4(export_data->left_to_right_mat, object_left->obmat,
	            object_right_imat);
}

DerivedMesh *NewBooleanDerivedMesh(DerivedMesh *dm, struct Object *ob,
                                   DerivedMesh *dm_select, struct Object *ob_select,
                                   int int_op_type)
{

	struct CarveMeshDescr *left, *right, *output = NULL;
	DerivedMesh *output_dm = NULL;
	int operation;
	bool result;

	if (dm == NULL || dm_select == NULL) {
		return NULL;
	}

	operation = operation_from_optype(int_op_type);
	if (operation == -1) {
		return NULL;
	}

	left = carve_mesh_from_dm(ob_select, dm_select);
	right = carve_mesh_from_dm(ob, dm);

	result = carve_performBooleanOperation(left, right, operation, &output);

	carve_deleteMesh(left);
	carve_deleteMesh(right);

	if (result) {
		ExportMeshData export_data;

		prepare_export_data(ob_select, dm_select, ob, dm, &export_data);

		carve_exportMesh(output, &MeshExporter, &export_data);
		output_dm = export_data.dm;

		/* Free memory used by export mesh. */
		BLI_ghash_free(export_data.material_hash, NULL, NULL);

		output_dm->dirty |= DM_DIRTY_NORMALS;
		carve_deleteMesh(output);
	}

	return output_dm;
}
