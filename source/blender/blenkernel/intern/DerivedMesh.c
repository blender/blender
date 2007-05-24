/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <zlib.h>

#include "PIL_time.h"

#include "MEM_guardedalloc.h"

#include "DNA_effect_types.h"
#include "DNA_mesh_types.h"
#include "DNA_key_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_object_fluidsim.h" // N_T
#include "DNA_scene_types.h" // N_T
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"
#include "BLI_linklist.h"

#include "BKE_utildefines.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_subsurf.h"
#include "BKE_deform.h"
#include "BKE_modifier.h"
#include "BKE_key.h"
#include "BKE_bmesh.h"

#ifdef WITH_VERSE
#include "BKE_verse.h"
#endif

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_resources.h"

#include "multires.h"

// headers for fluidsim bobj meshes
#include <stdlib.h>
#include "LBM_fluidsim.h"
#include "elbeem.h"

///////////////////////////////////
///////////////////////////////////

MVert *dm_getVertArray(DerivedMesh *dm)
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

MEdge *dm_getEdgeArray(DerivedMesh *dm)
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

MFace *dm_getFaceArray(DerivedMesh *dm)
{
	MFace *mface = CustomData_get_layer(&dm->faceData, CD_MFACE);

	if (!mface) {
		mface = CustomData_add_layer(&dm->faceData, CD_MFACE, CD_CALLOC, NULL,
			dm->getNumFaces(dm));
		CustomData_set_layer_flag(&dm->faceData, CD_MFACE, CD_FLAG_TEMPORARY);
		dm->copyFaceArray(dm, mface);
	}

	return mface;
}

MLoop *dm_getLoopArray(DerivedMesh *dm)
{
	MLoop *mloop = CustomData_get_layer(&dm->loopData, CD_MLOOP);

	if (!mloop) {
		mloop = CustomData_add_layer(&dm->loopData, CD_MLOOP, CD_CALLOC, NULL,
			dm->getNumLoops(dm));
		CustomData_set_layer_flag(&dm->loopData, CD_MFACE, CD_FLAG_TEMPORARY);
		dm->copyLoopArray(dm, mloop);
	}

	return mloop;
}

MPoly *dm_getPolyArray(DerivedMesh *dm)
{
	MPoly *mpoly = CustomData_get_layer(&dm->faceData, CD_MPOLY);

	if (!mpoly) {
		mpoly = CustomData_add_layer(&dm->polyData, CD_MPOLY, CD_CALLOC, NULL,
			dm->getNumPolys(dm));
		CustomData_set_layer_flag(&dm->polyData, CD_MPOLY, CD_FLAG_TEMPORARY);
		dm->copyPolyArray(dm, mpoly);
	}

	return mpoly;
}

MVert *dm_dupVertArray(DerivedMesh *dm)
{
	MVert *tmp = MEM_callocN(sizeof(*tmp) * dm->getNumVerts(dm),
	                         "dm_dupVertArray tmp");

	if(tmp) dm->copyVertArray(dm, tmp);

	return tmp;
}

MEdge *dm_dupEdgeArray(DerivedMesh *dm)
{
	MEdge *tmp = MEM_callocN(sizeof(*tmp) * dm->getNumEdges(dm),
	                         "dm_dupEdgeArray tmp");

	if(tmp) dm->copyEdgeArray(dm, tmp);

	return tmp;
}

MFace *dm_dupFaceArray(DerivedMesh *dm)
{
	MFace *tmp = MEM_callocN(sizeof(*tmp) * dm->getNumFaces(dm),
	                         "dm_dupFaceArray tmp");

	if(tmp) dm->copyFaceArray(dm, tmp);

	return tmp;
}

MLoop *dm_dupLoopArray(DerivedMesh *dm)
{
	MLoop *tmp = MEM_callocN(sizeof(*tmp) * dm->getNumLoops(dm),
	                         "dm_dupFaceArray tmp");

	if(tmp) dm->copyLoopArray(dm, tmp);

	return tmp;
}


MPoly *dm_dupPolyArray(DerivedMesh *dm)
{
	MPoly *tmp = MEM_callocN(sizeof(*tmp) * dm->getNumPolys(dm),
	                         "dm_dupFaceArray tmp");

	if(tmp) dm->copyPolyArray(dm, tmp);

	return tmp;
}

void *DM_get_loop_data_layer(DerivedMesh *dm, int type);
void *DM_get_poly_data_layer(DerivedMesh *dm, int type);
void *DM_get_loop_data(DerivedMesh *dm, int index, int type);
void *DM_get_poly_data(DerivedMesh *dm, int index, int type);

void DM_init_funcs(DerivedMesh *dm)
{
	/* default function implementations */
	dm->getVertArray = dm_getVertArray;
	dm->getEdgeArray = dm_getEdgeArray;
	dm->getFaceArray = dm_getFaceArray;
	dm->getLoopArray = dm_getLoopArray;
	dm->getPolyArray = dm_getPolyArray;

	dm->dupVertArray = dm_dupVertArray;
	dm->dupEdgeArray = dm_dupEdgeArray;
	dm->dupFaceArray = dm_dupFaceArray;
	dm->dupLoopArray = dm_dupLoopArray;
	dm->dupPolyArray = dm_dupPolyArray;

	dm->getVertData = DM_get_vert_data;
	dm->getEdgeData = DM_get_edge_data;
	dm->getFaceData = DM_get_face_data;
	dm->getLoopData = DM_get_loop_data;
	dm->getPolyData = DM_get_poly_data;
	
	dm->getVertDataArray = DM_get_vert_data_layer;
	dm->getEdgeDataArray = DM_get_edge_data_layer;
	dm->getFaceDataArray = DM_get_face_data_layer;
	dm->getLoopDataArray = DM_get_loop_data_layer;
	dm->getPolyDataArray = DM_get_poly_data_layer;
	
}

void DM_init(DerivedMesh *dm,
             int numVerts, int numEdges, int numFaces, int numLoops, int numPolys)
{
	CustomData_add_layer(&dm->vertData, CD_ORIGINDEX, CD_CALLOC, NULL, numVerts);
	CustomData_add_layer(&dm->edgeData, CD_ORIGINDEX, CD_CALLOC, NULL, numEdges);
	CustomData_add_layer(&dm->faceData, CD_ORIGINDEX, CD_CALLOC, NULL, numFaces);
	CustomData_add_layer(&dm->loopData, CD_ORIGINDEX, CD_CALLOC, NULL, numLoops);
	CustomData_add_layer(&dm->polyData, CD_ORIGINDEX, CD_CALLOC, NULL, numPolys);

	dm->numVertData = numVerts;
	dm->numEdgeData = numEdges;
	dm->numFaceData = numFaces;
	dm->numLoopData = numLoops;
	dm->numPolyData = numPolys;

	DM_init_funcs(dm);
	
	dm->needsFree = 1;
}

void DM_from_template(DerivedMesh *dm, DerivedMesh *source,
                      int numVerts, int numEdges, int numFaces, int numLoops, int numPolys)
{
	printf("DM_from_template called!\n");
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

void DM_to_mesh(DerivedMesh *dm, Mesh *me)
{
	/* dm might depend on me, so we need to do everything with a local copy */
	Mesh tmp = *me;
	int totvert, totedge, totface, totloop, totpoly;

	printf("DM_to_mesh called!\n");

	memset(&tmp.vdata, 0, sizeof(tmp.vdata));
	memset(&tmp.edata, 0, sizeof(tmp.edata));
	memset(&tmp.fdata, 0, sizeof(tmp.fdata));
	memset(&tmp.ldata, 0, sizeof(tmp.ldata));
	memset(&tmp.pdata, 0, sizeof(tmp.pdata));

	totvert = tmp.totvert = dm->getNumVerts(dm);
	totedge = tmp.totedge = dm->getNumEdges(dm);
	totface = tmp.totface = dm->getNumFaces(dm);
	totloop = tmp.totloop = dm->getNumLoops(dm);
	totpoly = tmp.totpoly = dm->getNumPolys(dm);

	CustomData_copy(&dm->vertData, &tmp.vdata, CD_MASK_MESH, CD_DUPLICATE, totvert);
	CustomData_copy(&dm->edgeData, &tmp.edata, CD_MASK_MESH, CD_DUPLICATE, totedge);
	CustomData_copy(&dm->faceData, &tmp.fdata, CD_MASK_MESH, CD_DUPLICATE, totface);
	CustomData_copy(&dm->loopData, &tmp.fdata, CD_MASK_MESH, CD_DUPLICATE, totloop);
	CustomData_copy(&dm->polyData, &tmp.fdata, CD_MASK_MESH, CD_DUPLICATE, totpoly);

	/* not all DerivedMeshes store their verts/edges/faces in CustomData, so
	   we set them here in case they are missing */
	if(!CustomData_has_layer(&tmp.vdata, CD_MVERT))
		CustomData_add_layer(&tmp.vdata, CD_MVERT, CD_ASSIGN, dm->dupVertArray(dm), totvert);
	if(!CustomData_has_layer(&tmp.edata, CD_MEDGE))
		CustomData_add_layer(&tmp.edata, CD_MEDGE, CD_ASSIGN, dm->dupEdgeArray(dm), totedge);
	if(!CustomData_has_layer(&tmp.fdata, CD_MFACE))
		CustomData_add_layer(&tmp.fdata, CD_MFACE, CD_ASSIGN, dm->dupFaceArray(dm), totface);
	if(!CustomData_has_layer(&tmp.ldata, CD_MLOOP))
		CustomData_add_layer(&tmp.ldata, CD_MLOOP, CD_ASSIGN, dm->dupLoopArray(dm), totloop);
	if(!CustomData_has_layer(&tmp.pdata, CD_MPOLY))
		CustomData_add_layer(&tmp.pdata, CD_MPOLY, CD_ASSIGN, dm->dupPolyArray(dm), totpoly);

	mesh_update_customdata_pointers(&tmp);

	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->fdata, me->totface);
	CustomData_free(&me->ldata, me->totloop);
	CustomData_free(&me->pdata, me->totpoly);

	/* if the number of verts has changed, remove invalid data */
	if(tmp.totvert != me->totvert) {
		if(me->key) me->key->id.us--;
		me->key = NULL;
	}

	*me = tmp;
}

void DM_set_only_copy(DerivedMesh *dm, CustomDataMask mask)
{
	CustomData_set_only_copy(&dm->vertData, mask);
	CustomData_set_only_copy(&dm->edgeData, mask);
	CustomData_set_only_copy(&dm->faceData, mask);
	CustomData_set_only_copy(&dm->loopData, mask);
	CustomData_set_only_copy(&dm->polyData, mask);
}

void DM_add_vert_layer(DerivedMesh *dm, int type, int alloctype, void *layer)
{
	CustomData_add_layer(&dm->vertData, type, alloctype, layer, dm->numVertData);
}

void DM_add_edge_layer(DerivedMesh *dm, int type, int alloctype, void *layer)
{
	CustomData_add_layer(&dm->edgeData, type, alloctype, layer, dm->numEdgeData);
}

void DM_add_face_layer(DerivedMesh *dm, int type, int alloctype, void *layer)
{
	CustomData_add_layer(&dm->faceData, type, alloctype, layer, dm->numFaceData);
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

void *DM_get_loop_data(DerivedMesh *dm, int index, int type)
{
	return CustomData_get(&dm->loopData, index, type);
}

void *DM_get_poly_data(DerivedMesh *dm, int index, int type)
{
	return CustomData_get(&dm->polyData, index, type);
}

void *DM_get_vert_data_layer(DerivedMesh *dm, int type)
{
	return CustomData_get_layer(&dm->vertData, type);
}

void *DM_get_edge_data_layer(DerivedMesh *dm, int type)
{
	return CustomData_get_layer(&dm->edgeData, type);
}

void *DM_get_face_data_layer(DerivedMesh *dm, int type)
{
	return CustomData_get_layer(&dm->faceData, type);
}

void *DM_get_loop_data_layer(DerivedMesh *dm, int type)
{
	return CustomData_get_layer(&dm->loopData, type);
}

void *DM_get_poly_data_layer(DerivedMesh *dm, int type)
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

void DM_copy_face_data(DerivedMesh *source, DerivedMesh *dest,
                       int source_index, int dest_index, int count)
{
	CustomData_copy_data(&source->faceData, &dest->faceData,
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

void DM_free_face_data(struct DerivedMesh *dm, int index, int count)
{
	CustomData_free_elem(&dm->faceData, index, count);
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

void DM_interp_face_data(DerivedMesh *source, DerivedMesh *dest,
                         int *src_indices,
                         float *weights, FaceVertWeight *vert_weights,
                         int count, int dest_index)
{
	CustomData_interp(&source->faceData, &dest->faceData, src_indices,
	                  weights, (float*)vert_weights, count, dest_index);
}

void DM_swap_face_data(DerivedMesh *dm, int index, int *corner_indices)
{
	CustomData_swap(&dm->faceData, index, corner_indices);
}

static DerivedMesh *getMeshDerivedMesh(Mesh *me, Object *ob, float (*vertCos)[3])
{
	DerivedMesh *dm = CDDM_from_mesh(me, ob);
	int i, dofluidsim;

	dofluidsim = ((ob->fluidsimFlag & OB_FLUIDSIM_ENABLE) &&
	              (ob->fluidsimSettings->type & OB_FLUIDSIM_DOMAIN)&&
	              (ob->fluidsimSettings->meshSurface) &&
	              (1) && (!give_parteff(ob)) && // doesnt work together with particle systems!
	              (me->totvert == ((Mesh *)(ob->fluidsimSettings->meshSurface))->totvert));

	if (vertCos && !dofluidsim)
		CDDM_apply_vert_coords(dm, vertCos);

	CDDM_calc_normals(dm);

	/* apply fluidsim normals */ 	
	if (dofluidsim) {
		// use normals from readBobjgz
		// TODO? check for modifiers!?
		MVert *fsvert = ob->fluidsimSettings->meshSurfNormals;
		short (*normals)[3] = MEM_mallocN(sizeof(short)*3*me->totvert, "fluidsim nor");

		for (i=0; i<me->totvert; i++) {
			VECCOPY(normals[i], fsvert[i].no);
			//mv->no[0]= 30000; mv->no[1]= mv->no[2]= 0; // DEBUG fixed test normals
		}

		CDDM_apply_vert_normals(dm, normals);

		MEM_freeN(normals);
	}

	return dm;
}

///
typedef struct {
	DerivedMesh dm;

	BME_Mesh *bmesh;
	float (*vertexCos)[3];
	float (*vertexNos)[3];
	float (*faceNos)[3];
	bglCacheDrawInterface *cdraw;
	int recalc_cdraw;
} EditBMeshDerivedMesh;

typedef struct {
	DerivedMesh dm;

	EditMesh *em;
	float (*vertexCos)[3];
	float (*vertexNos)[3];
	float (*faceNos)[3];
} EditMeshDerivedMesh;

static void emDM_recalcDrawCache(EditBMeshDerivedMesh *emdm);

static void emDM_foreachMappedVert(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no_f, short *no_s), void *userData)
{
	EditBMeshDerivedMesh *bem = (EditBMeshDerivedMesh*) dm;
	BME_Vert *bev;
	int i;
	//short no[3];
	
	for (bev=bem->bmesh->verts.first, i=0; bev; i++, bev=bev->next) {
		if (bem->vertexCos) {
			func(userData, i, bem->vertexCos[i], bem->vertexNos[i], NULL);
		} else {
			func(userData, i, bev->co, bev->no, NULL);
		}
	}
}
static void emDM_foreachMappedEdge(DerivedMesh *dm, void (*func)(void *userData, int index, float *v0co, float *v1co), void *userData)
{
	EditBMeshDerivedMesh *bem = (EditBMeshDerivedMesh*) dm;
	BME_Edge *bed;
	int i;
	//short no[3];
	
	for (bed=bem->bmesh->edges.first, i=0; bed; i++, bed=bed->next) {
		if (bem->vertexCos) {
			func(userData, i, bem->vertexCos[bed->v1->tflag2], bem->vertexNos[bed->v1->tflag2]);
		} else {
			func(userData, i, bed->v1->co, bed->v2->co);
		}
	}
}

static void emDM_drawMappedEdges(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData) 
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;
	emdm->bmesh = emdm->bmesh;
}

static void emDM_drawEdges(DerivedMesh *dm, int drawLooseEdges)
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;
	emdm->bmesh = emdm->bmesh;
	emdm->bmesh = emdm->bmesh;
	if (emdm->recalc_cdraw) emDM_recalcDrawCache(emdm);

	emdm->cdraw->drawWireEdges(emdm->cdraw, 1.0);
}

static void emDM_drawMappedEdgesInterp(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void (*setDrawInterpOptions)(void *userData, int index, float t), void *userData) 
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;
}

static void emDM_drawUVEdges(DerivedMesh *dm)
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;
	emdm->bmesh = emdm->bmesh;

}

static void emDM_drawEditVerts(DerivedMesh *dm, float alpha)
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;
   if (emdm->recalc_cdraw) emDM_recalcDrawCache(emdm);

   emdm->cdraw->drawVertPoints(emdm->cdraw, alpha);
}

static void emDM_recalcDrawCache(EditBMeshDerivedMesh *emdm)
{
	BME_Poly *efa;
	BME_Loop *loop;
	BME_Edge *eed;
	BME_Vert *eve;
	float v[3][3], no[3][3];
	float nor[3], vsize;
	char high[4], vcol[3], svcol[3], ecol[3], secol[3];

	BIF_GetThemeColor3ubv(TH_VERTEX, vcol);
	BIF_GetThemeColor3ubv(TH_VERTEX_SELECT, svcol);
	vsize = BIF_GetThemeValuef(TH_VERTEX_SIZE);

	BIF_GetThemeColor3ubv(TH_EDGE, ecol);
	BIF_GetThemeColor3ubv(TH_EDGE_SELECT, secol);

	/*Eck! remember to write code to set high (which is the transparent highlight color!)*/
	if (!emdm->cdraw) emdm->cdraw = bglCacheNew();

	printf("in emDM_recalcDrawCache!\n");
	emdm->cdraw->release(emdm->cdraw); /*resets the entire draw cache*/
	emdm->cdraw->setMaterials(emdm->cdraw, G.obedit->totcol, G.obedit->mat);
	emdm->cdraw->beginCache(emdm->cdraw);

	printf("bleh 1\n");
	for (efa=emdm->bmesh->polys.first; efa; efa=efa->next) {
		loop = efa->loopbase->next;
		do {
			VECCOPY(v[0], loop->v->co);
			VECCOPY(v[1], loop->next->v->co);
			VECCOPY(v[2], efa->loopbase->v->co);
			if (efa->flag & ME_NSMOOTH) {
				VECCOPY(no[0], loop->v->no);
				VECCOPY(no[1], loop->next->v->no);
				VECCOPY(no[2], efa->loopbase->v->no);
				printf("smooth face!\n");
			} else {
				CalcNormFloat(efa->loopbase->v->co, efa->loopbase->next->v->co, 
				              efa->loopbase->next->next->v->co, nor);
				VECCOPY(no[0], nor);
				VECCOPY(no[1], nor);
				VECCOPY(no[2], nor);
			}
			emdm->cdraw->addTriangle(emdm->cdraw, v, no, NULL, high, efa->mat_nr);
			loop=loop->next;
		} while (loop != efa->loopbase->prev);
	}
	
	printf("bleh 2\n");
	for (eve=emdm->bmesh->verts.first; eve; eve=eve->next) {
		if (eve->flag & SELECT) emdm->cdraw->addVertPoint(emdm->cdraw, eve->co, svcol, vsize);
		else emdm->cdraw->addVertPoint(emdm->cdraw, eve->co, vcol, vsize);
	}

	printf("bleh 3\n");
	for (eed=emdm->bmesh->edges.first; eed; eed=eed->next) {
		if (eed->flag & SELECT) emdm->cdraw->addEdgeWire(emdm->cdraw, eed->v1->co, eed->v2->co, secol, secol);
		else emdm->cdraw->addEdgeWire(emdm->cdraw, eed->v1->co, eed->v2->co, ecol, ecol);
	}
	emdm->recalc_cdraw = 0;
	emdm->cdraw->endCache(emdm->cdraw);
}

static void emDM__calcFaceCent(EditFace *efa, float cent[3], float (*vertexCos)[3])
{

}
static void emDM_foreachMappedFaceCenter(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no), void *userData)
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;
	emdm->bmesh = emdm->bmesh;

}
static void emDM_drawMappedFaces(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index, int *drawSmooth_r), void *userData, int useColors)
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;
	if (emdm->recalc_cdraw) emDM_recalcDrawCache(emdm);

	printf("drawing!\n");
	emdm->cdraw->drawFacesSolid(emdm->cdraw, 0);
	printf("end draw.\n");
}

static void emDM_getMinMax(DerivedMesh *dm, float min_r[3], float max_r[3])
{
	EditBMeshDerivedMesh *bem = (EditBMeshDerivedMesh*) dm;
	BME_Vert *bev;
	
	INIT_MINMAX(min_r, max_r);
	
	for (bev=bem->bmesh->verts.first; bev; bev=bev->next) {
		DO_MINMAX(bev->co, min_r, max_r);
	}
}
static int emDM_getNumVerts(DerivedMesh *dm)
{
	return ((EditBMeshDerivedMesh*)dm)->bmesh->totvert;
}

static int emDM_getNumEdges(DerivedMesh *dm)
{
	return ((EditBMeshDerivedMesh*)dm)->bmesh->totedge;
}

static int emDM_getNumFaces(DerivedMesh *dm)
{
	return 0;
}

static int emDM_getNumPolys(DerivedMesh *dm)
{
	return ((EditBMeshDerivedMesh*)dm)->bmesh->totpoly;
}

void emDM_getVert(DerivedMesh *dm, int index, MVert *vert_r)
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;
	emdm->bmesh = emdm->bmesh;
}

void emDM_getEdge(DerivedMesh *dm, int index, MEdge *edge_r)
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;
	emdm->bmesh = emdm->bmesh;
}

void emDM_getFace(DerivedMesh *dm, int index, MFace *face_r)
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;
	emdm->bmesh = emdm->bmesh;
}

void emDM_copyVertArray(DerivedMesh *dm, MVert *vert_r)
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;
	emdm->bmesh = emdm->bmesh;
}

void emDM_copyEdgeArray(DerivedMesh *dm, MEdge *edge_r)
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;
	emdm->bmesh = emdm->bmesh;
}

void emDM_copyFaceArray(DerivedMesh *dm, MFace *face_r)
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;
	emdm->bmesh = emdm->bmesh;
}

static void emDM_release(DerivedMesh *dm)
{
	EditBMeshDerivedMesh *emdm = (EditBMeshDerivedMesh*) dm;

	printf("stupid free!\n");
	if (emdm->cdraw) {
		emdm->cdraw->release(emdm->cdraw);
		MEM_freeN(emdm->cdraw);
		emdm->cdraw = NULL;
	}
	emdm->recalc_cdraw = 1;
}

static DerivedMesh *getEditMeshDerivedMesh(BME_Mesh *em, Object *ob,
                                           float (*vertexCos)[3])
{
	EditBMeshDerivedMesh *emdm = MEM_callocN(sizeof(*emdm), "emdm");

	DM_init(&emdm->dm, em->totvert, em->totedge, 0, em->totloop, em->totpoly);

	emdm->cdraw = bglCacheNew();
	emdm->recalc_cdraw = 1;

	emdm->dm.getMinMax = emDM_getMinMax;

	emdm->dm.getNumVerts = emDM_getNumVerts;
	emdm->dm.getNumEdges = emDM_getNumEdges;
	emdm->dm.getNumFaces = emDM_getNumFaces;
	emdm->dm.getNumPolys = emDM_getNumPolys ;
	
	emdm->dm.getVert = emDM_getVert;
	emdm->dm.getEdge = emDM_getEdge;
	emdm->dm.getFace = emDM_getFace;
	emdm->dm.copyVertArray = emDM_copyVertArray;
	emdm->dm.copyEdgeArray = emDM_copyEdgeArray;
	emdm->dm.copyFaceArray = emDM_copyFaceArray;

	emdm->dm.foreachMappedVert = emDM_foreachMappedVert;
	emdm->dm.foreachMappedEdge = emDM_foreachMappedEdge;
	emdm->dm.foreachMappedFaceCenter = emDM_foreachMappedFaceCenter;

	emdm->dm.drawEditVerts = emDM_drawEditVerts;
	emdm->dm.drawEdges = emDM_drawEdges;
	emdm->dm.drawMappedEdges = emDM_drawMappedEdges;
	emdm->dm.drawMappedEdgesInterp = emDM_drawMappedEdgesInterp;
	emdm->dm.drawMappedFaces = emDM_drawMappedFaces;
	emdm->dm.drawUVEdges = emDM_drawUVEdges;

	emdm->dm.release = emDM_release;
	
	emdm->bmesh = em;
	emdm->vertexCos = vertexCos;

	if(CustomData_has_layer(&em->vdata, CD_MDEFORMVERT)) {
		EditVert *eve;
		int i;

		DM_add_vert_layer(&emdm->dm, CD_MDEFORMVERT, CD_CALLOC, NULL);

		for(eve = em->verts.first, i = 0; eve; eve = eve->next, ++i)
			DM_set_vert_data(&emdm->dm, i, CD_MDEFORMVERT,
			                 CustomData_em_get(&em->vdata, eve->data, CD_MDEFORMVERT));
	}

	if(vertexCos) {
		//BME_Vert *eve;
		//BME_Poly *efa;
		//int totpoly = em->totpoly;
		//int i;

		//for (i=0,eve=em->verts.first; eve; eve= eve->next)
		//	eve->tmp.l = (long) i++;

		//emdm->vertexNos = MEM_callocN(sizeof(*emdm->vertexNos)*i, "emdm_vno");
		//emdm->faceNos = MEM_mallocN(sizeof(*emdm->faceNos)*totpoly, "emdm_vno");
	}

	return (DerivedMesh*) emdm;
}

#ifdef WITH_VERSE

/* verse derived mesh */
typedef struct {
	struct DerivedMesh dm;
	struct VNode *vnode;
	struct VLayer *vertex_layer;
	struct VLayer *polygon_layer;
	struct ListBase *edges;
	float (*vertexCos)[3];
} VDerivedMesh;

/* this function set up border points of verse mesh bounding box */
static void vDM_getMinMax(DerivedMesh *dm, float min_r[3], float max_r[3])
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseVert *vvert;

	if(!vdm->vertex_layer) return;

	vvert = (VerseVert*)vdm->vertex_layer->dl.lb.first;

	if(vdm->vertex_layer->dl.da.count > 0) {
		while(vvert) {
			DO_MINMAX(vdm->vertexCos ? vvert->cos : vvert->co, min_r, max_r);
			vvert = vvert->next;
		}
	}
	else {
		min_r[0] = min_r[1] = min_r[2] = max_r[0] = max_r[1] = max_r[2] = 0.0;
	}
}

/* this function return number of vertexes in vertex layer */
static int vDM_getNumVerts(DerivedMesh *dm)
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;

	if(!vdm->vertex_layer) return 0;
	else return vdm->vertex_layer->dl.da.count;
}

/* this function return number of 'fake' edges */
static int vDM_getNumEdges(DerivedMesh *dm)
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;

	return BLI_countlist(vdm->edges);
}

/* this function returns number of polygons in polygon layer */
static int vDM_getNumFaces(DerivedMesh *dm)
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;

	if(!vdm->polygon_layer) return 0;
	else return vdm->polygon_layer->dl.da.count;
}

/* this function doesn't return vertex with index of access array,
 * but it return 'indexth' vertex of dynamic list */
void vDM_getVert(DerivedMesh *dm, int index, MVert *vert_r)
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseVert *vvert;
	int i;

	if(!vdm->vertex_layer) return;

	for(vvert = vdm->vertex_layer->dl.lb.first, i=0 ; i<index; i++) vvert = vvert->next;

	if(vvert) {
		VECCOPY(vert_r->co, vvert->co);

		vert_r->no[0] = vvert->no[0] * 32767.0;
		vert_r->no[1] = vvert->no[1] * 32767.0;
		vert_r->no[2] = vvert->no[2] * 32767.0;

		/* TODO what to do with vert_r->flag and vert_r->mat_nr? */
		vert_r->mat_nr = 0;
		vert_r->flag = 0;
	}
}

/* this function returns fake verse edge */
void vDM_getEdge(DerivedMesh *dm, int index, MEdge *edge_r)
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseEdge *vedge;
	struct VLayer *vert_vlayer = vdm->vertex_layer;
	struct VerseVert *vvert;
	int j;

	if(!vdm->vertex_layer || !vdm->edges) return;

	if(vdm->edges->first) {
		struct VerseVert *vvert1, *vvert2;

		/* store vert indices in tmp union */
		for(vvert = vdm->vertex_layer->dl.lb.first, j = 0; vvert; vvert = vvert->next, j++)
			vvert->tmp.index = j;

		for(vedge = vdm->edges->first; vedge; vedge = vedge->next) {
			if(vedge->tmp.index==index) {
				vvert1 = BLI_dlist_find_link(&(vert_vlayer->dl), (unsigned int)vedge->v0);
				vvert2 = BLI_dlist_find_link(&(vert_vlayer->dl), (unsigned int)vedge->v1);
				
				if(vvert1 && vvert2) {
					edge_r->v1 = vvert1->tmp.index;
					edge_r->v2 = vvert2->tmp.index;
				}
				else {
					edge_r->v1 = 0;
					edge_r->v2 = 0;
				}
				/* not supported yet */
				edge_r->flag = 0;
				edge_r->crease = 0;
				break;
			}
		}
	}
}

/* this function doesn't return face with index of access array,
 * but it returns 'indexth' vertex of dynamic list */
void vDM_getFace(DerivedMesh *dm, int index, MFace *face_r)
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseFace *vface;
	struct VerseVert *vvert;
	struct VerseVert *vvert0, *vvert1, *vvert2, *vvert3;
	int i;

	if(!vdm->vertex_layer || !vdm->polygon_layer) return;

	for(vface = vdm->polygon_layer->dl.lb.first, i = 0; i < index; ++i) vface = vface->next;

	face_r->mat_nr = 0;
	face_r->flag = 0;

	/* goddamn, we have to search all verts to find indices */
	vvert0 = vface->vvert0;
	vvert1 = vface->vvert1;
	vvert2 = vface->vvert2;
	vvert3 = vface->vvert3;
	if(!vvert3) face_r->v4 = 0;

	for(vvert = vdm->vertex_layer->dl.lb.first, i = 0; vvert0 || vvert1 || vvert2 || vvert3; i++, vvert = vvert->next) {
		if(vvert == vvert0) {
			face_r->v1 = i;
			vvert0 = NULL;
		}
		if(vvert == vvert1) {
			face_r->v2 = i;
			vvert1 = NULL;
		}
		if(vvert == vvert2) {
			face_r->v3 = i;
			vvert2 = NULL;
		}
		if(vvert == vvert3) {
			face_r->v4 = i;
			vvert3 = NULL;
		}
	}

	test_index_face(face_r, NULL, 0, vface->vvert3?4:3);
}

/* fill array of mvert */
void vDM_copyVertArray(DerivedMesh *dm, MVert *vert_r)
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseVert *vvert;

	if(!vdm->vertex_layer) return;

	for(vvert = vdm->vertex_layer->dl.lb.first ; vvert; vvert = vvert->next, ++vert_r) {
		VECCOPY(vert_r->co, vvert->co);

		vert_r->no[0] = vvert->no[0] * 32767.0;
		vert_r->no[1] = vvert->no[1] * 32767.0;
		vert_r->no[2] = vvert->no[2] * 32767.0;

		vert_r->mat_nr = 0;
		vert_r->flag = 0;
	}
}

/* dummy function, edges arent supported in verse mesh */
void vDM_copyEdgeArray(DerivedMesh *dm, MEdge *edge_r)
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;

	if(!vdm->vertex_layer || !vdm->edges) return;

	if(vdm->edges->first) {
		struct VerseEdge *vedge;
		struct VLayer *vert_vlayer = vdm->vertex_layer;
		struct VerseVert *vvert, *vvert1, *vvert2;
		int j;

		/* store vert indices in tmp union */
		for(vvert = vdm->vertex_layer->dl.lb.first, j = 0; vvert; vvert = vvert->next, ++j)
			vvert->tmp.index = j;

		for(vedge = vdm->edges->first, j=0 ; vedge; vedge = vedge->next, ++edge_r, j++) {
			/* create temporary edge index */
			vedge->tmp.index = j;
			vvert1 = BLI_dlist_find_link(&(vert_vlayer->dl), (unsigned int)vedge->v0);
			vvert2 = BLI_dlist_find_link(&(vert_vlayer->dl), (unsigned int)vedge->v1);
			if(vvert1 && vvert2) {
				edge_r->v1 = vvert1->tmp.index;
				edge_r->v2 = vvert2->tmp.index;
			}
			else {
				printf("error: vDM_copyEdgeArray: %d, %d\n", vedge->v0, vedge->v1);
				edge_r->v1 = 0;
				edge_r->v2 = 0;
			}
			/* not supported yet */
			edge_r->flag = 0;
			edge_r->crease = 0;
		}
	}
}

/* fill array of mfaces */
void vDM_copyFaceArray(DerivedMesh *dm, MFace *face_r)
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseFace *vface;
	struct VerseVert *vvert;
	int i;
	
	if(!vdm->vertex_layer || !vdm->polygon_layer) return;
	
	/* store vertexes indices in tmp union */
	for(vvert = vdm->vertex_layer->dl.lb.first, i = 0; vvert; vvert = vvert->next, ++i)
		vvert->tmp.index = i;

	for(vface = vdm->polygon_layer->dl.lb.first; vface; vface = vface->next, ++face_r) {
		face_r->mat_nr = 0;
		face_r->flag = 0;

		face_r->v1 = vface->vvert0->tmp.index;
		face_r->v2 = vface->vvert1->tmp.index;
		face_r->v3 = vface->vvert2->tmp.index;
		if(vface->vvert3) face_r->v4 = vface->vvert3->tmp.index;
		else face_r->v4 = 0;

		test_index_face(face_r, NULL, 0, vface->vvert3?4:3);
	}
}

/* return coordination of vertex with index */
static void vDM_getVertCo(DerivedMesh *dm, int index, float co_r[3])
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseVert *vvert = NULL;

	if(!vdm->vertex_layer) return;

	vvert = BLI_dlist_find_link(&(vdm->vertex_layer->dl), index);
	
	if(vvert) {
		VECCOPY(co_r, vdm->vertexCos ? vvert->cos : vvert->co);
	}
	else {
		co_r[0] = co_r[1] = co_r[2] = 0.0;
	}
}

/* return array of vertex coordiantions */
static void vDM_getVertCos(DerivedMesh *dm, float (*cos_r)[3])
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseVert *vvert;
	int i = 0;

	if(!vdm->vertex_layer) return;

	vvert = vdm->vertex_layer->dl.lb.first;
	while(vvert) {
		VECCOPY(cos_r[i], vdm->vertexCos ? vvert->cos : vvert->co);
		i++;
		vvert = vvert->next;
	}
}

/* return normal of vertex with index */
static void vDM_getVertNo(DerivedMesh *dm, int index, float no_r[3])
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseVert *vvert = NULL;

	if(!vdm->vertex_layer) return;

	vvert = BLI_dlist_find_link(&(vdm->vertex_layer->dl), index);
	if(vvert) {
		VECCOPY(no_r, vvert->no);
	}
	else {
		no_r[0] = no_r[1] = no_r[2] = 0.0;
	}
}

/* draw all VerseVertexes */
static void vDM_drawVerts(DerivedMesh *dm)
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseVert *vvert;

	if(!vdm->vertex_layer) return;

	vvert = vdm->vertex_layer->dl.lb.first;

	bglBegin(GL_POINTS);
	while(vvert) {
		bglVertex3fv(vdm->vertexCos ? vvert->cos : vvert->co);
		vvert = vvert->next;
	}
	bglEnd();
}

/* draw all edges of VerseFaces ... it isn't optimal, because verse
 * specification doesn't support edges :-( ... bother eskil ;-)
 * ... some edges (most of edges) are drawn twice */
static void vDM_drawEdges(DerivedMesh *dm, int drawLooseEdges)
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseEdge *vedge;
	struct VLayer *vert_vlayer = vdm->vertex_layer;

	if(vert_vlayer && vdm->edges && (BLI_countlist(vdm->edges) > 0)) {
		struct VerseVert *vvert1, *vvert2;

		glBegin(GL_LINES);
		for(vedge = vdm->edges->first; vedge; vedge = vedge->next) {
			vvert1 = BLI_dlist_find_link(&(vert_vlayer->dl), (unsigned int)vedge->v0);
			vvert2 = BLI_dlist_find_link(&(vert_vlayer->dl), (unsigned int)vedge->v1);
			if(vvert1 && vvert2) {
				glVertex3fv(vdm->vertexCos ? vvert1->cos : vvert1->co);
				glVertex3fv(vdm->vertexCos ? vvert2->cos : vvert2->co);
			}
		}
		glEnd();
	}
}

/* verse spec doesn't support edges ... loose edges can't exist */
void vDM_drawLooseEdges(DerivedMesh *dm)
{
}

/* draw uv edges, not supported yet */
static void vDM_drawUVEdges(DerivedMesh *dm)
{
}

/* draw all VerseFaces */
static void vDM_drawFacesSolid(DerivedMesh *dm, int (*setMaterial)(int))
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseFace *vface;

	if(!vdm->polygon_layer) return;

	vface = vdm->polygon_layer->dl.lb.first;

	glShadeModel(GL_FLAT);
	while(vface) {
		glBegin(vface->vvert3?GL_QUADS:GL_TRIANGLES);
		glNormal3fv(vface->no);
		glVertex3fv(vdm->vertexCos ? vface->vvert0->cos : vface->vvert0->co);
		glVertex3fv(vdm->vertexCos ? vface->vvert1->cos : vface->vvert1->co);
		glVertex3fv(vdm->vertexCos ? vface->vvert2->cos : vface->vvert2->co);
		if(vface->vvert3)
			glVertex3fv(vdm->vertexCos ? vface->vvert3->cos : vface->vvert3->co);
		glEnd();
		vface = vface->next;
	}
}

/* this function should draw mesh with mapped texture, but it isn't supported yet */
static void vDM_drawFacesTex(DerivedMesh *dm, int (*setDrawOptions)(MTFace *tface, MCol *mcol, int matnr))
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseFace *vface;

	if(!vdm->polygon_layer) return;

	vface = vdm->polygon_layer->dl.lb.first;

	while(vface) {
		glBegin(vface->vvert3?GL_QUADS:GL_TRIANGLES);
		glVertex3fv(vdm->vertexCos ? vface->vvert0->cos : vface->vvert0->co);
		glVertex3fv(vdm->vertexCos ? vface->vvert1->cos : vface->vvert1->co);
		glVertex3fv(vdm->vertexCos ? vface->vvert2->cos : vface->vvert2->co);
		if(vface->vvert3)
			glVertex3fv(vdm->vertexCos ? vface->vvert3->cos : vface->vvert3->co);
		glEnd();

		vface = vface->next;
	}
}

/* this function should draw mesh with colored faces (weight paint, vertex
 * colors, etc.), but it isn't supported yet */
static void vDM_drawFacesColored(DerivedMesh *dm, int useTwoSided, unsigned char *col1, unsigned char *col2)
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;
	struct VerseFace *vface;

	if(!vdm->polygon_layer) return;

	vface = vdm->polygon_layer->dl.lb.first;

	while(vface) {
		glBegin(vface->vvert3?GL_QUADS:GL_TRIANGLES);
		glVertex3fv(vdm->vertexCos ? vface->vvert0->cos : vface->vvert0->co);
		glVertex3fv(vdm->vertexCos ? vface->vvert1->cos : vface->vvert1->co);
		glVertex3fv(vdm->vertexCos ? vface->vvert2->cos : vface->vvert2->co);
		if(vface->vvert3)
			glVertex3fv(vdm->vertexCos ? vface->vvert3->cos : vface->vvert3->co);
		glEnd();

		vface = vface->next;
	}
}

/**/
static void vDM_foreachMappedVert(
		DerivedMesh *dm,
		void (*func)(void *userData, int index, float *co, float *no_f, short *no_s),
		void *userData)
{
}

/**/
static void vDM_foreachMappedEdge(
		DerivedMesh *dm,
		void (*func)(void *userData, int index, float *v0co, float *v1co),
		void *userData)
{
}

/**/
static void vDM_foreachMappedFaceCenter(
		DerivedMesh *dm,
		void (*func)(void *userData, int index, float *cent, float *no),
		void *userData)
{
}

/**/
static void vDM_drawMappedFacesTex(
		DerivedMesh *dm,
		int (*setDrawParams)(void *userData, int index),
		void *userData)
{
}

/**/
static void vDM_drawMappedFaces(
		DerivedMesh *dm,
		int (*setDrawOptions)(void *userData, int index, int *drawSmooth_r),
		void *userData,
		int useColors)
{
}

/**/
static void vDM_drawMappedEdges(
		DerivedMesh *dm,
		int (*setDrawOptions)(void *userData, int index),
		void *userData)
{
}

/**/
static void vDM_drawMappedEdgesInterp(
		DerivedMesh *dm, 
		int (*setDrawOptions)(void *userData, int index), 
		void (*setDrawInterpOptions)(void *userData, int index, float t),
		void *userData)
{
}

/* free all DerivedMesh data */
static void vDM_release(DerivedMesh *dm)
{
	VDerivedMesh *vdm = (VDerivedMesh*)dm;

	if (DM_release(dm)) {
		if(vdm->vertexCos) MEM_freeN(vdm->vertexCos);
		MEM_freeN(vdm);
	}
}

/* create derived mesh from verse mesh ... it is used in object mode, when some other client can
 * change shared data and want to see this changes in real time too */
DerivedMesh *derivedmesh_from_versemesh(VNode *vnode, float (*vertexCos)[3])
{
	VDerivedMesh *vdm = MEM_callocN(sizeof(*vdm), "vdm");

	vdm->vnode = vnode;
	vdm->vertex_layer = find_verse_layer_type((VGeomData*)vnode->data, VERTEX_LAYER);
	vdm->polygon_layer = find_verse_layer_type((VGeomData*)vnode->data, POLYGON_LAYER);
	vdm->edges = &((VGeomData*)vnode->data)->edges;

	/* vertex and polygon layer has to exist */
	if(vdm->vertex_layer && vdm->polygon_layer)
		DM_init(&vdm->dm, vdm->vertex_layer->dl.da.count, BLI_countlist(vdm->edges), vdm->polygon_layer->dl.da.count, 0, 0);
	else
		DM_init(&vdm->dm, 0, 0, 0, 0, 0);
	
	vdm->dm.getMinMax = vDM_getMinMax;

	vdm->dm.getNumVerts = vDM_getNumVerts;
	vdm->dm.getNumEdges = vDM_getNumEdges;
	vdm->dm.getNumFaces = vDM_getNumFaces;

	vdm->dm.getVert = vDM_getVert;
	vdm->dm.getEdge = vDM_getEdge;
	vdm->dm.getFace = vDM_getFace;
	vdm->dm.copyVertArray = vDM_copyVertArray;
	vdm->dm.copyEdgeArray = vDM_copyEdgeArray;
	vdm->dm.copyFaceArray = vDM_copyFaceArray;
	
	vdm->dm.foreachMappedVert = vDM_foreachMappedVert;
	vdm->dm.foreachMappedEdge = vDM_foreachMappedEdge;
	vdm->dm.foreachMappedFaceCenter = vDM_foreachMappedFaceCenter;

	vdm->dm.getVertCos = vDM_getVertCos;
	vdm->dm.getVertCo = vDM_getVertCo;
	vdm->dm.getVertNo = vDM_getVertNo;

	vdm->dm.drawVerts = vDM_drawVerts;

	vdm->dm.drawEdges = vDM_drawEdges;
	vdm->dm.drawLooseEdges = vDM_drawLooseEdges;
	vdm->dm.drawUVEdges = vDM_drawUVEdges;

	vdm->dm.drawFacesSolid = vDM_drawFacesSolid;
	vdm->dm.drawFacesTex = vDM_drawFacesTex;
	vdm->dm.drawFacesColored = vDM_drawFacesColored;

	vdm->dm.drawMappedFacesTex = vDM_drawMappedFacesTex;
	vdm->dm.drawMappedFaces = vDM_drawMappedFaces;
	vdm->dm.drawMappedEdges = vDM_drawMappedEdges;
	vdm->dm.drawMappedEdgesInterp = vDM_drawMappedEdgesInterp;

	vdm->dm.release = vDM_release;

	vdm->vertexCos = vertexCos;

	return (DerivedMesh*) vdm;
}

#endif

/***/

DerivedMesh *mesh_create_derived_for_modifier(Object *ob, ModifierData *md)
{
	Mesh *me = ob->data;
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	DerivedMesh *dm;

	if (!(md->mode&eModifierMode_Realtime)) return NULL;
	if (mti->isDisabled && mti->isDisabled(md)) return NULL;

	if (mti->type==eModifierTypeType_OnlyDeform) {
		int numVerts;
		float (*deformedVerts)[3] = mesh_getVertexCos(me, &numVerts);

		mti->deformVerts(md, ob, NULL, deformedVerts, numVerts);
#ifdef WITH_VERSE
		if(me->vnode) dm = derivedmesh_from_versemesh(me->vnode, deformedVerts);
		else dm = getMeshDerivedMesh(me, ob, deformedVerts);
#else
		dm = getMeshDerivedMesh(me, ob, deformedVerts);
#endif

		MEM_freeN(deformedVerts);
	} else {
		DerivedMesh *tdm = getMeshDerivedMesh(me, ob, NULL);
		dm = mti->applyModifier(md, ob, tdm, 0, 0);

		if(tdm != dm) tdm->release(tdm);
	}

	return dm;
}

CustomDataMask get_viewedit_datamask()
{
	CustomDataMask mask = CD_MASK_BAREMESH;
	ScrArea *sa;

	/* check if we need tfaces & mcols due to face select or texture paint */
	if(G.f & G_FACESELECT || G.f & G_TEXTUREPAINT) {
		mask |= CD_MASK_MTFACE | CD_MASK_MCOL;
	} else {
		/* check if we need tfaces & mcols due to view mode */
		for(sa = G.curscreen->areabase.first; sa; sa = sa->next) {
			if(sa->spacetype == SPACE_VIEW3D) {
				View3D *view = sa->spacedata.first;
				if(view->drawtype == OB_SHADED) {
					/* this includes normals for mesh_create_shadedColors */
					mask |= CD_MASK_MTFACE | CD_MASK_MCOL | CD_MASK_NORMAL;
				}
				if(view->drawtype == OB_TEXTURE) {
					mask |= CD_MASK_MTFACE | CD_MASK_MCOL;
				}
			}
		}
	}

	/* check if we need mcols due to vertex paint or weightpaint */
	if(G.f & G_VERTEXPAINT || G.f & G_WEIGHTPAINT)
		mask |= CD_MASK_MCOL;

	return mask;
}

static void mesh_calc_modifiers(Object *ob, float (*inputVertexCos)[3],
                                DerivedMesh **deform_r, DerivedMesh **final_r,
                                int useRenderParams, int useDeform,
                                int needMapping, CustomDataMask dataMask)
{
	Mesh *me = ob->data;
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	LinkNode *datamasks, *curr;
	float (*deformedVerts)[3] = NULL;
	DerivedMesh *dm;
	int numVerts = me->totvert;
	int fluidsimMeshUsed = 0;
	int required_mode;

	modifiers_clearErrors(ob);

	/* we always want to keep original indices */
	dataMask |= CD_MASK_ORIGINDEX;

	datamasks = modifiers_calcDataMasks(md, dataMask);
	curr = datamasks;

	if(deform_r) *deform_r = NULL;
	*final_r = NULL;

	/* replace original mesh by fluidsim surface mesh for fluidsim
	 * domain objects
	 */
	if((G.obedit!=ob) && !needMapping) {
		if((ob->fluidsimFlag & OB_FLUIDSIM_ENABLE) &&
		   (1) && (!give_parteff(ob)) ) { // doesnt work together with particle systems!
			if(ob->fluidsimSettings->type & OB_FLUIDSIM_DOMAIN) {
				loadFluidsimMesh(ob,useRenderParams);
				fluidsimMeshUsed = 1;
				/* might have changed... */
				me = ob->data;
				numVerts = me->totvert;
			}
		}
	}

	if(useRenderParams) required_mode = eModifierMode_Render;
	else required_mode = eModifierMode_Realtime;

	if(useDeform) {
		if(do_ob_key(ob)) /* shape key makes deform verts */
			deformedVerts = mesh_getVertexCos(me, &numVerts);
		
		/* Apply all leading deforming modifiers */
		for(; md; md = md->next, curr = curr->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);

			if((md->mode & required_mode) != required_mode) continue;
			if(mti->isDisabled && mti->isDisabled(md)) continue;

			if(mti->type == eModifierTypeType_OnlyDeform) {
				if(!deformedVerts)
					deformedVerts = mesh_getVertexCos(me, &numVerts);

				mti->deformVerts(md, ob, NULL, deformedVerts, numVerts);
			} else {
				break;
			}
		}

		/* Result of all leading deforming modifiers is cached for
		 * places that wish to use the original mesh but with deformed
		 * coordinates (vpaint, etc.)
		 */
		if (deform_r) {
#ifdef WITH_VERSE
			if(me->vnode) *deform_r = derivedmesh_from_versemesh(me->vnode, deformedVerts);
			else {
				*deform_r = CDDM_from_mesh(me, ob);
				if(deformedVerts) {
					CDDM_apply_vert_coords(*deform_r, deformedVerts);
					CDDM_calc_normals(*deform_r);
				}
			}
#else
			*deform_r = CDDM_from_mesh(me, ob);
			if(deformedVerts) {
				CDDM_apply_vert_coords(*deform_r, deformedVerts);
				CDDM_calc_normals(*deform_r);
			}
#endif
		}
	} else {
		if(!fluidsimMeshUsed) {
			/* default behaviour for meshes */
			deformedVerts = inputVertexCos;
		} else {
			/* the fluid sim mesh might have more vertices than the original 
			 * one, so inputVertexCos shouldnt be used
			 */
			deformedVerts = mesh_getVertexCos(me, &numVerts);
		}
	}


	/* Now apply all remaining modifiers. If useDeform is off then skip
	 * OnlyDeform ones. 
	 */
	dm = NULL;

#ifdef WITH_VERSE
	/* hack to make sure modifiers don't try to use mesh data from a verse
	 * node
	 */
	if(me->vnode) dm = derivedmesh_from_versemesh(me->vnode, deformedVerts);
#endif

	for(; md; md = md->next, curr = curr->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if((md->mode & required_mode) != required_mode) continue;
		if(mti->type == eModifierTypeType_OnlyDeform && !useDeform) continue;
		if((mti->flags & eModifierTypeFlag_RequiresOriginalData) && dm) {
			modifier_setError(md, "Internal error, modifier requires "
			                  "original data (bad stack position).");
			continue;
		}
		if(mti->isDisabled && mti->isDisabled(md)) continue;
		if(needMapping && !modifier_supportsMapping(md)) continue;

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

			mti->deformVerts(md, ob, dm, deformedVerts, numVerts);
		} else {
			DerivedMesh *ndm;

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

				if(deformedVerts) {
					CDDM_apply_vert_coords(dm, deformedVerts);
					CDDM_calc_normals(dm);
				}
			}

			/* set the DerivedMesh to only copy needed data */
			DM_set_only_copy(dm, (CustomDataMask)curr->link);

			ndm = mti->applyModifier(md, ob, dm, useRenderParams,
			                         !inputVertexCos);

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
		}
	}

	/* Yay, we are done. If we have a DerivedMesh and deformed vertices
	 * need to apply these back onto the DerivedMesh. If we have no
	 * DerivedMesh then we need to build one.
	 */
	if(dm && deformedVerts) {
		*final_r = CDDM_copy(dm);

		dm->release(dm);

		CDDM_apply_vert_coords(*final_r, deformedVerts);
		CDDM_calc_normals(*final_r);
	} else if(dm) {
		*final_r = dm;
	} else {
#ifdef WITH_VERSE
		if(me->vnode)
			*final_r = derivedmesh_from_versemesh(me->vnode, deformedVerts);
		else {
			*final_r = CDDM_from_mesh(me, ob);
			if(deformedVerts) {
				CDDM_apply_vert_coords(*final_r, deformedVerts);
				CDDM_calc_normals(*final_r);
			}
		}
#else
		*final_r = CDDM_from_mesh(me, ob);
		if(deformedVerts) {
			CDDM_apply_vert_coords(*final_r, deformedVerts);
			CDDM_calc_normals(*final_r);
		}
#endif
	}

	if(deformedVerts && deformedVerts != inputVertexCos)
		MEM_freeN(deformedVerts);

	BLI_linklist_free(datamasks, NULL);

	/* restore mesh in any case */
	if(fluidsimMeshUsed) ob->data = ob->fluidsimSettings->orgMesh;
}

static float (*editmesh_getVertexCos(EditMesh *em, int *numVerts_r))[3]
{
	return NULL; //EDITBMESHGREP
}

static void editmesh_calc_modifiers(DerivedMesh **cage_r,
                                    DerivedMesh **final_r,
                                    CustomDataMask dataMask)
{
	Object *ob = G.obedit;
	BME_Mesh *em = G.editMesh;
	int cageIndex = modifiers_getCageIndex(ob, NULL);
	float (*deformedVerts)[3] = NULL;

	if(cage_r && cageIndex == -1) {
		*cage_r = getEditMeshDerivedMesh(em, ob, NULL);
	}

	if (cage_r && *cage_r) {
		*final_r = *cage_r;
	} else {
		if (!*final_r) *final_r = getEditMeshDerivedMesh(em, ob, deformedVerts);
		deformedVerts = NULL;
	}

#if 0
	Object *ob = G.obedit;
	EditMesh *em = G.editMesh;
	ModifierData *md;
	float (*deformedVerts)[3] = NULL;
	DerivedMesh *dm;
	int i, numVerts = 0, cageIndex = modifiers_getCageIndex(ob, NULL);
	int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;
	LinkNode *datamasks, *curr;

	modifiers_clearErrors(ob);

	if(cage_r && cageIndex == -1) {
		*cage_r = getEditMeshDerivedMesh(em, ob, NULL);
	}

	dm = NULL;
	md = ob->modifiers.first;

	/* we always want to keep original indices */
	dataMask |= CD_MASK_ORIGINDEX;

	datamasks = modifiers_calcDataMasks(md, dataMask);

	curr = datamasks;
	for(i = 0; md; i++, md = md->next, curr = curr->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if((md->mode & required_mode) != required_mode) continue;
		if((mti->flags & eModifierTypeFlag_RequiresOriginalData) && dm) {
			modifier_setError(md, "Internal error, modifier requires"
			                  "original data (bad stack position).");
			continue;
		}
		if(mti->isDisabled && mti->isDisabled(md)) continue;
		if(!(mti->flags & eModifierTypeFlag_SupportsEditmode)) continue;

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
					deformedVerts = editmesh_getVertexCos(em, &numVerts);
				}
			}

			mti->deformVertsEM(md, ob, em, dm, deformedVerts, numVerts);
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
				dm = CDDM_from_editmesh(em, ob->data);

				if(deformedVerts) {
					CDDM_apply_vert_coords(dm, deformedVerts);
					CDDM_calc_normals(dm);
				}
			}

			/* set the DerivedMesh to only copy needed data */
			DM_set_only_copy(dm, (CustomDataMask)curr->link);

			ndm = mti->applyModifierEM(md, ob, em, dm);

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
				    getEditMeshDerivedMesh(em, ob,
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
		CDDM_calc_normals(*final_r);
	} else if (dm) {
		*final_r = dm;
	} else if (cage_r && *cage_r) {
		*final_r = *cage_r;
	} else {
		*final_r = getEditMeshDerivedMesh(em, ob, deformedVerts);
		deformedVerts = NULL;
	}

	if(deformedVerts)
		MEM_freeN(deformedVerts);
#endif
}

/***/


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
	else if (input<=0.75){	// green->yellow
		*fr= blend * ((input-0.50f)*4.0f);
		*fg= blend;
		*fb= 0.0f;
	}
	else if (input<=1.0){ // yellow->red
		*fr= blend;
		*fg= blend * (1.0f-((input-0.75f)*4.0f)); 
		*fb= 0.0f;
	}
}
static void calc_weightpaint_vert_color(Object *ob, int vert, unsigned char *col)
{
	Mesh *me = ob->data;
	float fr, fg, fb, input = 0.0f;
	int i;

	if (me->dvert) {
		for (i=0; i<me->dvert[vert].totweight; i++)
			if (me->dvert[vert].dw[i].def_nr==ob->actdef-1)
				input+=me->dvert[vert].dw[i].weight;		
	}

	CLAMP(input, 0.0f, 1.0f);
	
	weight_to_rgb(input, &fr, &fg, &fb);
	
	col[3] = (unsigned char)(fr * 255.0f);
	col[2] = (unsigned char)(fg * 255.0f);
	col[1] = (unsigned char)(fb * 255.0f);
	col[0] = 255;
}
static unsigned char *calc_weightpaint_colors(Object *ob) 
{
	Mesh *me = ob->data;
	MFace *mf = me->mface;
	unsigned char *wtcol;
	int i;
	
	wtcol = MEM_callocN (sizeof (unsigned char) * me->totface*4*4, "weightmap");
	
	memset(wtcol, 0x55, sizeof (unsigned char) * me->totface*4*4);
	for (i=0; i<me->totface; i++, mf++){
		calc_weightpaint_vert_color(ob, mf->v1, &wtcol[(i*4 + 0)*4]); 
		calc_weightpaint_vert_color(ob, mf->v2, &wtcol[(i*4 + 1)*4]); 
		calc_weightpaint_vert_color(ob, mf->v3, &wtcol[(i*4 + 2)*4]); 
		if (mf->v4)
			calc_weightpaint_vert_color(ob, mf->v4, &wtcol[(i*4 + 3)*4]); 
	}
	
	return wtcol;
}

static void clear_mesh_caches(Object *ob)
{
	Mesh *me= ob->data;

		/* also serves as signal to remake texspace */
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
}

static void mesh_build_data(Object *ob, CustomDataMask dataMask)
{
	Mesh *me = ob->data;
	float min[3], max[3];

	clear_mesh_caches(ob);

	if(ob!=G.obedit) {
		Object *obact = G.scene->basact?G.scene->basact->object:NULL;
		int editing = (G.f & (G_FACESELECT|G_WEIGHTPAINT|G_VERTEXPAINT|G_TEXTUREPAINT));
		int needMapping = editing && (ob==obact);

		if( (G.f & G_WEIGHTPAINT) && ob==obact ) {
			MCol *wpcol = (MCol*)calc_weightpaint_colors(ob);
			int layernum = CustomData_number_of_layers(&me->fdata, CD_MCOL);

			/* ugly hack here, we temporarily add a new active mcol layer with
			   weightpaint colors in it, that is then duplicated in CDDM_from_mesh */
			CustomData_add_layer(&me->fdata, CD_MCOL, CD_ASSIGN, wpcol, me->totface);
			CustomData_set_layer_active(&me->fdata, CD_MCOL, layernum);

			mesh_calc_modifiers(ob, NULL, &ob->derivedDeform,
			                    &ob->derivedFinal, 0, 1,
			                    needMapping, dataMask);

			CustomData_free_layer_active(&me->fdata, CD_MCOL, me->totface);
		} else {
			mesh_calc_modifiers(ob, NULL, &ob->derivedDeform,
			                    &ob->derivedFinal, 0, 1,
			                    needMapping, dataMask);
		}
		
		INIT_MINMAX(min, max);

		ob->derivedFinal->getMinMax(ob->derivedFinal, min, max);

		boundbox_set_from_min_max(mesh_get_bb(ob->data), min, max);

		ob->derivedFinal->needsFree = 0;
		ob->derivedDeform->needsFree = 0;
		ob->lastDataMask = dataMask;
	}
}

static void bmesh_build_data(CustomDataMask dataMask)
{
	BME_Mesh *em = G.editMesh;
	float min[3], max[3];

	clear_mesh_caches(G.obedit);
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
	
	em->derivedFinal = getEditMeshDerivedMesh(em, G.obedit, NULL);
	em->derivedCage = em->derivedFinal;
	
	em->derivedFinal->needsFree = 0;
	em->derivedCage->needsFree = 0;	
	em->lastDataMask = dataMask;
	return;
	
	INIT_MINMAX(min, max);

	em->derivedFinal->getMinMax(em->derivedFinal, min, max);

	boundbox_set_from_min_max(mesh_get_bb(G.obedit->data), min, max);
	em->derivedFinal->needsFree = 0;
	em->derivedCage->needsFree = 0;
	
}

void makeDerivedMesh(Object *ob, CustomDataMask dataMask)
{
	if (ob==G.obedit) {
		bmesh_build_data(dataMask);
	} else {
		PartEff *paf= give_parteff(ob);
		
		mesh_build_data(ob, dataMask);
		
		if(paf) {
			if((paf->flag & PAF_STATIC) || (ob->recalc & OB_RECALC_TIME)==0)
				build_particle_system(ob);
		}
	}
}

/***/

DerivedMesh *mesh_get_derived_final(Object *ob, CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	if(!ob->derivedFinal || (dataMask & ob->lastDataMask) != dataMask)
		mesh_build_data(ob, dataMask);

	return ob->derivedFinal;
}

DerivedMesh *mesh_get_derived_deform(Object *ob, CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	if(!ob->derivedDeform || (dataMask & ob->lastDataMask) != dataMask)
		mesh_build_data(ob, dataMask);

	return ob->derivedDeform;
}

/* Move to multires Pin level, returns a copy of the original vertex coords. */
float *multires_render_pin(Object *ob, Mesh *me, int *orig_lvl)
{
	float *vert_copy= NULL;

	if(me->mr) {
		MultiresLevel *lvl= NULL;
		int i;
		
		/* Make sure all mesh edits are properly stored in the multires data*/
		multires_update_levels(me, 1);
	
		/* Copy the highest level of multires verts */
		*orig_lvl= me->mr->current;
		lvl= multires_level_n(me->mr, BLI_countlist(&me->mr->levels));
		vert_copy= MEM_callocN(sizeof(float)*3*lvl->totvert, "multires vert_copy");
		for(i=0; i<lvl->totvert; ++i)
			VecCopyf(&vert_copy[i*3], lvl->verts[i].co);
	
		/* Goto the pin level for multires */
		me->mr->newlvl= me->mr->pinlvl;
		multires_set_level(ob, me, 1);
	}
	
	return vert_copy;
}

/* Propagate the changes to render level - fails if mesh topology changed */
void multires_render_final(Object *ob, Mesh *me, DerivedMesh **dm, float *vert_copy, const int orig_lvl)
{
	if(me->mr) {
		if((*dm)->getNumVerts(*dm) == me->totvert &&
		   (*dm)->getNumFaces(*dm) == me->totface) {
			MultiresLevel *lvl= multires_level_n(me->mr, BLI_countlist(&me->mr->levels));
			DerivedMesh *old= NULL;
			int i;

			(*dm)->copyVertArray(*dm, me->mvert);
			(*dm)->release(*dm);

			me->mr->newlvl= me->mr->renderlvl;
			multires_set_level(ob, me, 1);
			(*dm)= getMeshDerivedMesh(me, ob, NULL);

			/* Some of the data in dm is referenced externally, so make a copy */
			old= *dm;
			(*dm)= CDDM_copy(old);
			old->release(old);

			/* Restore the original verts */
			me->mr->newlvl= BLI_countlist(&me->mr->levels);
			multires_set_level(ob, me, 1);
			for(i=0; i<lvl->totvert; ++i)
				VecCopyf(me->mvert[i].co, &vert_copy[i*3]);
		}
		
		if(vert_copy)
			MEM_freeN(vert_copy);
			
		me->mr->newlvl= orig_lvl;
		multires_set_level(ob, me, 1);
	}
}

/* Multires note - if mesh has multires enabled, mesh is first set to the Pin level,
   where all modifiers are applied, then if the topology hasn't changed, the changes
   from modifiers are propagated up to the Render level. */
DerivedMesh *mesh_create_derived_render(Object *ob, CustomDataMask dataMask)
{
	DerivedMesh *final;
	Mesh *me= get_mesh(ob);
	float *vert_copy= NULL;
	int orig_lvl= 0;
	
	vert_copy= multires_render_pin(ob, me, &orig_lvl);
	mesh_calc_modifiers(ob, NULL, NULL, &final, 1, 1, 0, dataMask);
	multires_render_final(ob, me, &final, vert_copy, orig_lvl);

	return final;
}

DerivedMesh *mesh_create_derived_view(Object *ob, CustomDataMask dataMask)
{
	DerivedMesh *final;

	mesh_calc_modifiers(ob, NULL, NULL, &final, 0, 1, 0, dataMask);

	return final;
}

DerivedMesh *mesh_create_derived_no_deform(Object *ob, float (*vertCos)[3],
                                           CustomDataMask dataMask)
{
	DerivedMesh *final;
	
	mesh_calc_modifiers(ob, vertCos, NULL, &final, 0, 0, 0, dataMask);

	return final;
}

DerivedMesh *mesh_create_derived_no_deform_render(Object *ob,
                                                  float (*vertCos)[3],
                                                  CustomDataMask dataMask)
{
	DerivedMesh *final;
	Mesh *me= get_mesh(ob);
	float *vert_copy= NULL;
	int orig_lvl= 0;

	vert_copy= multires_render_pin(ob, me, &orig_lvl);
	mesh_calc_modifiers(ob, vertCos, NULL, &final, 1, 0, 0, dataMask);
	multires_render_final(ob, me, &final, vert_copy, orig_lvl);

	return final;
}

/***/

DerivedMesh *editmesh_get_derived_cage_and_final(DerivedMesh **final_r,
                                                 CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	if(!G.editMesh->derivedCage ||
	   (G.editMesh->lastDataMask & dataMask) != dataMask)
		bmesh_build_data(dataMask);

	*final_r = G.editMesh->derivedFinal;
	return G.editMesh->derivedCage;
}

DerivedMesh *editmesh_get_derived_cage(CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	if(!G.editMesh->derivedCage ||
	   (G.editMesh->lastDataMask & dataMask) != dataMask)
		bmesh_build_data(dataMask);

	return G.editMesh->derivedCage;
}

DerivedMesh *editmesh_get_derived_base(void)
{
	BME_Mesh *bme = G.editMesh;
	
	if (bme->derivedFinal) return bme->derivedFinal;
	else return getEditMeshDerivedMesh(G.editMesh, G.obedit, NULL);
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

float *mesh_get_mapped_verts_nors(Object *ob)
{
	Mesh *me= ob->data;
	DerivedMesh *dm;
	float *vertexcosnos;
	
	/* lets prevent crashing... */
	if(ob->type!=OB_MESH || me->totvert==0)
		return NULL;
	
	dm= mesh_get_derived_final(ob, CD_MASK_BAREMESH);
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


/* ************************* fluidsim bobj file handling **************************** */

#ifndef DISABLE_ELBEEM

#ifdef WIN32
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

/* write .bobj.gz file for a mesh object */
void writeBobjgz(char *filename, struct Object *ob, int useGlobalCoords, int append, float time) 
{
	char debugStrBuffer[256];
	int wri,i,j,totvert,totface;
	float wrf;
	gzFile gzf;
	DerivedMesh *dm;
	float vec[3];
	float rotmat[3][3];
	MVert *mvert;
	MFace *mface;
	//if(append)return; // DEBUG

	if(!ob->data || (ob->type!=OB_MESH)) {
		snprintf(debugStrBuffer,256,"Writing GZ_BOBJ Invalid object %s ...\n", ob->id.name); 
		elbeemDebugOut(debugStrBuffer);
		return;
	}
	if((ob->size[0]<0.0) || (ob->size[0]<0.0) || (ob->size[0]<0.0) ) {
		snprintf(debugStrBuffer,256,"\nfluidSim::writeBobjgz:: Warning object %s has negative scaling - check triangle ordering...?\n\n", ob->id.name); 
		elbeemDebugOut(debugStrBuffer);
	}

	snprintf(debugStrBuffer,256,"Writing GZ_BOBJ '%s' ... ",filename); elbeemDebugOut(debugStrBuffer); 
	if(append) gzf = gzopen(filename, "a+b9");
	else       gzf = gzopen(filename, "wb9");
	if (!gzf) {
		snprintf(debugStrBuffer,256,"writeBobjgz::error - Unable to open file for writing '%s'\n", filename);
		elbeemDebugOut(debugStrBuffer);
		return;
	}

	dm = mesh_create_derived_render(ob, CD_MASK_BAREMESH);
	//dm = mesh_create_derived_no_deform(ob,NULL);

	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);
	totvert = dm->getNumVerts(dm);
	totface = dm->getNumFaces(dm);

	// write time value for appended anim mesh
	if(append) {
		gzwrite(gzf, &time, sizeof(time));
	}

	// continue with verts/norms
	if(sizeof(wri)!=4) { snprintf(debugStrBuffer,256,"Writing GZ_BOBJ, Invalid int size %d...\n", wri); elbeemDebugOut(debugStrBuffer); return; } // paranoia check
	wri = dm->getNumVerts(dm);
	mvert = dm->getVertArray(dm);
	gzwrite(gzf, &wri, sizeof(wri));
	for(i=0; i<wri;i++) {
		VECCOPY(vec, mvert[i].co);
		if(useGlobalCoords) { Mat4MulVecfl(ob->obmat, vec); }
		for(j=0; j<3; j++) {
			wrf = vec[j]; 
			gzwrite(gzf, &wrf, sizeof( wrf )); 
		}
	}

	// should be the same as Vertices.size
	wri = totvert;
	gzwrite(gzf, &wri, sizeof(wri));
	EulToMat3(ob->rot, rotmat);
	for(i=0; i<wri;i++) {
		VECCOPY(vec, mvert[i].no);
		Normalize(vec);
		if(useGlobalCoords) { Mat3MulVecfl(rotmat, vec); }
		for(j=0; j<3; j++) {
			wrf = vec[j];
			gzwrite(gzf, &wrf, sizeof( wrf )); 
		}
	}

	// append only writes verts&norms 
	if(!append) {
		//float side1[3],side2[3],norm1[3],norm2[3];
		//float inpf;
	
		// compute no. of triangles 
		wri = 0;
		for(i=0; i<totface; i++) {
			wri++;
			if(mface[i].v4) { wri++; }
		}
		gzwrite(gzf, &wri, sizeof(wri));
		for(i=0; i<totface; i++) {

			int face[4];
			face[0] = mface[i].v1;
			face[1] = mface[i].v2;
			face[2] = mface[i].v3;
			face[3] = mface[i].v4;
			//snprintf(debugStrBuffer,256,"F %s %d = %d,%d,%d,%d \n",ob->id.name, i, face[0],face[1],face[2],face[3] ); elbeemDebugOut(debugStrBuffer);
			//VecSubf(side1, mvert[face[1]].co,mvert[face[0]].co);
			//VecSubf(side2, mvert[face[2]].co,mvert[face[0]].co);
			//Crossf(norm1,side1,side2);
			gzwrite(gzf, &(face[0]), sizeof( face[0] )); 
			gzwrite(gzf, &(face[1]), sizeof( face[1] )); 
			gzwrite(gzf, &(face[2]), sizeof( face[2] )); 
			if(face[3]) { 
				//VecSubf(side1, mvert[face[2]].co,mvert[face[0]].co);
				//VecSubf(side2, mvert[face[3]].co,mvert[face[0]].co);
				//Crossf(norm2,side1,side2);
				//inpf = Inpf(norm1,norm2);
				//if(inpf>0.) {
				gzwrite(gzf, &(face[0]), sizeof( face[0] )); 
				gzwrite(gzf, &(face[2]), sizeof( face[2] )); 
				gzwrite(gzf, &(face[3]), sizeof( face[3] )); 
				//} else {
					//gzwrite(gzf, &(face[0]), sizeof( face[0] )); 
					//gzwrite(gzf, &(face[3]), sizeof( face[3] )); 
					//gzwrite(gzf, &(face[2]), sizeof( face[2] )); 
				//}
			} // quad
		}
	}

	snprintf(debugStrBuffer,256,"Done. #Vertices: %d, #Triangles: %d\n", totvert, totface ); 
	elbeemDebugOut(debugStrBuffer);
	
	gzclose( gzf );
	dm->release(dm);
}

void initElbeemMesh(struct Object *ob, 
		int *numVertices, float **vertices, 
		int *numTriangles, int **triangles,
		int useGlobalCoords) 
{
	DerivedMesh *dm = NULL;
	MVert *mvert;
	MFace *mface;
	int countTris=0, i, totvert, totface;
	float *verts;
	int *tris;

	dm = mesh_create_derived_render(ob, CD_MASK_BAREMESH);
	//dm = mesh_create_derived_no_deform(ob,NULL);

	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);
	totvert = dm->getNumVerts(dm);
	totface = dm->getNumFaces(dm);

	*numVertices = totvert;
	verts = MEM_callocN( totvert*3*sizeof(float), "elbeemmesh_vertices");
	for(i=0; i<totvert; i++) {
		VECCOPY( &verts[i*3], mvert[i].co);
		if(useGlobalCoords) { Mat4MulVecfl(ob->obmat, &verts[i*3]); }
	}
	*vertices = verts;

	for(i=0; i<totface; i++) {
		countTris++;
		if(mface[i].v4) { countTris++; }
	}
	*numTriangles = countTris;
	tris = MEM_callocN( countTris*3*sizeof(int), "elbeemmesh_triangles");
	countTris = 0;
	for(i=0; i<totface; i++) {
		int face[4];
		face[0] = mface[i].v1;
		face[1] = mface[i].v2;
		face[2] = mface[i].v3;
		face[3] = mface[i].v4;

		tris[countTris*3+0] = face[0]; 
		tris[countTris*3+1] = face[1]; 
		tris[countTris*3+2] = face[2]; 
		countTris++;
		if(face[3]) { 
			tris[countTris*3+0] = face[0]; 
			tris[countTris*3+1] = face[2]; 
			tris[countTris*3+2] = face[3]; 
			countTris++;
		}
	}
	*triangles = tris;

	dm->release(dm);
}

/* read .bobj.gz file into a fluidsimDerivedMesh struct */
Mesh* readBobjgz(char *filename, Mesh *orgmesh, float* bbstart, float *bbsize) //, fluidsimDerivedMesh *fsdm)
{
	int wri,i,j;
	char debugStrBuffer[256];
	float wrf;
	Mesh *newmesh; 
	const int debugBobjRead = 1;
	// init data from old mesh (materials,flags)
	MFace *origMFace = &((MFace*) orgmesh->mface)[0];
	int mat_nr = -1;
	int flag = -1;
	MFace *fsface = NULL;
	int gotBytes;
	gzFile gzf;

	if(!orgmesh) return NULL;
	if(!origMFace) return NULL;
	mat_nr = origMFace->mat_nr;
	flag = origMFace->flag;

	// similar to copy_mesh
	newmesh = MEM_dupallocN(orgmesh);
	newmesh->mat= orgmesh->mat;

	newmesh->mvert= NULL;
	newmesh->medge= NULL;
	newmesh->mface= NULL;
	newmesh->mtface= NULL;

	newmesh->dvert = NULL;

	newmesh->mcol= NULL;
	newmesh->msticky= NULL;
	newmesh->texcomesh= NULL;
	memset(&newmesh->vdata, 0, sizeof(newmesh->vdata));
	memset(&newmesh->edata, 0, sizeof(newmesh->edata));
	memset(&newmesh->fdata, 0, sizeof(newmesh->fdata));

	newmesh->key= NULL;
	newmesh->totface = 0;
	newmesh->totvert = 0;
	newmesh->totedge = 0;
	newmesh->medge = NULL;


	snprintf(debugStrBuffer,256,"Reading '%s' GZ_BOBJ... ",filename); elbeemDebugOut(debugStrBuffer); 
	gzf = gzopen(filename, "rb");
	// gzf = fopen(filename, "rb");
	// debug: fread(b,c,1,a) = gzread(a,b,c)
	if (!gzf) {
		//snprintf(debugStrBuffer,256,"readBobjgz::error - Unable to open file for reading '%s'\n", filename); // DEBUG
		MEM_freeN(newmesh);
		return NULL;
	}

	//if(sizeof(wri)!=4) { snprintf(debugStrBuffer,256,"Reading GZ_BOBJ, Invalid int size %d...\n", wri); return NULL; } // paranoia check
	gotBytes = gzread(gzf, &wri, sizeof(wri));
	newmesh->totvert = wri;
	newmesh->mvert = CustomData_add_layer(&newmesh->vdata, CD_MVERT, CD_CALLOC, NULL, newmesh->totvert);
	if(debugBobjRead){ snprintf(debugStrBuffer,256,"#vertices %d ", newmesh->totvert); elbeemDebugOut(debugStrBuffer); } //DEBUG
	for(i=0; i<newmesh->totvert;i++) {
		//if(debugBobjRead) snprintf(debugStrBuffer,256,"V %d = ",i);
		for(j=0; j<3; j++) {
			gotBytes = gzread(gzf, &wrf, sizeof( wrf )); 
			newmesh->mvert[i].co[j] = wrf;
			//if(debugBobjRead) snprintf(debugStrBuffer,256,"%25.20f ", wrf);
		}
		//if(debugBobjRead) snprintf(debugStrBuffer,256,"\n");
	}

	// should be the same as Vertices.size
	gotBytes = gzread(gzf, &wri, sizeof(wri));
	if(wri != newmesh->totvert) {
		// complain #vertices has to be equal to #normals, reset&abort
		CustomData_free_layer_active(&newmesh->vdata, CD_MVERT, newmesh->totvert);
		MEM_freeN(newmesh);
		snprintf(debugStrBuffer,256,"Reading GZ_BOBJ, #normals=%d, #vertices=%d, aborting...\n", wri,newmesh->totvert );
		return NULL;
	}
	for(i=0; i<newmesh->totvert;i++) {
		for(j=0; j<3; j++) {
			gotBytes = gzread(gzf, &wrf, sizeof( wrf )); 
			newmesh->mvert[i].no[j] = (short)(wrf*32767.0f);
			//newmesh->mvert[i].no[j] = 0.5; // DEBUG tst
		}
	//fprintf(stderr,"  DEBDPCN nm%d, %d = %d,%d,%d \n",
			//(int)(newmesh->mvert), i, newmesh->mvert[i].no[0], newmesh->mvert[i].no[1], newmesh->mvert[i].no[2]);
	}
	//fprintf(stderr,"  DPCN 0 = %d,%d,%d \n", newmesh->mvert[0].no[0], newmesh->mvert[0].no[1], newmesh->mvert[0].no[2]);

	
	/* compute no. of triangles */
	gotBytes = gzread(gzf, &wri, sizeof(wri));
	newmesh->totface = wri;
	newmesh->mface = CustomData_add_layer(&newmesh->fdata, CD_MFACE, CD_CALLOC, NULL, newmesh->totface);
	if(debugBobjRead){ snprintf(debugStrBuffer,256,"#faces %d ", newmesh->totface); elbeemDebugOut(debugStrBuffer); } //DEBUG
	fsface = newmesh->mface;
	for(i=0; i<newmesh->totface; i++) {
		int face[4];

		gotBytes = gzread(gzf, &(face[0]), sizeof( face[0] )); 
		gotBytes = gzread(gzf, &(face[1]), sizeof( face[1] )); 
		gotBytes = gzread(gzf, &(face[2]), sizeof( face[2] )); 
		face[3] = 0;

		fsface[i].v1 = face[0];
		fsface[i].v2 = face[1];
		fsface[i].v3 = face[2];
		fsface[i].v4 = face[3];
	}

	// correct triangles with v3==0 for blender, cycle verts
	for(i=0; i<newmesh->totface; i++) {
		if(!fsface[i].v3) {
			int temp = fsface[i].v1;
			fsface[i].v1 = fsface[i].v2;
			fsface[i].v2 = fsface[i].v3;
			fsface[i].v3 = temp;
		}
	}
	
	gzclose( gzf );
	for(i=0;i<newmesh->totface;i++) { 
		fsface[i].mat_nr = mat_nr;
		fsface[i].flag = flag;
		fsface[i].edcode = ME_V1V2 | ME_V2V3 | ME_V3V1;
		//snprintf(debugStrBuffer,256,"%d : %d,%d,%d\n", i,fsface[i].mat_nr, fsface[i].flag, fsface[i].edcode );
	}

	snprintf(debugStrBuffer,256," (%d,%d) done\n", newmesh->totvert,newmesh->totface); elbeemDebugOut(debugStrBuffer); //DEBUG
	return newmesh;
}

/* read zipped fluidsim velocities into the co's of the fluidsimsettings normals struct */
void readVelgz(char *filename, Object *srcob)
{
	char debugStrBuffer[256];
	int wri, i, j;
	float wrf;
	gzFile gzf;
	MVert *vverts = srcob->fluidsimSettings->meshSurfNormals;
	int len = strlen(filename);
	Mesh *mesh = srcob->data;
	// mesh and vverts have to be valid from loading...

	// clean up in any case
	for(i=0; i<mesh->totvert;i++) { 
		for(j=0; j<3; j++) {
		 	vverts[i].co[j] = 0.; 
		} 
	} 
	if(srcob->fluidsimSettings->domainNovecgen>0) return;

	if(len<7) { 
		//printf("readVelgz Eror: invalid filename '%s'\n",filename); // DEBUG
		return; 
	}

	// .bobj.gz , correct filename
	// 87654321
	filename[len-6] = 'v';
	filename[len-5] = 'e';
	filename[len-4] = 'l';

	snprintf(debugStrBuffer,256,"Reading '%s' GZ_VEL... ",filename); elbeemDebugOut(debugStrBuffer); 
	gzf = gzopen(filename, "rb");
	if (!gzf) { 
		//printf("readVelgz Eror: unable to open file '%s'\n",filename); // DEBUG
		return; 
	}

	gzread(gzf, &wri, sizeof( wri ));
	if(wri != mesh->totvert) {
		//printf("readVelgz Eror: invalid no. of velocities %d vs. %d aborting.\n" ,wri ,mesh->totvert ); // DEBUG
		return; 
	}

	for(i=0; i<mesh->totvert;i++) {
		for(j=0; j<3; j++) {
			gzread(gzf, &wrf, sizeof( wrf )); 
			vverts[i].co[j] = wrf;
		}
		//if(i<20) fprintf(stderr, "GZ_VELload %d = %f,%f,%f  \n",i,vverts[i].co[0],vverts[i].co[1],vverts[i].co[2]); // DEBUG
	}

	gzclose(gzf);
}


/* ***************************** fluidsim derived mesh ***************************** */

/* check which file to load, and replace old mesh of the object with it */
/* this replacement is undone at the end of mesh_calc_modifiers */
void loadFluidsimMesh(Object *srcob, int useRenderParams)
{
	Mesh *mesh = NULL;
	float *bbStart = NULL, *bbSize = NULL;
	float lastBB[3];
	int displaymode = 0;
	int curFrame = G.scene->r.cfra - 1 /*G.scene->r.sfra*/; /* start with 0 at start frame */
	char targetDir[FILE_MAXFILE+FILE_MAXDIR], targetFile[FILE_MAXFILE+FILE_MAXDIR];
	char debugStrBuffer[256];
	//snprintf(debugStrBuffer,256,"loadFluidsimMesh call (obid '%s', rp %d)\n", srcob->id.name, useRenderParams); // debug

	if((!srcob)||(!srcob->fluidsimSettings)) {
		snprintf(debugStrBuffer,256,"DEBUG - Invalid loadFluidsimMesh call, rp %d, dm %d)\n", useRenderParams, displaymode); // debug
		elbeemDebugOut(debugStrBuffer); // debug
		return;
	}
	// make sure the original mesh data pointer is stored
	if(!srcob->fluidsimSettings->orgMesh) {
		srcob->fluidsimSettings->orgMesh = srcob->data;
	}

	// free old mesh, if there is one (todo, check if it's still valid?)
	if(srcob->fluidsimSettings->meshSurface) {
		Mesh *freeFsMesh = srcob->fluidsimSettings->meshSurface;

		// similar to free_mesh(...) , but no things like unlink...
		CustomData_free(&freeFsMesh->vdata, freeFsMesh->totvert);
		CustomData_free(&freeFsMesh->edata, freeFsMesh->totedge);
		CustomData_free(&freeFsMesh->fdata, freeFsMesh->totface);
		MEM_freeN(freeFsMesh);
		
		if(srcob->data == srcob->fluidsimSettings->meshSurface)
		 srcob->data = srcob->fluidsimSettings->orgMesh;
		srcob->fluidsimSettings->meshSurface = NULL;

		if(srcob->fluidsimSettings->meshSurfNormals) MEM_freeN(srcob->fluidsimSettings->meshSurfNormals);
		srcob->fluidsimSettings->meshSurfNormals = NULL;
	} 

	// init bounding box
	bbStart = srcob->fluidsimSettings->bbStart; 
	bbSize = srcob->fluidsimSettings->bbSize;
	lastBB[0] = bbSize[0];  // TEST
	lastBB[1] = bbSize[1]; 
	lastBB[2] = bbSize[2];
	fluidsimGetAxisAlignedBB(srcob->fluidsimSettings->orgMesh, srcob->obmat, bbStart, bbSize, &srcob->fluidsimSettings->meshBB);
	// check free fsmesh... TODO
	
	if(!useRenderParams) {
		displaymode = srcob->fluidsimSettings->guiDisplayMode;
	} else {
		displaymode = srcob->fluidsimSettings->renderDisplayMode;
	}
	
	snprintf(debugStrBuffer,256,"loadFluidsimMesh call (obid '%s', rp %d, dm %d), curFra=%d, sFra=%d #=%d \n", 
			srcob->id.name, useRenderParams, displaymode, G.scene->r.cfra, G.scene->r.sfra, curFrame ); // debug
	elbeemDebugOut(debugStrBuffer); // debug

 	strncpy(targetDir, srcob->fluidsimSettings->surfdataPath, FILE_MAXDIR);
	// use preview or final mesh?
	if(displaymode==1) {
		// just display original object
		srcob->data = srcob->fluidsimSettings->orgMesh;
		return;
	} else if(displaymode==2) {
		strcat(targetDir,"fluidsurface_preview_#");
	} else { // 3
		strcat(targetDir,"fluidsurface_final_#");
	}
	BLI_convertstringcode(targetDir, G.sce, curFrame); // fixed #frame-no 
	strcpy(targetFile,targetDir);
	strcat(targetFile, ".bobj.gz");

	snprintf(debugStrBuffer,256,"loadFluidsimMesh call (obid '%s', rp %d, dm %d) '%s' \n", srcob->id.name, useRenderParams, displaymode, targetFile);  // debug
	elbeemDebugOut(debugStrBuffer); // debug

	if(displaymode!=2) { // dont add bounding box for final
		mesh = readBobjgz(targetFile, srcob->fluidsimSettings->orgMesh ,NULL,NULL);
	} else {
		mesh = readBobjgz(targetFile, srcob->fluidsimSettings->orgMesh, bbSize,bbSize );
	}
	if(!mesh) {
		// switch, abort background rendering when fluidsim mesh is missing
		const char *strEnvName2 = "BLENDER_ELBEEMBOBJABORT"; // from blendercall.cpp
		if(G.background==1) {
			if(getenv(strEnvName2)) {
				int elevel = atoi(getenv(strEnvName2));
				if(elevel>0) {
					printf("Env. var %s set, fluid sim mesh '%s' not found, aborting render...\n",strEnvName2, targetFile);
					exit(1);
				}
			}
		}
		
		// display org. object upon failure
		srcob->data = srcob->fluidsimSettings->orgMesh;
		return;
	}

	if((mesh)&&(mesh->totvert>0)) {
		make_edges(mesh, 0);	// 0 = make all edges draw
	}
	srcob->fluidsimSettings->meshSurface = mesh;
	srcob->data = mesh;
	srcob->fluidsimSettings->meshSurfNormals = MEM_dupallocN(mesh->mvert);

	// load vertex velocities, if they exist...
	// TODO? use generate flag as loading flag as well?
	// warning, needs original .bobj.gz mesh loading filename
	if(displaymode==3) {
		readVelgz(targetFile, srcob);
	} else {
		// no data for preview, only clear...
		int i,j;
		for(i=0; i<mesh->totvert;i++) { for(j=0; j<3; j++) { srcob->fluidsimSettings->meshSurfNormals[i].co[j] = 0.; }} 
	}

	//fprintf(stderr,"LOADFLM DEBXHCH fs=%d 3:%d,%d,%d \n", (int)mesh, ((Mesh *)(srcob->fluidsimSettings->meshSurface))->mvert[3].no[0], ((Mesh *)(srcob->fluidsimSettings->meshSurface))->mvert[3].no[1], ((Mesh *)(srcob->fluidsimSettings->meshSurface))->mvert[3].no[2]);
	return;
}

/* helper function */
/* init axis aligned BB for mesh object */
void fluidsimGetAxisAlignedBB(struct Mesh *mesh, float obmat[][4],
		 /*RET*/ float start[3], /*RET*/ float size[3], /*RET*/ struct Mesh **bbmesh )
{
	float bbsx=0.0, bbsy=0.0, bbsz=0.0;
	float bbex=1.0, bbey=1.0, bbez=1.0;
	int i;
	float vec[3];

	VECCOPY(vec, mesh->mvert[0].co); 
	Mat4MulVecfl(obmat, vec);
	bbsx = vec[0]; bbsy = vec[1]; bbsz = vec[2];
	bbex = vec[0]; bbey = vec[1]; bbez = vec[2];

	for(i=1; i<mesh->totvert;i++) {
		VECCOPY(vec, mesh->mvert[i].co);
		Mat4MulVecfl(obmat, vec);

		if(vec[0] < bbsx){ bbsx= vec[0]; }
		if(vec[1] < bbsy){ bbsy= vec[1]; }
		if(vec[2] < bbsz){ bbsz= vec[2]; }
		if(vec[0] > bbex){ bbex= vec[0]; }
		if(vec[1] > bbey){ bbey= vec[1]; }
		if(vec[2] > bbez){ bbez= vec[2]; }
	}

	// return values...
	if(start) {
		start[0] = bbsx;
		start[1] = bbsy;
		start[2] = bbsz;
	} 
	if(size) {
		size[0] = bbex-bbsx;
		size[1] = bbey-bbsy;
		size[2] = bbez-bbsz;
	}

	// init bounding box mesh?
	if(bbmesh) {
		int i,j;
		Mesh *newmesh = NULL;
		if(!(*bbmesh)) { newmesh = MEM_callocN(sizeof(Mesh), "fluidsimGetAxisAlignedBB_meshbb"); }
		else {           newmesh = *bbmesh; }

		newmesh->totvert = 8;
		if(!newmesh->mvert)
			newmesh->mvert = CustomData_add_layer(&newmesh->vdata, CD_MVERT, CD_CALLOC, NULL, newmesh->totvert);
		for(i=0; i<8; i++) {
			for(j=0; j<3; j++) newmesh->mvert[i].co[j] = start[j]; 
		}

		newmesh->totface = 6;
		if(!newmesh->mface)
			newmesh->mface = CustomData_add_layer(&newmesh->fdata, CD_MFACE, CD_CALLOC, NULL, newmesh->totface);

		*bbmesh = newmesh;
	}
}

#else // DISABLE_ELBEEM

/* dummy for mesh_calc_modifiers */
void loadFluidsimMesh(Object *srcob, int useRenderParams) {
}

#endif // DISABLE_ELBEEM

