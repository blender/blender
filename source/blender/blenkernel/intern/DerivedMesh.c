/*
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
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h" // N_T

#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_array.h"
#include "BLI_pbvh.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_key.h"
#include "BKE_modifier.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_texture.h"
#include "BKE_multires.h"
#include "BKE_particle.h"
#include "BKE_tessmesh.h"
#include "BKE_bvhutils.h"


#include "BLO_sys_types.h" // for intptr_t support

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_buffers.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_material.h"

#include "ED_sculpt.h" /* for ED_sculpt_modifiers_changed */

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

static MFace *dm_getFaceArray(DerivedMesh *dm)
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
			dm->numLoopData);
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
			dm->getNumFaces(dm));
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
	MLoop *tmp = MEM_callocN(sizeof(*tmp) * dm->numLoopData,
							 "dm_dupLoopArray tmp");

	if(tmp) dm->copyLoopArray(dm, tmp);

	return tmp;
}

static MPoly *dm_dupPolyArray(DerivedMesh *dm)
{
	MPoly *tmp = MEM_callocN(sizeof(*tmp) * dm->numFaceData,
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

static CustomData *dm_getFaceCData(DerivedMesh *dm)
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
	dm->getTessFaceArray = dm_getFaceArray;
	dm->getLoopArray = dm_getLoopArray;
	dm->getPolyArray = dm_getPolyArray;
	dm->dupVertArray = dm_dupVertArray;
	dm->dupEdgeArray = dm_dupEdgeArray;
	dm->dupTessFaceArray = dm_dupFaceArray;
	dm->dupLoopArray = dm_dupLoopArray;
	dm->dupPolyArray = dm_dupPolyArray;

	dm->getVertDataLayout = dm_getVertCData;
	dm->getEdgeDataLayout = dm_getEdgeCData;
	dm->getTessFaceDataLayout = dm_getFaceCData;
	dm->getLoopDataLayout = dm_getLoopCData;
	dm->getFaceDataLayout = dm_getPolyCData;

	dm->getVertData = DM_get_vert_data;
	dm->getEdgeData = DM_get_edge_data;
	dm->getTessFaceData = DM_get_face_data;
	dm->getVertDataArray = DM_get_vert_data_layer;
	dm->getEdgeDataArray = DM_get_edge_data_layer;
	dm->getTessFaceDataArray = DM_get_tessface_data_layer;

	bvhcache_init(&dm->bvhCache);
}

void DM_init(DerivedMesh *dm, DerivedMeshType type, int numVerts, int numEdges,
	     int numFaces, int numLoops, int numPoly)
{
	dm->type = type;
	dm->numVertData = numVerts;
	dm->numEdgeData = numEdges;
	dm->numFaceData = numFaces;
	dm->numLoopData = numLoops;
	dm->numPolyData = numPoly;

	DM_init_funcs(dm);
	
	dm->needsFree = 1;
}

void DM_from_template(DerivedMesh *dm, DerivedMesh *source, DerivedMeshType type,
                      int numVerts, int numEdges, int numFaces,
		      int numLoops, int numPolys)
{
	CustomData_copy(&source->vertData, &dm->vertData, CD_MASK_DERIVEDMESH,
					CD_CALLOC, numVerts);
	CustomData_copy(&source->edgeData, &dm->edgeData, CD_MASK_DERIVEDMESH,
					CD_CALLOC, numEdges);
	CustomData_copy(&source->faceData, &dm->faceData, CD_MASK_DERIVEDMESH,
					CD_CALLOC, numFaces);
	CustomData_copy(&source->loopData, &dm->loopData, CD_MASK_DERIVEDMESH,
	                CD_CALLOC, numLoops);
	CustomData_copy(&source->polyData, &dm->polyData, CD_MASK_DERIVEDMESH,
	                CD_CALLOC, numPolys);

	dm->type = type;
	dm->numVertData = numVerts;
	dm->numEdgeData = numEdges;
	dm->numFaceData = numFaces;
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
		CustomData_free(&dm->faceData, dm->numFaceData);
		CustomData_free(&dm->loopData, dm->numLoopData);
		CustomData_free(&dm->polyData, dm->numPolyData);

		return 1;
	}
	else {
		CustomData_free_temporary(&dm->vertData, dm->numVertData);
		CustomData_free_temporary(&dm->edgeData, dm->numEdgeData);
		CustomData_free_temporary(&dm->faceData, dm->numFaceData);
		CustomData_free_temporary(&dm->loopData, dm->numLoopData);
		CustomData_free_temporary(&dm->polyData, dm->numPolyData);

		return 0;
	}
}

void DM_DupPolys(DerivedMesh *source, DerivedMesh *target)
{
	CustomData_copy(&source->loopData, &target->loopData, CD_MASK_DERIVEDMESH, CD_CALLOC, source->numLoopData);
	CustomData_copy(&source->polyData, &target->polyData, CD_MASK_DERIVEDMESH, CD_CALLOC, source->numPolyData);

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

void DM_to_mesh(DerivedMesh *dm, Mesh *me, Object *ob)
{
	/* dm might depend on me, so we need to do everything with a local copy */
	Mesh tmp = *me;
	int totvert, totedge, totface, totloop, totpoly;
	int did_shapekeys=0;
	
	memset(&tmp.vdata, 0, sizeof(tmp.vdata));
	memset(&tmp.edata, 0, sizeof(tmp.edata));
	memset(&tmp.fdata, 0, sizeof(tmp.fdata));
	memset(&tmp.ldata, 0, sizeof(tmp.ldata));
	memset(&tmp.pdata, 0, sizeof(tmp.pdata));

	totvert = tmp.totvert = dm->getNumVerts(dm);
	totedge = tmp.totedge = dm->getNumEdges(dm);
	totface = tmp.totface = dm->getNumTessFaces(dm);
	totpoly = tmp.totpoly = dm->getNumFaces(dm);
	totloop = tmp.totloop = dm->numLoopData;

	CustomData_copy(&dm->vertData, &tmp.vdata, CD_MASK_MESH, CD_DUPLICATE, totvert);
	CustomData_copy(&dm->edgeData, &tmp.edata, CD_MASK_MESH, CD_DUPLICATE, totedge);
	CustomData_copy(&dm->faceData, &tmp.fdata, CD_MASK_MESH, CD_DUPLICATE, totface);
	CustomData_copy(&dm->loopData, &tmp.ldata, CD_MASK_MESH, CD_DUPLICATE, totloop);
	CustomData_copy(&dm->polyData, &tmp.pdata, CD_MASK_MESH, CD_DUPLICATE, totpoly);

	if (CustomData_has_layer(&dm->vertData, CD_SHAPEKEY)) {
		KeyBlock *kb;
		int i=0;
		
		if (ob) {
			for (kb=me->key->block.first; kb; kb=kb->next, i++) {
				if (i == ob->shapenr-1) {
					i = kb->uid;
					break;
				}
			}
			
			if (!kb) {
				printf("error in DM_to_mesh: could not find active shapekey! eek!!\n");
				i = INT_MAX;
			}
		} else {
			/*if no object, set to INT_MAX so we don't mess up any shapekey layers*/
			i = INT_MAX;
		}
		
		shapekey_layers_to_keyblocks(dm, me, i);
		did_shapekeys = 1;
	}
	
	/* not all DerivedMeshes store their verts/edges/faces in CustomData, so
	   we set them here in case they are missing */
	if(!CustomData_has_layer(&tmp.vdata, CD_MVERT))
		CustomData_add_layer(&tmp.vdata, CD_MVERT, CD_ASSIGN, dm->dupVertArray(dm), totvert);
	if(!CustomData_has_layer(&tmp.edata, CD_MEDGE))
		CustomData_add_layer(&tmp.edata, CD_MEDGE, CD_ASSIGN, dm->dupEdgeArray(dm), totedge);
	if(!CustomData_has_layer(&tmp.fdata, CD_MFACE))
		CustomData_add_layer(&tmp.fdata, CD_MFACE, CD_ASSIGN, dm->dupTessFaceArray(dm), totface);
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

	mesh_update_customdata_pointers(&tmp);

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
		VECCOPY(fp, mvert->co);
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
	CustomData_add_layer(&dm->faceData, type, alloctype, layer, dm->numFaceData);
}

static void DM_add_loop_layer(DerivedMesh *dm, int type, int alloctype, void *layer)
{
	CustomData_add_layer(&dm->loopData, type, alloctype, layer, dm->numLoopData);
}

void DM_add_face_layer(DerivedMesh *dm, int type, int alloctype, void *layer)
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

void *DM_get_face_data(DerivedMesh *dm, int index, int type)
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
	if(type == CD_MFACE)
		return dm->getTessFaceArray(dm);

	return CustomData_get_layer(&dm->faceData, type);
}

void *DM_get_face_data_layer(DerivedMesh *dm, int type)
{
	return CustomData_get_layer(&dm->polyData, type);
}

void DM_set_vert_data(DerivedMesh *dm, int index, int type, void *data)
{
	CustomData_set(&dm->vertData, index, type, data);
}

void DM_set_edge_data(DerivedMesh *dm, int index, int type, void *data)
{
	CustomData_set(&dm->edgeData, index, type, data);
}

void DM_set_face_data(DerivedMesh *dm, int index, int type, void *data)
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

void DM_copy_face_data(DerivedMesh *source, DerivedMesh *dest,
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

void DM_free_face_data(struct DerivedMesh *dm, int index, int count)
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

void DM_interp_face_data(DerivedMesh *source, DerivedMesh *dest,
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

	md->scene= scene;
	
	if (!(md->mode&eModifierMode_Realtime)) return NULL;
	if (mti->isDisabled && mti->isDisabled(md, 0)) return NULL;
	
	if (build_shapekey_layers && me->key && ob->shapenr <= BLI_countlist(&me->key->block)) {
		key_to_mesh(BLI_findlink(&me->key->block, ob->shapenr-1), me);
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
	
	orco = MEM_mallocN(sizeof(float)*3*totvert, "EditMesh Orco");

	eve = BMIter_New(&iter, em->bm, BM_VERTS_OF_MESH, NULL);
	for (a=0; eve; eve=BMIter_Step(&iter), a+=3)
		VECCOPY(orco+a, eve->co);
	
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
			if (clmd) {
				KeyBlock *kb= key_get_keyblock(ob_get_key(ob), clmd->sim_parms->shapekey_rest);
	
				if(kb->data)
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

	if(em) dm= CDDM_from_BMEditMesh(em, me, 0);
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

void weight_to_rgb(float input, float *fr, float *fg, float *fb)
{
	float blend;
	
	blend= ((input/2.0f)+0.5f);
	
	if (input<=0.25f){	// blue->cyan
		*fr= 0.0f;
		*fg= blend*input*4.0f;
		*fb= blend;
	}
	else if (input<=0.50f){	// cyan->green
		*fr= 0.0f;
		*fg= blend;
		*fb= blend*(1.0f-((input-0.25f)*4.0f)); 
	}
	else if (input <= 0.75f){	// green->yellow
		*fr= blend * ((input-0.50f)*4.0f);
		*fg= blend;
		*fb= 0.0f;
	}
	else if (input <= 1.0f){ // yellow->red
		*fr= blend;
		*fg= blend * (1.0f-((input-0.75f)*4.0f)); 
		*fb= 0.0f;
	}
}

static void calc_weightpaint_vert_color(Object *ob, ColorBand *coba, int vert, unsigned char *col)
{
	Mesh *me = ob->data;
	float colf[4], input = 0.0f;
	int i;

	if (me->dvert) {
		for (i=0; i<me->dvert[vert].totweight; i++)
			if (me->dvert[vert].dw[i].def_nr==ob->actdef-1)
				input+=me->dvert[vert].dw[i].weight;		
	}

	CLAMP(input, 0.0f, 1.0f);
	
	if(coba)
		do_colorband(coba, input, colf);
	else
		weight_to_rgb(input, colf, colf+1, colf+2);
	
	col[3] = (unsigned char)(colf[0] * 255.0f);
	col[2] = (unsigned char)(colf[1] * 255.0f);
	col[1] = (unsigned char)(colf[2] * 255.0f);
	col[0] = 255;
}

static ColorBand *stored_cb= NULL;

void vDM_ColorBand_store(ColorBand *coba)
{
	stored_cb= coba;
}

static void add_weight_mcol_dm(Object *ob, DerivedMesh *dm)
{
	// Mesh *me = ob->data; // UNUSED
	MFace *mf = dm->getTessFaceArray(dm);
	MLoop *mloop = dm->getLoopArray(dm), *ml;
	MPoly *mp = dm->getPolyArray(dm);
	ColorBand *coba= stored_cb;	/* warning, not a local var */
	unsigned char *wtcol;
	unsigned char(*wlcol)[4] = NULL;
	BLI_array_declare(wlcol);
	int i, j, totface=dm->getNumTessFaces(dm), totloop;
	int *origIndex = dm->getVertDataArray(dm, CD_ORIGINDEX);
	
	wtcol = MEM_callocN (sizeof (unsigned char) * totface*4*4, "weightmap");
	
	/*first add colors to the tesselation faces*/
	memset(wtcol, 0x55, sizeof (unsigned char) * totface*4*4);
	for (i=0; i<totface; i++, mf++) {
		/*origindex being NULL means we're operating on original mesh data*/
		calc_weightpaint_vert_color(ob, coba, origIndex ? origIndex[mf->v1] : mf->v1, &wtcol[(i*4 + 0)*4]); 
		calc_weightpaint_vert_color(ob, coba, origIndex ? origIndex[mf->v2] : mf->v2, &wtcol[(i*4 + 1)*4]); 
		calc_weightpaint_vert_color(ob, coba, origIndex ? origIndex[mf->v3] : mf->v3, &wtcol[(i*4 + 2)*4]); 
		if (mf->v4)
			calc_weightpaint_vert_color(ob, coba, origIndex ? origIndex[mf->v4] : mf->v4, &wtcol[(i*4 + 3)*4]); 
	}
	
	CustomData_add_layer(&dm->faceData, CD_WEIGHT_MCOL, CD_ASSIGN, wtcol, totface);

	/*now add to loops, so the data can be passed through the modifier stack*/
	totloop = 0;
	for (i=0; i<dm->numPolyData; i++, mp++) {
		ml = mloop + mp->loopstart;

		for (j=0; j<mp->totloop; j++, ml++, totloop++) {
			BLI_array_growone(wlcol);

			calc_weightpaint_vert_color(ob, coba, origIndex ? origIndex[ml->v] : ml->v,
										(unsigned char *)&wlcol[totloop]);
		}
	}

	CustomData_add_layer(&dm->loopData, CD_WEIGHT_MLOOPCOL, CD_ASSIGN, wlcol, totloop);
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
			printf("eek! lost a shapekey layer!\n");
		}
	}
}

static void add_shapekey_layers(DerivedMesh *dm, Mesh *me, Object *UNUSED(ob))
{
	KeyBlock *kb;
	Key *key = me->key;
	int a, b;
	
	if (!me->key)
		return;
	
	if (dm->numVertData != me->totvert) {
		printf("error in add_shapekey_layers: dm isn't the same size as me\n");
		return;
	}
		
	for (a=0, kb=key->block.first; kb; kb=kb->next, a++) {
		float (*cos)[3] = CustomData_add_layer_named(&dm->vertData, CD_SHAPEKEY, CD_CALLOC, NULL, dm->numVertData, kb->name);
		int ci = CustomData_get_layer_index_n(&dm->vertData, CD_SHAPEKEY, a);
		
		dm->vertData.layers[ci].uid = kb->uid;
		if (kb->totelem != dm->numVertData) {
			printf("error in add_shapekey_layers: totelem and totvert don't match");
			continue;
		}
		
		for (b=0; b<kb->totelem; b++, cos++) {
			copy_v3_v3((float *)cos, ((float*)kb->data)+b*3);
		}
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
	ModifierData *firstmd, *md;
	LinkNode *datamasks, *curr;
	CustomDataMask mask, nextmask;
	float (*deformedVerts)[3] = NULL;
	DerivedMesh *dm=NULL, *orcodm, *clothorcodm, *finaldm;
	int numVerts = me->totvert;
	int required_mode;
	int isPrevDeform= FALSE;
	int skipVirtualArmature = (useDeform < 0);
	MultiresModifierData *mmd= get_multires_modifier(scene, ob, 0);
	int has_multires = mmd != NULL, multires_applied = 0;
	int sculpt_mode = ob->mode & OB_MODE_SCULPT && ob->sculpt;

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
			if(dm && isPrevDeform &&  mti->dependsOnNormals && mti->dependsOnNormals(md)) {
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
					DerivedMesh *tdm = CDDM_copy(dm, 0);
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

				/* Constructive modifiers need to have an origindex
				 * otherwise they wont have anywhere to copy the data from.
				 *
				 * Also create ORIGINDEX data if any of the following modifiers
				 * requests it, this way Mirror, Solidify etc will keep ORIGINDEX
				 * data by using generic DM_copy_vert_data() functions.
				 */
				if(needMapping || (nextmask & CD_MASK_ORIGINDEX)) {
					int i, *orig;
					
					/* calc */
					DM_add_vert_layer(dm, CD_ORIGINDEX, CD_CALLOC, NULL);
					DM_add_edge_layer(dm, CD_ORIGINDEX, CD_CALLOC, NULL);
					DM_add_face_layer(dm, CD_ORIGINDEX, CD_CALLOC, NULL);

					orig = DM_get_vert_data_layer(dm, CD_ORIGINDEX);
					for(i=0; i<dm->numVertData; i++) *orig++= i;
					orig = DM_get_edge_data_layer(dm, CD_ORIGINDEX);
					for(i=0; i<dm->numEdgeData; i++) *orig++= i;
					orig = DM_get_face_data_layer(dm, CD_ORIGINDEX);
					for(i=0; i<dm->numPolyData; i++) *orig++= i;
				}

				if((dataMask & CD_MASK_WEIGHT_MCOL) && (ob->mode & OB_MODE_WEIGHT_PAINT))
					add_weight_mcol_dm(ob, dm);

			}

			
			/* set the DerivedMesh to only copy needed data */
			mask= (CustomDataMask)GET_INT_FROM_POINTER(curr->link);
			/* needMapping check here fixes bug [#28112], otherwise its
			 * possible that it wont be copied */
			DM_set_only_copy(dm, mask | (needMapping ? CD_MASK_ORIGINDEX : 0));
			
			/* add cloth rest shape key if need */
			if(mask & CD_MASK_CLOTH_ORCO)
				add_orco_dm(ob, NULL, dm, clothorcodm, CD_CLOTH_ORCO);

			/* add an origspace layer if needed */
			if(((CustomDataMask)GET_INT_FROM_POINTER(curr->link)) & CD_MASK_ORIGSPACE)
				if(!CustomData_has_layer(&dm->faceData, CD_ORIGSPACE))
					DM_add_tessface_layer(dm, CD_ORIGSPACE, CD_DEFAULT, NULL);

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
		finaldm = CDDM_copy(dm, 0);

		dm->release(dm);

		CDDM_apply_vert_coords(finaldm, deformedVerts);
		CDDM_calc_normals(finaldm);

		if((dataMask & CD_MASK_WEIGHT_MCOL) && (ob->mode & OB_MODE_WEIGHT_PAINT))
			add_weight_mcol_dm(ob, finaldm);
	} else if(dm) {
		finaldm = dm;
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

		if(recalc_normals)
			CDDM_calc_normals(finaldm);
		
		if((dataMask & CD_MASK_WEIGHT_MCOL) && (ob->mode & OB_MODE_WEIGHT_PAINT))
			add_weight_mcol_dm(ob, finaldm);
		
	}

	/* add an orco layer if needed */
	if(dataMask & CD_MASK_ORCO) {
		add_orco_dm(ob, NULL, finaldm, orcodm, CD_ORCO);

		if(deform_r && *deform_r)
			add_orco_dm(ob, NULL, *deform_r, NULL, CD_ORCO);
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

	eve = BMIter_New(&iter, em->bm, BM_VERTS_OF_MESH, NULL);
	for (i=0; eve; eve=BMIter_Step(&iter), i++) {
		VECCOPY(cos[i], eve->co);
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
					DerivedMesh *tdm = CDDM_copy(dm, 0);
					if(!(cage_r && dm == *cage_r)) dm->release(dm);
					dm = tdm;

					CDDM_apply_vert_coords(dm, deformedVerts);
					CDDM_calc_normals(dm);
				} else if(cage_r && dm == *cage_r) {
					/* dm may be changed by this modifier, so we need to copy it
					 */
					dm = CDDM_copy(dm, 0);
				}

			} else {
				dm = CDDM_from_BMEditMesh(em, ob->data, 0);

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

			if(mask & CD_MASK_ORIGSPACE)
				if(!CustomData_has_layer(&dm->faceData, CD_ORIGSPACE))
					DM_add_tessface_layer(dm, CD_ORIGSPACE, CD_DEFAULT, NULL);
			
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
				*cage_r = CDDM_copy(dm, 0);
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
		*final_r = CDDM_copy(dm, 0);

		if(!(cage_r && dm == *cage_r)) dm->release(dm);

		CDDM_apply_vert_coords(*final_r, deformedVerts);
		CDDM_calc_normals(*final_r);
	} else if (dm) {
		*final_r = dm;
	} else if (!deformedVerts && cage_r && *cage_r) {
		*final_r = *cage_r;
	} else {
		*final_r = getEditDerivedBMesh(em, ob, deformedVerts);
		deformedVerts = NULL;
	}

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
		ED_sculpt_modifiers_changed(ob);
	}
}

static void mesh_build_data(Scene *scene, Object *ob, CustomDataMask dataMask,
	int build_shapekey_layers)
{
	Object *obact = scene->basact?scene->basact->object:NULL;
	int editing = paint_facesel_test(ob);
	/* weight paint and face select need original indices because of selection buffer drawing */
	int needMapping = (ob==obact) && (editing || (ob->mode & (OB_MODE_WEIGHT_PAINT|OB_MODE_VERTEX_PAINT)));

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

	VECCOPY(vec, co);
	vec+= 3;
	if(no_f) {
		VECCOPY(vec, no_f);
	}
	else {
		VECCOPY(vec, no_s);
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
	int numFaces;

} SGLSLMeshToTangent;

// interface
#include "mikktspace.h"

static int GetNumFaces(const SMikkTSpaceContext * pContext)
{
	SGLSLMeshToTangent * pMesh = (SGLSLMeshToTangent *) pContext->m_pUserData;
	return pMesh->numFaces;
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
	VECCOPY(fPos, co);
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
			VECCOPY(fNorm, &pMesh->precomputedFaceNormals[3*face_num]);
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
	VECCOPY(pRes, fvTangent);
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
		mesh2tangent.numFaces = totface;

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

void DM_vertex_attributes_from_gpu(DerivedMesh *dm, GPUVertexAttribs *gattribs, DMVertexAttribs *attribs)
{
	CustomData *vdata, *fdata, *tfdata = NULL;
	int a, b, layer;

	/* From the layers requested by the GLSL shader, figure out which ones are
	 * actually available for this derivedmesh, and retrieve the pointers */

	memset(attribs, 0, sizeof(DMVertexAttribs));

	vdata = &dm->vertData;
	fdata = tfdata = dm->getTessFaceDataLayout(dm);
	
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
			} /*else {
				int player;
				CustomData *pdata = dm->getFaceDataLayout(dm);
				
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
			}*/
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
		ob->bb= MEM_callocN(sizeof(BoundBox), "bb");

	boundbox_set_from_min_max(ob->bb, min, max);
}
