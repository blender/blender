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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/DerivedMesh.c
 *  \ingroup bke
 */


#include <string.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "DNA_cloth_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_linklist.h"
#include "BLI_task.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_colorband.h"
#include "BKE_editmesh.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_tangent.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"
#include "BKE_paint.h"
#include "BKE_multires.h"
#include "BKE_bvhutils.h"
#include "BKE_deform.h"
#include "BKE_global.h" /* For debug flag, DM_update_tessface_data() func. */

#include "BLI_sys_types.h" /* for intptr_t support */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#ifdef WITH_OPENSUBDIV
#  include "DNA_userdef_types.h"
#endif

/* very slow! enable for testing only! */
//#define USE_MODIFIER_VALIDATE

#ifdef USE_MODIFIER_VALIDATE
#  define ASSERT_IS_VALID_DM(dm) (BLI_assert((dm == NULL) || (DM_is_valid(dm) == true)))
#  define ASSERT_IS_VALID_MESH(mesh) (BLI_assert((mesh == NULL) || (BKE_mesh_is_valid(mesh) == true)))
#else
#  define ASSERT_IS_VALID_DM(dm)
#  define ASSERT_IS_VALID_MESH(mesh)
#endif


static ThreadRWMutex loops_cache_lock = PTHREAD_RWLOCK_INITIALIZER;


static void shapekey_layers_to_keyblocks(DerivedMesh *dm, Mesh *me, int actshape_uid);

static void mesh_init_origspace(Mesh *mesh);


/* -------------------------------------------------------------------- */

static MVert *dm_getVertArray(DerivedMesh *dm)
{
	MVert *mvert = CustomData_get_layer(&dm->vertData, CD_MVERT);

	if (!mvert) {
		mvert = CustomData_add_layer(&dm->vertData, CD_MVERT, CD_CALLOC, NULL,
		                             dm->getNumVerts(dm));
		CustomData_set_layer_flag(&dm->vertData, CD_MVERT, CD_FLAG_TEMPORARY);
		dm->copyVertArray(dm, mvert);
	}

	return mvert;
}

static MEdge *dm_getEdgeArray(DerivedMesh *dm)
{
	MEdge *medge = CustomData_get_layer(&dm->edgeData, CD_MEDGE);

	if (!medge) {
		medge = CustomData_add_layer(&dm->edgeData, CD_MEDGE, CD_CALLOC, NULL,
		                             dm->getNumEdges(dm));
		CustomData_set_layer_flag(&dm->edgeData, CD_MEDGE, CD_FLAG_TEMPORARY);
		dm->copyEdgeArray(dm, medge);
	}

	return medge;
}

static MFace *dm_getTessFaceArray(DerivedMesh *dm)
{
	MFace *mface = CustomData_get_layer(&dm->faceData, CD_MFACE);

	if (!mface) {
		int numTessFaces = dm->getNumTessFaces(dm);

		if (!numTessFaces) {
			/* Do not add layer if there's no elements in it, this leads to issues later when
			 * this layer is needed with non-zero size, but currently CD stuff does not check
			 * for requested layer size on creation and just returns layer which was previously
			 * added (sergey) */
			return NULL;
		}

		mface = CustomData_add_layer(&dm->faceData, CD_MFACE, CD_CALLOC, NULL, numTessFaces);
		CustomData_set_layer_flag(&dm->faceData, CD_MFACE, CD_FLAG_TEMPORARY);
		dm->copyTessFaceArray(dm, mface);
	}

	return mface;
}

static MLoop *dm_getLoopArray(DerivedMesh *dm)
{
	MLoop *mloop = CustomData_get_layer(&dm->loopData, CD_MLOOP);

	if (!mloop) {
		mloop = CustomData_add_layer(&dm->loopData, CD_MLOOP, CD_CALLOC, NULL,
		                             dm->getNumLoops(dm));
		CustomData_set_layer_flag(&dm->loopData, CD_MLOOP, CD_FLAG_TEMPORARY);
		dm->copyLoopArray(dm, mloop);
	}

	return mloop;
}

static MPoly *dm_getPolyArray(DerivedMesh *dm)
{
	MPoly *mpoly = CustomData_get_layer(&dm->polyData, CD_MPOLY);

	if (!mpoly) {
		mpoly = CustomData_add_layer(&dm->polyData, CD_MPOLY, CD_CALLOC, NULL,
		                             dm->getNumPolys(dm));
		CustomData_set_layer_flag(&dm->polyData, CD_MPOLY, CD_FLAG_TEMPORARY);
		dm->copyPolyArray(dm, mpoly);
	}

	return mpoly;
}

static MVert *dm_dupVertArray(DerivedMesh *dm)
{
	MVert *tmp = MEM_malloc_arrayN(dm->getNumVerts(dm), sizeof(*tmp),
	                         "dm_dupVertArray tmp");

	if (tmp) dm->copyVertArray(dm, tmp);

	return tmp;
}

static MEdge *dm_dupEdgeArray(DerivedMesh *dm)
{
	MEdge *tmp = MEM_malloc_arrayN(dm->getNumEdges(dm), sizeof(*tmp),
	                         "dm_dupEdgeArray tmp");

	if (tmp) dm->copyEdgeArray(dm, tmp);

	return tmp;
}

static MFace *dm_dupFaceArray(DerivedMesh *dm)
{
	MFace *tmp = MEM_malloc_arrayN(dm->getNumTessFaces(dm), sizeof(*tmp),
	                         "dm_dupFaceArray tmp");

	if (tmp) dm->copyTessFaceArray(dm, tmp);

	return tmp;
}

static MLoop *dm_dupLoopArray(DerivedMesh *dm)
{
	MLoop *tmp = MEM_malloc_arrayN(dm->getNumLoops(dm), sizeof(*tmp),
	                         "dm_dupLoopArray tmp");

	if (tmp) dm->copyLoopArray(dm, tmp);

	return tmp;
}

static MPoly *dm_dupPolyArray(DerivedMesh *dm)
{
	MPoly *tmp = MEM_malloc_arrayN(dm->getNumPolys(dm), sizeof(*tmp),
	                         "dm_dupPolyArray tmp");

	if (tmp) dm->copyPolyArray(dm, tmp);

	return tmp;
}

static int dm_getNumLoopTri(DerivedMesh *dm)
{
	const int numlooptris = poly_to_tri_count(dm->getNumPolys(dm), dm->getNumLoops(dm));
	BLI_assert(ELEM(dm->looptris.num, 0, numlooptris));
	return numlooptris;
}

static const MLoopTri *dm_getLoopTriArray(DerivedMesh *dm)
{
	MLoopTri *looptri;

	BLI_rw_mutex_lock(&loops_cache_lock, THREAD_LOCK_READ);
	looptri = dm->looptris.array;
	BLI_rw_mutex_unlock(&loops_cache_lock);

	if (looptri != NULL) {
		BLI_assert(dm->getNumLoopTri(dm) == dm->looptris.num);
	}
	else {
		BLI_rw_mutex_lock(&loops_cache_lock, THREAD_LOCK_WRITE);
		/* We need to ensure array is still NULL inside mutex-protected code, some other thread might have already
		 * recomputed those looptris. */
		if (dm->looptris.array == NULL) {
			dm->recalcLoopTri(dm);
		}
		looptri = dm->looptris.array;
		BLI_rw_mutex_unlock(&loops_cache_lock);
	}
	return looptri;
}

static CustomData *dm_getVertCData(DerivedMesh *dm)
{
	return &dm->vertData;
}

static CustomData *dm_getEdgeCData(DerivedMesh *dm)
{
	return &dm->edgeData;
}

static CustomData *dm_getTessFaceCData(DerivedMesh *dm)
{
	return &dm->faceData;
}

static CustomData *dm_getLoopCData(DerivedMesh *dm)
{
	return &dm->loopData;
}

static CustomData *dm_getPolyCData(DerivedMesh *dm)
{
	return &dm->polyData;
}

/**
 * Utility function to initialize a DerivedMesh's function pointers to
 * the default implementation (for those functions which have a default)
 */
void DM_init_funcs(DerivedMesh *dm)
{
	/* default function implementations */
	dm->getVertArray = dm_getVertArray;
	dm->getEdgeArray = dm_getEdgeArray;
	dm->getTessFaceArray = dm_getTessFaceArray;
	dm->getLoopArray = dm_getLoopArray;
	dm->getPolyArray = dm_getPolyArray;
	dm->dupVertArray = dm_dupVertArray;
	dm->dupEdgeArray = dm_dupEdgeArray;
	dm->dupTessFaceArray = dm_dupFaceArray;
	dm->dupLoopArray = dm_dupLoopArray;
	dm->dupPolyArray = dm_dupPolyArray;

	dm->getLoopTriArray = dm_getLoopTriArray;

	/* subtypes handle getting actual data */
	dm->getNumLoopTri = dm_getNumLoopTri;

	dm->getVertDataLayout = dm_getVertCData;
	dm->getEdgeDataLayout = dm_getEdgeCData;
	dm->getTessFaceDataLayout = dm_getTessFaceCData;
	dm->getLoopDataLayout = dm_getLoopCData;
	dm->getPolyDataLayout = dm_getPolyCData;

	dm->getVertData = DM_get_vert_data;
	dm->getEdgeData = DM_get_edge_data;
	dm->getTessFaceData = DM_get_tessface_data;
	dm->getPolyData = DM_get_poly_data;
	dm->getVertDataArray = DM_get_vert_data_layer;
	dm->getEdgeDataArray = DM_get_edge_data_layer;
	dm->getTessFaceDataArray = DM_get_tessface_data_layer;
	dm->getPolyDataArray = DM_get_poly_data_layer;
	dm->getLoopDataArray = DM_get_loop_data_layer;

	dm->bvhCache = NULL;
}

/**
 * Utility function to initialize a DerivedMesh for the desired number
 * of vertices, edges and faces (doesn't allocate memory for them, just
 * sets up the custom data layers)
 */
void DM_init(
        DerivedMesh *dm, DerivedMeshType type, int numVerts, int numEdges,
        int numTessFaces, int numLoops, int numPolys)
{
	dm->type = type;
	dm->numVertData = numVerts;
	dm->numEdgeData = numEdges;
	dm->numTessFaceData = numTessFaces;
	dm->numLoopData = numLoops;
	dm->numPolyData = numPolys;

	DM_init_funcs(dm);

	dm->needsFree = 1;
	dm->dirty = 0;

	/* don't use CustomData_reset(...); because we dont want to touch customdata */
	copy_vn_i(dm->vertData.typemap, CD_NUMTYPES, -1);
	copy_vn_i(dm->edgeData.typemap, CD_NUMTYPES, -1);
	copy_vn_i(dm->faceData.typemap, CD_NUMTYPES, -1);
	copy_vn_i(dm->loopData.typemap, CD_NUMTYPES, -1);
	copy_vn_i(dm->polyData.typemap, CD_NUMTYPES, -1);
}

/**
 * Utility function to initialize a DerivedMesh for the desired number
 * of vertices, edges and faces, with a layer setup copied from source
 */
void DM_from_template_ex(
        DerivedMesh *dm, DerivedMesh *source, DerivedMeshType type,
        int numVerts, int numEdges, int numTessFaces,
        int numLoops, int numPolys,
        CustomDataMask mask)
{
	CustomData_copy(&source->vertData, &dm->vertData, mask, CD_CALLOC, numVerts);
	CustomData_copy(&source->edgeData, &dm->edgeData, mask, CD_CALLOC, numEdges);
	CustomData_copy(&source->faceData, &dm->faceData, mask, CD_CALLOC, numTessFaces);
	CustomData_copy(&source->loopData, &dm->loopData, mask, CD_CALLOC, numLoops);
	CustomData_copy(&source->polyData, &dm->polyData, mask, CD_CALLOC, numPolys);

	dm->cd_flag = source->cd_flag;

	dm->type = type;
	dm->numVertData = numVerts;
	dm->numEdgeData = numEdges;
	dm->numTessFaceData = numTessFaces;
	dm->numLoopData = numLoops;
	dm->numPolyData = numPolys;

	DM_init_funcs(dm);

	dm->needsFree = 1;
	dm->dirty = 0;
}
void DM_from_template(
        DerivedMesh *dm, DerivedMesh *source, DerivedMeshType type,
        int numVerts, int numEdges, int numTessFaces,
        int numLoops, int numPolys)
{
	DM_from_template_ex(
	        dm, source, type,
	        numVerts, numEdges, numTessFaces,
	        numLoops, numPolys,
	        CD_MASK_DERIVEDMESH);
}

int DM_release(DerivedMesh *dm)
{
	if (dm->needsFree) {
		bvhcache_free(&dm->bvhCache);
		CustomData_free(&dm->vertData, dm->numVertData);
		CustomData_free(&dm->edgeData, dm->numEdgeData);
		CustomData_free(&dm->faceData, dm->numTessFaceData);
		CustomData_free(&dm->loopData, dm->numLoopData);
		CustomData_free(&dm->polyData, dm->numPolyData);

		if (dm->mat) {
			MEM_freeN(dm->mat);
			dm->mat = NULL;
			dm->totmat = 0;
		}

		MEM_SAFE_FREE(dm->looptris.array);
		dm->looptris.num = 0;
		dm->looptris.num_alloc = 0;

		return 1;
	}
	else {
		CustomData_free_temporary(&dm->vertData, dm->numVertData);
		CustomData_free_temporary(&dm->edgeData, dm->numEdgeData);
		CustomData_free_temporary(&dm->faceData, dm->numTessFaceData);
		CustomData_free_temporary(&dm->loopData, dm->numLoopData);
		CustomData_free_temporary(&dm->polyData, dm->numPolyData);

		return 0;
	}
}

void DM_DupPolys(DerivedMesh *source, DerivedMesh *target)
{
	CustomData_free(&target->loopData, source->numLoopData);
	CustomData_free(&target->polyData, source->numPolyData);

	CustomData_copy(&source->loopData, &target->loopData, CD_MASK_DERIVEDMESH, CD_DUPLICATE, source->numLoopData);
	CustomData_copy(&source->polyData, &target->polyData, CD_MASK_DERIVEDMESH, CD_DUPLICATE, source->numPolyData);

	target->numLoopData = source->numLoopData;
	target->numPolyData = source->numPolyData;

	if (!CustomData_has_layer(&target->polyData, CD_MPOLY)) {
		MPoly *mpoly;
		MLoop *mloop;

		mloop = source->dupLoopArray(source);
		mpoly = source->dupPolyArray(source);
		CustomData_add_layer(&target->loopData, CD_MLOOP, CD_ASSIGN, mloop, source->numLoopData);
		CustomData_add_layer(&target->polyData, CD_MPOLY, CD_ASSIGN, mpoly, source->numPolyData);
	}
}

void DM_ensure_normals(DerivedMesh *dm)
{
	if (dm->dirty & DM_DIRTY_NORMALS) {
		dm->calcNormals(dm);
	}
	BLI_assert((dm->dirty & DM_DIRTY_NORMALS) == 0);
}

static void DM_calc_loop_normals(DerivedMesh *dm, const bool use_split_normals, float split_angle)
{
	dm->calcLoopNormals(dm, use_split_normals, split_angle);
	dm->dirty |= DM_DIRTY_TESS_CDLAYERS;
}

/* note: until all modifiers can take MPoly's as input,
 * use this at the start of modifiers  */
void DM_ensure_tessface(DerivedMesh *dm)
{
	const int numTessFaces = dm->getNumTessFaces(dm);
	const int numPolys =     dm->getNumPolys(dm);

	if ((numTessFaces == 0) && (numPolys != 0)) {
		dm->recalcTessellation(dm);

		if (dm->getNumTessFaces(dm) != 0) {
			/* printf("info %s: polys -> ngons calculated\n", __func__); */
		}
		else {
			printf("warning %s: could not create tessfaces from %d polygons, dm->type=%u\n",
			       __func__, numPolys, dm->type);
		}
	}

	else if (dm->dirty & DM_DIRTY_TESS_CDLAYERS) {
		BLI_assert(CustomData_has_layer(&dm->faceData, CD_ORIGINDEX) || numTessFaces == 0);
		DM_update_tessface_data(dm);
	}

	dm->dirty &= ~DM_DIRTY_TESS_CDLAYERS;
}

/**
 * Ensure the array is large enough
 *
 * /note This function must always be thread-protected by caller. It should only be used by internal code.
 */
void DM_ensure_looptri_data(DerivedMesh *dm)
{
	const unsigned int totpoly = dm->numPolyData;
	const unsigned int totloop = dm->numLoopData;
	const int looptris_num = poly_to_tri_count(totpoly, totloop);

	BLI_assert(dm->looptris.array_wip == NULL);

	SWAP(MLoopTri *, dm->looptris.array, dm->looptris.array_wip);

	if ((looptris_num > dm->looptris.num_alloc) ||
	    (looptris_num < dm->looptris.num_alloc * 2) ||
	    (totpoly == 0))
	{
		MEM_SAFE_FREE(dm->looptris.array_wip);
		dm->looptris.num_alloc = 0;
		dm->looptris.num = 0;
	}

	if (totpoly) {
		if (dm->looptris.array_wip == NULL) {
			dm->looptris.array_wip = MEM_malloc_arrayN(looptris_num, sizeof(*dm->looptris.array_wip), __func__);
			dm->looptris.num_alloc = looptris_num;
		}

		dm->looptris.num = looptris_num;
	}
}

void DM_verttri_from_looptri(MVertTri *verttri, const MLoop *mloop, const MLoopTri *looptri, int looptri_num)
{
	int i;
	for (i = 0; i < looptri_num; i++) {
		verttri[i].tri[0] = mloop[looptri[i].tri[0]].v;
		verttri[i].tri[1] = mloop[looptri[i].tri[1]].v;
		verttri[i].tri[2] = mloop[looptri[i].tri[2]].v;
	}
}

/* Update tessface CD data from loop/poly ones. Needed when not retessellating after modstack evaluation. */
/* NOTE: Assumes dm has valid tessellated data! */
void DM_update_tessface_data(DerivedMesh *dm)
{
	MFace *mf, *mface = dm->getTessFaceArray(dm);
	MPoly *mp = dm->getPolyArray(dm);
	MLoop *ml = dm->getLoopArray(dm);

	CustomData *fdata = dm->getTessFaceDataLayout(dm);
	CustomData *ldata = dm->getLoopDataLayout(dm);

	const int totface = dm->getNumTessFaces(dm);
	int mf_idx;

	int *polyindex = CustomData_get_layer(fdata, CD_ORIGINDEX);
	unsigned int (*loopindex)[4];

	/* Should never occure, but better abort than segfault! */
	if (!polyindex)
		return;

	CustomData_from_bmeshpoly(fdata, ldata, totface);

	if (CustomData_has_layer(fdata, CD_MTFACE) ||
	    CustomData_has_layer(fdata, CD_MCOL) ||
	    CustomData_has_layer(fdata, CD_PREVIEW_MCOL) ||
	    CustomData_has_layer(fdata, CD_ORIGSPACE) ||
	    CustomData_has_layer(fdata, CD_TESSLOOPNORMAL) ||
	    CustomData_has_layer(fdata, CD_TANGENT))
	{
		loopindex = MEM_malloc_arrayN(totface, sizeof(*loopindex), __func__);

		for (mf_idx = 0, mf = mface; mf_idx < totface; mf_idx++, mf++) {
			const int mf_len = mf->v4 ? 4 : 3;
			unsigned int *ml_idx = loopindex[mf_idx];
			int i, not_done;

			/* Find out loop indices. */
			/* NOTE: This assumes tessface are valid and in sync with loop/poly... Else, most likely, segfault! */
			for (i = mp[polyindex[mf_idx]].loopstart, not_done = mf_len; not_done; i++) {
				const int tf_v = BKE_MESH_TESSFACE_VINDEX_ORDER(mf, ml[i].v);
				if (tf_v != -1) {
					ml_idx[tf_v] = i;
					not_done--;
				}
			}
		}

		/* NOTE: quad detection issue - fourth vertidx vs fourth loopidx:
		 * Here, our tfaces' fourth vertex index is never 0 for a quad. However, we know our fourth loop index may be
		 * 0 for quads (because our quads may have been rotated compared to their org poly, see tessellation code).
		 * So we pass the MFace's, and BKE_mesh_loops_to_tessdata will use MFace->v4 index as quad test.
		 */
		BKE_mesh_loops_to_tessdata(fdata, ldata, mface, polyindex, loopindex, totface);

		MEM_freeN(loopindex);
	}

	if (G.debug & G_DEBUG)
		printf("%s: Updated tessellated customdata of dm %p\n", __func__, dm);

	dm->dirty &= ~DM_DIRTY_TESS_CDLAYERS;
}

void DM_generate_tangent_tessface_data(DerivedMesh *dm, bool generate)
{
	MFace *mf, *mface = dm->getTessFaceArray(dm);
	MPoly *mp = dm->getPolyArray(dm);
	MLoop *ml = dm->getLoopArray(dm);

	CustomData *fdata = dm->getTessFaceDataLayout(dm);
	CustomData *ldata = dm->getLoopDataLayout(dm);

	const int totface = dm->getNumTessFaces(dm);
	int mf_idx;

	int *polyindex = CustomData_get_layer(fdata, CD_ORIGINDEX);
	unsigned int (*loopindex)[4] = NULL;

	/* Should never occure, but better abort than segfault! */
	if (!polyindex)
		return;

	if (generate) {
		for (int j = 0; j < ldata->totlayer; j++) {
			if (ldata->layers[j].type == CD_TANGENT) {
				CustomData_add_layer_named(fdata, CD_TANGENT, CD_CALLOC, NULL, totface, ldata->layers[j].name);
				CustomData_bmesh_update_active_layers(fdata, ldata);

				if (!loopindex) {
					loopindex = MEM_malloc_arrayN(totface, sizeof(*loopindex), __func__);
					for (mf_idx = 0, mf = mface; mf_idx < totface; mf_idx++, mf++) {
						const int mf_len = mf->v4 ? 4 : 3;
						unsigned int *ml_idx = loopindex[mf_idx];

						/* Find out loop indices. */
						/* NOTE: This assumes tessface are valid and in sync with loop/poly... Else, most likely, segfault! */
						for (int i = mp[polyindex[mf_idx]].loopstart, not_done = mf_len; not_done; i++) {
							const int tf_v = BKE_MESH_TESSFACE_VINDEX_ORDER(mf, ml[i].v);
							if (tf_v != -1) {
								ml_idx[tf_v] = i;
								not_done--;
							}
						}
					}
				}

				/* NOTE: quad detection issue - fourth vertidx vs fourth loopidx:
				 * Here, our tfaces' fourth vertex index is never 0 for a quad. However, we know our fourth loop index may be
				 * 0 for quads (because our quads may have been rotated compared to their org poly, see tessellation code).
				 * So we pass the MFace's, and BKE_mesh_loops_to_tessdata will use MFace->v4 index as quad test.
				 */
				BKE_mesh_tangent_loops_to_tessdata(fdata, ldata, mface, polyindex, loopindex, totface, ldata->layers[j].name);
			}
		}
		if (loopindex)
			MEM_freeN(loopindex);
		BLI_assert(CustomData_from_bmeshpoly_test(fdata, ldata, true));
	}

	if (G.debug & G_DEBUG)
		printf("%s: Updated tessellated tangents of dm %p\n", __func__, dm);
}


void DM_update_materials(DerivedMesh *dm, Object *ob)
{
	int i, totmat = ob->totcol + 1; /* materials start from 1, default material is 0 */

	if (dm->totmat != totmat) {
		dm->totmat = totmat;
		/* invalidate old materials */
		if (dm->mat)
			MEM_freeN(dm->mat);

		dm->mat = MEM_malloc_arrayN(totmat, sizeof(*dm->mat), "DerivedMesh.mat");
	}

	/* we leave last material as empty - rationale here is being able to index
	 * the materials by using the mf->mat_nr directly and leaving the last
	 * material as NULL in case no materials exist on mesh, so indexing will not fail */
	for (i = 0; i < totmat - 1; i++) {
		dm->mat[i] = give_current_material(ob, i + 1);
	}
	dm->mat[i] = NULL;
}

MLoopUV *DM_paint_uvlayer_active_get(DerivedMesh *dm, int mat_nr)
{
	MLoopUV *uv_base;

	BLI_assert(mat_nr < dm->totmat);

	if (dm->mat[mat_nr] && dm->mat[mat_nr]->texpaintslot &&
	    dm->mat[mat_nr]->texpaintslot[dm->mat[mat_nr]->paint_active_slot].uvname)
	{
		uv_base = CustomData_get_layer_named(&dm->loopData, CD_MLOOPUV,
		                                     dm->mat[mat_nr]->texpaintslot[dm->mat[mat_nr]->paint_active_slot].uvname);
		/* This can fail if we have changed the name in the UV layer list and have assigned the old name in the material
		 * texture slot.*/
		if (!uv_base)
			uv_base = CustomData_get_layer(&dm->loopData, CD_MLOOPUV);
	}
	else {
		uv_base = CustomData_get_layer(&dm->loopData, CD_MLOOPUV);
	}

	return uv_base;
}

void DM_to_mesh(DerivedMesh *dm, Mesh *me, Object *ob, CustomDataMask mask, bool take_ownership)
{
	/* dm might depend on me, so we need to do everything with a local copy */
	Mesh tmp = *me;
	int totvert, totedge /*, totface */ /* UNUSED */, totloop, totpoly;
	int did_shapekeys = 0;
	eCDAllocType alloctype = CD_DUPLICATE;

	if (take_ownership && dm->type == DM_TYPE_CDDM && dm->needsFree) {
		bool has_any_referenced_layers =
		        CustomData_has_referenced(&dm->vertData) ||
		        CustomData_has_referenced(&dm->edgeData) ||
		        CustomData_has_referenced(&dm->loopData) ||
		        CustomData_has_referenced(&dm->faceData) ||
		        CustomData_has_referenced(&dm->polyData);
		if (!has_any_referenced_layers) {
			alloctype = CD_ASSIGN;
		}
	}

	CustomData_reset(&tmp.vdata);
	CustomData_reset(&tmp.edata);
	CustomData_reset(&tmp.fdata);
	CustomData_reset(&tmp.ldata);
	CustomData_reset(&tmp.pdata);

	DM_ensure_normals(dm);

	totvert = tmp.totvert = dm->getNumVerts(dm);
	totedge = tmp.totedge = dm->getNumEdges(dm);
	totloop = tmp.totloop = dm->getNumLoops(dm);
	totpoly = tmp.totpoly = dm->getNumPolys(dm);
	tmp.totface = 0;

	CustomData_copy(&dm->vertData, &tmp.vdata, mask, alloctype, totvert);
	CustomData_copy(&dm->edgeData, &tmp.edata, mask, alloctype, totedge);
	CustomData_copy(&dm->loopData, &tmp.ldata, mask, alloctype, totloop);
	CustomData_copy(&dm->polyData, &tmp.pdata, mask, alloctype, totpoly);
	tmp.cd_flag = dm->cd_flag;
	tmp.runtime.deformed_only = dm->deformedOnly;

	if (CustomData_has_layer(&dm->vertData, CD_SHAPEKEY)) {
		KeyBlock *kb;
		int uid;

		if (ob) {
			kb = BLI_findlink(&me->key->block, ob->shapenr - 1);
			if (kb) {
				uid = kb->uid;
			}
			else {
				printf("%s: error - could not find active shapekey %d!\n",
				       __func__, ob->shapenr - 1);

				uid = INT_MAX;
			}
		}
		else {
			/* if no object, set to INT_MAX so we don't mess up any shapekey layers */
			uid = INT_MAX;
		}

		shapekey_layers_to_keyblocks(dm, me, uid);
		did_shapekeys = 1;
	}

	/* copy texture space */
	if (ob) {
		BKE_mesh_texspace_copy_from_object(&tmp, ob);
	}

	/* not all DerivedMeshes store their verts/edges/faces in CustomData, so
	 * we set them here in case they are missing */
	if (!CustomData_has_layer(&tmp.vdata, CD_MVERT)) {
		CustomData_add_layer(&tmp.vdata, CD_MVERT, CD_ASSIGN,
		                     (alloctype == CD_ASSIGN) ? dm->getVertArray(dm) : dm->dupVertArray(dm),
		                     totvert);
	}
	if (!CustomData_has_layer(&tmp.edata, CD_MEDGE)) {
		CustomData_add_layer(&tmp.edata, CD_MEDGE, CD_ASSIGN,
		                     (alloctype == CD_ASSIGN) ? dm->getEdgeArray(dm) : dm->dupEdgeArray(dm),
		                     totedge);
	}
	if (!CustomData_has_layer(&tmp.pdata, CD_MPOLY)) {
		tmp.mloop = (alloctype == CD_ASSIGN) ? dm->getLoopArray(dm) : dm->dupLoopArray(dm);
		tmp.mpoly = (alloctype == CD_ASSIGN) ? dm->getPolyArray(dm) : dm->dupPolyArray(dm);

		CustomData_add_layer(&tmp.ldata, CD_MLOOP, CD_ASSIGN, tmp.mloop, tmp.totloop);
		CustomData_add_layer(&tmp.pdata, CD_MPOLY, CD_ASSIGN, tmp.mpoly, tmp.totpoly);
	}

	/* object had got displacement layer, should copy this layer to save sculpted data */
	/* NOTE: maybe some other layers should be copied? nazgul */
	if (CustomData_has_layer(&me->ldata, CD_MDISPS)) {
		if (totloop == me->totloop) {
			MDisps *mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
			CustomData_add_layer(&tmp.ldata, CD_MDISPS, alloctype, mdisps, totloop);
		}
	}

	/* yes, must be before _and_ after tessellate */
	BKE_mesh_update_customdata_pointers(&tmp, false);

	/* since 2.65 caller must do! */
	// BKE_mesh_tessface_calc(&tmp);

	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->fdata, me->totface);
	CustomData_free(&me->ldata, me->totloop);
	CustomData_free(&me->pdata, me->totpoly);

	/* ok, this should now use new CD shapekey data,
	 * which should be fed through the modifier
	 * stack */
	if (tmp.totvert != me->totvert && !did_shapekeys && me->key) {
		printf("%s: YEEK! this should be recoded! Shape key loss!: ID '%s'\n", __func__, tmp.id.name);
		if (tmp.key && !(tmp.id.tag & LIB_TAG_NO_MAIN)) {
			id_us_min(&tmp.key->id);
		}
		tmp.key = NULL;
	}

	/* Clear selection history */
	MEM_SAFE_FREE(tmp.mselect);
	tmp.totselect = 0;
	BLI_assert(ELEM(tmp.bb, NULL, me->bb));
	if (me->bb) {
		MEM_freeN(me->bb);
		tmp.bb = NULL;
	}

	/* skip the listbase */
	MEMCPY_STRUCT_OFS(me, &tmp, id.prev);

	if (take_ownership) {
		if (alloctype == CD_ASSIGN) {
			CustomData_free_typemask(&dm->vertData, dm->numVertData, ~mask);
			CustomData_free_typemask(&dm->edgeData, dm->numEdgeData, ~mask);
			CustomData_free_typemask(&dm->loopData, dm->numLoopData, ~mask);
			CustomData_free_typemask(&dm->polyData, dm->numPolyData, ~mask);
		}
		dm->release(dm);
	}
}

/** Utility function to convert an (evaluated) Mesh to a shape key block. */
/* Just a shallow wrapper around BKE_keyblock_convert_from_mesh,
 * that ensures both evaluated mesh and original one has same number of vertices. */
void BKE_mesh_runtime_eval_to_meshkey(Mesh *me_deformed, Mesh *me, KeyBlock *kb)
{
	const int totvert = me_deformed->totvert;

	if (totvert == 0 || me->totvert == 0 || me->totvert != totvert) {
		return;
	}

	BKE_keyblock_convert_from_mesh(me_deformed, me->key, kb);
}

/**
 * set the CD_FLAG_NOCOPY flag in custom data layers where the mask is
 * zero for the layer type, so only layer types specified by the mask
 * will be copied
 */
void DM_set_only_copy(DerivedMesh *dm, CustomDataMask mask)
{
	CustomData_set_only_copy(&dm->vertData, mask);
	CustomData_set_only_copy(&dm->edgeData, mask);
	CustomData_set_only_copy(&dm->faceData, mask);
	/* this wasn't in 2.63 and is disabled for 2.64 because it gives problems with
	 * weight paint mode when there are modifiers applied, needs further investigation,
	 * see replies to r50969, Campbell */
#if 0
	CustomData_set_only_copy(&dm->loopData, mask);
	CustomData_set_only_copy(&dm->polyData, mask);
#endif
}

static void mesh_set_only_copy(Mesh *mesh, CustomDataMask mask)
{
	CustomData_set_only_copy(&mesh->vdata, mask);
	CustomData_set_only_copy(&mesh->edata, mask);
	CustomData_set_only_copy(&mesh->fdata, mask);
	/* this wasn't in 2.63 and is disabled for 2.64 because it gives problems with
	 * weight paint mode when there are modifiers applied, needs further investigation,
	 * see replies to r50969, Campbell */
#if 0
	CustomData_set_only_copy(&mesh->ldata, mask);
	CustomData_set_only_copy(&mesh->pdata, mask);
#endif
}

void DM_add_vert_layer(DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer)
{
	CustomData_add_layer(&dm->vertData, type, alloctype, layer, dm->numVertData);
}

void DM_add_edge_layer(DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer)
{
	CustomData_add_layer(&dm->edgeData, type, alloctype, layer, dm->numEdgeData);
}

void DM_add_tessface_layer(DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer)
{
	CustomData_add_layer(&dm->faceData, type, alloctype, layer, dm->numTessFaceData);
}

void DM_add_loop_layer(DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer)
{
	CustomData_add_layer(&dm->loopData, type, alloctype, layer, dm->numLoopData);
}

void DM_add_poly_layer(DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer)
{
	CustomData_add_layer(&dm->polyData, type, alloctype, layer, dm->numPolyData);
}

void *DM_get_vert_data(DerivedMesh *dm, int index, int type)
{
	BLI_assert(index >= 0 && index < dm->getNumVerts(dm));
	return CustomData_get(&dm->vertData, index, type);
}

void *DM_get_edge_data(DerivedMesh *dm, int index, int type)
{
	BLI_assert(index >= 0 && index < dm->getNumEdges(dm));
	return CustomData_get(&dm->edgeData, index, type);
}

void *DM_get_tessface_data(DerivedMesh *dm, int index, int type)
{
	BLI_assert(index >= 0 && index < dm->getNumTessFaces(dm));
	return CustomData_get(&dm->faceData, index, type);
}

void *DM_get_poly_data(DerivedMesh *dm, int index, int type)
{
	BLI_assert(index >= 0 && index < dm->getNumPolys(dm));
	return CustomData_get(&dm->polyData, index, type);
}


void *DM_get_vert_data_layer(DerivedMesh *dm, int type)
{
	if (type == CD_MVERT)
		return dm->getVertArray(dm);

	return CustomData_get_layer(&dm->vertData, type);
}

void *DM_get_edge_data_layer(DerivedMesh *dm, int type)
{
	if (type == CD_MEDGE)
		return dm->getEdgeArray(dm);

	return CustomData_get_layer(&dm->edgeData, type);
}

void *DM_get_tessface_data_layer(DerivedMesh *dm, int type)
{
	if (type == CD_MFACE)
		return dm->getTessFaceArray(dm);

	return CustomData_get_layer(&dm->faceData, type);
}

void *DM_get_poly_data_layer(DerivedMesh *dm, int type)
{
	return CustomData_get_layer(&dm->polyData, type);
}

void *DM_get_loop_data_layer(DerivedMesh *dm, int type)
{
	return CustomData_get_layer(&dm->loopData, type);
}

void DM_set_vert_data(DerivedMesh *dm, int index, int type, void *data)
{
	CustomData_set(&dm->vertData, index, type, data);
}

void DM_set_edge_data(DerivedMesh *dm, int index, int type, void *data)
{
	CustomData_set(&dm->edgeData, index, type, data);
}

void DM_set_tessface_data(DerivedMesh *dm, int index, int type, void *data)
{
	CustomData_set(&dm->faceData, index, type, data);
}

void DM_copy_vert_data(DerivedMesh *source, DerivedMesh *dest,
                       int source_index, int dest_index, int count)
{
	CustomData_copy_data(&source->vertData, &dest->vertData,
	                     source_index, dest_index, count);
}

void DM_copy_edge_data(DerivedMesh *source, DerivedMesh *dest,
                       int source_index, int dest_index, int count)
{
	CustomData_copy_data(&source->edgeData, &dest->edgeData,
	                     source_index, dest_index, count);
}

void DM_copy_tessface_data(DerivedMesh *source, DerivedMesh *dest,
                           int source_index, int dest_index, int count)
{
	CustomData_copy_data(&source->faceData, &dest->faceData,
	                     source_index, dest_index, count);
}

void DM_copy_loop_data(DerivedMesh *source, DerivedMesh *dest,
                       int source_index, int dest_index, int count)
{
	CustomData_copy_data(&source->loopData, &dest->loopData,
	                     source_index, dest_index, count);
}

void DM_copy_poly_data(DerivedMesh *source, DerivedMesh *dest,
                       int source_index, int dest_index, int count)
{
	CustomData_copy_data(&source->polyData, &dest->polyData,
	                     source_index, dest_index, count);
}

void DM_free_vert_data(struct DerivedMesh *dm, int index, int count)
{
	CustomData_free_elem(&dm->vertData, index, count);
}

void DM_free_edge_data(struct DerivedMesh *dm, int index, int count)
{
	CustomData_free_elem(&dm->edgeData, index, count);
}

void DM_free_tessface_data(struct DerivedMesh *dm, int index, int count)
{
	CustomData_free_elem(&dm->faceData, index, count);
}

void DM_free_loop_data(struct DerivedMesh *dm, int index, int count)
{
	CustomData_free_elem(&dm->loopData, index, count);
}

void DM_free_poly_data(struct DerivedMesh *dm, int index, int count)
{
	CustomData_free_elem(&dm->polyData, index, count);
}

/**
 * interpolates vertex data from the vertices indexed by src_indices in the
 * source mesh using the given weights and stores the result in the vertex
 * indexed by dest_index in the dest mesh
 */
void DM_interp_vert_data(
        DerivedMesh *source, DerivedMesh *dest,
        int *src_indices, float *weights,
        int count, int dest_index)
{
	CustomData_interp(&source->vertData, &dest->vertData, src_indices,
	                  weights, NULL, count, dest_index);
}

/**
 * interpolates edge data from the edges indexed by src_indices in the
 * source mesh using the given weights and stores the result in the edge indexed
 * by dest_index in the dest mesh.
 * if weights is NULL, all weights default to 1.
 * if vert_weights is non-NULL, any per-vertex edge data is interpolated using
 * vert_weights[i] multiplied by weights[i].
 */
void DM_interp_edge_data(
        DerivedMesh *source, DerivedMesh *dest,
        int *src_indices,
        float *weights, EdgeVertWeight *vert_weights,
        int count, int dest_index)
{
	CustomData_interp(&source->edgeData, &dest->edgeData, src_indices,
	                  weights, (float *)vert_weights, count, dest_index);
}

/**
 * interpolates face data from the faces indexed by src_indices in the
 * source mesh using the given weights and stores the result in the face indexed
 * by dest_index in the dest mesh.
 * if weights is NULL, all weights default to 1.
 * if vert_weights is non-NULL, any per-vertex face data is interpolated using
 * vert_weights[i] multiplied by weights[i].
 */
void DM_interp_tessface_data(
        DerivedMesh *source, DerivedMesh *dest,
        int *src_indices,
        float *weights, FaceVertWeight *vert_weights,
        int count, int dest_index)
{
	CustomData_interp(&source->faceData, &dest->faceData, src_indices,
	                  weights, (float *)vert_weights, count, dest_index);
}

void DM_swap_tessface_data(DerivedMesh *dm, int index, const int *corner_indices)
{
	CustomData_swap_corners(&dm->faceData, index, corner_indices);
}

void DM_interp_loop_data(
        DerivedMesh *source, DerivedMesh *dest,
        int *src_indices,
        float *weights, int count, int dest_index)
{
	CustomData_interp(&source->loopData, &dest->loopData, src_indices,
	                  weights, NULL, count, dest_index);
}

void DM_interp_poly_data(
        DerivedMesh *source, DerivedMesh *dest,
        int *src_indices,
        float *weights, int count, int dest_index)
{
	CustomData_interp(&source->polyData, &dest->polyData, src_indices,
	                  weights, NULL, count, dest_index);
}

DerivedMesh *mesh_create_derived(Mesh *me, float (*vertCos)[3])
{
	DerivedMesh *dm = CDDM_from_mesh(me);

	if (!dm)
		return NULL;

	if (vertCos) {
		CDDM_apply_vert_coords(dm, vertCos);
	}

	return dm;
}

/* XXX2.8(Sybren): can be removed once DerivedMesh port is done */
#ifdef WITH_DERIVEDMESH_DEPRECATED_FUNCS
DerivedMesh *mesh_create_derived_for_modifier(
        struct Depsgraph *depsgraph, Scene *scene, Object *ob,
        ModifierData *md, int build_shapekey_layers)
{
	Mesh *me = ob->data;
	const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	DerivedMesh *dm;
	KeyBlock *kb;
	ModifierEvalContext mectx = {depsgraph, ob, 0};

	md->scene = scene;

	if (!(md->mode & eModifierMode_Realtime)) {
		return NULL;
	}

	if (mti->isDisabled && mti->isDisabled(md, 0)) {
		return NULL;
	}

	if (build_shapekey_layers && me->key && (kb = BLI_findlink(&me->key->block, ob->shapenr - 1))) {
		BKE_keyblock_convert_to_mesh(kb, me);
	}

	if (mti->type == eModifierTypeType_OnlyDeform) {
		int numVerts;
		float (*deformedVerts)[3] = BKE_mesh_vertexCos_get(me, &numVerts);

		modwrap_deformVerts(md, &mectx, NULL, deformedVerts, numVerts);
		dm = mesh_create_derived(me, deformedVerts);

		if (build_shapekey_layers)
			add_shapekey_layers(dm, me, ob);

		MEM_freeN(deformedVerts);
	}
	else {
		DerivedMesh *tdm = mesh_create_derived(me, NULL);

		if (build_shapekey_layers)
			add_shapekey_layers(tdm, me, ob);

		dm = modwrap_applyModifier(md, &mectx, tdm);
		ASSERT_IS_VALID_DM(dm);

		if (tdm != dm) tdm->release(tdm);
	}

	return dm;
}
#endif

static float (*get_editbmesh_orco_verts(BMEditMesh *em))[3]
{
	BMIter iter;
	BMVert *eve;
	float (*orco)[3];
	int i;

	/* these may not really be the orco's, but it's only for preview.
	 * could be solver better once, but isn't simple */

	orco = MEM_malloc_arrayN(em->bm->totvert, sizeof(float) * 3, "BMEditMesh Orco");

	BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
		copy_v3_v3(orco[i], eve->co);
	}

	return orco;
}

/* orco custom data layer */
static float (*get_orco_coords_dm(Object *ob, BMEditMesh *em, int layer, int *free))[3]
{
	*free = 0;

	if (layer == CD_ORCO) {
		/* get original coordinates */
		*free = 1;

		if (em)
			return get_editbmesh_orco_verts(em);
		else
			return BKE_mesh_orco_verts_get(ob);
	}
	else if (layer == CD_CLOTH_ORCO) {
		/* apply shape key for cloth, this should really be solved
		 * by a more flexible customdata system, but not simple */
		if (!em) {
			ClothModifierData *clmd = (ClothModifierData *)modifiers_findByType(ob, eModifierType_Cloth);
			KeyBlock *kb = BKE_keyblock_from_key(BKE_key_from_object(ob), clmd->sim_parms->shapekey_rest);

			if (kb && kb->data) {
				return kb->data;
			}
		}

		return NULL;
	}

	return NULL;
}

static DerivedMesh *create_orco_dm(Object *ob, Mesh *me, BMEditMesh *em, int layer)
{
	DerivedMesh *dm;
	float (*orco)[3];
	int free;

	if (em) {
		dm = CDDM_from_editbmesh(em, false, false);
	}
	else {
		dm = CDDM_from_mesh(me);
	}

	orco = get_orco_coords_dm(ob, em, layer, &free);

	if (orco) {
		CDDM_apply_vert_coords(dm, orco);
		if (free) MEM_freeN(orco);
	}

	return dm;
}

static Mesh *create_orco_mesh(Object *ob, Mesh *me, BMEditMesh *em, int layer)
{
	Mesh *mesh;
	float (*orco)[3];
	int free;

	if (em) {
		mesh = BKE_bmesh_to_mesh_nomain(em->bm, &(struct BMeshToMeshParams){0});
	}
	else {
		BKE_id_copy_ex(
		        NULL, &me->id, (ID **)&mesh,
		        (LIB_ID_CREATE_NO_MAIN |
		         LIB_ID_CREATE_NO_USER_REFCOUNT |
		         LIB_ID_CREATE_NO_DEG_TAG |
		         LIB_ID_COPY_CD_REFERENCE),
		        false);
	}

	orco = get_orco_coords_dm(ob, em, layer, &free);

	if (orco) {
		BKE_mesh_apply_vert_coords(mesh, orco);
		if (free) MEM_freeN(orco);
	}

	return mesh;
}

static void add_orco_dm(
        Object *ob, BMEditMesh *em, DerivedMesh *dm,
        DerivedMesh *orcodm, int layer)
{
	float (*orco)[3], (*layerorco)[3];
	int totvert, free;

	totvert = dm->getNumVerts(dm);

	if (orcodm) {
		orco = MEM_calloc_arrayN(totvert, sizeof(float[3]), "dm orco");
		free = 1;

		if (orcodm->getNumVerts(orcodm) == totvert)
			orcodm->getVertCos(orcodm, orco);
		else
			dm->getVertCos(dm, orco);
	}
	else {
		/* TODO(sybren): totvert should potentially change here, as ob->data
		 * or em may have a different number of vertices than dm. */
		orco = get_orco_coords_dm(ob, em, layer, &free);
	}

	if (orco) {
		if (layer == CD_ORCO)
			BKE_mesh_orco_verts_transform(ob->data, orco, totvert, 0);

		if (!(layerorco = DM_get_vert_data_layer(dm, layer))) {
			DM_add_vert_layer(dm, layer, CD_CALLOC, NULL);
			layerorco = DM_get_vert_data_layer(dm, layer);
		}

		memcpy(layerorco, orco, sizeof(float) * 3 * totvert);
		if (free) MEM_freeN(orco);
	}
}

static void add_orco_mesh(
        Object *ob, BMEditMesh *em, Mesh *mesh,
        Mesh *orco_mesh, int layer)
{
	float (*orco)[3], (*layerorco)[3];
	int totvert, free;

	totvert = mesh->totvert;

	if (orco_mesh) {
		free = 1;

		if (orco_mesh->totvert == totvert) {
			orco = BKE_mesh_vertexCos_get(orco_mesh, NULL);
		}
		else {
			orco = BKE_mesh_vertexCos_get(mesh, NULL);
		}
	}
	else {
		/* TODO(sybren): totvert should potentially change here, as ob->data
		 * or em may have a different number of vertices than dm. */
		orco = get_orco_coords_dm(ob, em, layer, &free);
	}

	if (orco) {
		if (layer == CD_ORCO) {
			BKE_mesh_orco_verts_transform(ob->data, orco, totvert, 0);
		}

		if (!(layerorco = CustomData_get_layer(&mesh->vdata, layer))) {
			CustomData_add_layer(&mesh->vdata, layer, CD_CALLOC, NULL, mesh->totvert);
			BKE_mesh_update_customdata_pointers(mesh, false);

			layerorco = CustomData_get_layer(&mesh->vdata, layer);
		}

		memcpy(layerorco, orco, sizeof(float) * 3 * totvert);
		if (free) MEM_freeN(orco);
	}
}

/* weight paint colors */

/* Something of a hack, at the moment deal with weightpaint
 * by tucking into colors during modifier eval, only in
 * wpaint mode. Works ok but need to make sure recalc
 * happens on enter/exit wpaint.
 */

/* draw_flag's for calc_weightpaint_vert_color */
enum {
	/* only one of these should be set, keep first (for easy bit-shifting) */
	CALC_WP_GROUP_USER_ACTIVE   = (1 << 1),
	CALC_WP_GROUP_USER_ALL      = (1 << 2),

	CALC_WP_MULTIPAINT          = (1 << 3),
	CALC_WP_AUTO_NORMALIZE      = (1 << 4),
	CALC_WP_MIRROR_X            = (1 << 5),
};

typedef struct MERuntimeWeightColorInfo {
	const ColorBand *coba;
	const char *alert_color;
} MERuntimeWeightColorInfo;


static int dm_drawflag_calc(const ToolSettings *ts, const Mesh *me)
{
	return ((ts->multipaint ? CALC_WP_MULTIPAINT : 0) |
	        /* CALC_WP_GROUP_USER_ACTIVE or CALC_WP_GROUP_USER_ALL */
	        (1 << ts->weightuser) |
	        (ts->auto_normalize ? CALC_WP_AUTO_NORMALIZE : 0) |
	        ((me->editflag & ME_EDIT_MIRROR_X) ? CALC_WP_MIRROR_X : 0));
}

static void weightpaint_color(unsigned char r_col[4], MERuntimeWeightColorInfo *dm_wcinfo, const float input)
{
	float colf[4];

	if (dm_wcinfo && dm_wcinfo->coba) {
		BKE_colorband_evaluate(dm_wcinfo->coba, input, colf);
	}
	else {
		BKE_defvert_weight_to_rgb(colf, input);
	}

	/* don't use rgb_float_to_uchar() here because
	 * the resulting float doesn't need 0-1 clamp check */
	r_col[0] = (unsigned char)(colf[0] * 255.0f);
	r_col[1] = (unsigned char)(colf[1] * 255.0f);
	r_col[2] = (unsigned char)(colf[2] * 255.0f);
	r_col[3] = 255;
}


static void calc_weightpaint_vert_color(
        unsigned char r_col[4],
        const MDeformVert *dv,
        MERuntimeWeightColorInfo *dm_wcinfo,
        const int defbase_tot, const int defbase_act,
        const bool *defbase_sel, const int defbase_sel_tot,
        const int draw_flag)
{
	float input = 0.0f;

	bool show_alert_color = false;

	if ((defbase_sel_tot > 1) && (draw_flag & CALC_WP_MULTIPAINT)) {
		/* Multi-Paint feature */
		input = BKE_defvert_multipaint_collective_weight(
		        dv, defbase_tot, defbase_sel, defbase_sel_tot, (draw_flag & CALC_WP_AUTO_NORMALIZE) != 0);

		/* make it black if the selected groups have no weight on a vertex */
		if (input == 0.0f) {
			show_alert_color = true;
		}
	}
	else {
		/* default, non tricky behavior */
		input = defvert_find_weight(dv, defbase_act);

		if (draw_flag & CALC_WP_GROUP_USER_ACTIVE) {
			if (input == 0.0f) {
				show_alert_color = true;
			}
		}
		else if (draw_flag & CALC_WP_GROUP_USER_ALL) {
			if (input == 0.0f) {
				show_alert_color = defvert_is_weight_zero(dv, defbase_tot);
			}
		}
	}

	if (show_alert_color == false) {
		CLAMP(input, 0.0f, 1.0f);
		weightpaint_color(r_col, dm_wcinfo, input);
	}
	else {
		copy_v3_v3_char((char *)r_col, dm_wcinfo->alert_color);
		r_col[3] = 255;
	}
}

static MERuntimeWeightColorInfo G_me_runtime_wcinfo;

void BKE_mesh_runtime_color_band_store(const ColorBand *coba, const char alert_color[4])
{
	G_me_runtime_wcinfo.coba        = coba;
	G_me_runtime_wcinfo.alert_color = alert_color;
}

/**
 * return an array of vertex weight colors, caller must free.
 *
 * \note that we could save some memory and allocate RGB only but then we'd need to
 * re-arrange the colors when copying to the face since MCol has odd ordering,
 * so leave this as is - campbell
 */
static void calc_weightpaint_vert_array(
        Object *ob, DerivedMesh *dm, int const draw_flag, MERuntimeWeightColorInfo *dm_wcinfo,
        unsigned char (*r_wtcol_v)[4])
{
	BMEditMesh *em = (dm->type == DM_TYPE_EDITBMESH) ? BKE_editmesh_from_object(ob) : NULL;
	const int numVerts = dm->getNumVerts(dm);

	if ((ob->actdef != 0) &&
	    (CustomData_has_layer(em ? &em->bm->vdata : &dm->vertData, CD_MDEFORMVERT)))
	{
		unsigned char (*wc)[4] = r_wtcol_v;
		unsigned int i;

		/* variables for multipaint */
		const int defbase_tot = BLI_listbase_count(&ob->defbase);
		const int defbase_act = ob->actdef - 1;

		int defbase_sel_tot = 0;
		bool *defbase_sel = NULL;

		if (draw_flag & CALC_WP_MULTIPAINT) {
			defbase_sel = BKE_object_defgroup_selected_get(ob, defbase_tot, &defbase_sel_tot);

			if (defbase_sel_tot > 1 && (draw_flag & CALC_WP_MIRROR_X)) {
				BKE_object_defgroup_mirror_selection(ob, defbase_tot, defbase_sel, defbase_sel, &defbase_sel_tot);
			}
		}

		/* editmesh won't have deform verts unless modifiers require it,
		 * avoid having to create an array of deform-verts only for drawing
		 * by reading from the bmesh directly. */
		if (em) {
			BMIter iter;
			BMVert *eve;
			const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
			BLI_assert(cd_dvert_offset != -1);

			BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
				const MDeformVert *dv = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
				calc_weightpaint_vert_color(
				        (unsigned char *)wc, dv, dm_wcinfo,
				        defbase_tot, defbase_act, defbase_sel, defbase_sel_tot, draw_flag);
				wc++;
			}
		}
		else {
			const MDeformVert *dv = DM_get_vert_data_layer(dm, CD_MDEFORMVERT);
			for (i = numVerts; i != 0; i--, wc++, dv++) {
				calc_weightpaint_vert_color(
				        (unsigned char *)wc, dv, dm_wcinfo,
				        defbase_tot, defbase_act, defbase_sel, defbase_sel_tot, draw_flag);
			}
		}

		if (defbase_sel) {
			MEM_freeN(defbase_sel);
		}
	}
	else {
		unsigned char col[4];
		if ((ob->actdef == 0) && !BLI_listbase_is_empty(&ob->defbase)) {
			/* color-code for missing data (full brightness isn't easy on the eye). */
			ARRAY_SET_ITEMS(col, 0xa0, 0, 0xa0, 0xff);
		}
		else if (draw_flag & (CALC_WP_GROUP_USER_ACTIVE | CALC_WP_GROUP_USER_ALL)) {
			copy_v3_v3_char((char *)col, dm_wcinfo->alert_color);
			col[3] = 255;
		}
		else {
			weightpaint_color(col, dm_wcinfo, 0.0f);
		}
		copy_vn_i((int *)r_wtcol_v, numVerts, *((int *)col));
	}
}

static void calc_weightpaint_vert_array_mesh(
        Object *ob, Mesh *mesh, int const draw_flag, MERuntimeWeightColorInfo *dm_wcinfo,
        unsigned char (*r_wtcol_v)[4])
{
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	const int numVerts = mesh->totvert;

	if ((ob->actdef != 0) &&
	    (CustomData_has_layer(em ? &em->bm->vdata : &mesh->vdata, CD_MDEFORMVERT)))
	{
		unsigned char (*wc)[4] = r_wtcol_v;
		unsigned int i;

		/* variables for multipaint */
		const int defbase_tot = BLI_listbase_count(&ob->defbase);
		const int defbase_act = ob->actdef - 1;

		int defbase_sel_tot = 0;
		bool *defbase_sel = NULL;

		if (draw_flag & CALC_WP_MULTIPAINT) {
			defbase_sel = BKE_object_defgroup_selected_get(ob, defbase_tot, &defbase_sel_tot);

			if (defbase_sel_tot > 1 && (draw_flag & CALC_WP_MIRROR_X)) {
				BKE_object_defgroup_mirror_selection(ob, defbase_tot, defbase_sel, defbase_sel, &defbase_sel_tot);
			}
		}

		/* editmesh won't have deform verts unless modifiers require it,
		 * avoid having to create an array of deform-verts only for drawing
		 * by reading from the bmesh directly. */
		if (em) {
			BMIter iter;
			BMVert *eve;
			const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
			BLI_assert(cd_dvert_offset != -1);

			BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
				const MDeformVert *dv = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
				calc_weightpaint_vert_color(
				        (unsigned char *)wc, dv, dm_wcinfo,
				        defbase_tot, defbase_act, defbase_sel, defbase_sel_tot, draw_flag);
				wc++;
			}
		}
		else {
			const MDeformVert *dv = CustomData_get_layer(&mesh->vdata, CD_MDEFORMVERT);
			for (i = numVerts; i != 0; i--, wc++, dv++) {
				calc_weightpaint_vert_color(
				        (unsigned char *)wc, dv, dm_wcinfo,
				        defbase_tot, defbase_act, defbase_sel, defbase_sel_tot, draw_flag);
			}
		}

		if (defbase_sel) {
			MEM_freeN(defbase_sel);
		}
	}
	else {
		unsigned char col[4];
		if ((ob->actdef == 0) && !BLI_listbase_is_empty(&ob->defbase)) {
			/* color-code for missing data (full brightness isn't easy on the eye). */
			ARRAY_SET_ITEMS(col, 0xa0, 0, 0xa0, 0xff);
		}
		else if (draw_flag & (CALC_WP_GROUP_USER_ACTIVE | CALC_WP_GROUP_USER_ALL)) {
			copy_v3_v3_char((char *)col, dm_wcinfo->alert_color);
			col[3] = 255;
		}
		else {
			weightpaint_color(col, dm_wcinfo, 0.0f);
		}
		copy_vn_i((int *)r_wtcol_v, numVerts, *((int *)col));
	}
}

/** return an array of vertex weight colors from given weights, caller must free.
 *
 * \note that we could save some memory and allocate RGB only but then we'd need to
 * re-arrange the colors when copying to the face since MCol has odd ordering,
 * so leave this as is - campbell
 */
static void calc_colors_from_weights_array(
        const int num, const float *weights,
        unsigned char (*r_wtcol_v)[4])
{
	unsigned char (*wc)[4] = r_wtcol_v;
	int i;

	for (i = 0; i < num; i++, wc++, weights++) {
		weightpaint_color((unsigned char *)wc, NULL, *weights);
	}
}

void DM_update_weight_mcol(
        Object *ob, DerivedMesh *dm, int const draw_flag,
        float *weights, int num, const int *indices)
{
	BMEditMesh *em = (dm->type == DM_TYPE_EDITBMESH) ? BKE_editmesh_from_object(ob) : NULL;
	unsigned char (*wtcol_v)[4];
	int numVerts = dm->getNumVerts(dm);
	int i;

	if (em) {
		BKE_editmesh_color_ensure(em, BM_VERT);
		wtcol_v = em->derivedVertColor;
	}
	else {
		wtcol_v = MEM_malloc_arrayN(numVerts, sizeof(*wtcol_v), __func__);
	}

	/* Weights are given by caller. */
	if (weights) {
		float *w = weights;
		/* If indices is not NULL, it means we do not have weights for all vertices,
		 * so we must create them (and set them to zero)... */
		if (indices) {
			w = MEM_calloc_arrayN(numVerts, sizeof(float), "Temp weight array DM_update_weight_mcol");
			i = num;
			while (i--)
				w[indices[i]] = weights[i];
		}

		/* Convert float weights to colors. */
		calc_colors_from_weights_array(numVerts, w, wtcol_v);

		if (indices)
			MEM_freeN(w);
	}
	else {
		/* No weights given, take them from active vgroup(s). */
		calc_weightpaint_vert_array(ob, dm, draw_flag, &G_me_runtime_wcinfo, wtcol_v);
	}

	if (dm->type == DM_TYPE_EDITBMESH) {
		/* editmesh draw function checks specifically for this */
	}
	else {
		const int dm_totpoly = dm->getNumPolys(dm);
		const int dm_totloop = dm->getNumLoops(dm);
		unsigned char(*wtcol_l)[4] = CustomData_get_layer(dm->getLoopDataLayout(dm), CD_PREVIEW_MLOOPCOL);
		MLoop *mloop = dm->getLoopArray(dm), *ml;
		MPoly *mp = dm->getPolyArray(dm);
		int l_index;
		int j;

		/* now add to loops, so the data can be passed through the modifier stack
		 * If no CD_PREVIEW_MLOOPCOL existed yet, we have to add a new one! */
		if (!wtcol_l) {
			wtcol_l = MEM_malloc_arrayN(dm_totloop, sizeof(*wtcol_l), __func__);
			CustomData_add_layer(&dm->loopData, CD_PREVIEW_MLOOPCOL, CD_ASSIGN, wtcol_l, dm_totloop);
		}

		l_index = 0;
		for (i = 0; i < dm_totpoly; i++, mp++) {
			ml = mloop + mp->loopstart;

			for (j = 0; j < mp->totloop; j++, ml++, l_index++) {
				copy_v4_v4_uchar(&wtcol_l[l_index][0],
				                 &wtcol_v[ml->v][0]);
			}
		}
		MEM_freeN(wtcol_v);

		dm->dirty |= DM_DIRTY_TESS_CDLAYERS;
	}
}

static void mesh_update_weight_mcol(
        Object *ob, Mesh *mesh, int const draw_flag,
        float *weights, int num, const int *indices)
{
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	unsigned char (*wtcol_v)[4];
	int numVerts = mesh->totvert;
	int i;

	if (em) {
		BKE_editmesh_color_ensure(em, BM_VERT);
		wtcol_v = em->derivedVertColor;
	}
	else {
		wtcol_v = MEM_malloc_arrayN(numVerts, sizeof(*wtcol_v), __func__);
	}

	/* Weights are given by caller. */
	if (weights) {
		float *w = weights;
		/* If indices is not NULL, it means we do not have weights for all vertices,
		 * so we must create them (and set them to zero)... */
		if (indices) {
			w = MEM_calloc_arrayN(numVerts, sizeof(float), "Temp weight array DM_update_weight_mcol");
			i = num;
			while (i--)
				w[indices[i]] = weights[i];
		}

		/* Convert float weights to colors. */
		calc_colors_from_weights_array(numVerts, w, wtcol_v);

		if (indices)
			MEM_freeN(w);
	}
	else {
		/* No weights given, take them from active vgroup(s). */
		calc_weightpaint_vert_array_mesh(ob, mesh, draw_flag, &G_me_runtime_wcinfo, wtcol_v);
	}

	if (em) {
		/* editmesh draw function checks specifically for this */
	}
	else {
		const int totpoly = mesh->totpoly;
		const int totloop = mesh->totloop;
		unsigned char(*wtcol_l)[4] = CustomData_get_layer(&mesh->ldata, CD_PREVIEW_MLOOPCOL);
		MLoop *mloop = mesh->mloop, *ml;
		MPoly *mp = mesh->mpoly;
		int l_index;
		int j;

		/* now add to loops, so the data can be passed through the modifier stack
		 * If no CD_PREVIEW_MLOOPCOL existed yet, we have to add a new one! */
		if (!wtcol_l) {
			wtcol_l = MEM_malloc_arrayN(totloop, sizeof(*wtcol_l), __func__);
			CustomData_add_layer(&mesh->ldata, CD_PREVIEW_MLOOPCOL, CD_ASSIGN, wtcol_l, totloop);
		}

		l_index = 0;
		for (i = 0; i < totpoly; i++, mp++) {
			ml = mloop + mp->loopstart;

			for (j = 0; j < mp->totloop; j++, ml++, l_index++) {
				copy_v4_v4_uchar(&wtcol_l[l_index][0],
				                 &wtcol_v[ml->v][0]);
			}
		}
		MEM_freeN(wtcol_v);

		BKE_mesh_tessface_clear(mesh);
	}
}

static void DM_update_statvis_color(const Scene *scene, Object *ob, DerivedMesh *dm)
{
	BMEditMesh *em = BKE_editmesh_from_object(ob);

	BKE_editmesh_statvis_calc(em, dm, &scene->toolsettings->statvis);
}

static void shapekey_layers_to_keyblocks(DerivedMesh *dm, Mesh *me, int actshape_uid)
{
	KeyBlock *kb;
	int i, j, tot;

	if (!me->key)
		return;

	tot = CustomData_number_of_layers(&dm->vertData, CD_SHAPEKEY);
	for (i = 0; i < tot; i++) {
		CustomDataLayer *layer = &dm->vertData.layers[CustomData_get_layer_index_n(&dm->vertData, CD_SHAPEKEY, i)];
		float (*cos)[3], (*kbcos)[3];

		for (kb = me->key->block.first; kb; kb = kb->next) {
			if (kb->uid == layer->uid)
				break;
		}

		if (!kb) {
			kb = BKE_keyblock_add(me->key, layer->name);
			kb->uid = layer->uid;
		}

		if (kb->data)
			MEM_freeN(kb->data);

		cos = CustomData_get_layer_n(&dm->vertData, CD_SHAPEKEY, i);
		kb->totelem = dm->numVertData;

		kb->data = kbcos = MEM_malloc_arrayN(kb->totelem, 3 * sizeof(float), "kbcos DerivedMesh.c");
		if (kb->uid == actshape_uid) {
			MVert *mvert = dm->getVertArray(dm);

			for (j = 0; j < dm->numVertData; j++, kbcos++, mvert++) {
				copy_v3_v3(*kbcos, mvert->co);
			}
		}
		else {
			for (j = 0; j < kb->totelem; j++, cos++, kbcos++) {
				copy_v3_v3(*kbcos, *cos);
			}
		}
	}

	for (kb = me->key->block.first; kb; kb = kb->next) {
		if (kb->totelem != dm->numVertData) {
			if (kb->data)
				MEM_freeN(kb->data);

			kb->totelem = dm->numVertData;
			kb->data = MEM_calloc_arrayN(kb->totelem, 3 * sizeof(float), "kb->data derivedmesh.c");
			fprintf(stderr, "%s: lost a shapekey layer: '%s'! (bmesh internal error)\n", __func__, kb->name);
		}
	}
}

static void UNUSED_FUNCTION(add_shapekey_layers)(DerivedMesh *dm, Mesh *me, Object *UNUSED(ob))
{
	KeyBlock *kb;
	Key *key = me->key;
	int i;

	if (!me->key)
		return;

	/* ensure we can use mesh vertex count for derived mesh custom data */
	if (me->totvert != dm->getNumVerts(dm)) {
		fprintf(stderr,
		        "%s: vertex size mismatch (mesh/dm) '%s' (%d != %d)\n",
		        __func__, me->id.name + 2, me->totvert, dm->getNumVerts(dm));
		return;
	}

	for (i = 0, kb = key->block.first; kb; kb = kb->next, i++) {
		int ci;
		float *array;

		if (me->totvert != kb->totelem) {
			fprintf(stderr,
			        "%s: vertex size mismatch (Mesh '%s':%d != KeyBlock '%s':%d)\n",
			        __func__, me->id.name + 2, me->totvert, kb->name, kb->totelem);
			array = MEM_calloc_arrayN((size_t)me->totvert, 3 * sizeof(float), __func__);
		}
		else {
			array = MEM_malloc_arrayN((size_t)me->totvert, 3 * sizeof(float), __func__);
			memcpy(array, kb->data, (size_t)me->totvert * 3 * sizeof(float));
		}

		CustomData_add_layer_named(&dm->vertData, CD_SHAPEKEY, CD_ASSIGN, array, dm->numVertData, kb->name);
		ci = CustomData_get_layer_index_n(&dm->vertData, CD_SHAPEKEY, i);

		dm->vertData.layers[ci].uid = kb->uid;
	}
}

/**
 * Called after calculating all modifiers.
 *
 * \note tessfaces should already be calculated.
 */
static void dm_ensure_display_normals(DerivedMesh *dm)
{
	/* Note: dm *may* have a poly CD_NORMAL layer (generated by a modifier needing poly normals e.g.).
	 *       We do not use it here, though. And it should be tagged as temp!
	 */
	/* BLI_assert((CustomData_has_layer(&dm->polyData, CD_NORMAL) == false)); */

	if ((dm->type == DM_TYPE_CDDM) &&
	    ((dm->dirty & DM_DIRTY_NORMALS) || CustomData_has_layer(&dm->polyData, CD_NORMAL) == false))
	{
		/* if normals are dirty we want to calculate vertex normals too */
		CDDM_calc_normals_mapping_ex(dm, (dm->dirty & DM_DIRTY_NORMALS) ? false : true);
	}
}

static void mesh_ensure_display_normals(Mesh *mesh)
{
	/* Note: mesh *may* have a poly CD_NORMAL layer (generated by a modifier needing poly normals e.g.).
	 *       We do not use it here, though. And it should be tagged as temp!
	 */
	/* BLI_assert((CustomData_has_layer(&mesh->pdata, CD_NORMAL) == false)); */

	if (mesh->runtime.cd_dirty_vert & CD_MASK_NORMAL || !CustomData_has_layer(&mesh->pdata, CD_NORMAL)) {
		float (*face_nors)[3] = NULL;
		face_nors = MEM_malloc_arrayN(mesh->totpoly, sizeof(*face_nors), "face_nors");

		/* if normals are dirty we want to calculate vertex normals too */
		bool only_face_normals = !(mesh->runtime.cd_dirty_vert & CD_MASK_NORMAL);

		/* calculate face normals */
		BKE_mesh_calc_normals_poly(
		        mesh->mvert, NULL, mesh->totvert, mesh->mloop, mesh->mpoly,
		        mesh->totloop, mesh->totpoly, face_nors,
		        only_face_normals);

		CustomData_add_layer(&mesh->pdata, CD_NORMAL, CD_ASSIGN, face_nors, mesh->totpoly);

		mesh->runtime.cd_dirty_vert &= ~CD_MASK_NORMAL;
	}
}

static void mesh_calc_modifiers(
        struct Depsgraph *depsgraph, Scene *scene, Object *ob, float (*inputVertexCos)[3],
        int useDeform,
        const bool need_mapping, CustomDataMask dataMask,
        const int index, const bool useCache, const bool build_shapekey_layers,
        const bool allow_gpu,
        /* return args */
        Mesh **r_deform_mesh, Mesh **r_final_mesh)
{
	Mesh *me = ob->data;
	ModifierData *firstmd, *md, *previewmd = NULL;
	CDMaskLink *datamasks, *curr;
	/* XXX Always copying POLYINDEX, else tessellated data are no more valid! */
	CustomDataMask mask, nextmask, previewmask = 0, append_mask = CD_MASK_ORIGINDEX;
	float (*deformedVerts)[3] = NULL;
	int numVerts = me->totvert;
	const bool useRenderParams = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
	const int required_mode = useRenderParams ? eModifierMode_Render : eModifierMode_Realtime;
	bool isPrevDeform = false;
	MultiresModifierData *mmd = get_multires_modifier(scene, ob, 0);
	const bool has_multires = (mmd && mmd->sculptlvl != 0);
	bool multires_applied = false;
	const bool sculpt_mode = ob->mode & OB_MODE_SCULPT && ob->sculpt && !useRenderParams;
	const bool sculpt_dyntopo = (sculpt_mode && ob->sculpt->bm)  && !useRenderParams;
	const int draw_flag = dm_drawflag_calc(scene->toolsettings, me);

	/* Generic preview only in object mode! */
	const bool do_mod_mcol = (ob->mode == OB_MODE_OBJECT);
#if 0 /* XXX Will re-enable this when we have global mod stack options. */
	const bool do_final_wmcol = (scene->toolsettings->weights_preview == WP_WPREVIEW_FINAL) && do_wmcol;
#endif
	const bool do_final_wmcol = false;
	const bool do_init_wmcol = ((dataMask & CD_MASK_PREVIEW_MLOOPCOL) && (ob->mode & OB_MODE_WEIGHT_PAINT) && !do_final_wmcol);
	/* XXX Same as above... For now, only weights preview in WPaint mode. */
	const bool do_mod_wmcol = do_init_wmcol;

	const bool do_loop_normals = (me->flag & ME_AUTOSMOOTH) != 0;

	VirtualModifierData virtualModifierData;

	ModifierApplyFlag app_flags = useRenderParams ? MOD_APPLY_RENDER : 0;
	ModifierApplyFlag deform_app_flags = app_flags;

	BLI_assert((me->id.tag & LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT) == 0);

	if (useCache)
		app_flags |= MOD_APPLY_USECACHE;
	if (allow_gpu)
		app_flags |= MOD_APPLY_ALLOW_GPU;
	if (useDeform)
		deform_app_flags |= MOD_APPLY_USECACHE;

	/* TODO(sybren): do we really need three context objects? Or do we modify
	 * them on the fly to change the flags where needed? */
	const ModifierEvalContext mectx_deform = {depsgraph, ob, deform_app_flags};
	const ModifierEvalContext mectx_apply = {depsgraph, ob, app_flags};
	const ModifierEvalContext mectx_orco = {depsgraph, ob, (app_flags & ~MOD_APPLY_USECACHE) | MOD_APPLY_ORCO};

	md = firstmd = modifiers_getVirtualModifierList(ob, &virtualModifierData);

	modifiers_clearErrors(ob);

	if (do_mod_wmcol || do_mod_mcol) {
		/* Find the last active modifier generating a preview, or NULL if none. */
		/* XXX Currently, DPaint modifier just ignores this.
		 *     Needs a stupid hack...
		 *     The whole "modifier preview" thing has to be (re?)designed, anyway! */
		previewmd = modifiers_getLastPreview(scene, md, required_mode);

		/* even if the modifier doesn't need the data, to make a preview it may */
		if (previewmd) {
			if (do_mod_wmcol) {
				previewmask = CD_MASK_MDEFORMVERT;
			}
		}
	}

	datamasks = modifiers_calcDataMasks(scene, ob, md, dataMask, required_mode, previewmd, previewmask);
	curr = datamasks;

	if (r_deform_mesh) {
		*r_deform_mesh = NULL;
	}
	*r_final_mesh = NULL;

	if (useDeform) {
		if (inputVertexCos)
			deformedVerts = inputVertexCos;

		/* Apply all leading deforming modifiers */
		for (; md; md = md->next, curr = curr->next) {
			const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

			if (!modifier_isEnabled(scene, md, required_mode)) {
				continue;
			}

			if (useDeform < 0 && mti->dependsOnTime && mti->dependsOnTime(md)) {
				continue;
			}

			if (mti->type == eModifierTypeType_OnlyDeform && !sculpt_dyntopo) {
				if (!deformedVerts)
					deformedVerts = BKE_mesh_vertexCos_get(me, &numVerts);

				modifier_deformVerts_ensure_normals(md, &mectx_deform, NULL, deformedVerts, numVerts);
			}
			else {
				break;
			}

			/* grab modifiers until index i */
			if ((index != -1) && (BLI_findindex(&ob->modifiers, md) >= index))
				break;
		}

		/* Result of all leading deforming modifiers is cached for
		 * places that wish to use the original mesh but with deformed
		 * coordinates (vpaint, etc.)
		 */
		if (r_deform_mesh) {
			BKE_id_copy_ex(
			        NULL, &me->id, (ID **)r_deform_mesh,
			        (LIB_ID_CREATE_NO_MAIN |
			         LIB_ID_CREATE_NO_USER_REFCOUNT |
			         LIB_ID_CREATE_NO_DEG_TAG |
			         LIB_ID_COPY_CD_REFERENCE),
			        false);

			/* XXX: Is build_shapekey_layers ever even true? This should have crashed long ago... */
			BLI_assert(!build_shapekey_layers);
			UNUSED_VARS_NDEBUG(build_shapekey_layers);
			//if (build_shapekey_layers)
			//	add_shapekey_layers(*r_deform_mesh, me, ob);

			if (deformedVerts) {
				BKE_mesh_apply_vert_coords(*r_deform_mesh, deformedVerts);
			}
		}
	}
	else {
		/* default behavior for meshes */
		if (inputVertexCos)
			deformedVerts = inputVertexCos;
		else
			deformedVerts = BKE_mesh_vertexCos_get(me, &numVerts);
	}


	/* Now apply all remaining modifiers. If useDeform is off then skip
	 * OnlyDeform ones.
	 */
	Mesh *mesh = NULL;
	Mesh *orco_mesh = NULL;
	Mesh *cloth_orco_mesh = NULL;

	for (; md; md = md->next, curr = curr->next) {
		const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (!modifier_isEnabled(scene, md, required_mode)) {
			continue;
		}

		if (mti->type == eModifierTypeType_OnlyDeform && !useDeform) {
			continue;
		}

		if ((mti->flags & eModifierTypeFlag_RequiresOriginalData) && mesh) {
			modifier_setError(md, "Modifier requires original data, bad stack position");
			continue;
		}

		if (sculpt_mode &&
		    (!has_multires || multires_applied || sculpt_dyntopo))
		{
			bool unsupported = false;

			if (md->type == eModifierType_Multires && ((MultiresModifierData *)md)->sculptlvl == 0) {
				/* If multires is on level 0 skip it silently without warning message. */
				if (!sculpt_dyntopo) {
					continue;
				}
			}

			if (sculpt_dyntopo && !useRenderParams)
				unsupported = true;

			if (scene->toolsettings->sculpt->flags & SCULPT_ONLY_DEFORM)
				unsupported |= (mti->type != eModifierTypeType_OnlyDeform);

			unsupported |= multires_applied;

			if (unsupported) {
				if (sculpt_dyntopo)
					modifier_setError(md, "Not supported in dyntopo");
				else
					modifier_setError(md, "Not supported in sculpt mode");
				continue;
			}
			else {
				modifier_setError(md, "Hide, Mask and optimized display disabled");
			}
		}

		if (need_mapping && !modifier_supportsMapping(md)) {
			continue;
		}

		if (useDeform < 0 && mti->dependsOnTime && mti->dependsOnTime(md)) {
			continue;
		}

		/* add an orco layer if needed by this modifier */
		if (mti->requiredDataMask)
			mask = mti->requiredDataMask(ob, md);
		else
			mask = 0;

		if (mesh && (mask & CD_MASK_ORCO)) {
			add_orco_mesh(ob, NULL, mesh, orco_mesh, CD_ORCO);
		}

		/* How to apply modifier depends on (a) what we already have as
		 * a result of previous modifiers (could be a Mesh or just
		 * deformed vertices) and (b) what type the modifier is.
		 */

		if (mti->type == eModifierTypeType_OnlyDeform) {
			/* No existing verts to deform, need to build them. */
			if (!deformedVerts) {
				if (mesh) {
					/* Deforming a mesh, read the vertex locations
					 * out of the mesh and deform them. Once done with this
					 * run of deformers verts will be written back.
					 */
					deformedVerts = BKE_mesh_vertexCos_get(mesh, &numVerts);
				}
				else {
					deformedVerts = BKE_mesh_vertexCos_get(me, &numVerts);
				}
			}

			/* if this is not the last modifier in the stack then recalculate the normals
			 * to avoid giving bogus normals to the next modifier see: [#23673] */
			if (isPrevDeform && mti->dependsOnNormals && mti->dependsOnNormals(md)) {
				/* XXX, this covers bug #23673, but we may need normal calc for other types */
				if (mesh) {
					BKE_mesh_apply_vert_coords(mesh, deformedVerts);
				}
			}

			modifier_deformVerts_ensure_normals(md, &mectx_deform, mesh, deformedVerts, numVerts);
		}
		else {
			/* determine which data layers are needed by following modifiers */
			if (curr->next)
				nextmask = curr->next->mask;
			else
				nextmask = dataMask;

			/* apply vertex coordinates or build a Mesh as necessary */
			if (mesh) {
				if (deformedVerts) {
					BKE_mesh_apply_vert_coords(mesh, deformedVerts);
				}
			}
			else {
				BKE_id_copy_ex(
				        NULL, &me->id, (ID **)&mesh,
				        (LIB_ID_CREATE_NO_MAIN |
				         LIB_ID_CREATE_NO_USER_REFCOUNT |
				         LIB_ID_CREATE_NO_DEG_TAG |
				         LIB_ID_COPY_CD_REFERENCE),
				        false);
				ASSERT_IS_VALID_MESH(mesh);

				// XXX: port to Mesh if build_shapekey_layers can ever be true
				//if (build_shapekey_layers)
				//	add_shapekey_layers(mesh, me, ob);

				if (deformedVerts) {
					BKE_mesh_apply_vert_coords(mesh, deformedVerts);
				}

				if (do_init_wmcol)
					mesh_update_weight_mcol(ob, mesh, draw_flag, NULL, 0, NULL);

				/* Constructive modifiers need to have an origindex
				 * otherwise they wont have anywhere to copy the data from.
				 *
				 * Also create ORIGINDEX data if any of the following modifiers
				 * requests it, this way Mirror, Solidify etc will keep ORIGINDEX
				 * data by using generic DM_copy_vert_data() functions.
				 */
				if (need_mapping || (nextmask & CD_MASK_ORIGINDEX)) {
					/* calc */
					CustomData_add_layer(&mesh->vdata, CD_ORIGINDEX, CD_CALLOC, NULL, mesh->totvert);
					CustomData_add_layer(&mesh->edata, CD_ORIGINDEX, CD_CALLOC, NULL, mesh->totedge);
					CustomData_add_layer(&mesh->pdata, CD_ORIGINDEX, CD_CALLOC, NULL, mesh->totpoly);

					/* Not worth parallelizing this, gives less than 0.1% overall speedup in best of best cases... */
					range_vn_i(CustomData_get_layer(&mesh->vdata, CD_ORIGINDEX), mesh->totvert, 0);
					range_vn_i(CustomData_get_layer(&mesh->edata, CD_ORIGINDEX), mesh->totedge, 0);
					range_vn_i(CustomData_get_layer(&mesh->pdata, CD_ORIGINDEX), mesh->totpoly, 0);
				}
			}


			/* set the Mesh to only copy needed data */
			mask = curr->mask;
			/* needMapping check here fixes bug [#28112], otherwise it's
			 * possible that it won't be copied */
			mask |= append_mask;
			mesh_set_only_copy(mesh, mask | (need_mapping ? CD_MASK_ORIGINDEX : 0));

			/* add cloth rest shape key if needed */
			if (mask & CD_MASK_CLOTH_ORCO)
				add_orco_mesh(ob, NULL, mesh, orco_mesh, CD_CLOTH_ORCO);

			/* add an origspace layer if needed */
			if ((curr->mask) & CD_MASK_ORIGSPACE_MLOOP) {
				if (!CustomData_has_layer(&mesh->ldata, CD_ORIGSPACE_MLOOP)) {
					CustomData_add_layer(&mesh->ldata, CD_ORIGSPACE_MLOOP, CD_CALLOC, NULL, mesh->totloop);
					mesh_init_origspace(mesh);
				}
			}

			Mesh *new_mesh = modifier_applyModifier_ensure_normals(md, &mectx_apply, mesh);
			ASSERT_IS_VALID_MESH(new_mesh);

			if (new_mesh) {
				/* if the modifier returned a new mesh, release the old one */
				if (mesh && mesh != new_mesh) {
					BLI_assert(mesh != me);
					BKE_id_free(NULL, mesh);
				}

				mesh = new_mesh;

				if (deformedVerts) {
					if (deformedVerts != inputVertexCos)
						MEM_freeN(deformedVerts);

					deformedVerts = NULL;
				}
			}

			/* create an orco mesh in parallel */
			if (nextmask & CD_MASK_ORCO) {
				if (!orco_mesh) {
					orco_mesh = create_orco_mesh(ob, me, NULL, CD_ORCO);
				}

				nextmask &= ~CD_MASK_ORCO;
				mesh_set_only_copy(orco_mesh, nextmask | CD_MASK_ORIGINDEX |
				                 (mti->requiredDataMask ?
				                  mti->requiredDataMask(ob, md) : 0));

				new_mesh = modifier_applyModifier_ensure_normals(md, &mectx_orco, orco_mesh);
				ASSERT_IS_VALID_MESH(new_mesh);

				if (new_mesh) {
					/* if the modifier returned a new mesh, release the old one */
					if (orco_mesh && orco_mesh != new_mesh) {
						BLI_assert(orco_mesh != me);
						BKE_id_free(NULL, orco_mesh);
					}

					orco_mesh = new_mesh;
				}
			}

			/* create cloth orco mesh in parallel */
			if (nextmask & CD_MASK_CLOTH_ORCO) {
				if (!cloth_orco_mesh) {
					cloth_orco_mesh = create_orco_mesh(ob, me, NULL, CD_CLOTH_ORCO);
				}

				nextmask &= ~CD_MASK_CLOTH_ORCO;
				mesh_set_only_copy(cloth_orco_mesh, nextmask | CD_MASK_ORIGINDEX);

				new_mesh = modifier_applyModifier_ensure_normals(md, &mectx_orco, cloth_orco_mesh);
				ASSERT_IS_VALID_DM(new_mesh);

				if (new_mesh) {
					/* if the modifier returned a new mesh, release the old one */
					if (cloth_orco_mesh && cloth_orco_mesh != new_mesh) {
						BLI_assert(orco_mesh != me);
						BKE_id_free(NULL, cloth_orco_mesh);
					}

					cloth_orco_mesh = new_mesh;
				}
			}

			/* in case of dynamic paint, make sure preview mask remains for following modifiers */
			/* XXX Temp and hackish solution! */
			if (md->type == eModifierType_DynamicPaint)
				append_mask |= CD_MASK_PREVIEW_MLOOPCOL;
			/* In case of active preview modifier, make sure preview mask remains for following modifiers. */
			else if ((md == previewmd) && (do_mod_wmcol)) {
				mesh_update_weight_mcol(ob, mesh, draw_flag, NULL, 0, NULL);
				append_mask |= CD_MASK_PREVIEW_MLOOPCOL;
			}

			mesh->runtime.deformed_only = false;
		}

		isPrevDeform = (mti->type == eModifierTypeType_OnlyDeform);

		/* grab modifiers until index i */
		if ((index != -1) && (BLI_findindex(&ob->modifiers, md) >= index))
			break;

		if (sculpt_mode && md->type == eModifierType_Multires) {
			multires_applied = true;
		}
	}

	for (md = firstmd; md; md = md->next)
		modifier_freeTemporaryData(md);

	/* Yay, we are done. If we have a Mesh and deformed vertices
	 * need to apply these back onto the Mesh. If we have no
	 * Mesh then we need to build one.
	 */
	Mesh *final_mesh;

	if (mesh) {
		final_mesh = mesh;

		if (deformedVerts) {
			BKE_mesh_apply_vert_coords(final_mesh, deformedVerts);
		}

#if 0 /* For later nice mod preview! */
		/* In case we need modified weights in CD_PREVIEW_MCOL, we have to re-compute it. */
		if (do_final_wmcol)
			mesh_update_weight_mcol(ob, final_mesh, draw_flag, NULL, 0, NULL);
#endif
	}
	else {
		BKE_id_copy_ex(
		        NULL, &me->id, (ID **)&final_mesh,
		        (LIB_ID_CREATE_NO_MAIN |
		         LIB_ID_CREATE_NO_USER_REFCOUNT |
		         LIB_ID_CREATE_NO_DEG_TAG |
		         LIB_ID_COPY_CD_REFERENCE),
		        false);

		//if (build_shapekey_layers) {
		//	add_shapekey_layers(final_mesh, me, ob);
		//}

		if (deformedVerts) {
			BKE_mesh_apply_vert_coords(final_mesh, deformedVerts);
		}

		/* In this case, we should never have weight-modifying modifiers in stack... */
		if (do_init_wmcol)
			mesh_update_weight_mcol(ob, final_mesh, draw_flag, NULL, 0, NULL);
	}

	/* add an orco layer if needed */
	if (dataMask & CD_MASK_ORCO) {
		add_orco_mesh(ob, NULL, final_mesh, orco_mesh, CD_ORCO);

		if (r_deform_mesh && *r_deform_mesh)
			add_orco_mesh(ob, NULL, *r_deform_mesh, NULL, CD_ORCO);
	}

	if (do_loop_normals) {
		/* Compute loop normals (note: will compute poly and vert normals as well, if needed!) */
		BKE_mesh_calc_normals_split(final_mesh);
		BKE_mesh_tessface_clear(final_mesh);
	}

	if (sculpt_dyntopo == false) {
		/* watch this! after 2.75a we move to from tessface to looptri (by default) */
		if (dataMask & CD_MASK_MFACE) {
			BKE_mesh_tessface_ensure(final_mesh);
		}

		/* without this, drawing ngon tri's faces will show ugly tessellated face
		 * normals and will also have to calculate normals on the fly, try avoid
		 * this where possible since calculating polygon normals isn't fast,
		 * note that this isn't a problem for subsurf (only quads) or editmode
		 * which deals with drawing differently.
		 *
		 * Only calc vertex normals if they are flagged as dirty.
		 * If using loop normals, poly nors have already been computed.
		 */
		if (!do_loop_normals) {
			mesh_ensure_display_normals(final_mesh);
		}
	}

	/* Some modifiers, like datatransfer, may generate those data as temp layer, we do not want to keep them,
	 * as they are used by display code when available (i.e. even if autosmooth is disabled). */
	if (!do_loop_normals && CustomData_has_layer(&final_mesh->ldata, CD_NORMAL)) {
		CustomData_free_layers(&final_mesh->ldata, CD_NORMAL, final_mesh->totloop);
	}

	*r_final_mesh = final_mesh;

	if (orco_mesh) {
		BKE_id_free(NULL, orco_mesh);
	}
	if (cloth_orco_mesh) {
		BKE_id_free(NULL, cloth_orco_mesh);
	}

	if (deformedVerts && deformedVerts != inputVertexCos)
		MEM_freeN(deformedVerts);

	BLI_linklist_free((LinkNode *)datamasks, NULL);
}

static void mesh_calc_modifiers_dm(
        struct Depsgraph *depsgraph, Scene *scene, Object *ob, float (*inputVertexCos)[3],
        int useDeform,
        const bool need_mapping, CustomDataMask dataMask,
        const int index, const bool useCache, const bool build_shapekey_layers,
        const bool allow_gpu,
        /* return args */
        DerivedMesh **r_deformdm, DerivedMesh **r_finaldm)
{
	Mesh *deform_mesh = NULL, *final_mesh = NULL;

	mesh_calc_modifiers(
	        depsgraph, scene, ob, inputVertexCos, useDeform,
	        need_mapping, dataMask, index, useCache, build_shapekey_layers, allow_gpu,
	        (r_deformdm ? &deform_mesh : NULL), &final_mesh);

	if (deform_mesh) {
		*r_deformdm = CDDM_from_mesh_ex(deform_mesh, CD_DUPLICATE, CD_MASK_MESH);
		BKE_id_free(NULL, deform_mesh);
	}

	*r_finaldm = CDDM_from_mesh_ex(final_mesh, CD_DUPLICATE, CD_MASK_MESH);
	BKE_id_free(NULL, final_mesh);
}

float (*editbmesh_get_vertex_cos(BMEditMesh *em, int *r_numVerts))[3]
{
	BMIter iter;
	BMVert *eve;
	float (*cos)[3];
	int i;

	*r_numVerts = em->bm->totvert;

	cos = MEM_malloc_arrayN(em->bm->totvert, 3 * sizeof(float), "vertexcos");

	BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
		copy_v3_v3(cos[i], eve->co);
	}

	return cos;
}

bool editbmesh_modifier_is_enabled(Scene *scene, ModifierData *md, DerivedMesh *dm)
{
	const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	const int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;

	if (!modifier_isEnabled(scene, md, required_mode)) {
		return false;
	}

	if ((mti->flags & eModifierTypeFlag_RequiresOriginalData) && dm) {
		modifier_setError(md, "Modifier requires original data, bad stack position");
		return false;
	}

	return true;
}

static void editbmesh_calc_modifiers(
        struct Depsgraph *depsgraph, Scene *scene, Object *ob,
        BMEditMesh *em, CustomDataMask dataMask,
        /* return args */
        DerivedMesh **r_cage, DerivedMesh **r_final)
{
	ModifierData *md, *previewmd = NULL;
	float (*deformedVerts)[3] = NULL;
	CustomDataMask mask = 0, previewmask = 0, append_mask = 0;
	DerivedMesh *dm = NULL, *orcodm = NULL;
	int i, numVerts = 0, cageIndex = modifiers_getCageIndex(scene, ob, NULL, 1);
	CDMaskLink *datamasks, *curr;
	const int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;
	int draw_flag = dm_drawflag_calc(scene->toolsettings, ob->data);

	// const bool do_mod_mcol = true; // (ob->mode == OB_MODE_OBJECT);
#if 0 /* XXX Will re-enable this when we have global mod stack options. */
	const bool do_final_wmcol = (scene->toolsettings->weights_preview == WP_WPREVIEW_FINAL) && do_wmcol;
#endif
	const bool do_final_wmcol = false;
	const bool do_init_wmcol = ((((Mesh *)ob->data)->drawflag & ME_DRAWEIGHT) && !do_final_wmcol);

	const bool do_init_statvis = ((((Mesh *)ob->data)->drawflag & ME_DRAW_STATVIS) && !do_init_wmcol);
	const bool do_mod_wmcol = do_init_wmcol;
	VirtualModifierData virtualModifierData;

	/* TODO(sybren): do we really need multiple objects, or shall we change the flags where needed? */
	const ModifierEvalContext mectx = {depsgraph, ob, 0};
	const ModifierEvalContext mectx_orco = {depsgraph, ob, MOD_APPLY_ORCO};
	const ModifierEvalContext mectx_cache_gpu = {depsgraph, ob, MOD_APPLY_USECACHE | MOD_APPLY_ALLOW_GPU};

	const bool do_loop_normals = (((Mesh *)(ob->data))->flag & ME_AUTOSMOOTH) != 0;
	const float loop_normals_split_angle = ((Mesh *)(ob->data))->smoothresh;

	modifiers_clearErrors(ob);

	if (r_cage && cageIndex == -1) {
		*r_cage = getEditDerivedBMesh(em, ob, dataMask, NULL);
	}

	md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

	/* copied from mesh_calc_modifiers */
	if (do_mod_wmcol) {
		previewmd = modifiers_getLastPreview(scene, md, required_mode);
		/* even if the modifier doesn't need the data, to make a preview it may */
		if (previewmd) {
			previewmask = CD_MASK_MDEFORMVERT;
		}
	}

	datamasks = modifiers_calcDataMasks(scene, ob, md, dataMask, required_mode, previewmd, previewmask);

	curr = datamasks;
	for (i = 0; md; i++, md = md->next, curr = curr->next) {
		const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (!editbmesh_modifier_is_enabled(scene, md, dm)) {
			continue;
		}

		/* add an orco layer if needed by this modifier */
		if (dm && mti->requiredDataMask) {
			mask = mti->requiredDataMask(ob, md);
			if (mask & CD_MASK_ORCO)
				add_orco_dm(ob, em, dm, orcodm, CD_ORCO);
		}

		/* How to apply modifier depends on (a) what we already have as
		 * a result of previous modifiers (could be a DerivedMesh or just
		 * deformed vertices) and (b) what type the modifier is.
		 */

		if (mti->type == eModifierTypeType_OnlyDeform) {
			/* No existing verts to deform, need to build them. */
			if (!deformedVerts) {
				if (dm) {
					/* Deforming a derived mesh, read the vertex locations
					 * out of the mesh and deform them. Once done with this
					 * run of deformers verts will be written back.
					 */
					numVerts = dm->getNumVerts(dm);
					deformedVerts =
					    MEM_malloc_arrayN(numVerts, sizeof(*deformedVerts), "dfmv");
					dm->getVertCos(dm, deformedVerts);
				}
				else {
					deformedVerts = editbmesh_get_vertex_cos(em, &numVerts);
				}
			}

			if (mti->deformVertsEM || mti->deformVertsEM_DM)
				modwrap_deformVertsEM(md, &mectx, em, dm, deformedVerts, numVerts);
			else
				modwrap_deformVerts(md, &mectx, dm, deformedVerts, numVerts);
		}
		else {
			DerivedMesh *ndm;

			/* apply vertex coordinates or build a DerivedMesh as necessary */
			if (dm) {
				if (deformedVerts) {
					DerivedMesh *tdm = CDDM_copy(dm);
					if (!(r_cage && dm == *r_cage)) {
						dm->release(dm);
					}
					dm = tdm;

					CDDM_apply_vert_coords(dm, deformedVerts);
				}
				else if (r_cage && dm == *r_cage) {
					/* dm may be changed by this modifier, so we need to copy it */
					dm = CDDM_copy(dm);
				}

			}
			else {
				dm = CDDM_from_editbmesh(em, false, false);
				ASSERT_IS_VALID_DM(dm);

				if (deformedVerts) {
					CDDM_apply_vert_coords(dm, deformedVerts);
				}

				if (do_init_wmcol) {
					DM_update_weight_mcol(ob, dm, draw_flag, NULL, 0, NULL);
				}
			}

			/* create an orco derivedmesh in parallel */
			mask = curr->mask;
			if (mask & CD_MASK_ORCO) {
				if (!orcodm)
					orcodm = create_orco_dm(ob, ob->data, em, CD_ORCO);

				mask &= ~CD_MASK_ORCO;
				DM_set_only_copy(orcodm, mask | CD_MASK_ORIGINDEX);

				if (mti->applyModifierEM || mti->applyModifierEM_DM) {
					ndm = modwrap_applyModifierEM(md, &mectx_orco, em, orcodm);
				}
				else {
					ndm = modwrap_applyModifier(md, &mectx_orco, orcodm);
				}
				ASSERT_IS_VALID_DM(ndm);

				if (ndm) {
					/* if the modifier returned a new dm, release the old one */
					if (orcodm && orcodm != ndm) orcodm->release(orcodm);
					orcodm = ndm;
				}
			}

			/* set the DerivedMesh to only copy needed data */
			mask |= append_mask;
			mask = curr->mask; /* CD_MASK_ORCO may have been cleared above */

			DM_set_only_copy(dm, mask | CD_MASK_ORIGINDEX);

			if (mask & CD_MASK_ORIGSPACE_MLOOP) {
				if (!CustomData_has_layer(&dm->loopData, CD_ORIGSPACE_MLOOP)) {
					DM_add_loop_layer(dm, CD_ORIGSPACE_MLOOP, CD_CALLOC, NULL);
					DM_init_origspace(dm);
				}
			}

			if (mti->applyModifierEM || mti->applyModifierEM_DM)
				ndm = modwrap_applyModifierEM(md, &mectx_cache_gpu, em, dm);
			else
				ndm = modwrap_applyModifier(md, &mectx_cache_gpu, dm);
			ASSERT_IS_VALID_DM(ndm);

			if (ndm) {
				if (dm && dm != ndm)
					dm->release(dm);

				dm = ndm;

				if (deformedVerts) {
					MEM_freeN(deformedVerts);
					deformedVerts = NULL;
				}
			}

			dm->deformedOnly = false;
		}

		/* In case of active preview modifier, make sure preview mask remains for following modifiers. */
		if ((md == previewmd) && (do_mod_wmcol)) {
			DM_update_weight_mcol(ob, dm, draw_flag, NULL, 0, NULL);
			append_mask |= CD_MASK_PREVIEW_MLOOPCOL;
		}

		if (r_cage && i == cageIndex) {
			if (dm && deformedVerts) {
				*r_cage = CDDM_copy(dm);
				CDDM_apply_vert_coords(*r_cage, deformedVerts);
			}
			else if (dm) {
				*r_cage = dm;
			}
			else {
				struct Mesh *mesh = ob->data;
				if (mesh->id.tag & LIB_TAG_COPIED_ON_WRITE) {
					BKE_mesh_runtime_ensure_edit_data(mesh);
					mesh->runtime.edit_data->vertexCos = MEM_dupallocN(deformedVerts);
				}
				*r_cage = getEditDerivedBMesh(
				        em, ob, mask,
				        deformedVerts ? MEM_dupallocN(deformedVerts) : NULL);
			}
		}
	}

	BLI_linklist_free((LinkNode *)datamasks, NULL);

	/* Yay, we are done. If we have a DerivedMesh and deformed vertices need
	 * to apply these back onto the DerivedMesh. If we have no DerivedMesh
	 * then we need to build one.
	 */
	if (dm && deformedVerts) {
		*r_final = CDDM_copy(dm);

		if (!(r_cage && dm == *r_cage)) {
			dm->release(dm);
		}

		CDDM_apply_vert_coords(*r_final, deformedVerts);
	}
	else if (dm) {
		*r_final = dm;
	}
	else if (!deformedVerts && r_cage && *r_cage) {
		/* cage should already have up to date normals */
		*r_final = *r_cage;

		/* In this case, we should never have weight-modifying modifiers in stack... */
		if (do_init_wmcol)
			DM_update_weight_mcol(ob, *r_final, draw_flag, NULL, 0, NULL);
		if (do_init_statvis)
			DM_update_statvis_color(scene, ob, *r_final);
	}
	else {
		/* this is just a copy of the editmesh, no need to calc normals */
		struct Mesh *mesh = ob->data;
		if (mesh->id.tag & LIB_TAG_COPIED_ON_WRITE) {
			BKE_mesh_runtime_ensure_edit_data(mesh);
			if (mesh->runtime.edit_data->vertexCos != NULL)
				MEM_freeN((void *)mesh->runtime.edit_data->vertexCos);
			mesh->runtime.edit_data->vertexCos = MEM_dupallocN(deformedVerts);
		}
		*r_final = getEditDerivedBMesh(em, ob, dataMask, deformedVerts);
		deformedVerts = NULL;

		/* In this case, we should never have weight-modifying modifiers in stack... */
		if (do_init_wmcol)
			DM_update_weight_mcol(ob, *r_final, draw_flag, NULL, 0, NULL);
		if (do_init_statvis)
			DM_update_statvis_color(scene, ob, *r_final);
	}

	if (do_loop_normals) {
		/* Compute loop normals */
		DM_calc_loop_normals(*r_final, do_loop_normals, loop_normals_split_angle);
		if (r_cage && *r_cage && (*r_cage != *r_final)) {
			DM_calc_loop_normals(*r_cage, do_loop_normals, loop_normals_split_angle);
		}
	}

	/* BMESH_ONLY, ensure tessface's used for drawing,
	 * but don't recalculate if the last modifier in the stack gives us tessfaces
	 * check if the derived meshes are DM_TYPE_EDITBMESH before calling, this isn't essential
	 * but quiets annoying error messages since tessfaces wont be created. */
	if (dataMask & CD_MASK_MFACE) {
		if ((*r_final)->type != DM_TYPE_EDITBMESH) {
			DM_ensure_tessface(*r_final);
		}
		if (r_cage && *r_cage) {
			if ((*r_cage)->type != DM_TYPE_EDITBMESH) {
				if (*r_cage != *r_final) {
					DM_ensure_tessface(*r_cage);
				}
			}
		}
	}
	/* --- */

	/* same as mesh_calc_modifiers (if using loop normals, poly nors have already been computed). */
	if (!do_loop_normals) {
		dm_ensure_display_normals(*r_final);

		/* Some modifiers, like datatransfer, may generate those data, we do not want to keep them,
		 * as they are used by display code when available (i.e. even if autosmooth is disabled). */
		if (CustomData_has_layer(&(*r_final)->loopData, CD_NORMAL)) {
			CustomData_free_layers(&(*r_final)->loopData, CD_NORMAL, (*r_final)->numLoopData);
		}
		if (r_cage && CustomData_has_layer(&(*r_cage)->loopData, CD_NORMAL)) {
			CustomData_free_layers(&(*r_cage)->loopData, CD_NORMAL, (*r_cage)->numLoopData);
		}
	}

	/* add an orco layer if needed */
	if (dataMask & CD_MASK_ORCO)
		add_orco_dm(ob, em, *r_final, orcodm, CD_ORCO);

	if (orcodm)
		orcodm->release(orcodm);

	if (deformedVerts)
		MEM_freeN(deformedVerts);
}

static void mesh_finalize_eval(Object *object)
{
	Mesh *mesh = (Mesh *)object->data;
	Mesh *mesh_eval = object->runtime.mesh_eval;
	/* Special Tweaks for cases when evaluated mesh came from
	 * BKE_mesh_new_nomain_from_template().
	 */
	BLI_strncpy(mesh_eval->id.name, mesh->id.name, sizeof(mesh_eval->id.name));
	if (mesh_eval->mat != NULL) {
		MEM_freeN(mesh_eval->mat);
	}
	/* Set flag which makes it easier to see what's going on in a debugger. */
	mesh_eval->id.tag |= LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT;
	mesh_eval->mat = MEM_dupallocN(mesh->mat);
	mesh_eval->totcol = mesh->totcol;
	/* Make evaluated mesh to share same edit mesh pointer as original
	 * and copied meshes.
	 */
	mesh_eval->edit_btmesh = mesh->edit_btmesh;
	/* Copy autosmooth settings from original mesh.
	 * This is not done by BKE_mesh_new_nomain_from_template(), so need to take
	 * extra care here.
	 */
	mesh_eval->flag |= (mesh->flag & ME_AUTOSMOOTH);
	mesh_eval->smoothresh = mesh->smoothresh;
	/* Replace evaluated object's data with fully evaluated mesh. */
	/* TODO(sergey): There was statement done by Sybren and Mai that this
	 * caused modifiers to be applied twice. which is weirtd and shouldn't
	 * really happen. But since there is no reference to the report, can not
	 * do much about this.
	 */

	/* Object is sometimes not evaluated!
	 * TODO(sergey): BAD TEMPORARY HACK FOR UNTIL WE ARE SMARTER */
	if (object->id.tag & LIB_TAG_COPIED_ON_WRITE) {
		object->data = mesh_eval;
	}
	else {
		/* evaluated will be available via: 'object->runtime.mesh_eval' */
	}
}

static void mesh_build_data(
        struct Depsgraph *depsgraph, Scene *scene, Object *ob, CustomDataMask dataMask,
        const bool build_shapekey_layers, const bool need_mapping)
{
	BLI_assert(ob->type == OB_MESH);

	BKE_object_free_derived_caches(ob);
	BKE_object_sculpt_modifiers_changed(ob);

	mesh_calc_modifiers(
	        depsgraph, scene, ob, NULL, 1, need_mapping, dataMask, -1, true, build_shapekey_layers,
	        true,
	        &ob->runtime.mesh_deform_eval, &ob->runtime.mesh_eval);

	mesh_finalize_eval(ob);

	ob->derivedDeform = CDDM_from_mesh_ex(ob->runtime.mesh_deform_eval, CD_REFERENCE, CD_MASK_MESH);
	ob->derivedFinal = CDDM_from_mesh_ex(ob->runtime.mesh_eval, CD_REFERENCE, CD_MASK_MESH);

	DM_set_object_boundbox(ob, ob->derivedFinal);
	/* TODO(sergey): Convert bounding box calculation to use mesh, then
	 * we can skip this copy.
	 */
	BKE_mesh_texspace_copy_from_object(ob->runtime.mesh_eval, ob);

	ob->derivedFinal->needsFree = 0;
	ob->derivedDeform->needsFree = 0;
	ob->lastDataMask = dataMask;
	ob->lastNeedMapping = need_mapping;

	if ((ob->mode & OB_MODE_ALL_SCULPT) && ob->sculpt) {
		/* create PBVH immediately (would be created on the fly too,
		 * but this avoids waiting on first stroke) */
		/* XXX Disabled for now.
		 * This can create horrible nasty bugs by generating re-entrant call of mesh_get_eval_final! */
//		BKE_sculpt_update_mesh_elements(depsgraph, scene, scene->toolsettings->sculpt, ob, false, false);
	}

	BLI_assert(!(ob->derivedFinal->dirty & DM_DIRTY_NORMALS));
}

static void editbmesh_build_data(
        struct Depsgraph *depsgraph, Scene *scene,
        Object *obedit, BMEditMesh *em, CustomDataMask dataMask)
{
	BKE_object_free_derived_caches(obedit);
	BKE_object_sculpt_modifiers_changed(obedit);

	BKE_editmesh_free_derivedmesh(em);

	editbmesh_calc_modifiers(
	        depsgraph, scene, obedit, em, dataMask,
	        &em->derivedCage, &em->derivedFinal);

	DM_set_object_boundbox(obedit, em->derivedFinal);

	em->lastDataMask = dataMask;
	em->derivedFinal->needsFree = 0;
	em->derivedCage->needsFree = 0;

	BLI_assert(!(em->derivedFinal->dirty & DM_DIRTY_NORMALS));
}

static CustomDataMask object_get_datamask(const Depsgraph *depsgraph, Object *ob, bool *r_need_mapping)
{
	ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
	Object *actob = view_layer->basact ? DEG_get_original_object(view_layer->basact->object) : NULL;
	CustomDataMask mask = ob->customdata_mask;

	if (r_need_mapping) {
		*r_need_mapping = false;
	}

	if (DEG_get_original_object(ob) == actob) {
		bool editing = BKE_paint_select_face_test(ob);

		/* weight paint and face select need original indices because of selection buffer drawing */
		if (r_need_mapping) {
			*r_need_mapping = (editing || (ob->mode & (OB_MODE_WEIGHT_PAINT | OB_MODE_VERTEX_PAINT)));
		}

		/* check if we need tfaces & mcols due to face select or texture paint */
		if ((ob->mode & OB_MODE_TEXTURE_PAINT) || editing) {
			mask |= CD_MASK_MLOOPUV | CD_MASK_MLOOPCOL;
		}

		/* check if we need mcols due to vertex paint or weightpaint */
		if (ob->mode & OB_MODE_VERTEX_PAINT) {
			mask |= CD_MASK_MLOOPCOL;
		}

		if (ob->mode & OB_MODE_WEIGHT_PAINT) {
			mask |= CD_MASK_PREVIEW_MLOOPCOL;
		}

		if (ob->mode & OB_MODE_EDIT)
			mask |= CD_MASK_MVERT_SKIN;
	}

	return mask;
}

void makeDerivedMesh(
        struct Depsgraph *depsgraph, Scene *scene, Object *ob, BMEditMesh *em,
        CustomDataMask dataMask, const bool build_shapekey_layers)
{
	bool need_mapping;
	dataMask |= object_get_datamask(depsgraph, ob, &need_mapping);

	if (em) {
		editbmesh_build_data(depsgraph, scene, ob, em, dataMask);
	}
	else {
		mesh_build_data(depsgraph, scene, ob, dataMask, build_shapekey_layers, need_mapping);
	}
}

/***/

#ifdef USE_DERIVEDMESH
/* Deprecated DM, use: 'mesh_get_eval_final'. */
DerivedMesh *mesh_get_derived_final(
        struct Depsgraph *depsgraph, Scene *scene, Object *ob, CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	bool need_mapping;
	dataMask |= object_get_datamask(depsgraph, ob, &need_mapping);

	if (!ob->derivedFinal ||
	    ((dataMask & ob->lastDataMask) != dataMask) ||
	    (need_mapping != ob->lastNeedMapping))
	{
		mesh_build_data(depsgraph, scene, ob, dataMask, false, need_mapping);
	}

	if (ob->derivedFinal) { BLI_assert(!(ob->derivedFinal->dirty & DM_DIRTY_NORMALS)); }
	return ob->derivedFinal;
}
#endif
Mesh *mesh_get_eval_final(
        struct Depsgraph *depsgraph, Scene *scene, Object *ob, CustomDataMask dataMask)
{
	/* if there's no evaluated mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	bool need_mapping;
	dataMask |= object_get_datamask(depsgraph, ob, &need_mapping);

	if (!ob->runtime.mesh_eval ||
	    ((dataMask & ob->lastDataMask) != dataMask) ||
	    (need_mapping != ob->lastNeedMapping))
	{
		mesh_build_data(depsgraph, scene, ob, dataMask, false, need_mapping);
	}

	if (ob->runtime.mesh_eval) { BLI_assert(!(ob->runtime.mesh_eval->runtime.cd_dirty_vert & CD_MASK_NORMAL)); }
	return ob->runtime.mesh_eval;
}

#ifdef USE_DERIVEDMESH
/* Deprecated DM, use: 'mesh_get_eval_deform' instead. */
DerivedMesh *mesh_get_derived_deform(struct Depsgraph *depsgraph, Scene *scene, Object *ob, CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	bool need_mapping;

	dataMask |= object_get_datamask(depsgraph, ob, &need_mapping);

	if (!ob->derivedDeform ||
	    ((dataMask & ob->lastDataMask) != dataMask) ||
	    (need_mapping != ob->lastNeedMapping))
	{
		mesh_build_data(depsgraph, scene, ob, dataMask, false, need_mapping);
	}

	return ob->derivedDeform;
}
#endif
Mesh *mesh_get_eval_deform(struct Depsgraph *depsgraph, Scene *scene, Object *ob, CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	bool need_mapping;

	dataMask |= object_get_datamask(depsgraph, ob, &need_mapping);

	if (!ob->runtime.mesh_deform_eval ||
	    ((dataMask & ob->lastDataMask) != dataMask) ||
	    (need_mapping != ob->lastNeedMapping))
	{
		mesh_build_data(depsgraph, scene, ob, dataMask, false, need_mapping);
	}

	return ob->runtime.mesh_deform_eval;
}


DerivedMesh *mesh_create_derived_render(struct Depsgraph *depsgraph, Scene *scene, Object *ob, CustomDataMask dataMask)
{
	DerivedMesh *final;

	mesh_calc_modifiers_dm(
	        depsgraph, scene, ob, NULL, 1, false, dataMask, -1, false, false, false,
	        NULL, &final);

	return final;
}

#ifdef USE_DERIVEDMESH
/* Deprecated, use `mesh_create_eval_final_index_render` instead. */
DerivedMesh *mesh_create_derived_index_render(struct Depsgraph *depsgraph, Scene *scene, Object *ob, CustomDataMask dataMask, int index)
{
	DerivedMesh *final;

	mesh_calc_modifiers_dm(
	        depsgraph, scene, ob, NULL, 1, false, dataMask, index, false, false, false,
	        NULL, &final);

	return final;
}
#endif
struct Mesh *mesh_create_eval_final_index_render(
        struct Depsgraph *depsgraph, struct Scene *scene,
        struct Object *ob, CustomDataMask dataMask, int index)
{
	Mesh *final;

	mesh_calc_modifiers(
	        depsgraph, scene, ob, NULL, 1, false, dataMask, index, false, false, false,
	        NULL, &final);

	return final;
}

#ifdef USE_DERIVEDMESH
/* Deprecated, use `mesh_create_eval_final_view` instead. */
DerivedMesh *mesh_create_derived_view(
        struct Depsgraph *depsgraph, Scene *scene,
        Object *ob, CustomDataMask dataMask)
{
	DerivedMesh *final;

	/* XXX hack
	 * psys modifier updates particle state when called during dupli-list generation,
	 * which can lead to wrong transforms. This disables particle system modifier execution.
	 */
	ob->transflag |= OB_NO_PSYS_UPDATE;

	mesh_calc_modifiers_dm(
	        depsgraph, scene, ob, NULL, 1, false, dataMask, -1, false, false, false,
	        NULL, &final);

	ob->transflag &= ~OB_NO_PSYS_UPDATE;

	return final;
}
#endif

Mesh *mesh_create_eval_final_view(
        struct Depsgraph *depsgraph, Scene *scene,
        Object *ob, CustomDataMask dataMask)
{
	Mesh *final;

	/* XXX hack
	 * psys modifier updates particle state when called during dupli-list generation,
	 * which can lead to wrong transforms. This disables particle system modifier execution.
	 */
	ob->transflag |= OB_NO_PSYS_UPDATE;

	mesh_calc_modifiers(
	        depsgraph, scene, ob, NULL, 1, false, dataMask, -1, false, false, false,
	        NULL, &final);

	ob->transflag &= ~OB_NO_PSYS_UPDATE;

	return final;
}

DerivedMesh *mesh_create_derived_no_deform(
        struct Depsgraph *depsgraph, Scene *scene, Object *ob,
        float (*vertCos)[3], CustomDataMask dataMask)
{
	DerivedMesh *final;

	mesh_calc_modifiers_dm(
	        depsgraph, scene, ob, vertCos, 0, false, dataMask, -1, false, false, false,
	        NULL, &final);

	return final;
}

/***/

DerivedMesh *editbmesh_get_derived_cage_and_final(
        struct Depsgraph *depsgraph, Scene *scene, Object *obedit, BMEditMesh *em,
        CustomDataMask dataMask,
        /* return args */
        DerivedMesh **r_final)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	dataMask |= object_get_datamask(depsgraph, obedit, NULL);

	if (!em->derivedCage ||
	    (em->lastDataMask & dataMask) != dataMask)
	{
		editbmesh_build_data(depsgraph, scene, obedit, em, dataMask);
	}

	*r_final = em->derivedFinal;
	if (em->derivedFinal) { BLI_assert(!(em->derivedFinal->dirty & DM_DIRTY_NORMALS)); }
	return em->derivedCage;
}

DerivedMesh *editbmesh_get_derived_cage(
        struct Depsgraph *depsgraph, Scene *scene, Object *obedit, BMEditMesh *em,
        CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	dataMask |= object_get_datamask(depsgraph, obedit, NULL);

	if (!em->derivedCage ||
	    (em->lastDataMask & dataMask) != dataMask)
	{
		editbmesh_build_data(depsgraph, scene, obedit, em, dataMask);
	}

	return em->derivedCage;
}

DerivedMesh *editbmesh_get_derived_base(Object *obedit, BMEditMesh *em, CustomDataMask data_mask)
{
	return getEditDerivedBMesh(em, obedit, data_mask, NULL);
}

/***/

/* get derived mesh from an object, using editbmesh if available. */
DerivedMesh *object_get_derived_final(Object *ob, const bool for_render)
{
	if (for_render) {
		/* TODO(sergey): use proper derived render here in the future. */
		return ob->derivedFinal;
	}

	/* only return the editmesh if its from this object because
	 * we don't a mesh from another object's modifier stack: T43122 */
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		BMEditMesh *em = me->edit_btmesh;
		if (em && (em->ob == ob)) {
			DerivedMesh *dm = em->derivedFinal;
			return dm;
		}
	}

	return ob->derivedFinal;
}


/* UNUSED */
#if 0

/* ********* For those who don't grasp derived stuff! (ton) :) *************** */

static void make_vertexcosnos__mapFunc(void *userData, int index, const float co[3],
                                       const float no_f[3], const short no_s[3])
{
	DMCoNo *co_no = &((DMCoNo *)userData)[index];

	/* check if we've been here before (normal should not be 0) */
	if (!is_zero_v3(co_no->no)) {
		return;
	}

	copy_v3_v3(co_no->co, co);
	if (no_f) {
		copy_v3_v3(co_no->no, no_f);
	}
	else {
		normal_short_to_float_v3(co_no->no, no_s);
	}
}

/* always returns original amount me->totvert of vertices and normals, but fully deformed and subsurfered */
/* this is needed for all code using vertexgroups (no subsurf support) */
/* it stores the normals as floats, but they can still be scaled as shorts (32767 = unit) */
/* in use now by vertex/weight paint and particle generating */

DMCoNo *mesh_get_mapped_verts_nors(Scene *scene, Object *ob)
{
	Mesh *me = ob->data;
	DerivedMesh *dm;
	DMCoNo *vertexcosnos;

	/* lets prevent crashing... */
	if (ob->type != OB_MESH || me->totvert == 0)
		return NULL;

	dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH | CD_MASK_ORIGINDEX);

	if (dm->foreachMappedVert) {
		vertexcosnos = MEM_calloc_arrayN(me->totvert, sizeof(DMCoNo), "vertexcosnos map");
		dm->foreachMappedVert(dm, make_vertexcosnos__mapFunc, vertexcosnos);
	}
	else {
		DMCoNo *v_co_no = vertexcosnos = MEM_malloc_arrayN(me->totvert, sizeof(DMCoNo), "vertexcosnos map");
		int a;
		for (a = 0; a < me->totvert; a++, v_co_no++) {
			dm->getVertCo(dm, a, v_co_no->co);
			dm->getVertNo(dm, a, v_co_no->no);
		}
	}

	dm->release(dm);
	return vertexcosnos;
}

#endif

/* same as above but for vert coords */
typedef struct {
	float (*vertexcos)[3];
	BLI_bitmap *vertex_visit;
} MappedUserData;

static void make_vertexcos__mapFunc(
        void *userData, int index, const float co[3],
        const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	MappedUserData *mappedData = (MappedUserData *)userData;

	if (BLI_BITMAP_TEST(mappedData->vertex_visit, index) == 0) {
		/* we need coord from prototype vertex, not from copies,
		 * assume they stored in the beginning of vertex array stored in DM
		 * (mirror modifier for eg does this) */
		copy_v3_v3(mappedData->vertexcos[index], co);
		BLI_BITMAP_ENABLE(mappedData->vertex_visit, index);
	}
}

void mesh_get_mapped_verts_coords(DerivedMesh *dm, float (*r_cos)[3], const int totcos)
{
	if (dm->foreachMappedVert) {
		MappedUserData userData;
		memset(r_cos, 0, sizeof(*r_cos) * totcos);
		userData.vertexcos = r_cos;
		userData.vertex_visit = BLI_BITMAP_NEW(totcos, "vertexcos flags");
		dm->foreachMappedVert(dm, make_vertexcos__mapFunc, &userData, DM_FOREACH_NOP);
		MEM_freeN(userData.vertex_visit);
	}
	else {
		int i;
		for (i = 0; i < totcos; i++) {
			dm->getVertCo(dm, i, r_cos[i]);
		}
	}
}

void DM_add_named_tangent_layer_for_uv(
        CustomData *uv_data, CustomData *tan_data, int numLoopData,
        const char *layer_name)
{
	if (CustomData_get_named_layer_index(tan_data, CD_TANGENT, layer_name) == -1 &&
	    CustomData_get_named_layer_index(uv_data, CD_MLOOPUV, layer_name) != -1)
	{
		CustomData_add_layer_named(
		        tan_data, CD_TANGENT, CD_CALLOC, NULL,
		        numLoopData, layer_name);
	}
}

void DM_calc_loop_tangents(
        DerivedMesh *dm, bool calc_active_tangent,
        const char (*tangent_names)[MAX_NAME], int tangent_names_len)
{
	BKE_mesh_calc_loop_tangent_ex(
	        dm->getVertArray(dm),
	        dm->getPolyArray(dm), dm->getNumPolys(dm),
	        dm->getLoopArray(dm),
	        dm->getLoopTriArray(dm), dm->getNumLoopTri(dm),
	        &dm->loopData,
	        calc_active_tangent,
	        tangent_names, tangent_names_len,
	        CustomData_get_layer(&dm->polyData, CD_NORMAL),
	        dm->getLoopDataArray(dm, CD_NORMAL),
	        dm->getVertDataArray(dm, CD_ORCO),  /* may be NULL */
	        /* result */
	        &dm->loopData, dm->getNumLoops(dm),
	        &dm->tangent_mask);
}

/* Set object's bounding box based on DerivedMesh min/max data */
void DM_set_object_boundbox(Object *ob, DerivedMesh *dm)
{
	float min[3], max[3];

	INIT_MINMAX(min, max);
	dm->getMinMax(dm, min, max);

	if (!ob->bb)
		ob->bb = MEM_callocN(sizeof(BoundBox), "DM-BoundBox");

	BKE_boundbox_init_from_minmax(ob->bb, min, max);

	ob->bb->flag &= ~BOUNDBOX_DIRTY;
}

void DM_init_origspace(DerivedMesh *dm)
{
	const float default_osf[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

	OrigSpaceLoop *lof_array = CustomData_get_layer(&dm->loopData, CD_ORIGSPACE_MLOOP);
	const int numpoly = dm->getNumPolys(dm);
	// const int numloop = dm->getNumLoops(dm);
	MVert *mv = dm->getVertArray(dm);
	MLoop *ml = dm->getLoopArray(dm);
	MPoly *mp = dm->getPolyArray(dm);
	int i, j, k;

	float (*vcos_2d)[2] = NULL;
	BLI_array_staticdeclare(vcos_2d, 64);

	for (i = 0; i < numpoly; i++, mp++) {
		OrigSpaceLoop *lof = lof_array + mp->loopstart;

		if (mp->totloop == 3 || mp->totloop == 4) {
			for (j = 0; j < mp->totloop; j++, lof++) {
				copy_v2_v2(lof->uv, default_osf[j]);
			}
		}
		else {
			MLoop *l = &ml[mp->loopstart];
			float p_nor[3], co[3];
			float mat[3][3];

			float min[2] = {FLT_MAX, FLT_MAX}, max[2] = {-FLT_MAX, -FLT_MAX};
			float translate[2], scale[2];

			BKE_mesh_calc_poly_normal(mp, l, mv, p_nor);
			axis_dominant_v3_to_m3(mat, p_nor);

			BLI_array_clear(vcos_2d);
			BLI_array_reserve(vcos_2d, mp->totloop);
			for (j = 0; j < mp->totloop; j++, l++) {
				mul_v3_m3v3(co, mat, mv[l->v].co);
				copy_v2_v2(vcos_2d[j], co);

				for (k = 0; k < 2; k++) {
					if (co[k] > max[k])
						max[k] = co[k];
					else if (co[k] < min[k])
						min[k] = co[k];
				}
			}

			/* Brings min to (0, 0). */
			negate_v2_v2(translate, min);

			/* Scale will bring max to (1, 1). */
			sub_v2_v2v2(scale, max, min);
			if (scale[0] == 0.0f)
				scale[0] = 1e-9f;
			if (scale[1] == 0.0f)
				scale[1] = 1e-9f;
			invert_v2(scale);

			/* Finally, transform all vcos_2d into ((0, 0), (1, 1)) square and assing them as origspace. */
			for (j = 0; j < mp->totloop; j++, lof++) {
				add_v2_v2v2(lof->uv, vcos_2d[j], translate);
				mul_v2_v2(lof->uv, scale);
			}
		}
	}

	dm->dirty |= DM_DIRTY_TESS_CDLAYERS;
	BLI_array_free(vcos_2d);
}

static void mesh_init_origspace(Mesh *mesh)
{
	const float default_osf[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

	OrigSpaceLoop *lof_array = CustomData_get_layer(&mesh->ldata, CD_ORIGSPACE_MLOOP);
	const int numpoly = mesh->totpoly;
	// const int numloop = mesh->totloop;
	MVert *mv = mesh->mvert;
	MLoop *ml = mesh->mloop;
	MPoly *mp = mesh->mpoly;
	int i, j, k;

	float (*vcos_2d)[2] = NULL;
	BLI_array_staticdeclare(vcos_2d, 64);

	for (i = 0; i < numpoly; i++, mp++) {
		OrigSpaceLoop *lof = lof_array + mp->loopstart;

		if (mp->totloop == 3 || mp->totloop == 4) {
			for (j = 0; j < mp->totloop; j++, lof++) {
				copy_v2_v2(lof->uv, default_osf[j]);
			}
		}
		else {
			MLoop *l = &ml[mp->loopstart];
			float p_nor[3], co[3];
			float mat[3][3];

			float min[2] = {FLT_MAX, FLT_MAX}, max[2] = {-FLT_MAX, -FLT_MAX};
			float translate[2], scale[2];

			BKE_mesh_calc_poly_normal(mp, l, mv, p_nor);
			axis_dominant_v3_to_m3(mat, p_nor);

			BLI_array_clear(vcos_2d);
			BLI_array_reserve(vcos_2d, mp->totloop);
			for (j = 0; j < mp->totloop; j++, l++) {
				mul_v3_m3v3(co, mat, mv[l->v].co);
				copy_v2_v2(vcos_2d[j], co);

				for (k = 0; k < 2; k++) {
					if (co[k] > max[k])
						max[k] = co[k];
					else if (co[k] < min[k])
						min[k] = co[k];
				}
			}

			/* Brings min to (0, 0). */
			negate_v2_v2(translate, min);

			/* Scale will bring max to (1, 1). */
			sub_v2_v2v2(scale, max, min);
			if (scale[0] == 0.0f)
				scale[0] = 1e-9f;
			if (scale[1] == 0.0f)
				scale[1] = 1e-9f;
			invert_v2(scale);

			/* Finally, transform all vcos_2d into ((0, 0), (1, 1)) square and assing them as origspace. */
			for (j = 0; j < mp->totloop; j++, lof++) {
				add_v2_v2v2(lof->uv, vcos_2d[j], translate);
				mul_v2_v2(lof->uv, scale);
			}
		}
	}

	BKE_mesh_tessface_clear(mesh);
	BLI_array_free(vcos_2d);
}


/* derivedmesh info printing function,
 * to help track down differences DM output */

#ifndef NDEBUG
#include "BLI_dynstr.h"

static void dm_debug_info_layers(
        DynStr *dynstr, DerivedMesh *dm, CustomData *cd,
        void *(*getElemDataArray)(DerivedMesh *, int))
{
	int type;

	for (type = 0; type < CD_NUMTYPES; type++) {
		if (CustomData_has_layer(cd, type)) {
			/* note: doesnt account for multiple layers */
			const char *name = CustomData_layertype_name(type);
			const int size = CustomData_sizeof(type);
			const void *pt = getElemDataArray(dm, type);
			const int pt_size = pt ? (int)(MEM_allocN_len(pt) / size) : 0;
			const char *structname;
			int structnum;
			CustomData_file_write_info(type, &structname, &structnum);
			BLI_dynstr_appendf(dynstr,
			                   "        dict(name='%s', struct='%s', type=%d, ptr='%p', elem=%d, length=%d),\n",
			                   name, structname, type, (const void *)pt, size, pt_size);
		}
	}
}

char *DM_debug_info(DerivedMesh *dm)
{
	DynStr *dynstr = BLI_dynstr_new();
	char *ret;
	const char *tstr;

	BLI_dynstr_appendf(dynstr, "{\n");
	BLI_dynstr_appendf(dynstr, "    'ptr': '%p',\n", (void *)dm);
	switch (dm->type) {
		case DM_TYPE_CDDM:     tstr = "DM_TYPE_CDDM";     break;
		case DM_TYPE_EDITBMESH: tstr = "DM_TYPE_EDITMESH";  break;
		case DM_TYPE_CCGDM:    tstr = "DM_TYPE_CCGDM";     break;
		default:               tstr = "UNKNOWN";           break;
	}
	BLI_dynstr_appendf(dynstr, "    'type': '%s',\n", tstr);
	BLI_dynstr_appendf(dynstr, "    'numVertData': %d,\n", dm->numVertData);
	BLI_dynstr_appendf(dynstr, "    'numEdgeData': %d,\n", dm->numEdgeData);
	BLI_dynstr_appendf(dynstr, "    'numTessFaceData': %d,\n", dm->numTessFaceData);
	BLI_dynstr_appendf(dynstr, "    'numPolyData': %d,\n", dm->numPolyData);
	BLI_dynstr_appendf(dynstr, "    'deformedOnly': %d,\n", dm->deformedOnly);

	BLI_dynstr_appendf(dynstr, "    'vertexLayers': (\n");
	dm_debug_info_layers(dynstr, dm, &dm->vertData, dm->getVertDataArray);
	BLI_dynstr_appendf(dynstr, "    ),\n");

	BLI_dynstr_appendf(dynstr, "    'edgeLayers': (\n");
	dm_debug_info_layers(dynstr, dm, &dm->edgeData, dm->getEdgeDataArray);
	BLI_dynstr_appendf(dynstr, "    ),\n");

	BLI_dynstr_appendf(dynstr, "    'loopLayers': (\n");
	dm_debug_info_layers(dynstr, dm, &dm->loopData, dm->getLoopDataArray);
	BLI_dynstr_appendf(dynstr, "    ),\n");

	BLI_dynstr_appendf(dynstr, "    'polyLayers': (\n");
	dm_debug_info_layers(dynstr, dm, &dm->polyData, dm->getPolyDataArray);
	BLI_dynstr_appendf(dynstr, "    ),\n");

	BLI_dynstr_appendf(dynstr, "    'tessFaceLayers': (\n");
	dm_debug_info_layers(dynstr, dm, &dm->faceData, dm->getTessFaceDataArray);
	BLI_dynstr_appendf(dynstr, "    ),\n");

	BLI_dynstr_appendf(dynstr, "}\n");

	ret = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return ret;
}

void DM_debug_print(DerivedMesh *dm)
{
	char *str = DM_debug_info(dm);
	puts(str);
	fflush(stdout);
	MEM_freeN(str);
}

void DM_debug_print_cdlayers(CustomData *data)
{
	int i;
	const CustomDataLayer *layer;

	printf("{\n");

	for (i = 0, layer = data->layers; i < data->totlayer; i++, layer++) {

		const char *name = CustomData_layertype_name(layer->type);
		const int size = CustomData_sizeof(layer->type);
		const char *structname;
		int structnum;
		CustomData_file_write_info(layer->type, &structname, &structnum);
		printf("        dict(name='%s', struct='%s', type=%d, ptr='%p', elem=%d, length=%d),\n",
		       name, structname, layer->type, (const void *)layer->data, size, (int)(MEM_allocN_len(layer->data) / size));
	}

	printf("}\n");
}

bool DM_is_valid(DerivedMesh *dm)
{
	const bool do_verbose = true;
	const bool do_fixes = false;

	bool is_valid = true;
	bool changed = true;

	is_valid &= BKE_mesh_validate_all_customdata(
	        dm->getVertDataLayout(dm),
	        dm->getEdgeDataLayout(dm),
	        dm->getLoopDataLayout(dm),
	        dm->getPolyDataLayout(dm),
	        false,  /* setting mask here isn't useful, gives false positives */
	        do_verbose, do_fixes, &changed);

	is_valid &= BKE_mesh_validate_arrays(
	        NULL,
	        dm->getVertArray(dm), dm->getNumVerts(dm),
	        dm->getEdgeArray(dm), dm->getNumEdges(dm),
	        dm->getTessFaceArray(dm), dm->getNumTessFaces(dm),
	        dm->getLoopArray(dm), dm->getNumLoops(dm),
	        dm->getPolyArray(dm), dm->getNumPolys(dm),
	        dm->getVertDataArray(dm, CD_MDEFORMVERT),
	        do_verbose, do_fixes, &changed);

	BLI_assert(changed == false);

	return is_valid;
}

#endif /* NDEBUG */

/* -------------------------------------------------------------------- */

MVert *DM_get_vert_array(DerivedMesh *dm, bool *allocated)
{
	CustomData *vert_data = dm->getVertDataLayout(dm);
	MVert *mvert = CustomData_get_layer(vert_data, CD_MVERT);
	*allocated = false;

	if (mvert == NULL) {
		mvert = MEM_malloc_arrayN(dm->getNumVerts(dm), sizeof(MVert), "dmvh vert data array");
		dm->copyVertArray(dm, mvert);
		*allocated = true;
	}

	return mvert;
}

MEdge *DM_get_edge_array(DerivedMesh *dm, bool *allocated)
{
	CustomData *edge_data = dm->getEdgeDataLayout(dm);
	MEdge *medge = CustomData_get_layer(edge_data, CD_MEDGE);
	*allocated = false;

	if (medge == NULL) {
		medge = MEM_malloc_arrayN(dm->getNumEdges(dm), sizeof(MEdge), "dm medge data array");
		dm->copyEdgeArray(dm, medge);
		*allocated = true;
	}

	return medge;
}

MLoop *DM_get_loop_array(DerivedMesh *dm, bool *r_allocated)
{
	CustomData *loop_data = dm->getLoopDataLayout(dm);
	MLoop *mloop = CustomData_get_layer(loop_data, CD_MLOOP);
	*r_allocated = false;

	if (mloop == NULL) {
		mloop = MEM_malloc_arrayN(dm->getNumLoops(dm), sizeof(MLoop), "dm loop data array");
		dm->copyLoopArray(dm, mloop);
		*r_allocated = true;
	}

	return mloop;
}

MPoly *DM_get_poly_array(DerivedMesh *dm, bool *r_allocated)
{
	CustomData *poly_data = dm->getPolyDataLayout(dm);
	MPoly *mpoly = CustomData_get_layer(poly_data, CD_MPOLY);
	*r_allocated = false;

	if (mpoly == NULL) {
		mpoly = MEM_malloc_arrayN(dm->getNumPolys(dm), sizeof(MPoly), "dm poly data array");
		dm->copyPolyArray(dm, mpoly);
		*r_allocated = true;
	}

	return mpoly;
}

MFace *DM_get_tessface_array(DerivedMesh *dm, bool *r_allocated)
{
	CustomData *tessface_data = dm->getTessFaceDataLayout(dm);
	MFace *mface = CustomData_get_layer(tessface_data, CD_MFACE);
	*r_allocated = false;

	if (mface == NULL) {
		int numTessFaces = dm->getNumTessFaces(dm);

		if (numTessFaces > 0) {
			mface = MEM_malloc_arrayN(numTessFaces, sizeof(MFace), "bvh mface data array");
			dm->copyTessFaceArray(dm, mface);
			*r_allocated = true;
		}
	}

	return mface;
}
