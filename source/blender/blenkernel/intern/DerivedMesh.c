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
#include "limits.h"

#include "MEM_guardedalloc.h"

#include "DNA_cloth_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h" // N_T

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_array.h"
#include "BLI_pbvh.h"
#include "BLI_utildefines.h"
#include "BLI_linklist.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_key.h"
#include "BKE_modifier.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_texture.h"
#include "BKE_multires.h"
#include "BKE_armature.h"
#include "BKE_particle.h"
#include "BKE_tessmesh.h"
#include "BKE_bvhutils.h"
#include "BKE_deform.h"

#ifdef WITH_GAMEENGINE
#include "BKE_navmesh_conversion.h"
static DerivedMesh *navmesh_dm_createNavMeshForVisualization(DerivedMesh *dm);
#endif

#include "BLO_sys_types.h" // for intptr_t support

#include "GL/glew.h"

#include "GPU_buffers.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_material.h"

extern GLubyte stipple_quarttone[128]; /* glutil.c, bad level data */

static void add_shapekey_layers(DerivedMesh *dm, Mesh *me, Object *ob);
static void shapekey_layers_to_keyblocks(DerivedMesh *dm, Mesh *me, int actshape_uid);

///////////////////////////////////
///////////////////////////////////

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
		mface = CustomData_add_layer(&dm->faceData, CD_MFACE, CD_CALLOC, NULL,
			dm->getNumTessFaces(dm));
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
	MVert *tmp = MEM_callocN(sizeof(*tmp) * dm->getNumVerts(dm),
							 "dm_dupVertArray tmp");

	if(tmp) dm->copyVertArray(dm, tmp);

	return tmp;
}

static MEdge *dm_dupEdgeArray(DerivedMesh *dm)
{
	MEdge *tmp = MEM_callocN(sizeof(*tmp) * dm->getNumEdges(dm),
							 "dm_dupEdgeArray tmp");

	if(tmp) dm->copyEdgeArray(dm, tmp);

	return tmp;
}

static MFace *dm_dupFaceArray(DerivedMesh *dm)
{
	MFace *tmp = MEM_callocN(sizeof(*tmp) * dm->getNumTessFaces(dm),
							 "dm_dupFaceArray tmp");

	if(tmp) dm->copyTessFaceArray(dm, tmp);

	return tmp;
}

static MLoop *dm_dupLoopArray(DerivedMesh *dm)
{
	MLoop *tmp = MEM_callocN(sizeof(*tmp) * dm->getNumLoops(dm),
							 "dm_dupLoopArray tmp");

	if(tmp) dm->copyLoopArray(dm, tmp);

	return tmp;
}

static MPoly *dm_dupPolyArray(DerivedMesh *dm)
{
	MPoly *tmp = MEM_callocN(sizeof(*tmp) * dm->getNumPolys(dm),
							 "dm_dupPolyArray tmp");

	if(tmp) dm->copyPolyArray(dm, tmp);

	return tmp;
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

	dm->getVertDataLayout = dm_getVertCData;
	dm->getEdgeDataLayout = dm_getEdgeCData;
	dm->getTessFaceDataLayout = dm_getTessFaceCData;
	dm->getLoopDataLayout = dm_getLoopCData;
	dm->getPolyDataLayout = dm_getPolyCData;

	dm->getVertData = DM_get_vert_data;
	dm->getEdgeData = DM_get_edge_data;
	dm->getTessFaceData = DM_get_tessface_data;
	dm->getVertDataArray = DM_get_vert_data_layer;
	dm->getEdgeDataArray = DM_get_edge_data_layer;
	dm->getTessFaceDataArray = DM_get_tessface_data_layer;

	bvhcache_init(&dm->bvhCache);
}

void DM_init(DerivedMesh *dm, DerivedMeshType type, int numVerts, int numEdges,
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
	dm->auto_bump_scale = -1.0f;
}

void DM_from_template(DerivedMesh *dm, DerivedMesh *source, DerivedMeshType type,
                      int numVerts, int numEdges, int numTessFaces,
                      int numLoops, int numPolys)
{
	CustomData_copy(&source->vertData, &dm->vertData, CD_MASK_DERIVEDMESH,
					CD_CALLOC, numVerts);
	CustomData_copy(&source->edgeData, &dm->edgeData, CD_MASK_DERIVEDMESH,
					CD_CALLOC, numEdges);
	CustomData_copy(&source->faceData, &dm->faceData, CD_MASK_DERIVEDMESH,
					CD_CALLOC, numTessFaces);
	CustomData_copy(&source->loopData, &dm->loopData, CD_MASK_DERIVEDMESH,
	                CD_CALLOC, numLoops);
	CustomData_copy(&source->polyData, &dm->polyData, CD_MASK_DERIVEDMESH,
	                CD_CALLOC, numPolys);

	dm->type = type;
	dm->numVertData = numVerts;
	dm->numEdgeData = numEdges;
	dm->numTessFaceData = numTessFaces;
	dm->numLoopData = numLoops;
	dm->numPolyData = numPolys;

	DM_init_funcs(dm);

	dm->needsFree = 1;
}

int DM_release(DerivedMesh *dm)
{
	if (dm->needsFree) {
		bvhcache_free(&dm->bvhCache);
		GPU_drawobject_free( dm );
		CustomData_free(&dm->vertData, dm->numVertData);
		CustomData_free(&dm->edgeData, dm->numEdgeData);
		CustomData_free(&dm->faceData, dm->numTessFaceData);
		CustomData_free(&dm->loopData, dm->numLoopData);
		CustomData_free(&dm->polyData, dm->numPolyData);

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

/* note: until all modifiers can take MPoly's as input,
 * use this at the start of modifiers  */
void DM_ensure_tessface(DerivedMesh *dm)
{
	const int numTessFaces = dm->getNumTessFaces(dm);
	const int numPolys =     dm->getNumPolys(dm);

	if ( (numTessFaces == 0) && (numPolys != 0)) {
		dm->recalcTesselation(dm);

		if (dm->getNumTessFaces(dm) != 0) {
			/* printf("info %s: polys -> ngons calculated\n", __func__); */
		}
		else {
			printf("warning %s: could not create tessfaces from %d polygons, dm->type=%d\n",
			       __func__, numPolys, dm->type);
		}
	}
}

void DM_to_mesh(DerivedMesh *dm, Mesh *me, Object *ob)
{
	/* dm might depend on me, so we need to do everything with a local copy */
	Mesh tmp = *me;
	int totvert, totedge /*, totface */ /* UNUSED */, totloop, totpoly;
	int did_shapekeys=0;
	
	memset(&tmp.vdata, 0, sizeof(tmp.vdata));
	memset(&tmp.edata, 0, sizeof(tmp.edata));
	memset(&tmp.fdata, 0, sizeof(tmp.fdata));
	memset(&tmp.ldata, 0, sizeof(tmp.ldata));
	memset(&tmp.pdata, 0, sizeof(tmp.pdata));

	totvert = tmp.totvert = dm->getNumVerts(dm);
	totedge = tmp.totedge = dm->getNumEdges(dm);
	totloop = tmp.totloop = dm->getNumLoops(dm);
	totpoly = tmp.totpoly = dm->getNumPolys(dm);

	CustomData_copy(&dm->vertData, &tmp.vdata, CD_MASK_MESH, CD_DUPLICATE, totvert);
	CustomData_copy(&dm->edgeData, &tmp.edata, CD_MASK_MESH, CD_DUPLICATE, totedge);
	CustomData_copy(&dm->loopData, &tmp.ldata, CD_MASK_MESH, CD_DUPLICATE, totloop);
	CustomData_copy(&dm->polyData, &tmp.pdata, CD_MASK_MESH, CD_DUPLICATE, totpoly);

	if (CustomData_has_layer(&dm->vertData, CD_SHAPEKEY)) {
		KeyBlock *kb;
		int uid;
		
		if (ob) {
			kb = BLI_findlink(&me->key->block, ob->shapenr-1);
			if (kb) {
				uid = kb->uid;
			}
			else {
				printf("%s: error - could not find active shapekey %d!\n",
				       __func__, ob->shapenr-1);

				uid = INT_MAX;
			}
		}
		else {
			/*if no object, set to INT_MAX so we don't mess up any shapekey layers*/
			uid = INT_MAX;
		}

		shapekey_layers_to_keyblocks(dm, me, uid);
		did_shapekeys = 1;
	}
	
	/* not all DerivedMeshes store their verts/edges/faces in CustomData, so
	   we set them here in case they are missing */
	if(!CustomData_has_layer(&tmp.vdata, CD_MVERT))
		CustomData_add_layer(&tmp.vdata, CD_MVERT, CD_ASSIGN, dm->dupVertArray(dm), totvert);
	if(!CustomData_has_layer(&tmp.edata, CD_MEDGE))
		CustomData_add_layer(&tmp.edata, CD_MEDGE, CD_ASSIGN, dm->dupEdgeArray(dm), totedge);
	if(!CustomData_has_layer(&tmp.pdata, CD_MPOLY)) {
		tmp.mloop = dm->dupLoopArray(dm);
		tmp.mpoly = dm->dupPolyArray(dm);

		CustomData_add_layer(&tmp.ldata, CD_MLOOP, CD_ASSIGN, tmp.mloop, tmp.totloop);
		CustomData_add_layer(&tmp.pdata, CD_MPOLY, CD_ASSIGN, tmp.mpoly, tmp.totpoly);
	}

	/* object had got displacement layer, should copy this layer to save sculpted data */
	/* NOTE: maybe some other layers should be copied? nazgul */
	if(CustomData_has_layer(&me->ldata, CD_MDISPS)) {
		if (totloop == me->totloop) {
			MDisps *mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
			CustomData_add_layer(&tmp.ldata, CD_MDISPS, CD_DUPLICATE, mdisps, totloop);
		}
	}

	/* yes, must be before _and_ after tesselate */
	mesh_update_customdata_pointers(&tmp, TRUE);

	BKE_mesh_tessface_calc(&tmp);

	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->fdata, me->totface);
	CustomData_free(&me->ldata, me->totloop);
	CustomData_free(&me->pdata, me->totpoly);

	/*  ok, this should now use new CD shapekey data,
	    which shouuld be fed through the modifier 
		stack*/
	if(tmp.totvert != me->totvert && !did_shapekeys && me->key) {
		printf("YEEK! this should be recoded! Shape key loss!!!\n");
		if(tmp.key) tmp.key->id.us--;
		tmp.key = NULL;
	}

	*me = tmp;
}

void DM_to_meshkey(DerivedMesh *dm, Mesh *me, KeyBlock *kb)
{
	int a, totvert = dm->getNumVerts(dm);
	float *fp;
	MVert *mvert;
	
	if(totvert==0 || me->totvert==0 || me->totvert!=totvert) return;
	
	if(kb->data) MEM_freeN(kb->data);
	kb->data= MEM_callocN(me->key->elemsize*me->totvert, "kb->data");
	kb->totelem= totvert;
	
	fp= kb->data;
	mvert=dm->getVertDataArray(dm, CD_MVERT);
	
	for(a=0; a<kb->totelem; a++, fp+=3, mvert++) {
		copy_v3_v3(fp, mvert->co);
	}
}

void DM_set_only_copy(DerivedMesh *dm, CustomDataMask mask)
{
	CustomData_set_only_copy(&dm->vertData, mask);
	CustomData_set_only_copy(&dm->edgeData, mask);
	CustomData_set_only_copy(&dm->faceData, mask);
}

void DM_add_vert_layer(DerivedMesh *dm, int type, int alloctype, void *layer)
{
	CustomData_add_layer(&dm->vertData, type, alloctype, layer, dm->numVertData);
}

void DM_add_edge_layer(DerivedMesh *dm, int type, int alloctype, void *layer)
{
	CustomData_add_layer(&dm->edgeData, type, alloctype, layer, dm->numEdgeData);
}

void DM_add_tessface_layer(DerivedMesh *dm, int type, int alloctype, void *layer)
{
	CustomData_add_layer(&dm->faceData, type, alloctype, layer, dm->numTessFaceData);
}

void DM_add_loop_layer(DerivedMesh *dm, int type, int alloctype, void *layer)
{
	CustomData_add_layer(&dm->loopData, type, alloctype, layer, dm->numLoopData);
}

void DM_add_poly_layer(DerivedMesh *dm, int type, int alloctype, void *layer)
{
	CustomData_add_layer(&dm->polyData, type, alloctype, layer, dm->numPolyData);
}

void *DM_get_vert_data(DerivedMesh *dm, int index, int type)
{
	return CustomData_get(&dm->vertData, index, type);
}

void *DM_get_edge_data(DerivedMesh *dm, int index, int type)
{
	return CustomData_get(&dm->edgeData, index, type);
}

void *DM_get_tessface_data(DerivedMesh *dm, int index, int type)
{
	return CustomData_get(&dm->faceData, index, type);
}

void *DM_get_vert_data_layer(DerivedMesh *dm, int type)
{
	if(type == CD_MVERT)
		return dm->getVertArray(dm);

	return CustomData_get_layer(&dm->vertData, type);
}

void *DM_get_edge_data_layer(DerivedMesh *dm, int type)
{
	if(type == CD_MEDGE)
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

void DM_interp_vert_data(DerivedMesh *source, DerivedMesh *dest,
						 int *src_indices, float *weights,
						 int count, int dest_index)
{
	CustomData_interp(&source->vertData, &dest->vertData, src_indices,
					  weights, NULL, count, dest_index);
}

void DM_interp_edge_data(DerivedMesh *source, DerivedMesh *dest,
						 int *src_indices,
						 float *weights, EdgeVertWeight *vert_weights,
						 int count, int dest_index)
{
	CustomData_interp(&source->edgeData, &dest->edgeData, src_indices,
					  weights, (float*)vert_weights, count, dest_index);
}

void DM_interp_tessface_data(DerivedMesh *source, DerivedMesh *dest,
						 int *src_indices,
						 float *weights, FaceVertWeight *vert_weights,
						 int count, int dest_index)
{
	CustomData_interp(&source->faceData, &dest->faceData, src_indices,
					  weights, (float*)vert_weights, count, dest_index);
}

void DM_swap_tessface_data(DerivedMesh *dm, int index, const int *corner_indices)
{
	CustomData_swap(&dm->faceData, index, corner_indices);
}

void DM_interp_loop_data(DerivedMesh *source, DerivedMesh *dest,
                         int *src_indices,
                         float *weights, int count, int dest_index)
{
	CustomData_interp(&source->loopData, &dest->loopData, src_indices,
	                  weights, NULL, count, dest_index);
}

void DM_interp_poly_data(DerivedMesh *source, DerivedMesh *dest,
                         int *src_indices,
                         float *weights, int count, int dest_index)
{
	CustomData_interp(&source->polyData, &dest->polyData, src_indices,
	                  weights, NULL, count, dest_index);
}

///
DerivedMesh *mesh_create_derived(Mesh *me, Object *ob, float (*vertCos)[3])
{
	DerivedMesh *dm = CDDM_from_mesh(me, ob);
	
	if(!dm)
		return NULL;
	
	if (vertCos)
		CDDM_apply_vert_coords(dm, vertCos);

	CDDM_calc_normals(dm);

	return dm;
}

/***/

DerivedMesh *mesh_create_derived_for_modifier(Scene *scene, Object *ob, 
                                              ModifierData *md, int build_shapekey_layers)
{
	Mesh *me = ob->data;
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	DerivedMesh *dm;
	KeyBlock *kb;

	md->scene= scene;
	
	if (!(md->mode&eModifierMode_Realtime)) return NULL;
	if (mti->isDisabled && mti->isDisabled(md, 0)) return NULL;
	
	if (build_shapekey_layers && me->key && (kb = BLI_findlink(&me->key->block, ob->shapenr-1))) {
		key_to_mesh(kb, me);
	}
	
	if (mti->type==eModifierTypeType_OnlyDeform) {
		int numVerts;
		float (*deformedVerts)[3] = mesh_getVertexCos(me, &numVerts);

		mti->deformVerts(md, ob, NULL, deformedVerts, numVerts, 0, 0);
		dm = mesh_create_derived(me, ob, deformedVerts);

		if (build_shapekey_layers)
			add_shapekey_layers(dm, me, ob);
		
		MEM_freeN(deformedVerts);
	} else {
		DerivedMesh *tdm = mesh_create_derived(me, ob, NULL);

		if (build_shapekey_layers)
			add_shapekey_layers(tdm, me, ob);
		
		dm = mti->applyModifier(md, ob, tdm, 0, 0);

		if(tdm != dm) tdm->release(tdm);
	}

	return dm;
}

static float *get_editbmesh_orco_verts(BMEditMesh *em)
{
	BMIter iter;
	BMVert *eve;
	float *orco;
	int a, totvert;

	/* these may not really be the orco's, but it's only for preview.
	 * could be solver better once, but isn't simple */

	totvert= em->bm->totvert;
	
	orco = MEM_mallocN(sizeof(float)*3*totvert, "BMEditMesh Orco");

	eve = BM_iter_new(&iter, em->bm, BM_VERTS_OF_MESH, NULL);
	for (a=0; eve; eve=BM_iter_step(&iter), a+=3) {
		copy_v3_v3(orco+a, eve->co);
	}
	
	return orco;
}

/* orco custom data layer */
static void *get_orco_coords_dm(Object *ob, BMEditMesh *em, int layer, int *free)
{
	*free= 0;

	if(layer == CD_ORCO) {
		/* get original coordinates */
		*free= 1;

		if(em)
			return (float(*)[3])get_editbmesh_orco_verts(em);
		else
			return (float(*)[3])get_mesh_orco_verts(ob);
	}
	else if(layer == CD_CLOTH_ORCO) {
		/* apply shape key for cloth, this should really be solved
		   by a more flexible customdata system, but not simple */
		if(!em) {
			ClothModifierData *clmd = (ClothModifierData *)modifiers_findByType(ob, eModifierType_Cloth);
			KeyBlock *kb= key_get_keyblock(ob_get_key(ob), clmd->sim_parms->shapekey_rest);

			if(kb->data)
				return kb->data;
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

	if(em) dm= CDDM_from_BMEditMesh(em, me, FALSE, FALSE);
	else dm= CDDM_from_mesh(me, ob);

	orco= get_orco_coords_dm(ob, em, layer, &free);

	if(orco) {
		CDDM_apply_vert_coords(dm, orco);
		if(free) MEM_freeN(orco);
	}

	CDDM_calc_normals(dm);

	return dm;
}

static void add_orco_dm(Object *ob, BMEditMesh *em, DerivedMesh *dm,
						DerivedMesh *orcodm, int layer)
{
	float (*orco)[3], (*layerorco)[3];
	int totvert, free;

	totvert= dm->getNumVerts(dm);

	if(orcodm) {
		orco= MEM_callocN(sizeof(float)*3*totvert, "dm orco");
		free= 1;

		if(orcodm->getNumVerts(orcodm) == totvert)
			orcodm->getVertCos(orcodm, orco);
		else
			dm->getVertCos(dm, orco);
	}
	else
		orco= get_orco_coords_dm(ob, em, layer, &free);

	if(orco) {
		if(layer == CD_ORCO)
			transform_mesh_orco_verts(ob->data, orco, totvert, 0);

		if(!(layerorco = DM_get_vert_data_layer(dm, layer))) {
			DM_add_vert_layer(dm, layer, CD_CALLOC, NULL);
			layerorco = DM_get_vert_data_layer(dm, layer);
		}

		memcpy(layerorco, orco, sizeof(float)*3*totvert);
		if(free) MEM_freeN(orco);
	}
}

/* weight paint colors */

/* Something of a hack, at the moment deal with weightpaint
 * by tucking into colors during modifier eval, only in
 * wpaint mode. Works ok but need to make sure recalc
 * happens on enter/exit wpaint.
 */

void weight_to_rgb(float r_rgb[3], const float weight)
{
	const float blend= ((weight/2.0f)+0.5f);

	if (weight<=0.25f) {	// blue->cyan
		r_rgb[0]= 0.0f;
		r_rgb[1]= blend*weight*4.0f;
		r_rgb[2]= blend;
	}
	else if (weight<=0.50f) {	// cyan->green
		r_rgb[0]= 0.0f;
		r_rgb[1]= blend;
		r_rgb[2]= blend*(1.0f-((weight-0.25f)*4.0f));
	}
	else if (weight <= 0.75f) {	// green->yellow
		r_rgb[0]= blend * ((weight-0.50f)*4.0f);
		r_rgb[1]= blend;
		r_rgb[2]= 0.0f;
	}
	else if (weight <= 1.0f) { // yellow->red
		r_rgb[0]= blend;
		r_rgb[1]= blend * (1.0f-((weight-0.75f)*4.0f));
		r_rgb[2]= 0.0f;
	}
	else {
		/* exceptional value, unclamped or nan,
		 * avoid uninitialized memory use */
		r_rgb[0]= 1.0f;
		r_rgb[1]= 0.0f;
		r_rgb[2]= 1.0f;
	}
}

/* draw_flag's for calc_weightpaint_vert_color */
enum {
	CALC_WP_MULTIPAINT= (1<<0),
	CALC_WP_AUTO_NORMALIZE= (1<<1)
};

static void weightpaint_color(unsigned char r_col[4], ColorBand *coba, const float input)
{
	float colf[4];

	if(coba) do_colorband(coba, input, colf);
	else     weight_to_rgb(colf, input);

	r_col[3] = (unsigned char)(colf[0] * 255.0f);
	r_col[2] = (unsigned char)(colf[1] * 255.0f);
	r_col[1] = (unsigned char)(colf[2] * 255.0f);
	r_col[0] = 255;
}


static void calc_weightpaint_vert_color(
        unsigned char r_col[4],
        MDeformVert *dv, ColorBand *coba,
        const int defbase_tot, const int defbase_act,
        const char *dg_flags,
        const int selected, const int draw_flag)
{
	float input = 0.0f;
	
	int make_black= FALSE;

	if ((selected > 1) && (draw_flag & CALC_WP_MULTIPAINT)) {
		int was_a_nonzero= FALSE;
		unsigned int i;

		MDeformWeight *dw= dv->dw;
		for (i = dv->totweight; i != 0; i--, dw++) {
			/* in multipaint, get the average if auto normalize is inactive
			 * get the sum if it is active */
			if (dw->def_nr < defbase_tot) {
				if (dg_flags[dw->def_nr]) {
					if (dw->weight) {
						input += dw->weight;
						was_a_nonzero= TRUE;
					}
				}
			}
		}

		/* make it black if the selected groups have no weight on a vertex */
		if (was_a_nonzero == FALSE) {
			make_black = TRUE;
		}
		else if ((draw_flag & CALC_WP_AUTO_NORMALIZE) == FALSE) {
			input /= selected; /* get the average */
		}
	}
	else {
		/* default, non tricky behavior */
		input= defvert_find_weight(dv, defbase_act);
	}

	if (make_black) { /* TODO, theme color */
		r_col[3] = 0;
		r_col[2] = 0;
		r_col[1] = 0;
		r_col[0] = 255;
	}
	else {
		CLAMP(input, 0.0f, 1.0f);
		weightpaint_color(r_col, coba, input);
	}
}

static ColorBand *stored_cb= NULL;

void vDM_ColorBand_store(ColorBand *coba)
{
	stored_cb= coba;
}

/* return an array of vertex weight colors, caller must free.
 *
 * note that we could save some memory and allocate RGB only but then we'd need to
 * re-arrange the colors when copying to the face since MCol has odd ordering,
 * so leave this as is - campbell */
static unsigned char *calc_weightpaint_vert_array(Object *ob, DerivedMesh *dm, int const draw_flag, ColorBand *coba)
{
	MDeformVert *dv = DM_get_vert_data_layer(dm, CD_MDEFORMVERT);
	int numVerts = dm->getNumVerts(dm);
	unsigned char *wtcol_v = MEM_mallocN (sizeof(unsigned char) * numVerts * 4, "weightmap_v");

	if (dv) {
		unsigned char *wc = wtcol_v;
		unsigned int i;

		/* variables for multipaint */
		const int defbase_tot = BLI_countlist(&ob->defbase);
		const int defbase_act = ob->actdef-1;
		char *dg_flags = MEM_mallocN(defbase_tot * sizeof(char), __func__);
		const int selected = get_selected_defgroups(ob, dg_flags, defbase_tot);

		for (i = numVerts; i != 0; i--, wc += 4, dv++) {
			calc_weightpaint_vert_color(wc, dv, coba, defbase_tot, defbase_act, dg_flags, selected, draw_flag);
		}

		MEM_freeN(dg_flags);
	}
	else {
		int col_i;
		weightpaint_color((unsigned char *)&col_i, coba, 0.0f);
		fill_vn_i((int *)wtcol_v, numVerts, col_i);
	}

	return wtcol_v;
}

/* return an array of vertex weight colors from given weights, caller must free.
 *
 * note that we could save some memory and allocate RGB only but then we'd need to
 * re-arrange the colors when copying to the face since MCol has odd ordering,
 * so leave this as is - campbell */
static unsigned char *calc_colors_from_weights_array(const int num, float *weights)
{
	unsigned char *wtcol_v = MEM_mallocN(sizeof(unsigned char) * num * 4, "weightmap_v");
	unsigned char *wc = wtcol_v;
	int i;

	for (i = 0; i < num; i++, wc += 4, weights++)
		weightpaint_color((unsigned char *) wc, NULL, *weights);

	return wtcol_v;
}

void DM_update_weight_mcol(Object *ob, DerivedMesh *dm, int const draw_flag,
                           float *weights, int num, const int *indices)
{
	ColorBand *coba= stored_cb;	/* warning, not a local var */

	unsigned char *wtcol_v;
#if 0 /* See coment below. */
	unsigned char *wtcol_f = dm->getTessFaceDataArray(dm, CD_WEIGHT_MCOL);
#endif
	unsigned char(*wtcol_l)[4] = CustomData_get_layer(dm->getLoopDataLayout(dm), CD_WEIGHT_MLOOPCOL);
#if 0 /* See coment below. */
	MFace *mf = dm->getTessFaceArray(dm);
#endif
	MLoop *mloop = dm->getLoopArray(dm), *ml;
	MPoly *mp = dm->getPolyArray(dm);
	int numFaces = dm->getNumTessFaces(dm);
	int numVerts = dm->getNumVerts(dm);
	int totloop;
	int i, j;

#if 0 /* See comment below */
	/* If no CD_WEIGHT_MCOL existed yet, add a new one! */
	if (!wtcol_f)
		wtcol_f = CustomData_add_layer(&dm->faceData, CD_WEIGHT_MCOL, CD_CALLOC, NULL, numFaces);

	if (wtcol_f) {
		unsigned char *wtcol_f_step = wtcol_f;
# else
	/* XXX We have to create a CD_WEIGHT_MCOL, else it might sigsev (after a SubSurf mod, eg)... */
	if(!dm->getTessFaceDataArray(dm, CD_WEIGHT_MCOL))
		CustomData_add_layer(&dm->faceData, CD_WEIGHT_MCOL, CD_CALLOC, NULL, numFaces);

	{
#endif

		/* Weights are given by caller. */
		if (weights) {
			float *w = weights;
			/* If indices is not NULL, it means we do not have weights for all vertices,
			 * so we must create them (and set them to zero)... */
			if(indices) {
				w = MEM_callocN(sizeof(float)*numVerts, "Temp weight array DM_update_weight_mcol");
				i = num;
				while(i--)
					w[indices[i]] = weights[i];
			}

			/* Convert float weights to colors. */
			wtcol_v = calc_colors_from_weights_array(numVerts, w);

			if(indices)
				MEM_freeN(w);
		}

		/* No weights given, take them from active vgroup(s). */
		else
			wtcol_v = calc_weightpaint_vert_array(ob, dm, draw_flag, coba);

		/* Now copy colors in all face verts. */
		/*first add colors to the tesselation faces*/
		/* XXX Why update that layer? We have to update WEIGHT_MLOOPCOL anyway, 
		 *     and tesselation recreates mface layers from mloop/mpoly ones, so no
		 *     need to fill WEIGHT_MCOL here. */
#if 0
		for (i = 0; i < numFaces; i++, mf++, wtcol_f_step += (4 * 4)) {
			/*origindex being NULL means we're operating on original mesh data*/
#if 0
			unsigned int fidx= mf->v4 ? 3:2;

#else	/* better zero out triangles 4th component. else valgrind complains when the buffer's copied */
			unsigned int fidx;
			if (mf->v4) {
				fidx = 3;
			}
			else {
				fidx = 2;
				*(int *)(&wtcol_f_step[3 * 4]) = 0;
			}
#endif

			do {
				copy_v4_v4_char((char *)&wtcol_f_step[fidx * 4],
				                (char *)&wtcol_v[4 * (*(&mf->v1 + fidx))]);
			} while (fidx--);
		}
#endif
		/*now add to loops, so the data can be passed through the modifier stack*/
		/* If no CD_WEIGHT_MLOOPCOL existed yet, we have to add a new one! */
		if (!wtcol_l) {
			BLI_array_declare(wtcol_l);
			totloop = 0;
			for (i=0; i<dm->numPolyData; i++, mp++) {
				ml = mloop + mp->loopstart;

				BLI_array_growitems(wtcol_l, mp->totloop);
				for (j = 0; j < mp->totloop; j++, ml++, totloop++) {
					copy_v4_v4_char((char *)&wtcol_l[totloop],
					                (char *)&wtcol_v[4 * ml->v]);
				}
			}
			CustomData_add_layer(&dm->loopData, CD_WEIGHT_MLOOPCOL, CD_ASSIGN, wtcol_l, totloop);
		}
		else {
			totloop = 0;
			for (i=0; i < dm->numPolyData; i++, mp++) {
				ml = mloop + mp->loopstart;

				for (j=0; j < mp->totloop; j++, ml++, totloop++) {
					copy_v4_v4_char((char *)&wtcol_l[totloop],
					                (char *)&wtcol_v[4 * ml->v]);
				}
			}
		}
		MEM_freeN(wtcol_v);
	}
}


static void shapekey_layers_to_keyblocks(DerivedMesh *dm, Mesh *me, int actshape_uid)
{
	KeyBlock *kb;
	int i, j, tot;
	
	if (!me->key)
		return;	
	
	tot = CustomData_number_of_layers(&dm->vertData, CD_SHAPEKEY);
	for (i=0; i<tot; i++) {
		CustomDataLayer *layer = &dm->vertData.layers[CustomData_get_layer_index_n(&dm->vertData, CD_SHAPEKEY, i)];
		float (*cos)[3], (*kbcos)[3];
		
		for (kb=me->key->block.first; kb; kb=kb->next) {
			if (kb->uid == layer->uid)
				break;
		}
		
		if (!kb) {
			kb = add_keyblock(me->key, layer->name);
			kb->uid = layer->uid;
		}
		
		if (kb->data)
			MEM_freeN(kb->data);
		
		cos = CustomData_get_layer_n(&dm->vertData, CD_SHAPEKEY, i);
		kb->totelem = dm->numVertData;

		kb->data = kbcos = MEM_mallocN(sizeof(float)*3*kb->totelem, "kbcos DerivedMesh.c");
		if (kb->uid == actshape_uid) {
			MVert *mvert = dm->getVertArray(dm);
			
			for (j=0; j<dm->numVertData; j++, kbcos++, mvert++) {
				copy_v3_v3(*kbcos, mvert->co);
			}
		} else {
			for (j=0; j<kb->totelem; j++, cos++, kbcos++) {
				copy_v3_v3(*kbcos, *cos);
			}
		}
	}
	
	for (kb=me->key->block.first; kb; kb=kb->next) {
		if (kb->totelem != dm->numVertData) {
			if (kb->data)
				MEM_freeN(kb->data);
			
			kb->totelem = dm->numVertData;
			kb->data = MEM_callocN(sizeof(float)*3*kb->totelem, "kb->data derivedmesh.c");
			fprintf(stderr, "%s: lost a shapekey layer! (bmesh internal error)\n", __func__);
		}
	}
}

static void add_shapekey_layers(DerivedMesh *dm, Mesh *me, Object *UNUSED(ob))
{
	KeyBlock *kb;
	Key *key = me->key;
	int i;
	const size_t shape_alloc_len = sizeof(float) * 3 * me->totvert;

	if (!me->key)
		return;

	/* ensure we can use mesh vertex count for derived mesh custom data */
	if (me->totvert != dm->getNumVerts(dm)) {
		fprintf(stderr,
		        "%s: vertex size mismatch (mesh/dm) '%s' (%d != %d)\n",
		        __func__, me->id.name+2, me->totvert, dm->getNumVerts(dm));
		return;
	}

	for (i=0, kb=key->block.first; kb; kb=kb->next, i++) {
		int ci;
		float *array;

		if (me->totvert != kb->totelem) {
			fprintf(stderr,
			        "%s: vertex size mismatch (mesh/keyblock) '%s' (%d != %d)\n",
			        __func__, me->id.name+2, me->totvert, kb->totelem);
			array = MEM_callocN(shape_alloc_len, __func__);
		}
		else {
			array = MEM_mallocN(shape_alloc_len, __func__);
			memcpy(array, kb->data, shape_alloc_len);
		}

		CustomData_add_layer_named(&dm->vertData, CD_SHAPEKEY, CD_ASSIGN, array, dm->numVertData, kb->name);
		ci = CustomData_get_layer_index_n(&dm->vertData, CD_SHAPEKEY, i);

		dm->vertData.layers[ci].uid = kb->uid;
	}
}

/* new value for useDeform -1  (hack for the gameengine):
 * - apply only the modifier stack of the object, skipping the virtual modifiers,
 * - don't apply the key
 * - apply deform modifiers and input vertexco
 */
static void mesh_calc_modifiers(Scene *scene, Object *ob, float (*inputVertexCos)[3],
								DerivedMesh **deform_r, DerivedMesh **final_r,
								int useRenderParams, int useDeform,
								int needMapping, CustomDataMask dataMask, 
								int index, int useCache, int build_shapekey_layers)
{
	Mesh *me = ob->data;
	ModifierData *firstmd, *md, *previewmd = NULL;
	LinkNode *datamasks, *curr;
	CustomDataMask mask, nextmask, append_mask = 0;
	float (*deformedVerts)[3] = NULL;
	DerivedMesh *dm=NULL, *orcodm, *clothorcodm, *finaldm;
	int numVerts = me->totvert;
	int required_mode;
	int isPrevDeform= FALSE;
	int skipVirtualArmature = (useDeform < 0);
	MultiresModifierData *mmd= get_multires_modifier(scene, ob, 0);
	int has_multires = mmd != NULL, multires_applied = 0;
	int sculpt_mode = ob->mode & OB_MODE_SCULPT && ob->sculpt;

	const int draw_flag= ((scene->toolsettings->multipaint ? CALC_WP_MULTIPAINT : 0) |
	                      (scene->toolsettings->auto_normalize ? CALC_WP_AUTO_NORMALIZE : 0));
	/* Generic preview only in object mode! */
	const int do_mod_mcol = (ob->mode == OB_MODE_OBJECT);
#if 0 /* XXX Will re-enable this when we have global mod stack options. */
	const int do_final_wmcol = (scene->toolsettings->weights_preview == WP_WPREVIEW_FINAL) && do_wmcol;
#endif
	const int do_final_wmcol = FALSE;
	int do_init_wmcol = ((dataMask & CD_MASK_WEIGHT_MCOL) && (ob->mode & OB_MODE_WEIGHT_PAINT) && !do_final_wmcol);
	/* XXX Same as above... For now, only weights preview in WPaint mode. */
	const int do_mod_wmcol = do_init_wmcol;

	if(mmd && !mmd->sculptlvl)
		has_multires = 0;

	if(!skipVirtualArmature) {
		firstmd = modifiers_getVirtualModifierList(ob);
	}
	else {
		/* game engine exception */
		firstmd = ob->modifiers.first;
		if(firstmd && firstmd->type == eModifierType_Armature)
			firstmd = firstmd->next;
	}

	md = firstmd;

	modifiers_clearErrors(ob);

	if(useRenderParams) required_mode = eModifierMode_Render;
	else required_mode = eModifierMode_Realtime;

	datamasks = modifiers_calcDataMasks(scene, ob, md, dataMask, required_mode);
	curr = datamasks;

	if(do_mod_wmcol || do_mod_mcol) {
		/* Find the last active modifier generating a preview, or NULL if none. */
		/* XXX Currently, DPaint modifier just ignores this.
		 *     Needs a stupid hack...
		 *     The whole "modifier preview" thing has to be (re?)designed, anyway! */
		previewmd = modifiers_getLastPreview(scene, md, required_mode);
	}

	if(deform_r) *deform_r = NULL;
	*final_r = NULL;

	if(useDeform) {
		if(inputVertexCos)
			deformedVerts = inputVertexCos;
		
		/* Apply all leading deforming modifiers */
		for(;md; md = md->next, curr = curr->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);

			md->scene= scene;
			
			if(!modifier_isEnabled(scene, md, required_mode)) continue;
			if(useDeform < 0 && mti->dependsOnTime && mti->dependsOnTime(md)) continue;

			if(mti->type == eModifierTypeType_OnlyDeform) {
				if(!deformedVerts)
					deformedVerts = mesh_getVertexCos(me, &numVerts);

				mti->deformVerts(md, ob, NULL, deformedVerts, numVerts, useRenderParams, useDeform);
			} else {
				break;
			}
			
			/* grab modifiers until index i */
			if((index >= 0) && (modifiers_indexInObject(ob, md) >= index))
				break;
		}

		/* Result of all leading deforming modifiers is cached for
		 * places that wish to use the original mesh but with deformed
		 * coordinates (vpaint, etc.)
		 */
		if (deform_r) {
			*deform_r = CDDM_from_mesh(me, ob);
			 
			if (build_shapekey_layers)
				add_shapekey_layers(dm, me, ob);
			
			if(deformedVerts) {
				CDDM_apply_vert_coords(*deform_r, deformedVerts);
				CDDM_calc_normals(*deform_r);
			}
		}
	} else {
		/* default behaviour for meshes */
		if(inputVertexCos)
			deformedVerts = inputVertexCos;
		else
			deformedVerts = mesh_getVertexCos(me, &numVerts);
	}


	/* Now apply all remaining modifiers. If useDeform is off then skip
	 * OnlyDeform ones. 
	 */
	dm = NULL;
	orcodm = NULL;
	clothorcodm = NULL;

	for(;md; md = md->next, curr = curr->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		md->scene= scene;

		if(!modifier_isEnabled(scene, md, required_mode)) continue;
		if(mti->type == eModifierTypeType_OnlyDeform && !useDeform) continue;
		if((mti->flags & eModifierTypeFlag_RequiresOriginalData) && dm) {
			modifier_setError(md, "Modifier requires original data, bad stack position.");
			continue;
		}
		if(sculpt_mode && (!has_multires || multires_applied)) {
			int unsupported= 0;

			if(scene->toolsettings->sculpt->flags & SCULPT_ONLY_DEFORM)
				unsupported|= mti->type != eModifierTypeType_OnlyDeform;

			unsupported|= md->type == eModifierType_Multires && ((MultiresModifierData*)md)->sculptlvl==0;
			unsupported|= multires_applied;

			if(unsupported) {
				modifier_setError(md, "Not supported in sculpt mode.");
				continue;
			}
		}
		if(needMapping && !modifier_supportsMapping(md)) continue;
		if(useDeform < 0 && mti->dependsOnTime && mti->dependsOnTime(md)) continue;

		/* add an orco layer if needed by this modifier */
		if(mti->requiredDataMask)
			mask = mti->requiredDataMask(ob, md);
		else
			mask = 0;

		if(dm && (mask & CD_MASK_ORCO))
			add_orco_dm(ob, NULL, dm, orcodm, CD_ORCO);

		/* How to apply modifier depends on (a) what we already have as
		 * a result of previous modifiers (could be a DerivedMesh or just
		 * deformed vertices) and (b) what type the modifier is.
		 */

		if(mti->type == eModifierTypeType_OnlyDeform) {
			/* No existing verts to deform, need to build them. */
			if(!deformedVerts) {
				if(dm) {
					/* Deforming a derived mesh, read the vertex locations
					 * out of the mesh and deform them. Once done with this
					 * run of deformers verts will be written back.
					 */
					numVerts = dm->getNumVerts(dm);
					deformedVerts =
						MEM_mallocN(sizeof(*deformedVerts) * numVerts, "dfmv");
					dm->getVertCos(dm, deformedVerts);
				} else {
					deformedVerts = mesh_getVertexCos(me, &numVerts);
				}
			}

			/* if this is not the last modifier in the stack then recalculate the normals
			 * to avoid giving bogus normals to the next modifier see: [#23673] */
			if(isPrevDeform &&  mti->dependsOnNormals && mti->dependsOnNormals(md)) {
				/* XXX, this covers bug #23673, but we may need normal calc for other types */
				if(dm && dm->type == DM_TYPE_CDDM) {
					CDDM_apply_vert_coords(dm, deformedVerts);
					CDDM_calc_normals(dm);
				}
			}

			mti->deformVerts(md, ob, dm, deformedVerts, numVerts, useRenderParams, useDeform);
		} else {
			DerivedMesh *ndm;

			/* determine which data layers are needed by following modifiers */
			if(curr->next)
				nextmask= (CustomDataMask)GET_INT_FROM_POINTER(curr->next->link);
			else
				nextmask= dataMask;

			/* apply vertex coordinates or build a DerivedMesh as necessary */
			if(dm) {
				if(deformedVerts) {
					DerivedMesh *tdm = CDDM_copy(dm);
					dm->release(dm);
					dm = tdm;

					CDDM_apply_vert_coords(dm, deformedVerts);
					CDDM_calc_normals(dm);
				}
			} else {
				dm = CDDM_from_mesh(me, ob);

				if (build_shapekey_layers)
					add_shapekey_layers(dm, me, ob);

				if(deformedVerts) {
					CDDM_apply_vert_coords(dm, deformedVerts);
					CDDM_calc_normals(dm);
				}

				if(do_init_wmcol)
					DM_update_weight_mcol(ob, dm, draw_flag, NULL, 0, NULL);

				/* Constructive modifiers need to have an origindex
				 * otherwise they wont have anywhere to copy the data from.
				 *
				 * Also create ORIGINDEX data if any of the following modifiers
				 * requests it, this way Mirror, Solidify etc will keep ORIGINDEX
				 * data by using generic DM_copy_vert_data() functions.
				 */
				if(needMapping || (nextmask & CD_MASK_ORIGINDEX)) {
					/* calc */
					DM_add_vert_layer(dm, CD_ORIGINDEX, CD_CALLOC, NULL);
					DM_add_edge_layer(dm, CD_ORIGINDEX, CD_CALLOC, NULL);
					DM_add_poly_layer(dm, CD_ORIGINDEX, CD_CALLOC, NULL);

					range_vn_i(DM_get_vert_data_layer(dm, CD_ORIGINDEX), dm->numVertData, 0);
					range_vn_i(DM_get_edge_data_layer(dm, CD_ORIGINDEX), dm->numEdgeData, 0);
					range_vn_i(DM_get_poly_data_layer(dm, CD_ORIGINDEX), dm->numPolyData, 0);
				}
			}

			
			/* set the DerivedMesh to only copy needed data */
			mask= (CustomDataMask)GET_INT_FROM_POINTER(curr->link);
			/* needMapping check here fixes bug [#28112], otherwise its
			 * possible that it wont be copied */
			mask |= append_mask;
			DM_set_only_copy(dm, mask | (needMapping ? CD_MASK_ORIGINDEX : 0));
			
			/* add cloth rest shape key if need */
			if(mask & CD_MASK_CLOTH_ORCO)
				add_orco_dm(ob, NULL, dm, clothorcodm, CD_CLOTH_ORCO);

			/* add an origspace layer if needed */
			if(((CustomDataMask)GET_INT_FROM_POINTER(curr->link)) & CD_MASK_ORIGSPACE_MLOOP) {
				if(!CustomData_has_layer(&dm->loopData, CD_ORIGSPACE_MLOOP)) {
					DM_add_loop_layer(dm, CD_ORIGSPACE_MLOOP, CD_CALLOC, NULL);
					DM_init_origspace(dm);
				}
			}

			ndm = mti->applyModifier(md, ob, dm, useRenderParams, useCache);

			if(ndm) {
				/* if the modifier returned a new dm, release the old one */
				if(dm && dm != ndm) dm->release(dm);

				dm = ndm;

				if(deformedVerts) {
					if(deformedVerts != inputVertexCos)
						MEM_freeN(deformedVerts);

					deformedVerts = NULL;
				}
			} 

			/* create an orco derivedmesh in parallel */
			if(nextmask & CD_MASK_ORCO) {
				if(!orcodm)
					orcodm= create_orco_dm(ob, me, NULL, CD_ORCO);

				nextmask &= ~CD_MASK_ORCO;
				DM_set_only_copy(orcodm, nextmask | CD_MASK_ORIGINDEX);
				ndm = mti->applyModifier(md, ob, orcodm, useRenderParams, 0);

				if(ndm) {
					/* if the modifier returned a new dm, release the old one */
					if(orcodm && orcodm != ndm) orcodm->release(orcodm);
					orcodm = ndm;
				}
			}

			/* create cloth orco derivedmesh in parallel */
			if(nextmask & CD_MASK_CLOTH_ORCO) {
				if(!clothorcodm)
					clothorcodm= create_orco_dm(ob, me, NULL, CD_CLOTH_ORCO);

				nextmask &= ~CD_MASK_CLOTH_ORCO;
				DM_set_only_copy(clothorcodm, nextmask | CD_MASK_ORIGINDEX);
				ndm = mti->applyModifier(md, ob, clothorcodm, useRenderParams, 0);

				if(ndm) {
					/* if the modifier returned a new dm, release the old one */
					if(clothorcodm && clothorcodm != ndm) clothorcodm->release(clothorcodm);
					clothorcodm = ndm;
				}
			}

			/* in case of dynamic paint, make sure preview mask remains for following modifiers */
			/* XXX Temp and hackish solution! */
			if (md->type == eModifierType_DynamicPaint)
				append_mask |= CD_MASK_WEIGHT_MCOL;
			/* In case of active preview modifier, make sure preview mask remains for following modifiers. */
			else if ((md == previewmd) && (do_mod_wmcol)) {
				DM_update_weight_mcol(ob, dm, draw_flag, NULL, 0, NULL);
				append_mask |= CD_MASK_WEIGHT_MCOL;
			}
		}

		isPrevDeform= (mti->type == eModifierTypeType_OnlyDeform);

		/* grab modifiers until index i */
		if((index >= 0) && (modifiers_indexInObject(ob, md) >= index))
			break;

		if(sculpt_mode && md->type == eModifierType_Multires)
			multires_applied = 1;
	}

	for(md=firstmd; md; md=md->next)
		modifier_freeTemporaryData(md);

	/* Yay, we are done. If we have a DerivedMesh and deformed vertices
	 * need to apply these back onto the DerivedMesh. If we have no
	 * DerivedMesh then we need to build one.
	 */
	if(dm && deformedVerts) {
		finaldm = CDDM_copy(dm);

		dm->release(dm);

		CDDM_apply_vert_coords(finaldm, deformedVerts);
		CDDM_calc_normals(finaldm);

#if 0 /* For later nice mod preview! */
		/* In case we need modified weights in CD_WEIGHT_MCOL, we have to re-compute it. */
		if(do_final_wmcol)
			DM_update_weight_mcol(ob, finaldm, draw_flag, NULL, 0, NULL);
#endif
	} else if(dm) {
		finaldm = dm;

#if 0 /* For later nice mod preview! */
		/* In case we need modified weights in CD_WEIGHT_MCOL, we have to re-compute it. */
		if(do_final_wmcol)
			DM_update_weight_mcol(ob, finaldm, draw_flag, NULL, 0, NULL);
#endif
	} else {
		int recalc_normals= 0;

		finaldm = CDDM_from_mesh(me, ob);
		
		if(build_shapekey_layers) {
			add_shapekey_layers(finaldm, me, ob);
			recalc_normals= 1;
		}
		
		if(deformedVerts) {
			CDDM_apply_vert_coords(finaldm, deformedVerts);
			recalc_normals= 1;
		}

		if(recalc_normals) {
			CDDM_calc_normals(finaldm);
		}

		/* In this case, we should never have weight-modifying modifiers in stack... */
		if(do_init_wmcol)
			DM_update_weight_mcol(ob, finaldm, draw_flag, NULL, 0, NULL);
	}

	/* add an orco layer if needed */
	if(dataMask & CD_MASK_ORCO) {
		add_orco_dm(ob, NULL, finaldm, orcodm, CD_ORCO);

		if(deform_r && *deform_r)
			add_orco_dm(ob, NULL, *deform_r, NULL, CD_ORCO);
	}

#ifdef WITH_GAMEENGINE
	/* NavMesh - this is a hack but saves having a NavMesh modifier */
	if ((ob->gameflag & OB_NAVMESH) && (finaldm->type == DM_TYPE_CDDM)) {
		DerivedMesh *tdm;
		tdm= navmesh_dm_createNavMeshForVisualization(finaldm);
		if (finaldm != tdm) {
			finaldm->release(finaldm);
			finaldm= tdm;
		}
	}
#endif /* WITH_GAMEENGINE */

	{
		/* calculating normals can re-calculate tessfaces in some cases */
		int num_tessface = finaldm->getNumTessFaces(finaldm);
		/* --------------------------------------------------------------------- */
		/* First calculate the polygon and vertex normals, re-tesselation
		 * copies these into the tessface's normal layer */


		/* comment because this causes a bug when deform is applied after a
		 * bug when applied after a subsurf modifier (SubSurf -> Cast) for eg,
		 * it also looks like this isn't even needed since code above recalc's
		 * normals - campbell */
#if 0
		finaldm->calcNormals(finaldm);
#endif

		/* Re-tesselation is necessary to push render data (uvs, textures, colors)
		 * from loops and polys onto the tessfaces. This may be currently be
		 * redundantin cases where the render mode doesn't use these inputs, but
		 * ideally eventually tesselation would happen on-demand, and this is one
		 * of the primary places it would be needed. */
		if (num_tessface == 0 && finaldm->getNumTessFaces(finaldm) == 0) {
			finaldm->recalcTesselation(finaldm);
		}
		/* Need to watch this, it can cause issues, see bug [#29338]             */
		/* take care with this block, we really need testing frameworks          */
		/* --------------------------------------------------------------------- */
	}


	*final_r = finaldm;

	if(orcodm)
		orcodm->release(orcodm);
	if(clothorcodm)
		clothorcodm->release(clothorcodm);

	if(deformedVerts && deformedVerts != inputVertexCos)
		MEM_freeN(deformedVerts);

	BLI_linklist_free(datamasks, NULL);
}

float (*editbmesh_get_vertex_cos(BMEditMesh *em, int *numVerts_r))[3]
{
	int i, numVerts = *numVerts_r = em->bm->totvert;
	float (*cos)[3];
	BMIter iter;
	BMVert *eve;

	cos = MEM_mallocN(sizeof(float)*3*numVerts, "vertexcos");

	eve = BM_iter_new(&iter, em->bm, BM_VERTS_OF_MESH, NULL);
	for (i=0; eve; eve=BM_iter_step(&iter), i++) {
		copy_v3_v3(cos[i], eve->co);
	}

	return cos;
}

int editbmesh_modifier_is_enabled(Scene *scene, ModifierData *md, DerivedMesh *dm)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;

	if(!modifier_isEnabled(scene, md, required_mode)) return 0;
	if((mti->flags & eModifierTypeFlag_RequiresOriginalData) && dm) {
		modifier_setError(md, "Modifier requires original data, bad stack position.");
		return 0;
	}
	
	return 1;
}

static void editbmesh_calc_modifiers(Scene *scene, Object *ob, BMEditMesh *em, DerivedMesh **cage_r,
									DerivedMesh **final_r,
									CustomDataMask dataMask)
{
	ModifierData *md;
	float (*deformedVerts)[3] = NULL;
	CustomDataMask mask;
	DerivedMesh *dm, *orcodm = NULL;
	int i, numVerts = 0, cageIndex = modifiers_getCageIndex(scene, ob, NULL, 1);
	LinkNode *datamasks, *curr;
	int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;

	modifiers_clearErrors(ob);

	if(cage_r && cageIndex == -1) {
		*cage_r = getEditDerivedBMesh(em, ob, NULL);
	}

	dm = NULL;
	md = modifiers_getVirtualModifierList(ob);

	datamasks = modifiers_calcDataMasks(scene, ob, md, dataMask, required_mode);

	curr = datamasks;
	for(i = 0; md; i++, md = md->next, curr = curr->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		md->scene= scene;
		
		if(!editbmesh_modifier_is_enabled(scene, md, dm))
			continue;

		/* add an orco layer if needed by this modifier */
		if(dm && mti->requiredDataMask) {
			mask = mti->requiredDataMask(ob, md);
			if(mask & CD_MASK_ORCO)
				add_orco_dm(ob, em, dm, orcodm, CD_ORCO);
		}

		/* How to apply modifier depends on (a) what we already have as
		 * a result of previous modifiers (could be a DerivedMesh or just
		 * deformed vertices) and (b) what type the modifier is.
		 */

		if(mti->type == eModifierTypeType_OnlyDeform) {
			/* No existing verts to deform, need to build them. */
			if(!deformedVerts) {
				if(dm) {
					/* Deforming a derived mesh, read the vertex locations
					 * out of the mesh and deform them. Once done with this
					 * run of deformers verts will be written back.
					 */
					numVerts = dm->getNumVerts(dm);
					deformedVerts =
						MEM_mallocN(sizeof(*deformedVerts) * numVerts, "dfmv");
					dm->getVertCos(dm, deformedVerts);
				} else {
					deformedVerts = editbmesh_get_vertex_cos(em, &numVerts);
				}
			}

			if (mti->deformVertsEM)
				mti->deformVertsEM(md, ob, em, dm, deformedVerts, numVerts);
			else mti->deformVerts(md, ob, dm, deformedVerts, numVerts, 0, 0);
		} else {
			DerivedMesh *ndm;

			/* apply vertex coordinates or build a DerivedMesh as necessary */
			if(dm) {
				if(deformedVerts) {
					DerivedMesh *tdm = CDDM_copy(dm);
					if(!(cage_r && dm == *cage_r)) dm->release(dm);
					dm = tdm;

					CDDM_apply_vert_coords(dm, deformedVerts);
					CDDM_calc_normals(dm);
				} else if(cage_r && dm == *cage_r) {
					/* dm may be changed by this modifier, so we need to copy it
					 */
					dm = CDDM_copy(dm);
				}

			} else {
				dm = CDDM_from_BMEditMesh(em, ob->data, FALSE, FALSE);

				if(deformedVerts) {
					CDDM_apply_vert_coords(dm, deformedVerts);
					CDDM_calc_normals(dm);
				}
			}

			/* create an orco derivedmesh in parallel */
			mask= (CustomDataMask)GET_INT_FROM_POINTER(curr->link);
			if(mask & CD_MASK_ORCO) {
				if(!orcodm)
					orcodm= create_orco_dm(ob, ob->data, em, CD_ORCO);

				mask &= ~CD_MASK_ORCO;
				DM_set_only_copy(orcodm, mask | CD_MASK_ORIGINDEX);

				if (mti->applyModifierEM)
					ndm = mti->applyModifierEM(md, ob, em, orcodm);
				else
					ndm = mti->applyModifier(md, ob, orcodm, 0, 0);

				if(ndm) {
					/* if the modifier returned a new dm, release the old one */
					if(orcodm && orcodm != ndm) orcodm->release(orcodm);
					orcodm = ndm;
				}
			}

			/* set the DerivedMesh to only copy needed data */
			mask= (CustomDataMask)GET_INT_FROM_POINTER(curr->link); /* CD_MASK_ORCO may have been cleared above */

			DM_set_only_copy(dm, mask | CD_MASK_ORIGINDEX);

			if(mask & CD_MASK_ORIGSPACE_MLOOP) {
				if(!CustomData_has_layer(&dm->loopData, CD_ORIGSPACE_MLOOP)) {
					DM_add_loop_layer(dm, CD_ORIGSPACE_MLOOP, CD_CALLOC, NULL);
					DM_init_origspace(dm);
				}
			}
			
			if (mti->applyModifierEM)
				ndm = mti->applyModifierEM(md, ob, em, dm);
			else
				ndm = mti->applyModifier(md, ob, dm, 0, 0);

			if (ndm) {
				if(dm && dm != ndm)
					dm->release(dm);

				dm = ndm;

				if (deformedVerts) {
					MEM_freeN(deformedVerts);
					deformedVerts = NULL;
				}
			}
		}

		if(cage_r && i == cageIndex) {
			if(dm && deformedVerts) {
				*cage_r = CDDM_copy(dm);
				CDDM_apply_vert_coords(*cage_r, deformedVerts);
			} else if(dm) {
				*cage_r = dm;
			} else {
				*cage_r =
					getEditDerivedBMesh(em, ob,
						deformedVerts ? MEM_dupallocN(deformedVerts) : NULL);
			}
		}
	}

	BLI_linklist_free(datamasks, NULL);

	/* Yay, we are done. If we have a DerivedMesh and deformed vertices need
	 * to apply these back onto the DerivedMesh. If we have no DerivedMesh
	 * then we need to build one.
	 */
	if(dm && deformedVerts) {
		*final_r = CDDM_copy(dm);

		if(!(cage_r && dm == *cage_r)) dm->release(dm);

		CDDM_apply_vert_coords(*final_r, deformedVerts);
		CDDM_calc_normals(*final_r); /* was CDDM_calc_normals_mapping - campbell */
	} else if (dm) {
		*final_r = dm;
		(*final_r)->calcNormals(*final_r); /* BMESH_ONLY - BMESH_TODO. check if this is needed */
	} else if (!deformedVerts && cage_r && *cage_r) {
		*final_r = *cage_r;
		(*final_r)->calcNormals(*final_r); /* BMESH_ONLY - BMESH_TODO. check if this is needed */
	} else {
		*final_r = getEditDerivedBMesh(em, ob, deformedVerts);
		deformedVerts = NULL;
		(*final_r)->calcNormals(*final_r); /* BMESH_ONLY - BMESH_TODO. check if this is needed */
	}

	/* --- */
	/* BMESH_ONLY, ensure tessface's used for drawing,
	 * but dont recalculate if the last modifier in the stack gives us tessfaces  */
	DM_ensure_tessface(*final_r);
	if (cage_r && (*cage_r != *final_r)) DM_ensure_tessface(*cage_r);
	/* --- */

	/* add an orco layer if needed */
	if(dataMask & CD_MASK_ORCO)
		add_orco_dm(ob, em, *final_r, orcodm, CD_ORCO);

	if(orcodm)
		orcodm->release(orcodm);

	if(deformedVerts)
		MEM_freeN(deformedVerts);
}

static void clear_mesh_caches(Object *ob)
{
	Mesh *me= ob->data;

		/* also serves as signal to remake texspace */
	if (ob->bb) {
		MEM_freeN(ob->bb);
		ob->bb = NULL;
	}
	if (me->bb) {
		MEM_freeN(me->bb);
		me->bb = NULL;
	}

	freedisplist(&ob->disp);

	if (ob->derivedFinal) {
		ob->derivedFinal->needsFree = 1;
		ob->derivedFinal->release(ob->derivedFinal);
		ob->derivedFinal= NULL;
	}
	if (ob->derivedDeform) {
		ob->derivedDeform->needsFree = 1;
		ob->derivedDeform->release(ob->derivedDeform);
		ob->derivedDeform= NULL;
	}

	if(ob->sculpt) {
		object_sculpt_modifiers_changed(ob);
	}
}

static void mesh_build_data(Scene *scene, Object *ob, CustomDataMask dataMask,
                            int build_shapekey_layers)
{
	Object *obact = scene->basact?scene->basact->object:NULL;
	int editing = paint_facesel_test(ob);
	/* weight paint and face select need original indices because of selection buffer drawing */
	int needMapping = (ob==obact) && (editing || (ob->mode & (OB_MODE_WEIGHT_PAINT|OB_MODE_VERTEX_PAINT|OB_MODE_TEXTURE_PAINT)));

	clear_mesh_caches(ob);

	mesh_calc_modifiers(scene, ob, NULL, &ob->derivedDeform,
						&ob->derivedFinal, 0, 1,
						needMapping, dataMask, -1, 1, build_shapekey_layers);

	DM_set_object_boundbox (ob, ob->derivedFinal);

	ob->derivedFinal->needsFree = 0;
	ob->derivedDeform->needsFree = 0;
	ob->lastDataMask = dataMask;
}

static void editbmesh_build_data(Scene *scene, Object *obedit, BMEditMesh *em, CustomDataMask dataMask)
{
	clear_mesh_caches(obedit);

	if (em->derivedFinal) {
		if (em->derivedFinal!=em->derivedCage) {
			em->derivedFinal->needsFree = 1;
			em->derivedFinal->release(em->derivedFinal);
		}
		em->derivedFinal = NULL;
	}
	if (em->derivedCage) {
		em->derivedCage->needsFree = 1;
		em->derivedCage->release(em->derivedCage);
		em->derivedCage = NULL;
	}

	editbmesh_calc_modifiers(scene, obedit, em, &em->derivedCage, &em->derivedFinal, dataMask);
	DM_set_object_boundbox (obedit, em->derivedFinal);

	em->lastDataMask = dataMask;
	em->derivedFinal->needsFree = 0;
	em->derivedCage->needsFree = 0;
}

void makeDerivedMesh(Scene *scene, Object *ob, BMEditMesh *em,
                     CustomDataMask dataMask, int build_shapekey_layers)
{
	if (em) {
		editbmesh_build_data(scene, ob, em, dataMask);
	} else {
		mesh_build_data(scene, ob, dataMask, build_shapekey_layers);
	}
}

/***/

DerivedMesh *mesh_get_derived_final(Scene *scene, Object *ob, CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	if(!ob->derivedFinal || (dataMask & ob->lastDataMask) != dataMask)
		mesh_build_data(scene, ob, dataMask, 0);

	return ob->derivedFinal;
}

DerivedMesh *mesh_get_derived_deform(Scene *scene, Object *ob, CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	if(!ob->derivedDeform || (dataMask & ob->lastDataMask) != dataMask)
		mesh_build_data(scene, ob, dataMask, 0);

	return ob->derivedDeform;
}

DerivedMesh *mesh_create_derived_render(Scene *scene, Object *ob, CustomDataMask dataMask)
{
	DerivedMesh *final;
	
	mesh_calc_modifiers(scene, ob, NULL, NULL, &final, 1, 1, 0, dataMask, -1, 0, 0);

	return final;
}

DerivedMesh *mesh_create_derived_index_render(Scene *scene, Object *ob, CustomDataMask dataMask, int index)
{
	DerivedMesh *final;
	
	mesh_calc_modifiers(scene, ob, NULL, NULL, &final, 1, 1, 0, dataMask, index, 0, 0);

	return final;
}

DerivedMesh *mesh_create_derived_view(Scene *scene, Object *ob, CustomDataMask dataMask)
{
	DerivedMesh *final;

	mesh_calc_modifiers(scene, ob, NULL, NULL, &final, 0, 1, 0, dataMask, -1, 0, 0);

	return final;
}

DerivedMesh *mesh_create_derived_no_deform(Scene *scene, Object *ob, float (*vertCos)[3],
										   CustomDataMask dataMask)
{
	DerivedMesh *final;
	
	mesh_calc_modifiers(scene, ob, vertCos, NULL, &final, 0, 0, 0, dataMask, -1, 0, 0);

	return final;
}

DerivedMesh *mesh_create_derived_no_virtual(Scene *scene, Object *ob, float (*vertCos)[3],
											CustomDataMask dataMask)
{
	DerivedMesh *final;
	
	mesh_calc_modifiers(scene, ob, vertCos, NULL, &final, 0, -1, 0, dataMask, -1, 0, 0);

	return final;
}

DerivedMesh *mesh_create_derived_physics(Scene *scene, Object *ob, float (*vertCos)[3],
											CustomDataMask dataMask)
{
	DerivedMesh *final;
	
	mesh_calc_modifiers(scene, ob, vertCos, NULL, &final, 0, -1, 1, dataMask, -1, 0, 0);

	return final;
}

DerivedMesh *mesh_create_derived_no_deform_render(Scene *scene, Object *ob,
												  float (*vertCos)[3],
												  CustomDataMask dataMask)
{
	DerivedMesh *final;

	mesh_calc_modifiers(scene, ob, vertCos, NULL, &final, 1, 0, 0, dataMask, -1, 0, 0);

	return final;
}

/***/

DerivedMesh *editbmesh_get_derived_cage_and_final(Scene *scene, Object *obedit, BMEditMesh *em, DerivedMesh **final_r,
												 CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	if(!em->derivedCage ||
	   (em->lastDataMask & dataMask) != dataMask)
		editbmesh_build_data(scene, obedit, em, dataMask);

	*final_r = em->derivedFinal;
	return em->derivedCage;
}

DerivedMesh *editbmesh_get_derived_cage(Scene *scene, Object *obedit, BMEditMesh *em, CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	if(!em->derivedCage ||
	   (em->lastDataMask & dataMask) != dataMask)
		editbmesh_build_data(scene, obedit, em, dataMask);

	return em->derivedCage;
}

DerivedMesh *editbmesh_get_derived_base(Object *obedit, BMEditMesh *em)
{
	return getEditDerivedBMesh(em, obedit, NULL);
}


/* ********* For those who don't grasp derived stuff! (ton) :) *************** */

static void make_vertexcosnos__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	float *vec = userData;
	
	vec+= 6*index;

	/* check if we've been here before (normal should not be 0) */
	if(vec[3] || vec[4] || vec[5]) return;

	copy_v3_v3(vec, co);
	vec+= 3;
	if(no_f) {
		copy_v3_v3(vec, no_f);
	}
	else {
		normal_short_to_float_v3(vec, no_s);
	}
}

/* always returns original amount me->totvert of vertices and normals, but fully deformed and subsurfered */
/* this is needed for all code using vertexgroups (no subsurf support) */
/* it stores the normals as floats, but they can still be scaled as shorts (32767 = unit) */
/* in use now by vertex/weight paint and particle generating */

float *mesh_get_mapped_verts_nors(Scene *scene, Object *ob)
{
	Mesh *me= ob->data;
	DerivedMesh *dm;
	float *vertexcosnos;
	
	/* lets prevent crashing... */
	if(ob->type!=OB_MESH || me->totvert==0)
		return NULL;
	
	dm= mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH|CD_MASK_ORIGINDEX);
	vertexcosnos= MEM_callocN(6*sizeof(float)*me->totvert, "vertexcosnos map");
	
	if(dm->foreachMappedVert) {
		dm->foreachMappedVert(dm, make_vertexcosnos__mapFunc, vertexcosnos);
	}
	else {
		float *fp= vertexcosnos;
		int a;
		
		for(a=0; a< me->totvert; a++, fp+=6) {
			dm->getVertCo(dm, a, fp);
			dm->getVertNo(dm, a, fp+3);
		}
	}
	
	dm->release(dm);
	return vertexcosnos;
}

/* ******************* GLSL ******************** */

typedef struct
{
	float * precomputedFaceNormals;
	MTFace * mtface;	// texture coordinates
	MFace * mface;		// indices
	MVert * mvert;		// vertices & normals
	float (*orco)[3];
	float (*tangent)[4];	// destination
	int numTessFaces;

} SGLSLMeshToTangent;

// interface
#include "mikktspace.h"

static int GetNumFaces(const SMikkTSpaceContext * pContext)
{
	SGLSLMeshToTangent * pMesh = (SGLSLMeshToTangent *) pContext->m_pUserData;
	return pMesh->numTessFaces;
}

static int GetNumVertsOfFace(const SMikkTSpaceContext * pContext, const int face_num)
{
	SGLSLMeshToTangent * pMesh = (SGLSLMeshToTangent *) pContext->m_pUserData;
	return pMesh->mface[face_num].v4!=0 ? 4 : 3;
}

static void GetPosition(const SMikkTSpaceContext * pContext, float fPos[], const int face_num, const int vert_index)
{
	//assert(vert_index>=0 && vert_index<4);
	SGLSLMeshToTangent * pMesh = (SGLSLMeshToTangent *) pContext->m_pUserData;
	const float *co= pMesh->mvert[(&pMesh->mface[face_num].v1)[vert_index]].co;
	copy_v3_v3(fPos, co);
}

static void GetTextureCoordinate(const SMikkTSpaceContext * pContext, float fUV[], const int face_num, const int vert_index)
{
	//assert(vert_index>=0 && vert_index<4);
	SGLSLMeshToTangent * pMesh = (SGLSLMeshToTangent *) pContext->m_pUserData;

	if(pMesh->mtface!=NULL) {
		float * uv = pMesh->mtface[face_num].uv[vert_index];
		fUV[0]=uv[0]; fUV[1]=uv[1];
	}
	else {
		const float *orco= pMesh->orco[(&pMesh->mface[face_num].v1)[vert_index]];
		map_to_sphere( &fUV[0], &fUV[1], orco[0], orco[1], orco[2]);
	}
}

static void GetNormal(const SMikkTSpaceContext * pContext, float fNorm[], const int face_num, const int vert_index)
{
	//assert(vert_index>=0 && vert_index<4);
	SGLSLMeshToTangent * pMesh = (SGLSLMeshToTangent *) pContext->m_pUserData;

	const int smoothnormal = (pMesh->mface[face_num].flag & ME_SMOOTH);
	if(!smoothnormal) {	// flat
		if(pMesh->precomputedFaceNormals) {
			copy_v3_v3(fNorm, &pMesh->precomputedFaceNormals[3*face_num]);
		}
		else {
			MFace *mf= &pMesh->mface[face_num];
			float *p0= pMesh->mvert[mf->v1].co;
			float *p1= pMesh->mvert[mf->v2].co;
			float *p2= pMesh->mvert[mf->v3].co;

			if(mf->v4) {
				float *p3 = pMesh->mvert[mf->v4].co;
				normal_quad_v3(fNorm, p0, p1, p2, p3);
			}
			else {
				normal_tri_v3(fNorm, p0, p1, p2);
			}
		}
	}
	else {
		const short *no= pMesh->mvert[(&pMesh->mface[face_num].v1)[vert_index]].no;
		normal_short_to_float_v3(fNorm, no);
	}
}
static void SetTSpace(const SMikkTSpaceContext * pContext, const float fvTangent[], const float fSign, const int face_num, const int iVert)
{
	//assert(vert_index>=0 && vert_index<4);
	SGLSLMeshToTangent * pMesh = (SGLSLMeshToTangent *) pContext->m_pUserData;
	float * pRes = pMesh->tangent[4*face_num+iVert];
	copy_v3_v3(pRes, fvTangent);
	pRes[3]=fSign;
}


void DM_add_tangent_layer(DerivedMesh *dm)
{
	/* mesh vars */
	MTFace *mtface, *tf;
	MFace *mface, *mf;
	MVert *mvert, *v1, *v2, *v3, *v4;
	MemArena *arena= NULL;
	VertexTangent **vtangents= NULL;
	float (*orco)[3]= NULL, (*tangent)[4];
	float *uv1, *uv2, *uv3, *uv4, *vtang;
	float fno[3], tang[3], uv[4][2];
	int i, j, len, mf_vi[4], totvert, totface, iCalcNewMethod;
	float *nors;

	if(CustomData_get_layer_index(&dm->faceData, CD_TANGENT) != -1)
		return;

	nors = dm->getTessFaceDataArray(dm, CD_NORMAL);

	/* check we have all the needed layers */
	totvert= dm->getNumVerts(dm);
	totface= dm->getNumTessFaces(dm);

	mvert= dm->getVertArray(dm);
	mface= dm->getTessFaceArray(dm);
	mtface= dm->getTessFaceDataArray(dm, CD_MTFACE);

	if(!mtface) {
		orco= dm->getVertDataArray(dm, CD_ORCO);
		if(!orco)
			return;
	}
	
	/* create tangent layer */
	DM_add_tessface_layer(dm, CD_TANGENT, CD_CALLOC, NULL);
	tangent= DM_get_tessface_data_layer(dm, CD_TANGENT);
	
	/* allocate some space */
	arena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "tangent layer arena");
	BLI_memarena_use_calloc(arena);
	vtangents= MEM_callocN(sizeof(VertexTangent*)*totvert, "VertexTangent");

	// new computation method
	iCalcNewMethod = 1;
	if(iCalcNewMethod != 0) {
		SGLSLMeshToTangent mesh2tangent= {0};
		SMikkTSpaceContext sContext= {0};
		SMikkTSpaceInterface sInterface= {0};

		mesh2tangent.precomputedFaceNormals = nors;
		mesh2tangent.mtface = mtface;
		mesh2tangent.mface = mface;
		mesh2tangent.mvert = mvert;
		mesh2tangent.orco = orco;
		mesh2tangent.tangent = tangent;
		mesh2tangent.numTessFaces = totface;

		sContext.m_pUserData = &mesh2tangent;
		sContext.m_pInterface = &sInterface;
		sInterface.m_getNumFaces = GetNumFaces;
		sInterface.m_getNumVerticesOfFace = GetNumVertsOfFace;
		sInterface.m_getPosition = GetPosition;
		sInterface.m_getTexCoord = GetTextureCoordinate;
		sInterface.m_getNormal = GetNormal;
		sInterface.m_setTSpaceBasic = SetTSpace;

		// 0 if failed
		iCalcNewMethod = genTangSpaceDefault(&sContext);
	}

	if(!iCalcNewMethod) {
		/* sum tangents at connected vertices */
		for(i=0, tf=mtface, mf=mface; i < totface; mf++, tf++, i++) {
			v1= &mvert[mf->v1];
			v2= &mvert[mf->v2];
			v3= &mvert[mf->v3];

			if (mf->v4) {
				v4= &mvert[mf->v4];
				normal_quad_v3( fno,v4->co, v3->co, v2->co, v1->co);
			}
			else {
				v4= NULL;
				normal_tri_v3( fno,v3->co, v2->co, v1->co);
			}
		
			if(mtface) {
				uv1= tf->uv[0];
				uv2= tf->uv[1];
				uv3= tf->uv[2];
				uv4= tf->uv[3];
			}
			else {
				uv1= uv[0]; uv2= uv[1]; uv3= uv[2]; uv4= uv[3];
				map_to_sphere( &uv[0][0], &uv[0][1],orco[mf->v1][0], orco[mf->v1][1], orco[mf->v1][2]);
				map_to_sphere( &uv[1][0], &uv[1][1],orco[mf->v2][0], orco[mf->v2][1], orco[mf->v2][2]);
				map_to_sphere( &uv[2][0], &uv[2][1],orco[mf->v3][0], orco[mf->v3][1], orco[mf->v3][2]);
				if(v4)
					map_to_sphere( &uv[3][0], &uv[3][1],orco[mf->v4][0], orco[mf->v4][1], orco[mf->v4][2]);
			}
		
			tangent_from_uv(uv1, uv2, uv3, v1->co, v2->co, v3->co, fno, tang);
			sum_or_add_vertex_tangent(arena, &vtangents[mf->v1], tang, uv1);
			sum_or_add_vertex_tangent(arena, &vtangents[mf->v2], tang, uv2);
			sum_or_add_vertex_tangent(arena, &vtangents[mf->v3], tang, uv3);
		
			if(mf->v4) {
				v4= &mvert[mf->v4];
			
				tangent_from_uv(uv1, uv3, uv4, v1->co, v3->co, v4->co, fno, tang);
				sum_or_add_vertex_tangent(arena, &vtangents[mf->v1], tang, uv1);
				sum_or_add_vertex_tangent(arena, &vtangents[mf->v3], tang, uv3);
				sum_or_add_vertex_tangent(arena, &vtangents[mf->v4], tang, uv4);
			}
		}
	
		/* write tangent to layer */
		for(i=0, tf=mtface, mf=mface; i < totface; mf++, tf++, i++, tangent+=4) {
			len= (mf->v4)? 4 : 3; 

			if(mtface == NULL) {
				map_to_sphere( &uv[0][0], &uv[0][1],orco[mf->v1][0], orco[mf->v1][1], orco[mf->v1][2]);
				map_to_sphere( &uv[1][0], &uv[1][1],orco[mf->v2][0], orco[mf->v2][1], orco[mf->v2][2]);
				map_to_sphere( &uv[2][0], &uv[2][1],orco[mf->v3][0], orco[mf->v3][1], orco[mf->v3][2]);
				if(len==4)
					map_to_sphere( &uv[3][0], &uv[3][1],orco[mf->v4][0], orco[mf->v4][1], orco[mf->v4][2]);
			}
		
			mf_vi[0]= mf->v1;
			mf_vi[1]= mf->v2;
			mf_vi[2]= mf->v3;
			mf_vi[3]= mf->v4;
		
			for(j=0; j<len; j++) {
				vtang= find_vertex_tangent(vtangents[mf_vi[j]], mtface ? tf->uv[j] : uv[j]);
				normalize_v3_v3(tangent[j], vtang);
				((float *) tangent[j])[3]=1.0f;
			}
		}
	}
	
	BLI_memarena_free(arena);
	MEM_freeN(vtangents);
}

void DM_calc_auto_bump_scale(DerivedMesh *dm)
{
	/* int totvert= dm->getNumVerts(dm); */ /* UNUSED */
	int totface= dm->getNumTessFaces(dm);

	MVert * mvert = dm->getVertArray(dm);
	MFace * mface = dm->getTessFaceArray(dm);
	MTFace * mtface = dm->getTessFaceDataArray(dm, CD_MTFACE);

	if(mtface)
	{
		double dsum = 0.0;
		int nr_accumulated = 0;
		int f;

		for ( f=0; f<totface; f++ )
		{
			{
				float * verts[4], * tex_coords[4];
				const int nr_verts = mface[f].v4!=0 ? 4 : 3;
				int i, is_degenerate;

				verts[0]=mvert[mface[f].v1].co; verts[1]=mvert[mface[f].v2].co; verts[2]=mvert[mface[f].v3].co;
				tex_coords[0]=mtface[f].uv[0]; tex_coords[1]=mtface[f].uv[1]; tex_coords[2]=mtface[f].uv[2];
				if(nr_verts==4)
				{
					verts[3]=mvert[mface[f].v4].co;
					tex_coords[3]=mtface[f].uv[3];
				}

				// discard degenerate faces
				is_degenerate = 0;
				if(	equals_v3v3(verts[0], verts[1]) || equals_v3v3(verts[0], verts[2]) || equals_v3v3(verts[1], verts[2]) ||
					equals_v2v2(tex_coords[0], tex_coords[1]) || equals_v2v2(tex_coords[0], tex_coords[2]) || equals_v2v2(tex_coords[1], tex_coords[2]) )
				{
					is_degenerate = 1;
				}

				// verify last vertex as well if this is a quad
				if ( is_degenerate==0 && nr_verts==4 )
				{
					if(	equals_v3v3(verts[3], verts[0]) || equals_v3v3(verts[3], verts[1]) || equals_v3v3(verts[3], verts[2]) ||
						equals_v2v2(tex_coords[3], tex_coords[0]) || equals_v2v2(tex_coords[3], tex_coords[1]) || equals_v2v2(tex_coords[3], tex_coords[2]) )
					{
						is_degenerate = 1;
					}

					// verify the winding is consistent
					if ( is_degenerate==0 )
					{
						float prev_edge[2];
						int is_signed = 0;
						sub_v2_v2v2(prev_edge, tex_coords[0], tex_coords[3]);

						i = 0;
						while ( is_degenerate==0 && i<4 )
						{
							float cur_edge[2], signed_area;
							sub_v2_v2v2(cur_edge, tex_coords[(i+1)&0x3], tex_coords[i]);
							signed_area = prev_edge[0]*cur_edge[1] - prev_edge[1]*cur_edge[0];
							if ( i==0 ) is_signed = signed_area<0.0f ? 1 : 0;
							else if((is_signed!=0)!=(signed_area<0.0f)) is_degenerate=1;

							if ( is_degenerate==0 )
							{
								copy_v2_v2(prev_edge, cur_edge);
								++i;
							}
						}
					}
				}

				// proceed if not a degenerate face
				if ( is_degenerate==0 )
				{
					int nr_tris_to_pile=0;
					// quads split at shortest diagonal
					int offs = 0;		// initial triangulation is 0,1,2 and 0, 2, 3
					if ( nr_verts==4 )
					{
						float pos_len_diag0, pos_len_diag1;
						float vtmp[3];
						sub_v3_v3v3(vtmp, verts[2], verts[0]);
						pos_len_diag0 = dot_v3v3(vtmp, vtmp);
						sub_v3_v3v3(vtmp, verts[3], verts[1]);
						pos_len_diag1 = dot_v3v3(vtmp, vtmp);

						if(pos_len_diag1<pos_len_diag0)
							offs=1;		// alter split
						else if(pos_len_diag0==pos_len_diag1)		// do UV check instead
						{
							float tex_len_diag0, tex_len_diag1;

							sub_v2_v2v2(vtmp, tex_coords[2], tex_coords[0]);
							tex_len_diag0 = dot_v2v2(vtmp, vtmp);
							sub_v2_v2v2(vtmp, tex_coords[3], tex_coords[1]);
							tex_len_diag1 = dot_v2v2(vtmp, vtmp);

							if(tex_len_diag1<tex_len_diag0)
							{
								offs=1;		// alter split
							}
						}
					}
					nr_tris_to_pile = nr_verts-2 ;
					if ( nr_tris_to_pile==1 || nr_tris_to_pile==2 )
					{
						const int indices[] = {offs+0, offs+1, offs+2, offs+0, offs+2, (offs+3)&0x3 };
						int t;
						for ( t=0; t<nr_tris_to_pile; t++ )
						{
							float f2x_area_uv;
							float * p0 = verts[indices[t*3+0]];
							float * p1 = verts[indices[t*3+1]];
							float * p2 = verts[indices[t*3+2]];

							float edge_t0[2], edge_t1[2];
							sub_v2_v2v2(edge_t0, tex_coords[indices[t*3+1]], tex_coords[indices[t*3+0]]);
							sub_v2_v2v2(edge_t1, tex_coords[indices[t*3+2]], tex_coords[indices[t*3+0]]);

							f2x_area_uv = fabsf(edge_t0[0]*edge_t1[1] - edge_t0[1]*edge_t1[0]);
							if ( f2x_area_uv>FLT_EPSILON )
							{
								float norm[3], v0[3], v1[3], f2x_surf_area, fsurf_ratio;
								sub_v3_v3v3(v0, p1, p0);
								sub_v3_v3v3(v1, p2, p0);
								cross_v3_v3v3(norm, v0, v1);

								f2x_surf_area = len_v3(norm);
								fsurf_ratio = f2x_surf_area/f2x_area_uv;	// tri area divided by texture area

								++nr_accumulated;
								dsum += (double)(fsurf_ratio);
							}
						}
					}
				}
			}
		}

		// finalize
		{
			const float avg_area_ratio = (nr_accumulated>0) ? ((float)(dsum / nr_accumulated)) : 1.0f;
			const float use_as_render_bump_scale = sqrtf(avg_area_ratio);		// use width of average surface ratio as your bump scale
			dm->auto_bump_scale = use_as_render_bump_scale;
		}
	}
	else
	{
		dm->auto_bump_scale = 1.0f;
	}
}

void DM_vertex_attributes_from_gpu(DerivedMesh *dm, GPUVertexAttribs *gattribs, DMVertexAttribs *attribs)
{
	CustomData *vdata, *fdata, *tfdata = NULL;
	int a, b, layer;

	/* From the layers requested by the GLSL shader, figure out which ones are
	 * actually available for this derivedmesh, and retrieve the pointers */

	memset(attribs, 0, sizeof(DMVertexAttribs));

	vdata = &dm->vertData;
	fdata = tfdata = dm->getTessFaceDataLayout(dm);
	
	/* calc auto bump scale if necessary */
	if(dm->auto_bump_scale<=0.0f)
		DM_calc_auto_bump_scale(dm);

	/* add a tangent layer if necessary */
	for(b = 0; b < gattribs->totlayer; b++)
		if(gattribs->layer[b].type == CD_TANGENT)
			if(CustomData_get_layer_index(fdata, CD_TANGENT) == -1)
				DM_add_tangent_layer(dm);

	for(b = 0; b < gattribs->totlayer; b++) {
		if(gattribs->layer[b].type == CD_MTFACE) {
			/* uv coordinates */
			if(gattribs->layer[b].name[0])
				layer = CustomData_get_named_layer_index(tfdata, CD_MTFACE,
					gattribs->layer[b].name);
			else
				layer = CustomData_get_active_layer_index(tfdata, CD_MTFACE);

			if(layer != -1) {
				a = attribs->tottface++;

				attribs->tface[a].array = tfdata->layers[layer].data;
				attribs->tface[a].emOffset = tfdata->layers[layer].offset;
				attribs->tface[a].glIndex = gattribs->layer[b].glindex;
				attribs->tface[a].glTexco = gattribs->layer[b].gltexco;
			}
			/* BMESH ONLY, may need to get this working?, otherwise remove */
			/* else {
				int player;
				CustomData *pdata = dm->getPolyDataLayout(dm);
				
				if(gattribs->layer[b].name[0])
					player = CustomData_get_named_layer_index(pdata, CD_MTEXPOLY,
						gattribs->layer[b].name);
				else
					player = CustomData_get_active_layer_index(pdata, CD_MTEXPOLY);
				
				if (player != -1) {
					a = attribs->tottface++;
	
					attribs->tface[a].array = NULL;
					attribs->tface[a].emOffset = pdata->layers[layer].offset;
					attribs->tface[a].glIndex = gattribs->layer[b].glindex;
					attribs->tface[a].glTexco = gattribs->layer[b].gltexco;
					
				}
			}
			*/
		}
		else if(gattribs->layer[b].type == CD_MCOL) {
			/* vertex colors */
			if(gattribs->layer[b].name[0])
				layer = CustomData_get_named_layer_index(tfdata, CD_MCOL,
					gattribs->layer[b].name);
			else
				layer = CustomData_get_active_layer_index(tfdata, CD_MCOL);

			if(layer != -1) {
				a = attribs->totmcol++;

				attribs->mcol[a].array = tfdata->layers[layer].data;
				attribs->mcol[a].emOffset = tfdata->layers[layer].offset;
				attribs->mcol[a].glIndex = gattribs->layer[b].glindex;
			}
		}
		else if(gattribs->layer[b].type == CD_TANGENT) {
			/* tangents */
			layer = CustomData_get_layer_index(fdata, CD_TANGENT);

			if(layer != -1) {
				attribs->tottang = 1;

				attribs->tang.array = fdata->layers[layer].data;
				attribs->tang.emOffset = fdata->layers[layer].offset;
				attribs->tang.glIndex = gattribs->layer[b].glindex;
			}
		}
		else if(gattribs->layer[b].type == CD_ORCO) {
			/* original coordinates */
			layer = CustomData_get_layer_index(vdata, CD_ORCO);

			if(layer != -1) {
				attribs->totorco = 1;

				attribs->orco.array = vdata->layers[layer].data;
				attribs->orco.emOffset = vdata->layers[layer].offset;
				attribs->orco.glIndex = gattribs->layer[b].glindex;
				attribs->orco.glTexco = gattribs->layer[b].gltexco;
			}
		}
	}
}

/* Set object's bounding box based on DerivedMesh min/max data */
void DM_set_object_boundbox(Object *ob, DerivedMesh *dm)
{
	float min[3], max[3];

	INIT_MINMAX(min, max);

	dm->getMinMax(dm, min, max);

	if(!ob->bb)
		ob->bb= MEM_callocN(sizeof(BoundBox), "DM-BoundBox");

	boundbox_set_from_min_max(ob->bb, min, max);
}

/* --- NAVMESH (begin) --- */
#ifdef WITH_GAMEENGINE

/* BMESH_TODO, navmesh is not working right currently
 * All tools set this as MPoly data, but derived mesh currently draws from MFace (tessface)
 *
 * Proposed solution, rather then copy CD_RECAST into the MFace array,
 * use ORIGINDEX to get the original poly index and then get the CD_RECAST
 * data from the original me->mpoly layer. - campbell
 */


BM_INLINE int navmesh_bit(int a, int b)
{
	return (a & (1 << b)) >> b;
}

BM_INLINE void navmesh_intToCol(int i, float col[3])
{
	int	r = navmesh_bit(i, 0) + navmesh_bit(i, 3) * 2 + 1;
	int	g = navmesh_bit(i, 1) + navmesh_bit(i, 4) * 2 + 1;
	int	b = navmesh_bit(i, 2) + navmesh_bit(i, 5) * 2 + 1;
	col[0] = 1 - r*63.0f/255.0f;
	col[1] = 1 - g*63.0f/255.0f;
	col[2] = 1 - b*63.0f/255.0f;
}

static void navmesh_drawColored(DerivedMesh *dm)
{
	int a, glmode;
	MVert *mvert = (MVert *)CustomData_get_layer(&dm->vertData, CD_MVERT);
	MFace *mface = (MFace *)CustomData_get_layer(&dm->faceData, CD_MFACE);
	int *polygonIdx = (int *)CustomData_get_layer(&dm->faceData, CD_RECAST);
	float col[3];

	if (!polygonIdx)
		return;

	/*
	//UI_ThemeColor(TH_WIRE);
	glDisable(GL_LIGHTING);
	glLineWidth(2.0);
	dm->drawEdges(dm, 0, 1);
	glLineWidth(1.0);
	glEnable(GL_LIGHTING);*/

	glDisable(GL_LIGHTING);
	/*  if(GPU_buffer_legacy(dm) ) */ { /* TODO - VBO draw code, not high priority - campbell */
		DEBUG_VBO( "Using legacy code. drawNavMeshColored\n" );
		//glShadeModel(GL_SMOOTH);
		glBegin(glmode = GL_QUADS);
		for(a = 0; a < dm->numTessFaceData; a++, mface++) {
			int new_glmode = mface->v4?GL_QUADS:GL_TRIANGLES;
			int pi = polygonIdx[a];
			if (pi <= 0) {
				zero_v3(col);
			}
			else {
				navmesh_intToCol(pi, col);
			}

			if(new_glmode != glmode) {
				glEnd();
				glBegin(glmode = new_glmode);
			}
			glColor3fv(col);
			glVertex3fv(mvert[mface->v1].co);
			glVertex3fv(mvert[mface->v2].co);
			glVertex3fv(mvert[mface->v3].co);
			if(mface->v4) {
				glVertex3fv(mvert[mface->v4].co);
			}
		}
		glEnd();
	}
	glEnable(GL_LIGHTING);
}

static void navmesh_DM_drawFacesTex(DerivedMesh *dm,
			int (*setDrawOptions)(MTFace *tface, int has_mcol, int matnr),
			int (*compareDrawOptions)(void *userData, int cur_index, int next_index),
			void *userData)
{
	(void) setDrawOptions;
	(void) compareDrawOptions;
	(void) userData;

	navmesh_drawColored(dm);
}

static void navmesh_DM_drawFacesSolid(DerivedMesh *dm,
                                      float (*partial_redraw_planes)[4],
                                      int UNUSED(fast), int (*setMaterial)(int, void *attribs))
{
	(void) partial_redraw_planes;
	(void) setMaterial;

	//drawFacesSolid_original(dm, partial_redraw_planes, fast, setMaterial);
	navmesh_drawColored(dm);
}

static DerivedMesh *navmesh_dm_createNavMeshForVisualization(DerivedMesh *dm)
{
	DerivedMesh *result;
	int maxFaces = dm->getNumPolys(dm);
	int *recastData;
	int vertsPerPoly=0, nverts=0, ndtris=0, npolys=0;
	float* verts=NULL;
	unsigned short *dtris=NULL, *dmeshes=NULL, *polys=NULL;
	int *dtrisToPolysMap=NULL, *dtrisToTrisMap=NULL, *trisToFacesMap=NULL;
	int res;

	result = CDDM_copy(dm);
	if (!CustomData_has_layer(&result->faceData, CD_RECAST)) {
		int *sourceRecastData = (int*)CustomData_get_layer(&dm->faceData, CD_RECAST);
		if (sourceRecastData) {
			CustomData_add_layer_named(&result->faceData, CD_RECAST, CD_DUPLICATE,
			                           sourceRecastData, maxFaces, "recastData");
		}
	}
	recastData = (int*)CustomData_get_layer(&result->faceData, CD_RECAST);

	/* note: This is not good design! - really should not be doing this */
	result->drawFacesTex =  navmesh_DM_drawFacesTex;
	result->drawFacesSolid = navmesh_DM_drawFacesSolid;


	/* process mesh */
	res  = buildNavMeshDataByDerivedMesh(dm, &vertsPerPoly, &nverts, &verts, &ndtris, &dtris,
	                                     &npolys, &dmeshes, &polys, &dtrisToPolysMap, &dtrisToTrisMap,
	                                     &trisToFacesMap);
	if (res) {
		size_t polyIdx;

		/* invalidate concave polygon */
		for (polyIdx=0; polyIdx<(size_t)npolys; polyIdx++) {
			unsigned short* poly = &polys[polyIdx*2*vertsPerPoly];
			if (!polyIsConvex(poly, vertsPerPoly, verts)) {
				/* set negative polygon idx to all faces */
				unsigned short *dmesh = &dmeshes[4*polyIdx];
				unsigned short tbase = dmesh[2];
				unsigned short tnum = dmesh[3];
				unsigned short ti;

				for (ti=0; ti<tnum; ti++) {
					unsigned short triidx = dtrisToTrisMap[tbase+ti];
					unsigned short faceidx = trisToFacesMap[triidx];
					if (recastData[faceidx] > 0) {
						recastData[faceidx] = -recastData[faceidx];
					}
				}
			}
		}
	}
	else {
		printf("Error during creation polygon infos\n");
	}

	/* clean up */
	if (verts!=NULL)
		MEM_freeN(verts);
	if (dtris!=NULL)
		MEM_freeN(dtris);
	if (dmeshes!=NULL)
		MEM_freeN(dmeshes);
	if (polys!=NULL)
		MEM_freeN(polys);
	if (dtrisToPolysMap!=NULL)
		MEM_freeN(dtrisToPolysMap);
	if (dtrisToTrisMap!=NULL)
		MEM_freeN(dtrisToTrisMap);
	if (trisToFacesMap!=NULL)
		MEM_freeN(trisToFacesMap);

	return result;
}

#endif /* WITH_GAMEENGINE */

/* --- NAVMESH (end) --- */


void DM_init_origspace(DerivedMesh *dm)
{
	static float default_osf[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

	OrigSpaceLoop *lof_array = CustomData_get_layer(&dm->loopData, CD_ORIGSPACE_MLOOP);
	OrigSpaceLoop *lof;
	const int numpoly = dm->getNumPolys(dm);
	// const int numloop = dm->getNumLoops(dm);
	MPoly *mp = dm->getPolyArray(dm);
	int i, j;

	for (i = 0; i < numpoly; i++, mp++) {
		/* only quads/tri's for now */
		if (mp->totloop == 3 || mp->totloop == 4) {
			lof = lof_array + mp->loopstart;
			for (j = 0; j < mp->totloop; j++, lof++) {
				copy_v2_v2(lof->uv, default_osf[j]);
			}
		}
	}
}



/* derivedmesh info printing function,
 * to help track down differences DM output */

#ifndef NDEBUG
#include "BLI_dynstr.h"

static void dm_debug_info_layers(DynStr *dynstr, DerivedMesh *dm, void *(*getElemDataArray)(DerivedMesh *, int))
{
	int type;

	for (type = 0; type < CD_NUMTYPES; type++) {
		/* note: doesnt account for multiple layers */
		void *pt = getElemDataArray(dm, type);
		if (pt) {
			const char *name = CustomData_layertype_name(type);
			const int size = CustomData_sizeof(type);
			const char *structname;
			int structnum;
			CustomData_file_write_info(type, &structname, &structnum);
			BLI_dynstr_appendf(dynstr,
			                   "        dict(name='%s', struct='%s', type=%d, ptr='%p', elem=%d, length=%d),\n",
							   name, structname, type, (void *)pt, size, (int)(MEM_allocN_len(pt) / size));
		}
	}
}

char *DM_debug_info(DerivedMesh *dm)
{
	DynStr *dynstr= BLI_dynstr_new();
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
	dm_debug_info_layers(dynstr, dm, dm->getVertDataArray);
	BLI_dynstr_appendf(dynstr, "    ),\n");

	BLI_dynstr_appendf(dynstr, "    'loopLayers': (\n");
	dm_debug_info_layers(dynstr, dm, DM_get_loop_data_layer);
	BLI_dynstr_appendf(dynstr, "    ),\n");

	BLI_dynstr_appendf(dynstr, "    'edgeLayers': (\n");
	dm_debug_info_layers(dynstr, dm, dm->getEdgeDataArray);
	BLI_dynstr_appendf(dynstr, "    ),\n");

	BLI_dynstr_appendf(dynstr, "    'tessFaceLayers': (\n");
	dm_debug_info_layers(dynstr, dm, dm->getTessFaceDataArray);
	BLI_dynstr_appendf(dynstr, "    ),\n");

	BLI_dynstr_appendf(dynstr, "    'polyLayers': (\n");
	dm_debug_info_layers(dynstr, dm, DM_get_poly_data_layer);
	BLI_dynstr_appendf(dynstr, "    ),\n");

	BLI_dynstr_appendf(dynstr, "}\n");

	ret = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return ret;
}

void DM_debug_print(DerivedMesh *dm)
{
	char *str = DM_debug_info(dm);
	printf("%s", str);
	fflush(stdout);
	MEM_freeN(str);
}

#endif /* NDEBUG */
